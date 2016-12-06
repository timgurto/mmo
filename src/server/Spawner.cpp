#include "Spawner.h"

Spawner::Spawner(const Point &location, const ObjectType *type):
    _location(location),
    _type(type),

    _radius(0),
    _quantity(1),
    _respawnTime(0){}
