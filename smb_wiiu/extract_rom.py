#!/usr/bin/env python3
"""
SMB NES ROM Asset Extractor (Wii U port)

The Wii U homebrew in `smb_wiiu/` expects a small set of sprite sheets in
`content/sprites/...`. The Super-Mario-Bros.-Remastered project generates these by:
- Reading CHR ROM tiles from an SMB1 NES ROM
- Using JSON tile maps to paste tiles into green “template” PNGs
- Applying palette definitions from JSON files

This script re-implements that extraction flow for the subset of assets used by
the Wii U port, then crops them down to the Wii U port’s expected sizes.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import subprocess
from pathlib import Path

from PIL import Image


OPAQUE_GREEN = (0, 255, 0, 255)
VECTOR2I_RE = re.compile(r"Vector2i\(\s*(\d+)\s*,\s*(\d+)\s*\)")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def remastered_root() -> Path:
    return repo_root() / "Super-Mario-Bros.-Remastered-Public"


def read_chr_rom(rom_path: Path) -> bytes:
    data = rom_path.read_bytes()
    if data[:4] != b"NES\x1a":
        raise ValueError("Not a valid iNES ROM (missing NES header).")

    prg_size = data[4] * 16384
    chr_size = data[5] * 8192
    chr_off = 16 + prg_size
    chr_rom = data[chr_off : chr_off + chr_size]
    print(f"PRG ROM: {prg_size} bytes, CHR ROM: {chr_size} bytes")
    return chr_rom


def decode_tile(chr_rom: bytes, tile_index: int) -> list[list[int]] | None:
    """Decode one CHR tile (8x8) to values 0..3."""
    off = tile_index * 16
    if off + 16 > len(chr_rom):
        return None

    pixels = [[0] * 8 for _ in range(8)]
    for y in range(8):
        plane0 = chr_rom[off + y]
        plane1 = chr_rom[off + y + 8]
        for x in range(8):
            bit = 7 - x
            bit0 = (plane0 >> bit) & 1
            bit1 = (plane1 >> bit) & 1
            pixels[y][x] = bit0 | (bit1 << 1)
    return pixels


def parse_hex_rgba(s: str) -> tuple[int, int, int, int]:
    s = s.strip().lstrip("#")
    if len(s) == 8:
        a = int(s[0:2], 16)
        r = int(s[2:4], 16)
        g = int(s[4:6], 16)
        b = int(s[6:8], 16)
        return (r, g, b, a)
    if len(s) == 6:
        r = int(s[0:2], 16)
        g = int(s[2:4], 16)
        b = int(s[4:6], 16)
        return (r, g, b, 255)
    raise ValueError(f"Unexpected color string: {s!r}")


def load_palette(palette_name: str, palette_id: str) -> list[tuple[int, int, int, int]]:
    pal_path = (
        remastered_root()
        / "Resources/AssetRipper/Palettes/Default"
        / f"{palette_name}.json"
    )
    # Palette files in the upstream project are parsed by Godot and may include
    # trailing commas; accept a small superset of strict JSON here.
    pal_text = pal_path.read_text()
    pal_text = re.sub(r",\s*([}\]])", r"\1", pal_text)
    pal_data = json.loads(pal_text)
    palettes = pal_data.get("palettes", {})
    colors = palettes.get(palette_id)
    if colors is None:
        # Upstream uses `pal_dict.get(palette_id, PREVIEW_PALETTE)`. Prefer
        # `default` if present to keep output usable.
        colors = palettes.get("default")
    if colors is None and palettes:
        colors = next(iter(palettes.values()))
    if colors is None:
        # Transparent, dim gray, white, dark gray (similar to upstream preview)
        return [(0, 0, 0, 0), (105, 105, 105, 255), (255, 255, 255, 255), (64, 64, 64, 255)]
    return [parse_hex_rgba(c) for c in colors]


def parse_vector2i(s: str) -> tuple[int, int]:
    m = VECTOR2I_RE.search(s)
    if not m:
        raise ValueError(f"Bad Vector2i: {s!r}")
    return (int(m.group(1)), int(m.group(2)))


def parse_tiles_map(tiles_str: str) -> list[dict]:
    """
    Parses Remastered tile-map strings like:

      { Vector2(0, 0): { "flip_h": false, "flip_v": false, "index": 113 }, ... }

    into a list of dicts:
      {x, y, index, flip_h, flip_v, palette?}
    """
    items: list[dict] = []
    pattern = re.compile(
        r"Vector2\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\)\s*:\s*\{(.*?)\}\s*(?:,|\Z)",
        re.S,
    )
    for m in pattern.finditer(tiles_str.strip().strip("{}")):
        x = int(round(float(m.group(1))))
        y = int(round(float(m.group(2))))
        body = m.group(3)

        idx_m = re.search(r"\"index\"\s*:\s*(\d+)", body)
        if not idx_m:
            continue
        index = int(idx_m.group(1))

        flip_h = bool(re.search(r"\"flip_h\"\s*:\s*true", body))
        flip_v = bool(re.search(r"\"flip_v\"\s*:\s*true", body))

        pal_m = re.search(r"\"palette\"\s*:\s*\"([^\"]+)\"", body)
        palette = pal_m.group(1) if pal_m else None

        items.append(
            {
                "x": x,
                "y": y,
                "index": index,
                "flip_h": flip_h,
                "flip_v": flip_v,
                "palette": palette,
            }
        )
    return items


def draw_tile_chroma_key(
    img: Image.Image,
    chr_rom: bytes,
    tile_index: int,
    dst_x: int,
    dst_y: int,
    palette: list[tuple[int, int, int, int]],
    flip_h: bool,
    flip_v: bool,
) -> None:
    tile = decode_tile(chr_rom, tile_index)
    if tile is None:
        return

    for ty in range(8):
        sy = 7 - ty if flip_v else ty
        for tx in range(8):
            sx = 7 - tx if flip_h else tx
            ci = tile[sy][sx]
            px = dst_x + tx
            py = dst_y + ty
            if 0 <= px < img.width and 0 <= py < img.height:
                if img.getpixel((px, py)) == OPAQUE_GREEN:
                    img.putpixel((px, py), palette[ci])


def render_from_remastered_spec(chr_rom: bytes, template_path: Path, spec_path: Path) -> Image.Image:
    """
    Implements the Remastered ResourceGenerator.paste_sprite logic for one sprite:
    - loads a template PNG
    - uses the sprite JSON to paste CHR tiles into opaque-green pixels only
    - iterates palette IDs into a grid using (columns, sheet_size)
    """
    spec = json.loads(spec_path.read_text())

    columns = int(spec.get("columns", "4"))
    sheet_w, sheet_h = parse_vector2i(spec.get("sheet_size", "Vector2i(16, 16)"))
    palette_base = spec.get("palette_base", "Tile")

    palette_var = json.loads(spec.get("palettes", "{}"))
    if isinstance(palette_var, list):
        palette_lists = {palette_base: palette_var}
    else:
        palette_lists = palette_var

    tile_items = parse_tiles_map(spec.get("tiles", "{}"))

    img = Image.open(template_path).convert("RGBA")

    for palette_name, palette_ids in palette_lists.items():
        cur_column = 0
        ox = 0
        oy = 0

        for palette_id in palette_ids:
            palette = load_palette(palette_name, palette_id)

            for item in tile_items:
                tile_palette = item.get("palette") or palette_base
                if tile_palette != palette_name:
                    continue
                draw_tile_chroma_key(
                    img,
                    chr_rom,
                    item["index"],
                    item["x"] + ox,
                    item["y"] + oy,
                    palette,
                    item["flip_h"],
                    item["flip_v"],
                )

            cur_column += 1
            if cur_column >= columns:
                cur_column = 0
                ox = 0
                oy += sheet_h
            else:
                ox += sheet_w

    return img


def dump_debug_tiles(chr_rom: bytes, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    def dump_page(first_tile: int, out_name: str) -> None:
        img = Image.new("RGBA", (256, 256), (40, 40, 40, 255))
        for tile_idx in range(first_tile, first_tile + 256):
            tile = decode_tile(chr_rom, tile_idx)
            if not tile:
                continue
            tx = ((tile_idx - first_tile) % 16) * 16
            ty = ((tile_idx - first_tile) // 16) * 16
            for y in range(8):
                for x in range(8):
                    ci = tile[y][x]
                    c = [
                        (0, 0, 0, 0),
                        (128, 128, 128, 255),
                        (196, 196, 196, 255),
                        (255, 255, 255, 255),
                    ][ci]
                    img.putpixel((tx + x, ty + y), c)
        img.save(out_dir / out_name)

    dump_page(0, "debug_chr_page0.png")
    dump_page(256, "debug_chr_page1.png")


def extract_smb_assets(rom_path: Path, output_dir: Path) -> None:
    chr_rom = read_chr_rom(rom_path)

    (output_dir / "sprites/mario").mkdir(parents=True, exist_ok=True)
    (output_dir / "sprites/enemies").mkdir(parents=True, exist_ok=True)
    (output_dir / "sprites/items").mkdir(parents=True, exist_ok=True)
    (output_dir / "sprites/blocks").mkdir(parents=True, exist_ok=True)
    (output_dir / "sprites/players").mkdir(parents=True, exist_ok=True)
    (output_dir / "sprites/ui").mkdir(parents=True, exist_ok=True)

    rem = remastered_root()

    # Mario Small (render full 256x96, then crop the 6-frame strip used by the Wii U port)
    mario_small_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Players/Mario/Small.png",
        rem / "Resources/AssetRipper/Sprites/Players/Mario/Small.json",
    )
    mario_small = mario_small_full.crop((0, 16, 96, 32))
    mario_small.save(output_dir / "sprites/mario/Small.png")
    print("Created: sprites/mario/Small.png")

    # Mario Big (crop 96x32 from top)
    mario_big_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Players/Mario/Big.png",
        rem / "Resources/AssetRipper/Sprites/Players/Mario/Big.json",
    )
    mario_big = mario_big_full.crop((0, 0, 96, 32))
    mario_big.save(output_dir / "sprites/mario/Big.png")
    print("Created: sprites/mario/Big.png")

    # Goomba (top-left block is the default palette)
    goomba_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Enemies/Goomba.png",
        rem / "Resources/AssetRipper/Sprites/Enemies/Goomba.json",
    )
    goomba = goomba_full.crop((0, 0, 48, 16))
    goomba.save(output_dir / "sprites/enemies/Goomba.png")
    print("Created: sprites/enemies/Goomba.png")

    # KoopaTroopa (default block is 96x32 at 0,0; content starts at y=8 and is 24px tall)
    koopa_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Enemies/KoopaTroopa.png",
        rem / "Resources/AssetRipper/Sprites/Enemies/KoopaTroopa.json",
    )
    koopa_2frames = koopa_full.crop((0, 0, 96, 32)).crop((0, 8, 32, 32))
    koopa_2frames.save(output_dir / "sprites/enemies/KoopaTroopa.png")
    print("Created: sprites/enemies/KoopaTroopa.png")

    # Super Mushroom (crop to 16x16)
    mushroom_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Items/SuperMushroom.png",
        rem / "Resources/AssetRipper/Sprites/Items/SuperMushroom.json",
    )
    mushroom = mushroom_full.crop((0, 0, 16, 16))
    mushroom.save(output_dir / "sprites/items/SuperMushroom.png")
    print("Created: sprites/items/SuperMushroom.png")

    # Fireball (render from ROM; the template asset is a green placeholder)
    fireball_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Items/Fireball.png",
        rem / "Resources/AssetRipper/Sprites/Items/Fireball.json",
    )
    fireball = fireball_full.crop((0, 0, 16, 16))
    fireball.save(output_dir / "sprites/items/Fireball.png")
    print("Created: sprites/items/Fireball.png")

    # Question Block
    # Keep the full sheet so theme rows (Underground/Castle/Snow/etc) remain
    # available for rendering.
    question_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/Blocks/QuestionBlock.png",
        rem / "Resources/AssetRipper/Sprites/Blocks/QuestionBlock.json",
    )
    question_full.save(output_dir / "sprites/blocks/QuestionBlock.png")
    print("Created: sprites/blocks/QuestionBlock.png")

    # HUD coin icon (render from ROM; template contains placeholders)
    coin_icon_full = render_from_remastered_spec(
        chr_rom,
        rem / "Assets/Sprites/UI/CoinIcon.png",
        rem / "Resources/AssetRipper/Sprites/UI/CoinIcon.json",
    )
    coin_icon_full.save(output_dir / "sprites/ui/CoinIcon.png")
    print("Created: sprites/ui/CoinIcon.png")

    build_character_sheets(output_dir)
    copy_ui_assets(output_dir)

    # Generate C++ level data from the Godot project (SMB1 scenes).
    tools_script = Path(__file__).resolve().parent / "tools/godot_levels_to_cpp.py"
    if tools_script.exists():
        subprocess.run([sys.executable, str(tools_script)], check=True)

    dump_debug_tiles(chr_rom, output_dir)
    print("\nAsset extraction complete!")


def extract_tilesets(rom_path: Path, output_dir: Path) -> None:
    """
    Render Remastered tileset templates (often green placeholders) into real PNGs
    using the ROM’s CHR tiles and the AssetRipper JSON specs.

    Output goes to: <output_dir>/tilesets/*.png
    """
    chr_rom = read_chr_rom(rom_path)
    tilesets_out = output_dir / "tilesets"
    tilesets_out.mkdir(parents=True, exist_ok=True)

    rem = remastered_root()
    src_root = rem / "Assets/Sprites/Tilesets"
    spec_root = rem / "Resources/AssetRipper/Sprites/Tilesets"

    rendered = 0
    copied = 0
    for template_path in sorted(src_root.rglob("*.png")):
        rel = template_path.relative_to(src_root)
        spec_path = spec_root / rel.with_suffix(".json")
        out_path = tilesets_out / rel
        out_path.parent.mkdir(parents=True, exist_ok=True)

        if spec_path.exists():
            img = render_from_remastered_spec(chr_rom, template_path, spec_path)
            rendered += 1
        else:
            img = Image.open(template_path).convert("RGBA")
            copied += 1

        img.save(out_path)

    print(f"Tilesets written to: {tilesets_out}")
    print(f"Tilesets rendered: {rendered}, copied: {copied}")


def load_anim_rects(json_path: Path, anim_name: str) -> list[list[int]]:
    data = json.loads(json_path.read_text())
    frames = data["animations"][anim_name]["frames"]
    out = []
    for f in frames:
        out.append([int(f[0]), int(f[1]), int(f[2]), int(f[3])])
    return out


def build_character_sheets(output_dir: Path) -> None:
    """
    Build NES-sized player sheets (96x16 / 96x32) for all base characters by
    downscaling the Remastered 32x32 frames to match this Wii U port.
    """
    rem = remastered_root()
    chars = ["Mario", "Luigi", "Toad", "Toadette"]

    def small_anim_order(json_path: Path) -> list[list[int]]:
        # Small sheets: Idle, Move(3), Skid, Jump (6 frames)
        order: list[list[int]] = []
        order += load_anim_rects(json_path, "Idle")[:1]
        order += load_anim_rects(json_path, "Move")[:3]
        order += load_anim_rects(json_path, "Skid")[:1]
        order += load_anim_rects(json_path, "Jump")[:1]
        return order

    def big_anim_order(json_path: Path) -> list[list[int]]:
        # Big sheets: Idle, Crouch, Move(3), Skid, Jump (7 frames)
        order: list[list[int]] = []
        order += load_anim_rects(json_path, "Idle")[:1]
        order += load_anim_rects(json_path, "Crouch")[:1]
        order += load_anim_rects(json_path, "Move")[:3]
        order += load_anim_rects(json_path, "Skid")[:1]
        order += load_anim_rects(json_path, "Jump")[:1]
        return order

    def fire_anim_order(json_path: Path) -> list[list[int]]:
        # Fire sheets: Idle, Crouch, Move(3), Skid, Jump, Attack, AirAttack (9 frames)
        order: list[list[int]] = []
        order += load_anim_rects(json_path, "Idle")[:1]
        order += load_anim_rects(json_path, "Crouch")[:1]
        order += load_anim_rects(json_path, "Move")[:3]
        order += load_anim_rects(json_path, "Skid")[:1]
        order += load_anim_rects(json_path, "Jump")[:1]
        order += load_anim_rects(json_path, "Attack")[:1]
        order += load_anim_rects(json_path, "AirAttack")[:1]
        return order

    def union_bbox(img: Image.Image, frames: list[list[int]]) -> tuple[int, int]:
        max_w = 0
        max_h = 0
        for x, y, w, h in frames:
            frame = img.crop((x, y, x + w, y + h))
            bbox = frame.getbbox()
            if not bbox:
                continue
            bw = bbox[2] - bbox[0]
            bh = bbox[3] - bbox[1]
            if bw > max_w:
                max_w = bw
            if bh > max_h:
                max_h = bh
        if max_w == 0 or max_h == 0:
            return (1, 1)
        return (max_w, max_h)

    def bake_frame(
        frame: Image.Image, cw: int, ch: int, scale_x: float, scale_y: float
    ) -> Image.Image:
        """
        Convert a Remastered frame into a fixed-size NES-ish canvas.

        Approach:
        - Trim fully transparent padding (bbox).
        - Scale by the sheet's baseline (scale_x/scale_y) so big/small stay
          consistent, while shorter frames (crouch) remain shorter.
        - Bottom-align (feet), center horizontally.
        """
        bbox = frame.getbbox()
        if not bbox:
            return Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        frame = frame.crop(bbox)

        nw = max(1, int(round(frame.width * scale_x)))
        nh = max(1, int(round(frame.height * scale_y)))
        resized = frame.resize((nw, nh), Image.NEAREST)

        # If rounding made it spill by 1px, crop conservatively.
        if resized.width > cw:
            left = (resized.width - cw) // 2
            resized = resized.crop((left, 0, left + cw, resized.height))
        if resized.height > ch:
            resized = resized.crop((0, resized.height - ch, resized.width, resized.height))

        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        ox = (cw - resized.width) // 2
        oy = ch - resized.height
        canvas.paste(resized, (ox, oy))
        return canvas

    for name in chars:
        src_small = rem / f"Assets/Sprites/Players/{name}/Small.png"
        src_big = rem / f"Assets/Sprites/Players/{name}/Big.png"
        src_fire = rem / f"Assets/Sprites/Players/{name}/Fire.png"
        src_small_json = rem / f"Assets/Sprites/Players/{name}/Small.json"
        src_big_json = rem / f"Assets/Sprites/Players/{name}/Big.json"
        src_fire_json = rem / f"Assets/Sprites/Players/{name}/Fire.json"
        if not src_small.exists() or not src_big.exists():
            continue
        if not src_small_json.exists() or not src_big_json.exists():
            continue

        img_small = Image.open(src_small).convert("RGBA")
        img_big = Image.open(src_big).convert("RGBA")

        out_dir = output_dir / "sprites/players" / name
        out_dir.mkdir(parents=True, exist_ok=True)

        small_order = small_anim_order(src_small_json)
        big_order = big_anim_order(src_big_json)

        small_max_w, small_max_h = union_bbox(img_small, small_order)
        small_sx = 16 / small_max_w
        small_sy = 16 / small_max_h
        sheet_small = Image.new("RGBA", (16 * len(small_order), 16), (0, 0, 0, 0))
        for i, (x, y, w, h) in enumerate(small_order):
            frame = img_small.crop((x, y, x + w, y + h))
            frame = bake_frame(frame, 16, 16, small_sx, small_sy)
            sheet_small.paste(frame, (i * 16, 0))
        sheet_small.save(out_dir / "Small.png")

        # Big sheets: downscale Remastered Big frames into a fixed 16x32 strip.
        # Use a uniform scale based on the tallest frame so proportions stay
        # consistent (avoid a "thin stretched small" look).
        _, big_max_h = union_bbox(img_big, big_order)
        big_scale = 32 / big_max_h
        # Some render backends/drivers are picky about texture widths that aren't
        # aligned; pad Big to 8 frames (128px wide) while still using only the
        # first 7 frames worth of data.
        sheet_big = Image.new("RGBA", (16 * 8, 32), (0, 0, 0, 0))
        for i, (x, y, w, h) in enumerate(big_order):
            frame = img_big.crop((x, y, x + w, y + h))
            frame = bake_frame(frame, 16, 32, big_scale, big_scale)
            sheet_big.paste(frame, (i * 16, 0))
        sheet_big.save(out_dir / "Big.png")

        # Fire sheet (optional, but used for Fire power visuals in this port)
        if src_fire.exists() and src_fire_json.exists():
            img_fire = Image.open(src_fire).convert("RGBA")
            fire_order = fire_anim_order(src_fire_json)
            fire_max_w, fire_max_h = union_bbox(img_fire, fire_order)
            fire_sx = 16 / fire_max_w
            fire_sy = 32 / fire_max_h
            sheet_fire = Image.new("RGBA", (16 * len(fire_order), 32), (0, 0, 0, 0))
            for i, (x, y, w, h) in enumerate(fire_order):
                frame = img_fire.crop((x, y, x + w, y + h))
                frame = bake_frame(frame, 16, 32, fire_sx, fire_sy)
                sheet_fire.paste(frame, (i * 16, 0))
            sheet_fire.save(out_dir / "Fire.png")

        # Optional icon assets (if present)
        for icon_name in ["LifeIcon.png", "CharacterColour.png", "ColourPalette.png"]:
            icon_path = rem / f"Assets/Sprites/Players/{name}/{icon_name}"
            if icon_path.exists():
                Image.open(icon_path).convert("RGBA").save(out_dir / icon_name)

    print("Created: sprites/players/<Character>/Small.png + Big.png")


def copy_ui_assets(output_dir: Path) -> None:
    rem = remastered_root()
    ui_src = rem / "Assets/Sprites/UI"
    ui_dst = output_dir / "sprites/ui"
    ui_dst.mkdir(parents=True, exist_ok=True)
    for name in ["TitleSMB1.png", "Cursor.png", "MenuBG.png"]:
        p = ui_src / name
        if p.exists():
            Image.open(p).convert("RGBA").save(ui_dst / name)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("rom", help="Path to SMB1 NES ROM (.nes)")
    parser.add_argument(
        "output_dir",
        nargs="?",
        default="content",
        help="Output folder (default: content)",
    )
    parser.add_argument(
        "--tilesets-only",
        action="store_true",
        help="Only render Tilesets/*.png into <output_dir>/tilesets",
    )
    args = parser.parse_args(argv[1:])

    rom_path = Path(args.rom)
    output_dir = Path(args.output_dir)

    if not rom_path.exists():
        print(f"Error: ROM file not found: {rom_path}")
        return 1

    if not remastered_root().exists():
        print("Error: missing Super-Mario-Bros.-Remastered-Public folder next to this project.")
        return 1

    if args.tilesets_only:
        extract_tilesets(rom_path, output_dir)
    else:
        extract_smb_assets(rom_path, output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
