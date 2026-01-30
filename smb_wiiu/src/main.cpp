// Super Mario Bros. Wii U Port - Complete Implementation
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "game_types.h"
#include "levels.h"
#include <cmath>
#include <vector>
#include <utility>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <whb/proc.h>

namespace Physics {
constexpr float JUMP_GRAVITY = 11.0f, FALL_GRAVITY = 25.0f,
                JUMP_HEIGHT = 300.0f;
constexpr float MAX_FALL_SPEED = 280.0f, WALK_SPEED = 96.0f, RUN_SPEED = 160.0f;
constexpr float GROUND_ACCEL = 4.0f, AIR_ACCEL = 3.0f, DECEL = 3.0f;
constexpr float BOUNCE_HEIGHT = 200.0f, ENEMY_SPEED = 32.0f;
} // namespace Physics

constexpr int GAME_H = 240, TV_W = 1280, TV_H = 720;
constexpr int GAME_W = (GAME_H * TV_W + TV_H / 2) / TV_H;
constexpr int PLAYER_DRAW_W_SMALL = 16;
constexpr int PLAYER_DRAW_W_BIG = 16;

// Gameplay collision boxes are intentionally slightly smaller than the visuals
// to make tight gaps and pipe entry feel less "pixel perfect".
constexpr float PLAYER_HIT_W_SMALL = 14.0f;
constexpr float PLAYER_HIT_H_SMALL = 14.0f; // 1-tile gaps (16px) are enterable
constexpr float PLAYER_HIT_W_BIG = 15.0f;
constexpr float PLAYER_HIT_H_BIG = 31.0f;   // 2-tile gaps (32px) are enterable
enum GameState { GS_TITLE, GS_PLAYING, GS_FLAG, GS_DEAD, GS_GAMEOVER, GS_WIN, GS_PAUSE };
constexpr uint8_t QMETA_COIN = 0;
constexpr uint8_t QMETA_POWERUP = 1;
constexpr uint8_t QMETA_ONEUP = 2;
constexpr uint8_t QMETA_STAR = 3;
enum TitleMode {
  TITLE_MAIN = 0,
  TITLE_CHAR_SELECT = 1,
  TITLE_OPTIONS = 2,
  TITLE_EXTRAS = 3,
  TITLE_MULTI_SELECT = 4,
  TITLE_CHEATS = 5,
};
constexpr uint8_t COL_NONE = 0;
constexpr uint8_t COL_SOLID = 1;
constexpr uint8_t COL_ONEWAY = 2;
constexpr uint8_t ATLAS_NONE = 0;
constexpr uint8_t ATLAS_TERRAIN = 1;
constexpr uint8_t ATLAS_DECO = 2;
constexpr uint8_t ATLAS_LIQUID = 3;
constexpr int SFX_CH_BREAK = 0; // legacy (was reserved for brick breaks)

struct Rect {
  float x, y, w, h;
};
struct Entity {
  bool on;
  EType type;
  Rect r;
  float vx, vy;
  int dir, state;
  float timer;
  int a, b;
  float baseX, baseY;
  float prevX, prevY;
};
struct Player {
  Rect r;
  float vx, vy;
  bool ground, right, jumping;
  bool crouch;
  Power power;
  int lives, coins, score;
  float invT, animT;
  float throwT;
  float fireCooldown;
  float swimCooldown;
  float swimAnimT;
  bool dead;
};

static uint8_t g_map[MAP_H][MAP_W];
static Entity g_ents[64];
static Player g_players[4];
static Player &g_p = g_players[0];
static float g_camX = 0;
static GameState g_state = GS_PLAYING;
static uint32_t g_held = 0, g_pressed = 0;
static bool g_showDebugOverlay = false;
static uint32_t g_playerHeld[4] = {0, 0, 0, 0};
static uint32_t g_playerPressed[4] = {0, 0, 0, 0};
static int g_loadedTex = 0;
static int g_time = 400;
static float g_timeAcc = 0;
static float g_flagY = 0; // Flag sliding position
static int g_flagX = 0;
static bool g_hasFlag = false;
static int g_sectionIndex = 0;
static LevelSectionRuntime g_levelInfo = {};
static bool g_allowCameraBacktrack = false;
static bool g_multiplayerActive = false;
static bool g_cheatMoonJump = false;
static bool g_cheatGodMode = false;
static int g_playerCount = 1;
static int g_playerCharIndex[4] = {0, 0, 0, 0};
static bool g_playerReady[4] = {false, false, false, false};
static int g_playerRemoteChan[4] = {-1, -1, -1, -1}; // -1=GamePad, else 0..3
static bool g_remoteConnected[4] = {false, false, false, false};
static uint32_t g_remoteHeld[4] = {0, 0, 0, 0};
static uint32_t g_remotePressed[4] = {0, 0, 0, 0};
static uint32_t g_remoteHoldRaw[4] = {0, 0, 0, 0};

static SDL_Window *g_win = nullptr;
static SDL_Renderer *g_ren = nullptr;
static SDL_Texture *g_texPlayerSmall[4] = {nullptr, nullptr, nullptr, nullptr};
static SDL_Texture *g_texPlayerBig[4] = {nullptr, nullptr, nullptr, nullptr};
static SDL_Texture *g_texPlayerFire[4] = {nullptr, nullptr, nullptr, nullptr};
static SDL_Texture *g_texLifeIcon[4] = {nullptr, nullptr, nullptr, nullptr};
static SDL_Texture *g_texTitle = nullptr;
static SDL_Texture *g_texCursor = nullptr;
static SDL_Texture *g_texMenuBG = nullptr;
static SDL_Texture *g_texCoinIcon = nullptr;
static SDL_Texture *g_texGoomba = nullptr;
static SDL_Texture *g_texKoopa = nullptr;
static SDL_Texture *g_texKoopaSheet = nullptr;
static SDL_Texture *g_texCheepCheep = nullptr;
static SDL_Texture *g_texBulletBill = nullptr;
static SDL_Texture *g_texBlooper = nullptr;
static SDL_Texture *g_texBuzzy = nullptr;
static SDL_Texture *g_texLakitu = nullptr;
static SDL_Texture *g_texLakituCloud = nullptr;
static SDL_Texture *g_texSpiny = nullptr;
static SDL_Texture *g_texHammerBro = nullptr;
static SDL_Texture *g_texHammer = nullptr;
static SDL_Texture *g_texBowser = nullptr;
static SDL_Texture *g_texPlatform = nullptr;
static SDL_Texture *g_texBridgeAxe = nullptr;
static SDL_Texture *g_texQuestion = nullptr;
static SDL_Texture *g_texMushroom = nullptr;
static SDL_Texture *g_texFireFlower = nullptr;
	static SDL_Texture *g_texFireball = nullptr;
	static SDL_Texture *g_texCoin = nullptr;
	static SDL_Texture *g_texTerrain = nullptr;
	static SDL_Texture *g_texDeco = nullptr;
static SDL_Texture *g_texLiquids = nullptr;
static SDL_Texture *g_texBgHills = nullptr;
static SDL_Texture *g_texBgBushes = nullptr;
static SDL_Texture *g_texBgCloudOverlay = nullptr;
static SDL_Texture *g_texBgSky = nullptr;
static SDL_Texture *g_texBgSecondary = nullptr; // Trees/Mushrooms layer
static SDL_Texture *g_texParticleSnow = nullptr;
static SDL_Texture *g_texParticleLeaves = nullptr;
static SDL_Texture *g_texParticleAutumnLeaves = nullptr;
static SDL_Texture *g_texFlagPole = nullptr;
static SDL_Texture *g_texFlag = nullptr;
static SDL_Texture *g_texCastle = nullptr;
static int g_flagPoleShaftX = 8; // attachment point within 16px pole column
static SDL_Rect g_castleDrawDst = {0, 0, 0, 0};
static SDL_Rect g_castleOverlayDst = {0, 0, 0, 0};
static bool g_castleDrawOn = false;
static LevelTheme g_theme = THEME_OVERWORLD;
static const char *g_charNames[] = {"Mario", "Luigi", "Toad", "Toadette"};
static const char *g_charDisplayNames[] = {"MARIO", "LUIGI", "TOAD", "TOADETTE"};
static const int g_charCount = 4;
static int g_charIndex = 0; // Player 1 selection (mirrors g_playerCharIndex[0])
static int g_menuIndex = 0;
static int g_playerMenuIndex[4] = {0, 0, 0, 0};
static int g_titleMode = TITLE_MAIN;
static int g_optionsIndex = 0;
static int g_cheatsIndex = 0;
static int g_mainMenuIndex = 0;
static int g_pauseIndex = 0;
static const char *g_pauseOptions[] = {"RESUME", "NEXT LEVEL", "PREVIOUS LEVEL",
                                       "CYCLE THEME", "MAIN MENU"};
static const int g_pauseOptionCount = 5;
static int g_levelIndex = 0;
static float g_levelTimer = 0.0f;
static bool g_flagSfxPlayed = false;
static bool g_castleSfxPlayed = false;
static bool g_randomTheme = false;
static bool g_nightMode = false;
static int g_themeOverride = -1;

static Mix_Chunk *g_sfxJump = nullptr;
static Mix_Chunk *g_sfxBigJump = nullptr;
static Mix_Chunk *g_sfxStomp = nullptr;
static Mix_Chunk *g_sfxCoin = nullptr;
static Mix_Chunk *g_sfxPowerup = nullptr;
static Mix_Chunk *g_sfxBump = nullptr;
static Mix_Chunk *g_sfxBreak = nullptr;
static Mix_Chunk *g_sfxItemAppear = nullptr;
static Mix_Chunk *g_sfxDamage = nullptr;
static Mix_Chunk *g_sfxSkid = nullptr;
static Mix_Chunk *g_sfxMenuMove = nullptr;
static Mix_Chunk *g_sfxFlagSlide = nullptr;
static Mix_Chunk *g_sfxCastleClear = nullptr;
static Mix_Chunk *g_sfxPipe = nullptr;
static Mix_Chunk *g_sfxKick = nullptr;
static Mix_Chunk *g_sfxFireball = nullptr;
static Mix_Music *g_bgm = nullptr;
static Mix_Music *g_bgmOverworld = nullptr;
static Mix_Music *g_bgmUnderground = nullptr;
static Mix_Music *g_bgmCastle = nullptr;
static Mix_Music *g_bgmByTheme[(int)THEME_COUNT] = {};
static float g_skidCooldown = 0.0f;

struct ForegroundDeco {
  int tx;
  int ty;
  uint8_t w;
  uint8_t h;
  uint8_t cellCount;
  struct Cell {
    int8_t dx;
    int8_t dy;
    uint8_t ax;
    uint8_t ay;
  } cells[16];
};
static ForegroundDeco g_fgDecos[192];
static int g_fgDecoCount = 0;

void renderCopyExWithShadowAngle(SDL_Texture *tex, const SDL_Rect *src,
                                 const SDL_Rect *dst, double angle,
                                 SDL_RendererFlip flip,
                                 const SDL_Point *center);

// Ambient background particles (Godot LevelBG: Snow, Leaves, Ember).
// These are screen-space overlay particles (tied to the camera view, not world
// tiles) to match the upstream CPUParticles2D setup.
enum BgParticleMode { BG_PART_NONE = 0, BG_PART_SNOW = 1, BG_PART_LEAVES = 2, BG_PART_EMBER = 3, BG_PART_AUTO = 4 };
struct AmbientParticle {
  float x, y;
  float vx, vy;
  float rotDeg, vRotDeg;
  uint8_t frame;
  uint8_t a;
};
static AmbientParticle g_snowParticles[128];
static AmbientParticle g_leafParticles[64];
static AmbientParticle g_emberParticles[64];
static int g_effectiveParticles = BG_PART_NONE;
static float g_particleCamX = 0.0f;
static int g_particleModeLast = BG_PART_NONE;

struct TileBump {
  int tx;
  int ty;
  float t;
};
static TileBump g_tileBumps[64];

static void addTileBump(int tx, int ty) {
  for (auto &b : g_tileBumps) {
    if (b.t > 0.0f && b.tx == tx && b.ty == ty) {
      b.t = 0.12f;
      return;
    }
  }
  for (auto &b : g_tileBumps) {
    if (b.t <= 0.0f) {
      b.tx = tx;
      b.ty = ty;
      b.t = 0.12f;
      return;
    }
  }
}

static bool bumpTransformForTile(int tx, int ty, float &outLiftPx,
                                 float &outScale) {
  for (auto &b : g_tileBumps) {
    if (b.t > 0.0f && b.tx == tx && b.ty == ty) {
      float p = 1.0f - (b.t / 0.12f); // 0..1
      float tri = (p < 0.5f) ? (p * 2.0f) : (2.0f - p * 2.0f);
      outLiftPx = tri * 2.0f;
      outScale = 1.0f + tri * 0.06f;
      return true;
    }
  }
  return false;
}

static float frand(float a, float b) {
  float t = (float)rand() / (float)RAND_MAX;
  return a + (b - a) * t;
}

static int autoParticleModeForTheme() {
  // Mirrors Godot's LevelBGNew.gd mapping:
  // ["", "Snow", "Jungle", "Castle"] -> particles 0..3.
  if (g_theme == THEME_SNOW)
    return BG_PART_SNOW;
  if (g_theme == THEME_JUNGLE || g_theme == THEME_AUTUMN)
    return BG_PART_LEAVES;
  if (g_theme == THEME_CASTLE || g_theme == THEME_VOLCANO ||
      g_theme == THEME_CASTLE_WATER)
    return BG_PART_EMBER;
  return BG_PART_NONE;
}

static int effectiveBgParticles() {
  int p = g_levelInfo.bgParticles;
  // If a section doesn't explicitly set particles, we still want "vibe" by
  // default. Treat None as Auto so castle/snow/jungle themes show ambient
  // particles without needing per-level configuration.
  if (p == BG_PART_NONE)
    p = BG_PART_AUTO;
  if (p == BG_PART_AUTO)
    return autoParticleModeForTheme();
  return p;
}

static void resetAmbientParticles() {
  g_effectiveParticles = effectiveBgParticles();
  g_particleCamX = g_camX;
  g_particleModeLast = g_effectiveParticles;

  for (auto &p : g_snowParticles) {
    p.x = frand(0.0f, (float)GAME_W);
    p.y = frand(0.0f, (float)GAME_H);
    p.vx = frand(-6.0f, 6.0f);
    p.vy = frand(20.0f, 50.0f);
    p.rotDeg = 0.0f;
    p.vRotDeg = 0.0f;
    p.frame = 0;
    p.a = (uint8_t)frand(180.0f, 255.0f);
  }
  for (auto &p : g_leafParticles) {
    p.x = frand(0.0f, (float)GAME_W);
    p.y = frand(0.0f, (float)GAME_H);
    p.vx = frand(-18.0f, 18.0f);
    p.vy = frand(25.0f, 100.0f);
    p.rotDeg = frand(0.0f, 360.0f);
    p.vRotDeg = frand(-720.0f, 720.0f);
    p.frame = (uint8_t)(rand() & 1);
    p.a = (uint8_t)frand(200.0f, 255.0f);
  }
  for (auto &p : g_emberParticles) {
    p.x = frand(0.0f, (float)GAME_W);
    p.y = frand(0.0f, (float)GAME_H);
    p.vx = frand(-10.0f, 10.0f);
    p.vy = frand(-20.0f, -5.0f);
    p.rotDeg = 0.0f;
    p.vRotDeg = 0.0f;
    p.frame = (uint8_t)(rand() % 3);
    p.a = (uint8_t)frand(160.0f, 240.0f);
  }
}

static void updateAmbientParticles(float dt) {
  g_effectiveParticles = effectiveBgParticles();
  if (g_effectiveParticles == BG_PART_NONE)
    return;

  if (g_effectiveParticles != g_particleModeLast) {
    g_particleModeLast = g_effectiveParticles;
    g_particleCamX = g_camX;
  }
  constexpr float kParticlePanMul = 1.08f;
  float camDx = g_camX - g_particleCamX;
  g_particleCamX = g_camX;
  float pan = camDx * kParticlePanMul;

  if (g_effectiveParticles == BG_PART_SNOW) {
    for (auto &p : g_snowParticles) {
      p.x -= pan;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      // gentle horizontal sway
      p.x += sinf((p.y * 0.03f) + (p.x * 0.01f)) * 6.0f * dt;
      if (p.x < -8)
        p.x += GAME_W + 16;
      if (p.x > GAME_W + 8)
        p.x -= GAME_W + 16;
      if (p.y > GAME_H + 8) {
        p.y = -8;
        p.x = frand(0.0f, (float)GAME_W);
        p.vx = frand(-6.0f, 6.0f);
        p.vy = frand(20.0f, 50.0f);
        p.a = (uint8_t)frand(180.0f, 255.0f);
      }
    }
  } else if (g_effectiveParticles == BG_PART_LEAVES) {
    for (auto &p : g_leafParticles) {
      p.rotDeg += p.vRotDeg * dt;
      p.x -= pan;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      // slow drift / swirl
      p.x += sinf((p.y * 0.02f) + (p.rotDeg * 0.01f)) * 10.0f * dt;

      if (p.y > GAME_H + 16 || p.x < -16 || p.x > GAME_W + 16) {
        p.y = -16;
        p.x = frand(0.0f, (float)GAME_W);
        p.vx = frand(-18.0f, 18.0f);
        p.vy = frand(25.0f, 100.0f);
        p.rotDeg = frand(0.0f, 360.0f);
        p.vRotDeg = frand(-720.0f, 720.0f);
        p.frame = (uint8_t)(rand() & 1);
        p.a = (uint8_t)frand(200.0f, 255.0f);
      }
    }
  } else if (g_effectiveParticles == BG_PART_EMBER) {
    for (auto &p : g_emberParticles) {
      // light upward acceleration (matches Godot gravity = (0, -10))
      p.vy += -10.0f * dt;
      p.x -= pan;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      // Keep embers spanning the whole viewport even as the camera pans.
      // Without this, parallax can push particles off-screen permanently.
      while (p.x < -16.0f)
        p.x += (float)GAME_W + 32.0f;
      while (p.x > (float)GAME_W + 16.0f)
        p.x -= (float)GAME_W + 32.0f;
      if (p.y < -16) {
        p.y = GAME_H + 16;
        p.x = frand(0.0f, (float)GAME_W);
        p.vx = frand(-10.0f, 10.0f);
        p.vy = frand(-20.0f, -5.0f);
        p.frame = (uint8_t)(rand() % 3);
        p.a = (uint8_t)frand(160.0f, 240.0f);
      }
    }
  }
}

static void renderAmbientParticles() {
  if (g_effectiveParticles == BG_PART_NONE)
    return;

  if (g_effectiveParticles == BG_PART_SNOW) {
    if (!g_texParticleSnow)
      return;
    SDL_Rect src = {0, 0, 8, 8};
    for (auto &p : g_snowParticles) {
      SDL_SetTextureAlphaMod(g_texParticleSnow, p.a);
      SDL_Rect dst = {(int)p.x, (int)p.y, 8, 8};
      SDL_RenderCopy(g_ren, g_texParticleSnow, &src, &dst);
    }
    SDL_SetTextureAlphaMod(g_texParticleSnow, 255);
    return;
  }

  if (g_effectiveParticles == BG_PART_LEAVES) {
    SDL_Texture *tex = (g_theme == THEME_AUTUMN) ? g_texParticleAutumnLeaves : g_texParticleLeaves;
    if (!tex)
      return;
    for (auto &p : g_leafParticles) {
      SDL_SetTextureAlphaMod(tex, p.a);
      SDL_Rect src = {(int)p.frame * 8, 0, 8, 8};
      SDL_Rect dst = {(int)p.x, (int)p.y, 12, 12};
      SDL_Point c = {dst.w / 2, dst.h / 2};
      renderCopyExWithShadowAngle(tex, &src, &dst, p.rotDeg, SDL_FLIP_NONE, &c);
    }
    SDL_SetTextureAlphaMod(tex, 255);
    return;
  }

  if (g_effectiveParticles == BG_PART_EMBER) {
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    for (auto &p : g_emberParticles) {
      Uint8 r = 0, g = 0, b = 0;
      switch (p.frame % 3) {
      case 0: // dark ember
        r = 134;
        g = 49;
        b = 14;
        break;
      case 1: // light ember
        r = 255;
        g = 183;
        b = 98;
        break;
      default: // hot ember
        r = 247;
        g = 57;
        b = 16;
        break;
      }
      SDL_SetRenderDrawColor(g_ren, r, g, b, p.a);
      SDL_Rect dst = {(int)p.x, (int)p.y, 1, 3};
      SDL_RenderFillRect(g_ren, &dst);
    }
    return;
  }
}

// Simple 5x7 font (bits are 0..4 in each row)
static const uint8_t kFont5x7[][7] = {
    // space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // A-Z
    {0x1E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0E}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x11, 0x15, 0x1B, 0x11}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
    // 0-9
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}  // 9
};

int fontIndex(char c) {
  if (c == ' ')
    return 0;
  if (c >= 'A' && c <= 'Z')
    return 1 + (c - 'A');
  if (c >= '0' && c <= '9')
    return 27 + (c - '0');
  return 0;
}

void drawText(int x, int y, const char *text, int scale, SDL_Color color) {
  SDL_SetRenderDrawColor(g_ren, color.r, color.g, color.b, color.a);
  int cx = x;
  for (const char *p = text; *p; ++p) {
    int idx = fontIndex(*p);
    for (int row = 0; row < 7; row++) {
      uint8_t bits = kFont5x7[idx][row];
      for (int col = 0; col < 5; col++) {
        if (bits & (1 << (4 - col))) {
          SDL_Rect r = {cx + col * scale, y + row * scale, scale, scale};
          SDL_RenderFillRect(g_ren, &r);
        }
      }
    }
    cx += (6 * scale);
  }
}

void drawTextShadow(int x, int y, const char *text, int scale, SDL_Color color) {
  drawText(x + 1, y + 1, text, scale, {0, 0, 0, 255});
  drawText(x, y, text, scale, color);
}

int textWidth(const char *text, int scale) {
  int len = 0;
  for (const char *p = text; *p; ++p)
    len++;
  return len * 6 * scale;
}

constexpr int SPRITE_SHADOW_OFS = 2;
constexpr uint8_t SPRITE_SHADOW_ALPHA = 90;

struct TextureModGuard {
  SDL_Texture *t = nullptr;
  Uint8 r = 255, g = 255, b = 255, a = 255;
  explicit TextureModGuard(SDL_Texture *tex) : t(tex) {
    if (!t)
      return;
    SDL_GetTextureColorMod(t, &r, &g, &b);
    SDL_GetTextureAlphaMod(t, &a);
  }
  ~TextureModGuard() {
    if (!t)
      return;
    SDL_SetTextureColorMod(t, r, g, b);
    SDL_SetTextureAlphaMod(t, a);
  }
};

void renderCopyExWithShadow(SDL_Texture *tex, const SDL_Rect *src,
                            const SDL_Rect *dst, SDL_RendererFlip flip) {
  if (!tex || !dst)
    return;

  TextureModGuard guard(tex);

  SDL_Rect shadowDst = *dst;
  shadowDst.x += SPRITE_SHADOW_OFS;
  shadowDst.y += SPRITE_SHADOW_OFS;

  SDL_SetTextureColorMod(tex, 0, 0, 0);
  SDL_SetTextureAlphaMod(tex, SPRITE_SHADOW_ALPHA);
  SDL_RenderCopyEx(g_ren, tex, src, &shadowDst, 0, nullptr, flip);

  SDL_SetTextureColorMod(tex, 255, 255, 255);
  SDL_SetTextureAlphaMod(tex, 255);
  SDL_RenderCopyEx(g_ren, tex, src, dst, 0, nullptr, flip);
}

void renderCopyExWithShadowAngle(SDL_Texture *tex, const SDL_Rect *src,
                                 const SDL_Rect *dst, double angle,
                                 SDL_RendererFlip flip,
                                 const SDL_Point *center) {
  if (!tex || !dst)
    return;

  TextureModGuard guard(tex);

  SDL_Rect shadowDst = *dst;
  shadowDst.x += SPRITE_SHADOW_OFS;
  shadowDst.y += SPRITE_SHADOW_OFS;

  SDL_SetTextureColorMod(tex, 0, 0, 0);
  SDL_SetTextureAlphaMod(tex, SPRITE_SHADOW_ALPHA);
  SDL_RenderCopyEx(g_ren, tex, src, &shadowDst, angle, center, flip);

  SDL_SetTextureColorMod(tex, 255, 255, 255);
  SDL_SetTextureAlphaMod(tex, 255);
  SDL_RenderCopyEx(g_ren, tex, src, dst, angle, center, flip);
}

void renderCopyWithShadow(SDL_Texture *tex, const SDL_Rect *src,
                          const SDL_Rect *dst) {
  renderCopyExWithShadow(tex, src, dst, SDL_FLIP_NONE);
}

static void renderTiledBottomSlice(SDL_Texture *tex, float parallaxCamScale,
                                   float camX) {
  if (!tex)
    return;

  int texW = 0, texH = 0;
  SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);
  // Be robust against backends that don't report size reliably.
  if (texW <= 0)
    texW = 512;
  if (texH <= 0)
    texH = 512;

  // Some render backends incorrectly cull sprites whose destination rect
  // starts far off-screen (negative Y). Instead of rendering a 512px-tall
  // background anchored below the top of the screen, render only the visible
  // bottom slice directly into the viewport.
  int sliceH = (texH > GAME_H) ? GAME_H : texH;
  SDL_Rect src = {0, texH - sliceH, texW, sliceH};

  int startX = -(int)(camX * parallaxCamScale);
  startX %= texW;
  if (startX > 0)
    startX -= texW;

  int dstY = GAME_H - sliceH;
  for (int x = startX; x < GAME_W; x += texW) {
    SDL_Rect dst = {x, dstY, texW, sliceH};
    SDL_RenderCopy(g_ren, tex, &src, &dst);
  }
}

void renderTall16x32WithShadow(SDL_Texture *tex, int frameX, const SDL_Rect &dst,
                               SDL_RendererFlip flip) {
  SDL_Rect dstTop = dst;
  dstTop.h = dst.h / 2;
  SDL_Rect dstBot = dstTop;
  dstBot.y += dstTop.h;

  SDL_Rect srcTop = {frameX, 0, 16, 16};
  SDL_Rect srcBot = {frameX, 16, 16, 16};

  renderCopyExWithShadow(tex, &srcTop, &dstTop, flip);
  renderCopyExWithShadow(tex, &srcBot, &dstBot, flip);
}

bool overlap(const Rect &a, const Rect &b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
         a.y + a.h > b.y;
}

static Rect enemyHitRect(const Entity &e) {
  Rect r = e.r;
  // Match classic SMB-ish stomp feel: treat tall enemies as a 16x16 hitbox
  // anchored to their feet. This also normalizes collision across enemies so
  // Koopa/Buzzy/Lakitu/etc don't feel "fatter" than Goombas.
  if (r.h > 16.0f) {
    r.y = r.y + (r.h - 16.0f);
    r.h = 16.0f;
  }
  return r;
}

