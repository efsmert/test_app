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
constexpr float PLAYER_HIT_H_SMALL = 15.0f; // 1-tile gaps (16px) are enterable
constexpr float PLAYER_HIT_W_BIG = 15.0f;
constexpr float PLAYER_HIT_H_BIG = 31.0f;   // 2-tile gaps (32px) are enterable
enum GameState { GS_TITLE, GS_PLAYING, GS_FLAG, GS_DEAD, GS_GAMEOVER, GS_WIN, GS_PAUSE };
constexpr uint8_t QMETA_COIN = 0;
constexpr uint8_t QMETA_POWERUP = 1;
constexpr uint8_t QMETA_ONEUP = 2;
constexpr uint8_t QMETA_STAR = 3;
enum TitleMode { TITLE_MAIN = 0, TITLE_CHAR_SELECT = 1, TITLE_OPTIONS = 2, TITLE_EXTRAS = 3 };
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
  bool dead;
};

static uint8_t g_map[MAP_H][MAP_W];
static Entity g_ents[64];
static Player g_p;
static float g_camX = 0;
static GameState g_state = GS_PLAYING;
static uint32_t g_held = 0, g_pressed = 0;
static int g_loadedTex = 0;
static int g_time = 400;
static float g_timeAcc = 0;
static float g_flagY = 0; // Flag sliding position
static int g_flagX = 0;
static bool g_hasFlag = false;
static int g_sectionIndex = 0;
static LevelSectionRuntime g_levelInfo = {};

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
static SDL_Texture *g_texFlagPole = nullptr;
static SDL_Texture *g_texFlag = nullptr;
static SDL_Texture *g_texCastle = nullptr;
static SDL_Rect g_castleDrawDst = {0, 0, 0, 0};
static bool g_castleDrawOn = false;
static LevelTheme g_theme = THEME_OVERWORLD;
static const char *g_charNames[] = {"Luigi", "Toad", "Toadette"};
static const char *g_charDisplayNames[] = {"LUIGI", "TOAD", "TOADETTE"};
static const int g_charCount = 3;
static int g_charIndex = 0;
static int g_menuIndex = 0;
static int g_titleMode = TITLE_MAIN;
static int g_optionsIndex = 0;
static int g_mainMenuIndex = 0;
static int g_pauseIndex = 0;
static const char *g_pauseOptions[] = {"RESUME", "NEXT LEVEL", "PREVIOUS LEVEL",
                                       "CYCLE THEME", "MAIN MENU"};
static const int g_pauseOptionCount = 5;
static int g_levelIndex = 0;
static float g_levelTimer = 0.0f;
static bool g_flagSfxPlayed = false;
static bool g_castleSfxPlayed = false;
static float g_fireCooldown = 0.0f;
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

inline bool jumpPressed() {
  return (g_pressed & (VPAD_BUTTON_A | VPAD_BUTTON_B)) != 0;
}

inline bool jumpHeld() {
  return (g_held & (VPAD_BUTTON_A | VPAD_BUTTON_B)) != 0;
}

inline bool firePressed() {
  return (g_pressed & (VPAD_BUTTON_X | VPAD_BUTTON_Y)) != 0;
}

int mapWidth() { return g_levelInfo.mapWidth > 0 ? g_levelInfo.mapWidth : MAP_W; }

bool solidAt(int tx, int ty);
uint8_t collisionAt(int tx, int ty);

float standHeightForPower(Power p) {
  return (p >= P_BIG) ? PLAYER_HIT_H_BIG : PLAYER_HIT_H_SMALL;
}

