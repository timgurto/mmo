#include "RemoteClient.h"
#include "TestClient.h"
#include "TestServer.h"
#include "testing.h"

TEST_CASE("Basic declaration of war", "[war]"){
    // Given Alice is logged in
    TestServer s;
    TestClient alice = TestClient::WithUsername("alice");
    WAIT_UNTIL(s.users().size() == 1);

    // When Alice sends a CL_DECLARE_WAR message
    alice.sendMessage(CL_DECLARE_WAR, "bob");

    // Then Alice and Bob go to war
    WAIT_UNTIL(s.wars().isAtWar("alice", "bob"));
}

TEST_CASE("No erroneous wars", "[war]"){
    // When a clean server is started
    TestServer s;
    
    // Then Alice and Bob are not at war
    CHECK_FALSE(s.wars().isAtWar("alice", "bob"));
}

TEST_CASE("Wars are persistent", "[war]"){
    // Given Alice and Bob are at war, and there is no server running
    {
        TestServer server1;
        server1.wars().declare("alice", "bob");
    }

    // When a server begins that keeps persistent data
    TestServer server2 = TestServer::KeepingOldData();

    // Then Alice and Bob are still at war
    CHECK(server2.wars().isAtWar("alice", "bob"));
}

TEST_CASE("Clients are alerted of new wars", "[war]"){
    // Given Alice is logged in
    TestServer s;
    TestClient alice = TestClient::WithUsername("alice");
    WAIT_UNTIL(s.users().size() == 1);

    // When Alice declares war on Bob
    alice.sendMessage(CL_DECLARE_WAR, "bob");

    // Then Alice is alerted to the new war
    WAIT_UNTIL(alice->isAtWarWith("bob"));
}

TEST_CASE("Clients are told of existing wars on login", "[war]"){
    // Given Alice and Bob are at war
    TestServer s;
    s.wars().declare("alice", "bob");

    // When Alice logs in
    TestClient alice = TestClient::WithUsername("alice");
    WAIT_UNTIL(s.users().size() == 1);

    // Then she is told about the war
    WAIT_UNTIL(alice->isAtWarWith("bob"));
}

TEST_CASE("Wars cannot be redeclared", "[war]"){
    // Given Alice and Bob are at war, and Alice is logged in
    TestServer s;
    TestClient alice = TestClient::WithUsername("alice");
    s.wars().declare("alice", "bob");
    WAIT_UNTIL(s.users().size() == 1);

    // When Alice declares war on Bob
    alice.sendMessage(CL_DECLARE_WAR, "bob");

    // Then she receives an SV_ALREADY_AT_WAR error message
    CHECK(alice.waitForMessage(SV_ALREADY_AT_WAR));
}

TEST_CASE("A player can be at war with a city", "[war][city]"){
    // Given a running server;
    TestServer s;

    // And a city named Athens;
    s.cities().createCity("athens");

    // And a user named Alice
    TestClient c = TestClient::WithUsername("alice");
    WAIT_UNTIL(s.users().size() == 1);

    // When a war is declared between Alice and Athens
    s.wars().declare("alice", "athens");

    // Then they are considered to be at war.
    CHECK(s.wars().isAtWar("alice", "athens"));
}

TEST_CASE("A player at war with a city is at war with its members", "[war][city][remote]"){
    // Given a running server;
    TestServer s;

    // And a city named Athens;
    s.cities().createCity("athens");

    // And a user, Alice, who is a member of Athens;
    RemoteClient alice("-username alice");
    WAIT_UNTIL(s.users().size() == 1);
    s.cities().addPlayerToCity(s.getFirstUser(), "athens");

    // And a user, Bob
    TestClient bob = TestClient::WithUsername("bob");

    // When Bob and Athens go to war
    Wars::Belligerent
        b1("bob", Wars::Belligerent::PLAYER),
        b2("athens", Wars::Belligerent::CITY);
    s.wars().declare(b1, b2);

    // Then Bob is at war with Alice
    CHECK(s.wars().isAtWar("alice", "bob"));
}
