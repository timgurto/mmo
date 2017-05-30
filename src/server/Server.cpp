#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#ifndef SINGLE_THREAD
#include <mutex>
#include <thread>
#endif

#include "ItemSet.h"
#include "NPC.h"
#include "NPCType.h"
#include "ProgressLock.h"
#include "Recipe.h"
#include "Server.h"
#include "User.h"
#include "Vehicle.h"
#include "VehicleType.h"
#include "objects/Object.h"
#include "objects/ObjectType.h"
#include "../Socket.h"
#include "../messageCodes.h"
#include "../util.h"

extern Args cmdLineArgs;

Server *Server::_instance = nullptr;
LogConsole *Server::_debugInstance = nullptr;

const int Server::MAX_CLIENTS = 20;

const ms_t Server::CLIENT_TIMEOUT = 10000;
const ms_t Server::MAX_TIME_BETWEEN_LOCATION_UPDATES = 300;

const ms_t Server::SAVE_FREQUENCY = 30000;

const px_t Server::ACTION_DISTANCE = 30;
const px_t Server::CULL_DISTANCE = 450;
const px_t Server::TILE_W = 32;
const px_t Server::TILE_H = 32;

Server::Server():
_time(SDL_GetTicks()),
_lastTime(_time),
_socket(),
_loop(false),
_running(false),
_mapX(0),
_mapY(0),
_newPlayerSpawnLocation(0, 0),
_newPlayerSpawnRange(0),
_debug("server.log"),
_userFilesPath("Users/"),
_lastSave(_time),
_dataLoaded(false){
    _instance = this;
    _debugInstance = &_debug;
    if (cmdLineArgs.contains("quiet"))
        _debug.quiet();

    _debug << cmdLineArgs << Log::endl;
    if (Socket::debug == nullptr)
        Socket::debug = &_debug;

    User::init();

    if (cmdLineArgs.contains("user-files-path"))
        _userFilesPath = cmdLineArgs.getString("user-files-path") + "/";


    _debug("Server initialized");

    // Socket details
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888);

    _socket.bind(serverAddr);
    _debug << "Server address: " << inet_ntoa(serverAddr.sin_addr)
           << ":" << ntohs(serverAddr.sin_port) << Log::endl;
    _socket.listen();

    _debug("Ready for connections");
}

Server::~Server(){
    saveData(_entities, _wars, _cities);
    for (auto pair : _terrainTypes)
        delete pair.second;
    _instance = nullptr;
}

void Server::checkSockets(){
    // Populate socket list with active sockets
    static fd_set readFDs;
    FD_ZERO(&readFDs);
    FD_SET(_socket.getRaw(), &readFDs);
    for (const Socket &socket : _clientSockets) {
        FD_SET(socket.getRaw(), &readFDs);
    }

    // Poll for activity
    static const timeval selectTimeout = {0, 10000};
    int activity = select(0, &readFDs, nullptr, nullptr, &selectTimeout);
    if (activity == SOCKET_ERROR) {
        _debug << Color::RED << "Error polling sockets: " << WSAGetLastError() << Log::endl;
        return;
    }
    _time = SDL_GetTicks();

    // Activity on server socket: new connection
    if (FD_ISSET(_socket.getRaw(), &readFDs)) {

        if (_clientSockets.size() == MAX_CLIENTS) {
            _debug("No room for additional clients; all slots full");
            sockaddr_in clientAddr;
            SOCKET tempSocket = accept(_socket.getRaw(), (sockaddr*)&clientAddr,
                                       (int*)&Socket::sockAddrSize);
            Socket s(tempSocket);
            // Allow time for rejection message to be sent before closing socket
            s.delayClosing(5000);
            sendMessage(s, SV_SERVER_FULL);
        } else {
            sockaddr_in clientAddr;
            SOCKET tempSocket = accept(_socket.getRaw(), (sockaddr*)&clientAddr,
                                       (int*)&Socket::sockAddrSize);
            if (tempSocket == SOCKET_ERROR) {
                _debug << Color::RED << "Error accepting connection: "
                       << WSAGetLastError() << Log::endl;
            } else {
                _debug << Color::GREEN << "Connection accepted: "
                       << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port)
                       << ", socket number = " << tempSocket << Log::endl;
                _clientSockets.insert(tempSocket);
            }
        }
    }

    // Activity on client socket: message received or client disconnected
    for (std::set<Socket>::iterator it = _clientSockets.begin(); it != _clientSockets.end();) {
        SOCKET raw = it->getRaw();
        if (FD_ISSET(raw, &readFDs)) {
            sockaddr_in clientAddr;
            getpeername(raw, (sockaddr*)&clientAddr, (int*)&Socket::sockAddrSize);
            static char buffer[BUFFER_SIZE+1];
            const int charsRead = recv(raw, buffer, BUFFER_SIZE, 0);
            if (charsRead == SOCKET_ERROR) {
                int err = WSAGetLastError();
                _debug << "Client " << raw << " disconnected; error code: " << err << Log::endl;
                removeUser(raw);
                closesocket(raw);
                _clientSockets.erase(it++);
                continue;
            } else if (charsRead == 0) {
                // Client disconnected
                _debug << "Client " << raw << " disconnected" << Log::endl;
                removeUser(*it);
                closesocket(raw);
                _clientSockets.erase(it++);
                continue;
            } else {
                // Message received
                buffer[charsRead] = '\0';
                _messages.push(std::make_pair(*it, std::string(buffer)));
            }
        }
        ++it;
    }
}

