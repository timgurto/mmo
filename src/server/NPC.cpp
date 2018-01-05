#include <cassert>

#include "NPC.h"
#include "Server.h"

NPC::NPC(const NPCType *type, const MapPoint &loc):
    Entity(type, loc),
    _level(type->level()),
    _state(IDLE)
{
    _loot.reset(new Loot);
}

void NPC::update(ms_t timeElapsed){
    if (health() > 0){
        processAI(timeElapsed); // May call Entity::update()
    }

    Entity::update(timeElapsed);
}

CombatResult NPC::generateHitAgainst(const Entity &target, CombatType type, SpellSchool school, px_t range) const {
    const auto
        MISS_CHANCE = Percentage{ 5 };

    auto roll = rand() % 100;

    // Miss
    if (combatTypeCanHaveOutcome(type, MISS, school, range)) {
        if (roll < MISS_CHANCE)
            return MISS;
        roll -= MISS_CHANCE;
    }

    // Dodge
    auto dodgeChance = target.stats().dodge;
    if (combatTypeCanHaveOutcome(type, DODGE, school, range)) {
        if (roll < dodgeChance)
            return DODGE;
        roll -= dodgeChance;
    }

    // Block
    auto blockChance = target.stats().block;
    if (target.canBlock() && combatTypeCanHaveOutcome(type, BLOCK, school, range)) {
        if (roll < blockChance)
            return BLOCK;
        roll -= blockChance;
    }

    // Crit
    auto critChance = target.stats().critResist;
    if (critChance > 0 && combatTypeCanHaveOutcome(type, CRIT, school, range)) {
        if (roll < critChance)
            return CRIT;
        roll -= critChance;
    }

    return HIT;
}

void NPC::scaleThreatAgainst(Entity & target, double multiplier) {
    _threatTable.scaleThreat(target, multiplier);
}

void NPC::onHealthChange(){
    const Server &server = *Server::_instance;
    for (const User *user: server.findUsersInArea(location()))
        server.sendMessage(user->socket(), SV_ENTITY_HEALTH, makeArgs(serial(), health()));
}

void NPC::onDeath(){
    Server &server = *Server::_instance;
    server.forceAllToUntarget(*this);

    npcType()->lootTable().instantiate(*_loot);
    if (! _loot->empty())
        for (const User *user : server.findUsersInArea(location()))
            server.sendMessage(user->socket(), SV_LOOTABLE, makeArgs(serial()));

    /*
    Schedule a respawn, if this NPC came from a spawner.
    For non-NPCs, this happens in onRemove().  The object's _spawner is cleared afterwards to avoid
    onRemove() from doing so here.
    */
    if (spawner() != nullptr){
        spawner()->scheduleSpawn();
        spawner(nullptr);
    }

    Entity::onDeath();
}

void NPC::onAttackedBy(Entity &attacker, Hitpoints damage) {
    _threatTable.addThreat(attacker, damage);

    Entity::onAttackedBy(attacker, damage);
}

