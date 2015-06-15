#ifndef SERVER_H
#define SERVER_H

#include <set>
#include <utility>
#include <queue>
#include <string>

#include "Args.h"
#include "Branch.h"
#include "Item.h"
#include "Log.h"
#include "Socket.h"
#include "User.h"
#include "messageCodes.h"

class Server{
public:
    Server();
    ~Server();
    void run();

    static const Uint32 CLIENT_TIMEOUT; // How much radio silence before we drop a client

    static bool isServer;

private:

    static const int MAX_CLIENTS;
    static const size_t BUFFER_SIZE;

    static const int ACTION_DISTANCE; // How close a character must be to interact with an object

    Uint32 _time;

    Socket _socket;

    bool _loop;

    void loadData(); // Load data from files, or randomly generate new world.
    void saveData();

    std::queue<std::pair<Socket, std::string> > _messages;

    std::set<Socket> _clientSockets; // All connected sockets, including those without registered users
    std::set<User> _users; // All connected users
    std::set<std::string> _usernames; // All connected users' names, for faster lookup of duplicates

    std::set<Branch> _branches;

    std::set<Item> _items;

    mutable Log _debug;

    void draw() const;

    // Add the newly logged-in user; this happens not once the client connects, but rather when a CL_I_AM message is received.
    void addUser(const Socket &socket, const std::string &name);

    // Remove traces of a user who has disconnected.
    void removeUser(const Socket &socket);
    void removeUser(std::set<User>::iterator &it);

    void sendMessage(const Socket &dstSocket, MessageCode msgCode, const std::string &args = "") const;

    // Send a command to all users
    void broadcast(MessageCode msgCode, const std::string &args);

    void checkSockets();
    void handleMessage(const Socket &client, const std::string &msg);

    bool readUserData(User &user); // true: save data existed
    void writeUserData(const User &user) const;
    static const Uint32 SAVE_FREQUENCY;
    Uint32 _lastSave;
};

#endif