inline bool jumpPressed(uint32_t pressed) {
  return (pressed & (VPAD_BUTTON_A | VPAD_BUTTON_B)) != 0;
}

inline bool jumpPressed() { return jumpPressed(g_pressed); }

inline bool jumpHeld(uint32_t held) {
  return (held & (VPAD_BUTTON_A | VPAD_BUTTON_B)) != 0;
}

inline bool jumpHeld() { return jumpHeld(g_held); }

inline bool firePressed(uint32_t pressed) {
  return (pressed & (VPAD_BUTTON_X | VPAD_BUTTON_Y)) != 0;
}

inline bool firePressed() { return firePressed(g_pressed); }

int mapWidth() { return g_levelInfo.mapWidth > 0 ? g_levelInfo.mapWidth : MAP_W; }

bool solidAt(int tx, int ty);
uint8_t collisionAt(int tx, int ty);
static bool computeCastleDst(SDL_Rect &outMainDst, SDL_Rect &outOverlayDst);

float standHeightForPower(Power p) {
  return (p >= P_BIG) ? PLAYER_HIT_H_BIG : PLAYER_HIT_H_SMALL;
}

static bool cameraBacktrackEnabled() {
  return g_multiplayerActive || g_allowCameraBacktrack;
}

// Returns true if the player should ignore enemy/projectile damage.
// Pit death is handled separately and is intentionally not affected by cheats.
static bool playerIsInvulnerable(const Player &pl) {
  return g_cheatGodMode || pl.invT > 0.0f;
}

static int autoPrimaryBgForTheme() {
  // Matches the upstream Godot project's "Auto" behavior for primary BG.
  // 0 = Hills, 1 = Bush.
  int primary = 0;
  if ((g_theme == THEME_JUNGLE || g_theme == THEME_AUTUMN) &&
      ((g_levelInfo.world > 4 && g_levelInfo.world <= 8) || g_nightMode)) {
    primary = 1;
  }
  return primary;
}

static int effectiveBgPrimary() {
  // Godot enum: 0 Hills, 1 Bush, 2 None, 3 Auto.
  // For this port we always want a "full" decoration stack; treat None as Auto.
  int primary = g_levelInfo.bgPrimary;
  if (primary == 0 || primary == 1)
    return primary;
  return autoPrimaryBgForTheme();
}

static int effectiveBgSecondary() {
  // Godot enum: 0 None, 1 Mushrooms, 2 Trees.
  int secondary = g_levelInfo.bgSecondary;
  if (secondary == 1 || secondary == 2)
    return secondary;
  // Default on for richer stages unless explicitly set.
  return 2;
}

void setPlayerSizePreserveFeet(Player &p, float newW, float newH) {
  float footY = p.r.y + p.r.h;
  p.r.w = newW;
  p.r.h = newH;
  p.r.y = footY - newH;
}

void setPlayerSizePreserveFeet(float newW, float newH) {
  setPlayerSizePreserveFeet(g_p, newW, newH);
}

bool rectBlockedBySolids(const Rect &r) {
  int tx1 = (int)(r.x / TILE);
  int tx2 = (int)((r.x + r.w - 1) / TILE);
  int ty1 = (int)(r.y / TILE);
  int ty2 = (int)((r.y + r.h - 1) / TILE);
  for (int ty = ty1; ty <= ty2; ty++) {
    for (int tx = tx1; tx <= tx2; tx++) {
      if (solidAt(tx, ty))
        return true;
    }
  }
  return false;
}

bool solidAt(int tx, int ty) {
  return collisionAt(tx, ty) == COL_SOLID;
}

uint8_t collisionAt(int tx, int ty) {
  if (tx < 0 || tx >= mapWidth() || ty < 0 || ty >= MAP_H)
    return COL_NONE;
  switch ((Tile)g_map[ty][tx]) {
  case T_GROUND:
  case T_BRICK:
  case T_QUESTION:
  case T_USED:
  case T_PIPE:
  case T_CASTLE:
    return COL_SOLID;
  case T_EMPTY:
  case T_FLAG:
  case T_COIN:
    return COL_NONE;
  default:
    break;
  }
  if (g_levelInfo.collide) {
    uint8_t col = g_levelInfo.collide[ty][tx];
    // Heuristic: treat floating "top surface" terrain tiles as one-way so
    // mushroom platforms and grassy ledges can be jumped through from below,
    // while ground-backed tiles remain fully solid.
    if (col == COL_SOLID && g_levelInfo.atlasX && g_levelInfo.atlasY &&
        g_levelInfo.atlasX[ty][tx] != 255 && g_levelInfo.atlasY[ty][tx] == 0 &&
        ty + 1 < MAP_H && g_levelInfo.collide[ty + 1][tx] == COL_NONE) {
      return COL_ONEWAY;
    }
    return col;
  }
  if (g_levelInfo.atlasX && g_levelInfo.atlasY &&
      g_levelInfo.atlasX[ty][tx] != 255 && g_levelInfo.atlasY[ty][tx] != 255)
    return COL_SOLID;
  return COL_NONE;
}

uint8_t questionMetaAt(int tx, int ty) {
  if (tx < 0 || tx >= mapWidth() || ty < 0 || ty >= MAP_H)
    return QMETA_COIN;
  if (!g_levelInfo.qmeta)
    return QMETA_COIN;
  return g_levelInfo.qmeta[ty][tx];
}

int questionRowForTheme(LevelTheme t) {
  switch (t) {
  case THEME_UNDERGROUND:
  case THEME_GHOSTHOUSE:
    return 16;
  case THEME_CASTLE:
    return 32;
  case THEME_SNOW:
    return 48;
  case THEME_SPACE:
    return 64;
  case THEME_VOLCANO:
    return 80;
  case THEME_BONUS:
    return 96;
  default:
    return 0;
  }
}

int fireFlowerRowForTheme(LevelTheme t) {
  switch (t) {
  case THEME_UNDERGROUND:
  case THEME_CASTLE:
  case THEME_GHOSTHOUSE:
    return 16;
  case THEME_UNDERWATER:
  case THEME_CASTLE_WATER:
    return 32;
  case THEME_SNOW:
    return 48;
  case THEME_SPACE:
    return 64;
  case THEME_VOLCANO:
    return 80;
  case THEME_BONUS:
    return 96;
  default:
    return 0;
  }
}

static int flagPolePaletteIndexForTheme(LevelTheme t) {
  switch (t) {
  case THEME_UNDERGROUND:
  case THEME_GHOSTHOUSE:
    return 1;
  case THEME_CASTLE:
  case THEME_CASTLE_WATER:
    return 2;
  case THEME_UNDERWATER:
    return 3;
  case THEME_SNOW:
    return 4;
  case THEME_SPACE:
    return 5;
  default:
    return 0;
  }
}

static int flagPaletteIndexForTheme(LevelTheme t) {
  // Flag.png palettes: default, Underground, Underwater, Snow, Space, Volcano, Bonus
  switch (t) {
  case THEME_UNDERGROUND:
  case THEME_GHOSTHOUSE:
    return 1;
  case THEME_UNDERWATER:
  case THEME_CASTLE_WATER:
    return 2;
  case THEME_SNOW:
    return 3;
  case THEME_SPACE:
    return 4;
  case THEME_VOLCANO:
    return 5;
  case THEME_BONUS:
    return 6;
  default:
    return 0;
  }
}

bool playerStomp(const Player &p) { return p.vy > 0.0f; }

SDL_Texture *loadTex(const char *file) {
  SDL_Surface *s = nullptr;
  if (file[0] == '/' ||
      strstr(file, "Super-Mario-Bros.-Remastered-Public") != nullptr) {
    s = IMG_Load(file);
  } else {
    char path[512];
    const char *paths[] = {"content/%s", "../content/%s", "fs:/vol/content/%s"};
    for (int i = 0; i < 3 && !s; i++) {
      snprintf(path, sizeof(path), paths[i], file);
      s = IMG_Load(path);
    }
  }
  if (!s)
    return nullptr;

  auto surfaceCornerIsChromaGreen = [](SDL_Surface *surf) -> bool {
    if (!surf || surf->w <= 0 || surf->h <= 0 || !surf->pixels || !surf->format)
      return false;
    const int x = 0;
    const int y = 0;
    const int bpp = surf->format->BytesPerPixel;
    const uint8_t *p = (const uint8_t *)surf->pixels + y * surf->pitch + x * bpp;
    Uint32 pixel = 0;
    switch (bpp) {
    case 1:
      pixel = *p;
      break;
    case 2:
      pixel = *(const Uint16 *)p;
      break;
    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        pixel = (p[0] << 16) | (p[1] << 8) | p[2];
      else
        pixel = p[0] | (p[1] << 8) | (p[2] << 16);
      break;
    default:
      pixel = *(const Uint32 *)p;
      break;
    }
    Uint8 r = 0, g = 0, b = 0, a = 0;
    SDL_GetRGBA(pixel, surf->format, &r, &g, &b, &a);
    return (r == 0 && g == 255 && b == 0);
  };

  // Prefer PNG alpha when present; fall back to bright-green chroma key for
  // legacy placeholder sheets. Some Godot-exported PNGs keep an unused alpha
  // channel while still using bright-green backgrounds; detect that via the
  // top-left pixel so enemy/FX sheets don't render as solid green.
  if (s->format->Amask == 0 ||
      surfaceCornerIsChromaGreen(s) ||
      (strstr(file, "tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/ui/TitleSMB1.png") != nullptr) ||
      (strstr(file, "sprites/ui/CoinIcon.png") != nullptr) ||
      (strstr(file, "FlagPole.png") != nullptr) ||
      (strstr(file, "QuestionBlock.png") != nullptr) ||
      (strstr(file, "FireFlower.png") != nullptr) ||
      (strstr(file, "Fireball.png") != nullptr) ||
      (strstr(file, "SpinningCoin.png") != nullptr) ||
      (strstr(file, "Platform.png") != nullptr)) {
    Uint32 colorKey = SDL_MapRGB(s->format, 0, 255, 0);
    SDL_SetColorKey(s, SDL_TRUE, colorKey);
  }

  g_loadedTex++;
  SDL_Texture *t = SDL_CreateTextureFromSurface(g_ren, s);
  SDL_FreeSurface(s);
  if (t) {
    // Ensure alpha blending is enabled consistently (helps with some assets
    // that rely on partial transparency).
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  }
  return t;
}

void setSfxVolume(Mix_Chunk *sfx) {
  if (sfx)
    Mix_VolumeChunk(sfx, MIX_MAX_VOLUME);
}

SDL_Texture *loadTexScaled(const char *file, int outW, int outH) {
  SDL_Surface *s = nullptr;
  if (file[0] == '/' ||
      strstr(file, "Super-Mario-Bros.-Remastered-Public") != nullptr) {
    s = IMG_Load(file);
  } else {
    char path[512];
    const char *paths[] = {"content/%s", "../content/%s", "fs:/vol/content/%s"};
    for (int i = 0; i < 3 && !s; i++) {
      snprintf(path, sizeof(path), paths[i], file);
      s = IMG_Load(path);
    }
  }
  if (!s)
    return nullptr;

  auto surfaceCornerIsChromaGreen = [](SDL_Surface *surf) -> bool {
    if (!surf || surf->w <= 0 || surf->h <= 0 || !surf->pixels || !surf->format)
      return false;
    const int x = 0;
    const int y = 0;
    const int bpp = surf->format->BytesPerPixel;
    const uint8_t *p = (const uint8_t *)surf->pixels + y * surf->pitch + x * bpp;
    Uint32 pixel = 0;
    switch (bpp) {
    case 1:
      pixel = *p;
      break;
    case 2:
      pixel = *(const Uint16 *)p;
      break;
    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        pixel = (p[0] << 16) | (p[1] << 8) | p[2];
      else
        pixel = p[0] | (p[1] << 8) | (p[2] << 16);
      break;
    default:
      pixel = *(const Uint32 *)p;
      break;
    }
    Uint8 r = 0, g = 0, b = 0, a = 0;
    SDL_GetRGBA(pixel, surf->format, &r, &g, &b, &a);
    return (r == 0 && g == 255 && b == 0);
  };

  // Apply the same chroma-key rules as loadTex().
  if (s->format->Amask == 0 ||
      surfaceCornerIsChromaGreen(s) ||
      (strstr(file, "tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/ui/TitleSMB1.png") != nullptr) ||
      (strstr(file, "sprites/ui/CoinIcon.png") != nullptr) ||
      (strstr(file, "FlagPole.png") != nullptr) ||
      (strstr(file, "QuestionBlock.png") != nullptr) ||
      (strstr(file, "FireFlower.png") != nullptr) ||
      (strstr(file, "Fireball.png") != nullptr) ||
      (strstr(file, "SpinningCoin.png") != nullptr) ||
      (strstr(file, "Platform.png") != nullptr)) {
    Uint32 colorKey = SDL_MapRGB(s->format, 0, 255, 0);
    SDL_SetColorKey(s, SDL_TRUE, colorKey);
  }

  SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, s->format->format);
  if (!scaled) {
    SDL_FreeSurface(s);
    return nullptr;
  }
  SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
  SDL_Rect dst = {0, 0, outW, outH};
  SDL_BlitScaled(s, nullptr, scaled, &dst);

  g_loadedTex++;
  SDL_Texture *t = SDL_CreateTextureFromSurface(g_ren, scaled);
  SDL_FreeSurface(scaled);
  SDL_FreeSurface(s);
  if (t) {
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  }
  return t;
}

static SDL_Surface *loadSurface(const char *file) {
  SDL_Surface *s = nullptr;
  if (file[0] == '/' ||
      strstr(file, "Super-Mario-Bros.-Remastered-Public") != nullptr) {
    s = IMG_Load(file);
  } else {
    char path[512];
    const char *paths[] = {"content/%s", "../content/%s", "fs:/vol/content/%s"};
    for (int i = 0; i < 3 && !s; i++) {
      snprintf(path, sizeof(path), paths[i], file);
      s = IMG_Load(path);
    }
  }
  return s;
}

static int computeFlagPoleShaftX() {
  SDL_Surface *s = loadSurface("sprites/tilesets/FlagPole.png");
  if (!s)
    return 8;
  // Expect 16px-wide palette columns. Sample a row below the top ball where the
  // thin shaft begins, and attach the flag to the leftmost opaque pixel.
  int sx = 8;
  int sampleY = 16;
  if (sampleY < 0)
    sampleY = 0;
  if (sampleY >= s->h)
    sampleY = s->h - 1;

  SDL_LockSurface(s);
  Uint32 *pixels = (Uint32 *)s->pixels;
  int pitch32 = s->pitch / 4;
  Uint8 r, g, b, a;
  int found = -1;
  for (int x = 0; x < 16 && x < s->w; x++) {
    Uint32 px = pixels[sampleY * pitch32 + x];
    SDL_GetRGBA(px, s->format, &r, &g, &b, &a);
    if (a != 0) {
      found = x;
      break;
    }
  }
  SDL_UnlockSurface(s);
  SDL_FreeSurface(s);
  if (found >= 0)
    sx = found;
  if (sx < 0)
    sx = 0;
  if (sx > 15)
    sx = 15;
  return sx;
}

void destroyTex(SDL_Texture *&t) {
  if (t) {
    SDL_DestroyTexture(t);
    t = nullptr;
  }
}

const char *themeName(LevelTheme t) {
  switch (t) {
  case THEME_OVERWORLD:
    return "Overworld";
  case THEME_UNDERGROUND:
    return "Underground";
  case THEME_CASTLE:
    return "Castle";
  case THEME_UNDERWATER:
    return "Underwater";
  case THEME_AIRSHIP:
    return "Airship";
  case THEME_DESERT:
    return "Desert";
  case THEME_SNOW:
    return "Snow";
  case THEME_JUNGLE:
    return "Jungle";
  case THEME_BEACH:
    return "Beach";
  case THEME_GARDEN:
    return "Garden";
  case THEME_MOUNTAIN:
    return "Mountain";
  case THEME_SKY:
    return "Sky";
  case THEME_AUTUMN:
    return "Autumn";
  case THEME_PIPELAND:
    return "PipeLand";
  case THEME_SPACE:
    return "Space";
  case THEME_VOLCANO:
    return "Volcano";
  case THEME_GHOSTHOUSE:
    return "GhostHouse";
  case THEME_CASTLE_WATER:
    return "CastleWater";
  case THEME_BONUS:
    return "Bonus";
  default:
    return "Overworld";
  }
}

Mix_Music *loadBgmByName(const char *name) {
  if (!name)
    return nullptr;
  const char *paths[] = {
      "content/audio/bgm/%s.mp3",
      "../content/audio/bgm/%s.mp3",
      "fs:/vol/content/audio/bgm/%s.mp3"};
  Mix_Music *m = nullptr;
  for (int i = 0; i < 3 && !m; i++) {
    char p[512];
    snprintf(p, sizeof(p), paths[i], name);
    m = Mix_LoadMUS(p);
  }
  return m;
}

const char *bgHillsName(LevelTheme t) {
  switch (t) {
  case THEME_OVERWORLD:
    return "Overworld";
  case THEME_UNDERGROUND:
    return "Underground";
  case THEME_CASTLE:
    return "Castle";
  case THEME_UNDERWATER:
    return "Underwater";
  case THEME_AIRSHIP:
    return "Airship";
  case THEME_DESERT:
    return "Desert";
  case THEME_SNOW:
    return "Snow";
  case THEME_JUNGLE:
    return "Jungle";
  case THEME_BEACH:
    return "Beach";
  case THEME_GARDEN:
    return "GardenHill";
  case THEME_MOUNTAIN:
    return "Mountain";
  case THEME_SKY:
    return "Sky";
  case THEME_AUTUMN:
    return "Autumn";
  case THEME_PIPELAND:
    return "PipeLand";
  case THEME_SPACE:
    return "Space";
  case THEME_VOLCANO:
    return "Volcano";
  case THEME_GHOSTHOUSE:
    return "GhostHouse";
  case THEME_CASTLE_WATER:
    return "CastleWater";
  case THEME_BONUS:
    return "Bonus";
  default:
    return "Overworld";
  }
}

const char *bgBushesName(LevelTheme t) {
  switch (t) {
  case THEME_UNDERGROUND:
    return "UndergroundBush";
  case THEME_CASTLE:
    return "CastleBush";
  case THEME_CASTLE_WATER:
    return "CastleWaterBush";
  case THEME_UNDERWATER:
    return "UnderwaterBush";
  case THEME_AIRSHIP:
    return "AirshipBush";
  case THEME_DESERT:
    return "DesertBush";
  case THEME_SNOW:
    return "SnowBush";
  case THEME_JUNGLE:
    return "JungleBush";
  case THEME_GARDEN:
    return "GardenBush";
  case THEME_AUTUMN:
    return "AutumnBush";
  case THEME_VOLCANO:
    return "VolcanoBush";
  case THEME_GHOSTHOUSE:
    return "GhostHouseBush";
  case THEME_SPACE:
    return "SpaceBush";
  case THEME_BONUS:
    return "BonusBush";
  case THEME_OVERWORLD:
  case THEME_BEACH:
  case THEME_MOUNTAIN:
  case THEME_SKY:
  case THEME_PIPELAND:
  default:
    return "Bush";
  }
}

void loadBackgroundArt() {
  destroyTex(g_texBgHills);
  destroyTex(g_texBgBushes);
  destroyTex(g_texBgCloudOverlay);
  destroyTex(g_texBgSky);
  destroyTex(g_texBgSecondary);

  auto tryLoadBg = [&](const char *dir, const char *file) -> SDL_Texture * {
    // Use separate buffers: `file` may itself be a temporary buffer, and we must
    // never snprintf() into the same buffer we're also reading from.
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "sprites/Backgrounds/%s/%.*s", dir, 400,
             file);
    return loadTex(fullPath);
  };
  auto tryLoadBgName = [&](const char *dir, const char *base,
                           bool nightMode) -> SDL_Texture * {
    // Prefer LL variants; many non-LL files are chroma-key sources (green).
    char fileBuf[512];
    if (nightMode) {
      // Some underwater assets use ...LLNight instead of ...NightLL.
      snprintf(fileBuf, sizeof(fileBuf), "%sNightLL.png", base);
      if (auto *t = tryLoadBg(dir, fileBuf))
        return t;
      snprintf(fileBuf, sizeof(fileBuf), "%sLLNight.png", base);
      if (auto *t = tryLoadBg(dir, fileBuf))
        return t;
      snprintf(fileBuf, sizeof(fileBuf), "%sNight.png", base);
      if (auto *t = tryLoadBg(dir, fileBuf))
        return t;
    }
    snprintf(fileBuf, sizeof(fileBuf), "%sLL.png", base);
    if (auto *t = tryLoadBg(dir, fileBuf))
      return t;
    snprintf(fileBuf, sizeof(fileBuf), "%s.png", base);
    return tryLoadBg(dir, fileBuf);
  };

  const char *hillsBase = bgHillsName(g_theme);
  g_texBgHills = tryLoadBgName("Hills", hillsBase, g_nightMode);
  if (!g_texBgHills) {
    g_texBgHills = tryLoadBgName("Hills", themeName(g_theme), g_nightMode);
  }

  const char *bushBase = bgBushesName(g_theme);
  g_texBgBushes = tryLoadBgName("Bushes", bushBase, g_nightMode);
  if (!g_texBgBushes) {
    g_texBgBushes = tryLoadBgName("Bushes", "Bush", g_nightMode);
  }

  // Overlays: prefer LL; the non-LL overlay is a green chroma source.
  g_texBgCloudOverlay =
      loadTex("sprites/Backgrounds/CloudOverlays/CloudOverlayLL.png");
  if (!g_texBgCloudOverlay) {
    g_texBgCloudOverlay =
        loadTex("sprites/Backgrounds/CloudOverlays/CloudOverlay.png");
  }

  // Sky texture (optional). The current game clears to a flat color, but
  // adding the subtle sky pass helps match the Godot project's look.
  const char *skyBase = nullptr;
  if (g_nightMode) {
    if (g_theme == THEME_SNOW)
      skyBase = "SnowNightStars";
    else if (g_theme == THEME_SPACE)
      skyBase = "SpaceStars";
    else
      skyBase = "NightStars";
    g_texBgSky = tryLoadBgName("Skies", skyBase, false);
  } else {
    switch (g_theme) {
    case THEME_BEACH:
      skyBase = "BeachSky";
      break;
    case THEME_AUTUMN:
      skyBase = "AutumnSky";
      break;
    case THEME_VOLCANO:
      skyBase = "VolcanoSky";
      break;
    case THEME_SPACE:
      skyBase = "TheVoid";
      break;
    case THEME_SNOW:
      skyBase = "SnowSky";
      break;
    default:
      skyBase = "DaySky";
      break;
    }
    g_texBgSky = tryLoadBgName("Skies", skyBase, false);
  }

  // Foreground (behind player) themed layer: Trees/Mushrooms.
  int secondary = effectiveBgSecondary();

  if (secondary == 1) {
    // Mushrooms use a suffix Night convention (e.g. BeachMushroomsNight).
    const char *base = nullptr;
    switch (g_theme) {
    case THEME_BEACH:
      base = "BeachMushrooms";
      break;
    case THEME_SNOW:
      base = "SnowMushrooms";
      break;
    case THEME_UNDERWATER:
    case THEME_CASTLE_WATER:
      base = "UnderwaterMushrooms";
      break;
    case THEME_AIRSHIP:
      base = "AirshipMushrooms";
      break;
    default:
      base = "Mushrooms";
      break;
    }
    g_texBgSecondary = tryLoadBgName("SecondaryMushrooms", base, g_nightMode);
  } else if (secondary == 2) {
    // Trees mostly use an infix Night convention (e.g. JungleNightTrees).
    const char *day = nullptr;
    const char *night = nullptr;
    switch (g_theme) {
    case THEME_UNDERWATER:
    case THEME_CASTLE_WATER:
      day = "UnderwaterTrees";
      night = "UnderwaterTreesNight";
      break;
    case THEME_UNDERGROUND:
    case THEME_GHOSTHOUSE:
      day = "UndergroundTrees";
      night = "UndergroundTrees";
      break;
    case THEME_JUNGLE:
      day = "JungleTrees";
      night = "JungleNightTrees";
      break;
    case THEME_SNOW:
      day = "SnowTrees";
      night = "SnowNightTrees";
      break;
    case THEME_AUTUMN:
      day = "AutumnTrees";
      night = "AutumnNightTrees";
      break;
    case THEME_BEACH:
      day = "BeachTrees";
      night = "BeachNightTrees";
      break;
    case THEME_CASTLE:
      day = "CastleTrees";
      night = "CastleNightTrees";
      break;
    case THEME_SPACE:
      day = "SpaceTrees";
      night = "SpaceTrees";
      break;
    case THEME_VOLCANO:
      day = "VolcanoTrees";
      night = "VolcanoTrees";
      break;
    case THEME_BONUS:
      day = "BonusTrees";
      night = "BonusTreesNight";
      break;
    default:
      day = "Trees";
      night = "NightTrees";
      break;
    }
    if (g_nightMode) {
      g_texBgSecondary = tryLoadBgName("SecondaryTrees", night, false);
      if (!g_texBgSecondary)
        g_texBgSecondary = tryLoadBgName("SecondaryTrees", day, false);
    } else {
      g_texBgSecondary = tryLoadBgName("SecondaryTrees", day, false);
    }
  }
}

void loadThemeTilesets() {
  destroyTex(g_texTerrain);
  destroyTex(g_texDeco);
  destroyTex(g_texLiquids);
  char path[256];
  snprintf(path, sizeof(path), "tilesets/Terrain/%s.png", themeName(g_theme));
  g_texTerrain = loadTex(path);
  if (!g_texTerrain) {
    // Backward-compat with older content layouts.
    snprintf(path, sizeof(path), "sprites/tilesets/%s.png", themeName(g_theme));
    g_texTerrain = loadTex(path);
  }

  snprintf(path, sizeof(path), "tilesets/Deco/%sDeco.png", themeName(g_theme));
  g_texDeco = loadTex(path);
  if (!g_texDeco) {
    snprintf(path, sizeof(path), "sprites/tilesets/Deco/%sDeco.png", themeName(g_theme));
    g_texDeco = loadTex(path);
  }

  g_texLiquids = loadTex("tilesets/Liquids.png");
  if (!g_texLiquids)
    g_texLiquids = loadTex("sprites/tilesets/Liquids.png");
}

Mix_Music *themeMusic(LevelTheme t) {
  if (t >= 0 && t < THEME_COUNT) {
    if (!g_bgmByTheme[t]) {
      g_bgmByTheme[t] = loadBgmByName(themeName(t));
    }
    if (g_bgmByTheme[t])
      return g_bgmByTheme[t];
  }
  return g_bgmOverworld;
}

