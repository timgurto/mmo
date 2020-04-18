#include "RemoteClient.h"
#include "TestClient.h"
#include "TestServer.h"
#include "testing.h"

TEST_CASE("City creation") {
  GIVEN("a server") {
    TestServer s;

    THEN("no cities exist") {
      CHECK_FALSE(s.cities().doesCityExist("Fakeland"));
    }

    WHEN("a city is created") {
      s.cities().createCity("Athens", {});

      THEN("it exists") { CHECK(s.cities().doesCityExist("Athens")); }
    }
  }
}

TEST_CASE("Add a player to a city") {
  GIVEN("a player named Alice") {
    TestServer s;
    TestClient c = TestClient::WithUsername("Alice");
    s.waitForUsers(1);
    User &alice = s.getFirstUser();

    AND_GIVEN("a city called Athens") {
      s.cities().createCity("Athens", {});

      THEN("Alice is not in Athens") {
        CHECK_FALSE(s.cities().isPlayerInCity("Alice", "Athens"));
      }

      WHEN("Alice is added to Athens") {
        s.cities().addPlayerToCity(alice, "Athens");

        THEN("Alice is in Athens") {
          CHECK(s.cities().isPlayerInCity("Alice", "Athens"));
        }
      }
    }
  }
}

TEST_CASE("Cities can't be overwritten") {
  GIVEN("a player named Alice") {
    TestServer s;
    TestClient c = TestClient::WithUsername("Alice");
    s.waitForUsers(1);
    User &alice = s.getFirstUser();

    THEN("he knows he is not in a city") {
      CHECK(c->character().cityName() == "");
    }

    AND_GIVEN("she is a citizen of Athens") {
      s.cities().createCity("Athens", {});
      s.cities().addPlayerToCity(alice, "Athens");

      WHEN("another city called Athens is created") {
        s.cities().createCity("Athens", {});

        THEN("Alice is still in Athens") {
          CHECK(s.cities().isPlayerInCity("Alice", "Athens"));
        }
      }
    }
  }
}

TEST_CASE("Client is alerted to city membership") {
  GIVEN("a user named Alice") {
    TestServer s;
    TestClient c = TestClient::WithUsername("Alice");
    s.waitForUsers(1);
    User &alice = s.getFirstUser();

    AND_GIVEN("a city named Athens") {
      s.cities().createCity("Athens", {});

      WHEN("Alice joins Athens") {
        s.cities().addPlayerToCity(alice, "Athens");

        THEN("she receives a message to that effect") {
          bool messageReceived = c.waitForMessage(SV_JOINED_CITY);
          REQUIRE(messageReceived);

          AND_THEN("she knows she is in Athens") {
            WAIT_UNTIL(c->character().cityName() == "Athens");
          }
        }
      }
    }
  }
}

TEST_CASE("Cities are persistent", "[persistence]") {
  // Given Alice is in Athens
  {
    TestServer server1;
    TestClient client = TestClient::WithUsername("Alice");
    server1.waitForUsers(1);
    User &alice = server1.getFirstUser();

    server1.cities().createCity("Athens", {});
    server1.cities().addPlayerToCity(alice, "Athens");

    // When the server restarts
  }
  TestServer server2 = TestServer::KeepingOldData();

  // Then Alice is still in Athens
  CHECK(server2.cities().doesCityExist("Athens"));
  CHECK(server2.cities().isPlayerInCity("Alice", "Athens"));
}

TEST_CASE("Clients are told if in a city on login") {
  TestServer server;
  server.cities().createCity("Athens", {});
  {
    TestClient client1 = TestClient::WithUsername("Alice");
    server.waitForUsers(1);
    User &alice = server.getFirstUser();
    server.cities().addPlayerToCity(alice, "Athens");
  }

  TestClient client2 = TestClient::WithUsername("Alice");
  WAIT_UNTIL(client2->character().cityName() == "Athens");
}

