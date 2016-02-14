// (C) 2015 Tim Gurto

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
#include "Object.h"
#include "ObjectType.h"
#include "Recipe.h"
#include "Server.h"
#include "User.h"
#include "../Socket.h"
#include "../XmlReader.h"
#include "../XmlWriter.h"
#include "../messageCodes.h"
#include "../util.h"

extern Args cmdLineArgs;

Server *Server::_instance = 0;
LogConsole *Server::_debugInstance = 0;

const int Server::MAX_CLIENTS = 20;
const size_t Server::BUFFER_SIZE = 1023;

const Uint32 Server::CLIENT_TIMEOUT = 10000;
const Uint32 Server::MAX_TIME_BETWEEN_LOCATION_UPDATES = 300;

const Uint32 Server::SAVE_FREQUENCY = 1000;

const double Server::MOVEMENT_SPEED = 80;
const int Server::ACTION_DISTANCE = 30;

const int Server::TILE_W = 32;
const int Server::TILE_H = 32;

Server::Server():
_time(SDL_GetTicks()),
_lastTime(_time),
_socket(),
_loop(true),
_mapX(0),
_mapY(0),
_debug("server.log"),
_lastSave(_time){
    _instance = this;
    _debugInstance = &_debug;

    _debug << cmdLineArgs << Log::endl;
    Socket::debug = &_debug;

    loadData();

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

    _debug("Listening for connections");
}

Server::~Server(){
    saveData(_objects);
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
    int activity = select(0, &readFDs, 0, 0, &selectTimeout);
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
                if (err == WSAECONNRESET) {
                    // Client disconnected
                    _debug << "Client " << raw << " disconnected" << Log::endl;
                    removeUser(raw);
                    closesocket(raw);
                    _clientSockets.erase(it++);
                    continue;
                } else {
                    _debug << Color::RED << "Error receiving message: " << err << Log::endl;
                }
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
    while (_loop) {
        _time = SDL_GetTicks();
        const Uint32 timeElapsed = _time - _lastTime;
        _lastTime = _time;

        // Check that clients are alive
        for (std::set<User>::iterator it = _users.begin(); it != _users.end();) {
            if (!it->alive()) {
                _debug << Color::RED << "User " << it->name() << " has timed out." << Log::endl;
                std::set<User>::iterator next = it; ++next;
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
            saveData(_objects);
#else
            std::thread(saveData, _objects).detach();
#endif
            _lastSave = _time;
        }

        // Update users
        for (const User &user : _users)
            const_cast<User&>(user).update(timeElapsed);

        // Deal with any messages from the server
        while (!_messages.empty()){
            handleMessage(_messages.front().first, _messages.front().second);
            _messages.pop();
        }

        checkSockets();

        SDL_Delay(10);
    }

    // Save all user data
    for(const User &user : _users){
        writeUserData(user);
    }
}

void Server::addUser(const Socket &socket, const std::string &name){
    User newUser(name, 0, socket);
    const bool userExisted = readUserData(newUser);
    if (!userExisted) {
        do {
            newUser.location(mapRand());
        } while (!isLocationValid(newUser.location(), User::OBJECT_TYPE));
        _debug << "New";
    } else {
        _debug << "Existing";
    }
    _debug << " user, " << name << " has logged in." << Log::endl;
    _usernames.insert(name);

    // Send welcome message
    sendMessage(socket, SV_WELCOME);

    // Send new user the map
    sendMessage(newUser.socket(), SV_MAP_SIZE, makeArgs(_mapX, _mapY));
    for (size_t y = 0; y != _mapY; ++y){
        std::ostringstream oss;
        oss << "0," << y << "," << _mapX;
        for (size_t x = 0; x != _mapX; ++x)
            oss << "," << _map[x][y];
        sendMessage(newUser.socket(), SV_TERRAIN, oss.str());
    }

    // Send him everybody else's location
    for (const User &user : _users)
        sendMessage(newUser.socket(), SV_LOCATION, user.makeLocationCommand());

    // Send him object details
    for (const Object &obj : _objects) {
        assert (obj.type());
        sendMessage(newUser.socket(), SV_OBJECT,
                    makeArgs(obj.serial(), obj.location().x, obj.location().y, obj.type()->id()));
        if (!obj.owner().empty())
            sendMessage(newUser.socket(), SV_OWNER, makeArgs(obj.serial(), obj.owner()));
    }

    // Send him his inventory
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i) {
        if (newUser.inventory(i).first)
            sendInventoryMessage(newUser, 0, i);
    }

    // Add new user to list, and broadcast his location
    _users.insert(newUser);
    broadcast(SV_LOCATION, newUser.makeLocationCommand());
}