void playThemeMusic(LevelTheme t) {
  Mix_Music *m = themeMusic(t);
  if (m && m != g_bgm) {
    Mix_HaltMusic();
    g_bgm = m;
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    Mix_PlayMusic(g_bgm, -1);
  } else if (m && !Mix_PlayingMusic()) {
    g_bgm = m;
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    Mix_PlayMusic(g_bgm, -1);
  }
}

void loadAssets() {
  for (int i = 0; i < g_charCount; i++) {
    char pSmall[256];
    char pBig[256];
    char pLife[256];
    char pFire[256];
    snprintf(pSmall, sizeof(pSmall), "sprites/players/%s/Small.png",
             g_charNames[i]);
    snprintf(pBig, sizeof(pBig), "sprites/players/%s/Big.png",
             g_charNames[i]);
    snprintf(pFire, sizeof(pFire), "sprites/players/%s/Fire.png",
             g_charNames[i]);
    snprintf(pLife, sizeof(pLife), "sprites/players/%s/LifeIcon.png",
             g_charNames[i]);
    g_texPlayerSmall[i] = loadTex(pSmall);
    g_texPlayerBig[i] = loadTex(pBig);
    g_texPlayerFire[i] = loadTex(pFire);
    g_texLifeIcon[i] = loadTex(pLife);
  }
  for (int i = 0; i < g_charCount; i++) {
    if (!g_texPlayerSmall[i])
      g_texPlayerSmall[i] = g_texPlayerSmall[0];
    if (!g_texPlayerBig[i])
      g_texPlayerBig[i] = g_texPlayerBig[0];
    if (!g_texPlayerFire[i])
      g_texPlayerFire[i] = g_texPlayerBig[i];
    if (!g_texLifeIcon[i])
      g_texLifeIcon[i] = g_texLifeIcon[0];
  }

  g_texGoomba = loadTex("sprites/enemies/Goomba.png");
  g_texKoopa = loadTex("sprites/enemies/KoopaTroopa.png");
  g_texKoopaSheet = loadTex("sprites/enemies/KoopaTroopaSheet.png");
  g_texCheepCheep = loadTex("sprites/enemies/CheepCheep.png");
  g_texBulletBill = loadTex("sprites/enemies/BulletBill.png");
  g_texBlooper = loadTex("sprites/enemies/Blooper.png");
  g_texBuzzy = loadTex("sprites/enemies/BuzzyBeetle.png");
  g_texLakitu = loadTex("sprites/enemies/Lakitu.png");
  g_texLakituCloud = loadTex("sprites/enemies/LakituCloud.png");
  g_texSpiny = loadTex("sprites/enemies/Spiny.png");
  g_texHammerBro = loadTex("sprites/enemies/HammerBro.png");
  g_texHammer = loadTex("sprites/items/Hammer.png");
  g_texBowser = loadTex("sprites/enemies/Bowser.png");
  g_texPlatform = loadTex("sprites/tilesets/Platform.png");
  g_texBridgeAxe = loadTex("sprites/items/BridgeAxe.png");
  g_texQuestion = loadTex("sprites/blocks/QuestionBlock.png");
  g_texMushroom = loadTex("sprites/items/SuperMushroom.png");
  g_texFireFlower = loadTex("sprites/items/FireFlower.png");
  g_texFireball = loadTex("sprites/items/Fireball.png");
  g_texCoin = loadTex("sprites/items/SpinningCoin.png");
  g_texFlagPole = loadTex("sprites/tilesets/FlagPole.png");
  g_texFlag = loadTex("sprites/tilesets/Flag.png");
  g_texCastle = loadTex("sprites/tilesets/EndingCastleSprite.png");
  g_texParticleSnow = loadTex("sprites/particles/Snow.png");
  g_texParticleLeaves = loadTex("sprites/particles/Leaves.png");
  g_texParticleAutumnLeaves = loadTex("sprites/particles/AutumnLeaves.png");
  g_flagPoleShaftX = computeFlagPoleShaftX();
  loadThemeTilesets();
  loadBackgroundArt();

  g_texTitle = loadTex("sprites/ui/Title2.png");
  g_texCursor = loadTex("sprites/ui/Cursor.png");
  g_texMenuBG = loadTex("sprites/ui/MenuBG.png");
  g_texCoinIcon = loadTex("sprites/ui/CoinIcon.png");

  const char *sfxPaths[] = {"content/audio/sfx/%s", "../content/audio/sfx/%s",
                            "fs:/vol/content/audio/sfx/%s"};
  for (int i = 0; i < 3 && !g_sfxJump; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "SmallJump.wav");
    g_sfxJump = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxBigJump; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "BigJump.wav");
    g_sfxBigJump = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxStomp; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Stomp.wav");
    g_sfxStomp = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxCoin; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Coin.wav");
    g_sfxCoin = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxPowerup; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Powerup.wav");
    g_sfxPowerup = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxBump; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Bump.wav");
    g_sfxBump = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxBreak; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "BreakBlock.wav");
    g_sfxBreak = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxItemAppear; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "ItemAppear.wav");
    g_sfxItemAppear = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxDamage; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Damage.wav");
    g_sfxDamage = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxSkid; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Skid.wav");
    g_sfxSkid = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxMenuMove; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "MenuNavigate.wav");
    g_sfxMenuMove = Mix_LoadWAV(p);
  }

  for (int i = 0; i < 3 && !g_sfxFlagSlide; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "FlagSlide.wav");
    g_sfxFlagSlide = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxCastleClear; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "CastleClear.wav");
    g_sfxCastleClear = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxPipe; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Pipe.wav");
    g_sfxPipe = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxKick; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Kick.wav");
    g_sfxKick = Mix_LoadWAV(p);
  }
  for (int i = 0; i < 3 && !g_sfxFireball; i++) {
    char p[512];
    snprintf(p, sizeof(p), sfxPaths[i], "Fireball.wav");
    g_sfxFireball = Mix_LoadWAV(p);
  }

  const char *bgmPathsOver[] = {"content/audio/bgm/Overworld.mp3",
                                "../content/audio/bgm/Overworld.mp3",
                                "fs:/vol/content/audio/bgm/Overworld.mp3"};
  const char *bgmPathsUnd[] = {"content/audio/bgm/Underground.mp3",
                               "../content/audio/bgm/Underground.mp3",
                               "fs:/vol/content/audio/bgm/Underground.mp3"};
  const char *bgmPathsCast[] = {"content/audio/bgm/Castle.mp3",
                                "../content/audio/bgm/Castle.mp3",
                                "fs:/vol/content/audio/bgm/Castle.mp3"};
  for (int i = 0; i < 3 && !g_bgmOverworld; i++)
    g_bgmOverworld = Mix_LoadMUS(bgmPathsOver[i]);
  for (int i = 0; i < 3 && !g_bgmUnderground; i++)
    g_bgmUnderground = Mix_LoadMUS(bgmPathsUnd[i]);
  for (int i = 0; i < 3 && !g_bgmCastle; i++)
    g_bgmCastle = Mix_LoadMUS(bgmPathsCast[i]);

  g_bgmByTheme[THEME_OVERWORLD] = g_bgmOverworld;
  g_bgmByTheme[THEME_UNDERGROUND] = g_bgmUnderground;
  g_bgmByTheme[THEME_CASTLE] = g_bgmCastle;

  setSfxVolume(g_sfxJump);
  setSfxVolume(g_sfxBigJump);
  setSfxVolume(g_sfxStomp);
  setSfxVolume(g_sfxCoin);
  setSfxVolume(g_sfxPowerup);
  setSfxVolume(g_sfxBump);
  setSfxVolume(g_sfxBreak);
  setSfxVolume(g_sfxItemAppear);
  setSfxVolume(g_sfxDamage);
  setSfxVolume(g_sfxSkid);
  setSfxVolume(g_sfxMenuMove);
  setSfxVolume(g_sfxFlagSlide);
  setSfxVolume(g_sfxCastleClear);
  setSfxVolume(g_sfxPipe);
  setSfxVolume(g_sfxKick);
  setSfxVolume(g_sfxFireball);

  // Keep plenty of channels available so short SFX (like brick breaks) don't
  // get dropped during busy scenes.
}

