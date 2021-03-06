#include "../client/ui/TakeContainer.h"
#include "../server/Groups.h"
#include "TestClient.h"
#include "TestFixtures.h"
#include "TestServer.h"
#include "testing.h"

TEST_CASE_METHOD(ServerAndClientWithData,
                 "Client gets loot info and can loot") {
  GIVEN("an NPC that always drops 1 gold") {
    useData(R"(
      <npcType id="goldbug" maxHealth="1" >
        <loot id="gold" />
      </npcType>
      <item id="gold" />
    )");

    auto &goldbug = server->addNPC("goldbug", {10, 15});

    WHEN("a user kills it") {
      WAIT_UNTIL(client->objects().size() == 1);
      client->sendMessage(CL_TARGET_ENTITY, makeArgs(goldbug.serial()));
      WAIT_UNTIL(goldbug.isDead());

      THEN("the user can see one item in its loot window") {
        const auto &cGoldBug = client->getFirstNPC();
        WAIT_UNTIL(cGoldBug.lootContainer() != nullptr);
        WAIT_UNTIL(cGoldBug.lootContainer()->size() == 1);

        AND_THEN("the server survives a loot request") {
          client->sendMessage(CL_TAKE_ITEM, makeArgs(goldbug.serial(), 0));

          AND_THEN("the client receives the item") {
            WAIT_UNTIL(client->inventory()[0].first.type() != nullptr);
          }
        }
      }
    }
  }
}

TEST_CASE("Objects have health", "[strength]") {
  GIVEN("a chair type with strength 6*wood, and a wood item with strength 5") {
    auto data = R"(
      <objectType id="chair" name="Chair" >
        <durability item="wood" quantity="6" />
      </objectType>
      <item id="wood" name="Wood" durability="5" />
    )";
    auto s = TestServer::WithDataString(data);

    WHEN("a chair object is created") {
      s.addObject("chair");

      THEN("it has 30 (6*5) health") {
        CHECK(s.getFirstObject().health() == 30);
      }
    }
  }
}

TEST_CASE_METHOD(ServerAndClientWithData, "Clients discern NPCs with no loot") {
  GIVEN("an ant with 1 health and no loot table") {
    useData(R"(
      <npcType id="ant" maxHealth="1" />
    )");

    auto &serverAnt = server->addNPC("ant");
    WAIT_UNTIL(client->objects().size() == 1);

    WHEN("a user kills the ant") {
      serverAnt.onAttackedBy(*user, 1);
      serverAnt.kill();

      THEN("he doesn't believe he can loot it") {
        ClientNPC &clientAnt = client->getFirstNPC();
        REPEAT_FOR_MS(200);
        CHECK_FALSE(clientAnt.lootable());
      }
    }
  }
}

TEST_CASE_METHOD(ServerAndClientWithData,
                 "Chance for strength-items as loot from object",
                 "[strength]") {
  GIVEN("a snowman made of 1000 1-health snowflake items") {
    useData(R"(
      <item id="snowflake" stackSize="1000" durabilty="1" />
      <objectType id="snowman">
        <durability item="snowflake" quantity="1000" />
      </objectType>
    )");

    auto &snowman = server->addObject("snowman", {10, 15});

    WHEN("a user kills the snowman") {
      snowman.onAttackedBy(*user, 1);
      snowman.kill();

      THEN("the client finds out that it's lootable") {
        WAIT_UNTIL(client->objects().size() == 1);
        ClientObject &clientSnowman = client->getFirstObject();
        client->waitForMessage(SV_INVENTORY);

        WAIT_UNTIL(clientSnowman.lootable());

        AND_WHEN("he tries to take the item") {
          WAIT_UNTIL(clientSnowman.container().size() > 0);
          client->sendMessage(CL_TAKE_ITEM, makeArgs(snowman.serial(), 0));

          THEN("he recieves it") {
            WAIT_UNTIL(client->inventory()[0].first.type() != nullptr);
          }
        }
      }
    }
  }
}

