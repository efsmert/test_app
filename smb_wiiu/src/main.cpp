// Super Mario Bros. Wii U Port - Complete Implementation
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "game_types.h"
#include "levels.h"
#include <cmath>
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
enum GameState { GS_TITLE, GS_PLAYING, GS_FLAG, GS_DEAD, GS_GAMEOVER, GS_WIN, GS_PAUSE };
constexpr uint8_t QMETA_COIN = 0;
constexpr uint8_t QMETA_POWERUP = 1;
constexpr uint8_t QMETA_ONEUP = 2;
constexpr uint8_t QMETA_STAR = 3;
enum TitleMode { TITLE_CHAR_SELECT = 0, TITLE_OPTIONS = 1 };

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
static SDL_Texture *g_texGoomba = nullptr;
static SDL_Texture *g_texKoopa = nullptr;
static SDL_Texture *g_texKoopaSheet = nullptr;
static SDL_Texture *g_texQuestion = nullptr;
static SDL_Texture *g_texMushroom = nullptr;
static SDL_Texture *g_texFireFlower = nullptr;
static SDL_Texture *g_texFireball = nullptr;
static SDL_Texture *g_texCoin = nullptr;
static SDL_Texture *g_texTerrain = nullptr;
static SDL_Texture *g_texFlagPole = nullptr;
static SDL_Texture *g_texFlag = nullptr;
static SDL_Texture *g_texCastle = nullptr;
static LevelTheme g_theme = THEME_OVERWORLD;
static const char *g_charNames[] = {"Luigi", "Toad", "Toadette"};
static const char *g_charDisplayNames[] = {"LUIGI", "TOAD", "TOADETTE"};
static const int g_charCount = 3;
static int g_charIndex = 0;
static int g_menuIndex = 0;
static int g_titleMode = TITLE_CHAR_SELECT;
static int g_optionsIndex = 0;
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

int textWidth(const char *text, int scale) {
  int len = 0;
  for (const char *p = text; *p; ++p)
    len++;
  return len * 6 * scale;
}

bool overlap(const Rect &a, const Rect &b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
         a.y + a.h > b.y;
}

int mapWidth() { return g_levelInfo.mapWidth > 0 ? g_levelInfo.mapWidth : MAP_W; }

bool solidAt(int tx, int ty) {
  if (tx < 0 || tx >= mapWidth() || ty < 0 || ty >= MAP_H)
    return false;
  if (g_map[ty][tx] >= T_GROUND && g_map[ty][tx] != T_FLAG)
    return true;
  if (g_levelInfo.atlasX && g_levelInfo.atlasY &&
      g_levelInfo.atlasX[ty][tx] != 255 && g_levelInfo.atlasY[ty][tx] != 255)
    return true;
  return false;
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

SDL_Rect firePlayerSrcForFrame(int frame) {
  switch (frame) {
  case 0: // idle
    return {0, 0, 32, 48};
  case 1: // crouch
    return {32, 0, 32, 48};
  case 2: // walk 1
    return {64, 0, 32, 48};
  case 3: // walk 2
    return {96, 0, 32, 48};
  case 4: // walk 3 / skid fallback
    return {128, 0, 32, 48};
  case 5: // skid
    return {160, 0, 32, 48};
  case 6: // jump
    return {192, 0, 32, 48};
  default:
    return {0, 0, 32, 48};
  }
}

bool playerStomp(const Rect &enemy) {
  float playerBottom = g_p.r.y + g_p.r.h;
  return g_p.vy >= -20.0f && playerBottom <= enemy.y + 6.0f;
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
      (strstr(file, "sprites/ui/TitleSMB1.png") != nullptr) ||
      (strstr(file, "FlagPole.png") != nullptr) ||
      (strstr(file, "KoopaTroopaSheet.png") != nullptr) ||
      (strstr(file, "QuestionBlock.png") != nullptr) ||
      (strstr(file, "FireFlower.png") != nullptr) ||
      (strstr(file, "Fireball.png") != nullptr)) {
    Uint32 colorKey = SDL_MapRGB(s->format, 0, 255, 0);
    SDL_SetColorKey(s, SDL_TRUE, colorKey);
  }

  g_loadedTex++;
  SDL_Texture *t = SDL_CreateTextureFromSurface(g_ren, s);
  SDL_FreeSurface(s);
  return t;
}