void spawnEnemiesFromLevel() {
  for (int i = 0; i < 64; i++)
    g_ents[i].on = false;
  int idx = 0;
  for (int i = 0; i < g_levelInfo.enemyCount && idx < 64; i++) {
    const EnemySpawn &s = g_levelInfo.enemies[i];
    Entity &e = g_ents[idx];
    e.on = true;
    e.type = s.type;
    e.vx = 0.0f;
    e.vy = 0.0f;
    e.dir = s.dir;
    e.state = 0;
    e.timer = 0.0f;
    e.a = s.a;
    e.b = s.b;
    e.baseX = s.x;
    e.baseY = s.y;
    e.prevX = s.x;
    e.prevY = s.y;

    switch (s.type) {
    case E_GOOMBA:
    case E_KOOPA:
    case E_KOOPA_RED: {
      float h = 16.0f;
      float y = s.y - (h - 16.0f);
      e.r = {s.x, y, 16.0f, h};
      e.vx = -Physics::ENEMY_SPEED;
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_BUZZY_BEETLE: {
      e.r = {s.x, s.y, 16.0f, 16.0f};
      e.vx = -Physics::ENEMY_SPEED;
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_BLOOPER: {
      // Blooper frames are 16x24; keep the hitbox aligned to that size.
      e.r = {s.x, s.y, 16.0f, 24.0f};
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_LAKITU: {
      // Lakitu floats; treat y as its top.
      e.r = {s.x, s.y, 16.0f, 24.0f};
      e.baseY = s.y;
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_HAMMER_BRO: {
      // Hammer Bros are 16x24 sprites standing on blocks.
      e.r = {s.x, s.y - 8.0f, 16.0f, 24.0f};
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_CHEEP_SWIM:
    case E_CHEEP_LEAP:
    case E_BULLET_BILL: {
      e.r = {s.x, s.y, 16.0f, 16.0f};
      if (e.type == E_BULLET_BILL && e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_SPINY: {
      e.r = {s.x, s.y, 16.0f, 16.0f};
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    case E_BULLET_CANNON: {
      // Cannon is a logic-only emitter; the visible cannon is part of the
      // terrain tileset in the upstream levels.
      e.r = {s.x, s.y, 0.0f, 0.0f};
      e.state = 0;
      e.timer = 0.0f;
      break;
    }
    case E_PLATFORM_SIDEWAYS: {
      float w = (float)((s.a > 0) ? s.a : 48);
      e.r = {s.x, s.y, w, 8.0f};
      e.baseX = s.x;
      e.baseY = s.y;
      break;
    }
    case E_PLATFORM_VERTICAL: {
      // Two modes:
      // - `dir == 0`: pingpong platform (width stored in `a`)
      // - `dir != 0`: elevator wrap platform (top/bottom in `a/b`)
      if (s.dir == 0) {
        float w = (float)((s.a > 0) ? s.a : 48);
        e.r = {s.x, s.y, w, 8.0f};
      } else {
        e.r = {s.x, s.y, 48.0f, 8.0f};
      }
      e.baseX = s.x;
      e.baseY = s.y;
      break;
    }
    case E_PLATFORM_ROPE: {
      float w = (float)((s.a > 0) ? s.a : 48);
      e.r = {s.x, s.y, w, 8.0f};
      e.baseX = s.x;
      e.baseY = s.y;
      break;
    }
    case E_PLATFORM_FALLING: {
      float w = (float)((s.a > 0) ? s.a : 48);
      e.r = {s.x, s.y, w, 8.0f};
      e.baseX = s.x;
      e.baseY = s.y;
      e.state = 0; // waiting
      break;
    }
    case E_ENTITY_GENERATOR:
    case E_ENTITY_GENERATOR_STOP: {
      e.r = {s.x, s.y, 0.0f, 0.0f};
      e.state = 0; // inactive
      // Track the player's last X so activation matches Godot's
      // PlayerDetection (crossing the trigger once, not "player is >= x").
      e.prevX = g_p.r.x + g_p.r.w * 0.5f;
      e.timer = 0.0f;
      break;
    }
    case E_CASTLE_AXE: {
      e.r = {s.x, s.y, 16.0f, 16.0f};
      break;
    }
    case E_BOWSER: {
      // Spawn centered-ish; Bowser is large visually, but keep a smaller
      // gameplay rect for now.
      e.r = {s.x - 24.0f, s.y - 48.0f, 48.0f, 48.0f};
      if (e.dir == 0)
        e.dir = -1;
      break;
    }
    default: {
      e.r = {s.x, s.y, 16.0f, 16.0f};
      break;
    }
    }
    idx++;
  }
}

static void generateForegroundDecos() {
  g_fgDecoCount = 0;
  if (!g_texDeco)
    return;

  int w = mapWidth();

  // Avoid overlapping pipes and other key objects. PipeLink metadata only
  // exists for warp pipes, but we want to avoid *any* pipe tiles.
	  auto decoNearPipe = [&](int tx, int ty, int wTiles, int hTiles) -> bool {
    constexpr int kMarginX = 3;
    constexpr int kMarginY = 3;
    int left = tx - kMarginX;
    int right = tx + wTiles - 1 + kMarginX;
    int top = (ty - (hTiles - 1)) - kMarginY;
    int bottom = (ty + 1) + kMarginY; // include support row
	    for (int y = top; y <= bottom; y++) {
	      if (y < 0 || y >= MAP_H)
	        continue;
	      for (int x = left; x <= right; x++) {
	        if (x < 0 || x >= w)
	          continue;
	        if (g_map[y][x] == T_PIPE)
	          return true;
	      }
	    }
	    // Also avoid known pipe mouths from metadata (covers atlas-rendered pipes).
	    if (g_levelInfo.pipes && g_levelInfo.pipeCount > 0) {
	      SDL_Rect decoRect = {tx * TILE, (ty - (hTiles - 1)) * TILE, wTiles * TILE,
	                           hTiles * TILE};
	      for (int i = 0; i < g_levelInfo.pipeCount; i++) {
	        const PipeLink &p = g_levelInfo.pipes[i];
	        SDL_Rect mouth = {p.x * TILE, p.y * TILE, 2 * TILE, 2 * TILE};
	        SDL_Rect expanded = {mouth.x - 3 * TILE, mouth.y - 3 * TILE,
	                             mouth.w + 6 * TILE, mouth.h + 6 * TILE};
	        if (SDL_HasIntersection(&decoRect, &expanded))
	          return true;
	      }
	    }
	    return false;
	  };

  // Local RNG: stable per-load and doesn't affect gameplay RNG.
  uint32_t rng = (uint32_t)SDL_GetTicks();
  rng ^= (uint32_t)(g_levelIndex * 0x9E3779B1u);
  rng ^= (uint32_t)(g_sectionIndex * 0x85EBCA6Bu);
  rng ^= (uint32_t)(g_theme * 0xC2B2AE35u);
  auto nextU32 = [&]() -> uint32_t {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  auto inBounds = [&](int tx, int ty) -> bool {
    return tx >= 0 && tx < w && ty >= 0 && ty < MAP_H;
  };

  auto isEmptyTile = [&](int tx, int ty) -> bool {
    if (!inBounds(tx, ty))
      return false;
    return g_map[ty][tx] == T_EMPTY || g_map[ty][tx] == T_COIN;
  };

  auto areaEmpty = [&](int tx, int ty, int wTiles, int hTiles) -> bool {
    for (int y = 0; y < hTiles; y++) {
      for (int x = 0; x < wTiles; x++) {
        if (!isEmptyTile(tx + x, ty + y))
          return false;
      }
    }
    return true;
  };

  auto supportIsGround = [&](int tx, int ty) -> bool {
    if (!inBounds(tx, ty))
      return false;
    // Allow any solid/one-way floor as support, but avoid pipe-stamped safety
    // tiles and other special gameplay blocks.
    uint8_t tile = g_map[ty][tx];
    if (tile == T_EMPTY)
      return false;
    if (tile == T_PIPE || tile == T_QUESTION || tile == T_USED || tile == T_BRICK ||
        tile == T_CASTLE)
      return false;
    uint8_t col = collisionAt(tx, ty);
    return col == COL_SOLID || col == COL_ONEWAY;
  };

	  struct Pattern {
	    uint8_t w;
	    uint8_t h;
	    uint8_t cellCount;
	    ForegroundDeco::Cell cells[16];
	  };

	  // Extract patterns from the level's *placed* deco tiles. In the remastered
	  // project, a single "deco object" is made of multiple connected tiles, but
	  // the atlas itself is densely packed and can't be segmented reliably.
	  std::vector<Pattern> patterns;
	  {
	    if (!g_levelInfo.atlasT || !g_levelInfo.atlasX || !g_levelInfo.atlasY)
	      return;
	    int mw = mapWidth();
	    std::vector<uint8_t> vis(mw * MAP_H, 0);
	    auto vidx = [&](int tx, int ty) { return ty * mw + tx; };
	    auto isDeco = [&](int tx, int ty) -> bool {
	      if (tx < 0 || tx >= mw || ty < 0 || ty >= MAP_H)
	        return false;
	      uint8_t ax = g_levelInfo.atlasX[ty][tx];
	      uint8_t ay = g_levelInfo.atlasY[ty][tx];
	      if (ax == 255 || ay == 255)
	        return false;
	      return g_levelInfo.atlasT[ty][tx] == ATLAS_DECO;
	    };
	    auto keyFor = [&](Pattern &p) -> std::string {
	      // Sort cells for stable key
	      for (int i = 0; i < p.cellCount; i++) {
	        for (int j = i + 1; j < p.cellCount; j++) {
	          const auto &a = p.cells[i];
	          const auto &b = p.cells[j];
	          if (a.dy != b.dy)
	            continue;
	          if (a.dx != b.dx)
	            continue;
	        }
	      }
	      // Simple string key
	      std::string k;
	      k.reserve(64);
	      k += std::to_string(p.w) + "x" + std::to_string(p.h) + ":";
	      for (int i = 0; i < p.cellCount; i++) {
	        const auto &c = p.cells[i];
	        k += std::to_string((int)c.dx) + "," + std::to_string((int)c.dy) +
	             "," + std::to_string((int)c.ax) + "," +
	             std::to_string((int)c.ay) + ";";
	      }
	      return k;
	    };

	    std::vector<std::string> keys;
	    keys.reserve(64);

	    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	    for (int ty = 0; ty < MAP_H; ty++) {
	      for (int tx = 0; tx < mw; tx++) {
	        if (!isDeco(tx, ty) || vis[vidx(tx, ty)])
	          continue;
	        std::vector<std::pair<int, int>> q;
	        q.push_back({tx, ty});
	        vis[vidx(tx, ty)] = 1;
	        int minTx = tx, maxTx = tx, minTy = ty, maxTy = ty;
	        for (size_t qi = 0; qi < q.size(); qi++) {
	          auto [cx, cy] = q[qi];
	          if (cx < minTx)
	            minTx = cx;
	          if (cx > maxTx)
	            maxTx = cx;
	          if (cy < minTy)
	            minTy = cy;
	          if (cy > maxTy)
	            maxTy = cy;
	          for (auto &d : dirs) {
	            int nx = cx + d[0];
	            int ny = cy + d[1];
	            if (nx < 0 || nx >= mw || ny < 0 || ny >= MAP_H)
	              continue;
	            if (!isDeco(nx, ny) || vis[vidx(nx, ny)])
	              continue;
	            vis[vidx(nx, ny)] = 1;
	            q.push_back({nx, ny});
	          }
	        }

	        int pw = maxTx - minTx + 1;
	        int ph = maxTy - minTy + 1;
	        if (pw <= 0 || ph <= 0)
	          continue;
	        // Skip huge components (these are usually decorative backgrounds baked
	        // into the tilemap, not "placeable" foreground objects).
	        if (pw > 6 || ph > 4 || (int)q.size() > 16)
	          continue;

	        Pattern pat = {};
	        pat.w = (uint8_t)pw;
	        pat.h = (uint8_t)ph;
	        int bottomTy = maxTy;
	        for (auto &cell : q) {
	          if (pat.cellCount >=
	              (uint8_t)(sizeof(pat.cells) / sizeof(pat.cells[0])))
	            break;
	          int cx = cell.first;
	          int cy = cell.second;
	          pat.cells[pat.cellCount++] = {
	              (int8_t)(cx - minTx),
	              (int8_t)(cy - bottomTy),
	              g_levelInfo.atlasX[cy][cx],
	              g_levelInfo.atlasY[cy][cx],
	          };
	        }

	        std::string k = keyFor(pat);
	        bool dup = false;
	        for (auto &existing : keys) {
	          if (existing == k) {
	            dup = true;
	            break;
	          }
	        }
	        if (!dup) {
	          keys.push_back(std::move(k));
	          patterns.push_back(pat);
	        }
	      }
	    }
	  }
	  if (patterns.empty())
	    return;

  // Extra clearance so larger decos don't clip into nearby objects.
  constexpr int kPadX = 3;
  constexpr int kPadY = 1;

  int desired = 10 + w / 40;
  if (desired > 28)
    desired = 28;

	  int attempts = 0;
	  while (g_fgDecoCount < desired && attempts++ < 800) {
	    const Pattern &pat = patterns[(int)(nextU32() % (uint32_t)patterns.size())];

    if (w <= pat.w + kPadX * 2)
      continue;

    int minTx = kPadX;
    int maxTx = w - (int)pat.w - kPadX;
    if (maxTx < minTx)
      continue;
    int tx = minTx + (int)(nextU32() % (uint32_t)(maxTx - minTx + 1));

    for (int ty = MAP_H - 3; ty >= 2; ty--) {
      if (ty - (int)(pat.h - 1) < 0)
        continue;
      // Keep clutter down by avoiding very high platforms.
      if (ty < 8)
        continue;

      // The whole footprint must be empty.
      if (!areaEmpty(tx, ty - (int)(pat.h - 1), pat.w, pat.h))
        continue;

	      // Require solid support under every bottom cell in the pattern (prevents
	      // placing on pipes/blocks and avoids impossible overhangs).
	      bool supported = true;
	      for (int c = 0; c < pat.cellCount; c++) {
	        const auto &cell = pat.cells[c];
	        if (cell.dy != 0)
	          continue;
	        int sx = tx + cell.dx;
	        if (!supportIsGround(sx, ty + 1)) {
	          supported = false;
	          break;
	        }
	      }
	      if (!supported)
	        continue;

      int clearX = tx - kPadX;
      int clearY = (ty - (int)(pat.h - 1)) - kPadY;
      int clearW = pat.w + kPadX * 2;
      int clearH = pat.h + kPadY;
      if (!areaEmpty(clearX, clearY, clearW, clearH))
        continue;
      if (decoNearPipe(tx, ty, pat.w, pat.h))
        continue;

	      if (g_fgDecoCount < (int)(sizeof(g_fgDecos) / sizeof(g_fgDecos[0]))) {
	        ForegroundDeco d = {tx, ty, pat.w, pat.h, pat.cellCount, {}};
	        for (int i = 0; i < pat.cellCount; i++)
	          d.cells[i] = pat.cells[i];
	        g_fgDecos[g_fgDecoCount++] = d;
	      }
	      break;
	    }
	  }
}

void applySection(bool resetTimer, int spawnX, int spawnY) {
  LevelTheme chosen = g_levelInfo.theme;
  if (g_randomTheme) {
    static bool seeded = false;
    if (!seeded) {
      srand((unsigned)SDL_GetTicks());
      seeded = true;
    }
    chosen = (LevelTheme)(rand() % THEME_COUNT);
  } else if (g_themeOverride >= 0) {
    chosen = (LevelTheme)g_themeOverride;
  }
  g_theme = chosen;
  g_flagX = g_levelInfo.flagX;
  g_hasFlag = g_levelInfo.hasFlag;
  loadThemeTilesets();
  loadBackgroundArt();
  resetAmbientParticles();

  // Position all active players at the new section spawn. Player sizes/power
  // are preserved, but runtime motion states are reset.
  for (int i = 0; i < g_playerCount; i++) {
    Player &p = g_players[i];
    p.dead = false;
    p.vx = 0;
    p.vy = 0;
    p.ground = false;
    p.right = true;
    p.jumping = false;
    p.crouch = false;
    p.invT = 0;
    p.animT = 0;
    p.throwT = 0;
    p.fireCooldown = 0.0f;
    p.swimCooldown = 0.0f;
    p.swimAnimT = 9999.0f;

    p.r.w = (p.power >= P_BIG) ? PLAYER_HIT_W_BIG : PLAYER_HIT_W_SMALL;
    p.r.h = standHeightForPower(p.power);

    // Stagger non-leader spawns a bit to reduce immediate overlap.
    int ox = (i == 0) ? 0 : (-12 - 12 * (i - 1));
    p.r.x = (float)(spawnX + ox);
    p.r.y = (float)(spawnY - p.r.h);
  }
  g_flagSfxPlayed = false;
  g_castleSfxPlayed = false;

  g_camX = 0;
  if (resetTimer) {
    g_time = 400;
    g_timeAcc = 0;
  }
  g_flagY = 3 * TILE;
  spawnEnemiesFromLevel();
  generateForegroundDecos();
  g_state = GS_PLAYING;
  playThemeMusic(g_theme);
}

static void setupLevel() {
  if (!loadLevelSection(g_levelIndex, g_sectionIndex, g_map, g_levelInfo))
    return;
  applySection(true, g_levelInfo.startX, g_levelInfo.startY);
}

void startNewGame() {
  g_levelIndex = 0;
  g_sectionIndex = 0;
  for (int i = 0; i < g_playerCount; i++) {
    Player &p = g_players[i];
    p.lives = 3;
    p.coins = 0;
    p.score = 0;
    p.power = P_SMALL;
    p.r.w = PLAYER_HIT_W_SMALL;
    p.r.h = PLAYER_HIT_H_SMALL;
    p.crouch = false;
    p.fireCooldown = 0.0f;
    p.throwT = 0.0f;
    p.invT = 0.0f;
    p.animT = 0.0f;
    p.swimCooldown = 0.0f;
    p.swimAnimT = 9999.0f;
    p.dead = false;
    p.vx = 0.0f;
    p.vy = 0.0f;
    p.ground = false;
    p.jumping = false;
    p.right = true;
  }
  setupLevel();
}

void restartLevel() {
  for (int i = 0; i < g_playerCount; i++) {
    Player &p = g_players[i];
    p.power = P_SMALL;
    p.r.w = PLAYER_HIT_W_SMALL;
    p.r.h = PLAYER_HIT_H_SMALL;
    p.crouch = false;
    p.fireCooldown = 0.0f;
    p.throwT = 0.0f;
    p.invT = 0.0f;
    p.swimCooldown = 0.0f;
    p.swimAnimT = 9999.0f;
    p.dead = false;
  }
  setupLevel();
}

void nextLevel() {
  g_levelIndex++;
  if (g_levelIndex >= levelCount())
    g_levelIndex = 0;
  g_sectionIndex = 0;
  setupLevel();
}

static uint32_t mapWiimoteButtonsToVpad(uint32_t wpadButtons, bool menuContext) {
  uint32_t out = 0;
  // Wii Remote is held sideways in SMB-style play:
  //   - UP   (towards the IR camera) == LEFT
  //   - DOWN (towards the B trigger) == RIGHT
  //   - LEFT == DOWN
  //   - RIGHT == UP
  if (wpadButtons & WPAD_BUTTON_UP)
    out |= VPAD_BUTTON_LEFT;
  if (wpadButtons & WPAD_BUTTON_DOWN)
    out |= VPAD_BUTTON_RIGHT;
  if (wpadButtons & WPAD_BUTTON_LEFT)
    out |= VPAD_BUTTON_DOWN;
  if (wpadButtons & WPAD_BUTTON_RIGHT)
    out |= VPAD_BUTTON_UP;
  if (wpadButtons & WPAD_BUTTON_PLUS)
    out |= VPAD_BUTTON_PLUS;
  if (wpadButtons & WPAD_BUTTON_MINUS)
    out |= VPAD_BUTTON_MINUS;

  // Sideways Wii Remote convention:
  // - 2: Jump / Confirm
  // - 1: Run (gameplay) / Back (menus)
  if (wpadButtons & (WPAD_BUTTON_2 | WPAD_BUTTON_A))
    out |= VPAD_BUTTON_A;
  if (wpadButtons & (WPAD_BUTTON_1 | WPAD_BUTTON_B)) {
    out |= menuContext ? VPAD_BUTTON_B : VPAD_BUTTON_Y;
  }
  return out;
}

static bool anyWiimoteConnected() {
  for (int i = 0; i < 4; i++) {
    if (g_remoteConnected[i])
      return true;
  }
  return false;
}

void input() {
  // Default to "no input" so a dropped VPAD read doesn't leave stale buttons.
  g_pressed = 0;
  g_held = 0;

  VPADStatus vpad;
  VPADReadError err;
  VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
  if (err == VPAD_READ_SUCCESS) {
    g_pressed = vpad.trigger;
    g_held = vpad.hold;
    if (vpad.leftStick.x > 0.3f)
      g_held |= VPAD_BUTTON_RIGHT;
    else if (vpad.leftStick.x < -0.3f)
      g_held |= VPAD_BUTTON_LEFT;
  }

  // Wii Remotes (Players 2-4). Map inputs into VPAD-like bits so existing
  // logic can be reused.
  bool menuContext = (g_state == GS_TITLE) || (g_state == GS_PAUSE);
  for (int chan = 0; chan < 4; chan++) {
    WPADExtensionType ext = WPAD_EXT_DEV_NOT_FOUND;
    WPADError werr = WPADProbe((WPADChan)chan, &ext);
    bool connected = (werr != WPAD_ERROR_NO_CONTROLLER);
    g_remoteConnected[chan] = connected;
    if (!connected) {
      g_remoteHoldRaw[chan] = 0;
      g_remoteHeld[chan] = 0;
      g_remotePressed[chan] = 0;
      continue;
    }

    KPADStatus st;
    KPADError kerr = KPAD_ERROR_OK;
    uint32_t n = KPADReadEx((KPADChan)chan, &st, 1, &kerr);
    uint32_t rawHold = g_remoteHoldRaw[chan];
    uint32_t rawTrig = 0;
    if (n > 0 && kerr == KPAD_ERROR_OK) {
      rawHold = st.hold;
      rawTrig = st.trigger;
      g_remoteHoldRaw[chan] = rawHold;
    }
    g_remoteHeld[chan] = mapWiimoteButtonsToVpad(rawHold, menuContext);
    g_remotePressed[chan] = mapWiimoteButtonsToVpad(rawTrig, menuContext);
  }

  // Publish per-player button sets (player indices map to GamePad + assigned remotes).
  g_playerHeld[0] = g_held;
  g_playerPressed[0] = g_pressed;
  for (int i = 1; i < 4; i++) {
    int chan = g_playerRemoteChan[i];
    if (chan >= 0 && chan < 4) {
      g_playerHeld[i] = g_remoteHeld[chan];
      g_playerPressed[i] = g_remotePressed[chan];
    } else {
      g_playerHeld[i] = 0;
      g_playerPressed[i] = 0;
    }
  }

  SDL_Event e;
  while (SDL_PollEvent(&e))
    if (e.type == SDL_QUIT)
      WHBProcStopRunning();

  // Quick debug toggle (GamePad right-stick click). Useful on hardware where
  // some renderers behave differently than Cemu.
  if (g_pressed & VPAD_BUTTON_STICK_R) {
    g_showDebugOverlay = !g_showDebugOverlay;
  }
}

void updateTitle() {
  switch (g_titleMode) {
  case TITLE_MAIN: {
    constexpr int kCount = 3; // PLAY, SETTINGS, EXTRAS
    if (g_pressed & VPAD_BUTTON_UP) {
      g_mainMenuIndex = (g_mainMenuIndex + kCount - 1) % kCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    } else if (g_pressed & VPAD_BUTTON_DOWN) {
      g_mainMenuIndex = (g_mainMenuIndex + 1) % kCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }
    if (g_pressed & VPAD_BUTTON_A) {
      if (g_mainMenuIndex == 0) {
        if (anyWiimoteConnected()) {
          // Enter multiplayer character select.
          g_titleMode = TITLE_MULTI_SELECT;

          // Build the list of connected Wii Remote channels and assign them to
          // player slots 2..4.
          int chans[4] = {-1, -1, -1, -1};
          int count = 0;
          for (int c = 0; c < 4 && count < 3; c++) {
            if (g_remoteConnected[c])
              chans[count++] = c;
          }
          g_playerCount = 1 + count;
          g_playerRemoteChan[0] = -1;
          for (int i = 1; i < 4; i++)
            g_playerRemoteChan[i] = (i - 1 < count) ? chans[i - 1] : -1;

          for (int i = 0; i < 4; i++) {
            g_playerReady[i] = false;
            g_playerMenuIndex[i] = g_charIndex;
          }
          // Give each player a different default selection when possible.
          for (int i = 1; i < g_playerCount; i++) {
            g_playerMenuIndex[i] = (g_charIndex + i) % g_charCount;
          }
          g_playerMenuIndex[0] = g_charIndex;
        } else {
          g_titleMode = TITLE_CHAR_SELECT;
          g_menuIndex = g_charIndex;
        }
      } else if (g_mainMenuIndex == 1) {
        g_titleMode = TITLE_OPTIONS;
        g_optionsIndex = 0;
      } else {
        g_titleMode = TITLE_EXTRAS;
      }
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }
    break;
  }
  case TITLE_CHAR_SELECT: {
    if (g_pressed & VPAD_BUTTON_LEFT) {
      g_menuIndex = (g_menuIndex + g_charCount - 1) % g_charCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    } else if (g_pressed & VPAD_BUTTON_RIGHT) {
      g_menuIndex = (g_menuIndex + 1) % g_charCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_Y) {
      g_titleMode = TITLE_OPTIONS;
      g_optionsIndex = 0;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_B) {
      g_titleMode = TITLE_MAIN;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_A) {
      g_multiplayerActive = false;
      g_playerCount = 1;
      g_playerCharIndex[0] = g_menuIndex;
      g_charIndex = g_menuIndex;
      startNewGame();
    }
    break;
  }
  case TITLE_MULTI_SELECT: {
    auto allReady = [&]() -> bool {
      for (int i = 0; i < g_playerCount; i++) {
        if (!g_playerReady[i])
          return false;
      }
      return true;
    };

    for (int i = 0; i < g_playerCount; i++) {
      uint32_t pressed = g_playerPressed[i];
      if (!g_playerReady[i]) {
        if (pressed & VPAD_BUTTON_LEFT) {
          g_playerMenuIndex[i] =
              (g_playerMenuIndex[i] + g_charCount - 1) % g_charCount;
          if (g_sfxMenuMove)
            Mix_PlayChannel(-1, g_sfxMenuMove, 0);
        } else if (pressed & VPAD_BUTTON_RIGHT) {
          g_playerMenuIndex[i] = (g_playerMenuIndex[i] + 1) % g_charCount;
          if (g_sfxMenuMove)
            Mix_PlayChannel(-1, g_sfxMenuMove, 0);
        }
        if (pressed & VPAD_BUTTON_A) {
          g_playerReady[i] = true;
          if (g_sfxMenuMove)
            Mix_PlayChannel(-1, g_sfxMenuMove, 0);
        }
      } else {
        // Un-ready with back.
        if (pressed & VPAD_BUTTON_B) {
          g_playerReady[i] = false;
          if (g_sfxMenuMove)
            Mix_PlayChannel(-1, g_sfxMenuMove, 0);
        }
      }
    }

    // Back out to main menu (only GamePad can do this).
    if (g_playerPressed[0] & VPAD_BUTTON_B) {
      g_titleMode = TITLE_MAIN;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    // Start once everyone is ready; allow A or PLUS on GamePad.
    if (allReady() && (g_playerPressed[0] & (VPAD_BUTTON_PLUS | VPAD_BUTTON_A))) {
      g_multiplayerActive = (g_playerCount > 1);
      if (g_multiplayerActive)
        g_allowCameraBacktrack = true; // forced on in multiplayer

      for (int i = 0; i < g_playerCount; i++) {
        g_playerCharIndex[i] = g_playerMenuIndex[i];
      }
      g_charIndex = g_playerCharIndex[0];
      startNewGame();
    }

    break;
  }
  case TITLE_OPTIONS: {
    constexpr int kOptCount = 4; // random theme, night mode, backtrack, cheats
    if (g_pressed & VPAD_BUTTON_UP) {
      g_optionsIndex = (g_optionsIndex + kOptCount - 1) % kOptCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    } else if (g_pressed & VPAD_BUTTON_DOWN) {
      g_optionsIndex = (g_optionsIndex + 1) % kOptCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_A) {
      bool forcedBacktrack = g_multiplayerActive;
      if (g_optionsIndex == 0) {
        g_randomTheme = !g_randomTheme;
        if (g_randomTheme)
          g_themeOverride = -1;
      } else if (g_optionsIndex == 1) {
        g_nightMode = !g_nightMode;
      } else if (g_optionsIndex == 2) {
        if (!forcedBacktrack)
          g_allowCameraBacktrack = !g_allowCameraBacktrack;
      } else if (g_optionsIndex == 3) {
        g_titleMode = TITLE_CHEATS;
        g_cheatsIndex = 0;
      }
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_B) {
      g_titleMode = TITLE_MAIN;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }
    break;
  }
  case TITLE_CHEATS: {
    constexpr int kCheatCount = 2; // moonjump, godmode
    if (g_pressed & VPAD_BUTTON_UP) {
      g_cheatsIndex = (g_cheatsIndex + kCheatCount - 1) % kCheatCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    } else if (g_pressed & VPAD_BUTTON_DOWN) {
      g_cheatsIndex = (g_cheatsIndex + 1) % kCheatCount;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_A) {
      if (g_cheatsIndex == 0) {
        g_cheatMoonJump = !g_cheatMoonJump;
      } else if (g_cheatsIndex == 1) {
        g_cheatGodMode = !g_cheatGodMode;
      }
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_B) {
      g_titleMode = TITLE_OPTIONS;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }
    break;
  }
  case TITLE_EXTRAS: {
    if (g_pressed & VPAD_BUTTON_B) {
      g_titleMode = TITLE_MAIN;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }
    break;
  }
  default:
    g_titleMode = TITLE_MAIN;
    break;
  }
}

void updatePauseMenu() {
  if (g_pressed & VPAD_BUTTON_UP) {
    g_pauseIndex = (g_pauseIndex + g_pauseOptionCount - 1) % g_pauseOptionCount;
    if (g_sfxMenuMove)
      Mix_PlayChannel(-1, g_sfxMenuMove, 0);
  } else if (g_pressed & VPAD_BUTTON_DOWN) {
    g_pauseIndex = (g_pauseIndex + 1) % g_pauseOptionCount;
    if (g_sfxMenuMove)
      Mix_PlayChannel(-1, g_sfxMenuMove, 0);
  }

  if (g_pressed & VPAD_BUTTON_A) {
    switch (g_pauseIndex) {
    case 0: // Resume
      g_state = GS_PLAYING;
      break;
    case 1: // Next level
      nextLevel();
      break;
    case 2: // Previous level
      if (g_levelIndex > 0)
        g_levelIndex--;
      else
        g_levelIndex = levelCount() - 1;
      g_sectionIndex = 0;
      setupLevel();
      break;
    case 3: // Cycle theme
      g_randomTheme = false;
      if (g_themeOverride < 0)
        g_themeOverride = 0;
      else
        g_themeOverride = (g_themeOverride + 1) % THEME_COUNT;
      g_theme = (LevelTheme)g_themeOverride;
      loadThemeTilesets();
      playThemeMusic(g_theme);
      break;
    case 4: // Main menu
      g_state = GS_TITLE;
      g_titleMode = TITLE_MAIN;
      g_menuIndex = g_charIndex;
      g_multiplayerActive = false;
      g_playerCount = 1;
      g_playerRemoteChan[0] = -1;
      for (int i = 1; i < 4; i++)
        g_playerRemoteChan[i] = -1;
      Mix_HaltMusic();
      break;
    default:
      break;
    }
  }
}

void spawnCoinPopup(float x, float y) {
  for (int i = 0; i < 64; i++) {
    if (!g_ents[i].on) {
      g_ents[i] = {true, E_COIN_POPUP, {x, y - 16, 16, 16}, 0, -200, 1, 0, 0};
      if (g_sfxCoin)
        Mix_PlayChannel(-1, g_sfxCoin, 0);
      break;
    }
  }
}

void spawnMushroom(int tx, int ty) {
  for (int i = 0; i < 64; i++) {
    if (!g_ents[i].on) {
      g_ents[i] = {
          true, E_MUSHROOM, {(float)tx * TILE, (float)(ty - 1) * TILE, 16, 16},
          48,   0,          1,
          0,    0};
      if (g_sfxItemAppear)
        Mix_PlayChannel(-1, g_sfxItemAppear, 0);
      break;
    }
  }
}

void spawnFireFlower(int tx, int ty) {
  for (int i = 0; i < 64; i++) {
    if (!g_ents[i].on) {
      g_ents[i] = {true,
                   E_FIRE_FLOWER,
                   {(float)tx * TILE, (float)(ty - 1) * TILE, 16, 16},
                   0,
                   0,
                   0,
                   0,
                   0};
      if (g_sfxItemAppear)
        Mix_PlayChannel(-1, g_sfxItemAppear, 0);
      break;
    }
  }
}

void spawnFireball(Player &p) {
  if (p.fireCooldown > 0.0f)
    return;
  for (int i = 0; i < 64; i++) {
    if (!g_ents[i].on) {
      float x = p.r.x + (p.right ? p.r.w - 4 : -4);
      float y = p.r.y + (p.r.h * 0.5f);
      g_ents[i] = {true, E_FIREBALL, {x, y, 16, 16}, 0, -90, p.right ? 1 : -1,
                   0,    0};
      p.fireCooldown = 0.35f;
      p.throwT = 0.15f;
      if (g_sfxFireball)
        Mix_PlayChannel(-1, g_sfxFireball, 0);
      break;
    }
  }
}

static int findFreeEntitySlot() {
  for (int i = 0; i < 64; i++) {
    if (!g_ents[i].on)
      return i;
  }
  return -1;
}

static bool isLiquidAt(int tx, int ty) {
  if (!g_levelInfo.atlasT || !g_levelInfo.atlasX || !g_levelInfo.atlasY)
    return false;
  if (tx < 0 || tx >= mapWidth() || ty < 0 || ty >= MAP_H)
    return false;
  uint8_t ax = g_levelInfo.atlasX[ty][tx];
  uint8_t ay = g_levelInfo.atlasY[ty][tx];
  if (ax == 255 || ay == 255)
    return false;
  return g_levelInfo.atlasT[ty][tx] == ATLAS_LIQUID;
}

static bool rectTouchesLiquid(const Rect &r) {
  // Underwater themes are "fully submerged" even if the tilemap doesn't
  // explicitly mark every cell as liquid.
  if (g_theme == THEME_UNDERWATER || g_theme == THEME_CASTLE_WATER)
    return true;

  int tx1 = (int)floorf(r.x / (float)TILE);
  int tx2 = (int)floorf((r.x + r.w - 1.0f) / (float)TILE);
  int ty1 = (int)floorf(r.y / (float)TILE);
  int ty2 = (int)floorf((r.y + r.h - 1.0f) / (float)TILE);
  for (int ty = ty1; ty <= ty2; ty++) {
    for (int tx = tx1; tx <= tx2; tx++) {
      if (isLiquidAt(tx, ty))
        return true;
    }
  }
  return false;
}

static float liquidSurfaceYAtWorldX(float worldX) {
  int tx = (int)floorf(worldX / (float)TILE);
  if (tx < 0)
    tx = 0;
  if (tx >= mapWidth())
    tx = mapWidth() - 1;

  // Find the topmost liquid tile in this column.
  for (int ty = 0; ty < MAP_H; ty++) {
    if (!isLiquidAt(tx, ty))
      continue;
    // Consider this the surface only if the tile above isn't liquid.
    if (ty == 0 || !isLiquidAt(tx, ty - 1))
      return (float)(ty * TILE);
  }

  // No liquid in this column; fall back to near-bottom.
  return (float)((MAP_H - 2) * TILE);
}

static void updatePlatformsAndGenerators(float dt) {
  // Update simple moving platforms first and carry players riding them.
  for (int i = 0; i < 64; i++) {
    Entity &e = g_ents[i];
    if (!e.on)
      continue;

    if (e.type != E_PLATFORM_SIDEWAYS && e.type != E_PLATFORM_VERTICAL &&
        e.type != E_PLATFORM_FALLING)
      continue;

    e.prevX = e.r.x;
    e.prevY = e.r.y;

    if (e.type == E_PLATFORM_SIDEWAYS) {
      constexpr float kTravel = 48.0f;
      constexpr float kDuration = 2.0f;
      constexpr float kSpeed = kTravel / kDuration;
      if (e.state == 0)
        e.state = -1; // first leg moves left
      e.r.x += (float)e.state * kSpeed * dt;
      if (e.r.x <= e.baseX - kTravel) {
        e.r.x = e.baseX - kTravel;
        e.state = 1;
      } else if (e.r.x >= e.baseX) {
        e.r.x = e.baseX;
        e.state = -1;
      }
    } else if (e.type == E_PLATFORM_VERTICAL) {
      if (e.dir == 0) {
        constexpr float kTravel = 128.0f;
        constexpr float kDuration = 3.0f;
        constexpr float kSpeed = kTravel / kDuration;
        if (e.state == 0)
          e.state = 1; // first leg moves down
        e.r.y += (float)e.state * kSpeed * dt;
        if (e.r.y >= e.baseY + kTravel) {
          e.r.y = e.baseY + kTravel;
          e.state = -1;
        } else if (e.r.y <= e.baseY) {
          e.r.y = e.baseY;
          e.state = 1;
        }
      } else {
        // Elevator wrap platforms: top/bottom stored in a/b, vertical dir in `dir`.
        const float topY = (float)e.a;
        const float bottomY = (float)e.b;
        constexpr float kSpeed = 50.0f;
        int vdir = (e.dir == 0) ? 1 : e.dir;
        e.r.y += (float)vdir * kSpeed * dt;
        if (vdir > 0 && e.r.y > bottomY)
          e.r.y = topY;
        if (vdir < 0 && e.r.y < topY)
          e.r.y = bottomY;
      }
    } else if (e.type == E_PLATFORM_FALLING) {
      // Falling platforms drop while a player is standing on them (Godot:
      // FallingPlatform.gd). They do not reset on their own.
      bool stood = false;
      for (int pi = 0; pi < g_playerCount; pi++) {
        const Player &pl = g_players[pi];
        if (pl.dead)
          continue;
        float bottom = pl.r.y + pl.r.h;
        if (fabsf(bottom - e.prevY) > 0.75f)
          continue;
        if (!pl.ground)
          continue;
        if (pl.r.x + pl.r.w <= e.prevX + 0.5f)
          continue;
        if (pl.r.x >= e.prevX + e.r.w - 0.5f)
          continue;
        // Require the player to be slightly above the platform top (matches
        // the upstream y-4 check) so side contacts don't trigger falling.
        if (pl.r.y - 4.0f > e.prevY)
          continue;
        stood = true;
        break;
      }
      if (stood) {
        constexpr float kFallSpeed = 96.0f;
        e.r.y += kFallSpeed * dt;
      }
    }

    float dx = e.r.x - e.prevX;
    float dy = e.r.y - e.prevY;
    if (dx == 0.0f && dy == 0.0f)
      continue;

    for (int pi = 0; pi < g_playerCount; pi++) {
      Player &pl = g_players[pi];
      if (pl.dead)
        continue;
      float bottom = pl.r.y + pl.r.h;
      if (fabsf(bottom - e.prevY) > 0.75f)
        continue;
      if (pl.r.x + pl.r.w <= e.prevX + 0.5f)
        continue;
      if (pl.r.x >= e.prevX + e.r.w - 0.5f)
        continue;
      pl.r.x += dx;
      pl.r.y += dy;
      if (dy < 0.0f && rectBlockedBySolids(pl.r)) {
        if (pi == 0) {
          pl.dead = true;
          pl.lives--;
          g_state = GS_DEAD;
          Mix_HaltMusic();
        } else {
          pl.vx = 0;
          pl.vy = 0;
          pl.r.x = fmaxf(0.0f, g_p.r.x - 24.0f - 12.0f * (pi - 1));
          pl.r.y = g_p.r.y;
          pl.invT = 1.5f;
        }
      }
    }
  }

  // Rope elevator platforms (paired). They are exported as E_PLATFORM_ROPE with:
  // - `dir`: pair id
  // - `a`: platform width
  // - `b`: rope_top (world y)
  for (int i = 0; i < 64; i++) {
    Entity &a = g_ents[i];
    if (!a.on || a.type != E_PLATFORM_ROPE)
      continue;
    // Find partner.
    int partnerIndex = -1;
    for (int j = i + 1; j < 64; j++) {
      if (g_ents[j].on && g_ents[j].type == E_PLATFORM_ROPE &&
          g_ents[j].dir == a.dir) {
        partnerIndex = j;
        break;
      }
    }
    if (partnerIndex < 0)
      continue;
    Entity &b = g_ents[partnerIndex];

    if (a.state != 0 || b.state != 0) {
      // Dropped: both platforms fall away.
      a.prevY = a.r.y;
      b.prevY = b.r.y;
      constexpr float kGravity = 300.0f;
      a.vy += kGravity * dt;
      b.vy += kGravity * dt;
      a.r.y += a.vy * dt;
      b.r.y += b.vy * dt;
      continue;
    }

    {
      a.prevY = a.r.y;
      b.prevY = b.r.y;

      bool stoodA = false;
      bool stoodB = false;
      for (int pi = 0; pi < g_playerCount; pi++) {
        const Player &pl = g_players[pi];
        if (pl.dead || !pl.ground)
          continue;
        float bottom = pl.r.y + pl.r.h;
        auto stoodOn = [&](const Entity &p) {
          if (fabsf(bottom - p.r.y) > 0.75f)
            return false;
          if (pl.r.x + pl.r.w <= p.r.x + 0.5f)
            return false;
          if (pl.r.x >= p.r.x + p.r.w - 0.5f)
            return false;
          return true;
        };
        stoodA |= stoodOn(a);
        stoodB |= stoodOn(b);
      }

      // If both are stood on (multiplayer), cancel out to keep things stable.
      int net = (stoodA ? 1 : 0) - (stoodB ? 1 : 0);
      if (net != 0) {
        a.vy += (float)net * 120.0f * dt;
      } else {
        float t = dt * 2.0f;
        if (t > 1.0f)
          t = 1.0f;
        a.vy = a.vy + (0.0f - a.vy) * t;
      }
      b.vy = -a.vy;

      a.r.y += a.vy * dt;
      b.r.y += b.vy * dt;

      float ropeTop = (float)a.b;
      if (a.r.y <= ropeTop || b.r.y <= ropeTop) {
        a.state = 1;
        b.state = 1;
        // Start falling from current velocities.
      }

      float dyA = a.r.y - a.prevY;
      float dyB = b.r.y - b.prevY;
      if (dyA != 0.0f) {
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead)
            continue;
          float bottom = pl.r.y + pl.r.h;
          if (fabsf(bottom - a.prevY) > 0.75f)
            continue;
          if (pl.r.x + pl.r.w <= a.r.x + 0.5f)
            continue;
          if (pl.r.x >= a.r.x + a.r.w - 0.5f)
            continue;
          pl.r.y += dyA;
        }
      }
      if (dyB != 0.0f) {
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead)
            continue;
          float bottom = pl.r.y + pl.r.h;
          if (fabsf(bottom - b.prevY) > 0.75f)
            continue;
          if (pl.r.x + pl.r.w <= b.r.x + 0.5f)
            continue;
          if (pl.r.x >= b.r.x + b.r.w - 0.5f)
            continue;
          pl.r.y += dyB;
        }
      }
    }
  }

  // Entity generators / stoppers.
  for (int i = 0; i < 64; i++) {
    Entity &g = g_ents[i];
    if (!g.on)
      continue;
    if (g.type != E_ENTITY_GENERATOR && g.type != E_ENTITY_GENERATOR_STOP)
      continue;

    // Generators are driven by Player 1 to match the upstream "player detection"
    // trigger. Use a crossing test instead of `playerX >= baseX`, otherwise
    // "stopper" nodes would immediately re-activate generators behind the
    // player (Godot only triggers on area enter).
    float playerX = g_p.r.x + g_p.r.w * 0.5f;
    float prevPlayerX = g.prevX;
    g.prevX = playerX;
    bool crossed = (prevPlayerX < g.baseX - 8.0f) && (playerX >= g.baseX - 8.0f);
    if (g.type == E_ENTITY_GENERATOR_STOP) {
      if (g.state == 0 && crossed) {
        g.state = 1;
        // Deactivate all generators, but allow those further right to be
        // activated again when the player reaches them (matches Godot's
        // `deactivate_all_generators()` behavior).
        for (int j = 0; j < 64; j++) {
          if (!g_ents[j].on || g_ents[j].type != E_ENTITY_GENERATOR)
            continue;
          g_ents[j].state = 0;
          g_ents[j].timer = 0.0f;
          g_ents[j].prevX = playerX;
        }
      }
      continue;
    }

    if (g.state == 0) {
      if (!crossed)
        continue;
      g.state = 1;
      g.timer = 0.0f;
    }

    // Active generator: spawn immediately on activation, then periodically.
    float threshold = (g.b > 0) ? ((float)g.b / 1000.0f) : 2.0f;
    if (g.timer == 0.0f) {
      // First spawn.
      int slot = findFreeEntitySlot();
      if (slot >= 0) {
        Entity &e = g_ents[slot];
        e = {};
        e.on = true;
        e.type = (EType)g.a;
        e.state = 0;
        e.timer = 0.0f;
        e.vx = 0.0f;
        e.vy = 0.0f;
        e.baseX = 0.0f;
        e.baseY = 0.0f;
        e.prevX = 0.0f;
        e.prevY = 0.0f;

        if (e.type == E_BULLET_BILL) {
          float y = g_p.r.y + (float)((rand() % 9) - 4);
          if (y < 0)
            y = 0;
          if (y > GAME_H - 16)
            y = GAME_H - 16;
          e.r = {g_camX + GAME_W + 8.0f, y, 16.0f, 16.0f};
          e.dir = -1;
          e.vx = 90.0f;
        } else if (e.type == E_CHEEP_LEAP) {
          // Spawn within/around the current camera view so leaping fish can
          // appear both in front of and behind the player (classic "fish
          // jumping all around the screen" feel).
          int span = GAME_W + 64; // -32..(GAME_W+31)
          float x = g_camX + (float)((rand() % span) - 32);
          float surface = liquidSurfaceYAtWorldX(x);
          e.r = {x, surface - 16.0f, 16.0f, 16.0f};
          e.dir = (rand() & 1) ? -1 : 1;
          e.vx = (50.0f + (float)(rand() % 151)) * (float)e.dir;
          e.vy = -(250.0f + (float)(rand() % 101));
          e.b = (int)surface;
        } else {
          e.r = {g.baseX, g.baseY, 16.0f, 16.0f};
        }
      }
      // Stagger subsequent spawns like the upstream randf_range(-2, 0).
      g.timer = -((float)(rand() % 2000) / 1000.0f);
    } else if (g.timer >= threshold) {
      int slot = findFreeEntitySlot();
      if (slot >= 0) {
        Entity &e = g_ents[slot];
        e = {};
        e.on = true;
        e.type = (EType)g.a;
        e.state = 0;
        e.timer = 0.0f;
        if (e.type == E_BULLET_BILL) {
          float y = g_p.r.y + (float)((rand() % 9) - 4);
          if (y < 0)
            y = 0;
          if (y > GAME_H - 16)
            y = GAME_H - 16;
          e.r = {g_camX + GAME_W + 8.0f, y, 16.0f, 16.0f};
          e.dir = -1;
          e.vx = 90.0f;
        } else if (e.type == E_CHEEP_LEAP) {
          int span = GAME_W + 64;
          float x = g_camX + (float)((rand() % span) - 32);
          float surface = liquidSurfaceYAtWorldX(x);
          e.r = {x, surface - 16.0f, 16.0f, 16.0f};
          e.dir = (rand() & 1) ? -1 : 1;
          e.vx = (50.0f + (float)(rand() % 151)) * (float)e.dir;
          e.vy = -(250.0f + (float)(rand() % 101));
          e.b = (int)surface;
        } else {
          e.r = {g.baseX, g.baseY, 16.0f, 16.0f};
        }
      }
      g.timer = -((float)(rand() % 2000) / 1000.0f);
    }
  }
}

bool tryPipeEnter(const PipeLink &p) {
  if (g_sfxPipe)
    Mix_PlayChannel(-1, g_sfxPipe, 0);
  g_levelIndex = p.targetLevel;
  g_sectionIndex = p.targetSection;
  if (!loadLevelSection(g_levelIndex, g_sectionIndex, g_map, g_levelInfo))
    return false;
  applySection(false, p.targetX, p.targetY);
  return true;
}

static void updateCameraFromLeader() {
  // Center-follow camera. With backtracking disabled (classic SMB1), the camera
  // only moves forward; it starts moving once the player reaches mid-screen.
  float leaderMid = g_p.r.x + g_p.r.w * 0.5f;
  float target = leaderMid - (GAME_W * 0.5f);

  if (cameraBacktrackEnabled()) {
    g_camX = target;
  } else {
    if (target > g_camX)
      g_camX = target;
  }

  int viewTiles = (GAME_W + TILE - 1) / TILE;
  float maxCam = (mapWidth() - viewTiles) * TILE;
  if (maxCam < 0)
    maxCam = 0;
  if (g_camX < 0)
    g_camX = 0;
  if (g_camX > maxCam)
    g_camX = maxCam;
}

static void enforceNonLeaderPlayersInView() {
  if (g_playerCount <= 1)
    return;

  float left = g_camX;
  float right = g_camX + GAME_W;

  for (int i = 1; i < g_playerCount; i++) {
    Player &pl = g_players[i];
    if (pl.dead)
      continue;

    float oldX = pl.r.x;
    float minX = left;
    float maxX = right - pl.r.w;
    if (maxX < minX)
      maxX = minX;
    if (pl.r.x < minX)
      pl.r.x = minX;
    if (pl.r.x > maxX)
      pl.r.x = maxX;

    if (pl.r.x != oldX) {
      // If the camera pushes a player into a solid tile, treat it as a crush.
      if (rectBlockedBySolids(pl.r)) {
        pl.lives--;
        if (pl.lives <= 0) {
          pl.dead = true;
          continue;
        }
        pl.vx = 0.0f;
        pl.vy = 0.0f;
        pl.invT = 2.0f;
        pl.throwT = 0.0f;
        pl.fireCooldown = 0.0f;
        pl.swimCooldown = 0.0f;
        pl.swimAnimT = 9999.0f;
        pl.r.x = fmaxf(0.0f, g_p.r.x - 24.0f - 12.0f * (i - 1));
        pl.r.y = g_p.r.y;
      }
    }
  }
}

static void updateOnePlayer(int playerIndex, float dt) {
  Player &pl = g_players[playerIndex];
  if (pl.dead || g_state == GS_FLAG)
    return;

  uint32_t held = g_playerHeld[playerIndex];
  uint32_t pressed = g_playerPressed[playerIndex];

  if (pl.invT > 0)
    pl.invT -= dt;
  if (pl.throwT > 0.0f) {
    pl.throwT -= dt;
    if (pl.throwT < 0.0f)
      pl.throwT = 0.0f;
  }
  if (pl.fireCooldown > 0.0f) {
    pl.fireCooldown -= dt;
    if (pl.fireCooldown < 0.0f)
      pl.fireCooldown = 0.0f;
  }
  if (pl.swimCooldown > 0.0f) {
    pl.swimCooldown -= dt;
    if (pl.swimCooldown < 0.0f)
      pl.swimCooldown = 0.0f;
  }
  pl.animT += dt;

  bool inWater = rectTouchesLiquid(pl.r);
  if (inWater) {
    if (pl.swimAnimT < 9000.0f)
      pl.swimAnimT += dt;
  } else {
    pl.swimAnimT = 9999.0f;
  }

  float dir = 0;
  bool downHeld = (held & VPAD_BUTTON_DOWN);
  if (held & VPAD_BUTTON_RIGHT) {
    dir = 1;
    pl.right = true;
  } else if (held & VPAD_BUTTON_LEFT) {
    dir = -1;
    pl.right = false;
  }

  if (pl.power == P_FIRE && firePressed(pressed) && !pl.crouch) {
    spawnFireball(pl);
  }

  // Pipes are driven by Player 1 to avoid splitting sections/camera.
  if (playerIndex == 0 && g_levelInfo.pipes && g_levelInfo.pipeCount > 0) {
    for (int i = 0; i < g_levelInfo.pipeCount; i++) {
      const PipeLink &pipe = g_levelInfo.pipes[i];
      float px = pipe.x * TILE;
      float py = pipe.y * TILE;
      SDL_FRect mouth = {px, py, TILE * 2.0f, TILE * 2.0f};

      float midX = pl.r.x + pl.r.w * 0.5f;
      float midY = pl.r.y + pl.r.h * 0.5f;
      bool overlapY =
          (midY >= mouth.y - 8.0f && midY <= mouth.y + mouth.h + 8.0f);
      bool overMouthX = (midX >= mouth.x + 2 && midX <= mouth.x + mouth.w - 2);
      constexpr float kEdgeEps = 6.0f;

      switch (pipe.enterDir) {
      case 0: // Down
        if (downHeld && pl.ground && overMouthX &&
            fabsf((pl.r.y + pl.r.h) - mouth.y) <= 8.0f) {
          if (tryPipeEnter(pipe))
            return;
        }
        break;
      case 1: // Up
        if ((held & VPAD_BUTTON_UP) && overMouthX &&
            fabsf(pl.r.y - (mouth.y + mouth.h)) <= 8.0f) {
          if (tryPipeEnter(pipe))
            return;
        }
        break;
      case 2: // Left
        if ((held & VPAD_BUTTON_LEFT) && pl.ground && overlapY &&
            fabsf(pl.r.x - (mouth.x + mouth.w)) <= kEdgeEps) {
          if (tryPipeEnter(pipe))
            return;
        }
        break;
      case 3: // Right
        if ((held & VPAD_BUTTON_RIGHT) && pl.ground && overlapY &&
            fabsf((pl.r.x + pl.r.w) - mouth.x) <= kEdgeEps) {
          if (tryPipeEnter(pipe))
            return;
        }
        break;
      default:
        break;
      }
    }
  }

  // Crouch is controlled by DOWN and can persist into the air (so crouch-jump
  // keeps the crouch sprite + reduced hitbox).
  bool wantsCrouch = (pl.power >= P_BIG) && downHeld;
  if (wantsCrouch && !pl.crouch) {
    pl.crouch = true;
    setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
  } else if (!wantsCrouch && pl.crouch) {
    Rect standRect = pl.r;
    standRect.h = standHeightForPower(pl.power);
    standRect.y = (pl.r.y + pl.r.h) - standRect.h;
    standRect.w = PLAYER_HIT_W_BIG;
    if (!rectBlockedBySolids(standRect)) {
      pl.crouch = false;
      setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_BIG, standRect.h);
    }
  }

  // While crouched, don't allow steering; we want a slide.
  if (pl.crouch)
    dir = 0;

  float maxSpd = (held & VPAD_BUTTON_Y) ? Physics::RUN_SPEED : Physics::WALK_SPEED;
  float accel = pl.ground ? Physics::GROUND_ACCEL : Physics::AIR_ACCEL;
  float decel = pl.ground ? Physics::DECEL : Physics::AIR_ACCEL;
  if (inWater) {
    // Underwater motion is slower and floatier.
    maxSpd *= 0.70f;
    accel *= 0.65f;
    decel *= 0.70f;
  }
  if (pl.crouch && pl.ground) {
    // Duck-slide: preserve horizontal speed longer than normal friction.
    float slideDecel = Physics::DECEL * 0.70f;
    if (pl.vx > 0)
      pl.vx = fmaxf(0.0f, pl.vx - slideDecel);
    if (pl.vx < 0)
      pl.vx = fminf(0.0f, pl.vx + slideDecel);
  } else if (dir != 0) {
    // Avoid runaway acceleration: accelerate only until reaching the active
    // cap, and if we're over the cap (e.g. releasing RUN), decelerate down.
    float v = pl.vx;
    float s = (dir > 0) ? 1.0f : -1.0f;
    float speedAlongDir = v * s;
    if (speedAlongDir > maxSpd) {
      speedAlongDir = fmaxf(maxSpd, speedAlongDir - decel);
      pl.vx = speedAlongDir * s;
    } else {
      pl.vx += dir * accel;
      if (pl.vx > maxSpd)
        pl.vx = maxSpd;
      if (pl.vx < -maxSpd)
        pl.vx = -maxSpd;
    }
  } else {
    if (pl.vx > 0)
      pl.vx = fmaxf(0.0f, pl.vx - decel);
    if (pl.vx < 0)
      pl.vx = fminf(0.0f, pl.vx + decel);
  }

  if (inWater && jumpPressed(pressed) && pl.swimCooldown <= 0.0f) {
    // Swim stroke (works both in-air and from the floor).
    pl.vy = -130.0f;
    pl.jumping = true;
    pl.ground = false;
    pl.swimCooldown = 0.28f;
    pl.swimAnimT = 0.0f;
    if (g_sfxJump)
      Mix_PlayChannel(-1, g_sfxJump, 0);
  } else if (g_cheatMoonJump && jumpPressed(pressed) && !pl.ground && !inWater) {
    // Moonjump cheat: allow mid-air jumps (useful for testing unimplemented
    // sections). This intentionally does not bypass pit death.
    pl.vy = -Physics::JUMP_HEIGHT;
    pl.jumping = true;
    pl.ground = false;
    if (g_sfxJump)
      Mix_PlayChannel(-1, g_sfxJump, 0);
  } else if (jumpPressed(pressed) && pl.ground) {
    float jumpHeight = Physics::JUMP_HEIGHT;
    if (fabsf(pl.vx) > Physics::WALK_SPEED * 0.9f)
      jumpHeight *= 1.15f;
    pl.vy = -jumpHeight;
    pl.jumping = true;
    pl.ground = false;
    if (pl.power >= P_BIG) {
      if (g_sfxBigJump)
        Mix_PlayChannel(-1, g_sfxBigJump, 0);
      else if (g_sfxJump)
        Mix_PlayChannel(-1, g_sfxJump, 0);
    } else {
      if (g_sfxJump)
        Mix_PlayChannel(-1, g_sfxJump, 0);
    }
  }
  if (!jumpHeld(held) && pl.vy < (inWater ? -80.0f : -100.0f))
    pl.vy = inWater ? -80.0f : -100.0f;

  float jumpG = Physics::JUMP_GRAVITY;
  float fallG = Physics::FALL_GRAVITY;
  float maxFall = Physics::MAX_FALL_SPEED;
  if (inWater) {
    jumpG *= 0.25f;
    fallG *= 0.25f;
    maxFall *= 0.35f;
  }
  pl.vy += (pl.vy < 0 && pl.jumping) ? jumpG : fallG;
  if (pl.vy > maxFall)
    pl.vy = maxFall;

  float moveX = pl.vx * dt;
  int stepsX = (int)ceilf(fabsf(moveX) / 6.0f);
  if (stepsX < 1)
    stepsX = 1;
  float stepX = moveX / stepsX;
  float minWorldX = cameraBacktrackEnabled() ? 0.0f : g_camX;
  for (int i = 0; i < stepsX; i++) {
    pl.r.x += stepX;
    if (pl.r.x < minWorldX)
      pl.r.x = minWorldX;

    int ty1 = (int)(pl.r.y / TILE);
    int ty2 = (int)((pl.r.y + pl.r.h - 1) / TILE);
    int tx1 = (int)(pl.r.x / TILE);
    int tx2 = (int)((pl.r.x + pl.r.w) / TILE);
    bool hit = false;
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        if (solidAt(tx, ty)) {
          if (stepX > 0)
            pl.r.x = tx * TILE - pl.r.w;
          else if (stepX < 0)
            pl.r.x = (tx + 1) * TILE;
          pl.vx = 0;
          hit = true;
          break;
        }
      }
      if (hit)
        break;
    }
    if (hit)
      break;
  }

  float moveY = pl.vy * dt;
  int stepsY = (int)ceilf(fabsf(moveY) / 6.0f);
  if (stepsY < 1)
    stepsY = 1;
  float stepY = moveY / stepsY;
  pl.ground = false;
  for (int i = 0; i < stepsY; i++) {
    pl.r.y += stepY;
    int ty1 = (int)(pl.r.y / TILE);
    int ty2 = (stepY >= 0) ? (int)((pl.r.y + pl.r.h) / TILE)
                           : (int)((pl.r.y + pl.r.h - 1) / TILE);
    int tx1 = (int)(pl.r.x / TILE);
    int tx2 = (int)((pl.r.x + pl.r.w - 1) / TILE);
    bool hit = false;
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        uint8_t col = collisionAt(tx, ty);
        if (col == COL_NONE)
          continue;
        if (pl.vy > 0) {
          if (col == COL_ONEWAY) {
            float prevBottom = (pl.r.y - stepY) + pl.r.h;
            float tileTop = ty * TILE;
            if (prevBottom > tileTop + 0.1f)
              continue;
          }
          pl.r.y = ty * TILE - pl.r.h;
          pl.vy = 0;
          pl.ground = true;
          pl.jumping = false;
          hit = true;
        } else if (pl.vy < 0) {
          if (col != COL_SOLID)
            continue;
          int hitTy = ty;
          pl.r.y = (hitTy + 1) * TILE;
          pl.vy = 45;

          auto hitBlockAt = [&](int bx, int by) {
            if (bx < 0 || bx >= mapWidth() || by < 0 || by >= MAP_H)
              return;
            if (collisionAt(bx, by) != COL_SOLID)
              return;
            uint8_t t = g_map[by][bx];
            if (t == T_QUESTION) {
              g_map[by][bx] = T_USED;
              uint8_t meta = questionMetaAt(bx, by);
              if (meta == QMETA_POWERUP || meta == QMETA_STAR) {
                if (pl.power == P_SMALL)
                  spawnMushroom(bx, by);
                else
                  spawnFireFlower(bx, by);
              } else if (meta == QMETA_ONEUP) {
                pl.lives++;
                pl.score += 1000;
                if (g_sfxPowerup)
                  Mix_PlayChannel(-1, g_sfxPowerup, 0);
              } else {
                pl.coins++;
                pl.score += 200;
                spawnCoinPopup(bx * TILE, by * TILE);
              }
              if (g_sfxBump)
                Mix_PlayChannel(-1, g_sfxBump, 0);
              return;
            }
            if (t == T_BRICK && pl.power > P_SMALL) {
              g_map[by][bx] = T_EMPTY;
              pl.score += 50;
              if (g_sfxBreak)
                Mix_PlayChannel(-1, g_sfxBreak, 0);
              return;
            }
            if (t == T_BRICK) {
              if (g_sfxBump)
                Mix_PlayChannel(-1, g_sfxBump, 0);
              addTileBump(bx, by);
              return;
            }
          };

          int leftTx = (int)((pl.r.x + 1) / TILE);
          int rightTx = (int)((pl.r.x + pl.r.w - 2) / TILE);
          float midX = pl.r.x + pl.r.w * 0.5f;
          float seamX = (leftTx + 1) * (float)TILE;
          bool spansTwo = rightTx > leftTx;
          bool inSeam = spansTwo && fabsf(midX - seamX) <= 2.0f;
          if (inSeam) {
            hitBlockAt(leftTx, hitTy);
            hitBlockAt(rightTx, hitTy);
          } else if (!spansTwo) {
            hitBlockAt(leftTx, hitTy);
          } else {
            int pick = (midX < seamX) ? leftTx : rightTx;
            hitBlockAt(pick, hitTy);
          }
          hit = true;
        }
        if (hit)
          break;
      }
      if (hit)
        break;
    }

    // Moving platforms: allow landing from above (one-way).
    if (!hit && stepY > 0) {
      float prevBottom = (pl.r.y - stepY) + pl.r.h;
      float newBottom = pl.r.y + pl.r.h;
      for (int ei = 0; ei < 64; ei++) {
        const Entity &pf = g_ents[ei];
        if (!pf.on)
          continue;
        if (pf.type != E_PLATFORM_SIDEWAYS && pf.type != E_PLATFORM_VERTICAL &&
            pf.type != E_PLATFORM_ROPE && pf.type != E_PLATFORM_FALLING)
          continue;
        float platTop = pf.r.y;
        if (prevBottom > platTop + 0.1f)
          continue;
        if (newBottom < platTop - 0.1f)
          continue;
        if (pl.r.x + pl.r.w <= pf.r.x + 0.5f)
          continue;
        if (pl.r.x >= pf.r.x + pf.r.w - 0.5f)
          continue;
        pl.r.y = platTop - pl.r.h;
        pl.vy = 0;
        pl.ground = true;
        pl.jumping = false;
        hit = true;
        break;
      }
    }
    if (hit)
      break;
  }

  // Collect coins embedded in the tilemap.
  {
    int tx1 = (int)(pl.r.x / TILE);
    int tx2 = (int)((pl.r.x + pl.r.w - 1) / TILE);
    int ty1 = (int)(pl.r.y / TILE);
    int ty2 = (int)((pl.r.y + pl.r.h - 1) / TILE);
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        if (tx < 0 || tx >= mapWidth() || ty < 0 || ty >= MAP_H)
          continue;
        if (g_map[ty][tx] == T_COIN) {
          g_map[ty][tx] = T_EMPTY;
          pl.coins++;
          pl.score += 200;
          if (g_sfxCoin)
            Mix_PlayChannel(-1, g_sfxCoin, 0);
        }
      }
    }
  }

  // Flag pole collision (Player 1 only).
  if (playerIndex == 0) {
    int flagTx = (int)((pl.r.x + pl.r.w / 2) / TILE);
    if (g_hasFlag && flagTx == g_flagX && g_state == GS_PLAYING) {
      g_state = GS_FLAG;
      pl.vx = 0;
      pl.vy = 0;
      int height = 12 - (int)(pl.r.y / TILE);
      pl.score += height * 100;
      if (!g_flagSfxPlayed && g_sfxFlagSlide) {
        Mix_PlayChannel(-1, g_sfxFlagSlide, 0);
        g_flagSfxPlayed = true;
      }
      Mix_HaltMusic();
    }
  }

  // Pit death. Player 1 behaves like classic SMB; helpers respawn near P1.
  if (pl.r.y > GAME_H + 32) {
    if (playerIndex == 0) {
      pl.dead = true;
      pl.lives--;
      g_state = GS_DEAD;
      Mix_HaltMusic();
    } else {
      pl.vx = 0;
      pl.vy = 0;
      pl.r.x = fmaxf(0.0f, g_p.r.x - 24.0f - 12.0f * (playerIndex - 1));
      pl.r.y = g_p.r.y;
      pl.invT = 1.5f;
    }
  }
}

