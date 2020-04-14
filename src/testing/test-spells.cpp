#include "RemoteClient.h"
#include "TestClient.h"
#include "TestServer.h"
#include "testing.h"

TEST_CASE("Non-talent spells") {
  GIVEN("a spell") {
    auto data = R"(
      <spell id="fireball" >
        <targets enemy=1 />
      </spell>
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);

    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    CHECK_FALSE(user.getClass().knowsSpell("fireball"));

    WHEN("a player learns it") {
      user.getClass().teachSpell("fireball");

      THEN("he knows it") {
        CHECK(user.getClass().knowsSpell("fireball"));
        WAIT_UNTIL(c.knowsSpell("fireball"));

        AND_THEN("he doesn't know some other spell") {
          CHECK_FALSE(user.getClass().knowsSpell("iceball"));
          CHECK_FALSE(c.knowsSpell("iceball"));
        }
      }
    }
  }
}

TEST_CASE("Non-talent spells are persistent") {
  // Given a spell
  auto data = R"(
    <spell id="fireball" >
      <targets enemy=1 />
    </spell>
  )";
  {
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithUsernameAndDataString("alice", data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    // When Alice learns it
    user.getClass().teachSpell("fireball");

    // And the server restarts
  }
  {
    auto s = TestServer::WithDataStringAndKeepingOldData(data);
    auto c = TestClient::WithUsernameAndDataString("alice", data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    // Then she still knows it
    WAIT_UNTIL(user.getClass().knowsSpell("fireball"));

    // And she knows it
    WAIT_UNTIL(c.knowsSpell("fireball"));
  }
}

TEST_CASE("Spell cooldowns", "[remote]") {
  GIVEN(
      "Alice and Bob know a range of self-damaging spells, and extremely high "
      "hit chance") {
    auto data = R"(
      <spell id="hurtSelf1s" cooldown="1" >
        <targets self=1 />
        <function name="doDirectDamage" i1="5" />
      </spell>
      <spell id="hurtSelfAlso1s" cooldown="1" >
        <targets self=1 />
        <function name="doDirectDamage" i1="5" />
      </spell>
      <spell id="hurtSelf2s" cooldown="2" >
        <targets self=1 />
        <function name="doDirectDamage" i1="5" />
      </spell>
      <spell id="hurtSelf" >
        <targets self=1 />
        <function name="doDirectDamage" i1="5" />
      </spell>
    )";
    auto s = TestServer::WithDataString(data);

    auto oldStats = User::OBJECT_TYPE.baseStats();
    auto highHitChance = oldStats;
    highHitChance.hit = 100;
    User::OBJECT_TYPE.baseStats(highHitChance);

    auto cAlice = TestClient::WithUsername("Alice");
    s.waitForUsers(1);
    auto &alice = s.getFirstUser();

    alice.getClass().teachSpell("hurtSelf");
    alice.getClass().teachSpell("hurtSelf1s");
    alice.getClass().teachSpell("hurtSelfAlso1s");
    alice.getClass().teachSpell("hurtSelf2s");

    WHEN("Alice casts the spell with cooldown 1s") {
      cAlice.sendMessage(CL_CAST, "hurtSelf1s");
      REPEAT_FOR_MS(100);
      auto healthAfterFirstCast = alice.health();

      AND_WHEN("she tries casting it again") {
        cAlice.sendMessage(CL_CAST, "hurtSelf1s");

        THEN("she hasn't lost any health") {
          REPEAT_FOR_MS(100);
          CHECK(alice.health() >= healthAfterFirstCast);
        }
      }

      AND_WHEN("she tries a different spell with cooldown 1s") {
        cAlice.sendMessage(CL_CAST, "hurtSelfAlso1s");

        THEN("she has lost health") {
          REPEAT_FOR_MS(100);
          CHECK(alice.health() < healthAfterFirstCast);
        }
      }

      AND_WHEN("1s elapses") {
        REPEAT_FOR_MS(1000);

        AND_WHEN("she tries casting it again") {
          cAlice.sendMessage(CL_CAST, "hurtSelf1s");

          THEN("she has lost health") {
            REPEAT_FOR_MS(100);
            CHECK(alice.health() < healthAfterFirstCast);
          }
        }
      }

      AND_WHEN("she tries casting the spell with no cooldown") {
        cAlice.sendMessage(CL_CAST, "hurtSelf");

        THEN("she has lost health") {
          REPEAT_FOR_MS(100);
          CHECK(alice.health() < healthAfterFirstCast);
        }
      }

      AND_WHEN("Bob tries casting it") {
        auto cBob = RemoteClient{"-username Bob"};
        s.waitForUsers(2);
        auto &bob = *s->getUserByName("Bob");
        bob.getClass().teachSpell("hurtSelf1s");

        auto healthBefore = bob.health();
        s.sendMessage(bob.socket(),
                      {TST_SEND_THIS_BACK, makeArgs(CL_CAST, "hurtSelf1s")});

        THEN("he has lost health") {
          REPEAT_FOR_MS(100);
          CHECK(bob.health() < healthBefore);
        }
      }
    }

    WHEN("Alice casts the spell with cooldown 2s") {
      cAlice.sendMessage(CL_CAST, "hurtSelf2s");
      REPEAT_FOR_MS(100);
      auto healthAfterFirstCast = alice.health();

      AND_WHEN("1s elapses") {
        REPEAT_FOR_MS(1000);

        AND_WHEN("she tries casting it again") {
          cAlice.sendMessage(CL_CAST, "hurtSelf2s");

          THEN("she hasn't lost any health") {
            REPEAT_FOR_MS(100);
            CHECK(alice.health() >= healthAfterFirstCast);
          }
        }
      }
    }
    User::OBJECT_TYPE.baseStats(oldStats);
  }

  SECTION("Persistence") {
    // Given Alice knows a spell with a 2s cooldown"
    auto data = R"(
      <spell id="nop" cooldown="2" >
        <targets self=1 />
        <function name="doDirectDamage" i1="0" />
      </spell>
    )";
    auto s = TestServer::WithDataString(data);

    {
      auto c = TestClient::WithUsernameAndDataString("Alice", data);
      s.waitForUsers(1);
      auto &alice = s.getFirstUser();
      alice.getClass().teachSpell("nop");

      // When she casts it
      c.sendMessage(CL_CAST, "nop");
      WAIT_UNTIL(alice.isSpellCoolingDown("nop"));

      // And when she logs out and back in
    }
    {
      auto c = TestClient::WithUsernameAndDataString("Alice", data);
      s.waitForUsers(1);
      auto &alice = s.getFirstUser();

      // Then the spell is still cooling down
      CHECK(alice.isSpellCoolingDown("nop"));

      // And when she logs out and back in after 2s
    }
    {
      REPEAT_FOR_MS(2000);
      auto c = TestClient::WithUsernameAndDataString("Alice", data);
      s.waitForUsers(1);
      auto &alice = s.getFirstUser();

      // Then the spell has finished cooling down
      CHECK_FALSE(alice.isSpellCoolingDown("nop"));
    }
  }
}

