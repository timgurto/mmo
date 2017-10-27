#pragma once

#include "Sprite.h"

using namespace std::string_literals;

// A sprite that travels in a straight line before disappearing.
class Projectile : public Sprite {
public:
    struct Type;

    Projectile(const Type &type, const Point &start, const Point &end) :
        Sprite(&type, start), _end(end) {}

    virtual void update(double delta) override;

    using onReachDestination_t = void (*)(const Point &destination, const std::string &arg);
    void onReachDestination(onReachDestination_t f, const std::string &arg)
            { _onReachDestination = f; _onReachDestinationArg = arg; }

private:
    const Type &projectileType() const { return * dynamic_cast<const Type *>(this->type()); }
    double speed() const { return projectileType().speed; }

    Point _end;
    std::string _onReachDestinationArg = {};
    onReachDestination_t _onReachDestination = nullptr;


public:
    struct Type : public SpriteType {
        Type(const std::string &id, const Rect &drawRect) :
            SpriteType(drawRect, "Images/projectiles/"s +id + ".png"s), id(id) {
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