void updatePlayers(float dt) {
  for (auto &b : g_tileBumps) {
    if (b.t > 0.0f) {
      b.t -= dt;
      if (b.t < 0.0f)
        b.t = 0.0f;
    }
  }
  if (g_skidCooldown > 0)
    g_skidCooldown -= dt;

  for (int i = 0; i < g_playerCount; i++)
    updateOnePlayer(i, dt);

  updateCameraFromLeader();
  enforceNonLeaderPlayersInView();
}

void updateFlagSequence(float dt) {
  if (!g_flagSfxPlayed && g_sfxFlagSlide) {
    Mix_PlayChannel(-1, g_sfxFlagSlide, 0);
    g_flagSfxPlayed = true;
  }
  g_p.animT += dt;
  // Slide player down flag
  if (g_p.r.y < 12 * TILE - g_p.r.h)
    g_p.r.y += 100 * dt;
  else {
    // Walk to castle
    g_p.vx = 50;
    g_p.ground = true;
    g_p.r.x += 50 * dt;
    g_p.right = true;

    // Switch levels once the player reaches the "front overlay" portion of the
    // castle (the piece that hides the player entering the door), rather than
    // relying on hardcoded X thresholds or nearby ground tiles.
    bool atCastle = false;
    SDL_Rect mainDst, overlayDst;
    if (computeCastleDst(mainDst, overlayDst) && overlayDst.w > 0 &&
        overlayDst.h > 0) {
      Rect overlayWorld = {(float)(overlayDst.x + g_camX), (float)overlayDst.y,
                           (float)overlayDst.w, (float)overlayDst.h};
      // Don't end the level the moment we touch the overlay: let the player
      // walk a bit further so they get properly hidden by the castle front.
      Rect trigger = overlayWorld;
      trigger.x += 2 * TILE;
      trigger.w -= 2 * TILE;
      if (trigger.w < 1)
        trigger = overlayWorld;
      atCastle = overlap(g_p.r, trigger);
    } else {
      // Fallback if no castle markers exist in this section.
      atCastle = (g_p.r.x > (mapWidth() - 2) * TILE);
    }

    if (atCastle) {
      g_state = GS_WIN;
      g_p.score += g_time * 50;
      g_levelTimer = 0.0f;
      if (!g_castleSfxPlayed && g_sfxCastleClear) {
        Mix_PlayChannel(-1, g_sfxCastleClear, 0);
        g_castleSfxPlayed = true;
      }
    }
  }
  // Flag slides down
  if (g_flagY < 12 * TILE)
    g_flagY += 100 * dt;
}

