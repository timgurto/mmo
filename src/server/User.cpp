#include <cassert>

#include "ProgressLock.h"
#include "Server.h"
#include "User.h"

ObjectType User::OBJECT_TYPE("__clientObjectType__");

Stats User::BASE_STATS;

std::map<User::Class, std::string> User::CLASS_NAMES = {
    { SOLDIER, "Soldier" },
    { MAGUS, "Magus" },
    { PRIEST, "Priest" }
};
std::map<std::string, User::Class> User::CLASS_CODES;

Point User::newPlayerSpawn = {};
double User::spawnRadius = 0;

User::User(const std::string &name, const Point &loc, const Socket &socket) :
Object(&OBJECT_TYPE, loc),

_name(name),
_socket(socket),

_action(NO_ACTION),
_actionTime(0),
_actionObject(nullptr),
_actionRecipe(nullptr),
_actionObjectType(nullptr),
_actionSlot(INVENTORY_SIZE),
_actionLocation(0, 0),

_respawnPoint(newPlayerSpawn),

_driving(0),

_inventory(INVENTORY_SIZE),
_gear(GEAR_SLOTS),
_lastContact(SDL_GetTicks()),
_stats(BASE_STATS){
    if (!OBJECT_TYPE.collides()){
        OBJECT_TYPE.collisionRect(Rect(-5, -2, 10, 4));
    }
    health(BASE_STATS.health);
    energy(BASE_STATS.energy);
    for (size_t i = 0; i != INVENTORY_SIZE; ++i)
        _inventory[i] = std::make_pair<const ServerItem *, size_t>(0, 0);
}

User::User(const Socket &rhs):
    Object(Point()),
    _socket(rhs)
{}

User::User(const Point &loc):
    Object(loc),
    _socket(Socket::Empty())
{}

void User::init(){
    BASE_STATS.health = 100;
    BASE_STATS.energy = 100;
    BASE_STATS.eps = 1;
    BASE_STATS.attack = 8;
    BASE_STATS.attackTime = 1000;
    BASE_STATS.speed = 80.0;

    for (auto &pair : CLASS_NAMES)
        CLASS_CODES[pair.second] = pair.first;
}

bool User::compareXThenSerial::operator()( const User *a, const User *b) const{
    if (a->location().x != b->location().x)
        return a->location().x < b->location().x;
    return a->_socket < b->_socket; // Just need something unique.
}

bool User::compareYThenSerial::operator()( const User *a, const User *b) const{
    if (a->location().y != b->location().y)
        return a->location().y < b->location().y;
    return a->_socket < b->_socket; // Just need something unique.
}

std::string User::makeLocationCommand() const{
    return makeArgs(_name, location().x, location().y);
}

void User::contact(){
    _lastContact = SDL_GetTicks();
}

bool User::alive() const{
    return SDL_GetTicks() - _lastContact <= Server::CLIENT_TIMEOUT;
}

size_t User::giveItem(const ServerItem *item, size_t quantity){
    size_t remaining = quantity;

    // First pass: partial stacks
    for (size_t i = 0; i != INVENTORY_SIZE; ++i) {
        if (_inventory[i].first != item)
            continue;
        size_t spaceAvailable = item->stackSize() - _inventory[i].second;
        if (spaceAvailable > 0) {
            size_t qtyInThisSlot = min(spaceAvailable, remaining);
            _inventory[i].second += qtyInThisSlot;
            Server::instance().sendInventoryMessage(*this, i, Server::INVENTORY);
            remaining -= qtyInThisSlot;
        }
        if (remaining == 0)
            break;
    }

    // Second pass: empty slots
    if (remaining > 0){
        for (size_t i = 0; i != INVENTORY_SIZE; ++i) {
            if (_inventory[i].first != nullptr)
                continue;
            size_t qtyInThisSlot = min(item->stackSize(), remaining);
            _inventory[i].first = item;
            _inventory[i].second = qtyInThisSlot;
            Server::instance().sendInventoryMessage(*this, i, Server::INVENTORY);
            remaining -= qtyInThisSlot;
            if (remaining == 0)
                break;
        }
    }
    if (remaining < quantity)
        ProgressLock::triggerUnlocks(*this, ProgressLock::ITEM, item);
    return remaining;
}

void User::cancelAction() {
    if (_action == NO_ACTION)
        return;

    switch(_action){
    case GATHER:
        _actionObject->decrementGatheringUsers();
    }

    Server::instance().sendMessage(_socket, SV_ACTION_INTERRUPTED);
    _action = NO_ACTION;
}

void User::finishAction() {
    if (_action == NO_ACTION)
        return;

    _action = NO_ACTION;
}

