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
            if cur.name == "Tiles":
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

    return nodes


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


def build_levels():
    uid_map = parse_uid_map()
    scene_source_index, scene_map = parse_tileset_scene_map()

    level_files = sorted(LEVELS_ROOT.rglob("*.tscn"))
    sections = []
    section_key_to_index: dict[tuple[int, int, int], int] = {}

    for path in level_files:
        text = path.read_text(errors="ignore")
        lines = text.splitlines()
        ext = parse_ext_resources(lines)
        music_path = None
        for line in lines:
            if "music = ExtResource" in line:
                m = re.search(r'ExtResource\("([^"]+)"\)', line)
                if m and m.group(1) in ext:
                    music_path = ext[m.group(1)]
                break
        nodes = parse_nodes(lines, ext)

        tile_node = next((n for n in nodes if n.tile_map_data), None)
        if not tile_node:
            continue
        cells = decode_tile_map_data(tile_node.tile_map_data or "")
        if not cells:
            continue

        xs = [c[0] for c in cells]
        ys = [c[1] for c in cells]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        width = max_x - min_x + 1
        x_offset = -min_x
        # Shift so the second-to-last terrain row becomes the bottom row after
        # dropping the lowest terrain layer.
        y_offset = MAP_H - max_y

        # base map
        grid = [[0 for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_x = [[255 for _ in range(MAP_W)] for _ in range(MAP_H)]
        atlas_y = [[255 for _ in range(MAP_W)] for _ in range(MAP_H)]
        qmeta = [[0 for _ in range(MAP_W)] for _ in range(MAP_H)]
        for x, y, source, ax, ay, _alt in cells:
            # The SMB1 terrain set encodes base ground as three stacked rows at
            # the bottom. To match the classic two-row ground thickness, drop
            # the lowest of those three rows.
            if source == 0 and y == max_y:
                continue
            tx = x + x_offset
            ty = y + y_offset
            if 0 <= tx < MAP_W and 0 <= ty < MAP_H:
                if source == scene_source_index:
                    # Scene tiles (blocks, etc.) - map to gameplay tiles.
                    scene_id = _alt if _alt > 0 else (ax + 1)
                    scene_path = scene_map.get(scene_id)
                    if scene_path and "DeathPit" in scene_path:
                        grid[ty][tx] = 0  # T_EMPTY
                    elif scene_path and "QuestionBlock" in scene_path:
                        grid[ty][tx] = 3  # T_QUESTION
                        qmeta[ty][tx] = question_meta_for_path(scene_path)
                    elif scene_path and "BrickBlock" in scene_path:
                        grid[ty][tx] = 2  # T_BRICK
                    else:
                        grid[ty][tx] = 1  # default solid
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
                else:
                    grid[ty][tx] = 1  # T_GROUND
                    if source == 0:
                        atlas_x[ty][tx] = ax & 0xFF
                        atlas_y[ty][tx] = ay & 0xFF
                        if ax == 9:
                            grid[ty][tx] = 5  # T_PIPE

        # node-driven overlays / metadata
        pipes_entry = []
        pipes_exit: dict[int, tuple[int, int]] = {}
        pipe_pos_by_name: dict[str, tuple[int, int]] = {}
        enemies = []
        flag_x = 0
        has_flag = False
        castle_pos = None
        spawn_pos = None

        for n in nodes:
            if not n.resource_path or n.position is None:
                continue
            if n.parent.startswith("ChallengeModeNodes"):
                continue

            res = n.resource_path
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
                enemies.append(("E_GOOMBA", tx * TILE, (ty - 1) * TILE, -1))
            if "KoopaTroopa.tscn" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                enemies.append(("E_KOOPA", tx * TILE, (ty - 1) * TILE, -1))

            if "BrickBlock" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                if 0 <= tx < MAP_W and 0 <= ty < MAP_H:
                    grid[ty][tx] = 2  # T_BRICK
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
            if "QuestionBlock" in res:
                tx, ty = to_tile(n.position, x_offset, y_offset)
                if 0 <= tx < MAP_W and 0 <= ty < MAP_H:
                    grid[ty][tx] = 3  # T_QUESTION
                    atlas_x[ty][tx] = 255
                    atlas_y[ty][tx] = 255
                    qmeta[ty][tx] = question_meta_for_path(res)

            if res.endswith("PipeArea.tscn") or res.endswith("AutoExitPipeArea.tscn") or res.endswith("TeleportPipeArea.tscn") or res.endswith("WarpPipeArea.tscn"):
                tx, ty = to_tile(n.position, x_offset, y_offset)
                pipe_pos_by_name[n.name] = (tx, ty)
                # build pipe column down to ground
                ground_y = min(max_y + y_offset, MAP_H - 1)
                for y in range(ty, ground_y + 1):
                    if 0 <= y < MAP_H and 0 <= tx < MAP_W:
                        grid[y][tx] = 5  # T_PIPE
                    if 0 <= y < MAP_H and 0 <= tx + 1 < MAP_W:
                        grid[y][tx + 1] = 5
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

        # castle stamp
        if castle_pos:
            cx, cy = castle_pos
            for y in range(cy - 2, cy + 3):
                for x in range(cx, cx + 5):
                    if 0 <= x < MAP_W and 0 <= y < MAP_H:
                        grid[y][x] = 7  # T_CASTLE

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
                "atlas_x": atlas_x,
                "atlas_y": atlas_y,
                "qmeta": qmeta,
                "map_width": min(width, MAP_W),
                "map_height": MAP_H,
                "theme": theme,
                "flag_x": flag_x,
                "has_flag": has_flag,
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

    # resolve pipes
    for s in sections:
        for entry in s["pipes_entry"]:
            if entry.get("connect_name"):
                target_pos = s["pipe_pos_by_name"].get(entry["connect_name"])
                if target_pos:
                    target_x, target_y = target_pos
                    tgt_key = (s["world"], s["stage"])
                    if tgt_key in level_index_map:
                        entry["target_level_index"] = level_index_map[tgt_key]
                        entry["target_section_index"] = s["section"]
                        entry["target_x_px"] = target_x * TILE
                        entry["target_y_px"] = target_y * TILE
                continue
            target_path = resolve_target_level(entry["target_level"], uid_map)
            if not target_path:
                continue
            target_id = parse_level_id(target_path)
            if not target_id:
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
            entry["target_section_index"] = t_section
            entry["target_x_px"] = target_x * TILE
            entry["target_y_px"] = target_y * TILE

    # emit C++
    out = []
    out.append('#include "levels.h"')
    out.append("")

    # maps
    for s in sections:
        name = f"map_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const uint8_t {name}[MAP_H][MAP_W] = {{")
        for row in s["grid"]:
            out.append("  {" + ",".join(str(v) for v in row) + "},")
        out.append("};")
        out.append("")

        ax_name = f"atlasx_{s['world']}_{s['stage']}_{s['section']}"
        ay_name = f"atlasy_{s['world']}_{s['stage']}_{s['section']}"
        qm_name = f"qmeta_{s['world']}_{s['stage']}_{s['section']}"
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
        out.append(f"static const uint8_t {qm_name}[MAP_H][MAP_W] = {{")
        for row in s["qmeta"]:
            out.append("  {" + ",".join(str(v) for v in row) + "},")
        out.append("};")
        out.append("")

    # pipes and enemies
    for s in sections:
        pipes = [
            e
            for e in s["pipes_entry"]
            if "target_level_index" in e and "target_section_index" in e
        ]
        pname = f"pipes_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const PipeLink {pname}[] = {{")
        for p in pipes:
            out.append(
                f"  {{{p['tx']}, {p['ty']}, {p['target_level_index']}, {p['target_section_index']}, {p['target_x_px']}, {p['target_y_px']}, {p.get('enter_dir', 0)}}},"
            )
        out.append("};")
        out.append("")

        ename = f"enemies_{s['world']}_{s['stage']}_{s['section']}"
        out.append(f"static const EnemySpawn {ename}[] = {{")
        for etype, ex, ey, edir in s["enemies"]:
            out.append(f"  {{{etype}, {ex:.1f}f, {ey:.1f}f, {edir}}},")
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
            ax_name = f"atlasx_{s['world']}_{s['stage']}_{s['section']}"
            ay_name = f"atlasy_{s['world']}_{s['stage']}_{s['section']}"
            qm_name = f"qmeta_{s['world']}_{s['stage']}_{s['section']}"
            pname = f"pipes_{s['world']}_{s['stage']}_{s['section']}"
            ename = f"enemies_{s['world']}_{s['stage']}_{s['section']}"
            pipe_count = len(
                [
                    e
                    for e in s["pipes_entry"]
                    if "target_level_index" in e and "target_section_index" in e
                ]
            )
            enemy_count = len(s["enemies"])
            out.append(
                "  {"
                f"{map_name}, {ax_name}, {ay_name}, {qm_name}, "
                f"{s['map_width']}, {s['map_height']}, {s['theme']}, "
                f"{s['flag_x']}, {str(s['has_flag']).lower()}, "
                f"{s['start_x']}, {s['start_y']}, "
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
