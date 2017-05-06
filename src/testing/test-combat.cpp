#include "RemoteClient.h"
#include "Test.h"
#include "TestServer.h"
#include "TestClient.h"
#include "../client/ClientNPC.h"

TEST("Players can attack immediately")
    TestServer s = TestServer::WithData("ant");
    TestClient c = TestClient::WithData("ant");

    //Move user to middle
    WAIT_UNTIL (s.users().size() == 1);
    User &user = s.getFirstUser();

    // Add NPC
    s.addNPC("ant", Point(10, 10));
    WAIT_UNTIL (c.objects().size() == 1);

    // Start attacking
    const auto objects = c.objects();
    const ClientObject *objP = objects.begin()->second;
    const ClientNPC &ant = dynamic_cast<const ClientNPC &>(*objP);
    size_t serial = ant.serial();
    c.sendMessage(CL_TARGET_NPC, makeArgs(serial));
    
    // NPC should be damaged very quickly
    REPEAT_FOR_MS(200)
        if (ant.health() < ant.npcType()->maxHealth())
            return true;
    return false;
TEND

TEST("Belligerents can target each other")
    TestServer s;
    TestClient alice = TestClient::WithUsername("alice");
    RemoteClient bob("-username bob");
    WAIT_UNTIL(s.users().size() == 2);
    User
        &uAlice = s.findUser("alice"),
        &uBob = s.findUser("bob");

    alice.sendMessage(CL_DECLARE_WAR, "bob");
    alice.sendMessage(CL_TARGET_PLAYER, "bob");
    WAIT_UNTIL(uAlice.target() == &uBob);

    return true;
TEND

TEST("Peaceful players can't target each other")
    TestServer s;
    TestClient alice = TestClient::WithUsername("alice");
    RemoteClient bob("-username bob");
    WAIT_UNTIL(s.users().size() == 2);
    User
        &uAlice = s.findUser("alice"),
        &uBob = s.findUser("bob");

    alice.sendMessage(CL_TARGET_PLAYER, "bob");
    REPEAT_FOR_MS(500) {
        if (uAlice.target() == &uBob)
            return false;
    }

    return true;
TEND

TEST("Belliegerents can fight")
    TestServer s;
    TestClient alice = TestClient::WithUsername("alicex");
    RemoteClient bob("-username bobx");
    WAIT_UNTIL(s.users().size() == 2);

    alice.sendMessage(CL_DECLARE_WAR, "bobx");

    User
        &uAlice = s.findUser("alicex"),
        &uBob = s.findUser("bobx");
    while (distance(uAlice.location(), uBob.location()) > Server::ACTION_DISTANCE)
        uAlice.updateLocation(uBob.location());

    WAIT_UNTIL(alice->isAtWarWith("bobx"));

    alice.sendMessage(CL_TARGET_PLAYER, "bobx");
    WAIT_UNTIL(uBob.health() < uBob.maxHealth());

    return true;
TEND

TEST("Peaceful players can't fight")
    TestServer s;
    TestClient alice = TestClient::WithUsername("alice");
    RemoteClient bob("-username bob");
    WAIT_UNTIL(s.users().size() == 2);

    User
        &uAlice = s.findUser("alice"),
        &uBob = s.findUser("bob");
    while (distance(uAlice.location(), uBob.location()) > Server::ACTION_DISTANCE)
        uAlice.updateLocation(uBob.location());

    alice.sendMessage(CL_TARGET_PLAYER, "bob");
    REPEAT_FOR_MS(500) {
        if (uBob.health() < uBob.maxHealth())
            return false;
    }

    return true;
TEND