void User::beginGathering(Object *obj){
    _action = GATHER;
    _actionObject = obj;
    _actionObject->incrementGatheringUsers();
    assert(obj->type());
    _actionTime = obj->objType().gatherTime();
}

void User::beginCrafting(const Recipe &recipe){
    _action = CRAFT;
    _actionRecipe = &recipe;
    _actionTime = recipe.time();
}

void User::beginConstructing(const ObjectType &obj, const Point &location, size_t slot){
    _action = CONSTRUCT;
    _actionObjectType = &obj;
    _actionTime = obj.constructionTime();
    _actionSlot = slot;
    _actionLocation = location;
}

void User::beginDeconstructing(Object &obj){
    _action = DECONSTRUCT;
    _actionObject = &obj;
    _actionTime = obj.deconstruction().timeToDeconstruct();
}

void User::setTargetAndAttack(Entity *target){
    this->target(target);
    if (target == nullptr){
        cancelAction();
        return;
    }
    _action = ATTACK;
}

bool User::hasItems(const ItemSet &items) const{
    ItemSet remaining = items;
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i){
        const std::pair<const ServerItem *, size_t> &invSlot = _inventory[i];
        remaining.remove(invSlot.first, invSlot.second);
        if (remaining.isEmpty())
            return true;
    }
    return false;
}

bool User::hasTool(const std::string &tagName) const{

    // Check gear
    for (size_t i = 0; i != User::GEAR_SLOTS; ++i) {
        const ServerItem *item = _gear[i].first;
        if (item && item->isTag(tagName))
            return true;
    }

    // Check inventory
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i) {
        const ServerItem *item = _inventory[i].first;
        if (item && item->isTag(tagName))
            return true;
    }

    // Check nearby terrain
    Server &server = *Server::_instance;
    auto nearbyTerrain = server.nearbyTerrainTypes(collisionRect(), Server::ACTION_DISTANCE);
    for (char terrainType : nearbyTerrain){
        if (server.terrainType(terrainType)->tag() == tagName)
            return true;
    }

    // Check nearby objects
    // Note that checking collision chunks means ignoring non-colliding objects.
    auto superChunk = Server::_instance->getCollisionSuperChunk(location());
    for (CollisionChunk *chunk : superChunk)
        for (const auto &pair : chunk->entities()) {
            const Entity *pEnt = pair.second;
            const Object *pObj = dynamic_cast<const Object *>(pEnt);
            if (pObj == nullptr)
                continue;
            if (!pObj->isBeingBuilt() &&
                pObj->type()->isTag(tagName) &&
                distance(pObj->collisionRect(), collisionRect()) < Server::ACTION_DISTANCE)
                return true;
        }

    return false;
}

bool User::hasTools(const std::set<std::string> &classes) const{
    for (const std::string &tagName : classes)
        if (!hasTool(tagName))
            return false;
    return true;
}

void User::removeItems(const ItemSet &items) {
    std::set<size_t> invSlotsChanged;
    ItemSet remaining = items;
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i){
        std::pair<const ServerItem *, size_t> &invSlot = _inventory[i];
        if (remaining.contains(invSlot.first)) {
            size_t itemsToRemove = min(invSlot.second, remaining[invSlot.first]);
            remaining.remove(invSlot.first, itemsToRemove);
            _inventory[i].second -= itemsToRemove;
            if (_inventory[i].second == 0)
                _inventory[i].first = nullptr;
            invSlotsChanged.insert(i);
            if (remaining.isEmpty())
                break;
        }
    }
    for (size_t slotNum : invSlotsChanged) {
        const std::pair<const ServerItem *, size_t> &slot = _inventory[slotNum];
        std::string id = slot.first ? slot.first->id() : "none";
        Server::instance().sendInventoryMessage(*this, slotNum, Server::INVENTORY);
    }
}

