#include "ThreatTable.h"
#include "Server.h"

void ThreatTable::makeAwareOf(Entity& entity) {
  auto it = _container.find(&entity);
  if (it == _container.end()) _container[&entity] = 0;
}

void ThreatTable::forgetAbout(const Entity& entity) {
  auto& nonConstRef = const_cast<Entity&>(entity);
  auto it = _container.find(&nonConstRef);
  if (it == _container.end()) return;
  _container.erase(it);
}

void ThreatTable::addThreat(Entity& entity, Threat amount) {
  auto it = _container.find(&entity);
  if (it == _container.end())
    _container[&entity] = amount;
  else
    it->second += amount;
}

void ThreatTable::scaleThreat(Entity& entity, double multiplier) {
  auto it = _container.find(&entity);
  if (it == _container.end())
    return;
  else
    it->second = toInt(it->second * multiplier);
}

Entity* ThreatTable::getTarget() {
  auto highestThreat = -1;
  Entity* target = nullptr;

  for (auto pair : _container)
    if (pair.second > highestThreat) {
      highestThreat = pair.second;
      target = pair.first;
    }

  return target;
}

void ThreatTable::clear() { _container.clear(); }
