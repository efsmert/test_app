# smb_wiiu (Wii U homebrew)

## Extract sprites from an SMB1 NES ROM

This port expects generated sprite sheets under `smb_wiiu/content/sprites/...`.

From the repo root:

```sh
cd smb_wiiu
python3 extract_rom.py "../Super Mario Bros. (Japan, USA).nes" content
```

Render tilesets (including `Tilesets/Terrain/*.png`) into `smb_wiiu/content/tilesets/`:

```sh
cd smb_wiiu
python3 extract_rom.py "../Super Mario Bros. (Japan, USA).nes" content --tilesets-only
```

Outputs (overwritten):
- `smb_wiiu/content/sprites/mario/Small.png` (96x16)
- `smb_wiiu/content/sprites/mario/Big.png` (96x32)
- `smb_wiiu/content/sprites/players/<Character>/Small.png` (96x16)
- `smb_wiiu/content/sprites/players/<Character>/Big.png` (96x32)
- `smb_wiiu/content/sprites/enemies/Goomba.png` (48x16)
- `smb_wiiu/content/sprites/enemies/KoopaTroopa.png` (32x24)
- `smb_wiiu/content/sprites/blocks/QuestionBlock.png` (48x16)
- `smb_wiiu/content/sprites/items/SuperMushroom.png` (16x16)
- `smb_wiiu/content/debug_chr_page0.png`, `smb_wiiu/content/debug_chr_page1.png`

## Build (devkitPro)

Environment variables depend on your install. Example:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"
make
```

Sync to the local Cemu layout:

```sh
make cemu-sync
```
