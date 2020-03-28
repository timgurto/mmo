#include <list>
#include <utility>

#include "CollisionChunk.h"
#include "Entity.h"
#include "Server.h"

const px_t Server::COLLISION_CHUNK_SIZE = 160;

bool Server::isLocationValid(const MapPoint &loc, const EntityType &type) {
  auto rect = type.collisionRect() + loc;
  return isLocationValid(rect, type.allowedTerrain());
}

bool Server::isLocationValid(const MapPoint &loc, const Entity &thisEntity) {
  auto rect = thisEntity.type()->collisionRect() + loc;
  return isLocationValid(rect, thisEntity);
}

bool Server::isLocationValid(const MapRect &rect, const Entity &thisEntity) {
  return isLocationValid(rect, thisEntity.allowedTerrain(), &thisEntity);
}

bool Server::isLocationValid(const MapRect &rect,
                             const TerrainList &allowedTerrain,
                             const Entity *thisEntity) {
  // A user in a vehicle is unrestricted; the vehicle's restrictions will
  // dictate his location.
  if (thisEntity && !thisEntity->collides()) return true;

  const double right = rect.x + rect.w, bottom = rect.y + rect.h;
  // Map edges
  const double xLimit = _map.width() * Map::TILE_W - Map::TILE_W / 2,
               yLimit = _map.height() * Map::TILE_H;
  if (rect.x < 0 || right > xLimit || rect.y < 0 || bottom > yLimit) {
    return false;
  }

  // Terrain
  auto terrainTypesCovered = _map.terrainTypesOverlapping(rect);
  for (char terrainType : terrainTypesCovered)
    if (!allowedTerrain.allows(terrainType)) return false;

  // Objects
  MapPoint rectCenter(rect.x + rect.w / 2, rect.y + rect.h / 2);
  auto superChunk = getCollisionSuperChunk(rectCenter);
  for (CollisionChunk *chunk : superChunk)
    for (const auto &pair : chunk->entities()) {
      const Entity *pEnt = pair.second;
      if (pEnt == thisEntity) continue;
      if (!pEnt->collides()) continue;

      // Allow collisions between users and users/NPCs
      if (thisEntity && thisEntity->classTag() == 'u' &&
          (pEnt->classTag() == 'u' || pEnt->classTag() == 'n'))
        continue;

      if (thisEntity && thisEntity->classTag() == 'n' &&
          pEnt->classTag() == 'u')
        continue;

      if (rect.collides(pEnt->collisionRect())) return false;
    }

  return true;
}

std::pair<size_t, size_t> Server::getTileCoords(const MapPoint &p) const {
  size_t y = _map.getRow(p.y), x = _map.getCol(p.x, y);
  return std::make_pair(x, y);
}

char Server::findTile(const MapPoint &p) const {
  auto coords = getTileCoords(p);
  return _map[coords.first][coords.second];
}

CollisionChunk &Server::getCollisionChunk(const MapPoint &p) {
  size_t x = static_cast<size_t>(p.x / COLLISION_CHUNK_SIZE),
         y = static_cast<size_t>(p.y / COLLISION_CHUNK_SIZE);
  return _collisionGrid[x][y];
}

std::list<CollisionChunk *> Server::getCollisionSuperChunk(const MapPoint &p) {
  size_t x = static_cast<size_t>(p.x / COLLISION_CHUNK_SIZE), minX = x - 1,
         maxX = x + 1, y = static_cast<size_t>(p.y / COLLISION_CHUNK_SIZE),
         minY = y - 1, maxY = y + 1;
  if (x == 0) minX = 0;
  if (y == 0) minY = 0;
  std::list<CollisionChunk *> superChunk;
  for (size_t x = minX; x <= maxX; ++x)
    for (size_t y = minY; y <= maxY; ++y)
      superChunk.push_back(&_collisionGrid[x][y]);
  return superChunk;
}
