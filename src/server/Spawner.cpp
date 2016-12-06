#include "Server.h"
#include "Spawner.h"

Spawner::Spawner(const Point &location, const ObjectType *type):
    _location(location),
    _type(type),

    _radius(0),
    _quantity(1),
    _respawnTime(0){}

void Spawner::spawn(Server &server){
    static const size_t MAX_ATTEMPTS = 20;

    for (size_t attempt = 0; attempt != MAX_ATTEMPTS; ++attempt){

        // Choose location
        Point p = _location;
        // Random point in circle
        if (_radius != 0){
            double radius = sqrt(randDouble()) * _radius;
            double angle = randDouble() * 2 * PI;
            p.x += cos(angle) * radius;
            p.y -= sin(angle) * radius;
        }

        // Check terrain whitelist
        if (!_terrainWhitelist.empty()){
            size_t terrain = server.findTile(p);
            if (_terrainWhitelist.find(terrain) == _terrainWhitelist.end())
                continue;
        }

        // Check location validity
        if (!server.isLocationValid(p, *_type))
            continue;

        // Add object;
        if (_type->classTag() == 'n')
            server.addNPC(dynamic_cast<const NPCType *>(_type), p);
        else
            server.addObject(_type, p);
        return;

    }

    server._debug << Color::YELLOW << "Failed to spawn object " << _type->id() << "." << Log::endl;
}

