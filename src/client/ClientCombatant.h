#ifndef CLIENT_COMBATANT_H
#define CLIENT_COMBATANT_H

#include "ClientCombatantType.h"
#include "Sprite.h"
#include "../types.h"

class ClientCombatant{
public:
    ClientCombatant::ClientCombatant(const ClientCombatantType *type);

    const health_t &health() const { return _health; }
    void health(health_t n) { _health = n; }
    bool isAlive() const { return _health > 0; }
    bool isDead() const { return _health == 0; }
    const health_t &maxHealth() const { return _type->maxHealth(); }
    void drawHealthBarIfAppropriate(const Point &objectLocation, px_t objHeight) const;
    bool shouldDrawHealthBar() const;
    const Color &nameColor() const;

    virtual void sendTargetMessage() const = 0;
    virtual bool canBeAttackedByPlayer() const { return isAlive(); }
    virtual const Sprite *entityPointer() const = 0;
    virtual bool belongsToPlayerCity() const { return false; }

private:
    health_t _health;
    const ClientCombatantType *_type;
};

#endif