void Server::removeUser(const std::set<User>::iterator &it){
        // Broadcast message
        broadcast(SV_USER_DISCONNECTED, it->name());

        // Save user data
        writeUserData(*it);

        _usernames.erase(it->name());
        _users.erase(it);
}

void Server::removeUser(const Socket &socket){
    const std::set<User>::iterator it = _users.find(socket);
    if (it != _users.end())
        removeUser(it);
}

bool Server::isValidObject(const Socket &client, const User &user,
                           const std::set<Object>::const_iterator &it) const{
    // Object doesn't exist
    if (it == _objects.end()) {
        sendMessage(client, SV_DOESNT_EXIST);
        return false;
    }
    
    // Check distance from user
    if (distance(user.collisionRect(), it->collisionRect()) > ACTION_DISTANCE) {
        sendMessage(client, SV_TOO_FAR);
        return false;
    }

    return true;
}

void Server::handleMessage(const Socket &client, const std::string &msg){
    int msgCode;
    char del;
    static char buffer[BUFFER_SIZE+1];
    std::istringstream iss(msg);
    User *user = 0;
    while (iss.peek() == '[') {
        iss >> del >> msgCode >> del;
        
        // Discard message if this client has not yet sent CL_I_AM
        std::set<User>::iterator it = _users.find(client);
        if (it == _users.end() && msgCode != CL_I_AM) {
            continue;
        }
        if (msgCode != CL_I_AM) {
            User & userRef = const_cast<User&>(*it);
            user = &userRef;
            user->contact();
        }

        switch(msgCode) {

        case CL_PING:
        {
            Uint32 timeSent;
            iss >> timeSent  >> del;
            if (del != ']')
                return;
            sendMessage(user->socket(), SV_PING_REPLY, makeArgs(timeSent));
            break;
        }

        case CL_I_AM:
        {
            std::string name;
            iss.get(buffer, BUFFER_SIZE, ']');
            name = std::string(buffer);
            iss >> del;
            if (del != ']')
                return;

            // Check that username is valid
            bool invalid = false;
            for (char c : name){
                if (c < 'a' || c > 'z') {
                    sendMessage(client, SV_INVALID_USERNAME);
                    invalid = true;
                    break;
                }
            }
            if (invalid)
                break;

            // Check that user isn't already logged in
            if (_usernames.find(name) != _usernames.end()) {
                sendMessage(client, SV_DUPLICATE_USERNAME);
                invalid = true;
                break;
            }

            addUser(client, name);

            break;
        }

        case CL_LOCATION:
        {
            double x, y;
            iss >> x >> del >> y >> del;
            if (del != ']')
                return;
            user->cancelAction();
            user->updateLocation(Point(x, y));
            broadcast(SV_LOCATION, user->makeLocationCommand());
            break;
        }

        case CL_CRAFT:
        {
            std::string id;
            iss.get(buffer, BUFFER_SIZE, ']');
            id = std::string(buffer);
            iss >> del;
            if (del != ']')
                return;
            user->cancelAction();
            const std::set<Recipe>::const_iterator it = _recipes.find(id);
            if (it == _recipes.end()) {
                sendMessage(client, SV_INVALID_ITEM);
                break;
            }
            ItemSet remaining;
            if (!user->hasItems(it->materials())) {
                sendMessage(client, SV_NEED_MATERIALS);
                break;
            }
            if (!user->hasTools(it->tools())) {
                sendMessage(client, SV_NEED_TOOLS);
                break;
            }
            user->actionCraft(*it);
            sendMessage(client, SV_ACTION_STARTED, makeArgs(it->time()));
            break;
        }

        case CL_CONSTRUCT:
        {
            size_t slot;
            double x, y;
            iss >> slot >> del >> x >> del >> y >> del;
            if (del != ']')
                return;
            user->cancelAction();
            if (slot >= User::INVENTORY_SIZE) {
                sendMessage(client, SV_INVALID_SLOT);
                break;
            }
            const std::pair<const Item *, size_t> &invSlot = user->inventory(slot);
            if (!invSlot.first) {
                sendMessage(client, SV_EMPTY_SLOT);
                break;
            }
            const Item &item = *invSlot.first;
            if (!item.constructsObject()) {
                sendMessage(client, SV_CANNOT_CONSTRUCT);
                break;
            }
            const Point location(x, y);
            const ObjectType &objType = *item.constructsObject();
            if (distance(user->collisionRect(), objType.collisionRect() + location) >
                ACTION_DISTANCE) {
                sendMessage(client, SV_TOO_FAR);
                break;
            }
            if (!isLocationValid(location, objType)) {
                sendMessage(client, SV_BLOCKED);
                break;
            }
            user->actionConstruct(objType, location, slot);
            sendMessage(client, SV_ACTION_STARTED,
                        makeArgs(objType.constructionTime()));
            break;
        }

        case CL_CANCEL_ACTION:
        {
            iss >> del;
            if (del != ']')
                return;
            user->cancelAction();
            break;
        }

        case CL_GATHER:
        {
            int serial;
            iss >> serial >> del;
            if (del != ']')
                return;
            user->cancelAction();
            std::set<Object>::const_iterator it = _objects.find(serial);
            if (!isValidObject(client, *user, it))
                break;
            const Object &obj = *it;
            // Check that the user meets the requirements
            assert (obj.type());
            const std::string &gatherReq = obj.type()->gatherReq();
            if (gatherReq != "none" && !user->hasTool(gatherReq)) {
                sendMessage(client, SV_ITEM_NEEDED, gatherReq);
                break;
            }
            user->actionTarget(&obj);
            sendMessage(client, SV_ACTION_STARTED, makeArgs(obj.type()->actionTime()));
            break;
        }

        case CL_DROP:
        {
            size_t serial, slot;
            iss >> serial >> slot >> del;
            if (del != ']')
                return;
            Item::vect_t *container;
            if (serial == 0)
                container = &user->inventory();
            else {
                auto it = _objects.find(serial);
                if (!isValidObject(client, *user, it))
                    break;
                container = &(const_cast<Object&>(*it).container());
            }

            if (slot >= container->size()) {
                sendMessage(client, SV_INVALID_SLOT);
                break;
            }
            auto &containerSlot = (*container)[slot];
            if (containerSlot.second != 0) {
                containerSlot.first = 0;
                containerSlot.second = 0;
                sendInventoryMessage(*user, serial, slot);
            }
            break;
        }

        case CL_SWAP_ITEMS:
        {
            size_t obj1, slot1, obj2, slot2;
            iss >> obj1 >> del
                >> slot1 >> del
                >> obj2 >> del
                >> slot2 >> del;
            if (del != ']')
                return;
            Item::vect_t
                *containerFrom,
                *containerTo;
            if (obj1 == 0)
                containerFrom = &user->inventory();
            else {
                auto it = _objects.find(obj1);
                if (!isValidObject(client, *user, it))
                    break;
                containerFrom = &(const_cast<Object&>(*it).container());
            }
            if (obj2 == 0)
                containerTo = &user->inventory();
            else {
                auto it = _objects.find(obj2);
                if (!isValidObject(client, *user, it))
                    break;
                containerTo = &(const_cast<Object&>(*it).container());
            }

            if (slot1 >= containerFrom->size() || slot2 >= containerTo->size()) {
                sendMessage(client, SV_INVALID_SLOT);
                break;
            }

            // Perform the swap
            auto
                &slotFrom = (*containerFrom)[slot1],
                &slotTo = (*containerTo)[slot2];
            auto temp = slotTo;
            slotTo = slotFrom;
            slotFrom = temp;

            sendInventoryMessage(*user, obj1, slot1);
            sendInventoryMessage(*user, obj2, slot2);
            break;
        }

        default:
            _debug << Color::RED << "Unhandled message: " << msg << Log::endl;
        }
    }
}

