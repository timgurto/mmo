// (C) 2015 Tim Gurto

#ifndef USER_H
#define USER_H

#include <iostream>
#include <string>
#include <windows.h>

#include "Object.h"
#include "Point.h"
#include "Socket.h"

class Item;
class Server;

// Stores information about a single user account for the server
class User{
    std::string _name;
    Socket _socket;
    Point _location;
    const Object *_actionTarget; // Points to the object that the user is acting upon.
    const Item *_actionCrafting; // The item this user is currently crafting.
    const ObjectType *_actionConstructing; // The object this user is currently constructing.
    Uint32 _actionTime; // Time remaining on current action.
    std::vector<std::pair<const Item *, size_t> > _inventory;

    Uint32 _lastLocUpdate; // Time that the last CL_LOCATION was received
    Uint32 _lastContact;
    Uint32 _latency;

public:
    User(const std::string &name, const Point &loc, const Socket &socket);
    User(const Socket &rhs); // for use with set::find(), allowing find-by-socket

    bool operator<(const User &rhs) const { return _socket < rhs._socket; }

    const std::string &name() const { return _name; }
    const Socket &socket() const { return _socket; }
    const Point &location() const { return _location; }
    void location(const Point &loc) { _location = loc; }
    void location(std::istream &is); // Read co-ordinates from stream
    const std::pair<const Item *, size_t> &inventory(size_t index) const;
    std::pair<const Item *, size_t> &inventory(size_t index);

    const Object *actionTarget() const { return _actionTarget; }
    void actionTarget(const Object *object); // Configure user to perform an action on an object

    // Whether the user has enough materials to craft an item
    bool hasMaterials(const Item &item) const;
    void removeMaterials(const Item &item, Server &server);
    void actionCraft(const Item &item); // Configure user to craft an item

    // Configure user to construct an item
    void actionConstruct(const ObjectType &obj, const Point &location, size_t slot);

    void cancelAction(Server &server); // Cancel any action in progress, and alert the client

    std::string makeLocationCommand() const;

    static const size_t INVENTORY_SIZE;

    void contact();
    bool alive() const; // Whether the client has contacted the server recently enough

    /*
    Determine whether the proposed new location is legal, considering movement speed and
    time elapsed.
    Set location to the new, legal location.
    */
    void updateLocation(const Point &dest, const Server &server);

    // Whether the user has at least one item with the specified item class
    bool hasItemClass(const std::string &className) const;

    // Return value: which inventory slot was used
    size_t giveItem(const Item *item);

    void update(Uint32 timeElapsed, Server &server);
};

#endif