TEST_CASE_METHOD(ServerAndClientWithData, "Looting from a container",
                 "[container][only]") {
  GIVEN("a chest that can store 1000 gold") {
    useData(R"(
      <item id="gold" name="Gold" stackSize="100" />
      <objectType id="chest" name="Chest" >
        <container slots="10" />
      </objectType>
    )");

    const auto &gold = server->getFirstItem();
    auto &chest = server->addObject("chest", {10, 15});

    AND_GIVEN("the chest is full of gold") {
      chest.container().addItems(&gold, 1000);

      WHEN("the user destroys the chest") {
        chest.onAttackedBy(*user, 1);
        chest.kill();
        REQUIRE_FALSE(chest.loot().empty());
        REQUIRE(client->waitForMessage(SV_INVENTORY));

        auto &clientChest = client->getFirstObject();
        WAIT_UNTIL(clientChest.lootable());

        AND_WHEN("he loots it") {
          for (size_t i = 0; i != 10; ++i)
            client->sendMessage(CL_TAKE_ITEM, makeArgs(chest.serial(), i));

          THEN("he gets some gold") {
            WAIT_UNTIL(client->inventory()[0].first.type() != nullptr);

            AND_THEN("he doesn't get all of it") {
              WAIT_UNTIL(chest.container().isEmpty());
              ItemSet oneThousandGold;
              oneThousandGold.add(&gold, 1000);
              CHECK_FALSE(server->getFirstUser().hasItems(oneThousandGold));
            }
          }
        }
      }
    }

    WHEN("the user destroys the chest") {
      chest.onAttackedBy(*user, 1);
      chest.kill();

      THEN("there's no loot available") {
        REPEAT_FOR_MS(200);
        client->sendMessage(CL_TAKE_ITEM, makeArgs(chest.serial(), 0));
        REPEAT_FOR_MS(200);
        CHECK(client->inventory()[0].first.type() == nullptr);
      }
    }

    SECTION("An owned container can be looted from") {
      AND_GIVEN("the chest has another owner") {
        chest.permissions.setPlayerOwner("Alice");

        AND_GIVEN("it contains gold") {
          chest.container().addItems(&gold, 100);

          WHEN("the user destroys the chest") {
            chest.onAttackedBy(*user, 1);
            chest.reduceHealth(9999);
            REQUIRE_FALSE(chest.loot().empty());

            AND_WHEN("he loots it") {
              client->waitForMessage(SV_INVENTORY);
              client->sendMessage(CL_TAKE_ITEM, makeArgs(chest.serial(), 0));

              THEN("he gets some gold") {
                WAIT_UNTIL(client->inventory()[0].first.type() != nullptr);
              }
            }
          }
        }
      }
    }
  }
}

TEST_CASE("New users are alerted to lootable objects") {
  // Given an NPC that drops loot
  auto data = R"(
      <npcType id="goldbug" maxHealth="1" >
        <loot id="gold" />
      </npcType>
      <item id="gold" />
    )";
  auto s = TestServer::WithDataString(data);
  auto &goldbug = s.addNPC("goldbug", {10, 15});

  // When Alice kills it
  {
    auto c = TestClient::WithUsernameAndDataString("Alice", data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    goldbug.onAttackedBy(user, 1);
    goldbug.kill();

    // And when Alice logs out and back in
  }
  {
    auto c = TestClient::WithUsernameAndDataString("Alice", data);
    s.waitForUsers(1);

    // Then she knows it's lootable
    CHECK(c.waitForMessage(SV_INVENTORY));
  }
}