void Server::broadcast(MessageCode msgCode, const std::string &args){
    for (const User &user : _users){
        sendMessage(user.socket(), msgCode, args);
    }
}

void Server::gatherObject(size_t serial, User &user){
    // Give item to user
    const std::set<Object>::iterator it = _objects.find(serial);
    Object &obj = const_cast<Object &>(*it);
    const Item *const toGive = obj.chooseGatherItem();
    size_t qtyToGive = obj.chooseGatherQuantity(toGive);
    const size_t remaining = user.giveItem(toGive, qtyToGive);
    if (remaining > 0) {
        sendMessage(user.socket(), SV_INVENTORY_FULL);
        qtyToGive -= remaining;
    }
    // Remove tree if empty
    obj.removeItem(toGive, qtyToGive);
    if (obj.contents().isEmpty()) {
        // Ensure no other users are targeting this object, as it will be removed.
        for (const User &otherUserConst : _users) {
            const User & otherUser = const_cast<User &>(otherUserConst);
            if (&otherUser != &user &&
                otherUser.actionTarget() &&
                otherUser.actionTarget()->serial() == serial) {

                user.actionTarget(0);
                sendMessage(otherUser.socket(), SV_DOESNT_EXIST);
            }
        }
        broadcast(SV_REMOVE_OBJECT, makeArgs(serial));
        getCollisionChunk(obj.location()).removeObject(serial);
        _objects.erase(it);
    }

#ifdef SINGLE_THREAD
    saveData(_objects);
#else
    std::thread(saveData, _objects).detach();
#endif
}

