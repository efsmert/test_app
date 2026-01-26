/**
 * Wii U 2D Platformer Game
 * A complete platformer for the Aroma environment
 * 
 * Features:
 * - Player movement with physics (gravity, jumping)
 * - Platforms to traverse
 * - Collectible coins
 * - Enemy AI (patrol movement)
 * - Multiple lives system
 * - Score tracking
 * - Dual-screen output (TV + GamePad)
 * 
 * Controls:
 * - Left Stick / D-Pad: Move left/right
 * - A Button: Jump
 * - B Button: Sprint
 * - + Button: Pause
 * - HOME: Exit to menu
 */

#include <coreinit/memdefaultheap.h>
#include <coreinit/memheap.h>
#include <coreinit/time.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <vpad/input.h>
#include <whb/proc.h>

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>

// ============================================================================
// Constants
// ============================================================================

// Screen dimensions
static const int TV_WIDTH = 1280;
static const int TV_HEIGHT = 720;
static const int DRC_WIDTH = 854;
static const int DRC_HEIGHT = 480;

// Game constants
static const float GRAVITY = 0.5f;
static const float PLAYER_SPEED = 4.0f;
static const float PLAYER_SPRINT_SPEED = 7.0f;
static const float JUMP_FORCE = -12.0f;
static const float MAX_FALL_SPEED = 12.0f;

static const int MAX_PLATFORMS = 20;
static const int MAX_COINS = 15;
static const int MAX_ENEMIES = 5;
static const int PLAYER_START_LIVES = 3;

// ============================================================================
// Color Definitions (RGBA format for OSScreen)
// ============================================================================

static const uint32_t COLOR_BLACK = 0x000000FF;
static const uint32_t COLOR_WHITE = 0xFFFFFFFF;
static const uint32_t COLOR_RED = 0xFF0000FF;
static const uint32_t COLOR_GREEN = 0x00FF00FF;
static const uint32_t COLOR_BLUE = 0x4488FFFF;
static const uint32_t COLOR_YELLOW = 0xFFFF00FF;
static const uint32_t COLOR_ORANGE = 0xFF8800FF;
static const uint32_t COLOR_PURPLE = 0x8844FFFF;
static const uint32_t COLOR_BROWN = 0x8B5513FF;
static const uint32_t COLOR_DARK_GREEN = 0x228B22FF;
static const uint32_t COLOR_LIGHT_BLUE = 0x87CEEBFF;
static const uint32_t COLOR_GOLD = 0xFFD700FF;
static const uint32_t COLOR_GRAY = 0x808080FF;
static const uint32_t COLOR_DARK_GRAY = 0x303030FF;
static const uint32_t COLOR_SKIN = 0xFFDDB0FF;

// ============================================================================
// Game Structures
// ============================================================================

struct Rectangle {
    float x, y, width, height;
};

struct Player {
    Rectangle rect;
    float velX, velY;
    bool onGround;
    bool facingRight;
    int lives;
    int score;
    bool isDead;
    float respawnTimer;
    float invincibleTimer;
    float animTimer;
};

struct Platform {
    Rectangle rect;
    bool active;
    bool isMoving;
    float moveSpeed;
    float minX, maxX;
    int direction;
    uint32_t color;
};

struct Coin {
    Rectangle rect;
    bool active;
    float animTimer;
};

struct Enemy {
    Rectangle rect;
    bool active;
    float speed;
    float minX, maxX;
    int direction;
    float animTimer;
};

enum GameState {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER,
    STATE_LEVEL_COMPLETE
};

// ============================================================================
// Global Game State
// ============================================================================

static Player g_player;
static Platform g_platforms[MAX_PLATFORMS];
static Coin g_coins[MAX_COINS];
static Enemy g_enemies[MAX_ENEMIES];
static GameState g_gameState = STATE_TITLE;
static int g_coinsCollected = 0;
static int g_totalCoins = 0;
static int g_currentLevel = 1;
static float g_levelTime = 0.0f;
static float g_gameTimer = 0.0f;

