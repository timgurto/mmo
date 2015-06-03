#include <SDL.h>
#include <sstream>
#include <fstream>

#include "Client.h" //TODO remove; only here for random initial placement
#include "Socket.h"
#include "Server.h"
#include "User.h"
#include "messageCodes.h"

const int Server::MAX_CLIENTS = 20;
const int Server::BUFFER_SIZE = 1023;

const Uint32 Server::PING_FREQUENCY = 5000;

Server::Server(const Args &args):
_args(args),
_loop(true),
_debug(30),
_socket(&_debug),
_time(SDL_GetTicks()),
_lastPing(_time){
    _debug << args << Log::endl;

    int screenX = _args.contains("left") ?
                  _args.getInt("left") :
                  SDL_WINDOWPOS_UNDEFINED;
    int screenY = _args.contains("top") ?
                  _args.getInt("top") :
                  SDL_WINDOWPOS_UNDEFINED;
    _window = SDL_CreateWindow("Server", screenX, screenY, 800, 600, SDL_WINDOW_SHOWN);
    if (!_window)
        return;
    _screen = SDL_GetWindowSurface(_window);

    ServerMessage::serverSocket = &_socket;

    _debug("Server initialized");

    // Socket details
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888);

    _socket.bind(serverAddr);
    _debug << "Server address: " << inet_ntoa(serverAddr.sin_addr) << ":" << ntohs(serverAddr.sin_port) << Log::endl;
    _socket.listen();

    _debug("Listening for connections");
}

Server::~Server(){
    if (_window)
        SDL_DestroyWindow(_window);
}

void Server::checkSockets(){
    // Populate socket list with active sockets
    static fd_set readFDs;
    FD_ZERO(&readFDs);
    FD_SET(_socket.getRaw(), &readFDs);
    for (std::set<Socket>::iterator it = _clientSockets.begin(); it != _clientSockets.end(); ++it)
        FD_SET(it->getRaw(), &readFDs);

    // Poll for activity
    static timeval selectTimeout = {0, 10000};
    int activity = select(0, &readFDs, 0, 0, &selectTimeout);
    if (activity == SOCKET_ERROR) {
        _debug << Color::RED << "Error polling sockets: " << WSAGetLastError() << Log::endl;
        return;
    }
    _time = SDL_GetTicks();

    // Activity on server socket: new connection
    if (FD_ISSET(_socket.getRaw(), &readFDs)) {
        if (_clientSockets.size() == MAX_CLIENTS)
           _debug("No room for additional clients; all slots full");
        else {
            sockaddr_in clientAddr;
            SOCKET tempSocket = accept(_socket.getRaw(), (sockaddr*)&clientAddr, (int*)&Socket::sockAddrSize);
            if (tempSocket == SOCKET_ERROR) {
                _debug << Color::RED << "Error accepting connection: " << WSAGetLastError() << Log::endl;
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
            int charsRead = recv(raw, buffer, BUFFER_SIZE, 0);
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
                //_debug << "recv from client " << raw << ": " << buffer << Log::endl;
                if (charsRead == BUFFER_SIZE)
                    _debug << Color::RED << "Input buffer full; some messages are likely being discarded" << Log::endl;
                _messages.push(std::make_pair(*it, std::string(buffer)));
            }
        }
        ++it;
    }
}

void Server::run(){
    while (_loop) {
        _time = SDL_GetTicks();

        // Send pings
        if (_time - _lastPing > PING_FREQUENCY) {
            std::ostringstream oss;
            oss << _time ;
            broadcast(SV_PING, oss.str());
            _lastPing = _time;
        }

        // Deal with any messages from the server
        while (!_messages.empty()){
            handleMessage(_messages.front().first, _messages.front().second);
            _messages.pop();
        }

        // Handle user events
        static SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            switch(e.type) {
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE){
                    _loop = false;
                }
                break;
            case SDL_QUIT:
                _loop = false;
                break;

            default:
                // Unhandled event
                ;
            }
        }

        draw();

        checkSockets();
        checkSentMessages();

        SDL_Delay(10);
    }

    // Save all user data
    for(std::set<User>::const_iterator it = _users.begin(); it != _users.end(); ++it){
        writeUserData(*it);
    }
}