TEST_CASE("NPC spells") {
  GIVEN(
      "A Wizard NPC with no combat damage and a fireball spell (with a 2s "
      "cooldown)") {
    auto data = R"(
      <spell id="fireball" range=30 cooldown=2 >
        <targets enemy=1 />
        <function name="doDirectDamage" i1=5 />
      </spell>
      <npcType id="wizard">
        <spell id="fireball" />
      </npcType>
    )";
    auto s = TestServer::WithDataString(data);
    s.addNPC("wizard", {10, 15});

    WHEN("a player is nearby") {
      auto c = TestClient::WithDataString(data);
      s.waitForUsers(1);
      auto &user = s.getFirstUser();

      THEN("he gets damaged") {
        WAIT_UNTIL(user.health() < user.stats().maxHealth);

        AND_WHEN("1 more second elapses") {
          auto oldHealth = user.health();
          auto oldLocation = user.location();
          REPEAT_FOR_MS(1000);

          THEN("his health is unchanged") {
            // Proxies for the user not dying.  Before implementation, the user
            // died and respawned many times.
            // If the effects on dying are changed, perhaps this can be
            // improved or simplified.
            auto expectedHealthAfter1s =
                oldHealth + User::OBJECT_TYPE.baseStats().hps;
            CHECK(user.health() == expectedHealthAfter1s);
            CHECK(user.location() == oldLocation);
          }
        }
      }
    }
  }

  GIVEN("a sorcerer with a buff, and an apprentice with no spells") {
    auto data = R"(
      <spell id="buffMagic" >
          <targets self=1 />
          <function name="buff" s1="magic" />
      </spell>
      <buff id="magic" />
      <npcType id="sorcerer">
        <spell id="buffMagic" />
      </npcType>
      <npcType id="apprentice" />
    )";
    auto s = TestServer::WithDataString(data);
    s.addNPC("apprentice", {10, 15});

    WHEN("a user appears near the apprentice") {
      auto c = TestClient::WithDataString(data);
      s.waitForUsers(1);

      THEN("the apprentice has no buffs") {
        const auto &apprentice = s.getFirstNPC();
        REPEAT_FOR_MS(100);
        CHECK(apprentice.buffs().empty());
      }
    }
  }
}

