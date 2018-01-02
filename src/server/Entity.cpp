#include <algorithm>
#include <cassert>

#include "Entity.h"
#include "Server.h"
#include "Spawner.h"
#include "../util.h"

const px_t Entity::DEFAULT_ATTACK_RANGE = Podes{ 4 }.toPixels();

Stats Dummy::_stats{};

Entity::Entity(const EntityType *type, const MapPoint &loc):
    _type(type),
    _serial(generateSerial()),
    _spawner(nullptr),

    _location(loc),
    _lastLocUpdate(SDL_GetTicks()),

    _stats(type->baseStats()),
    _health(_stats.maxHealth),
    _energy(_stats.maxEnergy),

    _attackTimer(0),
    _target(nullptr),
    _loot(nullptr)
{}

Entity::Entity(size_t serial): // For set/map lookup ONLY
    _type(nullptr),
    _serial(serial),
    _spawner(nullptr),
    _loot(nullptr)
{}

Entity::Entity(const MapPoint &loc): // For set/map lookup ONLY
    _type(nullptr),
    _location(loc),
    _serial(0),
    _spawner(nullptr),
    _loot(nullptr)
{}

Entity::~Entity(){
    if (_spawner != nullptr)
        _spawner->scheduleSpawn();
}

bool Entity::compareSerial::operator()( const Entity *a, const Entity *b) const{
    return a->_serial < b->_serial;
}

bool Entity::compareXThenSerial::operator()( const Entity *a, const Entity *b) const{
    if (a->_location.x != b->_location.x)
        return a->_location.x < b->_location.x;
    return a->_serial < b->_serial;
}

bool Entity::compareYThenSerial::operator()( const Entity *a, const Entity *b) const{
    if (a->_location.y != b->_location.y)
        return a->_location.y < b->_location.y;
    return a->_serial < b->_serial;
}

size_t Entity::generateSerial() {
    static size_t currentSerial = Server::STARTING_SERIAL;
    return currentSerial++;
}

void Entity::markForRemoval(){
    Server::_instance->_entitiesToRemove.push_back(this);
}

bool Entity::combatTypeCanHaveOutcome(CombatType type, CombatResult outcome, SpellSchool school,
        px_t range) {
    /*
                Miss    Dodge   Block   Crit    Hit
    Spell               X       X
    Physical
    Ranged              X
    Heal        X       X       X
    Debuff              X       X       X
    */
    if (type == HEAL && outcome == MISS)
        return false;
    if (type == DEBUFF && outcome != CRIT && outcome != HIT)
        return false;
    if (outcome == DODGE && range > Podes::MELEE_RANGE)
        return false;
    if (outcome == BLOCK && school.isMagic())
        return false;
    if (type == THREAT_MOD && (outcome == BLOCK || outcome == DODGE))
        return false;

    return true;
}

void Entity::sendGotHitMessageTo(const User & user) const {
    Server::_instance->sendMessage(user.socket(), SV_ENTITY_WAS_HIT, makeArgs(serial()));
}

void Entity::reduceHealth(int damage) {
    if (damage == 0)
        return;
    if (damage >= static_cast<int>(_health)) {
        _health = 0;
        onHealthChange();
        onDeath();
    } else {
        _health -= damage;
        onHealthChange();
    }

    assert(_health <= this->_stats.maxHealth);
}

void Entity::reduceEnergy(int amount) {
    if (amount == 0)
        return;
    if (amount > static_cast<int>(_energy))
        amount = _energy;
    _energy -= amount;
    onEnergyChange();
}

void Entity::healBy(Hitpoints amount) {
    auto newHealth = min(health() + amount, _stats.maxHealth);
    _health = newHealth;
    onHealthChange();
}