// Camera
static float g_cameraX = 0.0f;
static const float LEVEL_WIDTH = 2000.0f;
static const float LEVEL_HEIGHT = 720.0f;

// Input state
static bool g_buttonAPressed = false;
static bool g_buttonAJustPressed = false;
static bool g_buttonBPressed = false;
static bool g_buttonPlusJustPressed = false;
static float g_leftStickX = 0.0f;
static bool g_dpadLeft = false;
static bool g_dpadRight = false;

// OSScreen buffers
static void* s_tvBuffer = nullptr;
static void* s_drcBuffer = nullptr;
static uint32_t s_tvBufferSize = 0;
static uint32_t s_drcBufferSize = 0;

// ============================================================================
// Utility Functions
// ============================================================================

bool rectsIntersect(const Rectangle& a, const Rectangle& b) {
    return (a.x < b.x + b.width &&
            a.x + a.width > b.x &&
            a.y < b.y + b.height &&
            a.y + a.height > b.y);
}

float clampf(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

// ============================================================================
// Drawing Functions (Software Rendering via OSScreen)
// ============================================================================

void drawRect(OSScreenID screen, int x, int y, int w, int h, uint32_t color) {
    int screenW = (screen == SCREEN_TV) ? TV_WIDTH : DRC_WIDTH;
    int screenH = (screen == SCREEN_TV) ? TV_HEIGHT : DRC_HEIGHT;
    
    // Clamp to screen bounds
    int x1 = (x < 0) ? 0 : x;
    int y1 = (y < 0) ? 0 : y;
    int x2 = (x + w > screenW) ? screenW : x + w;
    int y2 = (y + h > screenH) ? screenH : y + h;
    
    for (int py = y1; py < y2; py++) {
        for (int px = x1; px < x2; px++) {
            OSScreenPutPixelEx(screen, px, py, color);
        }
    }
}

void drawPlayer(OSScreenID screen, int x, int y, int w, int h, bool facingRight, bool blink) {
    if (blink) return; // Invincibility blink
    
    // Body (blue shirt)
    drawRect(screen, x + w/4, y + h/3, w/2, h/3, COLOR_BLUE);
    
    // Head
    drawRect(screen, x + w/4, y + 2, w/2, h/4, COLOR_SKIN);
    
    // Eyes
    int eyeX = facingRight ? (x + w/2 + 2) : (x + w/4);
    drawRect(screen, eyeX, y + 8, 4, 4, COLOR_BLACK);
    
    // Legs (pants)
    drawRect(screen, x + w/4 + 2, y + h*2/3, w/5, h/3 - 2, COLOR_DARK_GRAY);
    drawRect(screen, x + w/2, y + h*2/3, w/5, h/3 - 2, COLOR_DARK_GRAY);
}

void drawEnemy(OSScreenID screen, int x, int y, int w, int h) {
    // Red blob enemy
    drawRect(screen, x + 4, y + h/3, w - 8, h*2/3, COLOR_RED);
    
    // Eyes
    drawRect(screen, x + w/4, y + h/3 + 6, 8, 8, COLOR_WHITE);
    drawRect(screen, x + w*2/3 - 4, y + h/3 + 6, 8, 8, COLOR_WHITE);
    drawRect(screen, x + w/4 + 2, y + h/3 + 8, 3, 3, COLOR_BLACK);
    drawRect(screen, x + w*2/3 - 2, y + h/3 + 8, 3, 3, COLOR_BLACK);
    
    // Spikes
    drawRect(screen, x + w/4, y + h/6, 8, h/5, COLOR_ORANGE);
    drawRect(screen, x + w/2 - 4, y + 4, 8, h/4, COLOR_ORANGE);
    drawRect(screen, x + w*3/4 - 8, y + h/6, 8, h/5, COLOR_ORANGE);
}

void drawCoin(OSScreenID screen, int x, int y, int size, float anim) {
    int bob = (int)(sinf(anim * 4.0f) * 3.0f);
    drawRect(screen, x + 2, y + bob + 2, size - 4, size - 4, COLOR_GOLD);
    drawRect(screen, x + size/3, y + bob + size/4, size/4, size/4, COLOR_YELLOW);
}

void drawPlatform(OSScreenID screen, int x, int y, int w, int h, uint32_t color, bool moving) {
    drawRect(screen, x, y, w, h, color);
    drawRect(screen, x, y, w, 5, COLOR_DARK_GREEN); // Grass top
    
    // Moving indicator
    if (moving) {
        drawRect(screen, x + w/2 - 15, y + h/2, 30, 4, COLOR_YELLOW);
    }
}

void drawBackground(OSScreenID screen, int screenW, int screenH, float camX) {
    // Sky
    drawRect(screen, 0, 0, screenW, screenH, COLOR_LIGHT_BLUE);
    
    // Sun
    int sunX = screenW - 80 - (int)(camX * 0.05f) % 200;
    drawRect(screen, sunX, 40, 50, 50, COLOR_YELLOW);
    
    // Clouds (parallax)
    int cloudOff = (int)(camX * 0.15f);
    for (int i = 0; i < 4; i++) {
        int cx = ((i * 350 - cloudOff) % (screenW + 200)) - 100;
        int cy = 60 + (i % 3) * 25;
        drawRect(screen, cx, cy, 80, 25, COLOR_WHITE);
        drawRect(screen, cx + 15, cy - 10, 50, 20, COLOR_WHITE);
    }
    
    // Ground (at bottom)
    drawRect(screen, 0, screenH - 40, screenW, 40, COLOR_BROWN);
    drawRect(screen, 0, screenH - 40, screenW, 8, COLOR_DARK_GREEN);
}

void drawHUD(OSScreenID screen, int screenW) {
    char buf[64];
    
    // Score
    snprintf(buf, sizeof(buf), "SCORE: %d", g_player.score);
    OSScreenPutFontEx(screen, 1, 0, buf);
    
    // Lives
    snprintf(buf, sizeof(buf), "LIVES: %d", g_player.lives);
    OSScreenPutFontEx(screen, 1, 1, buf);
    
    // Coins
    snprintf(buf, sizeof(buf), "COINS: %d/%d", g_coinsCollected, g_totalCoins);
    OSScreenPutFontEx(screen, 1, 2, buf);
    
    // Level
    snprintf(buf, sizeof(buf), "LEVEL %d", g_currentLevel);
    OSScreenPutFontEx(screen, screenW/16 - 8, 0, buf);
}

// ============================================================================
// Level Setup
// ============================================================================

void initLevel(int level) {
    g_coinsCollected = 0;
    g_totalCoins = 0;
    g_levelTime = 0.0f;
    g_cameraX = 0.0f;
    
    // Reset player position
    g_player.rect = {100.0f, 400.0f, 32.0f, 48.0f};
    g_player.velX = 0.0f;
    g_player.velY = 0.0f;
    g_player.onGround = false;
    g_player.facingRight = true;
    g_player.isDead = false;
    g_player.respawnTimer = 0.0f;
    g_player.invincibleTimer = 0.0f;
    g_player.animTimer = 0.0f;
    
    // Clear platforms
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        g_platforms[i].active = false;
    }
    
    // Clear coins
    for (int i = 0; i < MAX_COINS; i++) {
        g_coins[i].active = false;
    }
    
    // Clear enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        g_enemies[i].active = false;
    }
    
    int pi = 0; // Platform index
    int ci = 0; // Coin index
    int ei = 0; // Enemy index
    
    // === GROUND PLATFORMS ===
    g_platforms[pi++] = {{0, 620, 400, 100}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{480, 620, 600, 100}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{1160, 620, 840, 100}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    
    // === FLOATING PLATFORMS ===
    g_platforms[pi++] = {{180, 500, 120, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{380, 420, 100, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{550, 340, 140, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    
    // Moving platform
    g_platforms[pi++] = {{720, 440, 100, 25}, true, true, 2.0f, 680, 920, 1, COLOR_PURPLE};
    
    // More static platforms
    g_platforms[pi++] = {{920, 360, 120, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{1100, 280, 100, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{1280, 480, 180, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    g_platforms[pi++] = {{1500, 380, 140, 25}, true, false, 0, 0, 0, 1, COLOR_BROWN};
    
    // Extra platforms for higher levels
    if (level >= 2) {
        g_platforms[pi++] = {{1700, 300, 80, 25}, true, true, 2.5f, 1650, 1850, 1, COLOR_PURPLE};
    }
    
    // === COINS ===
    g_coins[ci++] = {{220, 460, 24, 24}, true, 0.0f};
    g_coins[ci++] = {{410, 380, 24, 24}, true, 0.3f};
    g_coins[ci++] = {{600, 300, 24, 24}, true, 0.6f};
    g_coins[ci++] = {{770, 400, 24, 24}, true, 0.9f};
    g_coins[ci++] = {{970, 320, 24, 24}, true, 1.2f};
    g_coins[ci++] = {{1130, 240, 24, 24}, true, 1.5f};
    g_coins[ci++] = {{1350, 440, 24, 24}, true, 1.8f};
    g_coins[ci++] = {{550, 580, 24, 24}, true, 2.1f};
    g_coins[ci++] = {{700, 580, 24, 24}, true, 2.4f};
    g_coins[ci++] = {{1300, 580, 24, 24}, true, 2.7f};
    
    g_totalCoins = ci;
    
    // === ENEMIES ===
    g_enemies[ei++] = {{580, 580, 40, 40}, true, 1.5f, 500, 720, 1, 0.0f};
    g_enemies[ei++] = {{1300, 580, 40, 40}, true, 1.8f, 1200, 1450, -1, 0.5f};
    
    if (level >= 2) {
        g_enemies[ei++] = {{940, 320, 40, 40}, true, 1.2f, 920, 1020, 1, 1.0f};
    }
}

void resetGame() {
    g_player.lives = PLAYER_START_LIVES;
    g_player.score = 0;
    g_currentLevel = 1;
    initLevel(g_currentLevel);
}

// ============================================================================
// Input Processing
// ============================================================================

void processInput() {
    VPADStatus vpad;
    VPADReadError error;
    
    VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
    
    if (error == VPAD_READ_SUCCESS) {
        g_buttonAJustPressed = (vpad.trigger & VPAD_BUTTON_A) != 0;
        g_buttonPlusJustPressed = (vpad.trigger & VPAD_BUTTON_PLUS) != 0;
        
        g_buttonAPressed = (vpad.hold & VPAD_BUTTON_A) != 0;
        g_buttonBPressed = (vpad.hold & VPAD_BUTTON_B) != 0;
        
        g_dpadLeft = (vpad.hold & VPAD_BUTTON_LEFT) != 0;
        g_dpadRight = (vpad.hold & VPAD_BUTTON_RIGHT) != 0;
        
        g_leftStickX = vpad.leftStick.x;
    }
}

// ============================================================================
// Game Update Logic
// ============================================================================

void updatePlayer(float dt) {
    if (g_player.isDead) {
        g_player.respawnTimer -= dt;
        if (g_player.respawnTimer <= 0) {
            if (g_player.lives > 0) {
                g_player.isDead = false;
                g_player.rect.x = 100.0f;
                g_player.rect.y = 400.0f;
                g_player.velX = 0.0f;
                g_player.velY = 0.0f;
                g_player.invincibleTimer = 2.0f;
            } else {
                g_gameState = STATE_GAME_OVER;
            }
        }
        return;
    }
    
    if (g_player.invincibleTimer > 0) {
        g_player.invincibleTimer -= dt;
    }
    
    g_player.animTimer += dt;
    
    // Horizontal movement
    float moveDir = 0.0f;
    if (g_dpadRight || g_leftStickX > 0.25f) {
        moveDir = 1.0f;
        g_player.facingRight = true;
    } else if (g_dpadLeft || g_leftStickX < -0.25f) {
        moveDir = -1.0f;
        g_player.facingRight = false;
    }
    
    float speed = g_buttonBPressed ? PLAYER_SPRINT_SPEED : PLAYER_SPEED;
    g_player.velX = moveDir * speed;
    
    // Jumping
    if (g_buttonAJustPressed && g_player.onGround) {
        g_player.velY = JUMP_FORCE;
        g_player.onGround = false;
    }
    
    // Variable jump height
    if (!g_buttonAPressed && g_player.velY < -4.0f) {
        g_player.velY = -4.0f;
    }
    
    // Gravity
    g_player.velY += GRAVITY;
    if (g_player.velY > MAX_FALL_SPEED) {
        g_player.velY = MAX_FALL_SPEED;
    }
    
    // Move
    g_player.rect.x += g_player.velX;
    g_player.rect.y += g_player.velY;
    
    // Platform collision
    g_player.onGround = false;
    
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!g_platforms[i].active) continue;
        
        Rectangle& plat = g_platforms[i].rect;
        
        if (rectsIntersect(g_player.rect, plat)) {
            float overlapTop = (g_player.rect.y + g_player.rect.height) - plat.y;
            float overlapBottom = (plat.y + plat.height) - g_player.rect.y;
            float overlapLeft = (g_player.rect.x + g_player.rect.width) - plat.x;
            float overlapRight = (plat.x + plat.width) - g_player.rect.x;
            
            float minY = (overlapTop < overlapBottom) ? overlapTop : overlapBottom;
            float minX = (overlapLeft < overlapRight) ? overlapLeft : overlapRight;
            
            if (minY < minX) {
                if (overlapTop < overlapBottom && g_player.velY > 0) {
                    // Landing
                    g_player.rect.y = plat.y - g_player.rect.height;
                    g_player.velY = 0;
                    g_player.onGround = true;
                    
                    // Move with moving platform
                    if (g_platforms[i].isMoving) {
                        g_player.rect.x += g_platforms[i].moveSpeed * g_platforms[i].direction;
                    }
                } else if (g_player.velY < 0) {
                    // Bump head
                    g_player.rect.y = plat.y + plat.height;
                    g_player.velY = 0;
                }
            } else {
                // Side collision
                if (overlapLeft < overlapRight) {
                    g_player.rect.x = plat.x - g_player.rect.width;
                } else {
                    g_player.rect.x = plat.x + plat.width;
                }
                g_player.velX = 0;
            }
        }
    }
    
    // Bounds
    if (g_player.rect.x < 0) g_player.rect.x = 0;
    if (g_player.rect.x > LEVEL_WIDTH - g_player.rect.width) {
        g_player.rect.x = LEVEL_WIDTH - g_player.rect.width;
    }
    
    // Fell off screen
    if (g_player.rect.y > LEVEL_HEIGHT + 50) {
        g_player.lives--;
        g_player.isDead = true;
        g_player.respawnTimer = 1.5f;
    }
}

void updatePlatforms(float dt) {
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!g_platforms[i].active || !g_platforms[i].isMoving) continue;
        
        Platform& p = g_platforms[i];
        p.rect.x += p.moveSpeed * p.direction;
        
        if (p.rect.x <= p.minX) {
            p.rect.x = p.minX;
            p.direction = 1;
        } else if (p.rect.x >= p.maxX) {
            p.rect.x = p.maxX;
            p.direction = -1;
        }
    }
}

void updateCoins(float dt) {
    for (int i = 0; i < MAX_COINS; i++) {
        if (!g_coins[i].active) continue;
        
        g_coins[i].animTimer += dt;
        
        if (rectsIntersect(g_player.rect, g_coins[i].rect)) {
            g_coins[i].active = false;
            g_coinsCollected++;
            g_player.score += 100;
            
            if (g_coinsCollected >= g_totalCoins) {
                g_gameState = STATE_LEVEL_COMPLETE;
            }
        }
    }
}

void updateEnemies(float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) continue;
        
        Enemy& e = g_enemies[i];
        e.animTimer += dt;
        
        e.rect.x += e.speed * e.direction;
        
        if (e.rect.x <= e.minX) {
            e.rect.x = e.minX;
            e.direction = 1;
        } else if (e.rect.x >= e.maxX) {
            e.rect.x = e.maxX;
            e.direction = -1;
        }
        
        // Collision with player
        if (!g_player.isDead && g_player.invincibleTimer <= 0 &&
            rectsIntersect(g_player.rect, e.rect)) {
            
            // Stomp from above?
            if (g_player.velY > 0 &&
                g_player.rect.y + g_player.rect.height < e.rect.y + e.rect.height/2) {
                // Kill enemy
                e.active = false;
                g_player.velY = JUMP_FORCE * 0.6f;
                g_player.score += 200;
            } else {
                // Player hit
                g_player.lives--;
                g_player.isDead = true;
                g_player.respawnTimer = 1.5f;
            }
        }
    }
}

void updateCamera() {
    float targetX = g_player.rect.x - TV_WIDTH / 2 + g_player.rect.width / 2;
    g_cameraX += (targetX - g_cameraX) * 0.08f;
    g_cameraX = clampf(g_cameraX, 0, LEVEL_WIDTH - TV_WIDTH);
}

void updateGame(float dt) {
    g_gameTimer += dt;
    g_levelTime += dt;
    
    processInput();
    
    switch (g_gameState) {
        case STATE_TITLE:
            if (g_buttonAJustPressed) {
                resetGame();
                g_gameState = STATE_PLAYING;
            }
            break;
            
        case STATE_PLAYING:
            if (g_buttonPlusJustPressed) {
                g_gameState = STATE_PAUSED;
                break;
            }
            updatePlayer(dt);
            updatePlatforms(dt);
            updateCoins(dt);
            updateEnemies(dt);
            updateCamera();
            break;
            
        case STATE_PAUSED:
            if (g_buttonPlusJustPressed) {
                g_gameState = STATE_PLAYING;
            }
            break;
            
        case STATE_GAME_OVER:
            if (g_buttonAJustPressed) {
                g_gameState = STATE_TITLE;
            }
            break;
            
        case STATE_LEVEL_COMPLETE:
            if (g_buttonAJustPressed) {
                g_currentLevel++;
                int timeBonus = (int)(1000 - g_levelTime * 5);
                if (timeBonus > 0) g_player.score += timeBonus;
                initLevel(g_currentLevel);
                g_gameState = STATE_PLAYING;
            }
            break;
    }
}

// ============================================================================
// Rendering
// ============================================================================

void renderGame(OSScreenID screen, int screenW, int screenH) {
    float scaleX = (float)screenW / TV_WIDTH;
    float scaleY = (float)screenH / TV_HEIGHT;
    
    // Background
    drawBackground(screen, screenW, screenH, g_cameraX);
    
    // Platforms
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!g_platforms[i].active) continue;
        
        int px = (int)((g_platforms[i].rect.x - g_cameraX) * scaleX);
        int py = (int)(g_platforms[i].rect.y * scaleY);
        int pw = (int)(g_platforms[i].rect.width * scaleX);
        int ph = (int)(g_platforms[i].rect.height * scaleY);
        
        if (px + pw >= 0 && px < screenW) {
            drawPlatform(screen, px, py, pw, ph, g_platforms[i].color, g_platforms[i].isMoving);
        }
    }
    
    // Coins
    for (int i = 0; i < MAX_COINS; i++) {
        if (!g_coins[i].active) continue;
        
        int cx = (int)((g_coins[i].rect.x - g_cameraX) * scaleX);
        int cy = (int)(g_coins[i].rect.y * scaleY);
        int cs = (int)(g_coins[i].rect.width * scaleX);
        
        if (cx + cs >= 0 && cx < screenW) {
            drawCoin(screen, cx, cy, cs, g_coins[i].animTimer);
        }
    }
    
    // Enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) continue;
        
        int ex = (int)((g_enemies[i].rect.x - g_cameraX) * scaleX);
        int ey = (int)(g_enemies[i].rect.y * scaleY);
        int ew = (int)(g_enemies[i].rect.width * scaleX);
        int eh = (int)(g_enemies[i].rect.height * scaleY);
        
        if (ex + ew >= 0 && ex < screenW) {
            drawEnemy(screen, ex, ey, ew, eh);
        }
    }
    
    // Player
    if (!g_player.isDead) {
        int px = (int)((g_player.rect.x - g_cameraX) * scaleX);
        int py = (int)(g_player.rect.y * scaleY);
        int pw = (int)(g_player.rect.width * scaleX);
        int ph = (int)(g_player.rect.height * scaleY);
        
        bool blink = g_player.invincibleTimer > 0 && ((int)(g_player.animTimer * 10) % 2 == 0);
        drawPlayer(screen, px, py, pw, ph, g_player.facingRight, blink);
    }
    
    // HUD
    drawHUD(screen, screenW);
}

