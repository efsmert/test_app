#pragma once

#include <cstdint>

constexpr int TILE = 16;
constexpr int MAP_W = 512;
constexpr int MAP_H = 15;

enum Tile : uint8_t {
  T_EMPTY = 0,
  T_GROUND,
  T_BRICK,
  T_QUESTION,
  T_USED,
  T_PIPE,
  T_FLAG,
  T_CASTLE,
  T_COIN
};

enum Power { P_SMALL = 0, P_BIG, P_FIRE };
enum EType {
  E_NONE = 0,
  E_GOOMBA,
  E_KOOPA,
  E_KOOPA_RED,
  E_BUZZY_BEETLE,
  E_BLOOPER,
  E_SPINY,
  E_LAKITU,
  E_HAMMER_BRO,
  E_HAMMER,
  E_MUSHROOM,
  E_FIRE_FLOWER,
  E_FIREBALL,
  E_COIN_POPUP,

  // Additional enemies / level objects from the Godot project.
  E_CHEEP_SWIM,
  E_CHEEP_LEAP,
  E_BULLET_BILL,
  E_BULLET_CANNON,
  E_BOWSER,

  // Moving platforms.
  E_PLATFORM_SIDEWAYS,
  E_PLATFORM_VERTICAL,
  E_PLATFORM_ROPE,
  E_PLATFORM_FALLING,

  // Castle completion trigger (axe at the end of Bowser bridges).
  E_CASTLE_AXE,

  // Off-screen / trigger-only generators.
  E_ENTITY_GENERATOR,
  E_ENTITY_GENERATOR_STOP
};
enum LevelTheme {
  THEME_OVERWORLD = 0,
  THEME_UNDERGROUND,
  THEME_CASTLE,
  THEME_UNDERWATER,
  THEME_AIRSHIP,
  THEME_DESERT,
  THEME_SNOW,
  THEME_JUNGLE,
  THEME_BEACH,
  THEME_GARDEN,
  THEME_MOUNTAIN,
  THEME_SKY,
  THEME_AUTUMN,
  THEME_PIPELAND,
  THEME_SPACE,
  THEME_VOLCANO,
  THEME_GHOSTHOUSE,
  THEME_CASTLE_WATER,
  THEME_BONUS,
  THEME_COUNT
};