void NPC::processAI(ms_t timeElapsed){
    const auto
        VIEW_RANGE = 70_px,
        ATTACK_RANGE = 5_px,
        // Assumption: this is farther than any ranged attack/spell can reach.
        CONTINUE_ATTACKING_RANGE = Podes{ 35 }.toPixels();

    target(nullptr);

    // Become aware of nearby users
    for (User *user : Server::_instance->findUsersInArea(location(), VIEW_RANGE)){
        if (distance(collisionRect(), user->collisionRect()) <= VIEW_RANGE){
            _threatTable.makeAwareOf(*user);
        }
    }
    target(_threatTable.getTarget());

    auto distToTarget = target() ? distance(collisionRect(), target()->collisionRect()) : 0;

    // Transition if necessary
    switch(_state){
    case IDLE:
        if (stats().attack == 0) // NPCs that can't attack won't try.
            break;

        // React to recent attacker
        if (target()) {
            if (distToTarget > ATTACK_RANGE) {
                _state = CHASE;
                resetLocationUpdateTimer();
            } else
                _state = ATTACK;
        }

        break;

    case CHASE:
        // Target has disappeared
        if (!target()) {
            _state = IDLE;
            break;
        }

        // Target is dead
        if (target()->isDead()) {
            _state = IDLE;
            break;
        }

        // Target has run out of range: give up
        if (distToTarget > CONTINUE_ATTACKING_RANGE) {
            _state = IDLE;
            break;
        }
        
        // Target is close enough to attack
        if (distToTarget <= ATTACK_RANGE) {
            _state = ATTACK;
            break;
        }

        break;

    case ATTACK:
        // Target has disappeared
        if (target() == nullptr) {
            _state = IDLE;
            break;
        }

        // Target is dead
        if (target()->isDead()) {
            _state = IDLE;
            break;
        }

        // Target has run out of attack range: chase
        if (distToTarget > ATTACK_RANGE) {
            _state = CHASE;
            resetLocationUpdateTimer();
            break;
        }

        break;
    }

    // Act
    switch(_state){
    case IDLE:
        target(nullptr);
        break;

    case CHASE:
        // Move towards player
        updateLocation(target()->location());
        break;

    case ATTACK:
        break; // Entity::update() will handle it
    }
}

void NPC::forgetAbout(const Entity & entity) {
    _threatTable.forgetAbout(entity);
}

void NPC::sendInfoToClient(const User &targetUser) const {
    const Server &server = Server::instance();
    const Socket &client = targetUser.socket();

    server.sendMessage(client, SV_OBJECT, makeArgs(serial(), location().x, location().y,
                                                   type()->id()));

    // Level
    server.sendMessage(client, SV_NPC_LEVEL, makeArgs(serial(), _level));

    // Hitpoints
    if (health() < stats().maxHealth)
        server.sendMessage(client, SV_ENTITY_HEALTH, makeArgs(serial(), health()));

    // Loot
    if (!_loot->empty())
        server.sendMessage(client, SV_LOOTABLE, makeArgs(serial()));

    // Buffs/debuffs
    for (const auto &buff : buffs())
        server.sendMessage(client, SV_ENTITY_GOT_BUFF, makeArgs(serial(), buff.type()));
    for (const auto &debuff : debuffs())
        server.sendMessage(client, SV_ENTITY_GOT_DEBUFF, makeArgs(serial(), debuff.type()));
}

void NPC::describeSelfToNewWatcher(const User &watcher) const{
    _loot->sendContentsToUser(watcher, serial());
}

void NPC::alertWatcherOnInventoryChange(const User &watcher, size_t slot) const{
    _loot->sendSingleSlotToUser(watcher, serial(), slot);

    const Server &server = Server::instance();
    if (_loot->empty())
        server.sendMessage(watcher.socket(), SV_NOT_LOOTABLE, makeArgs(serial()));
}

ServerItem::Slot *NPC::getSlotToTakeFromAndSendErrors(size_t slotNum, const User &user){
    const Server &server = Server::instance();
    const Socket &socket = user.socket();

    if (_loot->empty()){
        server.sendMessage(socket, SV_EMPTY_SLOT);
        return nullptr;
    }

    if (!server.isEntityInRange(socket, user, this))
        return nullptr;

    if (! _loot->isValidSlot(slotNum)) {
        server.sendMessage(socket, SV_INVALID_SLOT);
        return nullptr;
    }

    ServerItem::Slot &slot = _loot->at(slotNum);
    if (slot.first == nullptr){
        server.sendMessage(socket, SV_EMPTY_SLOT);
        return nullptr;
    }

    return &slot;
}

void NPC::updateStats() {
    const Server &server = *Server::_instance;

    auto oldMaxHealth = stats().maxHealth;
    auto oldMaxEnergy = stats().maxEnergy;

    auto newStats = type()->baseStats();

    // Apply buffs
    for (auto &buff : buffs())
        buff.applyStatsTo(newStats);

    // Apply debuffs
    for (auto &debuff : debuffs())
        debuff.applyStatsTo(newStats);

    // Assumption: max health/energy won't change

    stats(newStats);
}