bool Server::readUserData(User &user){
    XmlReader xr((std::string("Users/") + user.name() + ".usr").c_str());
    if (!xr)
        return false;

    auto elem = xr.findChild("location");
    Point p;
    if (!elem || !xr.findAttr(elem, "x", p.x) || !xr.findAttr(elem, "y", p.y)) {
            _debug("Invalid user data (location)", Color::RED);
            return false;
    }
    bool randomizedLocation = false;
    while (!isLocationValid(p, User::OBJECT_TYPE)) {
        p = mapRand();
        randomizedLocation = true;
    }
    if (randomizedLocation)
        _debug << Color::YELLOW << "Player " << user.name()
               << " was moved due to an invalid location." << Log::endl;
    user.location(p);

    for (auto elem : xr.getChildren("inventory")) {
        for (auto slotElem : xr.getChildren("slot", elem)) {
            int slot; std::string id; int qty;
            if (!xr.findAttr(slotElem, "slot", slot)) continue;
            if (!xr.findAttr(slotElem, "id", id)) continue;
            if (!xr.findAttr(slotElem, "quantity", qty)) continue;

            std::set<Item>::const_iterator it = _items.find(id);
            if (it == _items.end()) {
                _debug("Invalid user data (inventory item).  Removing item.", Color::RED);
                continue;
            }
            user.inventory(slot) =
                std::make_pair<const Item *, size_t>(&*it, static_cast<size_t>(qty));
        }
    }
    return true;

}

