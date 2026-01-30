# SMB Wii U Homebrew Port - Agent Summary

## Project Overview
Native C++ port of Super Mario Bros. Remastered (Godot) for Wii U Aroma environment. Tested in Cemu.

## Key Paths
- **Working dir:** `/Users/samiareski/test_app`
- **Wii U project:** `smb_wiiu/`
- **Godot source:** `Super-Mario-Bros.-Remastered-Public/`
- **Runtime assets:** `smb_wiiu/content/`
- **Cemu build:** `smb_wiiu/smb_wiiu_cemu/`
- **NES ROM:** `Super Mario Bros. (Japan, USA).nes`

## Build Command
```bash
python3 smb_wiiu/tools/godot_levels_to_cpp.py
python3 smb_wiiu/extract_rom.py "Super Mario Bros. (Japan, USA).nes" smb_wiiu/content >/dev/null
python3 smb_wiiu/extract_rom.py "Super Mario Bros. (Japan, USA).nes" smb_wiiu/content --tilesets-only >/dev/null
rm -rf smb_wiiu/build
export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH="$PATH:$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$DEVKITPRO/portlibs/wiiu/bin"
make -C smb_wiiu && make -C smb_wiiu cemu-sync
```

## Key Files
| File | Purpose |
|------|---------|
| `smb_wiiu/src/main.cpp` | Main game (~3500 lines) |
| `smb_wiiu/src/levels.h/cpp` | Level structs & loading |
| `smb_wiiu/src/game_types.h` | Enums (entities, tiles) |
| `smb_wiiu/tools/godot_levels_to_cpp.py` | Godot → C++ level converter |
| `smb_wiiu/extract_rom.py` | NES ROM + Godot → content assets |
| `smb_wiiu/src/levels_generated.cpp` | Auto-generated level data |

## Implemented Features
- **Physics:** Jump/fall gravity, variable jump height, walk/run speeds from original Godot scripts
- **Levels:** Full World 1-1+ via Godot scene extraction, themes, pipes (entry/exit)
- **Enemies:** Goomba, Koopa with patrol AI, stomp detection
- **Blocks:** Question blocks with per-tile metadata (coin/powerup/1-up/star), breakable bricks
- **Power-ups:** Small→Big→Fire states, Fire Flower, fireballs with cooldown
- **Rendering:** Atlas-based tiles, chroma-key transparency, dual-screen (TV+GamePad)
- **UI:** HUD (score/coins/lives/timer), title screen, pause menu, character select
- **Audio:** SFX and BGM via SDL2_mixer

## Technical Details
- **Content paths:** Tries `content/`, `../content/`, `fs:/vol/content/`
- **Chroma key:** Green `#00FF00` transparency for sprites
- **Player hitbox:** 14-15px wide, 15px (small) or 31px (big) tall
- **Collision types:** `COL_NONE=0`, `COL_SOLID=1`, `COL_ONEWAY=2`
- **Entity types:** `E_GOOMBA`, `E_KOOPA`, `E_MUSHROOM`, `E_FIRE_FLOWER`, `E_FIREBALL`, etc.

## Known Issues / TODO
- Verify underground/castle question blocks visible after extraction fix
- Fire suit visuals may need polish
- Git repo setup blocked by permission issue (`.git/HEAD.lock`)
- WUHB content mounting works on real hardware; Cemu needs external `content/` folder

## Animation Frames
- **Small:** 6 frames (Idle, Move×3, Skid, Jump)
- **Big:** 7 frames (+Crouch)
- **Fire:** 9 frames (+Attack, AirAttack)

## Level Generator Output
`godot_levels_to_cpp.py` produces `levels_generated.cpp` with:
- Tile arrays, collision arrays, atlas arrays
- `qmeta_*` arrays for question block contents
- Pipe definitions, enemy spawns, flag/castle positions
- Theme metadata (music, tileset selection)

### More Recent

- We have been compiling directtly for wii u and ignore CEMU as CEMU is not accurately representing the game and how it plays on real hardware. Please create a .wuhb file for the game when you compile as we want to test on real hardware (as well as creating the cemu file with cemu sync)

