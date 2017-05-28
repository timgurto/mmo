#ifndef CLIENT_NPC_H
#define CLIENT_NPC_H

#include "ClientNPCType.h"
#include "ClientObject.h"

class TakeContainer;

class ClientNPC : public ClientObject {

    /*
    True if the NPC is dead and has loot available.  This is used as the loot itself is unknown
    until a user opens the container.
    */
    bool _lootable;
    TakeContainer *_lootContainer;

public:
    static const size_t LOOT_CAPACITY;

    ClientNPC(size_t serial, const ClientNPCType *type = nullptr, const Point &loc = Point());
    virtual ~ClientNPC(){}
    bool lootable() const { return _lootable; }
    void lootable(bool b) { _lootable = b; }
    virtual bool isFlat() const override;

    const ClientNPCType *npcType() const { return dynamic_cast<const ClientNPCType *>(type()); }

    virtual char classTag() const override { return 'n'; }

    // From ClientCombatant:
    virtual bool canBeAttackedByPlayer() const override;
    
    // From Sprite
    virtual void onRightClick(Client &client) override;
    virtual void assembleWindow(Client &client) override;
    virtual void draw(const Client &client) const;
    virtual void update(double delta) override;
    virtual const Texture &cursor(const Client &client) const override;
    virtual void onInventoryUpdate() override;
};

#endif