void setSfxVolume(Mix_Chunk *sfx) {
  if (sfx)
    Mix_VolumeChunk(sfx, MIX_MAX_VOLUME);
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

void loadThemeTilesets() {
  destroyTex(g_texTerrain);
  char path[256];
  snprintf(path, sizeof(path), "tilesets/Terrain/%s.png", themeName(g_theme));
  g_texTerrain = loadTex(path);
  if (!g_texTerrain) {
    // Backward-compat with older content layouts.
    snprintf(path, sizeof(path), "sprites/tilesets/%s.png", themeName(g_theme));
    g_texTerrain = loadTex(path);
  }
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

  g_texTitle = loadTex("sprites/ui/TitleSMB1.png");
  g_texCursor = loadTex("sprites/ui/Cursor.png");
  g_texMenuBG = loadTex("sprites/ui/MenuBG.png");

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
}

void spawnEnemiesFromLevel() {
  for (int i = 0; i < 64; i++)
    g_ents[i].on = false;
  int idx = 0;
  for (int i = 0; i < g_levelInfo.enemyCount && idx < 64; i++) {
    const EnemySpawn &s = g_levelInfo.enemies[i];
    g_ents[idx].on = true;
    g_ents[idx].type = s.type;
    float h = s.type == E_KOOPA ? 24.f : 16.f;
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
  g_p.r.w = 14;
  g_p.r.h = 16;
  g_p.crouch = false;
  setupLevel();
}

void restartLevel() {
  g_p.power = P_SMALL;
  g_p.r.w = 14;
  g_p.r.h = 16;
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
  if (g_titleMode == TITLE_CHAR_SELECT) {
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

    if (g_pressed & VPAD_BUTTON_A) {
      g_charIndex = g_menuIndex;
      startNewGame();
    }
  } else {
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
      g_titleMode = TITLE_CHAR_SELECT;
      if (g_sfxMenuMove)
        Mix_PlayChannel(-1, g_sfxMenuMove, 0);
    }
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
      g_titleMode = TITLE_CHAR_SELECT;
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
      g_ents[i] = {true, E_FIREBALL, {x, y, 16, 16}, 0, -120, g_p.right ? 1 : -1, 0, 0};
      g_fireCooldown = 0.35f;
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
  if (g_p.invT > 0)
    g_p.invT -= dt;
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

  if (g_p.power == P_FIRE && (g_pressed & VPAD_BUTTON_B) && !g_p.crouch) {
    spawnFireball();
  }

  if (g_levelInfo.pipes && g_levelInfo.pipeCount > 0) {
    float midY = g_p.r.y + g_p.r.h * 0.5f;
    float midX = g_p.r.x + g_p.r.w * 0.5f;

    auto pipeAt = [&](int x, int y) -> bool {
      return y >= 0 && y < MAP_H && x >= 0 && x < mapWidth() &&
             g_map[y][x] == T_PIPE;
    };

    for (int i = 0; i < g_levelInfo.pipeCount; i++) {
      const PipeLink &p = g_levelInfo.pipes[i];
      int px = p.x * TILE;
      int topTile = p.y;
      while (topTile > 0 &&
             (pipeAt(p.x, topTile - 1) || pipeAt(p.x + 1, topTile - 1))) {
        topTile--;
      }
      int heightTiles = 1;
      for (int y = topTile; y < MAP_H; y++) {
        if (pipeAt(p.x, y) || pipeAt(p.x + 1, y))
          heightTiles = y - topTile + 1;
        else
          break;
      }
      SDL_Rect pipeRect = {px, topTile * TILE, TILE * 2, TILE * heightTiles};
      bool overlapY =
          (midY >= pipeRect.y - 4 && midY <= pipeRect.y + pipeRect.h + 4);
      bool overPipeX =
          (midX >= pipeRect.x + 2 && midX <= pipeRect.x + pipeRect.w - 2);

      switch (p.enterDir) {
      case 0: // Down
        if (downHeld && g_p.ground && overPipeX &&
            fabsf((g_p.r.y + g_p.r.h) - pipeRect.y) <= 6.0f) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      case 1: // Up
        if ((g_held & VPAD_BUTTON_UP) && overPipeX &&
            fabsf(g_p.r.y - (pipeRect.y + pipeRect.h)) <= 6.0f) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      case 2: // Left
        if ((g_held & VPAD_BUTTON_LEFT) && overlapY &&
            g_p.r.x <= pipeRect.x + 2 &&
            g_p.r.x + g_p.r.w > pipeRect.x - 2) {
          if (tryPipeEnter(p))
            return;
        }
        break;
      case 3: // Right
        if ((g_held & VPAD_BUTTON_RIGHT) && overlapY &&
            g_p.r.x + g_p.r.w >= pipeRect.x + pipeRect.w - 2 &&
            g_p.r.x < pipeRect.x + pipeRect.w + 2) {
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
      g_p.r.h = 16;
      g_p.r.y += 16;
    } else if (!downHeld && g_p.crouch) {
      g_p.crouch = false;
      g_p.r.y -= 16;
      g_p.r.h = 32;
    }
  } else if (g_p.crouch) {
    g_p.crouch = false;
    g_p.r.y -= 16;
    g_p.r.h = 32;
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

  if ((g_pressed & VPAD_BUTTON_B) && g_p.ground) {
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
  if (!(g_held & VPAD_BUTTON_B) && g_p.vy < -100)
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
    int ty2 = (int)((g_p.r.y + g_p.r.h) / TILE);
    int tx1 = (int)(g_p.r.x / TILE);
    int tx2 = (int)((g_p.r.x + g_p.r.w - 1) / TILE);
    bool hit = false;
    for (int ty = ty1; ty <= ty2; ty++) {
      for (int tx = tx1; tx <= tx2; tx++) {
        if (solidAt(tx, ty)) {
          uint8_t tile = g_map[ty][tx];
          if (g_p.vy > 0) {
            g_p.r.y = ty * TILE - g_p.r.h;
            g_p.vy = 0;
            g_p.ground = true;
            g_p.jumping = false;
            hit = true;
          } else if (g_p.vy < 0) {
            g_p.r.y = (ty + 1) * TILE;
            g_p.vy = 45;
            if (tile == T_QUESTION) {
              g_map[ty][tx] = T_USED;
              uint8_t meta = questionMetaAt(tx, ty);
              if (meta == QMETA_POWERUP || meta == QMETA_STAR) {
                if (g_p.power == P_SMALL)
                  spawnMushroom(tx, ty);
                else
                  spawnFireFlower(tx, ty);
              } else if (meta == QMETA_ONEUP) {
                g_p.lives++;
                g_p.score += 1000;
                if (g_sfxPowerup)
                  Mix_PlayChannel(-1, g_sfxPowerup, 0);
              } else {
                g_p.coins++;
                g_p.score += 200;
                spawnCoinPopup(tx * TILE, ty * TILE);
              }
              if (g_sfxBump)
                Mix_PlayChannel(-1, g_sfxBump, 0);
            } else if (tile == T_BRICK && g_p.power > P_SMALL) {
              g_map[ty][tx] = T_EMPTY;
              g_p.score += 50;
              if (g_sfxBreak)
                Mix_PlayChannel(-1, g_sfxBreak, 0);
            } else if (tile == T_BRICK) {
              if (g_sfxBump)
                Mix_PlayChannel(-1, g_sfxBump, 0);
            }
            hit = true;
          }
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
        if (solidAt(midTx, bottomTy)) {
          e.r.y = bottomTy * TILE - e.r.h;
          e.vy = 0;
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
            g_p.invT <= 0 && !g_p.dead && overlap(g_p.r, e.r)) {
          if (playerStomp(e.r)) {
            e.state = 1;
            e.timer = 0;
            g_p.vy = (g_held & VPAD_BUTTON_B) ? -Physics::BOUNCE_HEIGHT * 1.5f
                                              : -Physics::BOUNCE_HEIGHT;
            g_p.score += 100;
            if (g_sfxStomp)
              Mix_PlayChannel(-1, g_sfxStomp, 0);
            else if (g_sfxKick)
              Mix_PlayChannel(-1, g_sfxKick, 0);
          } else {
            if (g_p.power > P_SMALL) {
              g_p.power = P_SMALL;
              g_p.r.h = 16;
              g_p.r.w = 14;
              g_p.crouch = false;
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
      if (solidAt(midTx, bottomTy)) {
        e.r.y = bottomTy * TILE - e.r.h;
        e.vy = 0;
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
          g_p.invT <= 0 && !g_p.dead && overlap(g_p.r, e.r)) {
        bool stomp = playerStomp(e.r);
        if (stomp) {
          g_p.vy = (g_held & VPAD_BUTTON_B) ? -Physics::BOUNCE_HEIGHT * 1.5f
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
        } else {
          if (e.state == 1) {
            e.state = 2;
            e.timer = 0;
            e.dir = (g_p.r.x < e.r.x) ? 1 : -1;
            if (g_sfxKick)
              Mix_PlayChannel(-1, g_sfxKick, 0);
          } else if (g_p.power > P_SMALL) {
            g_p.power = P_SMALL;
            g_p.r.h = 16;
            g_p.r.w = 14;
            g_p.crouch = false;
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
      if (ty >= 0 && ty < MAP_H &&
          g_map[ty][(int)((e.r.x + 8) / TILE)] >= T_GROUND) {
        e.r.y = ty * TILE - e.r.h;
        e.vy = 0;
      }
      int tx = e.dir > 0 ? (int)((e.r.x + e.r.w) / TILE) : (int)(e.r.x / TILE);
      if (tx >= 0 && tx < mapWidth() &&
          g_map[(int)(e.r.y / TILE)][tx] >= T_GROUND)
        e.dir = -e.dir;
      if (overlap(g_p.r, e.r)) {
        e.on = false;
        if (g_p.power == P_SMALL) {
          g_p.power = P_BIG;
          g_p.r.h = 32;
          g_p.r.y -= 16;
          g_p.r.w = 16;
          g_p.crouch = false;
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
      if (solidAt(midTx, ty)) {
        e.r.y = ty * TILE - e.r.h;
        e.vy = 0;
      }
      if (overlap(g_p.r, e.r)) {
        e.on = false;
        if (g_p.power == P_SMALL) {
          g_p.power = P_BIG;
          g_p.r.h = 32;
          g_p.r.y -= 16;
          g_p.r.w = 16;
          g_p.crouch = false;
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
      const float speed = 140.0f;
      e.r.x += e.dir * speed * dt;
      e.vy += 260.0f * dt;
      e.r.y += e.vy * dt;

      int midTx = (int)((e.r.x + e.r.w * 0.5f) / TILE);
      int bottomTy = (int)((e.r.y + e.r.h) / TILE);
      if (solidAt(midTx, bottomTy)) {
        e.r.y = bottomTy * TILE - e.r.h;
        e.vy = -140.0f;
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

  if (tile == T_QUESTION && g_texQuestion) {
    int frame = ((SDL_GetTicks() / 200) % 3);
    SDL_Rect src = {frame * 16, questionRowForTheme(g_theme), 16, 16};
    SDL_RenderCopy(g_ren, g_texQuestion, &src, &dst);
    return;
  }
  if (tile == T_USED && g_texQuestion) {
    SDL_Rect src = {0, questionRowForTheme(g_theme), 16, 16};
    SDL_SetTextureColorMod(g_texQuestion, 200, 140, 60);
    SDL_RenderCopy(g_ren, g_texQuestion, &src, &dst);
    SDL_SetTextureColorMod(g_texQuestion, 255, 255, 255);
    SDL_SetRenderDrawColor(g_ren, 150, 90, 30, 255);
    SDL_Rect inner = {dst.x + 4, dst.y + 4, 8, 8};
    SDL_RenderFillRect(g_ren, &inner);
    return;
  }

  // If a terrain atlas is available, prefer it for tiles coming from the
  // Godot TileMap so we preserve proper edges, gaps, and pipe pieces.
  if (g_texTerrain && g_levelInfo.atlasX && g_levelInfo.atlasY) {
    uint8_t ax = g_levelInfo.atlasX[ty][tx];
    uint8_t ay = g_levelInfo.atlasY[ty][tx];
    if (ax != 255 && ay != 255) {
      SDL_Rect src = {ax * 16, ay * 16, 16, 16};
      SDL_RenderCopy(g_ren, g_texTerrain, &src, &dst);
      return;
    }
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
      bool top = (ty == 0 || g_map[ty - 1][tx] != T_PIPE);
      src = top ? SDL_Rect{240, 0, 16, 16} : SDL_Rect{240, 16, 16, 16};
      break;
    }
    case T_CASTLE:
      src = {0, 176, 16, 16};
      break;
    default:
      use = false;
      break;
    }
    if (use) {
      SDL_RenderCopy(g_ren, g_texTerrain, &src, &dst);
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
  default:
    return;
  }
  SDL_RenderFillRect(g_ren, &dst);

}

void render() {
  if (g_state == GS_TITLE) {
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);

    if (g_texMenuBG) {
      SDL_Rect bg = {0, 0, GAME_W, GAME_H};
      SDL_RenderCopy(g_ren, g_texMenuBG, nullptr, &bg);
    } else {
      SDL_SetRenderDrawColor(g_ren, 10, 10, 20, 255);
      SDL_RenderClear(g_ren);
    }

    if (g_texTitle) {
      int w = 192, h = 64;
      SDL_Rect dst = {(GAME_W - w) / 2, 12, w, h};
      SDL_RenderCopy(g_ren, g_texTitle, nullptr, &dst);
    }

    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 180);
    SDL_Rect panel = {20, 80, GAME_W - 40, 110};
    SDL_RenderFillRect(g_ren, &panel);
    SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
    SDL_RenderDrawRect(g_ren, &panel);

    const char *selText = "SELECT CHARACTER";
    drawText((GAME_W - textWidth(selText, 2)) / 2 + 1, 86, selText, 2,
             {0, 0, 0, 255});
    drawText((GAME_W - textWidth(selText, 2)) / 2, 84, selText, 2,
             {255, 255, 255, 255});

    const int startX = (GAME_W - (g_charCount * 56 - 16)) / 2;
    const int y = GAME_H / 2 + 5;
    for (int i = 0; i < g_charCount; i++) {
      int x = startX + i * 56;
      int iconW = PLAYER_DRAW_W_SMALL * 2;
      int iconH = 16 * 2;
      SDL_Rect dst = {x + (24 - iconW) / 2, y + (24 - iconH) / 2, iconW,
                      iconH};
      SDL_Rect src = {0, 0, 16, 16};
      if (g_texPlayerSmall[i])
        SDL_RenderCopy(g_ren, g_texPlayerSmall[i], &src, &dst);

      SDL_Rect box = {x - 6, y - 6, 36, 36};
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      if (i == g_menuIndex) {
        SDL_RenderDrawRect(g_ren, &box);
        SDL_Rect inner = {box.x + 2, box.y + 2, box.w - 4, box.h - 4};
        SDL_RenderDrawRect(g_ren, &inner);
      }
    }

    drawText((GAME_W - textWidth(g_charDisplayNames[g_menuIndex], 2)) / 2, y + 30,
             g_charDisplayNames[g_menuIndex], 2, {255, 255, 255, 255});

    const char *pressText = "PRESS A";
    drawText((GAME_W - textWidth(pressText, 2)) / 2 + 1, y + 50, pressText, 2,
             {0, 0, 0, 255});
    drawText((GAME_W - textWidth(pressText, 2)) / 2, y + 48, pressText, 2,
             {255, 255, 0, 255});

    const char *optsHint = "Y OPTIONS";
    drawText((GAME_W - textWidth(optsHint, 1)) / 2, y + 68, optsHint, 1,
             {200, 200, 200, 255});

    if (g_titleMode == TITLE_OPTIONS) {
      SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 200);
      SDL_Rect oPanel = {40, 30, GAME_W - 80, 120};
      SDL_RenderFillRect(g_ren, &oPanel);
      SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
      SDL_RenderDrawRect(g_ren, &oPanel);

      const char *title = "OPTIONS";
      drawText((GAME_W - textWidth(title, 2)) / 2, 38, title, 2,
               {255, 255, 255, 255});

      const char *line1 = g_randomTheme ? "RANDOM THEME: ON" : "RANDOM THEME: OFF";
      const char *line2 = g_nightMode ? "NIGHT MODE: ON" : "NIGHT MODE: OFF";
      int baseY = 70;
      SDL_Color hi = {255, 255, 0, 255};
      SDL_Color norm = {255, 255, 255, 255};
      drawText(60, baseY, line1, 1, g_optionsIndex == 0 ? hi : norm);
      drawText(60, baseY + 16, line2, 1, g_optionsIndex == 1 ? hi : norm);
      drawText(60, baseY + 40, "B BACK", 1, {200, 200, 200, 255});
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

  // Tiles
  int startTx = (int)(g_camX / TILE) - 1;
  int viewTiles = (GAME_W + TILE - 1) / TILE + 2;
  for (int ty = 0; ty < MAP_H; ty++) {
    for (int tx = startTx; tx < startTx + viewTiles && tx < mapWidth(); tx++) {
      if (tx >= 0)
        drawTile(tx, ty, g_map[ty][tx]);
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
      int poleX = g_flagX * TILE - (int)g_camX + 8;
      int poleBottom = (groundY + 1) * TILE;

      if (g_texFlagPole) {
        int texW = 0, texH = 0;
        SDL_QueryTexture(g_texFlagPole, nullptr, nullptr, &texW, &texH);
        SDL_Rect src = {0, 0, 16, texH};
        SDL_Rect dst = {poleX - 8, poleBottom - texH, 16, texH};
        SDL_RenderCopy(g_ren, g_texFlagPole, &src, &dst);
      }
      if (g_texFlag) {
        int frame = ((SDL_GetTicks() / 150) % 3);
        SDL_Rect src = {frame * 16, 0, 16, 16};
        SDL_Rect dst = {poleX - 18, (int)g_flagY, 16, 16};
        SDL_RenderCopy(g_ren, g_texFlag, &src, &dst);
      }
    }

    // Castle
    if (g_texCastle) {
      int minX = MAP_W;
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
      if (maxY >= 0) {
        int groundY = maxY;
        for (int y = MAP_H - 1; y >= 0; y--) {
          bool found = false;
          for (int x = minX; x < minX + 5; x++) {
            if (solidAt(x, y)) {
              groundY = y;
              found = true;
              break;
            }
          }
          if (found)
            break;
        }
        int texW = 0, texH = 0;
        SDL_QueryTexture(g_texCastle, nullptr, nullptr, &texW, &texH);
        SDL_Rect src = {0, 0, texW, texH};
        SDL_Rect dst = {minX * TILE - (int)g_camX, (groundY + 1) * TILE - texH,
                        texW, texH};
        SDL_RenderCopy(g_ren, g_texCastle, &src, &dst);
      }
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
          SDL_RenderCopy(g_ren, g_texGoomba, &src, &dst);
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
              SDL_RenderCopy(g_ren, g_texKoopaSheet, &src, &dst);
            } else if (g_texKoopa) {
              SDL_Rect src = {0, 8, 16, 16};
              SDL_RenderCopy(g_ren, g_texKoopa, &src, &dst);
            }
          } else {
            SDL_Rect src = {((int)(e.timer * 8) % 2) * 16, 0, 16, 24};
            SDL_RendererFlip flip =
                (e.dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(g_ren, g_texKoopa, &src, &dst, 0, nullptr, flip);
          }
        } else {
          SDL_SetRenderDrawColor(g_ren, 0, 160, 0, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_MUSHROOM) {
        if (g_texMushroom) {
          SDL_Rect src = {0, 0, 16, 16};
          SDL_RenderCopy(g_ren, g_texMushroom, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 220, 60, 60, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_FIRE_FLOWER) {
        if (g_texFireFlower) {
          int frame = ((int)(e.timer * 12) % 4);
          SDL_Rect src = {frame * 16, fireFlowerRowForTheme(g_theme), 16, 16};
          SDL_RenderCopy(g_ren, g_texFireFlower, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 220, 140, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_FIREBALL) {
        if (g_texFireball) {
          SDL_Rect src = {0, 0, 8, 8};
          SDL_RenderCopy(g_ren, g_texFireball, &src, &dst);
        } else {
          SDL_SetRenderDrawColor(g_ren, 255, 120, 40, 255);
          SDL_RenderFillRect(g_ren, &dst);
        }
      } else if (e.type == E_COIN_POPUP) {
        if (g_texCoin) {
          int frame = 1 + ((int)(e.timer * 12) % 3);
          SDL_Rect src = {frame * 16, 0, 16, 16};
          SDL_RenderCopy(g_ren, g_texCoin, &src, &dst);
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
      if (g_p.crouch && g_p.power == P_BIG)
        drawY -= 16;
      if (g_p.power == P_FIRE) {
        // Fire sheets are 32x48 frames; preserve aspect ratio when scaling down
        // to the standard 32px render height.
        drawH = ph;
        drawW = (int)lroundf(drawH * (32.0f / 48.0f));
        drawX = px + (int)((g_p.r.w - drawW) / 2);
      }
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
        if (g_p.crouch && g_p.power >= P_BIG) {
          frame = 1;
        } else if (!g_p.ground) {
          frame = 5;
        } else {
          frame = skidding ? 4
                           : (fabsf(g_p.vx) > 10
                                  ? 1 + ((int)(g_p.animT * 10) % 3)
                                  : 0);
        }
        SDL_Rect src = {0, 0, 16, 16};
        if (g_p.power == P_FIRE) {
          int fireFrame = 0;
          if (g_p.crouch)
            fireFrame = 1;
          else if (!g_p.ground)
            fireFrame = 6;
          else if (skidding)
            fireFrame = 5;
          else if (fabsf(g_p.vx) > 10)
            fireFrame = 2 + ((int)(g_p.animT * 10) % 3);
          else
            fireFrame = 0;
          src = firePlayerSrcForFrame(fireFrame);
        } else {
          int srcH = (g_p.power == P_SMALL) ? 16 : 32;
          src = {frame * 16, 0, 16, srcH};
        }
        SDL_RendererFlip flip = g_p.right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        SDL_RenderCopyEx(g_ren, tex, &src, &dst, 0, nullptr, flip);
      } else {
        // Fallback rectangle if texture missing
        SDL_SetRenderDrawColor(g_ren, 228, 52, 52, 255);
        SDL_RenderFillRect(g_ren, &dst);
      }
    }
  if (g_nightMode) {
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 40, 100);
    SDL_Rect night = {0, 0, GAME_W, GAME_H};
    SDL_RenderFillRect(g_ren, &night);
  }
  // HUD
  SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 200);
  SDL_Rect hudBg = {0, 0, GAME_W, 24};
  SDL_RenderFillRect(g_ren, &hudBg);

  char buf[64];
  // Score
  snprintf(buf, sizeof(buf), "SCORE %06d", g_p.score % 1000000);
  drawText(6, 4, buf, 1, {255, 255, 255, 255});
  // World-Level
  snprintf(buf, sizeof(buf), "WORLD %d-%d", g_levelInfo.world,
           g_levelInfo.stage);
  drawText((GAME_W - textWidth(buf, 1)) / 2, 4, buf, 1,
           {255, 255, 255, 255});
  // Time
  snprintf(buf, sizeof(buf), "TIME %03d", g_time < 0 ? 0 : g_time);
  drawText(GAME_W - textWidth(buf, 1) - 6, 4, buf, 1,
           {255, 255, 255, 255});
  // Coins
  snprintf(buf, sizeof(buf), "COIN %02d", g_p.coins % 100);
  drawText(6, 14, buf, 1, {255, 255, 0, 255});
  // Lives
  snprintf(buf, sizeof(buf), "x%02d", g_p.lives);
  int livesTextW = textWidth(buf, 1);
  int iconSize = 8;
  int iconX = GAME_W - 6 - livesTextW - 4 - iconSize;
  int iconY = 14;
  if (g_texLifeIcon[g_charIndex]) {
    SDL_Rect lifeDst = {iconX, iconY, iconSize, iconSize};
    SDL_RenderCopy(g_ren, g_texLifeIcon[g_charIndex], nullptr, &lifeDst);
  }
  drawText(iconX + iconSize + 4, 14, buf, 1, {255, 255, 255, 255});

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
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 200);
    SDL_Rect overlay = {GAME_W / 4 - 10, GAME_H / 3 - 10, GAME_W / 2 + 20, GAME_H / 3 + 20};
    SDL_RenderFillRect(g_ren, &overlay);
    drawText((GAME_W - textWidth("PAUSED", 2)) / 2, GAME_H / 2 - 8, "PAUSED",
             2, {255, 255, 255, 255});
    int menuY = GAME_H / 2 + 10;
    for (int i = 0; i < g_pauseOptionCount; i++) {
      SDL_Color c = (i == g_pauseIndex) ? SDL_Color{255, 255, 0, 255}
                                       : SDL_Color{255, 255, 255, 255};
      drawText((GAME_W - textWidth(g_pauseOptions[i], 1)) / 2, menuY + i * 10,
               g_pauseOptions[i], 1, c);
    }
    drawText((GAME_W - textWidth("PRESS + TO RESUME", 1)) / 2, GAME_H - 18,
             "PRESS + TO RESUME", 1, {200, 200, 200, 255});
  }

  SDL_RenderPresent(g_ren);
}

int main(int argc, char **argv) {
  WHBProcInit();
  VPADInit();
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  IMG_Init(IMG_INIT_PNG);
  Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
  Mix_AllocateChannels(16);

  g_win = SDL_CreateWindow("SMB", 0, 0, TV_W, TV_H, SDL_WINDOW_FULLSCREEN);
  g_ren = SDL_CreateRenderer(
      g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(g_ren, GAME_W, GAME_H);

  loadAssets();
  g_state = GS_TITLE;
  g_menuIndex = g_charIndex;

  Uint32 lastTick = SDL_GetTicks();
  while (WHBProcIsRunning()) {
    float dt = (SDL_GetTicks() - lastTick) / 1000.0f;
    lastTick = SDL_GetTicks();
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