void updateEntities(float dt) {
  for (int i = 0; i < 64; i++) {
    Entity &e = g_ents[i];
    if (!e.on)
      continue;
    e.timer += dt;

    if (e.type == E_GOOMBA) {
      if (e.state == 0) {
        e.r.x += e.dir * Physics::ENEMY_SPEED * dt;
        e.vy += 15.0f;
        e.r.y += e.vy * dt;

        int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
        int bottomTy = (int)((e.r.y + e.r.h) / TILE);
        {
          uint8_t col = collisionAt(midTx, bottomTy);
          if (col == COL_SOLID || col == COL_ONEWAY) {
            if (col == COL_ONEWAY) {
              float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
              float tileTop = bottomTy * TILE;
              if (prevBottom > tileTop + 0.1f)
                col = COL_NONE;
            }
            if (col != COL_NONE) {
              e.r.y = bottomTy * TILE - e.r.h;
              e.vy = 0;
            }
          }
        }

        int frontTx =
            e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
        int frontTy = (int)((e.r.y + e.r.h - 1) / TILE);
        if (solidAt(frontTx, frontTy))
          e.dir = -e.dir;

        if (e.r.x < g_camX - 64 || e.r.y > GAME_H + 32) {
          e.on = false;
          continue;
        }

        if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
          Rect er = enemyHitRect(e);
          for (int pi = 0; pi < g_playerCount; pi++) {
            Player &pl = g_players[pi];
            if (pl.dead || !overlap(pl.r, er))
              continue;

            if (playerStomp(pl)) {
              e.state = 1;
              e.timer = 0;
              pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                                 : -Physics::BOUNCE_HEIGHT;
              pl.score += 100;
              if (g_sfxStomp)
                Mix_PlayChannel(-1, g_sfxStomp, 0);
              else if (g_sfxKick)
                Mix_PlayChannel(-1, g_sfxKick, 0);
            } else if (!playerIsInvulnerable(pl)) {
              if (pl.power > P_SMALL) {
                pl.power = P_SMALL;
                pl.crouch = false;
                setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
                pl.invT = 2.0f;
                if (g_sfxDamage)
                  Mix_PlayChannel(-1, g_sfxDamage, 0);
              } else if (pi == 0) {
                pl.dead = true;
                pl.lives--;
                g_state = GS_DEAD;
                Mix_HaltMusic();
              } else {
                // Helpers don't end the run; just grant brief i-frames.
                pl.invT = 2.0f;
                pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
              }
            }
            break;
          }
        }
      } else if (e.timer > 0.5f)
        e.on = false;
    } else if (e.type == E_KOOPA || e.type == E_KOOPA_RED) {
      float moveSpeed = 0.0f;
      if (e.state == 0)
        moveSpeed = Physics::ENEMY_SPEED;
      else if (e.state == 2)
        moveSpeed = Physics::RUN_SPEED;

      e.r.x += e.dir * moveSpeed * dt;
      e.vy += 15.0f;
      e.r.y += e.vy * dt;

      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int bottomTy = (int)((e.r.y + e.r.h) / TILE);
      {
        uint8_t col = collisionAt(midTx, bottomTy);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = bottomTy * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = bottomTy * TILE - e.r.h;
            e.vy = 0;
          }
        }
      }

      int frontTx =
          e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      int frontTy = (int)((e.r.y + e.r.h - 1) / TILE);
      if (solidAt(frontTx, frontTy))
        e.dir = -e.dir;

      if (e.r.x < g_camX - 64 || e.r.y > GAME_H + 32) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;

          bool stomp = playerStomp(pl);
          if (stomp) {
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
            else if (g_sfxKick)
              Mix_PlayChannel(-1, g_sfxKick, 0);

            if (e.state == 0) {
              e.state = 1;
              e.timer = 0;
              e.r.y += 8;
              e.r.h = 16;
              e.dir = 0;
            } else if (e.state == 2) {
              e.state = 1;
              e.timer = 0;
              e.dir = 0;
            }
          } else if (!playerIsInvulnerable(pl)) {
            if (e.state == 1) {
              e.state = 2;
              e.timer = 0;
              e.dir = (pl.r.x < e.r.x) ? 1 : -1;
              if (g_sfxKick)
                Mix_PlayChannel(-1, g_sfxKick, 0);
            } else if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_BUZZY_BEETLE) {
      // Buzzy Beetle: Koopa-like behavior, but uses a 16x16 body. State:
      // 0=walk, 1=shell idle, 2=shell moving.
      float moveSpeed = 0.0f;
      if (e.state == 0)
        moveSpeed = Physics::ENEMY_SPEED;
      else if (e.state == 2)
        moveSpeed = Physics::RUN_SPEED;

      e.r.x += e.dir * moveSpeed * dt;
      e.vy += 15.0f;
      e.r.y += e.vy * dt;

      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int bottomTy = (int)((e.r.y + e.r.h) / TILE);
      {
        uint8_t col = collisionAt(midTx, bottomTy);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = bottomTy * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = bottomTy * TILE - e.r.h;
            e.vy = 0;
          }
        }
      }

      int frontTx =
          e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      int frontTy = (int)((e.r.y + e.r.h - 1) / TILE);
      if (solidAt(frontTx, frontTy))
        e.dir = -e.dir;

      if (e.r.x < g_camX - 64 || e.r.y > GAME_H + 32) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;

          bool stomp = playerStomp(pl);
          if (stomp) {
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);

            if (e.state == 0) {
              e.state = 1;
              e.timer = 0.0f;
              e.dir = 0;
            } else if (e.state == 2) {
              e.state = 1;
              e.timer = 0.0f;
              e.dir = 0;
            }
          } else if (!playerIsInvulnerable(pl)) {
            if (e.state == 1) {
              e.state = 2;
              e.timer = 0.0f;
              e.dir = (pl.r.x < e.r.x) ? 1 : -1;
              if (g_sfxKick)
                Mix_PlayChannel(-1, g_sfxKick, 0);
            } else if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_BULLET_CANNON) {
      // BulletBillCannon (Godot: BulletBillCannon.gd): periodic emitter with a
      // limit on active bullets.
      if (e.a <= 0)
        e.a = 15; // countdown "ticks"
      if ((rand() % 9) == 8)
        e.a -= 1;
      if (e.a > 0)
        continue;

      // Reset timer (hard-mode cadence isn't modeled here).
      e.a = 15;

      // Only shoot when player isn't inside the cannon's detect box.
      if (fabsf((g_p.r.x + g_p.r.w * 0.5f) - e.baseX) < 24.0f &&
          fabsf((g_p.r.y + g_p.r.h * 0.5f) - e.baseY) < 24.0f) {
        continue;
      }

      // Keep at most 3 active bullet bills.
      int activeBills = 0;
      for (int j = 0; j < 64; j++) {
        if (g_ents[j].on && g_ents[j].type == E_BULLET_BILL)
          activeBills++;
      }
      if (activeBills >= 3)
        continue;

      int dir = (int)copysignf(1.0f, (g_p.r.x + g_p.r.w * 0.5f) - e.baseX);
      if (dir == 0)
        dir = 1;

      // Simple block check: don't shoot if a solid tile is directly in front.
      int tx = (int)(e.baseX / TILE);
      int ty = (int)((e.baseY + 8.0f) / TILE);
      if (solidAt(tx + dir, ty))
        continue;

      int slot = findFreeEntitySlot();
      if (slot >= 0) {
        Entity &b = g_ents[slot];
        b = {};
        b.on = true;
        b.type = E_BULLET_BILL;
        b.dir = dir;
        b.vx = 100.0f * (float)dir;
        b.r = {e.baseX + (float)(8 * dir), e.baseY + 8.0f, 16.0f, 16.0f};
      }
    } else if (e.type == E_HAMMER_BRO) {
      // Hammer Bro: patrols a bit, jumps between nearby platforms, and throws hammers.
      // State: 0=idle/walk anim, 1=throw anim (short).
      if (e.dir == 0)
        e.dir = (rand() & 1) ? 1 : -1;
      if (e.vx == 0.0f)
        e.vx = 18.0f + (float)(rand() % 10);

      // Gentle horizontal drift so it can walk off / reach ledges.
      e.r.x += (float)e.dir * e.vx * dt;

      // Bounce off walls.
      int midTy = (int)((e.r.y + e.r.h * 0.5f) / TILE);
      int frontTx =
          (e.dir > 0) ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      if (solidAt(frontTx, midTy)) {
        e.dir = -e.dir;
        e.r.x += (float)e.dir * e.vx * dt;
      }

      e.vy += 15.0f;
      e.r.y += e.vy * dt;
      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int bottomTy = (int)((e.r.y + e.r.h) / TILE);
      uint8_t landedOn = COL_NONE;
      {
        uint8_t col = collisionAt(midTx, bottomTy);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = bottomTy * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = bottomTy * TILE - e.r.h;
            e.vy = 0.0f;
            landedOn = col;
          }
        }
      }

      // Occasional jump when on "ground".
      bool onGround = (e.vy == 0.0f);
      if (onGround) {
        // Edge behavior: usually reverse at the edge, but sometimes keep going so the
        // bro can fall down to a lower platform (requested "jump down" behavior).
        int footTy = (int)((e.r.y + e.r.h + 1.0f) / TILE);
        int aheadTx =
            (e.dir > 0) ? (int)((e.r.x + e.r.w + 1.0f) / TILE) : (int)((e.r.x - 1.0f) / TILE);
        uint8_t under = collisionAt(aheadTx, footTy);
        if (under == COL_NONE) {
          if ((rand() % 5) != 0) {
            e.dir = -e.dir;
          }
        }

        // Small horizontal randomness while grounded.
        if ((rand() % 240) == 0) {
          e.dir = (rand() & 1) ? 1 : -1;
          e.vx = 16.0f + (float)(rand() % 14);
        }

        // Jump logic: prefer a "high" jump if there is a platform above within a few tiles.
        bool hasAbove = false;
        for (int dy = 2; dy <= 4; dy++) {
          uint8_t above = collisionAt(midTx, bottomTy - dy);
          if (above == COL_SOLID || above == COL_ONEWAY) {
            hasAbove = true;
            break;
          }
        }

        if ((rand() % 220) == 0) {
          e.vy = hasAbove ? -310.0f : ((rand() & 1) ? -240.0f : -150.0f);
        } else if (landedOn == COL_ONEWAY && (rand() % 360) == 0) {
          // Short hop with a bit more speed to help it leave a one-way platform.
          e.vy = -120.0f;
          e.vx = 40.0f + (float)(rand() % 25);
        }
      }

      // Throw cadence: burst a few hammers, then wait.
      if (e.b <= 0)
        e.b = 60 + (rand() % 120); // frames-ish until next burst
      e.b -= 1;
      if (e.b == 0) {
        e.a = 2 + (rand() % 5); // throw count remaining
      }
      if (e.a > 0) {
        // Throw one hammer every ~0.25s.
        if (e.timer >= 0.25f) {
          e.timer = 0.0f;
          int slot = findFreeEntitySlot();
          if (slot >= 0) {
            int dir = (g_p.r.x + g_p.r.w * 0.5f >= e.r.x) ? 1 : -1;
            Entity &h = g_ents[slot];
            h = {};
            h.on = true;
            h.type = E_HAMMER;
            h.dir = dir;
            h.r = {e.r.x + 8.0f, e.r.y + 8.0f, 16.0f, 16.0f};
            h.vx = 110.0f * (float)dir;
            h.vy = -260.0f;
          }
          e.state = 1;
          e.a -= 1;
        }
      } else if (e.state == 1 && e.timer >= 0.2f) {
        // Return to idle visuals after the throw frame.
        e.state = 0;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          if (playerStomp(pl)) {
            e.on = false;
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            pl.score += 200;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
          } else if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_HAMMER) {
      // Hammer projectile: arc, die on collision, hurts players.
      constexpr float kGravity = 450.0f;
      e.r.x += e.vx * dt;
      e.vy += kGravity * dt;
      e.r.y += e.vy * dt;

      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int midTy = (int)((e.r.y + e.r.h * 0.5f) / TILE);
      if (solidAt(midTx, midTy)) {
        e.on = false;
        continue;
      }
      if (e.r.x < g_camX - 96 || e.r.x > g_camX + GAME_W + 96 ||
          e.r.y > GAME_H + 64) {
        e.on = false;
        continue;
      }
      for (int pi = 0; pi < g_playerCount; pi++) {
        Player &pl = g_players[pi];
        if (pl.dead || !overlap(pl.r, e.r))
          continue;
        if (playerIsInvulnerable(pl))
          break;
        if (pl.power > P_SMALL) {
          pl.power = P_SMALL;
          pl.crouch = false;
          setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
          pl.invT = 2.0f;
          if (g_sfxDamage)
            Mix_PlayChannel(-1, g_sfxDamage, 0);
        } else if (pi == 0) {
          pl.dead = true;
          pl.lives--;
          g_state = GS_DEAD;
          Mix_HaltMusic();
        } else {
          pl.invT = 2.0f;
        }
        break;
      }
    } else if (e.type == E_LAKITU) {
      // Lakitu hovers near the top and tosses spinies.
      // `state` is used only for visuals: 0=idle, 1=throw (brief).
      if (e.state == 1 && e.timer >= 0.25f) {
        e.state = 0;
      }
      float playerX = g_p.r.x + g_p.r.w * 0.5f;
      float targetX = playerX + 64.0f;
      float dx = targetX - (e.r.x + 8.0f);
      float speed = 0.0f;
      if (fabsf(dx) > 16.0f) {
        speed = fmaxf(48.0f, fabsf(dx) * 2.0f);
        if (speed > 160.0f)
          speed = 160.0f;
        e.dir = (dx > 0.0f) ? 1 : -1;
      }
      e.r.x += (float)e.dir * speed * dt;
      e.r.y = e.baseY;

      // Throw spiny if under cap.
      int spinyCount = 0;
      for (int j = 0; j < 64; j++) {
        if (g_ents[j].on && g_ents[j].type == E_SPINY)
          spinyCount++;
      }
      if (spinyCount < 3) {
        if (e.a <= 0)
          e.a = 120 + (rand() % 180);
        e.a -= 1;
        if (e.a == 0) {
          int slot = findFreeEntitySlot();
          if (slot >= 0) {
            Entity &s = g_ents[slot];
            s = {};
            s.on = true;
            s.type = E_SPINY;
            s.state = 0; // egg
            s.dir = 0;
            s.r = {e.r.x + 8.0f, e.r.y + 16.0f, 16.0f, 16.0f};
            s.vx = 0.0f;
            s.vy = -150.0f;
          }
          // Show the throw frame for a moment.
          e.state = 1;
          e.timer = 0.0f;
        }
      }

      if (e.r.x > g_camX - 64 && e.r.x < g_camX + GAME_W + 64) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          if (playerStomp(pl)) {
            e.on = false;
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            pl.score += 200;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
          } else if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_SPINY) {
      // Spiny: egg falls, then walks. Can't be stomped.
      constexpr float kGravity = 15.0f;
      e.vy += kGravity;
      e.r.y += e.vy * dt;
      if (e.state == 0) {
        // Egg: no horizontal motion.
        int midTx = (int)((e.r.x + 8.0f) / TILE);
        int bottomTy = (int)((e.r.y + e.r.h) / TILE);
        uint8_t col = collisionAt(midTx, bottomTy);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          e.r.y = bottomTy * TILE - e.r.h;
          e.vy = 0.0f;
          e.state = 1;
          e.dir = (g_p.r.x + g_p.r.w * 0.5f >= e.r.x) ? 1 : -1;
          e.vx = 32.0f;
        }
      } else {
        if (e.dir == 0)
          e.dir = -1;
        if (e.vx == 0.0f)
          e.vx = 32.0f;
        e.r.x += e.dir * e.vx * dt;
        int frontTx =
            e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
        int frontTy = (int)((e.r.y + e.r.h - 1) / TILE);
        if (solidAt(frontTx, frontTy))
          e.dir = -e.dir;
      }

      if (e.r.x < g_camX - 96 || e.r.x > g_camX + GAME_W + 96 ||
          e.r.y > GAME_H + 64) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_BLOOPER) {
      // Blooper (Godot: Blooper.gd): drift down until near player, then rise.
      if (g_theme != THEME_UNDERWATER && g_theme != THEME_CASTLE_WATER) {
        // Only meaningful in underwater sections; keep it out of non-water themes.
        e.on = false;
        continue;
      }

      float playerY = g_p.r.y;
      if (e.state == 0) {
        e.r.y += 32.0f * dt;
        if (e.r.y >= playerY - 24.0f && e.a == 0) {
          // Begin rise.
          int dir = (g_p.r.x + g_p.r.w * 0.5f >= e.r.x) ? 1 : -1;
          e.dir = dir;
          e.vx = 32.0f * (float)dir / 0.75f;
          e.vy = -32.0f / 0.75f;
          // Add a little side drift so bloopers don't feel too rigid (requested).
          e.b = (rand() % 3) - 1; // -1,0,1
          e.state = 1;
          e.timer = 0.0f;
          e.a = 1;
        }
      } else if (e.state == 1) {
        // Occasionally tweak drift direction during the rise.
        if ((rand() % 40) == 0)
          e.b = (rand() % 3) - 1;
        float drift = (float)e.b * 14.0f * dt;
        e.r.x += e.vx * dt;
        e.r.x += drift;
        e.r.y += e.vy * dt;
        if (e.r.y < 0.0f)
          e.r.y = 0.0f;
        if (e.r.y > 64.0f)
          e.r.y = 64.0f;
        if (e.timer >= 0.75f) {
          e.state = 2;
          e.timer = 0.0f;
          e.vx = 0.0f;
          e.vy = 0.0f;
        }
      } else {
        if (e.timer >= 0.25f) {
          e.state = 0;
          e.timer = 0.0f;
          e.a = 0;
        }
      }

      if (e.r.x < g_camX - 96 || e.r.x > g_camX + GAME_W + 96 ||
          e.r.y > GAME_H + 64) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          // Blooper can't be stomped (underwater hazard).
          if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_BULLET_BILL) {
      float speed = (e.vx != 0.0f) ? fabsf(e.vx) : 90.0f;
      if (e.dir == 0)
        e.dir = -1;
      e.r.x += e.dir * speed * dt;

      if (e.r.x < g_camX - 96 || e.r.x > g_camX + GAME_W + 96) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          if (playerStomp(pl)) {
            e.on = false;
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            pl.score += 200;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
          } else if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_CHEEP_LEAP) {
      // If this cheep wasn't initialized by a generator, give it a sane default.
      if (e.vx == 0.0f && e.timer < 0.05f) {
        e.dir = (rand() & 1) ? -1 : 1;
        e.vx = (50.0f + (float)(rand() % 151)) * (float)e.dir;
        e.vy = -(250.0f + (float)(rand() % 101));
        e.b = (int)liquidSurfaceYAtWorldX(e.r.x + 8.0f);
      }

      constexpr float kGravity = 300.0f;
      e.r.x += e.vx * dt;
      e.vy += kGravity * dt;
      e.r.y += e.vy * dt;

      float surface = (e.b > 0) ? (float)e.b : liquidSurfaceYAtWorldX(e.r.x + 8.0f);
      if (e.b <= 0)
        e.b = (int)surface;
      if (e.vy > 0.0f && e.r.y > surface + 16.0f) {
        e.on = false;
        continue;
      }

      if (e.r.x < g_camX - 96 || e.r.x > g_camX + GAME_W + 96) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          if (playerStomp(pl)) {
            e.on = false;
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            pl.score += 200;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
          } else if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_CHEEP_SWIM) {
      if (e.vx == 0.0f)
        e.vx = 20.0f;
      if (e.dir == 0)
        e.dir = -1;

      e.r.x += e.dir * e.vx * dt;
      e.r.y = e.baseY + sinf(e.timer * 2.0f) * 4.0f;

      int frontTx =
          e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      int frontTy = (int)((e.r.y + e.r.h * 0.5f) / TILE);
      if (solidAt(frontTx, frontTy))
        e.dir = -e.dir;

      if (e.r.x < g_camX - 96 || e.r.x > g_camX + GAME_W + 96) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32) {
        Rect er = enemyHitRect(e);
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, er))
            continue;
          if (playerStomp(pl)) {
            e.on = false;
            pl.vy = jumpHeld(g_playerHeld[pi]) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                               : -Physics::BOUNCE_HEIGHT;
            pl.score += 200;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
          } else if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_CASTLE_AXE) {
      // Touching the axe ends the castle section (like SMB1 bridge axe).
      if (g_state == GS_PLAYING && overlap(g_p.r, e.r)) {
        e.on = false;
        g_state = GS_WIN;
        g_levelTimer = 0.0f;
        if (!g_castleSfxPlayed && g_sfxCastleClear) {
          Mix_PlayChannel(-1, g_sfxCastleClear, 0);
          g_castleSfxPlayed = true;
        }
      }
    } else if (e.type == E_BOWSER) {
      // Minimal Bowser: patrol + gravity + damage on contact. Fireballs can
      // "wear him down" to keep the section beatable even without the axe.
      if (e.dir == 0)
        e.dir = -1;
      float speed = 18.0f;
      e.r.x += e.dir * speed * dt;
      e.vy += 15.0f;
      e.r.y += e.vy * dt;

      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int bottomTy = (int)((e.r.y + e.r.h) / TILE);
      {
        uint8_t col = collisionAt(midTx, bottomTy);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = bottomTy * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = bottomTy * TILE - e.r.h;
            e.vy = 0;
          }
        }
      }

      int frontTx =
          e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      int frontTy = (int)((e.r.y + e.r.h - 1) / TILE);
      if (solidAt(frontTx, frontTy))
        e.dir = -e.dir;

      if (e.r.x < g_camX - 128 || e.r.y > GAME_H + 96) {
        e.on = false;
        continue;
      }

      if (e.r.x > g_camX - 64 && e.r.x < g_camX + GAME_W + 64) {
        for (int pi = 0; pi < g_playerCount; pi++) {
          Player &pl = g_players[pi];
          if (pl.dead || !overlap(pl.r, e.r))
            continue;
          if (!playerIsInvulnerable(pl)) {
            if (pl.power > P_SMALL) {
              pl.power = P_SMALL;
              pl.crouch = false;
              setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              pl.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else if (pi == 0) {
              pl.dead = true;
              pl.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            } else {
              pl.invT = 2.0f;
              pl.vy = -Physics::BOUNCE_HEIGHT * 0.75f;
            }
          }
          break;
        }
      }
    } else if (e.type == E_MUSHROOM) {
      e.vy += 15.0f;
      e.r.x += e.vx * dt * e.dir;
      e.r.y += e.vy * dt;
      int ty = (int)((e.r.y + e.r.h) / TILE);
      {
        int midTx = (int)((e.r.x + 8) / TILE);
        uint8_t col = collisionAt(midTx, ty);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = ty * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = ty * TILE - e.r.h;
            e.vy = 0;
          }
        }
      }
      int tx = e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      if (solidAt(tx, (int)(e.r.y / TILE)))
        e.dir = -e.dir;
      for (int pi = 0; pi < g_playerCount; pi++) {
        Player &pl = g_players[pi];
        if (pl.dead || !overlap(pl.r, e.r))
          continue;
        e.on = false;
        if (pl.power == P_SMALL) {
          pl.power = P_BIG;
          pl.crouch = false;
          setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_BIG, PLAYER_HIT_H_BIG);
        }
        pl.score += 1000;
        if (g_sfxPowerup)
          Mix_PlayChannel(-1, g_sfxPowerup, 0);
        else if (g_sfxItemAppear)
          Mix_PlayChannel(-1, g_sfxItemAppear, 0);
        break;
      }
    } else if (e.type == E_FIRE_FLOWER) {
      e.vy += 15.0f;
      e.r.y += e.vy * dt;
      int ty = (int)((e.r.y + e.r.h) / TILE);
      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      {
        uint8_t col = collisionAt(midTx, ty);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = ty * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = ty * TILE - e.r.h;
            e.vy = 0;
          }
        }
      }
      for (int pi = 0; pi < g_playerCount; pi++) {
        Player &pl = g_players[pi];
        if (pl.dead || !overlap(pl.r, e.r))
          continue;
        e.on = false;
        if (pl.power == P_SMALL) {
          pl.power = P_BIG;
          pl.crouch = false;
          setPlayerSizePreserveFeet(pl, PLAYER_HIT_W_BIG, PLAYER_HIT_H_BIG);
        } else {
          pl.power = P_FIRE;
        }
        pl.score += 1000;
        if (g_sfxPowerup)
          Mix_PlayChannel(-1, g_sfxPowerup, 0);
        else if (g_sfxItemAppear)
          Mix_PlayChannel(-1, g_sfxItemAppear, 0);
        break;
      }
    } else if (e.type == E_FIREBALL) {
      // A little faster with smaller hops for better feel.
      const float speed = 240.0f;
      e.r.x += e.dir * speed * dt;
      e.vy += 420.0f * dt;
      e.r.y += e.vy * dt;

      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int bottomTy = (int)((e.r.y + e.r.h) / TILE);
      {
        uint8_t col = collisionAt(midTx, bottomTy);
        if (col == COL_SOLID || col == COL_ONEWAY) {
          if (col == COL_ONEWAY) {
            float prevBottom = (e.r.y - (e.vy * dt)) + e.r.h;
            float tileTop = bottomTy * TILE;
            if (prevBottom > tileTop + 0.1f)
              col = COL_NONE;
          }
          if (col != COL_NONE) {
            e.r.y = bottomTy * TILE - e.r.h;
            e.vy = -55.0f;
          }
        }
      }

      int frontTx =
          e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      int frontTy = (int)((e.r.y + e.r.h * 0.5f) / TILE);
      if (solidAt(frontTx, frontTy)) {
        e.on = false;
        continue;
      }

      if (e.r.x < g_camX - 64 || e.r.x > g_camX + GAME_W + 64)
        e.on = false;

      for (int j = 0; j < 64 && e.on; j++) {
        Entity &t = g_ents[j];
        if (!t.on)
          continue;
        if (t.type != E_GOOMBA && t.type != E_KOOPA && t.type != E_KOOPA_RED &&
            t.type != E_BUZZY_BEETLE && t.type != E_BLOOPER &&
            t.type != E_LAKITU && t.type != E_SPINY && t.type != E_HAMMER_BRO &&
            t.type != E_CHEEP_LEAP && t.type != E_CHEEP_SWIM &&
            t.type != E_BULLET_BILL && t.type != E_BOWSER)
          continue;
        if (!overlap(e.r, enemyHitRect(t)))
          continue;
        bool awardPoints = true;
        if (t.type == E_BOWSER) {
          t.state += 1;
          if (t.state >= 8)
            t.on = false;
        } else if (t.type == E_BUZZY_BEETLE) {
          // Classic behavior: Buzzy Beetles shrug off fireballs.
          awardPoints = false;
        } else {
          t.on = false;
        }
        e.on = false;
        if (awardPoints)
          g_p.score += 200;
        break;
      }
    } else if (e.type == E_COIN_POPUP) {
      e.vy += 400 * dt;
      e.r.y += e.vy * dt;
      if (e.timer > 0.5f)
        e.on = false;
    }
  }

  auto enemyActive = [](const Entity &e) {
    if (!e.on)
      return false;
    if (e.type == E_GOOMBA)
      return e.state == 0;
    if (e.type == E_KOOPA || e.type == E_KOOPA_RED)
      return true;
    if (e.type == E_BUZZY_BEETLE)
      return true;
    if (e.type == E_SPINY || e.type == E_HAMMER_BRO)
      return true;
    return false;
  };

  for (int i = 0; i < 64; i++) {
    Entity &a = g_ents[i];
    if (!enemyActive(a))
      continue;
    for (int j = i + 1; j < 64; j++) {
      Entity &b = g_ents[j];
      if (!enemyActive(b))
        continue;
      if (!overlap(enemyHitRect(a), enemyHitRect(b)))
        continue;
      auto isShell = [](const Entity &e) {
        if (e.type == E_KOOPA || e.type == E_KOOPA_RED)
          return e.state == 2;
        if (e.type == E_BUZZY_BEETLE)
          return e.state == 2;
        return false;
      };
      bool aShell = isShell(a);
      bool bShell = isShell(b);

      if (aShell && !bShell) {
        b.on = false;
        g_p.score += 200;
        if (g_sfxKick)
          Mix_PlayChannel(-1, g_sfxKick, 0);
        continue;
      }
      if (bShell && !aShell) {
        a.on = false;
        g_p.score += 200;
        if (g_sfxKick)
          Mix_PlayChannel(-1, g_sfxKick, 0);
        continue;
      }

      if (a.dir != 0)
        a.dir = -a.dir;
      if (b.dir != 0)
        b.dir = -b.dir;
    }
  }
}