void Server::run(){
    if (!_dataLoaded)
        loadData();

    _loop = true;
    _running = true;
    _debug("Server is running", Color::GREEN);
    while (_loop) {
        _time = SDL_GetTicks();
        const ms_t timeElapsed = _time - _lastTime;
        _lastTime = _time;

        // Check that clients are alive
        for (std::set<User>::iterator it = _users.begin(); it != _users.end();) {
            if (!it->alive()) {
                _debug << Color::RED << "User " << it->name() << " has timed out." << Log::endl;
                std::set<User>::iterator next = it; ++next;

                auto socketIt = _clientSockets.find(it->socket());
                assert(socketIt != _clientSockets.end());
                _clientSockets.erase(socketIt);

                removeUser(it);
                it = next;
            } else {
                ++it;
            }
        }

        // Save data
        if (_time - _lastSave >= SAVE_FREQUENCY) {
            for (const User &user : _users) {
                writeUserData(user);
            }
#ifdef SINGLE_THREAD
            saveData(_entities, _wars, _cities);
#else
            std::thread(saveData, _entities, _wars, _cities).detach();
#endif
            _lastSave = _time;
        }

        // Update users
        for (const User &user : _users)
            const_cast<User&>(user).update(timeElapsed);

        // Update non-user entities
        for (Entity *entP : _entities)
            entP->update(timeElapsed);

        // Clean up dead objects
        for (Entity *entP : _entitiesToRemove){
            removeEntity(*entP);
        }
        _entitiesToRemove.clear();

        // Update spawners
        for (auto &pair : _spawners)
            pair.second.update(_time);

        // Deal with any messages from the server
        while (!_messages.empty()){
            handleMessage(_messages.front().first, _messages.front().second);
            _messages.pop();
        }

        checkSockets();

        SDL_Delay(1);
    }

    // Save all user data
    for(const User &user : _users){
        writeUserData(user);
    }
    _running = false;
}

