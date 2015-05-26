#include <cassert>
#include <SDL.h>
#include <sstream>

#include "Socket.h"
#include "Server.h"
#include "messageCodes.h"

const int Server::MAX_CLIENTS = 10;
const int Server::BUFFER_SIZE = 100;

int startSocketServer(void *server){
    ((Server*)server)->runSocketServer();
    return 0;
}

Server::Server():
_loop(true){
    int ret = SDL_Init(SDL_INIT_VIDEO);
    if (ret < 0)
        return;

    SDL_Thread *socketThreadID = SDL_CreateThread(startSocketServer, "Server socket handler", this);

    _window = SDL_CreateWindow("Server", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    if (!_window)
        return;
    _screen = SDL_GetWindowSurface(_window);
}

Server::~Server(){
    if (_window)
        SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Server::runSocketServer(){
    Socket s;
    // Socket details
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888);

    s.bind(serverAddr);
    s.listen();

    fd_set readFDs;
    char buffer[BUFFER_SIZE+1];
    for (int i = 0; i != BUFFER_SIZE; ++i)
        buffer[i] = '\0';

    while (true) {
        // Populate socket list with active sockets
        FD_ZERO(&readFDs);
        FD_SET(s.raw(), &readFDs);
        for (std::set<SOCKET>::iterator it = _clientSockets.begin(); it != _clientSockets.end(); ++it)
            FD_SET(*it, &readFDs);
        
        // Poll for activity
        int activity = select(0, &readFDs, 0, 0, 0);
        if (activity == SOCKET_ERROR) {
            std::cout << "Error polling sockets: " << WSAGetLastError() << std::endl;
            return;
        }

        // Activity on server socket: new connection
        if (FD_ISSET(s.raw(), &readFDs)) {
            if (_clientSockets.size() == MAX_CLIENTS)
                std::cout << "No room for additional clients; all slots full" << std::endl;
            else {
                sockaddr_in clientAddr;
                SOCKET tempSocket = accept(s.raw(), (sockaddr*)&clientAddr, (int*)&Socket::sockAddrSize);
                if (tempSocket == SOCKET_ERROR) {
                    std::cout << "Error accepting connection: " << WSAGetLastError() << std::endl;
                } else {
                    std::cout << "Connection accepted: "
                              << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl
                              << ", socket number = " << tempSocket << std::endl;
                    _clientSockets.insert(tempSocket);
                    addNewUser(tempSocket);
                }
            }
        }

        // Activity on client socket: message received or client disconnected
        for (std::set<SOCKET>::iterator it = _clientSockets.begin(); it != _clientSockets.end();) {
            if (FD_ISSET(*it, &readFDs)) {
                sockaddr_in clientAddr;
                getpeername(*it, (sockaddr*)&clientAddr, (int*)&Socket::sockAddrSize);
                int charsRead = recv(*it, buffer, BUFFER_SIZE, 0);
                if (charsRead == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAECONNRESET) {
                        // Client disconnected
                        std::cout << "Client " << *it << " disconnected" << std::endl;
                        closesocket(*it);
                        _clientSockets.erase(it++);
                        continue;
                    } else {
                        std::cout << "Error receiving message: " << err << std::endl;
                    }
                } else if (charsRead == 0) {
                    // Client disconnected
                    std::cout << "Client " << *it << " disconnected" << std::endl;
                    closesocket(*it);
                    _clientSockets.erase(it++);
                    continue;
                } else {
                    // Message received
                    buffer[charsRead] = '\0';
                    std::cout << "Message received from client " << *it << ": " << buffer << std::endl;
                    _messages.push(std::make_pair(*it, std::string(buffer)));
                }
            }
            ++it;
        }

    }

    // Should never reach here
    assert (false);
}

void Server::run(){

    while (_loop) {
        // Deal with any messages from the server
        if (!_messages.empty()){
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

        SDL_FillRect(_screen, 0, SDL_MapRGB(_screen->format, 0, 0, 0));
        SDL_UpdateWindowSurface(_window);
    }
}

void Server::addNewUser(SOCKET socket){
    // Give new user a location
    _userLocations[socket] = std::make_pair(500, 200);

    sendUserLocation(socket);

    // Send new user everybody else's location
    for (std::set<SOCKET>::iterator it = _clientSockets.begin(); it != _clientSockets.end(); ++it){
        if (*it != socket && _userLocations.find(*it) != _userLocations.end()) {
            std::ostringstream oss;
            oss << '[' << MSG_OTHER_LOCATION << ',' << *it << ',' << _userLocations[*it].first << ',' << _userLocations[*it].second << ']';
            Socket::sendMessage(socket, oss.str());
        }
    }
}

void Server::handleMessage(SOCKET user, std::string msg){
    int eof = std::char_traits<wchar_t>::eof();
    int msgCode;
    char del;
    bool sendLocation = false;
    std::istringstream iss(msg);
    while (iss.peek() == '[') {
        iss >> del >> msgCode >> del;
        switch(msgCode) {

        case REQ_MOVE_UP:
            if (del != ']')
                return;
            if (_userLocations.find(user) == _userLocations.end())
                break;
            _userLocations[user].second -= 20;
            sendLocation = true;
            break;

        case REQ_MOVE_DOWN:
            if (del != ']')
                return;
            if (_userLocations.find(user) == _userLocations.end())
                break;
            _userLocations[user].second += 20;
            sendLocation = true;
            break;

        case REQ_MOVE_LEFT:
            if (del != ']')
                return;
            if (_userLocations.find(user) == _userLocations.end())
                break;
            _userLocations[user].first -= 20;
            sendLocation = true;
            break;

        case REQ_MOVE_RIGHT:
            if (del != ']')
                return;
            if (_userLocations.find(user) == _userLocations.end())
                break;
            _userLocations[user].first += 20;
            sendLocation = true;
            break;

        default:
            ;
        }
    }

    if (sendLocation) {
        sendUserLocation(user);
    }

}

void Server::sendUserLocation(SOCKET socket){
    // User himself
    std::ostringstream oss;
    oss << '[' << MSG_LOCATION << ',' << _userLocations[socket].first << ',' << _userLocations[socket].second << ']';
    Socket::sendMessage(socket, oss.str());

    // Other users
    oss.str("");
    oss << '[' << MSG_OTHER_LOCATION << ',' << socket << ',' << _userLocations[socket].first << ',' << _userLocations[socket].second << ']';
    for (std::set<SOCKET>::iterator it = _clientSockets.begin(); it != _clientSockets.end(); ++it)
        if (*it != socket)
            Socket::sendMessage(*it, oss.str());
}