void User::update(ms_t timeElapsed){
    if (_action == NO_ACTION)
        return;

    if (_actionTime > timeElapsed)
        _actionTime -= timeElapsed;
    else
        _actionTime = 0;

    // Regen
    _timeSinceRegen += timeElapsed;
    while (_timeSinceRegen > 1000) {
        regen();
        _timeSinceRegen -= 1000;
    }

    // Attack actions:
    if (_action == ATTACK){
        Entity::update(timeElapsed);
        return;
    }

    // Non-attack actions:
    Server &server = *Server::_instance;

    if (_actionTime > 0) // Action hasn't finished yet.
        return;
    
    // Timer has finished; complete action
    switch(_action){
    case GATHER:
        if (!_actionObject->contents().isEmpty())
            server.gatherObject(_actionObject->serial(), *this);
        break;

    case CRAFT:
    {
        if (! hasRoomToCraft(*_actionRecipe)) {
            server.sendMessage(_socket, SV_INVENTORY_FULL);
            cancelAction();
            return;
        }
        // Remove materials from user's inventory
        removeItems(_actionRecipe->materials());
        // Give user his newly crafted items
        const ServerItem *product = toServerItem(_actionRecipe->product());
        giveItem(product, _actionRecipe->quantity());
        // Trigger any new unlocks
        ProgressLock::triggerUnlocks(*this, ProgressLock::RECIPE, _actionRecipe);
        break;
    }

    case CONSTRUCT:
    {
        // Create object
        server.addObject(_actionObjectType, _actionLocation, _name);
        if (_actionSlot == INVENTORY_SIZE) // Constructing an object without an item
            break;
        // Remove item from user's inventory
        std::pair<const ServerItem *, size_t> &slot = _inventory[_actionSlot];
        assert(slot.first->constructsObject() == _actionObjectType);
        --slot.second;
        if (slot.second == 0)
            slot.first = nullptr;
        server.sendInventoryMessage(*this, _actionSlot, Server::INVENTORY);
        // Trigger any new unlocks
        ProgressLock::triggerUnlocks(*this, ProgressLock::CONSTRUCTION, _actionObjectType);
        break;
    }

    case DECONSTRUCT:
    {
        //Check for inventory space
        const ServerItem *item = _actionObject->deconstruction().becomes();
        if (!vectHasSpace(_inventory, item)){
            server.sendMessage(_socket, SV_INVENTORY_FULL);
            cancelAction();
            return;
        }
        // Give user his item
        giveItem(item);
        // Remove object
        server.removeEntity(*_actionObject);
        break;
    }

    default:
        assert(false);
    }
    
    if (_action != ATTACK){ // ATTACK is a repeating action.
        server.sendMessage(_socket, SV_ACTION_FINISHED);
        finishAction();
    }
}

void User::regen() {
    auto newEnergy = min(energy() + _stats.eps, _stats.energy);
    if (newEnergy > energy()) {
        energy(newEnergy);
        onEnergyChange();
    }
}

bool User::hasRoomToCraft(const Recipe &recipe) const{
    size_t slotsFreedByMaterials = 0;
    ItemSet remainingMaterials = recipe.materials();
    ServerItem::vect_t inventoryCopy = _inventory;
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i){
        std::pair<const ServerItem *, size_t> &invSlot = inventoryCopy[i];
        if (remainingMaterials.contains(invSlot.first)) {
            size_t itemsToRemove = min(invSlot.second, remainingMaterials[invSlot.first]);
            remainingMaterials.remove(invSlot.first, itemsToRemove);
            inventoryCopy[i].second -= itemsToRemove;
            if (inventoryCopy[i].second == 0)
                inventoryCopy[i].first = nullptr;
            if (remainingMaterials.isEmpty())
                break;
        }
    }
    return vectHasSpace(inventoryCopy, toServerItem(recipe.product()), recipe.quantity());
}

const Rect User::collisionRect() const{
    return OBJECT_TYPE.collisionRect() + location();
}

bool User::canBeAttackedBy(const User & user) const {
    const Server &server = *Server::_instance;
    return server._wars.isAtWar({ _name }, { user._name });
}

void User::onHealthChange(){
    const Server &server = *Server::_instance;
    for (const User *userToInform: server.findUsersInArea(location()))
        server.sendMessage(userToInform->socket(), SV_PLAYER_HEALTH, makeArgs(_name, health()));
}

void User::onEnergyChange() {
    const Server &server = *Server::_instance;
    for (const User *userToInform : server.findUsersInArea(location()))
        server.sendMessage(userToInform->socket(), SV_PLAYER_ENERGY, makeArgs(_name, energy()));
}

void User::onDeath(){
    // Handle respawn etc.
    moveToSpawnPoint();

    health(maxHealth());
    onHealthChange();
}

void User::onNewOwnedObject(const ObjectType & type) const {
    if (type.isPlayerUnique())
        this->_playerUniqueCategoriesOwned.insert(type.playerUniqueCategory());
}

