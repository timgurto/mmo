#include <cassert>
#include <thread>

#include "TestServer.h"
#include "Test.h"
#include "../Args.h"

extern Args cmdLineArgs;

TestServer::TestServer(){
    _server = new Server;
    _server->loadData("testing/data/minimal");
    Server::newPlayerSpawnLocation = Point(10, 10);
    Server::newPlayerSpawnRange = 0;
}

TestServer TestServer::KeepOldData(){
    cmdLineArgs.remove("new");
    TestServer s;
    cmdLineArgs.add("new");
    return s;
}

TestServer::~TestServer(){
    if (_server == nullptr)
        return;
    stop();
    delete _server;
}

TestServer::TestServer(TestServer &rhs):
_server(rhs._server){
    rhs._server = nullptr;
}

TestServer &TestServer::operator=(TestServer &rhs){
    if (this == &rhs)
        return *this;
    delete _server;
    _server = rhs._server;
    rhs._server = nullptr;
    return *this;
}

void TestServer::run(){
    Server &server = *_server;
    std::thread([& server](){ server.run(); }).detach();
    WAIT_FOREVER_UNTIL (_server->_running);
}

void TestServer::stop(){
    _server->_loop = false;
    WAIT_FOREVER_UNTIL (!_server->_running);
}

void TestServer::addObject(const std::string &typeName, const Point &loc){
    const ObjectType *const type = _server->findObjectTypeByName(typeName);
    _server->addObject(type, loc);
}

void TestServer::addNPC(const std::string &typeName, const Point &loc){
    const ObjectType *const type = _server->findObjectTypeByName(typeName);
    assert(type->classTag() == 'n');
    const NPCType *const npcType = dynamic_cast<const NPCType *const>(type);
    _server->addNPC(npcType, loc);
}
