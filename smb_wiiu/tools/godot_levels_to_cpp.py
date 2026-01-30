#!/usr/bin/env python3
from __future__ import annotations

import base64
import re
import struct
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GODOT_ROOT = ROOT.parent / "Super-Mario-Bros.-Remastered-Public"
LEVELS_ROOT = GODOT_ROOT / "Scenes/Levels/SMB1"

MAP_W = 512
MAP_H = 15
TILE = 16
Q_COIN = 0
Q_POWERUP = 1
Q_ONEUP = 2
Q_STAR = 3

COL_NONE = 0
COL_SOLID = 1
COL_ONEWAY = 2

ATLAS_NONE = 0
ATLAS_TERRAIN = 1
ATLAS_DECO = 2
ATLAS_LIQUID = 3


def question_meta_for_path(path: str | None) -> int:
    if not path:
        return Q_COIN
    if "PowerUpQuestionBlock" in path:
        return Q_POWERUP
    if "OneUpQuestionBlock" in path:
        return Q_ONEUP
    if "StarQuestionBlock" in path:
        return Q_STAR
    return Q_COIN


@dataclass
class Node:
    name: str
    parent: str
    resource_path: str | None
    position: tuple[float, float] | None
    tile_map_data: str | None
    pipe_id: int | None
    exit_only: bool
    target_level: str | None
    target_sub_level: int | None
    enter_direction: int | None
    connecting_pipe: str | None
    vertical_direction: int | None
    top: int | None
    linked_platform: str | None
    rope_top: int | None
    bg_primary_layer: int | None
    bg_second_layer: int | None
    bg_overlay_clouds: bool | None
    bg_particles: int | None


def parse_uid_map() -> dict[str, Path]:
    uid_map: dict[str, Path] = {}
    for p in GODOT_ROOT.rglob("*.tscn"):
        text = p.read_text(errors="ignore")
        m = re.search(r'uid="(uid://[^"]+)"', text)
        if m:
            uid_map[m.group(1)] = p
    return uid_map


def parse_ext_resources(lines: list[str]) -> dict[str, str]:
    ext: dict[str, str] = {}
    for line in lines:
        if line.startswith("[ext_resource"):
            m = re.search(r'(?:^|\s)id="([^"]+)"', line)
            p = re.search(r'path="([^"]+)"', line)
            if m and p:
                ext[m.group(1)] = p.group(1)
    return ext


def parse_nodes(lines: list[str], ext: dict[str, str]) -> list[Node]:
    nodes: list[Node] = []
    cur: Node | None = None

    for line in lines:
        line = line.strip()
        if line.startswith("[node "):
            m_name = re.search(r'name="([^"]+)"', line)
            m_parent = re.search(r'parent="([^"]+)"', line)
            m_inst = re.search(r'instance=ExtResource\("([^"]+)"\)', line)
            res_path = ext.get(m_inst.group(1)) if m_inst else None
            cur = Node(
                name=m_name.group(1) if m_name else "",
                parent=m_parent.group(1) if m_parent else "",
                resource_path=res_path,
                position=None,
                tile_map_data=None,
                pipe_id=None,
                exit_only=False,
                target_level=None,
                target_sub_level=None,
                enter_direction=None,
                connecting_pipe=None,
                vertical_direction=None,
                top=None,
                linked_platform=None,
                rope_top=None,
                bg_primary_layer=None,
                bg_second_layer=None,
                bg_overlay_clouds=None,
                bg_particles=None,
            )
            nodes.append(cur)
            continue

        if not cur:
            continue

        if line.startswith("position = Vector2("):
            m = re.search(r"Vector2\(([^,]+),\s*([^)]+)\)", line)
            if m:
                cur.position = (float(m.group(1)), float(m.group(2)))
        elif line.startswith("tile_map_data = PackedByteArray"):
            # Levels commonly use multiple TileMap layers (e.g. `Tiles` and
            # `DecoTiles`). Capture tile_map_data for any TileMap node so
            # decorations can be preserved.
            if cur.name in ("Tiles", "DecoTiles"):
                m = re.search(r'PackedByteArray\("([^"]+)"\)', line)
                if m:
                    cur.tile_map_data = m.group(1)
        elif line.startswith("pipe_id = "):
            cur.pipe_id = int(line.split("=", 1)[1].strip())
        elif line.startswith("exit_only = "):
            cur.exit_only = line.split("=", 1)[1].strip().lower() == "true"
        elif line.startswith("target_level = "):
            val = line.split("=", 1)[1].strip().strip('"')
            cur.target_level = val
        elif line.startswith("target_sub_level = "):
            cur.target_sub_level = int(line.split("=", 1)[1].strip())
        elif line.startswith("enter_direction = "):
            cur.enter_direction = int(line.split("=", 1)[1].strip())
        elif line.startswith("connecting_pipe = "):
            m = re.search(r'NodePath\("([^"]+)"\)', line)
            if m:
                cur.connecting_pipe = m.group(1).split("/")[-1]
        elif line.startswith("linked_platform = "):
            m = re.search(r'NodePath\("([^"]+)"\)', line)
            if m:
                cur.linked_platform = m.group(1).split("/")[-1]
        elif line.startswith("vertical_direction = "):
            try:
                cur.vertical_direction = int(line.split("=", 1)[1].strip())
            except ValueError:
                pass
        elif line.startswith("top = "):
            try:
                cur.top = int(line.split("=", 1)[1].strip())
            except ValueError:
                pass
        elif line.startswith("rope_top = "):
            try:
                cur.rope_top = int(line.split("=", 1)[1].strip())
            except ValueError:
                pass
        elif line.startswith("primary_layer = "):
            try:
                cur.bg_primary_layer = int(line.split("=", 1)[1].strip())
            except ValueError:
                pass
        elif line.startswith("second_layer = "):
            try:
                cur.bg_second_layer = int(line.split("=", 1)[1].strip())
            except ValueError:
                pass
        elif line.startswith("overlay_clouds = "):
            cur.bg_overlay_clouds = line.split("=", 1)[1].strip().lower() == "true"
        elif line.startswith("particles = "):
            try:
                cur.bg_particles = int(line.split("=", 1)[1].strip())
            except ValueError:
                pass

    return nodes


def resolve_res_path(path: str | None, uid_map: dict[str, Path]) -> Path | None:
    if not path:
        return None
    if path.startswith("uid://"):
        return uid_map.get(path)
    if path.startswith("res://"):
        rel = path.replace("res://", "")
        return GODOT_ROOT / rel
    return None


