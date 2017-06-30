#include "RemoteClient.h"
#include "TestClient.h"
#include "TestServer.h"
#include "testing.h"

TEST_CASE("On login, players are told about their distant objects", "[.flaky][culling]"){
    // Given an object at (10000,10000) owned by Alice
    TestServer s = TestServer::WithData("signpost");
    s.addObject("signpost", Point(10000, 10000), "alice");

    // When Alice logs in
    TestClient c = TestClient::WithUsernameAndData("alice", "signpost");
    WAIT_UNTIL_TIMEOUT(s.users().size() == 1, 10000);

    // Alice knows about the object
    REPEAT_FOR_MS(500);
    CHECK(c.objects().size() == 1);
}

TEST_CASE("On login, players are not told about others' distant objects", "[.flaky][culling]"){
    // Given an object at (10000,10000) owned by Alice
    TestServer s = TestServer::WithData("signpost");
    s.addObject("signpost", Point(10000, 10000), "bob");

    // When Alice logs in
    TestClient c = TestClient::WithUsernameAndData("alice", "signpost");
    WAIT_UNTIL_TIMEOUT(s.users().size() == 1, 10000);

    // Alice does not know about the object
    REPEAT_FOR_MS(500);
    CHECK(c.objects().empty());
}

TEST_CASE("When a player moves away from his object, he is still aware of it", "[.slow][culling]"){
    // Given a server with signpost objects;
    TestServer s = TestServer::WithData("signpost");

    // And a signpost near the user spawn point that belongs to Alice;
    s.addObject("signpost", Point(10, 15), "alice");

    // And Alice is logged in
    TestClient c = TestClient::WithUsernameAndData("alice", "signpost");
    WAIT_UNTIL(s.users().size() == 1);
    WAIT_UNTIL(c.objects().size() == 1);

    // When Alice moves out of range of the signpost
    while (c->character().location().x < 1000){
        c.sendMessage(CL_LOCATION, makeArgs(1010, 10));

        // Then she is still aware of it
        if (c.objects().size() == 0)
            break;
        SDL_Delay(5);
    }
    CHECK(c.objects().size() == 1);
}

TEST_CASE("When a player moves away from his city's object, he is still aware of it",
          "[.slow][culling]"){
    // Given a server with signpost objects;
    TestServer s = TestServer::WithData("signpost");

    // And a city named Athens
    s.cities().createCity("athens");

    // And a signpost near the user spawn point that belongs to Athens;
    s.addObject("signpost", Point(10, 15));
    Object &signpost = s.getFirstObject();
    signpost.permissions().setCityOwner("athens");

    // And Alice is logged in
    TestClient c = TestClient::WithUsernameAndData("alice", "signpost");

    // And Alice is a member of Athens
    WAIT_UNTIL(s.users().size() == 1);
    User &user = s.getFirstUser();
    s.cities().addPlayerToCity(user, "athens");

    // When Alice moves out of range of the signpost
    WAIT_UNTIL(c.objects().size() == 1);
    while (c->character().location().x < 1000){
        c.sendMessage(CL_LOCATION, makeArgs(1010, 10));

        // Then she is still aware of it
        if (c.objects().size() == 0)
            break;
        SDL_Delay(5);
    }
    CHECK(c.objects().size() == 1);
}
