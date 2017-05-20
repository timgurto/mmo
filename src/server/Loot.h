#ifndef LOOT_H
#define LOOT_H

#include "ServerItem.h"

class User;

// Describes the specific loot available when an NPC dies
class Loot{
public:
    bool empty() const;
    void add(const ServerItem *item, size_t qty);
    void sendContentsToUser(const User &recipient, size_t serial) const;
    void sendSingleSlotToUser(const User &recipient, size_t serial, size_t slot) const;

private:
    ServerItem::vect_t _container;

};

#endif