def merge_nodes(base_nodes: list[Node], override_nodes: list[Node]) -> list[Node]:
    # Merge by (parent, name). Apply overrides only when the field is explicitly
    # provided in the override scene (or True for `exit_only`).
    merged = [Node(**n.__dict__) for n in base_nodes]
    index_by_key = {(n.parent, n.name): i for i, n in enumerate(merged)}

    for o in override_nodes:
        key = (o.parent, o.name)
        if key in index_by_key:
            b = merged[index_by_key[key]]
            if o.resource_path is not None:
                b.resource_path = o.resource_path
            if o.position is not None:
                b.position = o.position
            if o.tile_map_data is not None:
                b.tile_map_data = o.tile_map_data
            if o.pipe_id is not None:
                b.pipe_id = o.pipe_id
            if o.exit_only:
                b.exit_only = True
            if o.target_level is not None:
                b.target_level = o.target_level
            if o.target_sub_level is not None:
                b.target_sub_level = o.target_sub_level
            if o.enter_direction is not None:
                b.enter_direction = o.enter_direction
            if o.connecting_pipe is not None:
                b.connecting_pipe = o.connecting_pipe
            if o.vertical_direction is not None:
                b.vertical_direction = o.vertical_direction
            if o.top is not None:
                b.top = o.top
            if o.linked_platform is not None:
                b.linked_platform = o.linked_platform
            if o.rope_top is not None:
                b.rope_top = o.rope_top
            if o.bg_primary_layer is not None:
                b.bg_primary_layer = o.bg_primary_layer
            if o.bg_second_layer is not None:
                b.bg_second_layer = o.bg_second_layer
            if o.bg_overlay_clouds is not None:
                b.bg_overlay_clouds = o.bg_overlay_clouds
            if o.bg_particles is not None:
                b.bg_particles = o.bg_particles
        else:
            index_by_key[key] = len(merged)
            merged.append(o)
    return merged


def parse_scene_with_inheritance(
    path: Path, uid_map: dict[str, Path], depth: int = 0
) -> tuple[list[str], dict[str, str], list[Node]]:
    lines = path.read_text(errors="ignore").splitlines()
    ext = parse_ext_resources(lines)
    nodes = parse_nodes(lines, ext)

    # Many SMB1 sub-scenes (e.g. `1-2a.tscn`) instance a base `.tscn` and only
    # override a few fields. Merge the base scene so we don't lose inherited
    # PipeArea positions/types.
    if depth < 4:
        root_inst = next(
            (
                n
                for n in nodes
                if n.parent == "" and n.resource_path and n.resource_path.endswith(".tscn")
            ),
            None,
        )
        base_path = resolve_res_path(root_inst.resource_path, uid_map) if root_inst else None
        if base_path and base_path.exists() and base_path != path:
            base_lines, base_ext, base_nodes = parse_scene_with_inheritance(
                base_path, uid_map, depth + 1
            )
            nodes = merge_nodes(base_nodes, nodes)
            # Keep ext from the child scene; resource paths for instanced nodes
            # are already resolved during parsing.
    return lines, ext, nodes


def parse_tileset_scene_map() -> tuple[int, dict[int, str]]:
    tiles_path = GODOT_ROOT / "Scenes/Parts/Tiles.tscn"
    if not tiles_path.exists():
        return 1, {}
    lines = tiles_path.read_text(errors="ignore").splitlines()
    ext = parse_ext_resources(lines)

    scene_source_index = 1
    for line in lines:
        m = re.search(r'sources/(\d+) = SubResource\("TileSetScenesCollectionSource', line)
        if m:
            scene_source_index = int(m.group(1))
            break

    scene_map: dict[int, str] = {}
    in_scenes = False
    for line in lines:
        if line.startswith("[sub_resource type=\"TileSetScenesCollectionSource\""):
            in_scenes = True
            continue
        if in_scenes and line.startswith("["):
            break
        if not in_scenes:
            continue
        m = re.search(r'scenes/(\d+)/scene = ExtResource\("([^"]+)"\)', line)
        if m:
            scene_id = int(m.group(1))
            ext_id = m.group(2)
            if ext_id in ext:
                scene_map[scene_id] = ext[ext_id]
    return scene_source_index, scene_map


def parse_tileset_atlas_kinds_and_collision() -> tuple[dict[int, int], dict[tuple[int, int, int], int]]:
    tiles_path = GODOT_ROOT / "Scenes/Parts/Tiles.tscn"
    if not tiles_path.exists():
        return {}, {}

    lines = tiles_path.read_text(errors="ignore").splitlines()
    ext = parse_ext_resources(lines)

    atlas_subres_to_tex: dict[str, str] = {}
    atlas_subres_to_collide: dict[str, dict[tuple[int, int], int]] = {}

    cur_atlas: str | None = None
    for raw in lines:
        line = raw.strip()
        m = re.match(r'^\[sub_resource type="TileSetAtlasSource" id="([^"]+)"\]', line)
        if m:
            cur_atlas = m.group(1)
            atlas_subres_to_collide[cur_atlas] = {}
            continue
        if cur_atlas is None:
            continue
        if line.startswith("["):
            cur_atlas = None
            continue

        if line.startswith("texture = ExtResource("):
            mtex = re.search(r'ExtResource\("([^"]+)"\)', line)
            if mtex:
                tex_path = ext.get(mtex.group(1))
                if tex_path:
                    atlas_subres_to_tex[cur_atlas] = tex_path
            continue

        # Only export collisions for physics_layer_0. The tileset also contains
        # extra physics layers (different collision layer/mask) that are not
        # meant to block the player.
        mp = re.match(
            r"^(\d+):(\d+)/\d+/physics_layer_0/polygon_\d+/points\s*=",
            line,
        )
        if mp:
            ax = int(mp.group(1))
            ay = int(mp.group(2))
            prev = atlas_subres_to_collide[cur_atlas].get((ax, ay), COL_NONE)
            if prev != COL_ONEWAY:
                atlas_subres_to_collide[cur_atlas][(ax, ay)] = COL_SOLID
            continue

        mow = re.match(
            r"^(\d+):(\d+)/\d+/physics_layer_0/polygon_\d+/one_way\s*=\s*true",
            line,
        )
        if mow:
            ax = int(mow.group(1))
            ay = int(mow.group(2))
            atlas_subres_to_collide[cur_atlas][(ax, ay)] = COL_ONEWAY
            continue

    source_index_to_kind: dict[int, int] = {}
    source_index_to_atlas_subres: dict[int, str] = {}
    for raw in lines:
        line = raw.strip()
        m = re.match(r'^sources/(\d+)\s*=\s*SubResource\("([^"]+)"\)', line)
        if not m:
            continue
        idx = int(m.group(1))
        sub = m.group(2)
        if sub in atlas_subres_to_collide:
            source_index_to_atlas_subres[idx] = sub

    for idx, sub in source_index_to_atlas_subres.items():
        tex_path = atlas_subres_to_tex.get(sub, "")
        if "/Tilesets/Terrain/" in tex_path:
            source_index_to_kind[idx] = ATLAS_TERRAIN
        elif "/Tilesets/Deco/" in tex_path:
            source_index_to_kind[idx] = ATLAS_DECO
        elif "Liquids" in tex_path or "/Tilesets/Liquid" in tex_path:
            source_index_to_kind[idx] = ATLAS_LIQUID
        else:
            source_index_to_kind[idx] = ATLAS_NONE

    collision_by_source: dict[tuple[int, int, int], int] = {}
    for idx, sub in source_index_to_atlas_subres.items():
        for (ax, ay), ct in atlas_subres_to_collide.get(sub, {}).items():
            collision_by_source[(idx, ax, ay)] = ct

    return source_index_to_kind, collision_by_source