TEST_CASE("Clients know nearby players' cities", "[remote]") {
  GIVEN("Alice is in Athens") {
    TestServer s;
    s.cities().createCity("Athens", {});
    RemoteClient rc("-username Alice");
    s.waitForUsers(1);
    User &serverAlice = s.getFirstUser();
    s.cities().addPlayerToCity(serverAlice, "Athens");

    WHEN("another client connects") {
      TestClient c;
      WAIT_UNTIL(c.otherUsers().size() == 1);

      THEN("he knows that Alice is in Athens") {
        const Avatar &clientAlice = c.getFirstOtherUser();
        WAIT_UNTIL(clientAlice.cityName() == "Athens");
      }
    }
  }
}

TEST_CASE("Ceding") {
  GIVEN("rock objects and a user") {
    auto data = R"(
      <objectType id="rock" />
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);
    s.waitForUsers(1);
    User &user = s.getFirstUser();

    AND_GIVEN("a rock owned by nobody") {
      const auto &rock = s.addObject("rock", {10, 15});

      WHEN("the user tries to cede it") {
        c.sendMessage(CL_CEDE, makeArgs(rock.serial()));

        THEN("he receives an error message") {
          CHECK(c.waitForMessage(WARNING_NO_PERMISSION, 10000));

          AND_THEN("the rock does not belong to Athens") {
            CHECK_FALSE(rock.permissions.isOwnedByCity("Athens"));
          }
        }
      }
    }

    AND_GIVEN("a rock owned by the user") {
      const auto &rock = s.addObject("rock", {10, 15}, user.name());

      WHEN("he tries to cede it") {
        c.sendMessage(CL_CEDE, makeArgs(rock.serial()));

        THEN("he receives an error message") {
          REQUIRE(c.waitForMessage(ERROR_NOT_IN_CITY));

          AND_THEN("it still belongs to him") {
            CHECK(rock.permissions.isOwnedByPlayer(user.name()));
          }
        }
      }

      AND_GIVEN("he is in Athens") {
        s.cities().createCity("Athens", {});
        s.cities().addPlayerToCity(user, "Athens");

        WHEN("he tries to cede it") {
          WAIT_UNTIL(c.objects().size() == 1);
          Object &rock = s.getFirstObject();
          c.sendMessage(CL_CEDE, makeArgs(rock.serial()));

          THEN("it belongs to Athens") {
            WAIT_UNTIL(rock.permissions.isOwnedByCity("Athens"));

            AND_THEN("it doesn't belong to him") {
              CHECK_FALSE(rock.permissions.isOwnedByPlayer(user.name()));
            }
          }
        }
      }
    }
  }
}

TEST_CASE("A player can leave a city", "[.flaky]") {
  // Given a user named Alice;
  auto c = TestClient::WithUsername("Alice");
  auto s = TestServer{};
  s.waitForUsers(1);
  auto &user = s.getFirstUser();

  // Who is a member of Athens;
  s.cities().createCity("Athens", {});
  s.cities().addPlayerToCity(user, "Athens");
  WAIT_UNTIL(s.cities().isPlayerInCity("Alice", "Athens"));

  // And who knows it
  WAIT_UNTIL(c.cityName() == "Athens");

  SECTION("When Alice sends a leave-city message") {
    c.sendMessage(CL_LEAVE_CITY);
  }
  SECTION("When Alice enters \"/cquit\" into the chat") {
    c.performCommand("/cquit");
  }

  // Then Alice is not in a city;
  WAIT_UNTIL(!s.cities().isPlayerInCity("Alice", "Athens"));
  WAIT_UNTIL(s.cities().getPlayerCity("Alice").empty());

  // And the user knows it
  WAIT_UNTIL(c.cityName().empty());
}

TEST_CASE("A player can't leave a city if not in one") {
  GIVEN("a user") {
    auto c = TestClient{};
    auto s = TestServer{};
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    WHEN("he sends a leave-city message") {
      c.sendMessage(CL_LEAVE_CITY);

      THEN("he receives an error message") {
        CHECK(c.waitForMessage(ERROR_NOT_IN_CITY));
      }
    }
  }
}