void Server::writeUserData(const User &user) const{
    XmlWriter xw(std::string("Users/") + user.name() + ".usr");

    auto e = xw.addChild("location");
    xw.setAttr(e, "x", user.location().x);
    xw.setAttr(e, "y", user.location().y);

    e = xw.addChild("inventory");
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i) {
        const std::pair<const Item *, size_t> &slot = user.inventory(i);
        if (slot.first) {
            auto slotElement = xw.addChild("slot", e);
            xw.setAttr(slotElement, "slot", i);
            xw.setAttr(slotElement, "id", slot.first->id());
            xw.setAttr(slotElement, "quantity", slot.second);
        }
    }

    xw.publish();
}


void Server::sendMessage(const Socket &dstSocket, MessageCode msgCode,
                         const std::string &args) const{
    // Compile message
    std::ostringstream oss;
    oss << '[' << msgCode;
    if (args != "")
        oss << ',' << args;
    oss << ']';

    // Send message
    _socket.sendMessage(oss.str(), dstSocket);
}

void Server::loadData(){
    // Object types
    XmlReader xr("Data/objectTypes.xml");
    for (auto elem : xr.getChildren("objectType")) {
        std::string id;
        if (!xr.findAttr(elem, "id", id))
            continue;
        ObjectType ot(id);

        std::string s; int n;
        if (xr.findAttr(elem, "actionTime", n)) ot.actionTime(n);
        if (xr.findAttr(elem, "constructionTime", n)) ot.constructionTime(n);
        if (xr.findAttr(elem, "gatherReq", s)) ot.gatherReq(s);
        for (auto yield : xr.getChildren("yield", elem)) {
            if (!xr.findAttr(yield, "id", s))
                continue;
            double initMean = 1., initSD = 0, gatherMean = 1, gatherSD = 0;
            xr.findAttr(yield, "initialMean", initMean);
            xr.findAttr(yield, "initialSD", initSD);
            xr.findAttr(yield, "gatherMean", gatherMean);
            xr.findAttr(yield, "gatherSD", gatherSD);
            std::set<Item>::const_iterator itemIt = _items.insert(Item(s)).first;
            ot.addYield(&*itemIt, initMean, initSD, gatherMean, gatherSD);
        }
        auto collisionRect = xr.findChild("collisionRect", elem);
        if (collisionRect) {
            Rect r;
            xr.findAttr(collisionRect, "x", r.x);
            xr.findAttr(collisionRect, "y", r.y);
            xr.findAttr(collisionRect, "w", r.w);
            xr.findAttr(collisionRect, "h", r.h);
            ot.collisionRect(r);
        }
        for (auto objClass :xr.getChildren("class", elem))
            if (xr.findAttr(objClass, "name", s))
                ot.addClass(s);
        auto container = xr.findChild("container", elem);
        if (container) {
            if (xr.findAttr(container, "slots", n)) ot.containerSlots(n);
        }
        
        _objectTypes.insert(ot);
    }

    // Items
    xr.newFile("Data/items.xml");
    for (auto elem : xr.getChildren("item")) {
        std::string id, name;
        if (!xr.findAttr(elem, "id", id) || !xr.findAttr(elem, "name", name))
            continue; // ID and name are mandatory.
        Item item(id);

        std::string s; int n;
        if (xr.findAttr(elem, "stackSize", n)) item.stackSize(n);
        if (xr.findAttr(elem, "constructs", s))
            // Create dummy ObjectType if necessary
            item.constructsObject(&*(_objectTypes.insert(ObjectType(s)).first));

        for (auto child : xr.getChildren("class", elem))
            if (xr.findAttr(child, "name", s)) item.addClass(s);
        
        std::pair<std::set<Item>::iterator, bool> ret = _items.insert(item);
        if (!ret.second) {
            Item &itemInPlace = const_cast<Item &>(*ret.first);
            itemInPlace = item;
        }
    }

    // Recipes
    xr.newFile("Data/recipes.xml");
    for (auto elem : xr.getChildren("recipe")) {
        std::string id, name;
        if (!xr.findAttr(elem, "id", id))
            continue; // ID is mandatory.
        Recipe recipe(id);

        std::string s; int n;
        if (!xr.findAttr(elem, "product", s))
            continue; // product is mandatory.
        auto it = _items.find(s);
        if (it == _items.end()) {
            _debug << Color::RED << "Skipping recipe with invalid product " << s << Log::endl;
            continue;
        }
        recipe.product(&*it);

        if (xr.findAttr(elem, "time", n)) recipe.time(n);

        for (auto child : xr.getChildren("material", elem)) {
            int matQty = 1;
            xr.findAttr(child, "quantity", matQty);
            if (xr.findAttr(child, "id", s)) {
                auto it = _items.find(Item(s));
                if (it == _items.end()) {
                    _debug << Color::RED << "Skipping invalid recipe material " << s << Log::endl;
                    continue;
                }
                recipe.addMaterial(&*it, matQty);
            }
        }

        for (auto child : xr.getChildren("tool", elem)) {
            if (xr.findAttr(child, "name", s)) {
                recipe.addTool(s);
            }
        }
        
        _recipes.insert(recipe);
    }

    std::ifstream fs;
    // Detect/load state
    do {
        if (cmdLineArgs.contains("new"))
            break;

        // Map
        xr.newFile("World/map.world");
        if (!xr)
            break;
        auto elem = xr.findChild("size");
        if (!elem || !xr.findAttr(elem, "x", _mapX) || !xr.findAttr(elem, "y", _mapY)) {
            _debug("Map size missing or incomplete.", Color::RED);
            break;
        }
        _map = std::vector<std::vector<size_t> >(_mapX);
        for (size_t x = 0; x != _mapX; ++x)
            _map[x] = std::vector<size_t>(_mapY, 0);
        for (auto row : xr.getChildren("row")) {
            size_t y;
            if (!xr.findAttr(row, "y", y) || y >= _mapY)
                break;
            for (auto tile : xr.getChildren("tile", row)) {
                size_t x;
                if (!xr.findAttr(tile, "x", x) || x >= _mapX)
                    break;
                if (!xr.findAttr(tile, "terrain", _map[x][y]))
                    break;
            }
        }

        // Objects
        xr.newFile("World/objects.world");
        if (!xr)
            break;
        for (auto elem : xr.getChildren("object")) {
            std::string s;
            if (!xr.findAttr(elem, "id", s)) {
                _debug("Skipping importing object with no type.", Color::RED);
                continue;
            }

            Point p;
            auto loc = xr.findChild("location", elem);
            if (!xr.findAttr(loc, "x", p.x) || !xr.findAttr(loc, "y", p.y)) {
                _debug("Skipping importing object with invalid/no location", Color::RED);
                continue;
            }

            std::set<ObjectType>::const_iterator it = _objectTypes.find(s);
            if (it == _objectTypes.end()) {
                _debug << Color::RED << "Skipping importing object with unknown type \"" << s
                       << "\"." << Log::endl;
                continue;
            }

            Object obj(&*it, p);

            size_t n;
            if (xr.findAttr(elem, "owner", s)) obj.owner(s);

            ItemSet contents;
            for (auto content : xr.getChildren("gatherable", elem)) {
                if (!xr.findAttr(content, "id", s))
                    continue;
                n = 1;
                xr.findAttr(content, "quantity", n);
                contents.set(&*_items.find(s), n);
            }
            obj.contents(contents);

            size_t q;
            for (auto inventory : xr.getChildren("inventory", elem)) {
                if (!xr.findAttr(inventory, "id", s))
                    continue;
                if (!xr.findAttr(inventory, "slot", n))
                    continue;
                q = 1;
                xr.findAttr(inventory, "qty", q);
                if (obj.container().size() <= n) {
                    _debug << Color::RED << "Skipping object with invalid inventory slot." << Log::endl;
                    continue;
                }
                auto &invSlot = obj.container()[n];
                invSlot.first = &*_items.find(s);
                invSlot.second = q;
            }

            _objects.insert(obj);
        }

        return;
    } while (false);

    _debug("No/invalid world data detected; generating new world.", Color::YELLOW);
    generateWorld();
}

