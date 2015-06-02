#include <sstream>

#include "Client.h"
#include "Socket.h"
#include "User.h"
#include "messageCodes.h"
#include "util.h"

const double User::LEGAL_MOVEMENT_MARGIN = 1.05;

User::User(const std::string &name, const Point &loc, const Socket &socket):
_name(name),
location(loc),
_socket(socket),
_lastLocUpdate(SDL_GetTicks()){}

User::User(const Socket &rhs):
_socket(rhs){}

bool User::operator<(const User &rhs) const{
    return _socket < rhs._socket;
}

const std::string &User::getName() const{
    return _name;
}

const Socket &User::getSocket() const{
    return _socket;
}

std::string User::makeLocationCommand() const{
    std::ostringstream oss;
    oss << '[' << SV_LOCATION << ',' << _name << ',' << location.x << ',' << location.y << ']';
    return oss.str();
}

void User::updateLocation(double x, double y){
    Uint32 newTime = SDL_GetTicks();
    Uint32 timeElapsed = newTime - _lastLocUpdate;
    _lastLocUpdate = newTime;

    // Max legal distance: straight line
    double maxLegalDistance = timeElapsed / 1000.0 * Client::MOVEMENT_SPEED * LEGAL_MOVEMENT_MARGIN;
    double distanceMoved = distance(location, Point(x, y));
    if (distanceMoved <= maxLegalDistance) {
        location.x = x;
        location.y = y;
        return;
    }

    // Moved too far: interpolate
    double
        xNorm = (x - location.x) / distanceMoved,
        yNorm = (y - location.y) / distanceMoved;
    location.x += xNorm * maxLegalDistance;
    location.y += yNorm * maxLegalDistance;
}
