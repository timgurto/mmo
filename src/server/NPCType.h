#ifndef NPC_TYPE_H
#define NPC_TYPE_H

#include "LootTable.h"
#include "objects/ObjectType.h"

// Describes a class of NPCs, a la the ObjectType class.
class NPCType : public ObjectType{
    Hitpoints _maxHealth;
    Hitpoints _attack;
    ms_t _attackTime;

    LootTable _lootTable;

public:
    NPCType(const std::string &id, Hitpoints maxHealth);
    virtual ~NPCType(){}
    
    void maxHealth(Hitpoints hp) { _maxHealth = hp; }
    Hitpoints maxHealth() const { return _maxHealth; }
    Hitpoints attack() const { return _attack; }
    void attack(Hitpoints damage) { _attack = damage; }
    ms_t attackTime() const { return _attackTime; }
    void attackTime(ms_t time) { _attackTime = time; }
    const LootTable &lootTable() const { return _lootTable; }

    virtual char classTag() const override { return 'n'; }

    void addSimpleLoot(const ServerItem *item, double chance);
    void addNormalLoot(const ServerItem *item, double mean, double sd);

};

#endif