void Server::saveData(const std::set<Object> &objects){
    // Map
#ifndef SINGLE_THREAD
    static std::mutex mapFileMutex;
    mapFileMutex.lock();
#endif
    XmlWriter xw("World/map.world");
    auto e = xw.addChild("size");
    const Server &server = Server::instance();
    xw.setAttr(e, "x", server._mapX);
    xw.setAttr(e, "y", server._mapY);
    for (size_t y = 0; y != server._mapY; ++y){
        auto row = xw.addChild("row");
        xw.setAttr(row, "y", y);
        for (size_t x = 0; x != server._mapX; ++x){
            auto tile = xw.addChild("tile", row);
            xw.setAttr(tile, "x", x);
            xw.setAttr(tile, "terrain", server._map[x][y]);
        }
    }
    xw.publish();
#ifndef SINGLE_THREAD
    mapFileMutex.unlock();
#endif

    // Objects
#ifndef SINGLE_THREAD
    static std::mutex objectsFileMutex;
    objectsFileMutex.lock();
#endif
    xw.newFile("World/objects.world");
    for (const Object &obj : objects) {
        if (!obj.type())
            continue;
        auto e = xw.addChild("object");

        xw.setAttr(e, "id", obj.type()->id());

        for (auto &content : obj.contents()) {
            auto contentE = xw.addChild("gatherable", e);
            xw.setAttr(contentE, "id", content.first->id());
            xw.setAttr(contentE, "quantity", content.second);
        }

        if (!obj.owner().empty())
            xw.setAttr(e, "owner", obj.owner());

        auto loc = xw.addChild("location", e);
        xw.setAttr(loc, "x", obj.location().x);
        xw.setAttr(loc, "y", obj.location().y);

        const auto container = obj.container();
        for (size_t i = 0; i != container.size(); ++i) {
            if (container[i].second == 0)
                continue;
            auto invSlotE = xw.addChild("inventory", e);
            xw.setAttr(invSlotE, "slot", i);
            xw.setAttr(invSlotE, "item", container[i].first->id());
            xw.setAttr(invSlotE, "qty", container[i].second);
        }
    }
    xw.publish();
#ifndef SINGLE_THREAD
    objectsFileMutex.unlock();
#endif
}

