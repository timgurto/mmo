#ifndef TEST_CLIENT_H
#define TEST_CLIENT_H

#include "../client/CDataLoader.h"
#include "../client/CQuest.h"
#include "../client/Client.h"
#include "testing.h"

class CQuest;

// A wrapper of the client, with full access, used for testing.
class TestClient {
 public:
  TestClient();
  static TestClient WithUsername(const std::string &username);
  static TestClient WithData(const std::string &dataPath);
  static TestClient WithDataString(const std::string &data);
  static TestClient WithUsernameAndData(const std::string &username,
                                        const std::string &dataPath);
  static TestClient WithUsernameAndDataString(const std::string &username,
                                              const std::string &data);
  static TestClient WithClassAndDataString(const std::string &classID,
                                           const std::string &data);
  ~TestClient();

  // Move constructor/assignment
  TestClient(TestClient &rhs);
  TestClient &operator=(TestClient &rhs);

  void loadDataFromString(const std::string &data);

  bool connected() const {
    return _client->_connection.state() == Connection::CONNECTED;
  }
  bool loggedIn() const { return _client->_loggedIn; }
  void freeze();
  static void stopClientIfRunning();

  std::map<size_t, ClientObject *> &objects() { return _client->_objects; }
  Client::objectTypes_t &objectTypes() { return _client->_objectTypes; }
  const CQuests &quests() const { return _client->_quests; }
  const std::map<std::string, ClientItem> &items() const {
    return _client->_items;
  }
  const List &recipeList() const { return *_client->_recipeList; }
  void showCraftingWindow();
  void watchObject(ClientObject &obj);
  bool knowsConstruction(const std::string &id) const {
    return _client->_knownConstructions.find(id) !=
           _client->_knownConstructions.end();
  }
  const ChoiceList &uiBuildList() const { return *_client->_buildList; }
  bool knowsSpell(const std::string &id) const;
  Target target() { return _client->_target; }
  const std::map<std::string, Avatar *> &otherUsers() const {
    return _client->_otherUsers;
  }
  ClientItem::vect_t &inventory() { return _client->_inventory; }
  const std::string &name() const { return _client->username(); }
  const List *chatLog() const { return _client->_chatLog; }
  const Element::children_t &mapPins() const {
    return _client->_mapPins->children();
  }
  const Element::children_t &mapPinOutlines() const {
    return _client->_mapPinOutlines->children();
  }
  const std::vector<std::vector<char> > &map() const { return _client->_map; }
  const std::set<std::string> &allOnlinePlayers() const {
    return _client->_allOnlinePlayers;
  }

  Window *craftingWindow() const { return _client->_craftingWindow; }
  Window *buildWindow() const { return _client->_buildWindow; }
  Window *gearWindow() const { return _client->_gearWindow; }
  Window *mapWindow() const { return _client->_mapWindow; }
  bool isAtWarWith(const Avatar &user) const {
    return _client->isAtWarWith(user);
  }
  const std::string &cityName() const {
    return _client->character().cityName();
  }
  const Connection::State connectionState() const {
    return _client->_connection.state();
  }

  Avatar &getFirstOtherUser();
  ClientNPC &getFirstNPC();
  ClientObject &getFirstObject();
  const ClientObjectType &getFirstObjectType();
  const CQuest &getFirstQuest();

  Client *operator->() { return _client; }
  Client &client() { return *_client; }
  void performCommand(const std::string &command) {
    _client->performCommand(command);
  }
  void sendMessage(MessageCode code, const std::string &args = "") const {
    _client->sendMessage(code, args);
  }
  MessageCode getNextMessage() const;
  bool waitForMessage(MessageCode desiredMsg,
                      ms_t timeout = DEFAULT_TIMEOUT) const;
  void waitForRedraw();
  void simulateClick(const ScreenPoint &position);

 private:
  Client *_client;

  enum StringType { USERNAME, CLASS, DATA_PATH, DATA_STRING };
  using StringMap = std::map<StringType, std::string>;
  TestClient(const StringMap &strings);
  TestClient(const std::string &string, StringType type);
  TestClient(const std::string &username, const std::string &string,
             StringType type);

  void run();
  void stop();
  void loadData(const std::string path) {
    CDataLoader::FromPath(*_client, path).load();
  }

  bool TestClient::messageWasReceivedSince(MessageCode desiredMsg,
                                           size_t startingIndex) const;
};

#endif