TEST_CASE("Kills with spells give XP") {
  GIVEN("a high-damage spell and low-health NPC") {
    auto data = R"(
      <spell id="nuke" range=30 >
        <targets enemy=1 />
        <function name="doDirectDamage" i1=99999 />
      </spell>
      <npcType id="critter" maxHealth=1 />
    )";
    auto s = TestServer::WithDataString(data);
    s.addNPC("critter", {10, 15});

    WHEN("a user knows the spell") {
      auto c = TestClient::WithDataString(data);
      s.waitForUsers(1);
      auto &user = s.getFirstUser();
      user.getClass().teachSpell("nuke");

      AND_WHEN("he kills the NPC with the spell") {
        const auto &critter = s.getFirstNPC();
        c.sendMessage(CL_SELECT_ENTITY, makeArgs(critter.serial()));
        c.sendMessage(CL_CAST, "nuke");
        WAIT_UNTIL(critter.isDead());

        THEN("the user has some XP") { WAIT_UNTIL(user.xp() > 0); }
      }
    }
  }
}

TEST_CASE("Relearning a talent skill after death") {
  GIVEN("a talent that teaches a spell") {
    auto data = R"(
      <class name="Dancer">
          <tree name="Dancing">
              <tier>
                  <requires />
                  <talent type="spell" id="dance" />
              </tier>
          </tree>
      </class>
      <spell id="dance" name="Dance" >
        <targets self="1" />
        <function name="doDirectDamage" s1="0" />
      </spell>
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    WHEN("a level-2 user takes the talent") {
      user.levelUp();
      c.sendMessage(CL_TAKE_TALENT, "Dance");
      WAIT_UNTIL(user.getClass().knowsSpell("dance"));

      AND_WHEN("he dies") {
        user.kill();

        AND_WHEN("he tries to take the talent again") {
          c.sendMessage(CL_TAKE_TALENT, "Dance");

          THEN("he has the talent") {
            WAIT_UNTIL(user.getClass().knowsSpell("dance"));
          }
        }
      }
    }
  }
}

TEST_CASE("Non-damaging spells aggro NPCs") {
  GIVEN("a debuff spell, and an NPC out of range") {
    auto data = R"(
      <spell id="exhaust" range="100" >
        <targets enemy="1" />
        <function name="debuff" s1="exhausted" />
      </spell>
      <buff id="exhausted" duration="1000" >
        <stats energy="-1" />
      </buff>
      <npcType id="monster" maxHealth="100" />
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();

    s.addNPC("monster", {200, 200});

    WHEN("a user knows the spell") {
      user.getClass().teachSpell("exhaust");

      AND_WHEN("he casts it on the NPC") {
        const auto &monster = s.getFirstNPC();
        c.sendMessage(CL_TARGET_ENTITY, makeArgs(monster.serial()));
        c.sendMessage(CL_CAST, "exhaust");
        WAIT_UNTIL(monster.debuffs().size() == 1);

        THEN("it is aware of him") { WAIT_UNTIL(monster.isAwareOf(user)); }
      }
    }
  }
}