void Entity::update(ms_t timeElapsed){
    // Corpse timer
    if (isDead()){
        if (_corpseTime > timeElapsed)
            _corpseTime -= timeElapsed;
        else
            markForRemoval();
        return;
    }

    regen(timeElapsed);

    updateBuffs(timeElapsed);

    // Combat
    auto pTarget = target();
    if (!pTarget)
        return;

    if (_attackTimer > timeElapsed)
        _attackTimer -= timeElapsed;
    else
        _attackTimer = 0;

    assert(pTarget->health() > 0);

    if (_attackTimer > 0)
        return;

    // Reset timer
    _attackTimer = _stats.attackTime;

    // Check if within range
    if (distance(collisionRect(), pTarget->collisionRect()) <= attackRange()){
        const Server &server = Server::instance();
        MapPoint locus = midpoint(location(), pTarget->location());

        auto outcome = generateHitAgainst(*pTarget, DAMAGE, SpellSchool::PHYSICAL, attackRange());

        switch (outcome) {
        // These cases return
        case MISS:
            for (const User *userToInform : server.findUsersInArea( locus ))
                server.sendMessage( userToInform->socket(), SV_SHOW_MISS_AT, makeArgs(
                    pTarget->location().x, pTarget->location().y ) );
            return;
        case DODGE:
            for (const User *userToInform : server.findUsersInArea(locus))
                server.sendMessage(userToInform->socket(), SV_SHOW_DODGE_AT, makeArgs(
                    pTarget->location().x, pTarget->location().y));
            return;

        // These cases continue on
        case CRIT:
            for (const User *userToInform : server.findUsersInArea(locus))
                server.sendMessage(userToInform->socket(), SV_SHOW_CRIT_AT, makeArgs(
                    pTarget->location().x, pTarget->location().y));
            break;
        case BLOCK:
            for (const User *userToInform : server.findUsersInArea(locus))
                server.sendMessage(userToInform->socket(), SV_SHOW_BLOCK_AT, makeArgs(
                    pTarget->location().x, pTarget->location().y));
            break;
        }

        auto rawDamage = static_cast<double>(_stats.attack);
        if (outcome == CRIT)
            rawDamage *= 2;

        auto resistance = pTarget->_stats.resistanceByType(SpellSchool::PHYSICAL);
        auto resistanceMultiplier = (100 - resistance) / 100.0;
        rawDamage *= resistanceMultiplier;

        auto damage = SpellEffect::chooseRandomSpellMagnitude(rawDamage);

        if (outcome == BLOCK) {
            if (_stats.blockValue >= damage)
                damage = 0;
            else
                damage -= _stats.blockValue;
        }

        pTarget->reduceHealth(damage);

        // Give target opportunity to react
        pTarget->onAttackedBy(*this, damage);

        // Alert nearby clients
        MessageCode msgCode;
        std::string args;
        char
            attackerTag = classTag(),
            defenderTag = pTarget->classTag();
        if (attackerTag == 'u' && defenderTag != 'u'){
            msgCode = SV_PLAYER_HIT_ENTITY;
            args = makeArgs(
                    dynamic_cast<const User *>(this)->name(),
                    pTarget->serial());
        } else if (attackerTag != 'u' && defenderTag == 'u'){
            msgCode = SV_ENTITY_HIT_PLAYER;
            args = makeArgs(
                    serial(),
                    dynamic_cast<const User *>(pTarget)->name());
        } else if (attackerTag == 'u' && defenderTag == 'u') {
            msgCode = SV_PLAYER_HIT_PLAYER;
            args = makeArgs(
                    dynamic_cast<const User *>(this)->name(),
                    dynamic_cast<const User *>(pTarget)->name());
        } else {
            assert(false);
        }
        for (const User *userToInform : server.findUsersInArea(locus)){
            server.sendMessage(userToInform->socket(), msgCode, args);
        }
    }
}

void Entity::updateBuffs(ms_t timeElapsed) {
    for (auto i = 0; i != _buffs.size(); ) {
        _buffs[i].update(timeElapsed);
        if (_buffs[i].hasExpired())
            _buffs.erase(_buffs.begin() + i);
        else
            ++i;
    }
    for (auto i = 0; i != _debuffs.size(); ) {
        _debuffs[i].update(timeElapsed);
        if (_debuffs[i].hasExpired())
            _debuffs.erase(_debuffs.begin() + i);
        else
            ++i;
    }
}

void Entity::onDeath(){
    if (timeToRemainAsCorpse() == 0)
        markForRemoval();
    else
        startCorpseTimer();
}

void Entity::onAttackedBy(Entity &attacker, Hitpoints damage) {
    if (isDead())
        attacker.onKilled(*this);
}

void Entity::startCorpseTimer(){
    _corpseTime = timeToRemainAsCorpse();
}

void Entity::addWatcher(const std::string &username){
    _watchers.insert(username);
    Server::debug() << username << " is now watching an object." << Log::endl;
}

void Entity::removeWatcher(const std::string &username){
    _watchers.erase(username);
    Server::debug() << username << " is no longer watching an object." << Log::endl;
}

void Entity::applyBuff(const BuffType & type, Entity &caster) {
    auto newBuff = Buff{ type, *this, caster };

    // Check for duplicates
    for (auto &buff : _buffs)
        if (buff == newBuff)
            return;

    _buffs.push_back(newBuff);
    updateStats();

    sendBuffMsg(type.id());
}

void Entity::applyDebuff(const BuffType & type, Entity &caster) {
    auto newDebuff = Buff{ type, *this, caster };

    // Check for duplicates
    for (auto &debuff : _debuffs)
        if (debuff == newDebuff)
            return;

    _debuffs.push_back(newDebuff);
    updateStats();

    sendDebuffMsg(type.id());
}

void Entity::sendBuffMsg(const Buff::ID &buff) const {
    const Server &server = Server::instance();
    server.broadcastToArea(_location, SV_ENTITY_GOT_BUFF, makeArgs(_serial, buff));
}

void Entity::sendDebuffMsg(const Buff::ID &buff) const {
    const Server &server = Server::instance();
    server.broadcastToArea(_location, SV_ENTITY_GOT_DEBUFF, makeArgs(_serial, buff));
}

void Entity::regen(ms_t timeElapsed) {
    // Regen
    _timeSinceRegen += timeElapsed;
    if (_timeSinceRegen < 1000)
        return;

    _timeSinceRegen -= 1000;

    if (stats().hps != 0) {
        int rawNewHealth = health() + stats().hps;
        if (rawNewHealth < 0)
            health(0);
        else if (0 + rawNewHealth > static_cast<int>(stats().maxHealth) + 0)
            health(stats().maxHealth);
        else
            health(rawNewHealth);
        onHealthChange();
        if (isDead())
            onDeath();
    }

    if (stats().eps != 0) {
        int rawNewEnergy = energy() + stats().eps;
        if (rawNewEnergy < 0)
            energy(0);
        else if (rawNewEnergy > static_cast<int>(stats().maxEnergy))
            energy(stats().maxEnergy);
        else
            energy(rawNewEnergy);
        onEnergyChange();
    }
}
