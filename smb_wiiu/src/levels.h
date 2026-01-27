#pragma once

#include "game_types.h"

struct PipeLink {
  int x;
  int y;
  int targetLevel;
  int targetSection;
  int targetX;
  int targetY;
  int enterDir;
};

struct EnemySpawn {
  EType type;
  float x;
  float y;
  int dir;
};

struct LevelSectionRuntime {
  LevelTheme theme;
  int flagX;
  bool hasFlag;
  const uint8_t (*atlasT)[MAP_W];
  const uint8_t (*atlasX)[MAP_W];
  const uint8_t (*atlasY)[MAP_W];
  const uint8_t (*collide)[MAP_W];
  const uint8_t (*qmeta)[MAP_W];
  const PipeLink *pipes;
  int pipeCount;
  const EnemySpawn *enemies;
  int enemyCount;
  int world;
  int stage;
  const char *name;
  int startX;
  int startY;
  int mapWidth;
  int mapHeight;
  int bgPrimary;
  int bgSecondary;
  bool bgClouds;
};

struct LevelSectionData {
  const uint8_t (*map)[MAP_W];
  const uint8_t (*atlasT)[MAP_W];
  const uint8_t (*atlasX)[MAP_W];
  const uint8_t (*atlasY)[MAP_W];
  const uint8_t (*collide)[MAP_W];
  const uint8_t (*qmeta)[MAP_W];
  int mapWidth;
  int mapHeight;
  LevelTheme theme;
  int flagX;
  bool hasFlag;
  int startX;
  int startY;
  int bgPrimary;
  int bgSecondary;
  bool bgClouds;
  const PipeLink *pipes;
  int pipeCount;
  const EnemySpawn *enemies;
  int enemyCount;
};

struct LevelData {
  const char *name;
  int world;
  int stage;
  const LevelSectionData *sections;
  int sectionCount;
};

bool loadLevelSection(int levelIndex, int sectionIndex,
                      uint8_t map[MAP_H][MAP_W], LevelSectionRuntime &out);
int levelCount();