TEST_CASE("Cast-from-item returning an item") {
  GIVEN("items that can pour water; the bucket is returned afterwards") {
    auto data = R"(
      <spell id="pourWater"  >
        <targets self="1" />
        <function name="doDirectDamage" s1="0" />
      </spell>
      <item id="bucketOfWater" castsSpellOnUse="pourWater" returnsOnCast="emptyBucket" />
      <item id="glassOfWater" castsSpellOnUse="pourWater" returnsOnCast="emptyGlass" />
      <item id="magicWaterBubble" castsSpellOnUse="pourWater" />
      <item id="emptyBucket" />
      <item id="emptyGlass" />
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();
    const auto &invSlot = user.inventory(0).first;

    AND_GIVEN("a user has a bucket") {
      auto &bucketOfWater = s.findItem("bucketOfWater");
      user.giveItem(&bucketOfWater);

      WHEN("he pours out the water") {
        c.sendMessage(CL_CAST_ITEM, "0");

        THEN("he has an empty bucket") {
          auto &emptyBucket = s.findItem("emptyBucket");
          WAIT_UNTIL(invSlot.hasItem() && invSlot.type() == &emptyBucket);
        }
      }
    }

    AND_GIVEN("a user has a magic water bubble") {
      auto &magicWaterBubble = s.findItem("magicWaterBubble");
      user.giveItem(&magicWaterBubble);

      WHEN("he pours out the water") {
        c.sendMessage(CL_CAST_ITEM, "0");

        THEN("his inventory is empty") {
          REPEAT_FOR_MS(100);
          CHECK_FALSE(invSlot.hasItem());
        }
      }
    }

    AND_GIVEN("a user has a glass of water") {
      auto &glassOfWater = s.findItem("glassOfWater");
      user.giveItem(&glassOfWater);

      WHEN("he pours out the water") {
        c.sendMessage(CL_CAST_ITEM, "0");

        THEN("he has an empty glass") {
          auto &emptyGlass = s.findItem("emptyGlass");
          WAIT_UNTIL(invSlot.hasItem() && invSlot.type() == &emptyGlass);
        }
      }
    }
  }
}

TEST_CASE("Cast a spell from a stackable item") {
  GIVEN("stackable matches cast self-fireballs and return used matches") {
    auto data = R"(
      <spell id="fireball"  >
        <targets self="1" />
        <function name="doDirectDamage" i1="10" />
      </spell>
      <item id="match" stackSize="10" castsSpellOnUse="fireball" returnsOnCast="usedMatch" />
      <item id="usedMatch" />
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();
    const auto &match = s.findItem("match");

    AND_GIVEN("a user has three matches") {
      user.giveItem(&match, 3);

      WHEN("he casts the spell") {
        c.sendMessage(CL_CAST_ITEM, "0");

        THEN("he has two matches") {
          const auto &invSlot = user.inventory(0);
          WAIT_UNTIL(invSlot.second == 2);
        }
      }
    }

    AND_GIVEN("a user has no room for used matches") {
      user.giveItem(&match, 10 * User::INVENTORY_SIZE);

      WHEN("he casts the spell") {
        c.sendMessage(CL_CAST_ITEM, "0");

        THEN("he is still at full health") {
          REPEAT_FOR_MS(100);
          CHECK(user.health() == user.stats().maxHealth);
        }
      }
    }
  }
}

TEST_CASE("Target self if target is invalid") {
  GIVEN("a friendly-fire fireball spell and an enemy") {
    auto data = R"(
      <spell id="fireball"  >
        <targets friendly="1" self="1" />
        <function name="doDirectDamage" i1="10" />
      </spell>
      <npcType id="distraction" />
    )";
    auto s = TestServer::WithDataString(data);
    auto c = TestClient::WithDataString(data);
    s.waitForUsers(1);
    auto &user = s.getFirstUser();
    user.getClass().teachSpell("fireball");
    const auto &distraction = s.addNPC("distraction", {50, 50});

    WHEN("a user targets the object") {
      c.sendMessage(CL_TARGET_ENTITY, makeArgs(distraction.serial()));

      AND_WHEN("he tries to cast the spell") {
        c.sendMessage(CL_CAST, "fireball");

        THEN("he himself has lost health") {
          WAIT_UNTIL(user.health() < user.stats().maxHealth);
        }
      }
    }
  }
}

TEST_CASE("Objects can't be healed") {
  GIVEN("an object, and a heal-all spell") {
    auto data = R"(
      <spell id="megaHeal" range="100" >
        <targets friendly="1" self="1" enemy="1" />
        <function name="heal" i1="99999" />
      </spell>
      <item id="sprocket" durability="10" />
      <objectType id="machine">
        <durability item="sprocket" quantity="10"/>
      </objectType>
    )";
    auto s = TestServer::WithDataString(data);
    auto &machine = s.addObject("machine", {10, 15});

    AND_GIVEN("a user knows the spell") {
      auto c = TestClient::WithDataString(data);
      s.waitForUsers(1);
      auto &user = s.getFirstUser();
      user.getClass().teachSpell("megaHeal");

      AND_GIVEN("the object is damaged") {
        machine.reduceHealth(1);

        WHEN("the user tries to heal the object") {
          c.sendMessage(CL_SELECT_ENTITY, makeArgs(machine.serial()));
          c.sendMessage(CL_CAST, "megaHeal");

          THEN("it is not at full health") {
            REPEAT_FOR_MS(100);
            CHECK(machine.health() < machine.stats().maxHealth);
          }
        }
      }
    }
  }
}