void Server::addUser(const Socket &socket, const std::string &name){
    User newUser(name, 0, socket);
    const bool userExisted = readUserData(newUser);
    if (!userExisted) {
        newUser.setClass(User::Class(rand() % User::NUM_CLASSES));
        Point newLoc;
        size_t attempts = 0;
        static const size_t MAX_ATTEMPTS = 1000;
        do {
            if (attempts > MAX_ATTEMPTS){
                _debug("Failed to find valid spawn location for user", Color::FAILURE);
                return;
            }
            _debug << "Attempt #" << ++attempts << " at placing new user" << Log::endl;
            newLoc.x = (randDouble() * 2 - 1) * _newPlayerSpawnRange + _newPlayerSpawnLocation.x;
            newLoc.y = (randDouble() * 2 - 1) * _newPlayerSpawnRange + _newPlayerSpawnLocation.y;
        } while (!isLocationValid(newLoc, User::OBJECT_TYPE));
        newUser.location(newLoc);
        _debug << "New";
    } else {
        _debug << "Existing";
    }
    _debug << " user, " << name << " has logged in." << Log::endl;

    // Send welcome message
    sendMessage(socket, SV_WELCOME);

    // Tell him about himself
    newUser.sendInfoToClient(newUser);

    // Send him his wars
    auto belligerents = _wars.getAllWarsInvolving(name);
    for (auto it = belligerents.first; it != belligerents.second; ++it)
        sendMessage(newUser.socket(), SV_AT_WAR_WITH, it->second);

    for (const User *userP : findUsersInArea(newUser.location())){
        if (userP == &newUser)
            continue;

        // Send him information about other nearby users
        userP->sendInfoToClient(newUser);
        // Send nearby others this user's information
        newUser.sendInfoToClient(*userP);
    }

    // Send him entity details
    const Point &loc = newUser.location();
    auto loX = _entitiesByX.lower_bound(&Dummy::Location(loc.x - CULL_DISTANCE, 0));
    auto hiX = _entitiesByX.upper_bound(&Dummy::Location(loc.x + CULL_DISTANCE, 0));
    for (auto it = loX; it != hiX; ++it){
        const Entity &ent = **it;
        if (ent.type() == nullptr){
            _debug("Null-type object skipped", Color::RED);
            continue;
        }
        if (abs(ent.location().y - loc.y) > CULL_DISTANCE) // Cull y
            continue;
        ent.sendInfoToClient(newUser);
    }

    // Send him his inventory
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i) {
        if (newUser.inventory(i).first != nullptr)
            sendInventoryMessage(newUser, i, INVENTORY);
    }

    // Send him his gear
    for (size_t i = 0; i != User::GEAR_SLOTS; ++i) {
        if (newUser.gear(i).first != nullptr)
            sendInventoryMessage(newUser, i, GEAR);
    }

    // Send him the recipes he knows
    if (newUser.knownRecipes().size() > 0){
        std::string args = makeArgs(newUser.knownRecipes().size());
        for (const std::string &id : newUser.knownRecipes()){
            args = makeArgs(args, id);
        }
        sendMessage(newUser.socket(), SV_RECIPES, args);
    }

    // Send him the constructions he knows
    if (newUser.knownConstructions().size() > 0){
        std::string args = makeArgs(newUser.knownConstructions().size());
        for (const std::string &id : newUser.knownConstructions()){
            args = makeArgs(args, id);
        }
        sendMessage(newUser.socket(), SV_CONSTRUCTIONS, args);
    }

    // Calculate and send him his stats
    newUser.updateStats();

    // Add new user to list
    std::set<User>::const_iterator it = _users.insert(newUser).first;
    _usersByName[name] = &*it;

    // Add user to location-indexed trees
    const User *userP = &*it;
    _usersByX.insert(userP);
    _usersByY.insert(userP);
    _entitiesByX.insert(userP);
    _entitiesByY.insert(userP);
}

void Server::removeUser(const std::set<User>::iterator &it){
    // Alert nearby users
    for (const User *userP : findUsersInArea(it->location()))
        if (userP != &*it)
            sendMessage(userP->socket(), SV_USER_DISCONNECTED, it->name());

    forceAllToUntarget(*it);

    // Save user data
    writeUserData(*it);

    _usersByX.erase(&*it);
    _usersByY.erase(&*it);
    _entitiesByX.erase(&*it);
    _entitiesByY.erase(&*it);
    _usersByName.erase(it->name());

    _users.erase(it);
}

void Server::removeUser(const Socket &socket){
    const std::set<User>::iterator it = _users.find(socket);
    if (it != _users.end())
        removeUser(it);
    else
        _debug("User was already removed", Color::RED);
}

std::list<User *> Server::findUsersInArea(Point loc, double squareRadius) const{
    std::list<User *> users;
    auto loX = _usersByX.lower_bound(&User(Point(loc.x - squareRadius, 0)));
    auto hiX = _usersByX.upper_bound(&User(Point(loc.x + squareRadius, 0)));
    for (auto it = loX; it != hiX; ++it)
        if (abs(loc.y - (*it)->location().y) <= squareRadius)
            users.push_back(const_cast<User *>(*it));

    return users;
}

bool Server::isEntityInRange(const Socket &client, const User &user, const Entity *ent) const{
    // Doesn't exist
    if (ent == nullptr) {
        sendMessage(client, SV_DOESNT_EXIST);
        return false;
    }

    // Check distance from user
    if (distance(user.collisionRect(), ent->collisionRect()) > ACTION_DISTANCE) {
        sendMessage(client, SV_TOO_FAR);
        return false;
    }

    return true;
}