void Server::generateWorld(){
    _mapX = 30;
    _mapY = 30;

    // Grass by default
    _map = std::vector<std::vector<size_t> >(_mapX);
    for (size_t x = 0; x != _mapX; ++x){
        _map[x] = std::vector<size_t>(_mapY);
        for (size_t y = 0; y != _mapY; ++y)
            _map[x][y] = 0;
    }

    // Stone in circles
    for (int i = 0; i != 5; ++i) {
        const size_t centerX = rand() % _mapX;
        const size_t centerY = rand() % _mapY;
        for (size_t x = 0; x != _mapX; ++x)
            for (size_t y = 0; y != _mapY; ++y) {
                Point thisTile(x, y);
                if (y % 2 == 1)
                    thisTile.x -= .5;
                const double dist = distance(Point(centerX, centerY), thisTile);
                if (dist <= 4)
                    _map[x][y] = 1;
            }
    }

    // Roads
    for (int i = 0; i != 2; ++i) {
        const Point
            start(rand() % _mapX, rand() % _mapY),
            end(rand() % _mapX, rand() % _mapY);
        for (size_t x = 0; x != _mapX; ++x)
            for (size_t y = 0; y != _mapY; ++y) {
                Point thisTile(x, y);
                if (y % 2 == 1)
                    thisTile.x -= .5;
                double dist = distance(thisTile, start, end);
                if (dist <= 1)
                    _map[x][y] = 2;
            }
    }

    // River: randomish line
    const Point
        start(rand() % _mapX, rand() % _mapY),
        end(rand() % _mapX, rand() % _mapY);
    for (size_t x = 0; x != _mapX; ++x)
        for (size_t y = 0; y != _mapY; ++y) {
            Point thisTile(x, y);
            if (y % 2 == 1)
                thisTile.x -= .5;
            double dist = distance(thisTile, start, end) + randDouble() * 2 - 1;
            if (dist <= 0.5)
                _map[x][y] = 3;
            else if (dist <= 2)
                _map[x][y] = 4;
        }

    // Add tiles to chunks
    for (size_t x = 0; x != _mapX; ++x)
        for (size_t y = 0; y != _mapY; ++y) {
            Point tileMidpoint(x * TILE_W, y * TILE_H + TILE_H / 2);
            if (y % 2 == 1)
                tileMidpoint.x += TILE_W / 2;
            // Add terrain info to adjacent chunks too
            auto superChunk = getCollisionSuperChunk(tileMidpoint);
            for (CollisionChunk *chunk : superChunk){
                size_t tile = _map[x][y];
                bool passable = tile != 3 && tile != 4;
                chunk->addTile(x, y, passable);
            }
        }

    const ObjectType *const branch = &*_objectTypes.find(std::string("branch"));
    for (int i = 0; i != 30; ++i){
        Point loc;
        do {
            loc = mapRand();
        } while (!isLocationValid(loc, *branch));
        addObject(branch, loc);
    }

    const ObjectType *const tree = &*_objectTypes.find(std::string("tree"));
    for (int i = 0; i != 10; ++i) {
        Point loc;
        do {
            loc = mapRand();
        } while (!isLocationValid(loc, *tree));
        addObject(tree, loc);
    }

    const ObjectType *const chest = &*_objectTypes.find(std::string("chest"));
    for (int i = 0; i != 10; ++i) {
        Point loc;
        do {
            loc = mapRand();
        } while (!isLocationValid(loc, *chest));
        addObject(chest, loc);
    }
}