### 2026-01-29 (Latest)

- Restored decorations: `godot_levels_to_cpp.py` now parses both `Tiles` and `DecoTiles` TileMaps and exports deco atlas tiles (`ATLAS_DECO`) again, which re-enables both background deco tiles and the foreground deco generator.
- Fixed Mario sprites: `extract_rom.py` now builds Mario sheets from the correct 32px-wide AssetRipper cells (center 16px), pads Big to 8 frames (128px) for runtime expectations, and updates the Fire mapping.
- Flag/castle visuals: flagpole base now aligns to the top of stacked ground, flag attaches to the pole shaft, pole renders in front of the flag, and the semi-transparent cloud overlay renders in front of gameplay (except HUD) with faster drift.
- WUHB file seems to run properly on real hardware and CEMU so we can use just the single WUHB file to test on real hardware and CEMU

#### 2026-01-29 — Ambient Theme Particles (Snow / Leaves / Embers)

Goal: match the Godot project’s `LevelBG` ambient particles for “vibe” (falling snow, falling leaves in jungle/autumn, rising embers in castle/volcano) in the Wii U SDL renderer.

**Godot reference (source of truth)**
- `Super-Mario-Bros.-Remastered-Public/Scripts/Classes/LevelBGNew.gd`:
  - `@export_enum("None", "Snow", "Leaves", "Ember", "Auto") var particles := 0`
  - Auto mapping: `["", "Snow", "Jungle", "Castle"].find(Global.level_theme)`
- `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/LevelBG.tscn`:
  - `OverlayLayer/Particles/Snow` is `CPUParticles2D` using `Assets/Sprites/Particles/Snow.png` (8×8), direction down, speeds ~20–50.
  - `OverlayLayer/Particles/Leaves` is `CPUParticles2D` using `Assets/Sprites/Particles/Leaves.png` with `CanvasItemMaterial.particles_anim_h_frames = 2` (2× 8×8 frames in a 16×8 sheet). Variations in `Leaves.json` pick `AutumnLeaves.png` for Autumn theme.
  - `OverlayLayer/Particles/LavaEmber` uses a 3-color `GradientTexture1D` with `particles_anim_h_frames = 3` (effectively 3 ember colors).

**Port implementation (Wii U / SDL)**
- Level data now carries the Godot `particles` field per section:
  - `smb_wiiu/src/levels.h`: added `bgParticles` to `LevelSectionData` + `LevelSectionRuntime`.
  - `smb_wiiu/tools/godot_levels_to_cpp.py`: parses `particles = <int>` from the `LevelBG.tscn` node and emits it into `levels_generated.cpp`.
  - `smb_wiiu/src/levels.cpp`: copies `section.bgParticles` into runtime `g_levelInfo.bgParticles`.
- Runtime particle system:
  - `smb_wiiu/src/main.cpp`: implements a lightweight screen-space particle overlay:
    - `BG_PART_SNOW` (128 particles): 8×8 snowflakes falling down with slight sway.
    - `BG_PART_LEAVES` (64 particles): 2-frame 8×8 leaf sprites (scaled to 12×12) falling/rotating; uses `AutumnLeaves.png` when `THEME_AUTUMN`.
    - `BG_PART_EMBER` (64 particles): tiny 2×2 colored rectangles rising up (3-color palette approximates the Godot gradient).
  - Particles are **screen-space** (tied to the current camera view) and rendered **above gameplay, below the cloud overlay + HUD**.
  - `applySection()` calls `resetAmbientParticles()` so each section repopulates particles.
- Assets in `content/`:
  - `smb_wiiu/extract_rom.py` now copies `Snow.png`, `Leaves.png`, `AutumnLeaves.png` into `smb_wiiu/content/sprites/particles/` as part of the normal extract step, so the standard build command always produces the needed files.

**Notes / workflow tip**
- If you add more ambient particle types (wind, bubbles, etc.), follow the same pattern:
  1) Find the Godot source node + params (usually under `Scenes/Prefabs/LevelBG.tscn`).
  2) Ensure the required textures exist under `content/sprites/particles/` (prefer copying via `extract_rom.py` so the build pipeline stays one-command).
  3) Extend the `bgParticles` enum mapping + `updateAmbientParticles()` / `renderAmbientParticles()` in `smb_wiiu/src/main.cpp`.