void Server::forceAllToUntarget(const Entity &target, const User *userToExclude){
    // Fix users targeting the entity
    size_t serial = target.serial();
    for (const User &constUser : _users) {
        User & user = const_cast<User &>(constUser);
        if (&user == userToExclude)
            continue;
        if (user.action() == User::ATTACK && user.target() == &target) {
            user.finishAction();
            user.target(nullptr);
            continue;
        }
        if (user.action() == User::GATHER && user.actionObject()->serial() == serial) {
            sendMessage(user.socket(), SV_DOESNT_EXIST);
            user.target(nullptr);
            user.cancelAction();
        }
    }

    // Fix NPCs targeting the entity
    for (const Entity *pEnt : _entities) {
        Entity &entity = * const_cast<Entity *>(pEnt);
        if (entity.classTag() == 'n'){
            NPC &npc = dynamic_cast<NPC &>(entity);
            if (npc.target() == &target)
                npc.target(nullptr);
        }
    }
}

void Server::removeEntity(Entity &ent, const User *userToExclude){
    // Ensure no other users are targeting this object, as it will be removed.
    forceAllToUntarget(ent, userToExclude);

    // Alert nearby users of the removal
    size_t serial = ent.serial();
    for (const User *userP : findUsersInArea(ent.location()))
        sendMessage(userP->socket(), SV_REMOVE_OBJECT, makeArgs(serial));


    getCollisionChunk(ent.location()).removeEntity(serial);
    _entitiesByX.erase(&ent);
    _entitiesByY.erase(&ent);
    _entities.erase(&ent);

}

void Server::gatherObject(size_t serial, User &user){
    // Give item to user
    Object *obj = _entities.find<Object>(serial);
    const ServerItem *const toGive = obj->chooseGatherItem();
    size_t qtyToGive = obj->chooseGatherQuantity(toGive);
    const size_t remaining = user.giveItem(toGive, qtyToGive);
    if (remaining > 0) {
        sendMessage(user.socket(), SV_INVENTORY_FULL);
        qtyToGive -= remaining;
    }
    if (remaining < qtyToGive) // User received something: trigger any new unlocks
        ProgressLock::triggerUnlocks(user, ProgressLock::GATHER, toGive);
    // Remove object if empty
    obj->removeItem(toGive, qtyToGive);
    if (obj->contents().isEmpty()){
        if (obj->objType().transformsOnEmpty()){
            forceAllToUntarget(*obj);
            obj->removeAllGatheringUsers();
        } else
            removeEntity(*obj, &user);
    } else
        obj->decrementGatheringUsers();

#ifdef SINGLE_THREAD
    saveData(_objects, _wars, _cities);
#else
    std::thread(saveData, _entities, _wars, _cities).detach();
#endif
}

void Server::spawnInitialObjects(){
    // From spawners
    for (auto &pair: _spawners){
        Spawner &spawner = pair.second;
        assert(spawner.type() != nullptr);
        for (size_t i = 0; i != spawner.quantity(); ++i)
            spawner.spawn();
    }
}

Point Server::mapRand() const{
    return Point(randDouble() * (_mapX - 0.5) * TILE_W,
                 randDouble() * _mapY * TILE_H);
}

bool Server::itemIsTag(const ServerItem *item, const std::string &tagName) const{
    assert (item);
    return item->isTag(tagName);
}

const ObjectType *Server::findObjectTypeByName(const std::string &id) const{
    for (const ObjectType *type : _objectTypes)
        if (type->id() == id)
            return type;
    return nullptr;
}

Object &Server::addObject(const ObjectType *type, const Point &location, const std::string &owner){
    Object *newObj = type->classTag() == 'v' ?
            new Vehicle(dynamic_cast<const VehicleType *>(type), location) :
            new Object(type, location);
    if (! owner.empty())
        newObj->permissions().setPlayerOwner(owner);
    return dynamic_cast<Object &>(addEntity(newObj));
}

NPC &Server::addNPC(const NPCType *type, const Point &location){
    NPC *newNPC = new NPC(type, location);
    return dynamic_cast<NPC &>(addEntity(newNPC));
}

Entity &Server::addEntity(Entity *newEntity){
    _entities.insert(newEntity);
    const Point &loc = newEntity->location();

    // Alert nearby users
    for (const User *userP : findUsersInArea(loc))
        newEntity->sendInfoToClient(*userP);

    // Add object to relevant chunk
    if (newEntity->type()->collides())
        getCollisionChunk(loc).addEntity(newEntity);

    // Add object to x/y index sets
    _entitiesByX.insert(newEntity);
    _entitiesByY.insert(newEntity);

    return const_cast<Entity&>(*newEntity);
}

const User &Server::getUserByName(const std::string &username) const {
    return *_usersByName.find(username)->second;
}

const Terrain *Server::terrainType(char index) const{
    auto &types = const_cast<std::map<char, Terrain *> &> (_terrainTypes);
    return types[index];
}