void renderTitle(OSScreenID screen, int screenW, int screenH) {
    drawRect(screen, 0, 0, screenW, screenH, COLOR_DARK_GRAY);
    
    OSScreenPutFontEx(screen, screenW/32 - 10, 6, "================================");
    OSScreenPutFontEx(screen, screenW/32 - 8, 8, "WII U PLATFORMER");
    OSScreenPutFontEx(screen, screenW/32 - 10, 10, "================================");
    
    OSScreenPutFontEx(screen, screenW/32 - 8, 14, "Press A to Start!");
    
    OSScreenPutFontEx(screen, screenW/32 - 10, 18, "Controls:");
    OSScreenPutFontEx(screen, screenW/32 - 10, 19, "  D-Pad/Stick: Move");
    OSScreenPutFontEx(screen, screenW/32 - 10, 20, "  A: Jump");
    OSScreenPutFontEx(screen, screenW/32 - 10, 21, "  B: Sprint");
    OSScreenPutFontEx(screen, screenW/32 - 10, 22, "  +: Pause");
    
    // Preview player
    int previewX = screenW / 2 - 16;
    int previewY = screenH / 2 + 60;
    drawPlayer(screen, previewX, previewY, 32, 48, true, false);
}

void renderPaused(OSScreenID screen, int screenW, int screenH) {
    // Darken overlay
    for (int y = 0; y < screenH; y += 4) {
        drawRect(screen, 0, y, screenW, 2, COLOR_BLACK);
    }
    
    int boxX = screenW/2 - 120;
    int boxY = screenH/2 - 40;
    drawRect(screen, boxX, boxY, 240, 80, COLOR_DARK_GRAY);
    
    OSScreenPutFontEx(screen, screenW/32 - 3, screenH/32/2, "PAUSED");
    OSScreenPutFontEx(screen, screenW/32 - 7, screenH/32/2 + 2, "Press + to Resume");
}