void drawTile(int tx, int ty, uint8_t tile) {
  int x = tx * TILE - (int)g_camX;
  if (x < -TILE || x > GAME_W)
    return;
  SDL_Rect dst = {x, ty * TILE, TILE, TILE};
  {
    float lift = 0.0f;
    float scale = 1.0f;
    if (bumpTransformForTile(tx, ty, lift, scale)) {
      int dw = (int)lroundf(TILE * scale);
      int dh = (int)lroundf(TILE * scale);
      int extraW = dw - TILE;
      int extraH = dh - TILE;
      dst.x -= extraW / 2;
      dst.y -= extraH;
      dst.y -= (int)lroundf(lift);
      dst.w = dw;
      dst.h = dh;
    }
  }

  if (tile == T_COIN) {
    if (g_texCoin) {
      // Frame 0 of SpinningCoin.png is a solid green placeholder in the current
      // asset set, so skip it.
      int frame = 1 + ((SDL_GetTicks() / 120) % 3);
      SDL_Rect src = {frame * 16, 0, 16, 16};
      renderCopyWithShadow(g_texCoin, &src, &dst);
    } else {
      SDL_SetRenderDrawColor(g_ren, 255, 200, 0, 255);
      SDL_RenderFillRect(g_ren, &dst);
    }
    return;
  }

  if (tile == T_QUESTION && g_texQuestion) {
    int frame = ((SDL_GetTicks() / 200) % 3);
    SDL_Rect src = {frame * 16, questionRowForTheme(g_theme), 16, 16};
    renderCopyWithShadow(g_texQuestion, &src, &dst);
    return;
  }
  if (tile == T_USED && g_texQuestion) {
    // Use the dedicated "used" tile (check-mark) in the QuestionBlock sheet.
    SDL_Rect src = {2 * 16, questionRowForTheme(g_theme), 16, 16};
    renderCopyWithShadow(g_texQuestion, &src, &dst);
    return;
  }

  // If a terrain atlas is available, prefer it for tiles coming from the
  // Godot TileMap so we preserve proper edges, gaps, and pipe pieces.
  if (g_levelInfo.atlasX && g_levelInfo.atlasY) {
    uint8_t ax = g_levelInfo.atlasX[ty][tx];
    uint8_t ay = g_levelInfo.atlasY[ty][tx];
    if (ax != 255 && ay != 255) {
      SDL_Rect src = {ax * 16, ay * 16, 16, 16};
      SDL_Texture *atlasTex = g_texTerrain;
      uint8_t at = g_levelInfo.atlasT ? g_levelInfo.atlasT[ty][tx] : ATLAS_TERRAIN;
      if (at == ATLAS_DECO)
        atlasTex = g_texDeco ? g_texDeco : g_texTerrain;
      else if (at == ATLAS_LIQUID)
        atlasTex = g_texLiquids ? g_texLiquids : g_texTerrain;
      if (atlasTex)
        renderCopyWithShadow(atlasTex, &src, &dst);
      return;
    }

    // Collision-only markers: the level generator may stamp T_PIPE tiles
    // (without atlas coords) for PipeArea safety; don't render these.
    if (tile == T_PIPE)
      return;
  }

  // Fallback simple tiles
  if (g_texTerrain) {
    SDL_Rect src = {0, 0, 16, 16};
    bool use = true;
    switch (tile) {
    case T_GROUND:
      src = {0, 0, 16, 16};
      break;
    case T_BRICK:
      src = {0, 64, 16, 16};
      break;
    case T_USED:
      src = {0, 64, 16, 16};
      break;
    case T_PIPE: {
      // If we don't have atlas coordinates for a pipe tile, prefer the solid
      // color fallback below. The fixed atlas coords here only cover a subset
      // of pipe pieces and can create repeating/incorrect visuals.
      use = false;
      break;
    }
    case T_CASTLE:
      src = {0, 176, 16, 16};
      break;
    case T_COIN:
      use = false;
      break;
    default:
      use = false;
      break;
    }
    if (use) {
      renderCopyWithShadow(g_texTerrain, &src, &dst);
      return;
    }
  }

  switch (tile) {
  case T_GROUND:
    SDL_SetRenderDrawColor(g_ren, 200, 76, 12, 255);
    break;
  case T_BRICK:
    SDL_SetRenderDrawColor(g_ren, 180, 80, 20, 255);
    break;
  case T_USED:
    SDL_SetRenderDrawColor(g_ren, 180, 80, 20, 255);
    break;
  case T_QUESTION:
    SDL_SetRenderDrawColor(g_ren, tile == T_QUESTION ? 252 : 120,
                           tile == T_QUESTION ? 184 : 76, 0, 255);
    break;
  case T_PIPE:
    SDL_SetRenderDrawColor(g_ren, 0, 168, 0, 255);
    break;
  case T_FLAG:
    SDL_SetRenderDrawColor(g_ren, 100, 100, 100, 255);
    break;
  case T_CASTLE:
    SDL_SetRenderDrawColor(g_ren, 100, 100, 100, 255);
    break;
  case T_COIN:
    SDL_SetRenderDrawColor(g_ren, 255, 200, 0, 255);
    break;
  default:
    return;
  }
  SDL_RenderFillRect(g_ren, &dst);

}

static bool computeCastleDst(SDL_Rect &outMainDst, SDL_Rect &outOverlayDst) {
  if (!g_texCastle)
    return false;
  int minX = mapWidth();
  int maxY = -1;
  for (int y = 0; y < MAP_H; y++) {
    for (int x = 0; x < mapWidth(); x++) {
      if (g_map[y][x] == T_CASTLE) {
        if (x < minX)
          minX = x;
        if (y > maxY)
          maxY = y;
      }
    }
  }
  if (maxY < 0 || minX >= mapWidth())
    return false;
  int texW = 0, texH = 0;
  SDL_QueryTexture(g_texCastle, nullptr, nullptr, &texW, &texH);

  // Determine the ground line under the castle by scanning for the *surface*
  // collision (ignore T_CASTLE marker tiles, which may be stamped in the map).
  int castleTilesW = (texW + TILE - 1) / TILE;
  int surfaceTy = -1;
  for (int x = minX; x < minX + castleTilesW && x < mapWidth(); x++) {
    for (int y = MAP_H - 1; y >= 0; y--) {
      if (g_map[y][x] == T_CASTLE)
        continue;
      uint8_t col = collisionAt(x, y);
      if (!(col == COL_SOLID || col == COL_ONEWAY))
        continue;
      uint8_t above = (y > 0) ? collisionAt(x, y - 1) : COL_NONE;
      if (above == COL_NONE) {
        if (surfaceTy < 0 || y < surfaceTy)
          surfaceTy = y;
      }
      // Keep scanning upward in case this column has thick ground.
    }
  }
  if (surfaceTy < 0)
    surfaceTy = maxY;
  // The castle/overlay art is authored to sit *on* the ground tiles. The
  // surface tile we find here is the first solid tile; align the sprite to the
  // top of that tile and then lift by one tile to match the reference.
  int groundY = surfaceTy * TILE;
  groundY -= TILE;
  if (groundY < 0)
    groundY = 0;

  // EndingCastleSprite.png layout:
  // - Main castle sprite in the top 80px (y=0..79).
  // - A separate "front overlay" piece in the bottom 40px (y=120..159),
  //   positioned on the right half of the texture, used to hide the player as
  //   they enter the door.
  constexpr int kCastleMainH = 80;
  constexpr int kOverlayY = 120;
  constexpr int kOverlayX = 32;

  int mainH = (texH < kCastleMainH) ? texH : kCastleMainH;
  outMainDst = {minX * TILE - (int)g_camX, groundY - mainH, texW, mainH};

  int overlayH = (texH > kOverlayY) ? (texH - kOverlayY) : 0;
  int overlayW = (texW > kOverlayX) ? (texW - kOverlayX) : 0;
  outOverlayDst = {outMainDst.x + kOverlayX, groundY - overlayH, overlayW,
                   overlayH};
  return true;
}