TEST_CASE_METHOD(ServerAndClientWithData, "Non-taggers can't loot") {
  GIVEN("an NPC that drops loot") {
    useData(R"(
      <npcType id="goldbug" maxHealth="1" >
        <loot id="gold" />
      </npcType>
      <item id="gold" />
    )");
    auto &goldbug = server->addNPC("goldbug", {10, 15});

    WHEN("it dies without being tagged") {
      goldbug.kill();

      THEN("the user isn't told that it's lootable") {
        CHECK_FALSE(client->waitForMessage(SV_INVENTORY));
      }

      WHEN("the user tries to loot it") {
        client->sendMessage(CL_TAKE_ITEM, makeArgs(goldbug.serial(), 0));

        THEN("his inventory is still empty") {
          REPEAT_FOR_MS(100);
          CHECK_FALSE(user->inventory(0).first.hasItem());
        }
      }
    }
  }
}

TEST_CASE("Loot-table equality") {
  auto rock = ServerItem{"rock"};
  auto stick = ServerItem{"stick"};

  GIVEN("Two loot tables") {
    auto a = LootTable{}, b = LootTable{};
    a == b;

    WHEN("One has a rock") {
      a.addSimpleItem(&rock, 1.0);
      THEN("they are not equal") {
        CHECK(a != b);

        AND_WHEN("the other has a rock") {
          b.addSimpleItem(&rock, 1.0);
          THEN("they are equal") { CHECK(a == b); }
        }

        AND_WHEN("the other has a stick") {
          b.addSimpleItem(&stick, 1.0);
          THEN("they are not equal") { CHECK(a != b); }
        }
      }
    }

    WHEN("they have the same entries in different orders") {
      a.addSimpleItem(&rock, 1.0);
      a.addSimpleItem(&stick, 1.0);

      b.addSimpleItem(&stick, 1.0);
      b.addSimpleItem(&rock, 1.0);

      THEN("they are equal") { CHECK(a == b); }
    }

    WHEN("they differ only by loot chance") {
      a.addSimpleItem(&rock, 1.0);
      b.addSimpleItem(&rock, 0.5);

      THEN("they are not equal") { CHECK(a != b); }
    }

    WHEN("they differ only by distribution") {
      a.addNormalItem(&rock, 1.0);
      b.addNormalItem(&rock, 0.5);

      THEN("they are not equal") { CHECK(a != b); }
    }
  }
}

TEST_CASE("Composite loot tables") {
  GIVEN("two NPCs drop gold: one directly and one with a <lootTable>") {
    auto data = R"(
      <item id="gold" />
      <lootTable id="wealthyMob" >
        <loot id="gold" />
      </lootTable>

      <npcType id="merchant" >
        <loot id="gold" />
      </npcType>
      <npcType id="trader" >
        <lootTable id="wealthyMob" />
      </npcType>
    )";
    auto s = TestServer::WithDataString(data);
    const auto &merchant =
        dynamic_cast<const NPCType &>(*s->findObjectTypeByID("merchant"));
    const auto &trader =
        dynamic_cast<const NPCType &>(*s->findObjectTypeByID("trader"));

    THEN("they have identical loot tables") {
      CHECK(merchant.lootTable() == trader.lootTable());
    }
  }
}

TEST_CASE_METHOD(ServerAndClientWithData,
                 "Clients know when loot is all gone") {
  GIVEN("an NPC that drops an item") {
    useData(R"(
      <item id="wart" />
      <npcType id="frog" >
        <loot id="wart" />
      </npcType>
    )");
    auto &frog = server->addNPC("frog", {10, 15});

    WHEN("a user kills it") {
      frog.onAttackedBy(*user, 1);
      frog.kill();

      THEN("he knows it's lootable") {
        WAIT_UNTIL(client->objects().size() == 1);
        const auto &cFrog = client->getFirstNPC();
        WAIT_UNTIL(cFrog.lootable());

        AND_WHEN("he loots it") {
          client->sendMessage(CL_TAKE_ITEM, makeArgs(frog.serial(), 0));
          const auto &user = server->getFirstUser();
          WAIT_UNTIL(user.inventory(0).first.hasItem());

          THEN("he knows it isn't lootable") { WAIT_UNTIL(!cFrog.lootable()); }
        }
      }
    }
  }
}