Point Server::mapRand() const{
    return Point(randDouble() * (_mapX - 0.5) * TILE_W,
                 randDouble() * _mapY * TILE_H);
}

bool Server::itemIsClass(const Item *item, const std::string &className) const{
    assert (item);
    return item->isClass(className);
}

void Server::addObject (const ObjectType *type, const Point &location, const User *owner){
    Object newObj(type, location);
    if (owner)
        newObj.owner(owner->name());
    auto it = _objects.insert(newObj).first;
    broadcast(SV_OBJECT, makeArgs(newObj.serial(), location.x, location.y, type->id()));
    if (owner)
        broadcast(SV_OWNER, makeArgs(newObj.serial(), newObj.owner()));

    // Add item to relevant chunk
    if (type->collides())
        getCollisionChunk(location).addObject(&*it);
}

void Server::sendInventoryMessage(const User &user, size_t serial, size_t slot) const{
    const Item::vect_t *container;
    if (serial == 0)
        container = &user.inventory();
    else {
        auto it = _objects.find(serial);
        if (it == _objects.end())
            return; // Object doesn't exist.
        else
            container = &it->container();
    }
    if (slot >= container->size()) {
        sendMessage(user.socket(), SV_INVALID_SLOT);
        return;
    }
    auto containerSlot = (*container)[slot];
    std::string itemID = containerSlot.first ? containerSlot.first->id() : "none";
    sendMessage(user.socket(), SV_INVENTORY, makeArgs(serial, slot, itemID, containerSlot.second));
}
