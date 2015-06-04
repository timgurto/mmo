#ifndef SERVER_H
#define SERVER_H

#include <set>
#include <utility>
#include <queue>
#include <string>

#include "Args.h"
#include "Log.h"
#include "ServerMessage.h"
#include "Socket.h"
#include "User.h"

class Server{
public:
    Server(const Args &args);
    ~Server();
    void run();

    static const Uint32 ACK_TIMEOUT;

private:
    const Args &_args; //comand-line args

    static const int MAX_CLIENTS;
    static const int BUFFER_SIZE;

    Uint32 _time;
    std::set<ServerMessage> _sentMessages;
    void checkSentMessages(); // If any sent messages haven't been acknowledged in time, resend them

    Socket _socket;
    SDL_Window *_window;
    SDL_Surface *_screen;

    bool _loop;

    std::queue<std::pair<Socket, std::string> > _messages;

    std::set<Socket> _clientSockets; // All connected sockets, including those without registered users
    std::set<User> _users; // All connected users
    std::set<std::string> _usernames; // All connected users' names, for faster lookup of duplicates

    mutable Log _debug;

    void draw() const;

    // Add the newly logged-in user; this happens not once the client connects, but rather when a CL_I_AM message is received.
    void addUser(const Socket &socket, const std::string &name);

    // Remove traces of a user who has disconnected.
    void removeUser(const Socket &socket);

    // Send a command to all users
    void broadcast(MessageCode msgCode, const std::string &args);

    // Send a user's location to all users
    //void sendUserLocation(const User &user) const;

    void checkSockets();
    void handleMessage(const Socket &client, const std::string &msg);

    bool readUserData(User &user); // true: save data existed
    void writeUserData(const User &user) const;
};

#endif