void Server::draw() const{
    SDL_FillRect(_screen, 0, Color::BLACK);
    _debug.draw(_screen);
    SDL_UpdateWindowSurface(_window);
}

void Server::addUser(const Socket &socket, const std::string &name){
    User newUser(name, 0, socket);
    bool userExisted = readUserData(newUser);
    if (!userExisted) {
        newUser.location.x = rand() % (Client::SCREEN_WIDTH - 20);
        newUser.location.y = rand() % (Client::SCREEN_HEIGHT - 40);
    }
    _usernames.insert(name);

    // Send new user everybody else's location
    for (std::set<User>::const_iterator it = _users.begin(); it != _users.end(); ++it)
        _sentMessages.insert(ServerMessage(newUser.getSocket(), SV_LOCATION, it->makeLocationCommand()));

    // Add new user to list, and broadcast his location
    _users.insert(newUser);
    broadcast(SV_LOCATION, newUser.makeLocationCommand());

    // Measure latency with new user
    std::ostringstream oss;
    oss << _time;
    _sentMessages.insert(ServerMessage(socket, SV_PING, oss.str()));

    _debug << "New user, " << name << " has logged in." << Log::endl;
}

void Server::removeUser(const Socket &socket){
    std::set<User>::iterator it = _users.find(socket);
    if (it != _users.end()) {
        // Broadcast message
        broadcast(SV_USER_DISCONNECTED, it->getName());

        // Save user data
        writeUserData(*it);

        _usernames.erase(it->getName());
        _users.erase(it);
    }
}

void Server::handleMessage(const Socket &client, const std::string &msg){
    int eof = std::char_traits<wchar_t>::eof();
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
            user = &*it;
        }

        switch(msgCode) {

        case CL_PING_REPLY:
        {
            Uint32 timeSent, timeReplied;
            iss >> timeSent >> del >> timeReplied >> del;
            if (del != ']')
                return;
            user->latency = (_time - timeSent) / 2;
            std::ostringstream oss;
            oss << timeReplied;
            _sentMessages.insert(ServerMessage(user->getSocket(), SV_PING_REPLY_2, oss.str()));
            break;
        }

        case CL_LOCATION:
        {
            double x, y;
            iss >> x >> del >> y >> del;
            if (del != ']')
                return;
            user->updateLocation(x, y);
            broadcast(SV_LOCATION, user->makeLocationCommand());
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
            for (std::string::const_iterator it = name.begin(); it != name.end(); ++it){
                if (*it < 'a' || *it > 'z') {
                    _sentMessages.insert(ServerMessage(client, SV_INVALID_USERNAME));
                    invalid = true;
                    break;
                }
            }
            if (invalid)
                break;

            // Check that user isn't already logged in
            if (_usernames.find(name) != _usernames.end()) {
                _sentMessages.insert(ServerMessage(client, SV_DUPLICATE_USERNAME));
                invalid = true;
                break;
            }

            addUser(client, name);
            break;
        }

        default:
            _debug << Color::RED << "Unhandled message: " << msg;
        }
    }
}

void Server::broadcast(MessageCode msgCode, const std::string &args){
    for (std::set<User>::const_iterator it = _users.begin(); it != _users.end(); ++it){
        _sentMessages.insert(ServerMessage(it->getSocket(), msgCode, args));
    }
}

bool Server::readUserData(User &user){
    std::string filename = std::string("Users/") + user.getName() + ".usr";
    std::ifstream fs(filename.c_str());
    if (!fs.good()) // File didn't exist
        return false;
    fs >> user.location.x >> user.location.y;
    fs.close();
    return true;
}

void Server::writeUserData(const User &user) const{
    std::string filename = std::string("Users/") + user.getName() + ".usr";
    std::ofstream fs(filename.c_str());
    fs << user.location.x << ' ' << user.location.y;
    fs.close();
}