**Important gotcha (inheritance merge)**
- `godot_levels_to_cpp.py` uses `parse_scene_with_inheritance()` + `merge_nodes()` to handle SMB1 sub-scenes that instance a base level and override only a few properties.
- When adding new per-level BG fields (like `particles`), you MUST also merge them in `merge_nodes()`. Otherwise child scenes silently drop the value and the runtime sees the default (`0`), which looks like “particles never show up anywhere”.
- Fix implemented: `merge_nodes()` now also merges:
  - `bg_primary_layer`, `bg_second_layer`, `bg_overlay_clouds`, `bg_particles`.

#### 2026-01-30 — Movement Fixes (Runaway Speed + Crouch-Jump)

- Fixed runaway horizontal acceleration: the old movement code could continue increasing `vx` without bound when `abs(vx)` exceeded the active cap (walk vs run) because it added accel every frame and only subtracted a smaller decel. The updated logic decelerates down to the cap when overspeed and clamps correctly while accelerating. (`smb_wiiu/src/main.cpp`)
- Crouch behavior:
  - Crouch is now controlled by holding DOWN and can persist into the air, so a crouch-jump keeps the crouch sprite and the reduced hitbox for the duration of the crouch. This prevents the “jump while crouched turns into big sprite/hitbox” bug. (`smb_wiiu/src/main.cpp`)
  - Duck-slide friction is reduced so crouching while moving slides longer instead of stopping almost immediately. (`smb_wiiu/src/main.cpp`)
  - Small hitbox height is now `14px` and crouch uses the same height. (`smb_wiiu/src/main.cpp`)

#### 2026-01-30 — Particles Parallax + Wii Remote Sideways D-Pad + Cloud Tuning

**Why**
- Users noticed ambient particles felt “screen-locked” while the new fore-foreground cloud overlay has camera parallax; they wanted the particles to shift sideways with camera motion too (but **no** independent sideways auto-scroll).
- Wii Remote D-pad felt wrong because it was mapped as if held vertically; SMB-style play holds the remote **sideways**.
- Castle embers were too chunky; wanted “hairline” sparks.
- Fore-foreground clouds were too opaque and sitting too low (little/no clouds visible near the top of the screen).

**What changed**
- Ambient particles camera parallax (no drift):
  - `smb_wiiu/src/main.cpp` adds `g_particleCamX` + `g_particleModeLast` to track camera changes.
  - `updateAmbientParticles(dt)` subtracts `pan = (g_camX - g_particleCamX) * kParticlePanMul` from particle `x` each frame, so particles shift sideways when the camera pans (similar to the cloud overlay parallax), without any time-based drift.
  - Important: this uses **camera delta** (per-frame change) rather than `g_camX` absolute so particles don’t “jump” when sections reset; `resetAmbientParticles()` initializes `g_particleCamX = g_camX`.
  - Tuned `kParticlePanMul` to match the cloud overlay’s camera multiplier (`1.35f`) so both layers “slide” consistently during camera pans.
- Ember particle shape:
  - `renderAmbientParticles()` draws embers as `1x3` rectangles (was `3x3`) so castle sparks look thinner.
- Wii Remote sideways D-pad mapping:
  - `mapWiimoteButtonsToVpad()` remaps D-pad directions to match sideways grip:
    - `WPAD_UP → VPAD_LEFT`
    - `WPAD_DOWN → VPAD_RIGHT`
    - `WPAD_LEFT → VPAD_DOWN`
    - `WPAD_RIGHT → VPAD_UP`
  - This mapping applies in both gameplay and menu contexts so the controller feels consistent.
- Fore-foreground cloud overlay tuning:
  - Reduced alpha (`SDL_SetTextureAlphaMod`) to make the layer more transparent.
  - Increased `kCloudSrcY` (sampling lower rows of the 512×512 overlay) so the densest cloud band renders higher in the viewport.
  - Reduced independent drift speed slightly; camera parallax multiplier is unchanged.

