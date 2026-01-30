#include "levels.h"

#include <cstring>

extern const LevelData g_levels[];
extern const int g_levelCount;

int levelCount() { return g_levelCount; }

bool loadLevelSection(int levelIndex, int sectionIndex,
                      uint8_t map[MAP_H][MAP_W], LevelSectionRuntime &out) {
  if (levelIndex < 0 || levelIndex >= g_levelCount)
    return false;
  const LevelData &level = g_levels[levelIndex];
  if (sectionIndex < 0 || sectionIndex >= level.sectionCount)
    return false;
  const LevelSectionData &section = level.sections[sectionIndex];

  std::memset(map, T_EMPTY, MAP_W * MAP_H);
  if (section.map)
    std::memcpy(map, section.map, sizeof(uint8_t) * MAP_W * MAP_H);

  out.theme = section.theme;
  out.flagX = section.flagX;
  out.hasFlag = section.hasFlag;
  out.atlasT = section.atlasT;
  out.atlasX = section.atlasX;
  out.atlasY = section.atlasY;
  out.collide = section.collide;
  out.qmeta = section.qmeta;
  out.pipes = section.pipes;
  out.pipeCount = section.pipeCount;
  out.enemies = section.enemies;
  out.enemyCount = section.enemyCount;
  out.world = level.world;
  out.stage = level.stage;
  out.name = level.name;
  out.startX = section.startX;
  out.startY = section.startY;
  out.mapWidth = section.mapWidth;
  out.mapHeight = section.mapHeight;
  out.bgPrimary = section.bgPrimary;
  out.bgSecondary = section.bgSecondary;
  out.bgClouds = section.bgClouds;
  out.bgParticles = section.bgParticles;
  return true;
}