void setPlayerSizePreserveFeet(float newW, float newH) {
  float footY = g_p.r.y + g_p.r.h;
  g_p.r.w = newW;
  g_p.r.h = newH;
  g_p.r.y = footY - newH;
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

bool playerStomp(const Rect &enemy) {
  return g_p.vy > 0.0f;
}

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

  // Prefer PNG alpha when present; fall back to bright-green chroma key for
  // legacy placeholder sheets.
  if (s->format->Amask == 0 ||
      (strstr(file, "tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/ui/TitleSMB1.png") != nullptr) ||
      (strstr(file, "sprites/ui/CoinIcon.png") != nullptr) ||
      (strstr(file, "FlagPole.png") != nullptr) ||
      (strstr(file, "KoopaTroopaSheet.png") != nullptr) ||
      (strstr(file, "QuestionBlock.png") != nullptr) ||
      (strstr(file, "FireFlower.png") != nullptr) ||
      (strstr(file, "Fireball.png") != nullptr) ||
      (strstr(file, "SpinningCoin.png") != nullptr)) {
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

  // Apply the same chroma-key rules as loadTex().
  if (s->format->Amask == 0 ||
      (strstr(file, "tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/tilesets/Deco/") != nullptr) ||
      (strstr(file, "sprites/ui/TitleSMB1.png") != nullptr) ||
      (strstr(file, "sprites/ui/CoinIcon.png") != nullptr) ||
      (strstr(file, "FlagPole.png") != nullptr) ||
      (strstr(file, "KoopaTroopaSheet.png") != nullptr) ||
      (strstr(file, "QuestionBlock.png") != nullptr) ||
      (strstr(file, "FireFlower.png") != nullptr) ||
      (strstr(file, "Fireball.png") != nullptr) ||
      (strstr(file, "SpinningCoin.png") != nullptr)) {
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
    return "Pipeland";
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

  char path[512];
  auto tryLoadBg = [&](const char *dir, const char *file) -> SDL_Texture * {
    // Cap `file` to avoid -Wformat-truncation warnings (our filenames are tiny).
    snprintf(path, sizeof(path), "sprites/Backgrounds/%s/%.*s", dir, 400, file);
    return loadTex(path);
  };
  auto tryLoadBgName = [&](const char *dir, const char *base,
                           bool nightMode) -> SDL_Texture * {
    // Prefer LL variants; many non-LL files are chroma-key sources (green).
    if (nightMode) {
      // Some underwater assets use ...LLNight instead of ...NightLL.
      snprintf(path, sizeof(path), "%sNightLL.png", base);
      if (auto *t = tryLoadBg(dir, path))
        return t;
      snprintf(path, sizeof(path), "%sLLNight.png", base);
      if (auto *t = tryLoadBg(dir, path))
        return t;
      snprintf(path, sizeof(path), "%sNight.png", base);
      if (auto *t = tryLoadBg(dir, path))
        return t;
    }
    snprintf(path, sizeof(path), "%sLL.png", base);
    if (auto *t = tryLoadBg(dir, path))
      return t;
    snprintf(path, sizeof(path), "%s.png", base);
    return tryLoadBg(dir, path);
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
  int secondary = g_levelInfo.bgSecondary;
  if (secondary == 0 && g_theme == THEME_OVERWORLD) {
    // SMB1-style overworld generally benefits from the extra mid layer.
    secondary = 2;
  }

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
  g_texQuestion = loadTex("sprites/blocks/QuestionBlock.png");
  g_texMushroom = loadTex("sprites/items/SuperMushroom.png");
  g_texFireFlower = loadTex("sprites/items/FireFlower.png");
  g_texFireball = loadTex("sprites/items/Fireball.png");
  g_texCoin = loadTex("sprites/items/SpinningCoin.png");
  g_texFlagPole = loadTex("sprites/tilesets/FlagPole.png");
  g_texFlag = loadTex("sprites/tilesets/Flag.png");
  g_texCastle = loadTex("sprites/tilesets/EndingCastleSprite.png");
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
    g_ents[idx].on = true;
    g_ents[idx].type = s.type;
    float h = 16.f;
    float y = s.y - (h - 16.0f);
    g_ents[idx].r = {s.x, y, 16.f, h};
    g_ents[idx].vx = (s.type == E_GOOMBA || s.type == E_KOOPA)
                         ? -Physics::ENEMY_SPEED
                         : 0;
    g_ents[idx].vy = 0;
    g_ents[idx].dir = s.dir;
    g_ents[idx].state = 0;
    g_ents[idx].timer = 0;
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

  g_p.dead = false;
  g_p.r = {(float)spawnX, (float)(spawnY - g_p.r.h), g_p.r.w, g_p.r.h};
  g_p.vx = 0;
  g_p.vy = 0;
  g_p.ground = false;
  g_p.right = true;
  g_p.jumping = false;
  g_p.crouch = false;
  g_p.invT = 0;
  g_p.animT = 0;
  g_p.throwT = 0;
  g_fireCooldown = 0.0f;
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
  g_p.lives = 3;
  g_p.coins = 0;
  g_p.score = 0;
  g_p.power = P_SMALL;
  g_p.r.w = PLAYER_HIT_W_SMALL;
  g_p.r.h = PLAYER_HIT_H_SMALL;
  g_p.crouch = false;
  setupLevel();
}

void restartLevel() {
  g_p.power = P_SMALL;
  g_p.r.w = PLAYER_HIT_W_SMALL;
  g_p.r.h = PLAYER_HIT_H_SMALL;
  g_p.crouch = false;
  setupLevel();
}

void nextLevel() {
  g_levelIndex++;
  if (g_levelIndex >= levelCount())
    g_levelIndex = 0;
  g_sectionIndex = 0;
  setupLevel();
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
  SDL_Event e;
  while (SDL_PollEvent(&e))
    if (e.type == SDL_QUIT)
      WHBProcStopRunning();
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
        g_titleMode = TITLE_CHAR_SELECT;
        g_menuIndex = g_charIndex;
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
      g_charIndex = g_menuIndex;
      startNewGame();
    }
    break;
  }
  case TITLE_OPTIONS: {
    if (g_pressed & VPAD_BUTTON_UP) {
      g_optionsIndex = (g_optionsIndex + 2 - 1) % 2;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    } else if (g_pressed & VPAD_BUTTON_DOWN) {
      g_optionsIndex = (g_optionsIndex + 1) % 2;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }

    if (g_pressed & VPAD_BUTTON_A) {
      if (g_optionsIndex == 0) {
        g_randomTheme = !g_randomTheme;
        if (g_randomTheme)
          g_themeOverride = -1;
      } else if (g_optionsIndex == 1) {
        g_nightMode = !g_nightMode;
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

void spawnFireball() {
  if (g_fireCooldown > 0.0f)
    return;
  for (int i = 0; i < 64; i++) {
    if (!g_ents[i].on) {
      float x = g_p.r.x + (g_p.right ? g_p.r.w - 4 : -4);
      float y = g_p.r.y + (g_p.r.h * 0.5f);
      g_ents[i] = {true, E_FIREBALL, {x, y, 16, 16}, 0, -90, g_p.right ? 1 : -1, 0, 0};
      g_fireCooldown = 0.35f;
      g_p.throwT = 0.15f;
      if (g_sfxFireball)
        Mix_PlayChannel(-1, g_sfxFireball, 0);
      break;
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

void updatePlayer(float dt) {
  if (g_p.dead || g_state == GS_FLAG)
    return;
  for (auto &b : g_tileBumps) {
    if (b.t > 0.0f) {
      b.t -= dt;
      if (b.t < 0.0f)
        b.t = 0.0f;
    }
  }
  if (g_p.invT > 0)
    g_p.invT -= dt;
  if (g_p.throwT > 0.0f) {
    g_p.throwT -= dt;
    if (g_p.throwT < 0.0f)
      g_p.throwT = 0.0f;
  }
  if (g_fireCooldown > 0.0f)
    g_fireCooldown -= dt;
  g_p.animT += dt;
  if (g_skidCooldown > 0)
    g_skidCooldown -= dt;

  float dir = 0;
  bool downHeld = (g_held & VPAD_BUTTON_DOWN);
  if (g_held & VPAD_BUTTON_RIGHT) {
    dir = 1;
    g_p.right = true;
  } else if (g_held & VPAD_BUTTON_LEFT) {
    dir = -1;
    g_p.right = false;
  }

  if (g_p.power == P_FIRE && firePressed() && !g_p.crouch) {
    spawnFireball();
  }

  if (g_levelInfo.pipes && g_levelInfo.pipeCount > 0) {
    for (int i = 0; i < g_levelInfo.pipeCount; i++) {
      const PipeLink &p = g_levelInfo.pipes[i];
      // Use the PipeArea node location as the "mouth" area. This avoids
      // depending on tile classification (pipes use many atlas tiles) and fixes
      // sideways pipes that don't form a simple vertical column.
      float px = p.x * TILE;
      float py = p.y * TILE;
      SDL_FRect mouth = {px, py, TILE * 2.0f, TILE * 2.0f};

      float midX = g_p.r.x + g_p.r.w * 0.5f;
      float midY = g_p.r.y + g_p.r.h * 0.5f;
      bool overlapY = (midY >= mouth.y - 8.0f && midY <= mouth.y + mouth.h + 8.0f);
      bool overMouthX = (midX >= mouth.x + 2 && midX <= mouth.x + mouth.w - 2);
      constexpr float kEdgeEps = 6.0f;

      switch (p.enterDir) {
      case 0: // Down
        if (downHeld && g_p.ground && overMouthX &&
            fabsf((g_p.r.y + g_p.r.h) - mouth.y) <= 8.0f) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      case 1: // Up
        if ((g_held & VPAD_BUTTON_UP) && overMouthX &&
            fabsf(g_p.r.y - (mouth.y + mouth.h)) <= 8.0f) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      case 2: // Left
        if ((g_held & VPAD_BUTTON_LEFT) && g_p.ground && overlapY &&
            fabsf(g_p.r.x - (mouth.x + mouth.w)) <= kEdgeEps) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      case 3: // Right
        if ((g_held & VPAD_BUTTON_RIGHT) && g_p.ground && overlapY &&
            fabsf((g_p.r.x + g_p.r.w) - mouth.x) <= kEdgeEps) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      default:
        break;
      }
    }
  }

  if (g_p.power >= P_BIG && g_p.ground) {
    if (downHeld && !g_p.crouch) {
      g_p.crouch = true;
      setPlayerSizePreserveFeet(PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
    } else if (!downHeld && g_p.crouch) {
      // Only stand up if there's headroom.
      Rect standRect = g_p.r;
      standRect.h = standHeightForPower(g_p.power);
      standRect.y = (g_p.r.y + g_p.r.h) - standRect.h;
      standRect.w = PLAYER_HIT_W_BIG;
      if (!rectBlockedBySolids(standRect)) {
        g_p.crouch = false;
        setPlayerSizePreserveFeet(PLAYER_HIT_W_BIG, standRect.h);
      }
    }
  } else if (g_p.crouch) {
    // If we left the ground while crouched, try to stand (but don't force it if
    // we'd clip).
    Rect standRect = g_p.r;
    standRect.h = standHeightForPower(g_p.power);
    standRect.y = (g_p.r.y + g_p.r.h) - standRect.h;
    standRect.w = PLAYER_HIT_W_BIG;
    if (!rectBlockedBySolids(standRect)) {
      g_p.crouch = false;
      setPlayerSizePreserveFeet(PLAYER_HIT_W_BIG, standRect.h);
    }
  }

  if (g_p.crouch) {
    dir = 0;
  }

  float maxSpd =
      (g_held & VPAD_BUTTON_Y) ? Physics::RUN_SPEED : Physics::WALK_SPEED;
  float accel = g_p.ground ? Physics::GROUND_ACCEL : Physics::AIR_ACCEL;
  float decel = g_p.ground ? Physics::DECEL : Physics::AIR_ACCEL;
  if (g_p.crouch && g_p.ground) {
    maxSpd = 0;
    accel = 0;
    decel = Physics::DECEL * 4.0f;
  }
  if (dir != 0) {
    g_p.vx += dir * accel;
    if (fabsf(g_p.vx) > maxSpd) {
      // If we just released run, ease back toward walk speed.
      if (!(g_held & VPAD_BUTTON_Y)) {
        g_p.vx -= dir * decel;
        if (fabsf(g_p.vx) < maxSpd)
          g_p.vx = dir * maxSpd;
      } else {
        g_p.vx = dir * maxSpd;
      }
    }
  } else {
    if (g_p.vx > 0) {
      g_p.vx = fmaxf(0.0f, g_p.vx - decel);
    }
    if (g_p.vx < 0) {
      g_p.vx = fminf(0.0f, g_p.vx + decel);
    }
  }

  if (jumpPressed() && g_p.ground) {
    float jumpHeight = Physics::JUMP_HEIGHT;
    if (fabsf(g_p.vx) > Physics::WALK_SPEED * 0.9f)
      jumpHeight *= 1.15f;
    g_p.vy = -jumpHeight;
    g_p.jumping = true;
    g_p.ground = false;
    if (g_p.power >= P_BIG) {
      if (g_sfxBigJump)
        Mix_PlayChannel(-1, g_sfxBigJump, 0);
      else if (g_sfxJump)
        Mix_PlayChannel(-1, g_sfxJump, 0);
    } else {
      if (g_sfxJump)
        Mix_PlayChannel(-1, g_sfxJump, 0);
    }
  }
  if (!jumpHeld() && g_p.vy < -100)
    g_p.vy = -100;

  g_p.vy += (g_p.vy < 0 && g_p.jumping) ? Physics::JUMP_GRAVITY
                                        : Physics::FALL_GRAVITY;
  if (g_p.vy > Physics::MAX_FALL_SPEED)
    g_p.vy = Physics::MAX_FALL_SPEED;

  float moveX = g_p.vx * dt;
  int stepsX = (int)ceilf(fabsf(moveX) / 6.0f);
  if (stepsX < 1)
    stepsX = 1;
  float stepX = moveX / stepsX;
  for (int i = 0; i < stepsX; i++) {
    g_p.r.x += stepX;
    if (g_p.r.x < g_camX)
      g_p.r.x = g_camX;

    // X collision
    int ty1 = (int)(g_p.r.y / TILE),
        ty2 = (int)((g_p.r.y + g_p.r.h - 1) / TILE);
    int tx1 = (int)(g_p.r.x / TILE),
        tx2 = (int)((g_p.r.x + g_p.r.w) / TILE);
    bool hit = false;
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        if (solidAt(tx, ty)) {
          if (stepX > 0)
            g_p.r.x = tx * TILE - g_p.r.w;
          else if (stepX < 0)
            g_p.r.x = (tx + 1) * TILE;
          g_p.vx = 0;
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

  float moveY = g_p.vy * dt;
  int stepsY = (int)ceilf(fabsf(moveY) / 6.0f);
  if (stepsY < 1)
    stepsY = 1;
  float stepY = moveY / stepsY;
  g_p.ground = false;
  for (int i = 0; i < stepsY; i++) {
    g_p.r.y += stepY;
    // Y collision (per step)
    int ty1 = (int)(g_p.r.y / TILE);
    // When moving down, include the tile boundary at the player's feet to avoid
    // "ground" flicker when landing exactly on an edge.
    int ty2 = (stepY >= 0) ? (int)((g_p.r.y + g_p.r.h) / TILE)
                           : (int)((g_p.r.y + g_p.r.h - 1) / TILE);
    int tx1 = (int)(g_p.r.x / TILE);
    int tx2 = (int)((g_p.r.x + g_p.r.w - 1) / TILE);
    bool hit = false;
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        uint8_t col = collisionAt(tx, ty);
        if (col == COL_NONE)
          continue;
        if (g_p.vy > 0) {
          if (col == COL_ONEWAY) {
            float prevBottom = (g_p.r.y - stepY) + g_p.r.h;
            float tileTop = ty * TILE;
            if (prevBottom > tileTop + 0.1f)
              continue;
          }
          g_p.r.y = ty * TILE - g_p.r.h;
          g_p.vy = 0;
          g_p.ground = true;
          g_p.jumping = false;
          hit = true;
	        } else if (g_p.vy < 0) {
	          if (col != COL_SOLID)
	            continue;
	          int hitTy = ty;
	          g_p.r.y = (hitTy + 1) * TILE;
	          g_p.vy = 45;

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
	                if (g_p.power == P_SMALL)
	                  spawnMushroom(bx, by);
	                else
	                  spawnFireFlower(bx, by);
	              } else if (meta == QMETA_ONEUP) {
	                g_p.lives++;
	                g_p.score += 1000;
	                if (g_sfxPowerup)
	                  Mix_PlayChannel(-1, g_sfxPowerup, 0);
	              } else {
	                g_p.coins++;
	                g_p.score += 200;
	                spawnCoinPopup(bx * TILE, by * TILE);
	              }
	              if (g_sfxBump)
	                Mix_PlayChannel(-1, g_sfxBump, 0);
	              return;
	            }
	            if (t == T_BRICK && g_p.power > P_SMALL) {
	              g_map[by][bx] = T_EMPTY;
	              g_p.score += 50;
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

	          int leftTx = (int)((g_p.r.x + 1) / TILE);
	          int rightTx = (int)((g_p.r.x + g_p.r.w - 2) / TILE);
	          float midX = g_p.r.x + g_p.r.w * 0.5f;
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
    if (hit)
      break;
  }

  // Collect coins embedded in the tilemap.
  {
    int tx1 = (int)(g_p.r.x / TILE);
    int tx2 = (int)((g_p.r.x + g_p.r.w - 1) / TILE);
    int ty1 = (int)(g_p.r.y / TILE);
    int ty2 = (int)((g_p.r.y + g_p.r.h - 1) / TILE);
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        if (tx < 0 || tx >= mapWidth() || ty < 0 || ty >= MAP_H)
          continue;
        if (g_map[ty][tx] == T_COIN) {
          g_map[ty][tx] = T_EMPTY;
          g_p.coins++;
          g_p.score += 200;
          if (g_sfxCoin)
            Mix_PlayChannel(-1, g_sfxCoin, 0);
        }
      }
    }
  }

  // Flag pole collision
  int flagTx = (int)((g_p.r.x + g_p.r.w / 2) / TILE);
  if (g_hasFlag && flagTx == g_flagX && g_state == GS_PLAYING) {
    g_state = GS_FLAG;
    g_p.vx = 0;
    g_p.vy = 0;
    int height = 12 - (int)(g_p.r.y / TILE);
    g_p.score += height * 100;
    if (!g_flagSfxPlayed && g_sfxFlagSlide) {
      Mix_PlayChannel(-1, g_sfxFlagSlide, 0);
      g_flagSfxPlayed = true;
    }
    Mix_HaltMusic();
  }

  // Pit death
  if (g_p.r.y > GAME_H + 32) {
    g_p.dead = true;
    g_p.lives--;
    g_state = GS_DEAD;
    Mix_HaltMusic();
  }

  // Camera
  float target = g_p.r.x - GAME_W / 3.0f;
  if (target > g_camX)
    g_camX = target;
  int viewTiles = (GAME_W + TILE - 1) / TILE;
  float maxCam = (mapWidth() - viewTiles) * TILE;
  if (maxCam < 0)
    maxCam = 0;
  if (g_camX > maxCam)
    g_camX = maxCam;
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
    if (g_p.r.x > 205 * TILE) {
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

        if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32 &&
            !g_p.dead && overlap(g_p.r, e.r)) {
          if (playerStomp(e.r)) {
            e.state = 1;
            e.timer = 0;
            g_p.vy = jumpHeld() ? -Physics::BOUNCE_HEIGHT * 1.5f
                                : -Physics::BOUNCE_HEIGHT;
            g_p.score += 100;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
            else if (g_sfxKick)
              Mix_PlayChannel(-1, g_sfxKick, 0);
          } else if (g_p.invT <= 0) {
            if (g_p.power > P_SMALL) {
              g_p.power = P_SMALL;
              g_p.crouch = false;
              setPlayerSizePreserveFeet(PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
              g_p.invT = 2.0f;
              if (g_sfxDamage)
                Mix_PlayChannel(-1, g_sfxDamage, 0);
            } else {
              g_p.dead = true;
              g_p.lives--;
              g_state = GS_DEAD;
              Mix_HaltMusic();
            }
          }
        }
      } else if (e.timer > 0.5f)
        e.on = false;
    } else if (e.type == E_KOOPA) {
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

      if (e.r.x > g_camX - 32 && e.r.x < g_camX + GAME_W + 32 &&
          !g_p.dead && overlap(g_p.r, e.r)) {
        bool stomp = playerStomp(e.r);
        if (stomp) {
          g_p.vy = jumpHeld() ? -Physics::BOUNCE_HEIGHT * 1.5f
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
        } else if (g_p.invT <= 0) {
          if (e.state == 1) {
            e.state = 2;
            e.timer = 0;
            e.dir = (g_p.r.x < e.r.x) ? 1 : -1;
            if (g_sfxKick)
              Mix_PlayChannel(-1, g_sfxKick, 0);
          } else if (g_p.power > P_SMALL) {
            g_p.power = P_SMALL;
            g_p.crouch = false;
            setPlayerSizePreserveFeet(PLAYER_HIT_W_SMALL, PLAYER_HIT_H_SMALL);
            g_p.invT = 2.0f;
            if (g_sfxDamage)
              Mix_PlayChannel(-1, g_sfxDamage, 0);
          } else {
            g_p.dead = true;
            g_p.lives--;
            g_state = GS_DEAD;
            Mix_HaltMusic();
          }
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
      if (overlap(g_p.r, e.r)) {
        e.on = false;
        if (g_p.power == P_SMALL) {
          g_p.power = P_BIG;
          g_p.crouch = false;
          setPlayerSizePreserveFeet(PLAYER_HIT_W_BIG, PLAYER_HIT_H_BIG);
        }
        g_p.score += 1000;
        if (g_sfxPowerup)
          Mix_PlayChannel(-1, g_sfxPowerup, 0);
        else if (g_sfxItemAppear)
          Mix_PlayChannel(-1, g_sfxItemAppear, 0);
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
      if (overlap(g_p.r, e.r)) {
        e.on = false;
        if (g_p.power == P_SMALL) {
          g_p.power = P_BIG;
          g_p.crouch = false;
          setPlayerSizePreserveFeet(PLAYER_HIT_W_BIG, PLAYER_HIT_H_BIG);
        } else {
          g_p.power = P_FIRE;
        }
        g_p.score += 1000;
        if (g_sfxPowerup)
          Mix_PlayChannel(-1, g_sfxPowerup, 0);
        else if (g_sfxItemAppear)
          Mix_PlayChannel(-1, g_sfxItemAppear, 0);
      }
    } else if (e.type == E_FIREBALL) {
      const float speed = 200.0f;
      e.r.x += e.dir * speed * dt;
      e.vy += 260.0f * dt;
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
            e.vy = -90.0f;
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
        if (t.type != E_GOOMBA && t.type != E_KOOPA)
          continue;
        if (!overlap(e.r, t.r))
          continue;
        t.on = false;
        e.on = false;
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
    if (e.type == E_KOOPA)
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
      if (!overlap(a.r, b.r))
        continue;
      bool aShell = (a.type == E_KOOPA && a.state == 2);
      bool bShell = (b.type == E_KOOPA && b.state == 2);

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

static bool computeCastleDst(SDL_Rect &outDst) {
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
  int baseY = (maxY + 1) * TILE - texH;
  outDst = {minX * TILE - (int)g_camX, baseY, texW, texH};
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
    } else if (g_titleMode == TITLE_OPTIONS) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 190);
      SDL_Rect oPanel = {52, 58, GAME_W - 104, 130};
      SDL_RenderFillRect(g_ren, &oPanel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &oPanel);

      const char *title = "SETTINGS";
      drawTextShadow((GAME_W - textWidth(title, 2)) / 2, 66, title, 2,
                     {255, 255, 255, 255});

      const char *line1 =
          g_randomTheme ? "RANDOM THEME: ON" : "RANDOM THEME: OFF";
      const char *line2 = g_nightMode ? "NIGHT MODE: ON" : "NIGHT MODE: OFF";
      int baseY = 98;
      SDL_Color hi = {255, 255, 0, 255};
      SDL_Color norm = {255, 255, 255, 255};
      drawTextShadow(70, baseY, line1, 1, g_optionsIndex == 0 ? hi : norm);
      drawTextShadow(70, baseY + 16, line2, 1,
                     g_optionsIndex == 1 ? hi : norm);
      drawTextShadow(70, baseY + 44, "B BACK", 1, {220, 220, 220, 255});
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

    int primary = g_levelInfo.bgPrimary;
	    if (primary == 3) {
	      primary = 0;
	      if ((g_theme == THEME_JUNGLE || g_theme == THEME_AUTUMN) &&
	          ((g_levelInfo.world > 4 && g_levelInfo.world <= 8) || g_nightMode)) {
	        primary = 1;
	      }
	    }
	    // Some overworld sections in the source project mark the primary BG as
	    // "None", but SMB1-style stages look better with hills+bushes by default.
	    if (primary == 2 && g_theme == THEME_OVERWORLD)
	      primary = 0;

    // Match the general SMB1 feel: for "Hills" levels, draw both hills and
    // bushes; for "Bush" levels, draw bushes only. If a section specifies
    // "None", keep the background empty (except optional sky).
    if (primary == 0 && g_texBgHills) {
      // Draw the full texture anchored to the bottom so we don't accidentally
      // crop off tall/variable assets (e.g. overworld hills).
      SDL_Rect src = {0, 0, 512, 512};
      SDL_Rect dst = {-(int)(g_camX * 0.10f) % 512, GAME_H - 512, 512, 512};
      for (int x = dst.x; x < GAME_W; x += 512) {
        SDL_Rect d = {x, dst.y, dst.w, dst.h};
        SDL_RenderCopy(g_ren, g_texBgHills, &src, &d);
      }
    }

    if ((primary == 0 || primary == 1) && g_texBgBushes) {
      SDL_Rect src = {0, 0, 512, 512};
      SDL_Rect dst = {-(int)(g_camX * 0.18f) % 512, GAME_H - 512, 512, 512};
      for (int x = dst.x; x < GAME_W; x += 512) {
        SDL_Rect d = {x, dst.y, dst.w, dst.h};
        SDL_RenderCopy(g_ren, g_texBgBushes, &src, &d);
      }
    }

    // Secondary layer (trees/mushrooms) sits in front of the primary background
    // but still behind gameplay tiles/entities.
    int secondary = g_levelInfo.bgSecondary;
    if (secondary == 0 && g_theme == THEME_OVERWORLD) {
      // Default the classic SMB1 overworld tree layer on, even if the source
      // section doesn't explicitly request it.
      secondary = 2;
    }
    if (secondary > 0 && g_texBgSecondary) {
      SDL_Rect src = {0, 0, 512, 512};
      SDL_Rect dst = {-(int)(g_camX * 0.32f) % 512, GAME_H - 512, 512, 512};
      for (int x = dst.x; x < GAME_W; x += 512) {
        SDL_Rect d = {x, dst.y, dst.w, dst.h};
        SDL_RenderCopy(g_ren, g_texBgSecondary, &src, &d);
      }
    }

    // Clouds are subtle and look good on most outdoor levels; default them on
    // for overworld even if the source section doesn't request them.
    bool wantsClouds = g_levelInfo.bgClouds || (g_theme == THEME_OVERWORLD);
    if (wantsClouds && g_texBgCloudOverlay) {
      SDL_SetTextureAlphaMod(g_texBgCloudOverlay, 200);
      SDL_Rect src = {0, 0, 512, 512};
      SDL_Rect dst = {-(int)(g_camX * 0.06f) % 512, -40, 512, 512};
      for (int x = dst.x; x < GAME_W; x += 512) {
        SDL_Rect d = {x, dst.y, dst.w, dst.h};
        SDL_RenderCopy(g_ren, g_texBgCloudOverlay, &src, &d);
      }
      SDL_SetTextureAlphaMod(g_texBgCloudOverlay, 255);
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
	      int groundY = MAP_H - 1;
	      for (int y = MAP_H - 1; y >= 0; y--) {
	        if (solidAt(g_flagX, y) || solidAt(g_flagX + 1, y)) {
	          groundY = y;
	          break;
	        }
	      }
	      int poleX = g_flagX * TILE - (int)g_camX;
	      int poleBottom = (groundY + 1) * TILE;

	      if (g_texFlagPole) {
	        int texW = 0, texH = 0;
	        SDL_QueryTexture(g_texFlagPole, nullptr, nullptr, &texW, &texH);
	        SDL_Rect src = {0, 0, 16, texH};
	        SDL_Rect dst = {poleX, poleBottom - texH, 16, texH};
	        renderCopyWithShadow(g_texFlagPole, &src, &dst);
	      }
	      if (g_texFlag) {
	        int frame = ((SDL_GetTicks() / 150) % 3);
	        SDL_Rect src = {frame * 16, 0, 16, 16};
	        SDL_Rect dst = {poleX - 16, (int)g_flagY, 16, 16};
	        renderCopyWithShadow(g_texFlag, &src, &dst);
	      }
	    }

	    // Castle
	    g_castleDrawOn = computeCastleDst(g_castleDrawDst);
	    if (g_castleDrawOn) {
	      int topH = (g_castleDrawDst.h > 120) ? 120 : g_castleDrawDst.h;
	      SDL_Rect srcTop = {0, 0, g_castleDrawDst.w, topH};
	      SDL_Rect dstTop = g_castleDrawDst;
	      dstTop.h = topH;
	      renderCopyWithShadow(g_texCastle, &srcTop, &dstTop);
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
      } else if (e.type == E_KOOPA) {
        if (g_texKoopa || g_texKoopaSheet) {
          if (e.state >= 1) {
            dst.h = 16;
            dst.w = 16;
            if (g_texKoopaSheet) {
              int frame = (e.state == 2) ? ((int)(e.timer * 12) % 4) : 0;
              SDL_Rect src = {32 + frame * 16, 16, 16, 16};
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
          SDL_Rect src = {0, 0, 8, 8};
          SDL_Rect drawDst = {dst.x + 4, dst.y + 4, 8, 8};
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

    // Player
    bool showPlayer = (g_p.invT <= 0 || ((int)(g_p.animT * 8) % 2) == 0);
    if (!g_p.dead && showPlayer) {
      int px = (int)(g_p.r.x - g_camX);
	      int pw = (g_p.power == P_SMALL) ? PLAYER_DRAW_W_SMALL : PLAYER_DRAW_W_BIG;
	      int ph = (g_p.power == P_SMALL) ? 16 : 32;
	      int drawW = pw;
	      int drawH = ph;
	      int drawX = px + (int)((g_p.r.w - drawW) / 2);
	      int drawY = (int)g_p.r.y;
	      if (g_p.crouch && g_p.power >= P_BIG)
	        drawY -= 16;
	      SDL_Rect dst = {drawX, drawY, drawW, drawH};

      // Try to draw sprite texture on top (if it works, it will cover the
      // fallback)
      SDL_Texture *tex = g_texPlayerSmall[g_charIndex];
      if (g_p.power == P_FIRE)
        tex = g_texPlayerFire[g_charIndex];
      else if (g_p.power >= P_BIG)
        tex = g_texPlayerBig[g_charIndex];
      if (tex) {
        bool skidding =
            g_p.ground &&
            (((g_held & VPAD_BUTTON_LEFT) && g_p.vx > 20) ||
             ((g_held & VPAD_BUTTON_RIGHT) && g_p.vx < -20));
        if (skidding && g_sfxSkid && g_skidCooldown <= 0.0f) {
          Mix_PlayChannel(-1, g_sfxSkid, 0);
          g_skidCooldown = 0.35f;
        }
        int frame = 0;
        SDL_Rect src = {0, 0, 16, 16};
        if (g_p.power == P_SMALL) {
          if (!g_p.ground) {
            frame = 5;
          } else {
            frame = skidding ? 4
                             : (fabsf(g_p.vx) > 10
                                    ? 1 + ((int)(g_p.animT * 10) % 3)
                                    : 0);
          }
          src = {frame * 16, 0, 16, 16};
        } else if (g_p.power == P_FIRE) {
          int fireFrame = 0;
          if (g_p.throwT > 0.0f) {
            fireFrame = g_p.ground ? 7 : 8;
          } else if (g_p.crouch && g_p.ground) {
            fireFrame = 1;
          } else if (!g_p.ground) {
            fireFrame = 6;
          } else if (skidding) {
            fireFrame = 5;
          } else if (fabsf(g_p.vx) > 10) {
            fireFrame = 2 + ((int)(g_p.animT * 10) % 3);
          } else {
            fireFrame = 0;
          }
          frame = fireFrame;
          src = {frame * 16, 0, 16, 32};
        } else {
          // Big sheet: 0 idle, 1 crouch, 2..4 walk, 5 skid, 6 jump
          if (g_p.crouch && g_p.ground) {
            frame = 1;
          } else if (!g_p.ground) {
            frame = 6;
          } else {
            frame = skidding ? 5
                             : (fabsf(g_p.vx) > 10
                                    ? 2 + ((int)(g_p.animT * 10) % 3)
                                    : 0);
          }
          src = {frame * 16, 0, 16, 32};
        }

        SDL_RendererFlip flip = g_p.right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        if (g_p.power == P_SMALL) {
          renderCopyExWithShadow(tex, &src, &dst, flip);
        } else {
          // Some backends behave oddly with a single 16x32 blit (showing only the
          // top half). Split tall sprites into two 16x16 draws.
          renderTall16x32WithShadow(tex, frame * 16, dst, flip);
        }
      } else {
		        // Fallback rectangle if texture missing
		        SDL_SetRenderDrawColor(g_ren, 228, 52, 52, 255);
		        SDL_RenderFillRect(g_ren, &dst);
	      }
	    }
	    // Castle overlay: hides the player as they enter the door.
	    if (g_castleDrawOn && g_texCastle && g_castleDrawDst.h > 120) {
	      int overlayY = 120;
	      int overlayH = g_castleDrawDst.h - overlayY;
	      SDL_Rect src = {0, overlayY, g_castleDrawDst.w, overlayH};
	      SDL_Rect dst = {g_castleDrawDst.x, g_castleDrawDst.y + overlayY,
	                      g_castleDrawDst.w, overlayH};
	      SDL_RenderCopy(g_ren, g_texCastle, &src, &dst);
	    }
	  if (g_nightMode) {
	    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
	    SDL_SetRenderDrawColor(g_ren, 0, 0, 40, 100);
	    SDL_Rect night = {0, 0, GAME_W, GAME_H};
    SDL_RenderFillRect(g_ren, &night);
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

    // Reserve space for lives icon+count at the far right.
    char livesBuf[16];
    snprintf(livesBuf, sizeof(livesBuf), "%02d", g_p.lives);
    int lifeSize = 12;
    int livesW = textWidth(livesBuf, scale);
    int livesBlockW = lifeSize + 2 + livesW;

    int timeRight = GAME_W - margin - livesBlockW - 10;
    drawTextShadow(timeRight - textWidth(timeLabel, scale), y1, timeLabel, scale,
                   white);
    drawTextShadow(timeRight - textWidth(buf, scale), y2, buf, scale, white);

    // Lives (below time, far right)
    int iconX = GAME_W - margin - livesBlockW;
    int iconY = y2 - 1;
    if (g_texLifeIcon[g_charIndex]) {
      SDL_Rect dst = {iconX, iconY, lifeSize, lifeSize};
      SDL_RenderCopy(g_ren, g_texLifeIcon[g_charIndex], nullptr, &dst);
    }
    drawTextShadow(iconX + lifeSize + 2, y2, livesBuf, scale, white);
  }

  if (g_state == GS_DEAD || g_state == GS_GAMEOVER) {
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 200);
    SDL_Rect overlay = {GAME_W / 4, GAME_H / 3, GAME_W / 2, GAME_H / 3};
    SDL_RenderFillRect(g_ren, &overlay);
  }
  if (g_state == GS_WIN) {
    SDL_SetRenderDrawColor(g_ren, 0, 100, 0, 200);
    SDL_Rect overlay = {GAME_W / 4, GAME_H / 3, GAME_W / 2, GAME_H / 3};
    SDL_RenderFillRect(g_ren, &overlay);
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
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  IMG_Init(IMG_INIT_PNG);
  Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
  Mix_AllocateChannels(32);
  Mix_ReserveChannels(0);

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
      updatePlayer(dt);
      updateEntities(dt);
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
  WHBProcShutdown();
  return 0;
}