**Build / packaging gotcha (IMPORTANT)**
- `make -C smb_wiiu` builds the RPX, but it does **not** necessarily rebuild the `.wuhb`.
- Always run `make -C smb_wiiu wuhb` after code/content changes if you want an updated bundle for hardware/Cemu testing.

#### 2026-01-30 — Ambient Ember Particles “Cut Off” Fix (Camera Parallax)

- Symptom: castle/ember particles would appear on the first screen, but once the player moved the camera far enough, embers vanished and did not repopulate.
- Root cause: the newer camera-parallax logic shifts particle `x` each frame based on camera delta, but the ember update path only recycled particles when `y < -16` (no `x` wrapping / respawn), so particles could get pushed off-screen permanently.
- Fix: in `updateAmbientParticles()` for `BG_PART_EMBER`, wrap `x` back into `[-16, GAME_W+16]` (similar to snow/leaves bounds handling) so particles always span the whole viewport while the camera pans. (`smb_wiiu/src/main.cpp`)

#### 2026-01-30 — “Unbeatable Levels” Foundations (Swimming + Generators + Moving Platforms + Rope Platforms + Castle Axe/Bowser)

Goal: make later worlds/stages beatable by bringing over key gameplay objects from the Godot project: water interaction, Cheep-Cheeps, Bullet Bill generators, SMB elevator platforms, rope-linked platforms (World 3-3 style), and castle completion via bridge axe / Bowser.

**Godot references (source of truth)**
- Entity generators:
  - `Super-Mario-Bros.-Remastered-Public/Scripts/Parts/EntityGenerator.gd`
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/LevelObjects/BulletBillGenerator.tscn`
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/LevelObjects/CheepCheepGenerator.tscn`
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/LevelObjects/EntityGeneratorStopper.tscn`
- Cheep-Cheeps:
  - `Super-Mario-Bros.-Remastered-Public/Scripts/Classes/Entities/Enemies/LeapingCheepCheep.gd`
  - `Super-Mario-Bros.-Remastered-Public/Scripts/Classes/Entities/Enemies/SwimmingCheepCheep.gd`
- Moving platforms:
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/Entities/Objects/SidewaysPlatform.tscn` (48px pingpong over 2s)
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/Entities/Objects/VerticalPlatform.tscn` (128px pingpong over 3s)
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/Entities/Objects/ElevatorPlatform.tscn` (wrap between `top` and a bottom constant)
- Rope elevator platforms:
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/Entities/Objects/RopeElevatorPlatform.tscn`
  - `Super-Mario-Bros.-Remastered-Public/Scripts/Classes/Entities/Objects/RopeElevatorPlatform.gd`
- Castle completion:
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/LevelObjects/CastleBridge.tscn` (axe at local (208, -32))
  - `Super-Mario-Bros.-Remastered-Public/Scenes/Prefabs/Entities/Enemies/Bowser.tscn`

**Level export changes**
- `smb_wiiu/tools/godot_levels_to_cpp.py`:
  - Node parsing/merging now includes additional exported props for instanced objects:
    - `vertical_direction`, `top` (elevator platforms)
    - `linked_platform`, `rope_top` (rope elevator platforms)
  - Instanced objects export using world-pixel coordinates via `to_world_px()` (uses the same `x_offset/y_offset` as the tilemap).
  - Rope elevator platforms are exported as `E_PLATFORM_ROPE` with a stable **pair id** assigned per linked pair.

