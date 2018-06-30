#ifndef ENTITY_TYPE_H
#define ENTITY_TYPE_H

#include <string>

#include "../Rect.h"
#include "../Stats.h"
#include "TerrainList.h"

class EntityType {
 public:
  EntityType(const std::string id);
  const std::string &id() const { return _id; }
  bool isTag(const std::string &tagName) const;
  void addTag(const std::string &tagName);
  virtual char classTag() const = 0;

  // Space
  bool collides() const { return _collides; }
  const MapRect &collisionRect() const { return _collisionRect; }
  void collisionRect(const MapRect &r) {
    _collisionRect = r;
    _collides = true;
  }
  const TerrainList &allowedTerrain() const;
  void allowedTerrain(const std::string &id) {
    _allowedTerrain = TerrainList::findList(id);
  }

  // Combat
  void baseStats(const Stats &stats) { _baseStats = stats; }
  const Stats &baseStats() const { return _baseStats; }

 protected:
  mutable Stats _baseStats{};

 private:
  std::string _id;
  std::set<std::string> _tags;

  bool operator<(const EntityType &rhs) const { return _id < rhs._id; }

  // Space
  bool _collides;  // false by default; true if any collisionRect is specified.
  MapRect _collisionRect;  // Relative to position

  const TerrainList *_allowedTerrain;
};

#endif