TEST_CASE("A king can't leave his city", "[king]") {
  GIVEN("a user named Alice") {
    auto c = TestClient::WithUsername("Alice");
    auto s = TestServer{};
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    AND_GIVEN("she is a member of Athens") {
      s.cities().createCity("Athens", {});
      s.cities().addPlayerToCity(user, "Athens");
      WAIT_UNTIL(s.cities().isPlayerInCity("Alice", "Athens"));

      AND_GIVEN("she is a king") {
        s->makePlayerAKing(user);

        WHEN("Alice sends a leave-city message") {
          c.sendMessage(CL_LEAVE_CITY);

          THEN("Alice receives an error message") {
            CHECK(c.waitForMessage(ERROR_KING_CANNOT_LEAVE_CITY));

            AND_THEN("Alice is still in Athens") {
              REPEAT_FOR_MS(100);
              CHECK(s.cities().isPlayerInCity("Alice", "Athens"));
            }
          }
        }
      }
    }
  }
}

TEST_CASE("Kingship is persistent", "[king]") {
  // Given a user named Alice;
  {
    auto c = TestClient::WithUsername("Alice");
    auto s = TestServer{};
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    // Who is a king
    s->makePlayerAKing(user);

    // When the server restarts
  }
  {
    auto c = TestClient::WithUsername("Alice");
    auto s = TestServer::KeepingOldData();
    s.waitForUsers(1);

    // Then Alice is still a king
    WAIT_UNTIL(s.kings().isPlayerAKing("Alice"));
  }
}

TEST_CASE("Citizens know about their city's distant objects", "[flaky]") {
  GIVEN("a map big enough to exceed the cull distance") {
    auto data = R"(
      <terrain index="G" id="grass" />
      <list id="default" default="1" >
          <allow id="grass" />
      </list>
      <newPlayerSpawn x="10" y="10" range="0" />
      <size x="40" y="1" />
      <row    y="0" terrain = "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG" />
      <objectType id="townHall" />
    )";
    auto s = TestServer::WithDataString(data);

    AND_GIVEN("a city has a citizen and a distant object") {
      s.cities().createCity("Athens", {10, 10});
      s.addObject("townHall", {1200, 10}, {Permissions::Owner::CITY, "Athens"});

      {
        auto c = TestClient::WithUsernameAndDataString("Alice", data);
        s.waitForUsers(1);
        auto &user = s.getFirstUser();
        s.cities().addPlayerToCity(user, "Athens");
      }

      WHEN("the citizen logs in") {
        auto c = TestClient::WithUsernameAndDataString("Alice", data);

        THEN("the client knows about the object") {
          WAIT_UNTIL(c.objects().size() == 1);
        }
      }
    }
  }
}

TEST_CASE("New users know about cities") {
  auto s = TestServer{};

  GIVEN("a city called Athens") {
    s.cities().createCity("Athens", {10, 10});

    WHEN("a user logs in") {
      auto c = TestClient{};

      THEN("he knows about it") { WAIT_UNTIL(c.cities().count() == 1); }
    }
  }

  GIVEN("a city called Persia") {
    s.cities().createCity("Persia", {10, 10});

    WHEN("a user logs in") {
      auto c = TestClient{};

      THEN("he knows about it") { WAIT_UNTIL(c.cities().count() == 1); }
    }
  }

  GIVEN("two cities") {
    s.cities().createCity("Athens", {10, 10});
    s.cities().createCity("Persia", {10, 10});

    WHEN("a user logs in") {
      auto c = TestClient{};

      THEN("he knows there are two cities") {
        WAIT_UNTIL(c.cities().count() == 2);

        AND_THEN("he knows their names") {
          auto athensFound = false, persiaFound = false;
          for (const auto &city : c.cities()) {
            if (city.name == "Athens") athensFound = true;
            if (city.name == "Persia") persiaFound = true;
          }
          CHECK(athensFound);
          CHECK(persiaFound);
        }
      }
    }
  }

  WHEN("a user logs in") {
    auto c = TestClient{};

    THEN("he knows there are no cities") {
      REPEAT_FOR_MS(100);
      CHECK(c.cities().count() == 0);
    }
  }
}
