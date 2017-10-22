#include "Server.h"
#include "Wars.h"
#include "../XmlReader.h"
#include "../XmlWriter.h"


bool Wars::Belligerent::operator<(const Belligerent &rhs) const{
    if (name != rhs.name)
        return name < rhs.name;
    return type < rhs.type;
}

bool Wars::Belligerent::operator==(const Belligerent &rhs) const {
    return name == rhs.name && type == rhs.type;
}

bool Wars::Belligerent::operator!=(const Belligerent &rhs) const {
    return ! (*this == rhs);
}

void Wars::Belligerent::alertToWarWith(const Belligerent rhs) const{
    const Server &server = Server::instance();

    if (type == PLAYER)
        server.alertUserToWar(name, rhs);
    else{
        const auto members = server.cities().membersOf(name);
        for (const auto &member : members)
            server.alertUserToWar(member, rhs);
    }
}

Wars::War::War(const Belligerent & b1, const Belligerent & b2) {
    if (b1 < b2) {
        this->b1 = b1;
        this->b2 = b2;
    } else if (b2 < b1) {
        this->b1 = b2;
        this->b2 = b1;
    } else
        assert(false);
}

bool Wars::War::operator<(const War & rhs) const {
    if (b1 != rhs.b1)
        return b1 < rhs.b1;
    return b2 < rhs.b2;
}

void Wars::declare(const Belligerent &a, const Belligerent &b){
    if (isAtWar(a, b))
        return;
    container.insert({ a, b });

    a.alertToWarWith(b);
    b.alertToWarWith(a);
}

bool Wars::isAtWar(Belligerent a, Belligerent b) const{
    changePlayerBelligerentToHisCity(a);
    changePlayerBelligerentToHisCity(b);

    auto warExists = container.find({ a, b }) != container.end();
    return warExists;
}

void Wars::changePlayerBelligerentToHisCity(Belligerent &belligerent){
    if (belligerent.type == Belligerent::CITY)
        return;
    
    const Server &server = Server::instance();
    const auto &playerCity = server.cities().getPlayerCity(belligerent.name);
    if (playerCity.empty())
        return;

    belligerent.type = Belligerent::CITY;
    belligerent.name = playerCity;
}

void Wars::sendWarsToUser(const User & user, const Server & server) const {
    auto userAsBelligerent = Belligerent{ user.name() };
    changePlayerBelligerentToHisCity(userAsBelligerent);

    auto enemies = std::set<Belligerent>{};
    for (const auto &war : container) {
        auto otherBelligerent = Belligerent{};
        if (war.b1 == userAsBelligerent)
            otherBelligerent = war.b2;
        else if (war.b2 == userAsBelligerent)
            otherBelligerent = war.b1;
        else
            continue;
        if (otherBelligerent.type == Belligerent::PLAYER)
            changePlayerBelligerentToHisCity(otherBelligerent);

        enemies.insert(otherBelligerent);
    }
    for (auto &enemy : enemies) {
        const MessageCode code = enemy.type == Wars::Belligerent::CITY ?
            SV_AT_WAR_WITH_CITY : SV_AT_WAR_WITH_PLAYER;
        server.sendMessage(user.socket(), code, enemy.name);
    }
}

void Wars::writeToXMLFile(const std::string &filename) const{
    XmlWriter xw(filename);
    for (const auto &war : container) {
        auto e = xw.addChild("war");
        xw.setAttr(e, "name1", war.b1.name);
        if (war.b1.type == Belligerent::CITY)
            xw.setAttr(e, "isCity1", 1);
        xw.setAttr(e, "name2", war.b2.name);
        if (war.b2.type == Belligerent::CITY)
            xw.setAttr(e, "isCity2", 1);
    }
    xw.publish();
}

void Wars::readFromXMLFile(const std::string &filename){
    XmlReader xr(filename);
    if (! xr){
        Server::debug()("Failed to load data from " + filename, Color::FAILURE);
        return;
    }
    for (auto elem : xr.getChildren("war")) {
        Wars::Belligerent b1, b2;
        if (!xr.findAttr(elem, "name1", b1.name) ||
            !xr.findAttr(elem, "name2", b2.name)) {
                Server::debug()("Skipping war with insufficient belligerents.", Color::RED);
                continue;
        }
        int n;
        if (xr.findAttr(elem, "isCity1", n) && n != 0)
            b1.type = Belligerent::CITY;
        if (xr.findAttr(elem, "isCity2", n) && n != 0)
            b2.type = Belligerent::CITY;
        declare(b1, b2);
    }
}