void renderGameOver(OSScreenID screen, int screenW, int screenH) {
    drawRect(screen, 0, 0, screenW, screenH, COLOR_BLACK);
    
    OSScreenPutFontEx(screen, screenW/32 - 5, screenH/32/2 - 2, "GAME OVER");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Final Score: %d", g_player.score);
    OSScreenPutFontEx(screen, screenW/32 - 8, screenH/32/2 + 1, buf);
    
    OSScreenPutFontEx(screen, screenW/32 - 10, screenH/32/2 + 4, "Press A for Title Screen");
}

void renderLevelComplete(OSScreenID screen, int screenW, int screenH) {
    drawRect(screen, 0, 0, screenW, screenH, COLOR_DARK_GREEN);
    
    OSScreenPutFontEx(screen, screenW/32 - 7, screenH/32/2 - 2, "LEVEL COMPLETE!");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Score: %d", g_player.score);
    OSScreenPutFontEx(screen, screenW/32 - 6, screenH/32/2 + 1, buf);
    
    OSScreenPutFontEx(screen, screenW/32 - 10, screenH/32/2 + 4, "Press A for Next Level");
}

void render() {
    OSScreenClearBufferEx(SCREEN_TV, COLOR_BLACK);
    OSScreenClearBufferEx(SCREEN_DRC, COLOR_BLACK);
    
    switch (g_gameState) {
        case STATE_TITLE:
            renderTitle(SCREEN_TV, TV_WIDTH, TV_HEIGHT);
            renderTitle(SCREEN_DRC, DRC_WIDTH, DRC_HEIGHT);
            break;
            
        case STATE_PLAYING:
            renderGame(SCREEN_TV, TV_WIDTH, TV_HEIGHT);
            renderGame(SCREEN_DRC, DRC_WIDTH, DRC_HEIGHT);
            break;
            
        case STATE_PAUSED:
            renderGame(SCREEN_TV, TV_WIDTH, TV_HEIGHT);
            renderGame(SCREEN_DRC, DRC_WIDTH, DRC_HEIGHT);
            renderPaused(SCREEN_TV, TV_WIDTH, TV_HEIGHT);
            renderPaused(SCREEN_DRC, DRC_WIDTH, DRC_HEIGHT);
            break;
            
        case STATE_GAME_OVER:
            renderGameOver(SCREEN_TV, TV_WIDTH, TV_HEIGHT);
            renderGameOver(SCREEN_DRC, DRC_WIDTH, DRC_HEIGHT);
            break;
            
        case STATE_LEVEL_COMPLETE:
            renderLevelComplete(SCREEN_TV, TV_WIDTH, TV_HEIGHT);
            renderLevelComplete(SCREEN_DRC, DRC_WIDTH, DRC_HEIGHT);
            break;
    }
    
    DCFlushRange(s_tvBuffer, s_tvBufferSize);
    DCFlushRange(s_drcBuffer, s_drcBufferSize);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    WHBProcInit();
    VPADInit();
    OSScreenInit();
    
    s_tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    s_drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    
    s_tvBuffer = MEMAllocFromDefaultHeapEx(s_tvBufferSize, 0x100);
    s_drcBuffer = MEMAllocFromDefaultHeapEx(s_drcBufferSize, 0x100);
    
    if (!s_tvBuffer || !s_drcBuffer) {
        WHBProcShutdown();
        return 1;
    }
    
    OSScreenSetBufferEx(SCREEN_TV, s_tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, s_drcBuffer);
    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);
    
    g_gameState = STATE_TITLE;
    g_gameTimer = 0.0f;
    
    OSTick lastTick = OSGetTick();
    
    while (WHBProcIsRunning()) {
        OSTick currentTick = OSGetTick();
        float dt = (float)OSTicksToMicroseconds(currentTick - lastTick) / 1000000.0f;
        lastTick = currentTick;
        
        if (dt > 0.1f) dt = 0.1f;
        
        updateGame(dt);
        render();
    }
    
    if (s_tvBuffer) MEMFreeToDefaultHeap(s_tvBuffer);
    if (s_drcBuffer) MEMFreeToDefaultHeap(s_drcBuffer);
    
    OSScreenShutdown();
    VPADShutdown();
    WHBProcShutdown();
    
    return 0;
}
