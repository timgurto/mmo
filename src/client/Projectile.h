#pragma once

#include "Sprite.h"

using namespace std::string_literals;

// A sprite that travels in a straight line before disappearing.
class Projectile : public Sprite {
public:
    struct Type;

    Projectile(const Type &type, const MapPoint &start, const MapPoint &end) :
        Sprite(&type, start), _end(end) {}

    virtual void update(double delta) override;

    using onReachDestination_t = void (*)(const MapPoint &destination, const void *arg);
    void onReachDestination(onReachDestination_t f, const void *arg = nullptr)
            { _onReachDestination = f; _onReachDestinationArg = arg; }

private:
    const Type &projectileType() const { return * dynamic_cast<const Type *>(this->type()); }
    double speed() const { return projectileType().speed; }

    MapPoint _end;
    const void *_onReachDestinationArg = nullptr;
    onReachDestination_t _onReachDestination = nullptr;


public:
    struct Type : public SpriteType {
        Type(const std::string &id, const ScreenRect &drawRect) :
            SpriteType(drawRect, "Images/Projectiles/"s + id + ".png"s), id(id) {
        }
        static Type Dummy(const std::string &id) { return Type{ id, {} }; }

        struct ptrCompare {
            bool operator()(const Type *lhs, const Type *rhs) const {
                return lhs->id < rhs->id;
            }
        };

        std::string id;
        double speed = 0;
        std::string particlesAtEnd = {};
    };
};
