// (C) 2016 Tim Gurto

#include <cassert>
#include <thread>

#include "ServerTestInterface.h"
#include "Test.h"

const std::vector<std::vector<size_t> > ServerTestInterface::TINY_MAP(1, std::vector<size_t>(1,0));

void ServerTestInterface::run(){
    Server &server = _server;
    std::thread([& server](){ server.run(); }).detach();
    WAIT_UNTIL (_server._running);
}

void ServerTestInterface::stop(){
    _server._loop = false;
    WAIT_UNTIL (!_server._running);
}

void ServerTestInterface::setMap(const std::vector<std::vector<size_t> > &map){
    assert(map.size() > 0);
    _server._mapX = map.size();

    assert(map[0].size() > 0);
    _server._mapY = map[0].size();
    for (auto col : map)
        assert(col.size() == _server._mapY);

    _server._map = map;
}

void ServerTestInterface::addObject(const std::string &typeName, const Point &loc){
    const ObjectType *const type = _server.findObjectTypeByName(typeName);
    _server.addObject(type, loc);
}
