#pragma once

#include <map>

#include "../../../src/client/Texture.h"

struct EntityType {
  enum Category { OBJECT, NPC };
  Category category;
  Texture image;
  ScreenRect drawRect;
  ScreenRect collisionRect;

  using Container = std::map<std::string, EntityType>;
  static void load(Container &container, const std::string &filename);
};