void render() {
  if (g_state == GS_TITLE) {
    // Sky backdrop.
    SDL_SetRenderDrawColor(g_ren, 92, 148, 252, 255);
    SDL_RenderClear(g_ren);

    // Decorative background layers.
    {
      if (g_texBgSky) {
        SDL_Rect src = {0, 0, 512, 240};
        SDL_Rect dst = {0, 0, 512, GAME_H};
        for (int x = dst.x; x < GAME_W; x += 512) {
          SDL_Rect d = {x, dst.y, dst.w, dst.h};
          SDL_RenderCopy(g_ren, g_texBgSky, &src, &d);
        }
      }
      int y = GAME_H - 64;
      if (g_texBgHills) {
        SDL_Rect src = {0, 512 - 96, 512, 96};
        SDL_Rect dst = {0, y - 32, 512, 96};
        for (int x = dst.x; x < GAME_W; x += 512) {
          SDL_Rect d = {x, dst.y, dst.w, dst.h};
          SDL_RenderCopy(g_ren, g_texBgHills, &src, &d);
        }
      }
      if (g_texBgBushes) {
        SDL_Rect src = {0, 512 - 64, 512, 64};
        SDL_Rect dst = {0, y, 512, 64};
        for (int x = dst.x; x < GAME_W; x += 512) {
          SDL_Rect d = {x, dst.y, dst.w, dst.h};
          SDL_RenderCopy(g_ren, g_texBgBushes, &src, &d);
        }
      }
      if (g_texBgCloudOverlay) {
        SDL_SetTextureAlphaMod(g_texBgCloudOverlay, 200);
        SDL_Rect src = {0, 0, 512, 512};
        SDL_Rect dst = {0, -40, 512, 512};
        for (int x = dst.x; x < GAME_W; x += 512) {
          SDL_Rect d = {x, dst.y, dst.w, dst.h};
          SDL_RenderCopy(g_ren, g_texBgCloudOverlay, &src, &d);
        }
        SDL_SetTextureAlphaMod(g_texBgCloudOverlay, 255);
      }
    }

    // Ground strip.
    {
      int groundY = GAME_H - 32;
      SDL_Rect src = {0, 0, 16, 16};
      for (int row = 0; row < 2; row++) {
        for (int x = 0; x < GAME_W + 16; x += 16) {
          SDL_Rect dst = {x, groundY + row * 16, 16, 16};
          if (g_texTerrain)
            renderCopyWithShadow(g_texTerrain, &src, &dst);
          else {
            SDL_SetRenderDrawColor(g_ren, 200, 76, 12, 255);
            SDL_RenderFillRect(g_ren, &dst);
          }
        }
      }
    }

    // Title banner.
    // Title2.png is a 3x7 grid of 176x40 "REMSTERED" variants; render one cell.
    if (g_texTitle) {
      SDL_Rect src = {0, 0, 176, 40};
      int w = 320;
      int h = (w * src.h) / src.w;
      SDL_Rect dst = {(GAME_W - w) / 2, 18, w, h};
      SDL_RenderCopy(g_ren, g_texTitle, &src, &dst);
    }

    if (g_titleMode == TITLE_MAIN) {
      const char *items[] = {"PLAY GAME", "SETTINGS", "EXTRAS"};
      constexpr int itemCount = 3;
      int baseY = 180;
      for (int i = 0; i < itemCount; i++) {
        SDL_Color c = (i == g_mainMenuIndex) ? SDL_Color{255, 255, 0, 255}
                                             : SDL_Color{255, 255, 255, 255};
        int x = (GAME_W - textWidth(items[i], 2)) / 2;
        int y = baseY + i * 20;
        drawTextShadow(x, y, items[i], 2, c);
        if (i == g_mainMenuIndex && g_texMushroom) {
          SDL_Rect src = {0, 0, 16, 16};
          SDL_Rect dst = {x - 26, y + 2, 16, 16};
          renderCopyWithShadow(g_texMushroom, &src, &dst);
        }
      }
      drawTextShadow(8, GAME_H - 18, "V1.0.1", 1, {255, 255, 255, 255});
    } else if (g_titleMode == TITLE_CHAR_SELECT) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 170);
      SDL_Rect panel = {24, 82, GAME_W - 48, 108};
      SDL_RenderFillRect(g_ren, &panel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &panel);

      const char *selText = "SELECT CHARACTER";
      drawTextShadow((GAME_W - textWidth(selText, 2)) / 2, 90, selText, 2,
                     {255, 255, 255, 255});

      const int startX = (GAME_W - (g_charCount * 56 - 16)) / 2;
      const int y = 124;
      for (int i = 0; i < g_charCount; i++) {
        int x = startX + i * 56;
        int iconW = PLAYER_DRAW_W_SMALL * 2;
        int iconH = 16 * 2;
        SDL_Rect dst = {x + (24 - iconW) / 2, y + (24 - iconH) / 2, iconW,
                        iconH};
        SDL_Rect src = {0, 0, 16, 16};
        if (g_texPlayerSmall[i])
          renderCopyWithShadow(g_texPlayerSmall[i], &src, &dst);

        SDL_Rect box = {x - 6, y - 6, 36, 36};
        SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
        if (i == g_menuIndex) {
          SDL_RenderDrawRect(g_ren, &box);
          SDL_Rect inner = {box.x + 2, box.y + 2, box.w - 4, box.h - 4};
          SDL_RenderDrawRect(g_ren, &inner);
        }
      }

      drawTextShadow(
          (GAME_W - textWidth(g_charDisplayNames[g_menuIndex], 2)) / 2, y + 34,
          g_charDisplayNames[g_menuIndex], 2, {255, 255, 255, 255});
      drawTextShadow((GAME_W - textWidth("PRESS A", 2)) / 2, y + 54, "PRESS A",
                     2, {255, 255, 0, 255});
      drawTextShadow((GAME_W - textWidth("B BACK", 1)) / 2, y + 74, "B BACK", 1,
                     {220, 220, 220, 255});
      drawTextShadow((GAME_W - textWidth("Y OPTIONS", 1)) / 2, y + 86,
                     "Y OPTIONS", 1, {220, 220, 220, 255});
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
    } else if (g_titleMode == TITLE_MULTI_SELECT) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 170);
      SDL_Rect panel = {20, 64, GAME_W - 40, 140};
      SDL_RenderFillRect(g_ren, &panel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &panel);

      const char *selText = "SELECT PLAYERS";
      drawTextShadow((GAME_W - textWidth(selText, 2)) / 2, 72, selText, 2,
                     {255, 255, 255, 255});

      int slotCount = g_playerCount;
      int slotW = 68;
      int startX = (GAME_W - slotCount * slotW) / 2;
      int baseY = 102;

      const SDL_Color slotCols[4] = {
          {255, 255, 255, 255}, {255, 120, 120, 255}, {120, 255, 120, 255},
          {120, 160, 255, 255}};

      for (int i = 0; i < slotCount; i++) {
        int x = startX + i * slotW;
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "P%d", i + 1);
        drawTextShadow(x + 10, baseY - 10, pbuf, 1, slotCols[i]);

        int ci = g_playerMenuIndex[i] % g_charCount;
        SDL_Rect src = {0, 0, 16, 16};
        SDL_Rect dst = {x + 18, baseY + 10, 32, 32};
        if (g_texPlayerSmall[ci])
          renderCopyWithShadow(g_texPlayerSmall[ci], &src, &dst);

        SDL_Rect box = {x + 10, baseY + 2, 48, 48};
        SDL_SetRenderDrawColor(g_ren, slotCols[i].r, slotCols[i].g,
                               slotCols[i].b, 255);
        SDL_RenderDrawRect(g_ren, &box);

        const char *name = g_charDisplayNames[ci];
        drawTextShadow(x + (slotW - textWidth(name, 1)) / 2, baseY + 56, name,
                       1, {255, 255, 255, 255});

        const char *ready = g_playerReady[i] ? "READY" : "SELECT";
        SDL_Color rc = g_playerReady[i] ? SDL_Color{255, 255, 0, 255}
                                        : SDL_Color{200, 200, 200, 255};
        drawTextShadow(x + (slotW - textWidth(ready, 1)) / 2, baseY + 70, ready,
                       1, rc);
      }

      drawTextShadow((GAME_W - textWidth("P1: A READY  B BACK  + START", 1)) / 2,
                     panel.y + panel.h - 22, "P1: A READY  B BACK  + START", 1,
                     {220, 220, 220, 255});
      drawTextShadow((GAME_W - textWidth("P2-4: 2 READY  1 BACK", 1)) / 2,
                     panel.y + panel.h - 10, "P2-4: 2 READY  1 BACK", 1,
                     {220, 220, 220, 255});
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
    } else if (g_titleMode == TITLE_OPTIONS) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 190);
      SDL_Rect oPanel = {52, 58, GAME_W - 104, 146};
      SDL_RenderFillRect(g_ren, &oPanel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &oPanel);

      const char *title = "SETTINGS";
      drawTextShadow((GAME_W - textWidth(title, 2)) / 2, 66, title, 2,
                     {255, 255, 255, 255});

      const char *line1 =
          g_randomTheme ? "RANDOM THEME: ON" : "RANDOM THEME: OFF";
      const char *line2 = g_nightMode ? "NIGHT MODE: ON" : "NIGHT MODE: OFF";
      bool forcedBacktrack = g_multiplayerActive;
      const char *line3 = (forcedBacktrack || g_allowCameraBacktrack)
                              ? "CAMERA BACKTRACK: ON"
                              : "CAMERA BACKTRACK: OFF";
      int baseY = 98;
      SDL_Color hi = {255, 255, 0, 255};
      SDL_Color norm = {255, 255, 255, 255};
      drawTextShadow(70, baseY, line1, 1, g_optionsIndex == 0 ? hi : norm);
      drawTextShadow(70, baseY + 16, line2, 1,
                     g_optionsIndex == 1 ? hi : norm);
      SDL_Color camCol = forcedBacktrack ? SDL_Color{160, 160, 160, 255}
                                         : (g_optionsIndex == 2 ? hi : norm);
      drawTextShadow(70, baseY + 32, line3, 1, camCol);
      drawTextShadow(70, baseY + 48, "CHEATS", 1,
                     g_optionsIndex == 3 ? hi : norm);
      drawTextShadow(70, baseY + 72, "B BACK", 1, {220, 220, 220, 255});
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
    } else if (g_titleMode == TITLE_CHEATS) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 190);
      SDL_Rect cPanel = {52, 58, GAME_W - 104, 146};
      SDL_RenderFillRect(g_ren, &cPanel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &cPanel);

      const char *title = "CHEATS";
      drawTextShadow((GAME_W - textWidth(title, 2)) / 2, 66, title, 2,
                     {255, 255, 255, 255});

      const char *line1 =
          g_cheatMoonJump ? "MOONJUMP: ON" : "MOONJUMP: OFF";
      const char *line2 = g_cheatGodMode ? "GODMODE: ON" : "GODMODE: OFF";
      int baseY = 98;
      SDL_Color hi = {255, 255, 0, 255};
      SDL_Color norm = {255, 255, 255, 255};
      drawTextShadow(70, baseY, line1, 1, g_cheatsIndex == 0 ? hi : norm);
      drawTextShadow(70, baseY + 16, line2, 1, g_cheatsIndex == 1 ? hi : norm);
      drawTextShadow(70, baseY + 40, "NOTE: PITS STILL KILL", 1,
                     {200, 200, 200, 255});
      drawTextShadow(70, baseY + 72, "B BACK", 1, {220, 220, 220, 255});
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
    } else if (g_titleMode == TITLE_EXTRAS) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 190);
      SDL_Rect ePanel = {52, 70, GAME_W - 104, 110};
      SDL_RenderFillRect(g_ren, &ePanel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &ePanel);
      drawTextShadow((GAME_W - textWidth("EXTRAS", 2)) / 2, 78, "EXTRAS", 2,
                     {255, 255, 255, 255});
      drawTextShadow((GAME_W - textWidth("COMING SOON", 2)) / 2, 114,
                     "COMING SOON", 2, {255, 255, 255, 255});
      drawTextShadow((GAME_W - textWidth("B BACK", 1)) / 2, 150, "B BACK", 1,
                     {220, 220, 220, 255});
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
    }

    SDL_RenderPresent(g_ren);
    return;
  }

  switch (g_theme) {
  case THEME_UNDERGROUND:
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    break;
  case THEME_CASTLE:
    SDL_SetRenderDrawColor(g_ren, 40, 40, 40, 255);
    break;
  case THEME_OVERWORLD:
  default:
    SDL_SetRenderDrawColor(g_ren, 92, 148, 252, 255);
    break;
  }
  SDL_RenderClear(g_ren);

  // Background layers (decorative only).
  {
    if (g_texBgSky) {
      SDL_Rect src = {0, 0, 512, 240};
      SDL_Rect dst = {-(int)(g_camX * 0.03f) % 512, 0, 512, GAME_H};
      for (int x = dst.x; x < GAME_W; x += 512) {
        SDL_Rect d = {x, dst.y, dst.w, dst.h};
        SDL_RenderCopy(g_ren, g_texBgSky, &src, &d);
      }
    }

    int primary = effectiveBgPrimary();

    // Match the general SMB1 feel: for "Hills" levels, draw both hills and
    // bushes; for "Bush" levels, draw bushes only. If a section specifies
    // "None", keep the background empty (except optional sky).
    if (primary == 0 && g_texBgHills) {
      renderTiledBottomSlice(g_texBgHills, 0.10f, g_camX);
    }

    if ((primary == 0 || primary == 1) && g_texBgBushes) {
      renderTiledBottomSlice(g_texBgBushes, 0.18f, g_camX);
    }

    // Secondary layer (trees/mushrooms) sits in front of the primary background
    // but still behind gameplay tiles/entities.
    int secondary = effectiveBgSecondary();
    if (secondary > 0 && g_texBgSecondary) {
      renderTiledBottomSlice(g_texBgSecondary, 0.32f, g_camX);
    }

  }

  // Foreground decorations (still behind the player). Draw these *before*
  // gameplay tiles so pipes/blocks correctly occlude them.
  if (g_texDeco && g_fgDecoCount > 0) {
    for (int i = 0; i < g_fgDecoCount; i++) {
      const ForegroundDeco &d = g_fgDecos[i];
      int baseX = d.tx * TILE - (int)g_camX;
      if (baseX < -(int)(d.w * TILE) || baseX > GAME_W)
        continue;
      for (int c = 0; c < d.cellCount; c++) {
        const auto &cell = d.cells[c];
        SDL_Rect src = {(int)cell.ax * 16, (int)cell.ay * 16, 16, 16};
        SDL_Rect dst = {baseX + cell.dx * TILE, (d.ty + cell.dy) * TILE, 16,
                        16};
        renderCopyWithShadow(g_texDeco, &src, &dst);
      }
    }
  }

  if (g_showDebugOverlay) {
    char buf[128];
    int y = 2;
    SDL_Color c = {255, 255, 0, 255};

    snprintf(buf, sizeof(buf), "THEME %s  NIGHT %d", themeName(g_theme),
             g_nightMode ? 1 : 0);
    drawTextShadow(2, y, buf, 1, c);
    y += 10;

	    snprintf(buf, sizeof(buf), "BG P:%d->%d S:%d->%d C:%d",
	             g_levelInfo.bgPrimary, effectiveBgPrimary(), g_levelInfo.bgSecondary,
	             effectiveBgSecondary(), (g_levelInfo.bgClouds ? 1 : 0));
	    drawTextShadow(2, y, buf, 1, {255, 255, 255, 255});
	    y += 10;

	    snprintf(buf, sizeof(buf), "PARTICLES %d->%d  TEX snow:%s leaves:%s",
	             g_levelInfo.bgParticles, effectiveBgParticles(),
	             g_texParticleSnow ? "OK" : "NULL",
	             (g_texParticleLeaves && g_texParticleAutumnLeaves) ? "OK" : "NULL");
	    drawTextShadow(2, y, buf, 1, {255, 255, 255, 255});
	    y += 10;

    auto texLine = [&](const char *name, SDL_Texture *t) {
      int w = 0, h = 0;
      if (t)
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
      snprintf(buf, sizeof(buf), "%s %s %dx%d", name, t ? "OK" : "NULL", w, h);
      drawTextShadow(2, y, buf, 1, t ? SDL_Color{120, 255, 120, 255}
                                     : SDL_Color{255, 120, 120, 255});
      y += 10;
    };

    texLine("SKY", g_texBgSky);
    texLine("HILLS", g_texBgHills);
    texLine("BUSH", g_texBgBushes);
    texLine("SEC", g_texBgSecondary);
    texLine("CLOUD", g_texBgCloudOverlay);

    snprintf(buf, sizeof(buf), "FGDECO TEX %s  COUNT %d",
             g_texDeco ? "OK" : "NULL", g_fgDecoCount);
    drawTextShadow(2, y, buf, 1, {255, 255, 255, 255});
    y += 10;

    int atlasDeco = 0;
    int atlasAny = 0;
    if (g_levelInfo.atlasT && g_levelInfo.atlasX && g_levelInfo.atlasY) {
      int w = mapWidth();
      for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = 0; tx < w; tx++) {
          if (g_levelInfo.atlasX[ty][tx] == 255 ||
              g_levelInfo.atlasY[ty][tx] == 255)
            continue;
          atlasAny++;
          if (g_levelInfo.atlasT[ty][tx] == ATLAS_DECO)
            atlasDeco++;
        }
      }
    }
    snprintf(buf, sizeof(buf), "ATLAS TILES %d  DECO %d", atlasAny, atlasDeco);
    drawTextShadow(2, y, buf, 1, {255, 255, 255, 255});
  }

  // Tiles
  int startTx = (int)(g_camX / TILE) - 1;
	  int viewTiles = (GAME_W + TILE - 1) / TILE + 2;
	  auto isDecoTile = [&](int tx, int ty) -> bool {
    if (!g_levelInfo.atlasT || !g_levelInfo.atlasX || !g_levelInfo.atlasY)
      return false;
    if (g_levelInfo.atlasX[ty][tx] == 255 || g_levelInfo.atlasY[ty][tx] == 255)
      return false;
    return g_levelInfo.atlasT[ty][tx] == ATLAS_DECO;
  };
  // Draw decorations first (background), then gameplay terrain/blocks.
  for (int pass = 0; pass < 2; pass++) {
    bool decoPass = (pass == 0);
    for (int ty = 0; ty < MAP_H; ty++) {
      for (int tx = startTx; tx < startTx + viewTiles && tx < mapWidth(); tx++) {
        if (tx < 0)
          continue;
        bool deco = isDecoTile(tx, ty);
        if (decoPass != deco)
          continue;
        drawTile(tx, ty, g_map[ty][tx]);
      }
	    }
	  }

	    // Flagpole + flag
	    if (g_hasFlag) {
	      // Find the local ground under/near the pole. Some level data may place
	      // collision markers in the pole columns, so scan a small range.
	      int groundTy = MAP_H - 1;
	      int foundX = g_flagX;
	      bool found = false;
	      for (int y = MAP_H - 1; y >= 0 && !found; y--) {
	        for (int x = g_flagX - 2; x <= g_flagX + 2; x++) {
	          if (x < 0 || x >= mapWidth())
	            continue;
	          if (collisionAt(x, y) == COL_SOLID || collisionAt(x, y) == COL_ONEWAY) {
	            groundTy = y;
	            foundX = x;
	            found = true;
	            break;
	          }
	        }
	      }
	      // The ground is often 2 tiles thick; align the pole base to the *top*
	      // of that stack so it doesn't sink a tile into the floor.
	      while (groundTy > 0) {
	        uint8_t above = collisionAt(foundX, groundTy - 1);
	        if (above == COL_SOLID || above == COL_ONEWAY)
	          groundTy--;
	        else
	          break;
	      }
	      int poleX = g_flagX * TILE - (int)g_camX;
	      // Align the pole base to the top of the ground tile.
	      int poleBottom = groundTy * TILE;

	      // Draw the flag first, then the pole so the pole sits in front of the
	      // flag and its shadow never overlays the pole.
	      if (g_texFlag) {
	        int texW = 0, texH = 0;
	        SDL_QueryTexture(g_texFlag, nullptr, nullptr, &texW, &texH);
	        int idx = flagPaletteIndexForTheme(g_theme);
	        int cols = (texW / 16);
	        if (cols <= 0)
	          cols = 1;
	        int maxIdx = (cols * (texH / 16)) - 1;
	        if (maxIdx < 0)
	          maxIdx = 0;
	        if (idx > maxIdx)
	          idx = maxIdx;
	        SDL_Rect src = {(idx % cols) * 16, (idx / cols) * 16, 16, 16};
	        int attachX = poleX + g_flagPoleShaftX;
	        SDL_Rect dst = {attachX - 16, (int)g_flagY, 16, 16};
	        renderCopyWithShadow(g_texFlag, &src, &dst);
	      }
	      if (g_texFlagPole) {
	        int texW = 0, texH = 0;
	        SDL_QueryTexture(g_texFlagPole, nullptr, nullptr, &texW, &texH);
	        int idx = flagPolePaletteIndexForTheme(g_theme);
	        int maxIdx = (texW / 16) - 1;
	        if (maxIdx < 0)
	          maxIdx = 0;
	        if (idx > maxIdx)
	          idx = maxIdx;
	        SDL_Rect src = {idx * 16, 0, 16, texH};
	        SDL_Rect dst = {poleX, poleBottom - texH, 16, texH};
	        renderCopyWithShadow(g_texFlagPole, &src, &dst);
	      }
	    }

	    // Castle
	    g_castleDrawOn = computeCastleDst(g_castleDrawDst, g_castleOverlayDst);
	    if (g_castleDrawOn) {
	      SDL_Rect srcTop = {0, 0, g_castleDrawDst.w, g_castleDrawDst.h};
	      renderCopyWithShadow(g_texCastle, &srcTop, &g_castleDrawDst);
	    }

    // Entities
    for (int i = 0; i < 64; i++) {
      Entity &e = g_ents[i];
      if (!e.on)
        continue;
      int ex = (int)(e.r.x - g_camX);
      if (ex < -32 || ex > GAME_W + 32)
        continue;
      SDL_Rect dst = {ex, (int)e.r.y, (int)e.r.w, (int)e.r.h};

      if (e.type == E_GOOMBA) {
        if (g_texGoomba) {
          int frame = (e.state == 1) ? 2 : ((int)(e.timer * 8) % 2);
          SDL_Rect src = {frame * 16, 0, 16, 16};
          if (e.state == 1) {
            dst.h = 8;
            dst.y += 8;
          }
          renderCopyWithShadow(g_texGoomba, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 172, 100, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_KOOPA || e.type == E_KOOPA_RED) {
        if (g_texKoopa || g_texKoopaSheet) {
          if (e.state >= 1) {
            dst.h = 16;
            dst.w = 16;
            if (g_texKoopaSheet) {
              SDL_Rect src = {0, 16, 16, 16};
              if (e.state == 2) {
                static const int kSpinX[4] = {16, 32, 48, 0};
                int frame = ((int)(e.timer * 12) % 4);
                src.x = kSpinX[frame];
              }
              renderCopyWithShadow(g_texKoopaSheet, &src, &dst);
            } else if (g_texKoopa) {
              SDL_Rect src = {0, 8, 16, 16};
              renderCopyWithShadow(g_texKoopa, &src, &dst);
            }
          } else {
            SDL_Rect src = {((int)(e.timer * 8) % 2) * 16, 0, 16, 24};
            SDL_RendererFlip flip =
                (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_Rect drawDst = dst;
            drawDst.h = 24;
            drawDst.y -= 8;
            renderCopyExWithShadow(g_texKoopa, &src, &drawDst, flip);
          }
        } else {
          SDL_SetRenderDrawColor(g_ren, 0, 160, 0, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_BUZZY_BEETLE) {
        if (g_texBuzzy) {
          // BuzzyBeetle.png contains both walk frames (x=0..31) and shell
          // frames starting at x=32 (see BuzzyBeetleShell.json).
          SDL_Rect src = {0, 0, 16, 16};
          if (e.state == 0) {
            int frame = ((int)(e.timer * 8) % 2);
            src.x = frame * 16;
          } else if (e.state == 1) {
            // Shell idle: use the "Idle" frame (x=48) from the default strip.
            src.x = 48;
          } else {
            // Shell spinning: cycle 4 frames (x=48..96).
            static const int kSpinX[4] = {48, 64, 80, 96};
            int frame = ((int)(e.timer * 14) % 4);
            src.x = kSpinX[frame];
          }
          SDL_RendererFlip flip =
              (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texBuzzy, &src, &dst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 60, 60, 60, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_BLOOPER) {
        if (g_texBlooper) {
          // Blooper.png: 2 frames in a 32x24 strip (Rise at x=0, Fall at x=16).
          SDL_Rect src = {(e.state == 1) ? 0 : 16, 0, 16, 24};
          SDL_Rect drawDst = dst;
          drawDst.h = 24;
          renderCopyWithShadow(g_texBlooper, &src, &drawDst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 240, 240, 240, 255);
          SDL_Rect drawDst = dst;
          drawDst.h = 24;
          SDL_RenderFillRect(g_ren, &drawDst);
        }
      } else if (e.type == E_LAKITU) {
        // Lakitu rides a 32x32 cloud with a 16x24 body sprite.
        if (g_texLakituCloud) {
          SDL_Rect cloudSrc = {0, 0, 32, 32};
          SDL_Rect cloudDst = {dst.x - 8, dst.y + 8, 32, 32};
          renderCopyWithShadow(g_texLakituCloud, &cloudSrc, &cloudDst);
        }
        if (g_texLakitu) {
          SDL_Rect src = {(e.state == 1) ? 16 : 0, 0, 16, 24};
          SDL_Rect drawDst = dst;
          drawDst.h = 24;
          SDL_RendererFlip flip =
              (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texLakitu, &src, &drawDst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 250, 250, 250, 255);
          SDL_Rect drawDst = dst;
          drawDst.h = 24;
          SDL_RenderFillRect(g_ren, &drawDst);
        }
      } else if (e.type == E_SPINY) {
        if (g_texSpiny) {
          // Spiny.png: egg frames at y=0, walk frames at y=16.
          int frame = ((int)(e.timer * ((e.state == 0) ? 14 : 8)) % 2);
          SDL_Rect src = {frame * 16, (e.state == 0) ? 0 : 16, 16, 16};
          SDL_RendererFlip flip =
              (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texSpiny, &src, &dst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 220, 40, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_HAMMER_BRO) {
        if (g_texHammerBro) {
          // HammerBro.png: Idle (x=0..31), Hammer (x=32..63).
          bool throwing = (e.a > 0);
          int baseX = throwing ? 32 : 0;
          int frame = ((int)(e.timer * 10) % 2);
          SDL_Rect src = {baseX + frame * 16, 0, 16, 24};
          SDL_Rect drawDst = dst;
          drawDst.h = 24;
          SDL_RendererFlip flip =
              (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texHammerBro, &src, &drawDst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 240, 240, 240, 255);
          SDL_Rect drawDst = dst;
          drawDst.h = 24;
          SDL_RenderFillRect(g_ren, &drawDst);
        }
      } else if (e.type == E_HAMMER) {
        if (g_texHammer) {
          SDL_Rect src = {0, 0, 16, 16};
          SDL_Point center = {8, 8};
          double angle = fmod(e.timer * 720.0, 360.0);
          renderCopyExWithShadowAngle(g_texHammer, &src, &dst, angle,
                                      SDL_FLIP_NONE, &center);
        } else {
          SDL_SetRenderDrawColor(g_ren, 200, 200, 200, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_PLATFORM_SIDEWAYS || e.type == E_PLATFORM_VERTICAL ||
                 e.type == E_PLATFORM_ROPE || e.type == E_PLATFORM_FALLING) {
        if (g_texPlatform) {
          SDL_Rect srcL = {0, 0, 8, 8};
          SDL_Rect srcM = {8, 0, 8, 8};
          SDL_Rect srcR = {16, 0, 8, 8};

          SDL_Rect dstL = dst;
          dstL.w = 8;
          renderCopyWithShadow(g_texPlatform, &srcL, &dstL);
          SDL_Rect dstR = dst;
          dstR.w = 8;
          dstR.x = dst.x + dst.w - 8;
          renderCopyWithShadow(g_texPlatform, &srcR, &dstR);

          int midStart = dst.x + 8;
          int midEnd = dst.x + dst.w - 8;
          for (int x = midStart; x < midEnd; x += 8) {
            SDL_Rect dstM = {x, dst.y, 8, dst.h};
            if (x + 8 > midEnd)
              dstM.w = midEnd - x;
            SDL_Rect src = srcM;
            src.w = dstM.w;
            renderCopyWithShadow(g_texPlatform, &src, &dstM);
          }
        } else {
          SDL_SetRenderDrawColor(g_ren, 200, 200, 200, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_CHEEP_SWIM || e.type == E_CHEEP_LEAP) {
        if (g_texCheepCheep) {
          int frame = ((int)(e.timer * 8) % 2);
          SDL_Rect src = {frame * 16, 0, 16, 16};
          SDL_RendererFlip flip =
              ((e.vx > 0.0f) || (e.dir > 0)) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texCheepCheep, &src, &dst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 240, 80, 80, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_BULLET_BILL) {
        if (g_texBulletBill) {
          SDL_Rect src = {0, 0, 16, 16};
          SDL_RendererFlip flip =
              (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texBulletBill, &src, &dst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 40, 40, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_CASTLE_AXE) {
        if (g_texBridgeAxe) {
          SDL_Rect src = {0, 0, 16, 16};
          renderCopyWithShadow(g_texBridgeAxe, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 220, 220, 220, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_BOWSER) {
        if (g_texBowser) {
          SDL_Rect src = {0, 0, 48, 48};
          SDL_Rect drawDst = dst;
          drawDst.w = 48;
          drawDst.h = 48;
          SDL_RendererFlip flip =
              (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
          renderCopyExWithShadow(g_texBowser, &src, &drawDst, flip);
        } else {
          SDL_SetRenderDrawColor(g_ren, 120, 60, 20, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_MUSHROOM) {
        if (g_texMushroom) {
          SDL_Rect src = {0, 0, 16, 16};
          renderCopyWithShadow(g_texMushroom, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 220, 60, 60, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_FIRE_FLOWER) {
        if (g_texFireFlower) {
          int frame = ((int)(e.timer * 12) % 4);
          SDL_Rect src = {frame * 16, fireFlowerRowForTheme(g_theme), 16, 16};
          renderCopyWithShadow(g_texFireFlower, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 220, 140, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_FIREBALL) {
        if (g_texFireball) {
          // Fireball art is an 8x8 sprite packed into the top-left of a 16x16
          // sheet. Draw it centered so rotation doesn't "orbit" around (0,0).
          SDL_Rect src = {0, 0, 8, 8};
          SDL_Rect drawDst = {dst.x + (dst.w - 8) / 2, dst.y + (dst.h - 8) / 2,
                              8, 8};
          SDL_Point center = {4, 4};
          double angle = fmod(e.timer * 720.0, 360.0);
          renderCopyExWithShadowAngle(g_texFireball, &src, &drawDst, angle,
                                      SDL_FLIP_NONE, &center);
        } else {
          SDL_SetRenderDrawColor(g_ren, 255, 120, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_COIN_POPUP) {
        if (g_texCoin) {
          int frame = 1 + ((int)(e.timer * 12) % 3);
          SDL_Rect src = {frame * 16, 0, 16, 16};
          renderCopyWithShadow(g_texCoin, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 255, 200, 0, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      }
    }

    // Players
    for (int pi = 0; pi < g_playerCount; pi++) {
      Player &pl = g_players[pi];
      bool showPlayer = (pl.invT <= 0 || ((int)(pl.animT * 8) % 2) == 0);
      if (pl.dead || !showPlayer)
        continue;

      int px = (int)(pl.r.x - g_camX);
      int pw = (pl.power == P_SMALL) ? PLAYER_DRAW_W_SMALL : PLAYER_DRAW_W_BIG;
      int ph = (pl.power == P_SMALL) ? 16 : 32;
      int drawX = px + (int)((pl.r.w - pw) / 2);
      int drawY = (int)pl.r.y;
      if (pl.crouch && pl.power >= P_BIG)
        drawY -= 16;
      SDL_Rect dst = {drawX, drawY, pw, ph};

      int ci = g_playerCharIndex[pi] % g_charCount;
      SDL_Texture *tex = g_texPlayerSmall[ci];
      if (pl.power == P_FIRE)
        tex = g_texPlayerFire[ci];
      else if (pl.power >= P_BIG)
        tex = g_texPlayerBig[ci];

      if (tex) {
        uint32_t held = g_playerHeld[pi];
        bool inWaterDraw = rectTouchesLiquid(pl.r);
        bool swimStroke = inWaterDraw && (pl.swimAnimT < 0.25f);
        bool skidding =
            pl.ground && (((held & VPAD_BUTTON_LEFT) && pl.vx > 20) ||
                          ((held & VPAD_BUTTON_RIGHT) && pl.vx < -20));
        if (pi == 0 && skidding && g_sfxSkid && g_skidCooldown <= 0.0f) {
          Mix_PlayChannel(-1, g_sfxSkid, 0);
          g_skidCooldown = 0.35f;
        }

        int frame = 0;
        SDL_Rect src = {0, 0, 16, 16};
        if (pl.power == P_SMALL) {
          if (inWaterDraw) {
            if (swimStroke) {
              static const int kSwimSeq[6] = {5, 1, 2, 3, 2, 1};
              int idx = (int)(pl.swimAnimT * 24.0f) % 6;
              frame = kSwimSeq[idx];
            } else {
              frame = ((int)(pl.animT * 6.0f) % 2) ? 5 : 0;
            }
          } else if (!pl.ground) {
            frame = 5;
          } else {
            frame = skidding ? 4
                             : (fabsf(pl.vx) > 10
                                    ? 1 + ((int)(pl.animT * 10) % 3)
                                    : 0);
          }
          src = {frame * 16, 0, 16, 16};
        } else if (pl.power == P_FIRE) {
          int fireFrame = 0;
          if (pl.throwT > 0.0f) {
            fireFrame = pl.ground ? 7 : 8;
          } else if (pl.crouch) {
            fireFrame = 1;
          } else if (inWaterDraw) {
            fireFrame = swimStroke ? (2 + ((int)(pl.swimAnimT * 14.0f) % 3)) : 6;
          } else if (!pl.ground) {
            fireFrame = 6;
          } else if (skidding) {
            fireFrame = 5;
          } else if (fabsf(pl.vx) > 10) {
            fireFrame = 2 + ((int)(pl.animT * 10) % 3);
          } else {
            fireFrame = 0;
          }
          frame = fireFrame;
          src = {frame * 16, 0, 16, 32};
        } else {
          // Big sheet: 0 idle, 1 crouch, 2..4 walk, 5 skid, 6 jump
          if (pl.crouch) {
            frame = 1;
          } else if (inWaterDraw) {
            frame = swimStroke ? (2 + ((int)(pl.swimAnimT * 14.0f) % 3)) : 6;
          } else if (!pl.ground) {
            frame = 6;
          } else {
            frame = skidding ? 5
                             : (fabsf(pl.vx) > 10
                                    ? 2 + ((int)(pl.animT * 10) % 3)
                                    : 0);
          }
          src = {frame * 16, 0, 16, 32};
        }

        SDL_RendererFlip flip = pl.right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        if (pl.power == P_SMALL) {
          renderCopyExWithShadow(tex, &src, &dst, flip);
        } else {
          renderTall16x32WithShadow(tex, frame * 16, dst, flip);
        }
      } else {
        SDL_SetRenderDrawColor(g_ren, 228, 52, 52, 255);
        SDL_RenderFillRect(g_ren, &dst);
      }
    }
	    // Castle overlay: hides the player as they enter the door.
	    if (g_castleDrawOn && g_texCastle && g_castleOverlayDst.w > 0 &&
	        g_castleOverlayDst.h > 0) {
	      constexpr int kOverlayY = 120;
	      constexpr int kOverlayX = 32;
	      SDL_Rect src = {kOverlayX, kOverlayY, g_castleOverlayDst.w,
	                      g_castleOverlayDst.h};
	      SDL_RenderCopy(g_ren, g_texCastle, &src, &g_castleOverlayDst);
	    }
	    if (g_nightMode) {
	    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
	    SDL_SetRenderDrawColor(g_ren, 0, 0, 40, 100);
	    SDL_Rect night = {0, 0, GAME_W, GAME_H};
    SDL_RenderFillRect(g_ren, &night);
  }

  // Ambient particles: drawn above gameplay, below the cloud overlay + HUD.
  renderAmbientParticles();

  // Cloud overlay: should sit in front of everything except the HUD.
  // Only enabled when the level asks for it (or Overworld by default).
  {
    bool wantsClouds = g_levelInfo.bgClouds || (g_theme == THEME_OVERWORLD);
    if (wantsClouds && g_texBgCloudOverlay) {
      SDL_SetTextureAlphaMod(g_texBgCloudOverlay, 110);
      // Render only a visible slice of the overlay so it stays aligned to the
      // viewport and doesn't clip into ground. Shift the sample down so the
      // clouds read higher in the view.
      constexpr int kCloudSrcY = 250;
      SDL_Rect src = {0, kCloudSrcY, 512, GAME_H};
      // Fore-foreground layer: move *faster* than the world when the camera
      // pans, plus a slow independent drift.
      constexpr float kCloudPanMul = 1.08f;
      constexpr float kCloudDrift = 0.0012f;
      int drift = (int)(SDL_GetTicks() * kCloudDrift);
      int off = (int)(g_camX * kCloudPanMul) + drift;
      int x0 = -(off % 512);
      SDL_Rect dst = {x0, 0, 512, GAME_H};
      for (int x = dst.x; x < GAME_W; x += 512) {
        SDL_Rect d = {x, dst.y, dst.w, dst.h};
        SDL_RenderCopy(g_ren, g_texBgCloudOverlay, &src, &d);
      }
      SDL_SetTextureAlphaMod(g_texBgCloudOverlay, 255);
    }
  }

  // HUD (SMB-style, drawn directly on the sky)
  {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {255, 255, 0, 255};
    char buf[64];
    int scale = 1;
    int y1 = 6;
    int y2 = 16;
    int margin = 8;

    // Player + score (left)
    drawTextShadow(margin, y1, g_charDisplayNames[g_charIndex], scale, white);
    snprintf(buf, sizeof(buf), "%06d", g_p.score % 1000000);
    drawTextShadow(margin, y2, buf, scale, white);

	    // Coin count (upper middle-left)
	    int coinX = margin + 110;
	    if (g_texCoinIcon) {
	      SDL_Rect src = {0, 0, 8, 8};
	      SDL_Rect dst = {coinX, y1 - 1, 12, 12};
	      SDL_RenderCopy(g_ren, g_texCoinIcon, &src, &dst);
	    }
    snprintf(buf, sizeof(buf), "x%02d", g_p.coins % 100);
    drawTextShadow(coinX + 14, y1, buf, scale, yellow);

    // World (center)
    const char *worldLabel = "WORLD";
    snprintf(buf, sizeof(buf), "%d-%d", g_levelInfo.world, g_levelInfo.stage);
    int worldX = (GAME_W - textWidth(worldLabel, scale)) / 2;
    drawTextShadow(worldX, y1, worldLabel, scale, white);
    int worldValX = (GAME_W - textWidth(buf, scale)) / 2;
    drawTextShadow(worldValX, y2, buf, scale, white);

    // Time (upper right)
    const char *timeLabel = "TIME";
    snprintf(buf, sizeof(buf), "%03d", g_time < 0 ? 0 : g_time);

    int lifeSize = 12;
    // Reserve space for a compact lives column at the far right.
    int livesNumW = textWidth("x00", scale);
    int pLabelW = textWidth("P4", scale);
    int livesBlockW = pLabelW + 2 + lifeSize + 2 + livesNumW;

    int timeRight = GAME_W - margin - livesBlockW - 10;
    drawTextShadow(timeRight - textWidth(timeLabel, scale), y1, timeLabel, scale,
                   white);
    drawTextShadow(timeRight - textWidth(buf, scale), y2, buf, scale, white);

    // Lives for all active players (far right)
    int baseX = GAME_W - margin - livesBlockW;
    int baseY = y2 - 1;
    for (int pi = 0; pi < g_playerCount; pi++) {
      Player &pl = g_players[pi];
      int rowY = baseY + pi * 14;
      char pLabel[8];
      snprintf(pLabel, sizeof(pLabel), "P%d", pi + 1);
      drawTextShadow(baseX, rowY + 1, pLabel, scale, white);

      int ci = g_playerCharIndex[pi] % g_charCount;
      if (g_texLifeIcon[ci]) {
        SDL_Rect dst = {baseX + pLabelW + 2, rowY, lifeSize, lifeSize};
        SDL_RenderCopy(g_ren, g_texLifeIcon[ci], nullptr, &dst);
      }
      char livesBuf[16];
      snprintf(livesBuf, sizeof(livesBuf), "x%02d", pl.lives < 0 ? 0 : pl.lives);
      drawTextShadow(baseX + pLabelW + 2 + lifeSize + 2, rowY + 1, livesBuf,
                     scale, white);
    }
  }

  if (g_state == GS_DEAD || g_state == GS_GAMEOVER) {
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 200);
    SDL_Rect overlay = {GAME_W / 4, GAME_H / 3, GAME_W / 2, GAME_H / 3};
    SDL_RenderFillRect(g_ren, &overlay);

    const char *title = (g_state == GS_GAMEOVER) ? "GAME OVER" : "YOU DIED";
    drawTextShadow((GAME_W - textWidth(title, 2)) / 2, overlay.y + 22, title, 2,
                   {255, 255, 255, 255});
    const char *hint = "PRESS A";
    drawTextShadow((GAME_W - textWidth(hint, 1)) / 2, overlay.y + overlay.h - 28,
                   hint, 1, {200, 200, 200, 255});
  }
  if (g_state == GS_WIN) {
    SDL_SetRenderDrawColor(g_ren, 0, 100, 0, 200);
    SDL_Rect overlay = {GAME_W / 4, GAME_H / 3, GAME_W / 2, GAME_H / 3};
    SDL_RenderFillRect(g_ren, &overlay);

    const char *title = "COURSE CLEAR";
    drawTextShadow((GAME_W - textWidth(title, 2)) / 2, overlay.y + 22, title, 2,
                   {255, 255, 255, 255});
    char buf[32];
    snprintf(buf, sizeof(buf), "WORLD %d-%d", g_levelInfo.world, g_levelInfo.stage);
    drawTextShadow((GAME_W - textWidth(buf, 1)) / 2, overlay.y + 56, buf, 1,
                   {255, 255, 255, 255});
  }
  if (g_state == GS_PAUSE) {
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 200);
    SDL_Rect panel = {GAME_W / 2 - 140, GAME_H / 2 - 70, 280, 140};
    SDL_RenderFillRect(g_ren, &panel);
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
    SDL_RenderDrawRect(g_ren, &panel);

    const char *title = "PAUSED";
    int titleY = panel.y + 16;
    drawTextShadow((GAME_W - textWidth(title, 2)) / 2, titleY, title, 2,
                   {255, 255, 255, 255});

    int menuY = titleY + 28;
    for (int i = 0; i < g_pauseOptionCount; i++) {
      SDL_Color c = (i == g_pauseIndex) ? SDL_Color{255, 255, 0, 255}
                                       : SDL_Color{255, 255, 255, 255};
      drawTextShadow((GAME_W - textWidth(g_pauseOptions[i], 1)) / 2,
                     menuY + i * 14, g_pauseOptions[i], 1, c);
    }
    drawTextShadow((GAME_W - textWidth("PRESS + TO RESUME", 1)) / 2,
                   panel.y + panel.h - 16, "PRESS + TO RESUME", 1,
                   {200, 200, 200, 255});
  }

  SDL_RenderPresent(g_ren);
}

int main(int argc, char **argv) {
  WHBProcInit();
  VPADInit();
  KPADInit();
  WPADEnableURCC(TRUE);
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  IMG_Init(IMG_INIT_PNG);
  Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
  Mix_AllocateChannels(32);
  Mix_ReserveChannels(0);
  srand((unsigned)SDL_GetTicks());

  g_win = SDL_CreateWindow("SMB", 0, 0, TV_W, TV_H, SDL_WINDOW_FULLSCREEN);
  g_ren = SDL_CreateRenderer(
      g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(g_ren, GAME_W, GAME_H);

  loadAssets();
  g_state = GS_TITLE;
  g_titleMode = TITLE_MAIN;
  g_mainMenuIndex = 0;
  g_menuIndex = g_charIndex;

  Uint32 lastTick = SDL_GetTicks();
  while (WHBProcIsRunning()) {
    Uint32 now = SDL_GetTicks();
    float dt = (now - lastTick) / 1000.0f;
    lastTick = now;
    if (dt > 0.1f)
      dt = 0.1f;

    input();

    if (g_pressed & VPAD_BUTTON_PLUS) {
      if (g_state == GS_PLAYING)
        g_state = GS_PAUSE;
      else if (g_state == GS_PAUSE)
        g_state = GS_PLAYING;
      if (g_state == GS_PAUSE)
        g_pauseIndex = 0;
    }

    // Cycle theme/level (demo): press MINUS to switch tilesets.
    if (g_pressed & VPAD_BUTTON_MINUS) {
      g_theme = (LevelTheme)(((int)g_theme + 1) % 3);
      loadThemeTilesets();
      setupLevel();
    }

    if (g_state == GS_TITLE) {
      updateTitle();
    } else if (g_state == GS_PAUSE) {
      updatePauseMenu();
    } else if (g_state == GS_PLAYING) {
      updatePlatformsAndGenerators(dt);
      updatePlayers(dt);
      updateEntities(dt);
      updateAmbientParticles(dt);
      g_timeAcc += dt;
      if (g_timeAcc >= 1.0f) {
        g_timeAcc -= 1.0f;
        g_time--;
        if (g_time <= 0) {
          g_p.dead = true;
          g_p.lives--;
          g_state = GS_DEAD;
        }
      }
    } else if (g_state == GS_FLAG) {
      updateFlagSequence(dt);
      updateAmbientParticles(dt);
      updateCameraFromLeader();
      enforceNonLeaderPlayersInView();
    } else if (g_state == GS_DEAD) {
      if (g_pressed & VPAD_BUTTON_A) {
        if (g_p.lives > 0)
          restartLevel();
        else
          g_state = GS_GAMEOVER;
      }
    } else if (g_state == GS_WIN) {
      g_levelTimer += dt;
      if (g_levelTimer > 2.0f) {
        nextLevel();
      }
    } else if (g_state == GS_GAMEOVER) {
      if (g_pressed & VPAD_BUTTON_A)
        startNewGame();
    }

    render();
  }

  Mix_CloseAudio();
  SDL_DestroyRenderer(g_ren);
  SDL_DestroyWindow(g_win);
  IMG_Quit();
  SDL_Quit();
  KPADShutdown();
  WHBProcShutdown();
  return 0;
}