TEST_CASE("Grouped players can loot each other's kills") {
  GIVEN("a mouse that always drops a tail") {
    auto data = R"(
      <item id="tail" />
      <npcType id="mouse" >
        <loot id="tail" />
      </npcType>
    )";
    auto s = TestServer::WithDataString(data);
    auto &mouse = s.addNPC("mouse", {30, 30});

    AND_GIVEN("Alice and Bob are in a group") {
      auto cAlice = TestClient::WithUsernameAndDataString("Alice", data);
      auto cBob = TestClient::WithUsernameAndDataString("Bob", data);
      s.waitForUsers(2);
      auto &alice = s.findUser("Alice");
      auto &bob = s.findUser("Bob");

      s->groups->addToGroup("Bob", "Alice");

      WHEN("Alice kills the mouse") {
        mouse.onAttackedBy(alice, 1);
        mouse.kill();

        AND_WHEN("Bob tries to loot it") {
          cBob.sendMessage(CL_TAKE_ITEM, makeArgs(mouse.serial(), 0));

          THEN("Bob has an item") {
            WAIT_UNTIL(bob.inventory(0).first.hasItem());
          }
        }

        THEN("Bob is told that it's lootable") {
          CHECK(cBob.waitForMessage(SV_INVENTORY));
        }
      }
    }
  }
}

TEST_CASE("Loot that chooses from a set") {
  GIVEN("gentlemen drop either an umbrella or a hat") {
    auto data = R"(
      <item id="hat" stackSize="10" />
      <item id="umbrella" stackSize="10" />
      <npcType id="gentleman" >
        <chooseLoot>
          <choice item="hat" />
          <choice item="umbrella" />
        </chooseLoot>
      </npcType>
    )";
    auto s = TestServer::WithDataString(data);
    const auto &lootTable = s.getFirstNPCType().lootTable();

    WHEN("the gentleman loot table is instantiated") {
      auto loot = Loot{};
      lootTable.instantiate(loot, nullptr);

      THEN("one item was yielded") {
        const auto itemStacks = loot.size();
        CHECK(itemStacks == 1);

        const auto qty = loot.at(0).second;
        CHECK(qty == 1);
      }
    }

    WHEN("the gentleman loot table is instantiated 100 times") {
      auto hatAdded = false;
      auto umbrellaAdded = false;
      for (auto i = 0; i != 100; ++i) {
        auto loot = Loot{};
        lootTable.instantiate(loot, nullptr);
        auto itemID = loot.at(0).first.type()->id();
        if (itemID == "hat")
          hatAdded = true;
        else if (itemID == "umbrella")
          umbrellaAdded = true;

        if (hatAdded && umbrellaAdded) break;
      }

      THEN("both the hat and the umbrella get added at least once each") {
        CHECK(hatAdded);
        CHECK(umbrellaAdded);
      }
    }
  }

  GIVEN("chickens drop eggs") {
    auto data = R"(
      <item id="egg" />
      <npcType id="chicken" >
        <chooseLoot>
          <choice item="egg" />
        </chooseLoot>
      </npcType>
    )";
    auto s = TestServer::WithDataString(data);

    WHEN("the chicken loot table is instantiated") {
      const auto &lootTable = s.getFirstNPCType().lootTable();
      auto loot = Loot{};
      lootTable.instantiate(loot, nullptr);

      THEN("it contains an egg") {
        CHECK(loot.at(0).first.type()->id() == "egg");
      }
    }
  }

  SECTION("quantities >1 are supported") {
    GIVEN("skeletons drop five gold") {
      auto data = R"(
      <item id="gold" stackSize="10" />
      <npcType id="skeleton" >
        <chooseLoot>
          <choice item="gold" qty="5" />
        </chooseLoot>
      </npcType>
    )";
      auto s = TestServer::WithDataString(data);

      WHEN("the skeleton loot table is instantiated") {
        const auto &lootTable = s.getFirstNPCType().lootTable();
        auto loot = Loot{};
        lootTable.instantiate(loot, nullptr);

        THEN("it contains five items") { CHECK(loot.at(0).second == 5); }
      }
    }
  }
}