**Runtime / gameplay changes**
- `smb_wiiu/src/main.cpp`:
  - Added a pre-player update pass: `updatePlatformsAndGenerators(dt)` runs in `GS_PLAYING` before `updatePlayers(dt)` so platforms can move and carry riders correctly.
  - Moving platforms implemented:
    - `E_PLATFORM_SIDEWAYS`: linear pingpong between `baseX` and `baseX-48`.
    - `E_PLATFORM_VERTICAL` pingpong (`dir==0`): linear pingpong between `baseY` and `baseY+128`.
    - `E_PLATFORM_VERTICAL` elevator wrap (`dir!=0`): wraps between `a=topY` and `b=bottomY` at 50 px/s.
    - `E_PLATFORM_ROPE`: paired vertical motion (one down, other up) based on whether players are standing on either platform.
  - Generators implemented:
    - `E_ENTITY_GENERATOR` activates when Player 1 passes its X, spawns immediately, then spawns at `b` milliseconds with a random “stagger” (similar to Godot’s `randf_range(-2, 0)` behavior).
    - `E_ENTITY_GENERATOR_STOP` disables all generators for the remainder of the section once Player 1 passes it.
  - Swimming implemented:
    - Water detection uses `ATLAS_LIQUID` tiles where available (`rectTouchesLiquid()`), and treats `THEME_UNDERWATER/THEME_CASTLE_WATER` as fully submerged.
    - Jump in water becomes a “swim stroke” with a cooldown (`swimCooldown`) and floatier gravity.

**Common gotcha**
- If you add more instanced-object properties from Godot (like `linked_platform`, `rope_top`), remember:
  - You must parse them in `parse_nodes()` **and** merge them in `merge_nodes()` or inherited scenes will silently lose the overrides.

**Packaging reminder**
- The updated hardware/Cemu bundle is rebuilt via `make -C smb_wiiu wuhb` and outputs to `smb_wiiu/smb_wiiu.wuhb`.

### 2026-01-30 — Camera Clamp + World 2-3 “Ceiling” Fixes (Level Export) + Swim Anim + Platform Chroma

#### 1) Camera stopping after ~2 screens (e.g. 1-3 / 1-4)
- Symptom: camera would clamp very early (after ~2 screens), making some levels “unprogressable”.
- Root cause: `smb_wiiu/tools/godot_levels_to_cpp.py` accidentally reused the variable name `width` for **both**:
  - `width = max_x - min_x + 1` (tilemap width in tiles, used for `map_width`)
  - and later `width = 32/48` for platform sprite widths (Sideways/Vertical/Rope)
  This overwrote the computed map width right before `sections.append(...)`, causing `map_width` to become `32/48` even though the exported tilemap itself contained far more columns.
- Fix:
  - Renamed the tilemap width variable to `map_width_tiles`.
  - Renamed platform width locals to `plat_w`.
  - Regenerated `smb_wiiu/src/levels_generated.cpp` so section widths like 1-3/1-4 are now correct.

#### 2) World 2-3 “level raised into the ceiling”
- Symptom: stage geometry appeared shifted up (play area shoved into the top rows), caused by a few stray “terrain” tiles far below the real playfield.
- Root cause: the exporter aligned the bottom using the **absolute max** terrain Y (`max_y_terrain`), so any rare outlier terrain tile could drag the alignment window upward.
- Fix: added `compute_bottom_terrain_y()` which:
  - counts terrain tiles per Y row,
  - discards very-low-frequency rows (threshold = max(3 tiles, ~12.5% of the most common terrain row)),
  - and picks the maximum remaining Y as the “real” bottom row.
  This keeps normal stages aligned while avoiding bad outliers in levels like 2-3.

#### 3) Moving platform green fringe (chroma key)
- `smb_wiiu/content/sprites/tilesets/Platform.png` is RGBA but uses pure `#00FF00` pixels for transparency.
- Fix: `smb_wiiu/src/main.cpp` now includes `Platform.png` in the chroma-key whitelist in both `loadTex()` and `loadTexScaled()`.

#### 4) Swim animation fallback (assets don’t include dedicated swim frames yet)
- Current small sheets are only `6×16px` (idle/move/skid/jump) and big/fire are fixed frame strips; there are no dedicated swim frames in these extracted sheets.
- Implemented a pragmatic swim animation selection in `smb_wiiu/src/main.cpp`:
  - added `Player::swimAnimT`
  - on swim stroke, the player briefly cycles a “stroke” sequence using existing frames
  - otherwise uses a gentle idle (alternates idle/jump pose) while in water

#### 5) Softer parallax for clouds + particles
- Reduced camera-parallax multipliers so both layers feel less “slippery” while still moving slightly faster than the world:
  - cloud overlay `kCloudPanMul` reduced
  - ambient particles `kParticlePanMul` reduced
