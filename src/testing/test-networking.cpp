#include <cstdio>

#include "../Socket.h"
#include "../curlUtil.h"
#include "../server/ProgressLock.h"
#include "TestClient.h"
#include "TestServer.h"
#include "testing.h"

TEST_CASE("Read invalid URL", "[.slow]") {
  CHECK(readFromURL("fake.fake.fake").empty());
}

TEST_CASE("Read badly-formed URL", "[.slow]") {
  CHECK(readFromURL("1").empty());
}

TEST_CASE("Read blank URL") { CHECK(readFromURL("").empty()); }

TEST_CASE("Read test URL", "[.slow]") {
  CHECK(readFromURL("timgurto.com/test.txt") ==
        "This is a file for testing web-access accuracy.");
}

TEST_CASE("Use socket after cleanup") {
  {
    TestServer s;
    TestClient c;

    WAIT_UNTIL(c.connected());

    // Winsock should be cleaned up here.
  }
  {
    TestServer s;
    TestClient c;

    WAIT_UNTIL(c.connected());
  }
}

TEST_CASE("Download file") {
  downloadFile("timgurto.com/test.txt", "test.txt");

  {
    auto f = std::ifstream("test.txt");
    REQUIRE(f.good());

    auto word = std::string{};
    f >> word;
    CHECK(word == "This");
  }

  remove("test.txt");
}

TEST_CASE("Bulk messages") {
  const auto NUM_RECIPES = 100;

  GIVEN(NUM_RECIPES << " recipes") {
    auto data = std::ostringstream{};
    data << "<item id=\"A\" />";

    for (auto i = 0; i != NUM_RECIPES; ++i) {
      data << "<recipe id=\"longishRecipeID" << i << "\" product=\"A\" >";
      data << "<unlockedBy gather=\"A\"/></recipe>";
    }

    auto s = TestServer::WithDataString(data.str());

    AND_GIVEN("Alice unlocks them all, then logs in") {
      {
        auto c = TestClient::WithUsernameAndDataString("Alice", data.str());
        s.waitForUsers(1);
        auto &user = s.getFirstUser();
        ProgressLock::unlockAll(user);
      }
      auto c = TestClient::WithUsernameAndDataString("Alice", data.str());

      THEN("the client knows " << NUM_RECIPES << " recipes") {
        REPEAT_FOR_MS(100);
        WAIT_UNTIL(c.knownRecipes().size() == NUM_RECIPES);
      }
    }
  }
}