def decode_tile_map_data(encoded: str) -> list[tuple[int, int, int, int, int, int]]:
    data = base64.b64decode(encoded)
    if len(data) < 2:
        return []
    data = data[2:]
    if len(data) % 4 != 0:
        data = data[: len(data) - (len(data) % 4)]
    ints = struct.unpack("<%di" % (len(data) // 4), data)

    def split16(v: int) -> tuple[int, int]:
        lo = v & 0xFFFF
        hi = (v >> 16) & 0xFFFF
        if lo >= 0x8000:
            lo -= 0x10000
        if hi >= 0x8000:
            hi -= 0x10000
        return lo, hi

    cells: list[tuple[int, int, int, int, int, int]] = []
    for i in range(0, len(ints), 3):
        if i + 2 >= len(ints):
            break
        x, y = split16(ints[i])
        source, ax = split16(ints[i + 1])
        ay, alt = split16(ints[i + 2])
        cells.append((x, y, source, ax, ay, alt))
    return cells


def parse_level_id(path: Path) -> tuple[int, int, int] | None:
    name = path.stem  # e.g. 1-1a
    m = re.match(r"(\d+)-(\d+)([a-z]?)", name)
    if not m:
        return None
    world = int(m.group(1))
    stage = int(m.group(2))
    suffix = m.group(3)
    section = 0
    if suffix:
        section = 1 + (ord(suffix) - ord("a"))
    return (world, stage, section)


def resolve_target_level(target: str | None, uid_map: dict[str, Path]) -> Path | None:
    if not target:
        return None
    if target.startswith("uid://"):
        return uid_map.get(target)
    if target.startswith("res://"):
        rel = target.replace("res://", "")
        return GODOT_ROOT / rel
    return None


def clamp(val: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, val))


def to_tile(pos: tuple[float, float], x_offset: int, y_offset: int) -> tuple[int, int]:
    tx = int(round(pos[0] / TILE)) + x_offset
    ty = int(round(pos[1] / TILE)) + y_offset
    return tx, ty


def to_world_px(pos: tuple[float, float], x_offset: int, y_offset: int) -> tuple[float, float]:
    # Convert Godot world pixels into our world pixels by applying the same
    # tile-map offsets used for `to_tile`. This keeps instanced non-tile objects
    # (platforms, generators, etc.) aligned with the exported map.
    return (pos[0] + x_offset * TILE, pos[1] + y_offset * TILE)


def build_levels():
    uid_map = parse_uid_map()
    scene_source_index, scene_map = parse_tileset_scene_map()
    source_kind, collision_by_source = parse_tileset_atlas_kinds_and_collision()
    terrain_source = next((k for k, v in sorted(source_kind.items()) if v == ATLAS_TERRAIN), 0)

    def compute_bottom_terrain_y(
        cells: list[tuple[int, int, int, int, int, int]],
        fallback: int,
    ) -> int:
        # Some levels contain a handful of outlier terrain tiles far below the
        # playable area (e.g. editor artifacts). Using the absolute max Y would
        # shift the whole stage up into the ceiling. Instead, find the bottom
        # "dominant" terrain row by filtering out very-low-frequency rows.
        from collections import Counter

        ys = [c[1] for c in cells if source_kind.get(c[2], ATLAS_NONE) == ATLAS_TERRAIN]
        if not ys:
            return fallback
        counts = Counter(ys)
        maxc = max(counts.values())
        # Keep rows that have at least ~12.5% of the density of the most common
        # terrain row (and at least a few tiles).
        thresh = max(3, maxc // 8)
        candidates = [y for y, cnt in counts.items() if cnt >= thresh]
        if not candidates:
            return max(ys)
        return max(candidates)

    level_files = sorted(LEVELS_ROOT.rglob("*.tscn"))
    sections = []
    section_key_to_index: dict[tuple[int, int, int], int] = {}

    for path in level_files:
        lines, ext, nodes = parse_scene_with_inheritance(path, uid_map)
        music_path = None
        for line in lines:
            if "music = ExtResource" in line:
                m = re.search(r'ExtResource\("([^"]+)"\)', line)
                if m and m.group(1) in ext:
                    music_path = ext[m.group(1)]
                break

        # Levels use multiple TileMap layers. Critically, the "Tiles" layer and
        # "DecoTiles" layer reference different TileSets (different source ids),
        # so we must preserve which TileMap a cell came from.
        tile_nodes = [n for n in nodes if n.tile_map_data]
        if not tile_nodes:
            continue

        tile_layers: list[tuple[bool, list[tuple[int, int, int, int, int, int]]]] = []
        all_cells: list[tuple[int, int, int, int, int, int]] = []
        terrain_cells: list[tuple[int, int, int, int, int, int]] = []
        for tn in tile_nodes:
            decoded = decode_tile_map_data(tn.tile_map_data or "")
            if not decoded:
                continue
            is_deco_layer = tn.name == "DecoTiles"
            tile_layers.append((is_deco_layer, decoded))
            all_cells.extend(decoded)
            if not is_deco_layer:
                terrain_cells.extend(decoded)

        if not all_cells:
            continue

        xs = [c[0] for c in all_cells]
        ys = [c[1] for c in all_cells]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        map_width_tiles = max_x - min_x + 1
        x_offset = -min_x
        # Shift so the second-to-last terrain row becomes the bottom row after
        # dropping the lowest terrain layer.
        terrain_for_bottom = terrain_cells if terrain_cells else all_cells
        max_y_terrain = compute_bottom_terrain_y(terrain_for_bottom, max_y)
        y_offset = MAP_H - max_y_terrain

        # base map
        grid = [[0 for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_t = [[ATLAS_NONE for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_x = [[255 for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_y = [[255 for _ in range(MAP_W)] for _ in range(MAP_H)]
        collide = [[COL_NONE for _ in range(MAP_W)] for _ in range(MAP_H)]
        qmeta = [[0 for _ in range(MAP_W)] for _ in range(MAP_H)]
        # Apply terrain/scene first, then apply DecoTiles (never overwriting
        # gameplay tiles).
        tile_layers.sort(key=lambda it: 1 if it[0] else 0)
        for is_deco_layer, layer_cells in tile_layers:
            for x, y, source, ax, ay, _alt in layer_cells:
                # The SMB1 terrain set encodes base ground as three stacked rows
                # at the bottom. To match the classic two-row ground thickness,
                # drop the lowest of those three rows (terrain layer only).
                if (not is_deco_layer) and source_kind.get(source, ATLAS_NONE) == ATLAS_TERRAIN and y == max_y_terrain:
                    continue

                tx = x + x_offset
                ty = y + y_offset
                if not (0 <= tx < MAP_W and 0 <= ty < MAP_H):
                    continue

                if source == scene_source_index:
                    # Scene tiles (blocks, etc.) - map to gameplay tiles.
                    scene_id = _alt if _alt > 0 else (ax + 1)
                    scene_path = scene_map.get(scene_id)
                    if scene_path and "DeathPit" in scene_path:
                        grid[ty][tx] = 0  # T_EMPTY
                        collide[ty][tx] = COL_NONE
                    elif (
                        scene_path
                        and "/Scenes/Prefabs/Entities/Items/" in scene_path
                        and "Coin" in scene_path
                    ):
                        # Collectible coin entities placed in the TileMap.
                        grid[ty][tx] = 8  # T_COIN
                        collide[ty][tx] = COL_NONE
                    elif scene_path and "QuestionBlock" in scene_path:
                        grid[ty][tx] = 3  # T_QUESTION
                        qmeta[ty][tx] = question_meta_for_path(scene_path)
                        collide[ty][tx] = COL_SOLID
                    elif scene_path and "BrickBlock" in scene_path:
                        grid[ty][tx] = 2  # T_BRICK
                        collide[ty][tx] = COL_SOLID
                    else:
                        grid[ty][tx] = 1  # default solid tile (visible)
                        collide[ty][tx] = COL_SOLID
                    atlas_t[ty][tx] = ATLAS_NONE
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
                    continue

                if is_deco_layer:
                    # The DecoTiles TileMap is always decorative and should
                    # never block the player.
                    if grid[ty][tx] != 0:
                        continue
                    if atlas_x[ty][tx] != 255 or atlas_y[ty][tx] != 255:
                        continue
                    grid[ty][tx] = 255
                    atlas_t[ty][tx] = ATLAS_DECO
                    atlas_x[ty][tx] = ax & 0xFF
                    atlas_y[ty][tx] = ay & 0xFF
                    collide[ty][tx] = COL_NONE
                    continue

                kind = source_kind.get(source, ATLAS_NONE)
                if kind == ATLAS_NONE:
                    continue
                # Never let atlas-deco tiles overwrite terrain/scene tiles at
                # the same coordinate.
                if kind == ATLAS_DECO:
                    if grid[ty][tx] != 0:
                        continue
                    if atlas_x[ty][tx] != 255 or atlas_y[ty][tx] != 255:
                        continue
                grid[ty][tx] = 255
                atlas_t[ty][tx] = kind
                atlas_x[ty][tx] = ax & 0xFF
                atlas_y[ty][tx] = ay & 0xFF
                collide[ty][tx] = collision_by_source.get(
                    (source, ax & 0xFF, ay & 0xFF), COL_NONE
                )

        # node-driven overlays / metadata
        pipes_entry = []
        pipes_exit: dict[int, tuple[int, int]] = {}
        pipe_pos_by_name: dict[str, tuple[int, int]] = {}
        # Enemy / object spawns (including moving platforms + generators).
        # Tuple layout matches `EnemySpawn`:
        #   (etype, x_px, y_px, dir, a, b)
        enemies = []
        flag_x = 0
        has_flag = False
        castle_pos = None
        spawn_pos = None
        bg_primary = 0
        bg_secondary = 0
        bg_clouds = False
        bg_particles = 0
        rope_platform_nodes: dict[str, tuple[float, float, int, str | None, int | None]] = {}

        for n in nodes:
            if not n.resource_path:
                continue
            if n.parent.startswith("ChallengeModeNodes"):
                continue

            res = n.resource_path
            if res.endswith("LevelBG.tscn"):
                bg_primary = n.bg_primary_layer or 0
                bg_secondary = n.bg_second_layer or 0
                bg_clouds = bool(n.bg_overlay_clouds)
                bg_particles = n.bg_particles or 0
                continue
            if n.position is None:
                continue

            # Rope elevator platforms are paired objects linked via NodePath.
            # Export them after scanning the whole scene so we can assign stable
            # pair ids.
            if "RopeElevatorPlatform.tscn" in res:
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                plat_w = 32 if "SmallRopeElevatorPlatform.tscn" in res else 48
                rope_platform_nodes[n.name] = (
                    wx,
                    wy,
                    plat_w,
                    n.linked_platform,
                    n.rope_top,
                )
                continue
            if "Player.tscn" in res:
                spawn_pos = n.position
            if "EndFlagpole.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                flag_x = tx
                has_flag = True
            if "EndSmallCastle.tscn" in res or "EndFinalCastle.tscn" in res:
                castle_pos = to_tile(n.position, x_offset, y_offset)

            if "Goomba.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                enemies.append(("E_GOOMBA", tx * TILE, (ty - 1) * TILE, -1, 0, 0))
            if "KoopaTroopa.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                et = "E_KOOPA_RED" if "RedKoopaTroopa.tscn" in res else "E_KOOPA"
                enemies.append((et, tx * TILE, (ty - 1) * TILE, -1, 0, 0))
            if "BuzzyBeetle.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                enemies.append(("E_BUZZY_BEETLE", tx * TILE, (ty - 1) * TILE, -1, 0, 0))
            if "Blooper.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                enemies.append(("E_BLOOPER", tx * TILE, (ty - 1) * TILE, -1, 0, 0))
            if "HammerBro.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                enemies.append(("E_HAMMER_BRO", tx * TILE, (ty - 1) * TILE, -1, 0, 0))
            if "Lakitu.tscn" in res:
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_LAKITU", wx, wy, -1, 0, 0))

            if "CheepCheep.tscn" in res:
                # Underwater Cheep-Cheeps (Green/Red).
                tx, ty = to_tile(n.position, x_offset, y_offset)
                enemies.append(("E_CHEEP_SWIM", tx * TILE, (ty - 1) * TILE, -1, 0, 0))

            # Moving platforms (instanced scenes, not TileMap tiles).
            if "SidewaysPlatform.tscn" in res:
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                plat_w = 32 if "SmallSidewaysPlatform.tscn" in res else 48
                enemies.append(("E_PLATFORM_SIDEWAYS", wx - plat_w / 2, wy, 0, plat_w, 0))
            if "VerticalPlatform.tscn" in res:
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                plat_w = 32 if "SmallVerticalPlatform.tscn" in res else 48
                enemies.append(("E_PLATFORM_VERTICAL", wx - plat_w / 2, wy, 0, plat_w, 0))
            if "FallingPlatform.tscn" in res:
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_PLATFORM_FALLING", wx - 24, wy, 0, 48, 0))

            # Classic SMB elevators in 1-2 (wrap between top..bottom).
            if "ElevatorPlatform.tscn" in res:
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                vertical_dir = n.vertical_direction if n.vertical_direction is not None else 1
                top = n.top
                if top is None:
                    # Defaults from the upstream scenes/scripts.
                    if "SmallElevatorPlatform.tscn" in res or "MediumElevatorPlatform.tscn" in res:
                        top = -176
                    else:
                        top = -244
                top_world = int(top + y_offset * TILE)
                bottom_world = int(64 + y_offset * TILE)
                enemies.append(("E_PLATFORM_VERTICAL", wx - 24, wy, vertical_dir, top_world, bottom_world))

            # Off-screen generators / stoppers (EntityGenerator).
            if res.endswith("CheepCheepGenerator.tscn"):
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_ENTITY_GENERATOR", wx, wy, 1, "E_CHEEP_LEAP", 1000))
            if res.endswith("BulletBillGenerator.tscn"):
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_ENTITY_GENERATOR", wx, wy, 0, "E_BULLET_BILL", 2000))
            if res.endswith("EntityGeneratorStopper.tscn"):
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_ENTITY_GENERATOR_STOP", wx, wy, 0, 0, 0))
            if res.endswith("BulletBillCannon.tscn"):
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_BULLET_CANNON", wx, wy, 0, 0, 0))

            # Castle completion (axe) + Bowser.
            if res.endswith("CastleBridge.tscn"):
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_CASTLE_AXE", wx + 208, wy - 32, 0, 0, 0))
            if res.endswith("Bowser.tscn"):
                wx, wy = to_world_px(n.position, x_offset, y_offset)
                enemies.append(("E_BOWSER", wx, wy, -1, 0, 0))

            if "BrickBlock" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                if 0 <= tx < MAP_W and 0 <= ty < MAP_H:
                    grid[ty][tx] = 2  # T_BRICK
                    collide[ty][tx] = COL_SOLID
                    atlas_t[ty][tx] = ATLAS_NONE
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
            if "QuestionBlock" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                if 0 <= tx < MAP_W and 0 <= ty < MAP_H:
                    grid[ty][tx] = 3  # T_QUESTION
                    collide[ty][tx] = COL_SOLID
                    atlas_t[ty][tx] = ATLAS_NONE
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
                    qmeta[ty][tx] = question_meta_for_path(res)

            if res.endswith("PipeArea.tscn") or res.endswith("AutoExitPipeArea.tscn") or res.endswith("TeleportPipeArea.tscn") or res.endswith("WarpPipeArea.tscn"):
                tx, ty = to_tile(n.position, x_offset, y_offset)
                pipe_pos_by_name[n.name] = (tx, ty)
                # For collision-only safety, optionally stamp a vertical pipe
                # column IF tiles are missing. Avoid stamping sideways pipes,
                # and avoid overriding real atlas-rendered pipes (causes
                # duplicate/incorrect visuals in the port).
                enter_dir = n.enter_direction if n.enter_direction is not None else 0
                if enter_dir in (0, 1):
                    ground_y = min(max_y + y_offset, MAP_H - 1)
                    for y in range(ty, ground_y + 1):
                        if 0 <= y < MAP_H and 0 <= tx < MAP_W:
                            if grid[y][tx] == 0 and atlas_x[y][tx] == 255 and atlas_y[y][tx] == 255:
                                # Collision-only marker: keep the tile visually empty so it never renders,
                                # but ensure collision exists for the PipeArea bounds.
                                collide[y][tx] = COL_SOLID
                        if 0 <= y < MAP_H and 0 <= tx + 1 < MAP_W:
                            if grid[y][tx + 1] == 0 and atlas_x[y][tx + 1] == 255 and atlas_y[y][tx + 1] == 255:
                                collide[y][tx + 1] = COL_SOLID
                if res.endswith("TeleportPipeArea.tscn") and n.connecting_pipe and not n.exit_only:
                    pipes_entry.append(
                        {
                            "pipe_id": n.pipe_id or 0,
                            "tx": tx,
                            "ty": ty,
                            "target_level": None,
                            "target_sub_level": 0,
                            "enter_dir": n.enter_direction if n.enter_direction is not None else 0,
                            "connect_name": n.connecting_pipe,
                        }
                    )
                else:
                    pid = n.pipe_id or 0
                    if n.exit_only:
                        pipes_exit[pid] = (tx, ty)
                    else:
                        pipes_entry.append(
                            {
                                "pipe_id": pid,
                                "tx": tx,
                                "ty": ty,
                                "target_level": n.target_level,
                                "target_sub_level": n.target_sub_level or 0,
                                "enter_dir": n.enter_direction if n.enter_direction is not None else 0,
                            }
                        )

        # Export RopeElevatorPlatform pairs now that we've seen all nodes.
        if rope_platform_nodes:
            pair_id = 1
            assigned: dict[str, int] = {}
            for name, (_wx, _wy, _w, linked, _rope_top) in rope_platform_nodes.items():
                if name in assigned:
                    continue
                pid = pair_id
                pair_id += 1
                assigned[name] = pid
                if linked and linked in rope_platform_nodes:
                    assigned[linked] = pid

            for name, (wx, wy, width, _linked, rope_top) in rope_platform_nodes.items():
                pid = assigned.get(name, 0)
                if pid == 0:
                    continue
                top = rope_top if rope_top is not None else -160
                top_world = int(top + y_offset * TILE)
                enemies.append(("E_PLATFORM_ROPE", wx - width / 2, wy, pid, width, top_world))

        # castle stamp
        if castle_pos:
            cx, cy = castle_pos
            for y in range(cy - 2, cy + 3):
                for x in range(cx, cx + 5):
                    if 0 <= x < MAP_W and 0 <= y < MAP_H:
                        grid[y][x] = 7  # T_CASTLE
                        collide[y][x] = COL_SOLID

        # spawn
        if spawn_pos:
            sx, sy = to_tile(spawn_pos, x_offset, y_offset)
        else:
            sx, sy = 3, max_y + y_offset - 1
        sx_px = clamp(sx, 0, MAP_W - 1) * TILE
        sy_px = clamp(sy, 0, MAP_H - 1) * TILE

        level_id = parse_level_id(path)
        if not level_id:
            continue
        world, stage, section = level_id
        section_key_to_index[(world, stage, section)] = len(sections)
        theme = "THEME_OVERWORLD"
        if music_path:
            if "Underground" in music_path:
                theme = "THEME_UNDERGROUND"
            elif "CastleWater" in music_path:
                theme = "THEME_CASTLE_WATER"
            elif "Castle" in music_path or "Bowser" in music_path:
                theme = "THEME_CASTLE"
            elif "Underwater" in music_path:
                theme = "THEME_UNDERWATER"
            elif "Airship" in music_path:
                theme = "THEME_AIRSHIP"
            elif "Desert" in music_path:
                theme = "THEME_DESERT"
            elif "Snow" in music_path:
                theme = "THEME_SNOW"
            elif "Jungle" in music_path:
                theme = "THEME_JUNGLE"
            elif "Beach" in music_path:
                theme = "THEME_BEACH"
            elif "Garden" in music_path:
                theme = "THEME_GARDEN"
            elif "Mountain" in music_path:
                theme = "THEME_MOUNTAIN"
            elif "Sky" in music_path:
                theme = "THEME_SKY"
            elif "Autumn" in music_path:
                theme = "THEME_AUTUMN"
            elif "PipeLand" in music_path or "Pipeland" in music_path:
                theme = "THEME_PIPELAND"
            elif "Space" in music_path:
                theme = "THEME_SPACE"
            elif "Volcano" in music_path:
                theme = "THEME_VOLCANO"
            elif "GhostHouse" in music_path or "Ghost" in music_path:
                theme = "THEME_GHOSTHOUSE"
            elif "Bonus" in music_path or "CoinHeaven" in music_path:
                theme = "THEME_BONUS"

        sections.append(
            {
                "path": path,
                "world": world,
                "stage": stage,
                "section": section,
                "grid": grid,
                "atlas_t": atlas_t,
                "atlas_x": atlas_x,
                "atlas_y": atlas_y,
                "collide": collide,
                "qmeta": qmeta,
                "map_width": min(map_width_tiles, MAP_W),
                "map_height": MAP_H,
                "theme": theme,
                "flag_x": flag_x,
                "has_flag": has_flag,
                "bg_primary": bg_primary,
                "bg_secondary": bg_secondary,
                "bg_clouds": bg_clouds,
                "bg_particles": bg_particles,
                "pipes_entry": pipes_entry,
                "pipes_exit": pipes_exit,
                "pipe_pos_by_name": pipe_pos_by_name,
                "enemies": enemies,
                "start_x": sx_px,
                "start_y": sy_px,
            }
        )

    # build level list by world/stage
    level_keys = sorted({(s["world"], s["stage"]) for s in sections})
    level_index_map = {k: i for i, k in enumerate(level_keys)}

    # build sections per level
    level_sections: dict[tuple[int, int], list[dict]] = {k: [] for k in level_keys}
    for s in sections:
        level_sections[(s["world"], s["stage"])].append(s)

    def parse_special_section(path: Path, world: int, stage: int, section: int):
        # Re-run the same parsing logic for a special scene (e.g. UndergroundExit)
        # but force it into the given (world, stage, section) bucket.
        lines, ext, nodes = parse_scene_with_inheritance(path, uid_map)
        music_path = None
        for line in lines:
            if "music = ExtResource" in line:
                m = re.search(r'ExtResource\("([^"]+)"\)', line)
                if m and m.group(1) in ext:
                    music_path = ext[m.group(1)]
                break

        tile_node = next((n for n in nodes if n.tile_map_data), None)
        if not tile_node:
            return None
        cells = decode_tile_map_data(tile_node.tile_map_data or "")
        if not cells:
            return None

        xs = [c[0] for c in cells]
        ys = [c[1] for c in cells]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        width = max_x - min_x + 1
        x_offset = -min_x
        max_y_terrain = compute_bottom_terrain_y(cells, max_y)
        y_offset = MAP_H - max_y_terrain

        grid = [[0 for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_t = [[ATLAS_NONE for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_x = [[255 for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_y = [[255 for _ in range(MAP_W)] for _ in range(MAP_H)]
        collide = [[COL_NONE for _ in range(MAP_W)] for _ in range(MAP_H)]
        qmeta = [[0 for _ in range(MAP_W)] for _ in range(MAP_H)]

        for x, y, source, ax, ay, _alt in cells:
            if source_kind.get(source, ATLAS_NONE) == ATLAS_TERRAIN and y == max_y_terrain:
                continue
            tx = x + x_offset
            ty = y + y_offset
            if 0 <= tx < MAP_W and 0 <= ty < MAP_H:
                if source == scene_source_index:
                    scene_id = _alt if _alt > 0 else (ax + 1)
                    scene_path = scene_map.get(scene_id)
                    if scene_path and "DeathPit" in scene_path:
                        grid[ty][tx] = 0
                        collide[ty][tx] = COL_NONE
                    elif (
                        scene_path
                        and "/Scenes/Prefabs/Entities/Items/" in scene_path
                        and "Coin" in scene_path
                    ):
                        grid[ty][tx] = 8  # T_COIN
                        collide[ty][tx] = COL_NONE
                    elif scene_path and "QuestionBlock" in scene_path:
                        grid[ty][tx] = 3
                        qmeta[ty][tx] = question_meta_for_path(scene_path)
                        collide[ty][tx] = COL_SOLID
                    elif scene_path and "BrickBlock" in scene_path:
                        grid[ty][tx] = 2
                        collide[ty][tx] = COL_SOLID
                    else:
                        grid[ty][tx] = 1
                        collide[ty][tx] = COL_SOLID
                    atlas_t[ty][tx] = ATLAS_NONE
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
                else:
                    kind = source_kind.get(source, ATLAS_NONE)
                    if kind == ATLAS_NONE:
                        continue
                    grid[ty][tx] = 255
                    atlas_t[ty][tx] = kind
                    atlas_x[ty][tx] = ax & 0xFF
                    atlas_y[ty][tx] = ay & 0xFF
                    collide[ty][tx] = collision_by_source.get(
                        (source, ax & 0xFF, ay & 0xFF), COL_NONE
                    )

        pipes_entry = []
        pipes_exit: dict[int, tuple[int, int]] = {}
        pipe_pos_by_name: dict[str, tuple[int, int]] = {}
        enemies = []
        flag_x = 0
        has_flag = False
        castle_pos = None
        spawn_pos = None
        bg_primary = 0
        bg_secondary = 0
        bg_clouds = False
        bg_particles = 0

        for n in nodes:
            if not n.resource_path:
                continue
            if n.parent.startswith("ChallengeModeNodes"):
                continue
            res = n.resource_path
            if res.endswith("LevelBG.tscn"):
                bg_primary = n.bg_primary_layer or 0
                bg_secondary = n.bg_second_layer or 0
                bg_clouds = bool(n.bg_overlay_clouds)
                bg_particles = n.bg_particles or 0
                continue
            if n.position is None:
                continue
            if "Player.tscn" in res:
                spawn_pos = n.position
            if "EndFlagpole.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                flag_x = tx
                has_flag = True
            if "EndSmallCastle.tscn" in res or "EndFinalCastle.tscn" in res:
                castle_pos = to_tile(n.position, x_offset, y_offset)

            if res.endswith("PipeArea.tscn") or res.endswith("AutoExitPipeArea.tscn") or res.endswith("TeleportPipeArea.tscn") or res.endswith("WarpPipeArea.tscn"):
                tx, ty = to_tile(n.position, x_offset, y_offset)
                pipe_pos_by_name[n.name] = (tx, ty)
                pid = n.pipe_id or 0
                if n.exit_only:
                    pipes_exit[pid] = (tx, ty)
                else:
                    pipes_entry.append(
                        {
                            "pipe_id": pid,
                            "tx": tx,
                            "ty": ty,
                            "target_level": n.target_level,
                            "target_sub_level": n.target_sub_level or 0,
                            "enter_dir": n.enter_direction if n.enter_direction is not None else 0,
                        }
                    )

        if castle_pos:
            cx, cy = castle_pos
            for y in range(cy - 2, cy + 3):
                for x in range(cx, cx + 5):
                    if 0 <= x < MAP_W and 0 <= y < MAP_H:
                        grid[y][x] = 7  # T_CASTLE
                        collide[y][x] = COL_SOLID

        if spawn_pos:
            sx, sy = to_tile(spawn_pos, x_offset, y_offset)
        else:
            sx, sy = 3, max_y + y_offset - 1
        sx_px = clamp(sx, 0, MAP_W - 1) * TILE
        sy_px = clamp(sy, 0, MAP_H - 1) * TILE

        theme = "THEME_OVERWORLD"
        if music_path:
            if "Underground" in music_path:
                theme = "THEME_UNDERGROUND"
            elif "CastleWater" in music_path:
                theme = "THEME_CASTLE_WATER"
            elif "Castle" in music_path or "Bowser" in music_path:
                theme = "THEME_CASTLE"
            elif "Underwater" in music_path:
                theme = "THEME_UNDERWATER"
        return {
            "path": path,
            "world": world,
            "stage": stage,
            "section": section,
            "grid": grid,
            "atlas_t": atlas_t,
            "atlas_x": atlas_x,
            "atlas_y": atlas_y,
            "collide": collide,
            "qmeta": qmeta,
            "map_width": min(width, MAP_W),
            "map_height": MAP_H,
            "theme": theme,
            "flag_x": flag_x,
            "has_flag": has_flag,
            "bg_primary": bg_primary,
            "bg_secondary": bg_secondary,
            "bg_clouds": bg_clouds,
            "bg_particles": bg_particles,
            "pipes_entry": pipes_entry,
            "pipes_exit": pipes_exit,
            "pipe_pos_by_name": pipe_pos_by_name,
            "enemies": enemies,
            "start_x": sx_px,
            "start_y": sy_px,
        }

    # resolve pipes
    special_by_level: dict[tuple[int, int, str], int] = {}
    special_sections: list[dict] = []
    for s in sections:
        for entry in s["pipes_entry"]:
            if entry.get("connect_name"):
                target_pos = s["pipe_pos_by_name"].get(entry["connect_name"])
                if target_pos:
                    target_x, target_y = target_pos
                    tgt_key = (s["world"], s["stage"])
                    if tgt_key in level_index_map:
                        entry["target_level_index"] = level_index_map[tgt_key]
                        entry["target_section_number"] = s["section"]
                        entry["target_x_px"] = target_x * TILE
                        entry["target_y_px"] = target_y * TILE
                continue
            target_path = resolve_target_level(entry["target_level"], uid_map)
            if not target_path:
                continue
            target_id = parse_level_id(target_path)
            if not target_id:
                # Special shared scenes (e.g. UndergroundExit.tscn) - embed as an
                # extra section inside the *current* level so HUD world/stage
                # remains consistent.
                stem = target_path.stem
                if stem in ("UndergroundExit", "UnderwaterExit"):
                    key = (s["world"], s["stage"], stem)
                    if key not in special_by_level:
                        base_key = (s["world"], s["stage"])
                        existing = [sec["section"] for sec in level_sections.get(base_key, [])]
                        next_section = (max(existing) + 1) if existing else 1
                        sec = parse_special_section(target_path, s["world"], s["stage"], next_section)
                        if sec:
                            special_by_level[key] = next_section
                            special_sections.append(sec)
                            level_sections.setdefault(base_key, []).append(sec)
                    if key in special_by_level:
                        base_key = (s["world"], s["stage"])
                        sec_num = special_by_level[key]
                        sec_obj = next(
                            (sec for sec in level_sections.get(base_key, []) if sec["section"] == sec_num),
                            None,
                        )
                        if sec_obj:
                            pid = entry["pipe_id"]
                            exit_pos = sec_obj["pipes_exit"].get(pid)
                            if exit_pos:
                                target_x, target_y = exit_pos
                            else:
                                target_x = sec_obj["start_x"] // TILE
                                target_y = (sec_obj["start_y"] // TILE)
                            entry["target_level_index"] = level_index_map[base_key]
                            entry["target_section_number"] = sec_num
                            entry["target_x_px"] = target_x * TILE
                            entry["target_y_px"] = target_y * TILE
                continue
            t_world, t_stage, t_section = target_id
            if t_section == 0:
                t_section = entry["target_sub_level"]
            tgt_key = (t_world, t_stage)
            if tgt_key not in level_index_map:
                continue
            tgt_level_idx = level_index_map[tgt_key]
            tgt_sections = level_sections[tgt_key]
            target_section = next(
                (sec for sec in tgt_sections if sec["section"] == t_section), None
            )
            if not target_section:
                continue
            pid = entry["pipe_id"]
            exit_pos = target_section["pipes_exit"].get(pid)
            if exit_pos:
                target_x, target_y = exit_pos
            else:
                target_x = target_section["start_x"] // TILE
                target_y = (target_section["start_y"] // TILE)
            entry["target_level_index"] = tgt_level_idx
            entry["target_section_number"] = t_section
            entry["target_x_px"] = target_x * TILE
            entry["target_y_px"] = target_y * TILE

    # Build per-level section-number -> array-index mapping (PipeLink uses array indices).
    section_index_map: dict[tuple[int, int], dict[int, int]] = {}
    for k in level_keys:
        sec_list = sorted(level_sections[k], key=lambda s: s["section"])
        level_sections[k] = sec_list
        section_index_map[k] = {sec["section"]: i for i, sec in enumerate(sec_list)}

    # emit C++
    out = []
    out.append('#include "levels.h"')
    out.append("")

    # maps (include any embedded special sections)
    emit_sections = []
    seen = set()
    for k in level_keys:
        for s in level_sections[k]:
            key = (s["world"], s["stage"], s["section"])
            if key in seen:
                continue
            seen.add(key)
            emit_sections.append(s)

    for s in emit_sections:
        name = f"map_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const uint8_t {name}[MAP_H][MAP_W] = {{")
        for row in s["grid"]:
            out.append("  {" + ",".join(str(v) for v in row) + "},")
        out.append("};")
        out.append("")

        at_name = f"atlast_{s['world']}_{s['stage']}_{s['section']}"
        ax_name = f"atlasx_{s['world']}_{s['stage']}_{s['section']}"
        ay_name = f"atlasy_{s['world']}_{s['stage']}_{s['section']}"
        col_name = f"collide_{s['world']}_{s['stage']}_{s['section']}"
        qm_name = f"qmeta_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const uint8_t {at_name}[MAP_H][MAP_W] = {{")
        for row in s['atlas_t']:
            out.append('  {' + ','.join(str(v) for v in row) + '},')
        out.append("};")
        out.append("")
        out.append(f"static const uint8_t {ax_name}[MAP_H][MAP_W] = {{")
        for row in s["atlas_x"]:
            out.append("  {" + ",".join(str(v) for v in row) + "},")
        out.append("};")
        out.append("")
        out.append(f"static const uint8_t {ay_name}[MAP_H][MAP_W] = {{")
        for row in s["atlas_y"]:
            out.append("  {" + ",".join(str(v) for v in row) + "},")
        out.append("};")
        out.append("")
        out.append(f"static const uint8_t {col_name}[MAP_H][MAP_W] = {{")
        for row in s['collide']:
            out.append('  {' + ','.join(str(v) for v in row) + '},')
        out.append("};")
        out.append("")
        out.append(f"static const uint8_t {qm_name}[MAP_H][MAP_W] = {{")
        for row in s["qmeta"]:
            out.append("  {" + ",".join(str(v) for v in row) + "},")
        out.append("};")
        out.append("")

    # pipes and enemies
    for s in emit_sections:
        pipes = [
            e
            for e in s["pipes_entry"]
            if "target_level_index" in e and "target_section_number" in e
        ]
        pname = f"pipes_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const PipeLink {pname}[] = {{")
        for p in pipes:
            tgt_key = level_keys[p["target_level_index"]]
            tgt_section_idx = section_index_map[tgt_key].get(p["target_section_number"])
            if tgt_section_idx is None:
                continue
            out.append(
                f"  {{{p['tx']}, {p['ty']}, {p['target_level_index']}, {tgt_section_idx}, {p.get('target_x_px', 0)}, {p.get('target_y_px', 0)}, {p.get('enter_dir', 0)}}},"
            )
        out.append("};")
        out.append("")

        ename = f"enemies_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const EnemySpawn {ename}[] = {{")
        for etype, ex, ey, edir, a, b in s["enemies"]:
            # `a` may be an enum constant (string) for generator spawn types.
            a_text = a if isinstance(a, str) else str(int(a))
            out.append(
                f"  {{{etype}, {ex:.1f}f, {ey:.1f}f, {edir}, {a_text}, {int(b)}}},"
            )
        out.append("};")
        out.append("")

    # section defs
    for key in level_keys:
        world, stage = key
        sec_list = sorted(level_sections[key], key=lambda s: s["section"])
        sname = f"sections_{world}_{stage}"
        out.append(f"static const LevelSectionData {sname}[] = {{")
        for s in sec_list:
            map_name = f"map_{s['world']}_{s['stage']}_{s['section']}"
            at_name = f"atlast_{s['world']}_{s['stage']}_{s['section']}"
            ax_name = f"atlasx_{s['world']}_{s['stage']}_{s['section']}"
            ay_name = f"atlasy_{s['world']}_{s['stage']}_{s['section']}"
            col_name = f"collide_{s['world']}_{s['stage']}_{s['section']}"
            qm_name = f"qmeta_{s['world']}_{s['stage']}_{s['section']}"
            pname = f"pipes_{s['world']}_{s['stage']}_{s['section']}"
            ename = f"enemies_{s['world']}_{s['stage']}_{s['section']}"
            pipe_count = len(
                [
                    e
                    for e in s["pipes_entry"]
                    if "target_level_index" in e and "target_section_number" in e
                ]
            )
            enemy_count = len(s["enemies"])
            out.append(
                "  {"
                f"{map_name}, {at_name}, {ax_name}, {ay_name}, {col_name}, {qm_name}, "
                f"{s['map_width']}, {s['map_height']}, {s['theme']}, "
                f"{s['flag_x']}, {str(s['has_flag']).lower()}, "
                f"{s['start_x']}, {s['start_y']}, "
                f"{s.get('bg_primary', 0)}, {s.get('bg_secondary', 0)}, {str(bool(s.get('bg_clouds'))).lower()}, {s.get('bg_particles', 0)}, "
                f"{pname}, {pipe_count}, {ename}, {enemy_count}"
                "},"
            )
        out.append("};")
        out.append("")

    out.append(f"extern const int g_levelCount = {len(level_keys)};")
    out.append("extern const LevelData g_levels[] = {")
    for world, stage in level_keys:
        sname = f"sections_{world}_{stage}"
        name = f"\"{world}-{stage}\""
        out.append(
            f"  {{{name}, {world}, {stage}, {sname}, (int)(sizeof({sname})/sizeof({sname}[0]))}},"
        )
    out.append("};")
    out.append("")

    out_path = ROOT / "src/levels_generated.cpp"
    out_path.write_text("\n".join(out))
    print(f"Generated: {out_path}")


def main() -> int:
    if not GODOT_ROOT.exists():
        print("Missing Super-Mario-Bros.-Remastered-Public folder.")
        return 1
    build_levels()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
