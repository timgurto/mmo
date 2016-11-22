#ifndef AVATAR_H
#define AVATAR_H

#include <string>

#include "Entity.h"
#include "../Point.h"

// The client-side representation of a user, including the player
class Avatar : public Entity{
    static std::map<std::string, EntityType> _classes;
    static const Rect COLLISION_RECT, DRAW_RECT;

    Point _destination;
    std::string _name;
    std::string _class;

public:
    Avatar(const std::string &name = "", const Point &location = 0);

    void name(const std::string &newName) { _name = newName; }
    const std::string &name() const { return _name; }
    const Point &destination() const { return _destination; }
    void destination(const Point &dst) { _destination = dst; }
    const Rect &collisionRect() const { return COLLISION_RECT + location(); }
    void setClass(const std::string &c);
    const std::string &getClass() const { return _class; }

    virtual void draw(const Client &client) const override;
    virtual void update(double delta) override;
    virtual std::vector<std::string> getTooltipMessages(const Client &client) const override;

    static void cleanup();

private:
    /*
    Get the next location towards destination, with distance determined by
    this client's latency, and by time elapsed.
    This is used to smooth the apparent movement of other users.
    */
    Point interpolatedLocation(double delta);
};

#endif