void User::updateStats(){
    const Server &server = *Server::_instance;

    Hitpoints oldMaxHealth = maxHealth();

    _stats = BASE_STATS;
    for (size_t i = 0; i != GEAR_SLOTS; ++i){
        const ServerItem *item = _gear[i].first;
        if (item != nullptr)
            _stats &= item->stats();
    }

    // Special case: health must change to reflect new max health
    int healthDecrease = oldMaxHealth - maxHealth();
    if (healthDecrease != 0) {
        // Alert nearby users to new max health
        server.broadcastToArea(location(), SV_MAX_HEALTH, makeArgs(_name, maxHealth()));
    }
    if (healthDecrease > 0 && healthDecrease > static_cast<int>(health())) {
        health(1); // Implicit rule: changing gear can never kill you, only reduce you to 1 health.
        onHealthChange();
    } else {
        reduceHealth(healthDecrease);
    }


    server.sendMessage(socket(), SV_YOUR_STATS, makeArgs(
        _stats.health,
        _stats.energy,
        _stats.eps,
        _stats.attack,
        _stats.attackTime,
        _stats.speed
    ));
}

bool User::knowsConstruction(const std::string &id) const {
    const Server &server = *Server::_instance;
    const ObjectType *objectType = server.findObjectTypeByName(id);
    bool objectTypeExists = (objectType != nullptr);
    if (!objectTypeExists)
        return false;
    if (objectType->isKnownByDefault())
        return true;
    bool userKnowsConstruction = _knownConstructions.find(id) != _knownConstructions.end();
    return userKnowsConstruction;
}

bool User::knowsRecipe(const std::string &id) const {
    const Server &server = *Server::_instance;
    auto it = server._recipes.find(id);
    bool recipeExists = (it != server._recipes.end());
    if (!recipeExists)
        return false;
    if (it->isKnownByDefault())
        return true;
    bool userKnowsRecipe = _knownRecipes.find(id) != _knownRecipes.end();
    return userKnowsRecipe;
}

void User::sendInfoToClient(const User &targetUser) const {
    const Server &server = Server::instance();
    const Socket &client = targetUser.socket();

    // Location
    server.sendMessage(client, SV_LOCATION, makeLocationCommand());

    // Hitpoints
    server.sendMessage(client, SV_MAX_HEALTH, makeArgs(_name, maxHealth()));
    server.sendMessage(client, SV_PLAYER_HEALTH, makeArgs(_name, health()));

    // Energy
    server.sendMessage(client, SV_MAX_ENERGY, makeArgs(_name, maxEnergy()));
    server.sendMessage(client, SV_PLAYER_ENERGY, makeArgs(_name, energy()));

    // Class
    server.sendMessage(client, SV_CLASS, makeArgs(_name, className()));

    // City
    const City::Name city = server._cities.getPlayerCity(_name);
    if (! city.empty())
        server.sendMessage(client, SV_IN_CITY, makeArgs(_name, city));

    // King?
    if (server._kings.isPlayerAKing(_name))
        server.sendMessage(client, SV_KING, _name);

    // Gear
    for (size_t i = 0; i != User::GEAR_SLOTS; ++i){
        const ServerItem *item = gear(i).first;
        if (item != nullptr)
            server.sendMessage(client, SV_GEAR, makeArgs(_name, i, item->id()));
    }
}

void User::onOutOfRange(const Entity &rhs) const{
    if (rhs.shouldAlwaysBeKnownToUser(*this))
        return;

    const Server &server = *Server::_instance;
    auto message = rhs.outOfRangeMessage();
    server.sendMessage(socket(), message.code, message.args);
}

Message User::outOfRangeMessage() const{
    return Message(SV_USER_OUT_OF_RANGE, name());
}

void User::moveToSpawnPoint(bool isNewPlayer) {
    Server &server = Server::instance();

    Point newLoc;
    size_t attempts = 0;
    static const size_t MAX_ATTEMPTS = 1000;
    do {
        if (attempts > MAX_ATTEMPTS) {
            server._debug("Failed to find valid spawn location for user", Color::FAILURE);
            return;
        }
        server._debug << "Attempt #" << ++attempts << " at placing new user" << Log::endl;
        newLoc.x = (randDouble() * 2 - 1) * spawnRadius + _respawnPoint.x;
        newLoc.y = (randDouble() * 2 - 1) * spawnRadius + _respawnPoint.y;
    } while (!server.isLocationValid(newLoc, User::OBJECT_TYPE));
    auto oldLoc = location();
    location(newLoc);

    if (isNewPlayer)
        return;

    server.broadcastToArea(oldLoc, SV_LOCATION_INSTANT, makeArgs(name(), location().x, location().y));
    server.broadcastToArea(location(), SV_LOCATION_INSTANT, makeArgs(name(), location().x, location().y));
}

void User::broadcastHealth() const {
    Server::_instance->broadcastToArea(location(), SV_PLAYER_HEALTH,
        makeArgs(_name, health()));
}
