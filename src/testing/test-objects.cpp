#include <cassert>

#include "Test.h"
#include "TestClient.h"
#include "TestServer.h"

TEST("Thin objects block movement")
    // Given a server and client;
    TestServer s = TestServer::WithData("thin_wall");
    TestClient c = TestClient::WithData("thin_wall");

    // And a user;
    WAIT_UNTIL(s.users().size() == 1);
    User &user = s.getFirstUser();
    user.updateLocation(Point(10, 15));

    // And a wall just above him
    s.addObject("wall", Point(10, 10));

    // When the user tries to move up, through the wall
    REPEAT_FOR_MS(500) {
        c.sendMessage(CL_LOCATION, makeArgs(10, 5));

        // He fails
        if (user.location().y < 5.5)
            return false;
    }
    return true;
TEND

TEST("Dead objects don't block movement")
    // Given a server and client;
    TestServer s = TestServer::WithData("thin_wall");
    TestClient c = TestClient::WithData("thin_wall");

    // And a user;
    WAIT_UNTIL(s.users().size() == 1);
    User &user = s.getFirstUser();
    user.updateLocation(Point(10, 15));

    // And a wall just above him;
    s.addObject("wall", Point(10, 10));

    // And that wall is dead
    s.getFirstObject().reduceHealth(1000000);

    // When the user tries to move up, through the wall
    REPEAT_FOR_MS(3000) {
        c.sendMessage(CL_LOCATION, makeArgs(10, 5));

        // He succeeds
        if (user.location().y < 5.5)
            return true;
    }
    return false;
TEND

TEST("Damaged objects can't be deconstructed")
    // Given a server and client;
    // And a 'brick' object type with 2 health;
    TestServer s = TestServer::WithData("pickup_bricks");
    TestClient c = TestClient::WithData("pickup_bricks");

    // And a brick object exists with only 1 health
    s.addObject("brick", Point(10, 15));
    Object &brick = s.getFirstObject();
    brick.reduceHealth(1);
    assert(brick.health() == 1);

    // When the user tries to deconstruct the brick
    c.sendMessage(CL_DECONSTRUCT, makeArgs(brick.serial()));

    // Then the object still exists
    REPEAT_FOR_MS(100)
        if (s.entities().empty())
            return false;

    return true;
TEND
