#include <cassert>

#include "../versionUtil.h"
#include "Client.h"
#include "ClientCombatant.h"
#include "ClientNPC.h"
#include "ClientObject.h"
#include "ClientVehicle.h"
#include "Particle.h"
#include "ui/ConfirmationWindow.h"
#include "ui/ContainerGrid.h"

using namespace std::string_literals;

static void readString(std::istream &iss, std::string &str,
                       char delim = MSG_DELIM) {
  if (iss.peek() == delim) {
    str = "";
  } else {
    static const size_t BUFFER_SIZE = 1023;
    static char buffer[BUFFER_SIZE + 1];
    iss.get(buffer, BUFFER_SIZE, delim);
    str = buffer;
  }
}

std::istream &operator>>(std::istream &lhs, std::string &rhs) {
  readString(lhs, rhs, MSG_DELIM);
  return lhs;
}

void Client::handleMessage(const std::string &msg) {
  _partialMessage.append(msg);
  std::istringstream iss(_partialMessage);
  _partialMessage = "";
  int msgCode;
  char del;
  const auto BUFFER_SIZE = 1023;
  static char buffer[BUFFER_SIZE + 1];

  // Read while there are new messages
  while (!iss.eof()) {
    // Discard malformed data
    if (iss.peek() != MSG_START) {
      iss.get(buffer, BUFFER_SIZE, MSG_START);
      /*_debug << "Read " << iss.gcount() << " characters." << Log::endl;
      showErrorMessage("Malformed message; discarded \""s + buffer + "\""s,
      Color::ERR);*/
      if (iss.eof()) {
        break;
      }
    }

    // Get next message
    iss.get(buffer, BUFFER_SIZE, MSG_END);
    if (iss.eof()) {
      _partialMessage = buffer;
      break;
    } else {
      std::streamsize charsRead = iss.gcount();
      buffer[charsRead] = MSG_END;
      buffer[charsRead + 1] = '\0';
      iss.ignore();  // Throw away ']'
    }
    std::istringstream singleMsg(buffer);
    //_debug(buffer, Color::TODO);
    singleMsg >> del >> msgCode >> del;

    _messagesReceivedMutex.lock();
    _messagesReceived.push_back(MessageCode(msgCode));
    _messagesReceivedMutex.unlock();

    Color errorMessageColor = Color::CHAT_ERROR;

    switch (msgCode) {
      case SV_WELCOME: {
        if (del != MSG_END) break;

        // Hide/clean up from login screen
        SDL_StopTextInput();
        _instance->_createWindow->hide();

        _character.name(_username);

        _connection.state(Connection::LOGGED_IN);
        _loggedIn = true;
        _lastPingSent = _lastPingReply = _time;
#ifdef _DEBUG
        _debug("Successfully logged in to server", Color::CHAT_SUCCESS);
#endif
        _debug("Welcome to Hellas!");
        _allOnlinePlayers.insert(_username);
        populateOnlinePlayersList();
        break;
      }

      case SV_PING_REPLY: {
        ms_t timeSent;
        singleMsg >> timeSent >> del;
        if (del != MSG_END) break;
        _lastPingReply = _time;
        _latency = (_time - timeSent) / 2;
        break;
      }

      case SV_USER_CONNECTED: {
        std::string name;
        readString(singleMsg, name, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        _allOnlinePlayers.insert(name);
        populateOnlinePlayersList();
        break;
      }

      case SV_USERS_ALREADY_ONLINE: {
        int n;
        singleMsg >> n >> del;
        for (size_t i = 0; i != n; ++i) {
          std::string name;
          readString(singleMsg, name, i == n - 1 ? MSG_END : MSG_DELIM);
          singleMsg >> del;
          _allOnlinePlayers.insert(name);
        }
        populateOnlinePlayersList();
        break;
      }

      case SV_USER_DISCONNECTED:
      case SV_USER_OUT_OF_RANGE: {
        std::string name;
        readString(singleMsg, name, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        const std::map<std::string, Avatar *>::iterator it =
            _otherUsers.find(name);
        if (it != _otherUsers.end()) {
          // Remove as driver from vehicle
          if (it->second->isDriving())
            for (auto &objPair : _objects)
              if (objPair.second->classTag() == 'v') {
                ClientVehicle &v =
                    dynamic_cast<ClientVehicle &>(*objPair.second);
                if (v.driver() == it->second) {
                  v.driver(nullptr);
                  break;
                }
              }

          removeEntity(it->second);
          _otherUsers.erase(it);
        }
        if (msgCode == SV_USER_DISCONNECTED) {
          _debug << name << " disconnected." << Log::endl;
          _allOnlinePlayers.erase(name);
          populateOnlinePlayersList();
        }
        break;
      }

      case WARNING_WRONG_VERSION: {
        auto serverVersion = ""s;
        readString(singleMsg, serverVersion, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        infoWindow("Version mismatch. Server: v"s + serverVersion +
                   "; client: v"s + version());

        break;
      }

      case WARNING_DUPLICATE_USERNAME:
        if (del != MSG_END) break;
        infoWindow("The user "s + _username +
                   " is already connected to the server."s);
        break;

      case WARNING_INVALID_USERNAME:
        if (del != MSG_END) break;
        infoWindow("The username "s + _username + " is invalid."s);
        break;

      case WARNING_SERVER_FULL:
        if (del != MSG_END) break;
        _loggedIn = false;
        infoWindow("The server is full; attempting reconnection.");
        break;

      case WARNING_USER_DOESNT_EXIST:
      case WARNING_NAME_TAKEN:
        if (del != MSG_END) break;
        infoWindow(_errorMessages[msgCode]);
        break;

      case WARNING_TOO_FAR:
      case WARNING_DOESNT_EXIST:
      case WARNING_NEED_MATERIALS:
      case WARNING_NEED_TOOLS:
      case WARNING_ACTION_INTERRUPTED:
      case WARNING_BLOCKED:
      case WARNING_INVENTORY_FULL:
      case WARNING_NO_PERMISSION:
      case WARNING_NO_WARE:
      case WARNING_NO_PRICE:
      case WARNING_MERCHANT_INVENTORY_FULL:
      case WARNING_NOT_EMPTY:
      case WARNING_VEHICLE_OCCUPIED:
      case WARNING_NO_VEHICLE:
      case WARNING_WRONG_MATERIAL:
      case WARNING_UNIQUE_OBJECT:
      case WARNING_INVALID_SPELL_TARGET:
      case WARNING_NOT_ENOUGH_ENERGY:
      case WARNING_NO_TALENT_POINTS:
      case WARNING_MISSING_ITEMS_FOR_TALENT:
      case WARNING_MISSING_REQ_FOR_TALENT:
      case WARNING_STUNNED:
      case WARNING_YOU_ARE_ALREADY_IN_CITY:
      case WARNING_ITEM_NEEDED:
        errorMessageColor = Color::CHAT_ERROR;  // Yellow above, red below
      case ERROR_INVALID_USER:
      case ERROR_INVALID_ITEM:
      case ERROR_CANNOT_CRAFT:
      case ERROR_INVALID_SLOT:
      case ERROR_EMPTY_SLOT:
      case ERROR_CANNOT_CONSTRUCT:
      case ERROR_NOT_MERCHANT:
      case ERROR_INVALID_MERCHANT_SLOT:
      case ERROR_TARGET_DEAD:
      case ERROR_NPC_SWAP:
      case ERROR_TAKE_SELF:
      case ERROR_NOT_GEAR:
      case ERROR_NOT_VEHICLE:
      case ERROR_UNKNOWN_RECIPE:
      case ERROR_UNKNOWN_CONSTRUCTION:
      case ERROR_UNDER_CONSTRUCTION:
      case ERROR_ATTACKED_PEACFUL_PLAYER:
      case ERROR_INVALID_OBJECT:
      case ERROR_ALREADY_AT_WAR:
      case ERROR_NOT_IN_CITY:
      case ERROR_NO_INVENTORY:
      case ERROR_CANNOT_CEDE:
      case ERROR_NO_ACTION:
      case ERROR_KING_CANNOT_LEAVE_CITY:
      case ERROR_ALREADY_IN_CITY:
      case ERROR_NOT_A_KING:
      case ERROR_INVALID_TALENT:
      case ERROR_ALREADY_KNOW_SPELL:
      case ERROR_DONT_KNOW_SPELL:
        if (del != MSG_END) break;
        showErrorMessage(_errorMessages[msgCode], errorMessageColor);
        startAction(0);
        break;

      case WARNING_ITEM_TAG_NEEDED: {
        std::string reqItemTag;
        readString(singleMsg, reqItemTag, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        reqItemTag = tagName(reqItemTag);
        std::string msg = "You need a";
        const char first = reqItemTag.front();
        auto vowels = std::string{"AaEeIiOoUu"};
        if (vowels.find(first) != vowels.npos) msg += 'n';
        showErrorMessage(msg + ' ' + reqItemTag + " tool to do that.",
                         Color::CHAT_WARNING);
        startAction(0);
        break;
      }

      case WARNING_OUT_OF_AMMO: {
        auto ammoID = ""s;
        readString(singleMsg, ammoID, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        auto it = _items.find(ammoID);
        if (it == _items.end()) {
          showErrorMessage("Received warning about invalid item: "s + ammoID,
                           Color::CHAT_ERROR);
          break;
        }
        auto ammoName = it->second.name();

        std::string msg = "Attacking with that weapon requires a";
        const char first = ammoName.front();
        auto vowels = std::string{"AaEeIiOoUu"};
        if (vowels.find(first) != vowels.npos) msg += 'n';
        showErrorMessage(msg + " " + ammoName + ".", Color::CHAT_WARNING);
        startAction(0);
        break;
      }

      case WARNING_PLAYER_UNIQUE_OBJECT: {
        std::string category;
        readString(singleMsg, category, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        showErrorMessage("You may own only one " + category + " object",
                         Color::CHAT_WARNING);
        startAction(0);
        break;
      }

      case SV_TIME_PLAYED: {
        int secondsPlayed;
        singleMsg >> secondsPlayed >> del;
        if (del != MSG_END) break;
        _debug("Time played: "s + sAsTimeDisplay(secondsPlayed));
        break;
      }

      case SV_ACTION_STARTED:
        ms_t time;
        singleMsg >> time >> del;
        if (del != MSG_END) break;
        startAction(time);

        // If constructing, hide footprint now that it has successfully started.
        if (_selectedConstruction != nullptr && !_multiBuild) {
          _buildList->clearSelection();
          _constructionFootprint = Texture();
          _selectedConstruction = nullptr;
        } else if (ContainerGrid::getUseItem() != nullptr) {
          ContainerGrid::clearUseItem();
          _constructionFootprint = Texture();
        }

        break;

      case SV_ACTION_FINISHED:
        if (del != MSG_END) break;
        startAction(0);  // Effectively, hide the cast bar.
        break;

      case SV_LOCATION:  // Also the de-facto new-user announcement
      case SV_LOCATION_INSTANT_USER: {
        std::string name;
        double x, y;
        singleMsg >> name >> del >> x >> del >> y >> del;
        if (del != MSG_END) break;
        Avatar *newUser = nullptr;
        const MapPoint p(x, y);
        auto isSelf = name == _username;
        if (isSelf) {
          if (msgCode == SV_LOCATION_INSTANT_USER) {
            _pendingCharLoc = p;
            setEntityLocation(&_character, p);
            _serverHasOutOfDateLocationInfo = false;
          }
          if (p.x == _character.location().x) _pendingCharLoc.x = p.x;
          if (p.y == _character.location().y) _pendingCharLoc.y = p.y;
          _character.destination(p);
          if (!_loaded) {
            setEntityLocation(&_character, p);
            _pendingCharLoc = p;
          }
          updateOffset();
          _mapWindow->markChanged();
          _connection.state(Connection::LOADED);
          _loaded = true;
          _tooltipNeedsRefresh = true;
          _mouseMoved = true;
        } else {
          if (_otherUsers.find(name) == _otherUsers.end()) addUser(name, p);

          _otherUsers[name]->destination(p);
          if (msgCode == SV_LOCATION_INSTANT_USER)
            setEntityLocation(_otherUsers[name], p);
        }

        // Unwatch objects if out of range
        for (auto it = _objectsWatched.begin(); it != _objectsWatched.end();) {
          ClientObject &obj = **it;
          ++it;
          if (distance(playerCollisionRect(), obj.collisionRect()) >
              ACTION_DISTANCE) {
            obj.hideWindow();

            // Hide quest windows from this object
            // Note: this doesn't check that the object itself is the source of
            // the quest.  A more correct solution would make sure that there
            // are no watched objects of the same type.
            for (auto *questFromThisObject : obj.startsQuests()) {
              auto *questWindow = questFromThisObject->window();
              if (questWindow) const_cast<Window *>(questWindow)->hide();
            }
            for (auto *questFromThisObject : obj.completableQuests()) {
              auto *questWindow = questFromThisObject->window();
              if (questWindow) const_cast<Window *>(questWindow)->hide();
            }

            unwatchObject(obj);
          }
        }

        bool shouldTryToCullObjects = name == _username;
        if (shouldTryToCullObjects) {
          std::list<std::pair<size_t, Sprite *> > objectsToRemove;
          for (auto pair : _objects) {
            if (pair.second->canAlwaysSee()) continue;
            if (outsideCullRange(pair.second->location(),
                                 CULL_HYSTERESIS_DISTANCE))
              objectsToRemove.push_back(pair);
          }
          for (auto pair : objectsToRemove) {
            if (pair.second == _currentMouseOverEntity)
              _currentMouseOverEntity = nullptr;
            if (pair.second == targetAsEntity()) clearTarget();
            removeEntity(pair.second);
            _objects.erase(_objects.find(pair.first));
          }
        }
        if (name == _username) {  // We moved; look at everyone else
          std::list<Avatar *> usersToRemove;
          for (auto pair : _otherUsers)
            if (outsideCullRange(pair.second->location(),
                                 CULL_HYSTERESIS_DISTANCE)) {
              _debug("Removing other user");
              usersToRemove.push_back(pair.second);
            }
          for (Avatar *avatar : usersToRemove) {
            if (_currentMouseOverEntity == avatar)
              _currentMouseOverEntity = nullptr;
            _otherUsers.erase(_otherUsers.find(avatar->name()));
            removeEntity(avatar);
          }
        }

        _mapWindow->markChanged();

        break;
      }

      case SV_SET_SPAWN: {
        if (del != MSG_END) break;

        auto message =
            "By the grace of Hermes, you will return to this point should you "
            "ever "
            "fall in battle."s;
        toast("light"s, message);
        _debug(message);
        break;
      }

      case SV_CLASS: {
        auto username = ""s, classID = ""s;
        auto level = Level{};
        singleMsg >> username >> del >> classID >> del >> level >> del;
        if (del != MSG_END) break;

        if (username == _username) {
          _character.setClass(classID);
          _character.level(level);
          populateClassWindow();

        } else {
          auto it = _otherUsers.find(username);
          if (it == _otherUsers.end()) {
            // showErrorMessage("Class received for an unknown user. Ignoring.",
            // Color::TODO);
            break;
          }
          auto &otherUser = *it->second;
          otherUser.setClass(classID);
          otherUser.level(level);
        }
        break;
      }

      case SV_GEAR: {
        std::string username, id;
        size_t slot;
        singleMsg >> username >> del >> slot >> del;
        readString(singleMsg, id, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        if (username == _username) {
          // showErrorMessage("Own gear info received by wrong channel.
          // Ignoring.", Color::TODO);
          break;
        }
        if (_otherUsers.find(username) == _otherUsers.end()) {
          // showErrorMessage("Gear received for an unknown user.  Ignoring.",
          // Color::TODO);
          break;
        }

        // Handle empty id (item was unequipped)
        if (id == "") {
          _otherUsers[username]->gear()[slot].first = nullptr;
          break;
        }

        const auto it = _items.find(id);
        if (it == _items.end()) {
          showErrorMessage("Unknown gear received ("s + id + ").  Ignoring.",
                           Color::CHAT_ERROR);
          break;
        }
        const ClientItem &item = it->second;
        if (item.gearSlot() >= GEAR_SLOTS) {
          showErrorMessage("Gear info received for a non-gear item.  Ignoring.",
                           Color::CHAT_ERROR);
          break;
        }
        _otherUsers[username]->gear()[slot].first = &item;
        break;
      }

      case SV_INVENTORY: {
        size_t serial, slot, quantity;
        std::string itemID;
        singleMsg >> serial >> del >> slot >> del >> itemID >> del >>
            quantity >> del;
        if (del != MSG_END) break;

        handle_SV_INVENTORY(serial, slot, itemID, quantity);
        break;
      }

      case SV_RECEIVED_ITEM: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto id = std::string{buffer};
        auto qty = 0;
        singleMsg >> del >> qty >> del;

        if (del != MSG_END) return;

        auto item = _items.find(id);
        if (item == _items.end()) break;

        addFloatingCombatText("+"s + toString(qty) + " "s + item->second.name(),
                              _character.location(), Color::ITEM_NAME);

        auto logMessage = "Received "s;
        if (qty > 1) logMessage += toString(qty) + "x "s;
        logMessage += item->second.name();
        _debug(logMessage);

        break;
      }

      case SV_JOINED_CITY: {
        std::string cityName;
        readString(singleMsg, cityName, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        _character.cityName(cityName);

        auto message = "You have joined the city of "s + cityName + "."s;
        toast("column", message);
        _debug(message);

        break;
      }

      case SV_NO_CITY: {
        std::string username;
        readString(singleMsg, username, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        handle_SV_NO_CITY(username);
        break;
      }

      case SV_IN_CITY: {
        std::string username, cityName;
        readString(singleMsg, username);
        singleMsg >> del;
        readString(singleMsg, cityName, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        handle_SV_IN_CITY(username, cityName);
        break;
      }

      case SV_CITY_FOUNDED: {
        std::string founder, city;
        readString(singleMsg, founder);
        singleMsg >> del;
        readString(singleMsg, city, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        auto message =
            "The city of "s + city + " has been founded by "s + founder + "."s;
        if (founder == _username)
          message = "By the grace of Athena, you have founded the city of "s +
                    city + "."s;
        toast("column"s, message);
        _debug(message);
        break;
      }

      case SV_CITY_DESTROYED: {
        std::string city;
        readString(singleMsg, city, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        auto message = "The city of "s + city + " has been destroyed."s;
        toast("fireball"s, message);
        _debug(message);
        break;
      }

      case SV_KING: {
        std::string username;
        readString(singleMsg, username, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        handle_SV_KING(username);
        break;
      }

      case SV_OBJECT: {
        int serial;
        double x, y;
        std::string type;
        singleMsg >> serial >> del >> x >> del >> y >> del;
        readString(singleMsg, type, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        auto cot = findObjectType(type);
        if (!cot) {
          showErrorMessage("Received object of invalid type; ignored.",
                           Color::CHAT_ERROR);
          break;
        }

        std::map<size_t, ClientObject *>::iterator it = _objects.find(serial);
        if (it != _objects.end()) {
          // Existing object: update its info.
          ClientObject &obj = *it->second;
          obj.destination({x, y});
          obj.type(cot);
          if (targetAsEntity() == &obj) _target.onTypeChange();
          // Redraw window
          obj.assembleWindow(*this);
          obj.refreshTooltip();

        } else {
          // A new object was added; add entity to list
          ClientObject *obj;
          switch (cot->classTag()) {
            case 'n': {
              const ClientNPCType *npcType =
                  static_cast<const ClientNPCType *>(cot);
              obj = new ClientNPC(serial, npcType, {x, y});
              break;
            }
            case 'v': {
              const ClientVehicleType *vehicleType =
                  static_cast<const ClientVehicleType *>(cot);
              obj = new ClientVehicle(serial, vehicleType, {x, y});
              break;
            }
            case 'o':
            default:
              obj = new ClientObject(serial, cot, {x, y});
          }
          _entities.insert(obj);
          _objects[serial] = obj;
        }

        _mapWindow->markChanged();
        break;
      }

      case SV_LOCATION_INSTANT_OBJECT:
      case SV_OBJECT_LOCATION: {
        int serial;
        double x, y;
        singleMsg >> serial >> del >> x >> del >> y >> del;
        if (del != MSG_END) break;
        std::map<size_t, ClientObject *>::iterator it = _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Server removed an object we didn't know about.",
          // Color::TODO);
          break;  // We didn't know about this object
        }
        it->second->destination({x, y});
        // if (msgCode == SV_LOCATION_INSTANT_OBJECT) it->second->location({x,
        // y});
        break;
      }

      case SV_REMOVE_OBJECT:
      case SV_OBJECT_OUT_OF_RANGE: {
        int serial;
        singleMsg >> serial >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::const_iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Server removed an object we didn't know about.",
          // Color::TODO);
          break;  // We didn't know about this object
        }
        if (it->second == _currentMouseOverEntity)
          _currentMouseOverEntity = nullptr;
        if (it->second == targetAsEntity()) clearTarget();
        removeEntity(it->second);
        _objects.erase(it);
        break;
      }

      case SV_NPC_LEVEL: {
        auto serial = size_t{};
        auto level = Level{};
        singleMsg >> serial >> del >> level >> del;
        if (del != MSG_END) break;

        handle_SV_NPC_LEVEL(serial, level);

        break;
      }

      case SV_OWNER: {
        int serial;
        std::string type, name;
        singleMsg >> serial >> del;
        readString(singleMsg, type);
        singleMsg >> del;
        readString(singleMsg, name, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received ownership info for an unknown object.",
          // Color::TODO);
          break;
        }
        ClientObject &obj = *it->second;
        obj.owner(name);
        obj.assembleWindow(*this);
        obj.refreshTooltip();
        break;
      }

      case SV_GATHERING_OBJECT: {
        int serial;
        singleMsg >> serial >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received info about an unknown object.",
          // Color::TODO);
          break;
        }
        (it->second)->beingGathered(true);
        break;
      }

      case SV_NOT_GATHERING_OBJECT: {
        int serial;
        singleMsg >> serial >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received info about an unknown object.",
          // Color::TODO);
          break;
        }
        (it->second)->beingGathered(false);
        break;
      }

      case SV_RECIPES:
      case SV_NEW_RECIPES: {
        int n;
        singleMsg >> n >> del;
        for (size_t i = 0; i != n; ++i) {
          std::string recipe;
          readString(singleMsg, recipe, i == n - 1 ? MSG_END : MSG_DELIM);
          singleMsg >> del;
          _knownRecipes.insert(recipe);

          auto it = _recipes.find(recipe);
          if (it == _recipes.end()) continue;

          if (msgCode == SV_NEW_RECIPES) {
            auto message =
                "You have learned how to craft a new recipe: " + it->name();
            toast("hammer", message);
          }
        }
        if (msgCode == SV_NEW_RECIPES) {
          _debug << "You have discovered ";
          if (n == 1)
            _debug << "a new recipe";
          else
            _debug << n << " new recipes";
          _debug << "!" << Log::endl;
        }
        if (_recipeList != nullptr) {
          _recipeList->markChanged();
          populateFilters();
        }
        break;
      }

      case SV_CONSTRUCTIONS:
      case SV_NEW_CONSTRUCTIONS: {
        if (msgCode == SV_CONSTRUCTIONS) _knownConstructions.clear();

        int n;
        singleMsg >> n >> del;
        for (size_t i = 0; i != n; ++i) {
          std::string recipe;
          readString(singleMsg, recipe, i == n - 1 ? MSG_END : MSG_DELIM);
          singleMsg >> del;
          _knownConstructions.insert(recipe);

          auto cot = findObjectType(recipe);
          if (!cot) continue;
          if (msgCode == SV_NEW_CONSTRUCTIONS) {
            auto message = "You have learned how to construct a new object: " +
                           cot->name();
            toast("lumber", message);
          }
        }
        if (msgCode == SV_NEW_CONSTRUCTIONS) {
          _debug << "You have discovered ";
          if (n == 1)
            _debug << "a new construction";
          else
            _debug << n << " new constructions";
          _debug << "!" << Log::endl;
        }
        populateBuildList();
        break;
      }

      case SV_ENTITY_HEALTH: {
        size_t serial;
        Hitpoints health;
        singleMsg >> serial >> del >> health >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received health info for an unknown object.",
          // Color::TODO);
          break;
        }
        ClientObject &obj = *it->second;
        obj.health(health);
        if (health == 0) {
          obj.refreshTooltip();
          obj.assembleWindow(*this);
          obj.constructionMaterials({});
        }
        if (targetAsEntity() == &obj) {
          _target.updateHealth(health);
          if (health == 0) _target.makePassive();
        }
        obj.refreshTooltip();
        break;
      }

      case SV_PLAYER_HIT_ENTITY: {
        std::string username;
        int serial;
        readString(singleMsg, username, MSG_DELIM);
        singleMsg >> del >> serial >> del;
        if (del != MSG_END) break;
        const Avatar *attacker = nullptr;
        if (username == _username)
          attacker = &character();
        else {
          auto userIt = _otherUsers.find(username);
          if (userIt == _otherUsers.end()) {
            // showErrorMessage("Received combat info for an unknown player.",
            // Color::TODO);
            break;
          }
          attacker = userIt->second;
        }
        attacker->playAttackSound();

        handle_SV_ENTITY_WAS_HIT(serial);
        break;
      }

      case SV_ENTITY_HIT_PLAYER: {
        int serial;
        std::string username;
        singleMsg >> serial >> del;
        readString(singleMsg, username, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        auto objIt = _objects.find(serial);
        if (objIt == _objects.end()) {
          // showErrorMessage("Received combat info for an unknown object.",
          // Color::TODO);
          break;
        }
        const ClientNPC &attacker =
            *dynamic_cast<const ClientNPC *>(objIt->second);
        attacker.playAttackSound();

        handle_SV_PLAYER_WAS_HIT(username);
        break;
      }

      case SV_PLAYER_HIT_PLAYER: {
        std::string attackerName, defenderName;
        readString(singleMsg, attackerName);
        singleMsg >> del;
        readString(singleMsg, defenderName, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        const Avatar *attacker = nullptr;
        if (attackerName == _username)
          attacker = &character();
        else {
          auto userIt = _otherUsers.find(attackerName);
          if (userIt == _otherUsers.end()) {
            // showErrorMessage("Received combat info for an unknown attacking
            // player.", Color::TODO);
            break;
          }
          attacker = userIt->second;
        }

        attacker->playAttackSound();

        handle_SV_PLAYER_WAS_HIT(defenderName);
        break;
      }

      case SV_LOOTABLE: {
        size_t serial;
        singleMsg >> serial >> del;
        if (del != MSG_END) break;

        handle_SV_LOOTABLE(serial);
      }

      case SV_NOT_LOOTABLE: {
        size_t serial;
        singleMsg >> serial >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received loot info for an unknown object.",
          // Color::TODO);
          break;
        }
        ClientObject &object = *it->second;

        object.lootable(false);
        object.refreshTooltip();
        object.hideWindow();

        break;
      }

      case SV_LOOT_COUNT: {
        size_t serial, quantity;
        singleMsg >> serial >> del >> quantity >> del;
        if (del != MSG_END) break;
        const std::map<size_t, ClientObject *>::iterator it =
            _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received loot info for an unknown object.",
          // Color::TODO);
          break;
        }
        ClientObject &object = *it->second;

        object.container() =
            ClientItem::vect_t(quantity, std::make_pair(nullptr, 0));
        object.refreshTooltip();

        break;
      }

      case SV_CONSTRUCTION_MATERIALS: {
        int serial, n;
        ItemSet set;
        singleMsg >> serial >> del >> n >> del;
        auto it = _objects.find(serial);
        if (it == _objects.end()) {
          // showErrorMessage("Received construction-material info for unknown
          // object");
          break;
        }
        ClientObject &obj = *it->second;
        for (size_t i = 0; i != n; ++i) {
          std::string id;
          readString(singleMsg, id);
          const auto it = _items.find(id);
          if (it == _items.end()) {
            showErrorMessage("Received invalid construction-material info.",
                             Color::CHAT_ERROR);
            break;
          }
          size_t qty;
          singleMsg >> del >> qty >> del;
          const ClientItem *item = &it->second;
          set.add(item, qty);
        }
        obj.constructionMaterials(set);
        obj.assembleWindow(*this);
        obj.refreshTooltip();
        break;
      }

      case SV_PLAYER_HEALTH: {
        std::string username;
        Hitpoints newHealth;
        readString(singleMsg, username, MSG_DELIM);
        singleMsg >> del >> newHealth >> del;
        if (del != MSG_END) break;
        Avatar *target = nullptr;
        if (username == _username)
          target = &_character;
        else {
          auto userIt = _otherUsers.find(username);
          if (userIt == _otherUsers.end()) {
            // showErrorMessage("Received combat info for an unknown defending
            // player.", Color::TODO);
            break;
          }
          target = userIt->second;
        }
        if (newHealth < target->health()) target->createDamageParticles();
        target->health(newHealth);
        if (targetAsEntity() == target) _target.updateHealth(newHealth);
        break;
      }

      case SV_PLAYER_DAMAGED: {
        auto username = ""s;
        auto amount = Hitpoints{};
        readString(singleMsg, username, MSG_DELIM);
        singleMsg >> del >> amount >> del;
        if (del != MSG_END) break;
        handle_SV_PLAYER_DAMAGED(username, amount);
        break;
      }

      case SV_PLAYER_HEALED: {
        auto username = ""s;
        auto amount = Hitpoints{};
        readString(singleMsg, username, MSG_DELIM);
        singleMsg >> del >> amount >> del;
        if (del != MSG_END) break;
        handle_SV_PLAYER_HEALED(username, amount);
        break;
      }

      case SV_OBJECT_DAMAGED: {
        auto serial = 0;
        auto amount = Hitpoints{};
        singleMsg >> serial >> del >> amount >> del;
        if (del != MSG_END) break;
        handle_SV_OBJECT_DAMAGED(serial, amount);
        break;
      }

      case SV_OBJECT_HEALED: {
        auto serial = 0;
        auto amount = Hitpoints{};
        singleMsg >> serial >> del >> amount >> del;
        if (del != MSG_END) break;
        handle_SV_OBJECT_HEALED(serial, amount);
        break;
      }

      case SV_PLAYER_ENERGY: {
        std::string username;
        auto newEnergy = Energy{};
        readString(singleMsg, username, MSG_DELIM);
        singleMsg >> del >> newEnergy >> del;
        if (del != MSG_END) break;
        Avatar *target = nullptr;
        if (username == _username)
          target = &_character;
        else {
          auto userIt = _otherUsers.find(username);
          if (userIt == _otherUsers.end()) {
            // showErrorMessage("Received combat info for an unknown defending
            // player.", Color::TODO);
            break;
          }
          target = userIt->second;
        }
        target->energy(newEnergy);
        if (targetAsEntity() == target) _target.updateEnergy(newEnergy);
        break;
      }

      case SV_MOUNTED: {
        size_t serial;
        std::string user;
        singleMsg >> serial >> del;
        readString(singleMsg, user, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        Avatar *userP = nullptr;
        if (user == _username) {
          userP = &_character;
          /*
          Cancel any requested movement; assumes that this message was preceded
          by SV_LOCATION, sending us on top of the vehicle.
          */
          _pendingCharLoc = _character.destination();
        } else {
          auto it = _otherUsers.find(user);
          if (it == _otherUsers.end())
            ;  // showErrorMessage("Received vehicle info for an unknown user",
               // Color::TODO);
          else
            userP = it->second;
        }
        userP->driving(true);
        auto pairIt = _objects.find(serial);
        if (pairIt == _objects.end())
          ;  // showErrorMessage("Received driver info for an unknown vehicle",
             // Color::TODO);
        else {
          ClientVehicle *v = dynamic_cast<ClientVehicle *>(pairIt->second);
          v->driver(userP);
        }
        break;
      }

      case SV_UNMOUNTED: {
        size_t serial;
        std::string user;
        singleMsg >> serial >> del;
        readString(singleMsg, user, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        Avatar *userP = nullptr;
        if (user == _username) {
          userP = &_character;
          _isDismounting = false;
          /*
          Cancel any requested movement; assumes that this message was preceded
          by SV_LOCATION, sending us to the dismount location.
          */
          _pendingCharLoc = _character.destination();
        } else {
          auto it = _otherUsers.find(user);
          if (it == _otherUsers.end())
            ;  // showErrorMessage("Received vehicle info for an unknown user",
               // Color::TODO);
          else
            userP = it->second;
        }
        userP->driving(false);
        auto pairIt = _objects.find(serial);
        if (pairIt == _objects.end())
          ;  // showErrorMessage("Received driver info for an unknown vehicle",
             // Color::TODO);
        else {
          ClientVehicle *v = dynamic_cast<ClientVehicle *>(pairIt->second);
          v->driver(nullptr);
        }
        break;
      }

      case SV_YOUR_STATS: {
        singleMsg >> _stats.armor >> del >> _stats.maxHealth >> del >>
            _stats.maxEnergy >> del >> _stats.hps >> del >> _stats.eps >> del >>
            _stats.hit >> del >> _stats.crit >> del >> _stats.critResist >>
            del >> _stats.dodge >> del >> _stats.block >> del >>
            _stats.blockValue >> del >> _stats.magicDamage >> del >>
            _stats.physicalDamage >> del >> _stats.healing >> del >>
            _stats.airResist >> del >> _stats.earthResist >> del >>
            _stats.fireResist >> del >> _stats.waterResist >> del >>
            _stats.attackTime >> del >> _stats.speed >> del;
        if (del != MSG_END) break;
        _displaySpeed = Podes::displayFromPixels(_stats.speed);
        _character.maxHealth(_stats.maxHealth);
        _character.maxEnergy(_stats.maxEnergy);
        break;
      }

      case SV_XP: {
        auto xp = XP{}, maxXP = XP{};
        singleMsg >> xp >> del >> maxXP >> del;
        if (del != MSG_END) break;
        _xp = xp;
        _maxXP = maxXP;
        populateClassWindow();
        break;
      }

      case SV_XP_GAIN: {
        auto newXP = XP{};
        singleMsg >> newXP >> del;
        if (del != MSG_END) return;

        addFloatingCombatText("+"s + toString(newXP) + " XP"s,
                              _character.location(), Color::STAT_XP);

        break;
      }

      case SV_LEVEL_UP: {
        auto username = ""s;
        readString(singleMsg, username, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        handle_SV_LEVEL_UP(username);
        break;
      }

      case SV_MAX_HEALTH: {
        auto username = ""s;
        readString(singleMsg, username);
        auto newMaxHealth = Hitpoints{};
        singleMsg >> del >> newMaxHealth >> del;
        if (del != MSG_END) break;
        handle_SV_MAX_HEALTH(username, newMaxHealth);
        break;
      }

      case SV_MAX_ENERGY: {
        auto username = ""s;
        readString(singleMsg, username);
        auto newMaxEnergy = Energy{};
        singleMsg >> del >> newMaxEnergy >> del;
        if (del != MSG_END) break;
        handle_SV_MAX_ENERGY(username, newMaxEnergy);
        break;
      }

      case SV_MERCHANT_SLOT: {
        size_t serial, slot, wareQty, priceQty;
        std::string ware, price;
        singleMsg >> serial >> del >> slot >> del >> ware >> del >> wareQty >>
            del >> price >> del >> priceQty >> del;
        if (del != MSG_END) return;
        auto objIt = _objects.find(serial);
        if (objIt == _objects.end()) {
          // showErrorMessage("Info received about unknown object.",
          // Color::TODO);
          break;
        }
        ClientObject &obj = const_cast<ClientObject &>(*objIt->second);
        size_t slots = obj.objectType()->merchantSlots();
        if (slot >= slots) {
          showErrorMessage("Received invalid merchant slot.",
                           Color::CHAT_ERROR);
          break;
        }
        if (ware.empty() || price.empty()) {
          obj.setMerchantSlot(slot, ClientMerchantSlot());
          break;
        }
        auto wareIt = _items.find(ware);
        if (wareIt == _items.end()) {
          showErrorMessage("Received merchant slot describing invalid item",
                           Color::CHAT_ERROR);
          break;
        }
        const ClientItem *wareItem = &wareIt->second;
        auto priceIt = _items.find(price);
        if (priceIt == _items.end()) {
          showErrorMessage("Received merchant slot describing invalid item",
                           Color::CHAT_ERROR);
          break;
        }
        const ClientItem *priceItem = &priceIt->second;
        obj.setMerchantSlot(
            slot, ClientMerchantSlot(wareItem, wareQty, priceItem, priceQty));
        break;
      }

      case SV_TRANSFORM_TIME: {
        size_t serial, remaining;
        singleMsg >> serial >> del >> remaining >> del;
        if (del != MSG_END) return;
        auto objIt = _objects.find(serial);
        if (objIt == _objects.end()) {
          // showErrorMessage("Info received about unknown object.",
          // Color::TODO);
          break;
        }
        ClientObject &obj = const_cast<ClientObject &>(*objIt->second);
        obj.transformTimer(remaining);
        break;
      }

      case SV_AT_WAR_WITH_PLAYER:
      case SV_AT_WAR_WITH_CITY:
      case SV_YOUR_CITY_AT_WAR_WITH_PLAYER:
      case SV_YOUR_CITY_AT_WAR_WITH_CITY: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto name = std::string{buffer};
        singleMsg >> del;
        if (del != MSG_END) return;

        switch (msgCode) {
          case SV_AT_WAR_WITH_PLAYER:
            _warsAgainstPlayers.add(name);
            break;
          case SV_AT_WAR_WITH_CITY:
            _warsAgainstCities.add(name);
            break;
          case SV_YOUR_CITY_AT_WAR_WITH_PLAYER:
            _cityWarsAgainstPlayers.add(name);
            break;
          case SV_YOUR_CITY_AT_WAR_WITH_CITY:
            _cityWarsAgainstCities.add(name);
            break;
        }

        auto youAre = ""s;
        if (msgCode == SV_AT_WAR_WITH_PLAYER || msgCode == SV_AT_WAR_WITH_CITY)
          youAre = "You are"s;
        else
          youAre = "Your city is";
        auto message = youAre + " now at war with "s + name + "."s;
        _debug(message);
        toast("helmet", message);

        _mapWindow->markChanged();

        populateWarsList();
        break;
      }

      case SV_PEACE_WAS_PROPOSED_TO_YOU_BY_PLAYER:
      case SV_PEACE_WAS_PROPOSED_TO_YOU_BY_CITY:
      case SV_PEACE_WAS_PROPOSED_TO_YOUR_CITY_BY_PLAYER:
      case SV_PEACE_WAS_PROPOSED_TO_YOUR_CITY_BY_CITY: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto name = std::string{buffer};
        singleMsg >> del;
        if (del != MSG_END) return;

        switch (msgCode) {
          case SV_PEACE_WAS_PROPOSED_TO_YOU_BY_PLAYER:
            _warsAgainstPlayers.peaceWasProposedBy(name);
            break;
          case SV_PEACE_WAS_PROPOSED_TO_YOU_BY_CITY:
            _warsAgainstCities.peaceWasProposedBy(name);
            break;
          case SV_PEACE_WAS_PROPOSED_TO_YOUR_CITY_BY_PLAYER:
            _cityWarsAgainstPlayers.peaceWasProposedBy(name);
            break;
          case SV_PEACE_WAS_PROPOSED_TO_YOUR_CITY_BY_CITY:
            _cityWarsAgainstCities.peaceWasProposedBy(name);
            break;
        }

        _debug << name << " has sued for peace" << Log::endl;

        _mapWindow->markChanged();
        populateWarsList();
        break;
      }

      case SV_YOU_PROPOSED_PEACE_TO_PLAYER:
      case SV_YOU_PROPOSED_PEACE_TO_CITY:
      case SV_YOUR_CITY_PROPOSED_PEACE_TO_PLAYER:
      case SV_YOUR_CITY_PROPOSED_PEACE_TO_CITY: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto name = std::string{buffer};
        singleMsg >> del;
        if (del != MSG_END) return;

        switch (msgCode) {
          case SV_YOU_PROPOSED_PEACE_TO_PLAYER:
            _warsAgainstPlayers.proposePeaceWith(name);
            break;
          case SV_YOU_PROPOSED_PEACE_TO_CITY:
            _warsAgainstCities.proposePeaceWith(name);
            break;
          case SV_YOUR_CITY_PROPOSED_PEACE_TO_PLAYER:
            _cityWarsAgainstPlayers.proposePeaceWith(name);
            break;
          case SV_YOUR_CITY_PROPOSED_PEACE_TO_CITY:
            _cityWarsAgainstCities.proposePeaceWith(name);
            break;
        }

        _debug << "You have sued for peace with " << name << Log::endl;

        _mapWindow->markChanged();
        populateWarsList();
        break;
      }

      case SV_YOU_CANCELED_PEACE_OFFER_TO_PLAYER:
      case SV_YOU_CANCELED_PEACE_OFFER_TO_CITY:
      case SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_PLAYER:
      case SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_CITY:
      case SV_PEACE_OFFER_TO_YOU_FROM_PLAYER_WAS_CANCELED:
      case SV_PEACE_OFFER_TO_YOU_FROM_CITY_WAS_CANCELED:
      case SV_PEACE_OFFER_TO_YOUR_CITY_FROM_PLAYER_WAS_CANCELED:
      case SV_PEACE_OFFER_TO_YOUR_CITY_FROM_CITY_WAS_CANCELED: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto name = std::string{buffer};
        singleMsg >> del;
        if (del != MSG_END) return;

        switch (msgCode) {
          case SV_YOU_CANCELED_PEACE_OFFER_TO_PLAYER:
          case SV_PEACE_OFFER_TO_YOU_FROM_PLAYER_WAS_CANCELED:
            _warsAgainstPlayers.cancelPeaceOffer(name);
            break;
          case SV_YOU_CANCELED_PEACE_OFFER_TO_CITY:
          case SV_PEACE_OFFER_TO_YOU_FROM_CITY_WAS_CANCELED:
            _warsAgainstCities.cancelPeaceOffer(name);
            break;
          case SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_PLAYER:
          case SV_PEACE_OFFER_TO_YOUR_CITY_FROM_PLAYER_WAS_CANCELED:
            _cityWarsAgainstPlayers.cancelPeaceOffer(name);
            break;
          case SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_CITY:
          case SV_PEACE_OFFER_TO_YOUR_CITY_FROM_CITY_WAS_CANCELED:
            _cityWarsAgainstCities.cancelPeaceOffer(name);
            break;
        }

        switch (msgCode) {
          case SV_YOU_CANCELED_PEACE_OFFER_TO_PLAYER:
          case SV_YOU_CANCELED_PEACE_OFFER_TO_CITY:
          case SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_PLAYER:
          case SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_CITY:
            _debug << "You have revoked your offer for peace with " << name
                   << Log::endl;
            break;
          case SV_PEACE_OFFER_TO_YOU_FROM_PLAYER_WAS_CANCELED:
          case SV_PEACE_OFFER_TO_YOU_FROM_CITY_WAS_CANCELED:
          case SV_PEACE_OFFER_TO_YOUR_CITY_FROM_PLAYER_WAS_CANCELED:
          case SV_PEACE_OFFER_TO_YOUR_CITY_FROM_CITY_WAS_CANCELED:
            _debug << "Your offer for peace from " << name
                   << " has been revoked" << Log::endl;
            break;
        }

        _mapWindow->markChanged();
        populateWarsList();
        break;
      }

      case SV_AT_PEACE_WITH_PLAYER:
      case SV_AT_PEACE_WITH_CITY:
      case SV_YOUR_CITY_IS_AT_PEACE_WITH_PLAYER:
      case SV_YOUR_CITY_IS_AT_PEACE_WITH_CITY: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto name = std::string{buffer};
        singleMsg >> del;
        if (del != MSG_END) return;

        switch (msgCode) {
          case SV_AT_PEACE_WITH_PLAYER:
            _warsAgainstPlayers.remove(name);
            break;
          case SV_AT_PEACE_WITH_CITY:
            _warsAgainstCities.remove(name);
            break;
          case SV_YOUR_CITY_IS_AT_PEACE_WITH_PLAYER:
            _cityWarsAgainstPlayers.remove(name);
            break;
          case SV_YOUR_CITY_IS_AT_PEACE_WITH_CITY:
            _cityWarsAgainstCities.remove(name);
            break;
        }

        auto youAre = ""s;
        if (msgCode == SV_AT_PEACE_WITH_PLAYER ||
            msgCode == SV_AT_PEACE_WITH_CITY)
          youAre = "You are"s;
        else
          youAre = "Your city is";
        auto message = youAre + " now at peace with "s + name + "."s;
        _debug(message);
        toast("helmet", message);

        _mapWindow->markChanged();
        populateWarsList();
      }

      case SV_SPELL_HIT:
      case SV_SPELL_MISS:
      case SV_RANGED_NPC_HIT:
      case SV_RANGED_NPC_MISS:
      case SV_RANGED_WEAPON_HIT:
      case SV_RANGED_WEAPON_MISS: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto id = std::string{buffer};
        singleMsg >> del;

        double x1, y1, x2, y2;
        singleMsg >> x1 >> del >> y1 >> del >> x2 >> del >> y2 >> del;

        if (del != MSG_END) break;

        switch (msgCode) {
          case SV_SPELL_HIT:
            handle_SV_SPELL_HIT(id, {x1, y1}, {x2, y2});
            break;
          case SV_SPELL_MISS:
            handle_SV_SPELL_MISS(id, {x1, y1}, {x2, y2});
            break;
          case SV_RANGED_NPC_HIT:
            handle_SV_RANGED_NPC_HIT(id, {x1, y1}, {x2, y2});
            break;
          case SV_RANGED_NPC_MISS:
            handle_SV_RANGED_NPC_MISS(id, {x1, y1}, {x2, y2});
            break;
          case SV_RANGED_WEAPON_HIT:
            handle_SV_RANGED_WEAPON_HIT(id, {x1, y1}, {x2, y2});
            break;
          case SV_RANGED_WEAPON_MISS:
            handle_SV_RANGED_WEAPON_MISS(id, {x1, y1}, {x2, y2});
            break;
        }
        break;
      }

      case SV_PLAYER_WAS_HIT: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto username = std::string{buffer};
        singleMsg >> del;
        if (del != MSG_END) return;

        handle_SV_PLAYER_WAS_HIT(username);

        break;
      }

      case SV_ENTITY_WAS_HIT: {
        auto serial = size_t{};
        singleMsg >> serial >> del;
        if (del != MSG_END) return;

        handle_SV_ENTITY_WAS_HIT(serial);

        break;
      }

      case SV_SHOW_MISS_AT:
      case SV_SHOW_DODGE_AT:
      case SV_SHOW_BLOCK_AT:
      case SV_SHOW_CRIT_AT: {
        auto loc = MapPoint{};
        singleMsg >> loc.x >> del >> loc.y >> del;
        if (del != MSG_END) return;

        handle_SV_SHOW_OUTCOME_AT(msgCode, loc);

        break;
      }

      case SV_ENTITY_GOT_BUFF:
      case SV_ENTITY_GOT_DEBUFF: {
        auto serial = size_t{};
        singleMsg >> serial >> del;

        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto buffID = ClientBuffType::ID{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_ENTITY_GOT_BUFF(msgCode, serial, buffID);

        break;
      }

      case SV_REMAINING_BUFF_TIME:
      case SV_REMAINING_DEBUFF_TIME: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto buffID = ClientBuffType::ID{buffer};
        singleMsg >> del;

        auto timeRemaining = ms_t{0};
        singleMsg >> timeRemaining >> del;

        if (del != MSG_END) return;

        handle_SV_REMAINING_BUFF_TIME(buffID, timeRemaining,
                                      msgCode == SV_REMAINING_BUFF_TIME);

        break;
      }

      case SV_PLAYER_GOT_BUFF:
      case SV_PLAYER_GOT_DEBUFF: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto username = std::string{buffer};
        singleMsg >> del;

        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto buffID = ClientBuffType::ID{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_PLAYER_GOT_BUFF(msgCode, username, buffID);

        break;
      }

      case SV_ENTITY_LOST_BUFF:
      case SV_ENTITY_LOST_DEBUFF: {
        auto serial = size_t{};
        singleMsg >> serial >> del;

        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto buffID = ClientBuffType::ID{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_ENTITY_LOST_BUFF(msgCode, serial, buffID);

        break;
      }

      case SV_PLAYER_LOST_BUFF:
      case SV_PLAYER_LOST_DEBUFF: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto username = std::string{buffer};
        singleMsg >> del;

        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto buffID = ClientBuffType::ID{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_PLAYER_LOST_BUFF(msgCode, username, buffID);

        break;
      }

      case SV_KNOWN_SPELLS: {
        auto numSpellsKnown = 0;
        singleMsg >> numSpellsKnown >> del;

        auto knownSpellIDs = std::set<std::string>{};
        for (auto i = 0; i != numSpellsKnown; ++i) {
          auto expectedDelimiter =
              (i == numSpellsKnown - 1) ? MSG_END : MSG_DELIM;
          singleMsg.get(buffer, BUFFER_SIZE, expectedDelimiter);
          auto spellID = std::string{buffer};
          singleMsg >> del;

          knownSpellIDs.insert(spellID);
        }

        if (del != MSG_END) return;

        handle_SV_KNOWN_SPELLS(std::move(knownSpellIDs));
        break;
      }

      case SV_LEARNED_SPELL: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto spellID = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_LEARNED_SPELL(spellID);
        break;
      }

      case SV_UNLEARNED_SPELL: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto spellID = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_UNLEARNED_SPELL(spellID);
        break;
      }

      case SV_TALENT: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto talentName = std::string{buffer};
        auto level = 0;
        singleMsg >> del >> level >> del;

        if (del != MSG_END) return;

        _talentLevels[talentName] = level;
        populateClassWindow();
        break;
      }

      case SV_LOST_TALENT: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto talentName = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        auto message =
            "You have died, and the ferryman takes his toll: "
            "a talent point in "s +
            talentName + "."s;
        toast("skullRed"s, message);
        _debug(message);
        break;
      }

      case SV_POINTS_IN_TREE: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto treeName = std::string{buffer};
        auto points = 0;
        singleMsg >> del >> points >> del;

        if (del != MSG_END) return;

        _pointsInTrees[treeName] = points;
        populateClassWindow();
        break;
      }

      case SV_NO_TALENTS: {
        if (del != MSG_END) break;

        _talentLevels.clear();
        _pointsInTrees.clear();
        _knownSpells.clear();
        populateClassWindow();
        populateHotbar();
        break;
      }

      case SV_SPELL_COOLING_DOWN: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto spellID = std::string{buffer};
        auto timeRemaining = 0;
        singleMsg >> del >> timeRemaining >> del;

        if (del != MSG_END) return;

        _spellCooldowns[spellID] = timeRemaining;
        populateHotbar();
        break;
      }

      case SV_QUEST_CAN_BE_STARTED: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto questID = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_QUEST_CAN_BE_STARTED(questID);
        break;
      }

      case SV_QUEST_IN_PROGRESS: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto questID = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_QUEST_IN_PROGRESS(questID);
        break;
      }

      case SV_QUEST_CAN_BE_FINISHED: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto questID = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_QUEST_CAN_BE_FINISHED(questID);
        break;
      }

      case SV_QUEST_COMPLETED: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_END);
        auto questID = std::string{buffer};
        singleMsg >> del;

        if (del != MSG_END) return;

        handle_SV_QUEST_COMPLETED(questID);
        break;
      }

      case SV_QUEST_ACCEPTED: {
        singleMsg >> del;
        if (del != MSG_END) return;

        handle_SV_QUEST_ACCEPTED();
      }

      case SV_QUEST_PROGRESS: {
        singleMsg.get(buffer, BUFFER_SIZE, MSG_DELIM);
        auto questID = std::string{buffer};
        singleMsg >> del;

        auto objectiveIndex = size_t{0};
        singleMsg >> objectiveIndex >> del;

        auto progress = 0;
        singleMsg >> progress >> del;

        if (del != MSG_END) return;

        handle_SV_QUEST_PROGRESS(questID, objectiveIndex, progress);
        break;
      }

      case SV_SAY: {
        std::string username, message;
        singleMsg >> username >> del;
        readString(singleMsg, message, MSG_END);
        singleMsg >> del;
        if (del != MSG_END ||
            username == _username)  // We already know we said this.
          break;
        addChatMessage("[" + username + "] " + message, SAY_COLOR);
        break;
      }

      case SV_WHISPER: {
        std::string username, message;
        singleMsg >> username >> del;
        readString(singleMsg, message, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;
        addChatMessage("[From " + username + "] " + message, WHISPER_COLOR);
        _lastWhisperer = username;
        break;
      }

      case TST_SEND_THIS_BACK: {
        auto msgCodeAsInt = 0;
        singleMsg >> msgCodeAsInt >> del;
        auto msgCode = static_cast<MessageCode>(msgCodeAsInt);
        std::string args;
        readString(singleMsg, args, MSG_END);
        singleMsg >> del;
        if (del != MSG_END) break;

        sendMessage(msgCode, args);
        break;
      }

      default:;  // showErrorMessage("Unhandled message: "s + msg, Color::TODO);
    }

    if (del != MSG_END && !iss.eof()) {
      showErrorMessage("Bad message ending. code="s + toString(msgCode) +
                           "; remaining message = "s + toString(del) +
                           singleMsg.str(),
                       Color::CHAT_ERROR);
    }

    iss.peek();
  }
}

void Client::handle_SV_LOOTABLE(size_t serial) {
  const std::map<size_t, ClientObject *>::iterator it = _objects.find(serial);
  if (it == _objects.end()) {
    // showErrorMessage("Received loot info for an unknown object.",
    // Color::TODO);
    return;
  }
  ClientObject &object = *it->second;

  object.lootable(true);
  object.assembleWindow(*this);
  object.refreshTooltip();
}

void Client::handle_SV_INVENTORY(size_t serial, size_t slot,
                                 const std::string &itemID, size_t quantity) {
  const ClientItem *item = nullptr;
  if (quantity > 0) {
    const auto it = _items.find(itemID);
    if (it == _items.end()) {
      showErrorMessage(
          "Unknown inventory item \""s + itemID + "\"announced; ignored."s,
          Color::CHAT_ERROR);
      return;
    }
    item = &it->second;
  }

  ClientItem::vect_t *container;
  ClientObject *object = nullptr;
  switch (serial) {
    case INVENTORY:
      container = &_inventory;
      break;
    case GEAR:
      container = &_character.gear();
      break;
    default:
      auto it = _objects.find(serial);
      if (it == _objects.end()) {
        // showErrorMessage("Received inventory of nonexistent object;
        // ignored.", Color::TODO);
        break;
      }
      object = it->second;
      container = &object->container();
  }
  if (slot >= container->size()) {
    // showErrorMessage("Received item in invalid inventory slot; ignored.",
    // Color::TODO);
    return;
  }
  auto &invSlot = (*container)[slot];
  invSlot.first = item;
  invSlot.second = quantity;
  if (_recipeList != nullptr) _recipeList->markChanged();
  switch (serial) {
    case INVENTORY:
      _inventoryWindow->forceRefresh();
      break;
    case GEAR:
      _gearWindow->forceRefresh();
      break;
    default:
      object->onInventoryUpdate();
  }

  if (item != nullptr &&                          // It's an actual item
      (serial == INVENTORY || serial == GEAR) &&  // You are receiving it
      _actionTimer > 0)  // You were crafting or gathering
    if (item->sounds() != nullptr) item->sounds()->playOnce("drop");
}

void Client::handle_SV_MAX_HEALTH(const std::string &username,
                                  Hitpoints newMaxHealth) {
  if (username == _username) {
    _character.maxHealth(newMaxHealth);
    return;
  }
  auto it = _otherUsers.find(username);
  if (it == _otherUsers.end()) {
    return;
  }
  it->second->maxHealth(newMaxHealth);
}

void Client::handle_SV_MAX_ENERGY(const std::string &username,
                                  Energy newMaxEnergy) {
  if (username == _username) {
    _character.maxEnergy(newMaxEnergy);
    return;
  }
  auto it = _otherUsers.find(username);
  if (it == _otherUsers.end()) {
    return;
  }
  it->second->maxEnergy(newMaxEnergy);
}

void Client::handle_SV_IN_CITY(const std::string &username,
                               const std::string &cityName) {
  if (username == _username) {
    _character.cityName(cityName);
    refreshCitySection();
    return;
  }
  // Unknown user; add to lightweight city registry
  _userCities[username] = cityName;
  if (_otherUsers.find(username) == _otherUsers.end()) {
    return;
  }
  _otherUsers[username]->cityName(cityName);
}

void Client::handle_SV_NO_CITY(const std::string &username) {
  if (username == _username) {
    auto message =
        "You are no longer a citizen of "s + _character.cityName() + "."s;

    _character.cityName("");

    refreshCitySection();
    toast("column", message);
    _debug(message);
    return;
  }
  auto userIt = _otherUsers.find(username);
  if (userIt == _otherUsers.end()) return;
  userIt->second->cityName("");
}

void Client::handle_SV_KING(const std::string username) {
  if (username == _username) {
    _character.setAsKing();
    return;
  }
  auto userIt = _otherUsers.find(username);
  if (userIt == _otherUsers.end()) return;
  userIt->second->setAsKing();
}

void Client::handle_SV_SPELL_HIT(const std::string &spellID,
                                 const MapPoint &src, const MapPoint &dst) {
  auto it = _spells.find(spellID);
  if (it == _spells.end()) return;
  const auto &spell = *it->second;

  if (spell.sounds()) spell.sounds()->playOnce("launch");

  if (spell.projectile())
    spell.projectile()->instantiate(src, dst);
  else
    onSpellHit(dst, &spell);
}

void Client::handle_SV_SPELL_MISS(const std::string &spellID,
                                  const MapPoint &src, const MapPoint &dst) {
  auto it = _spells.find(spellID);
  if (it == _spells.end()) return;
  const auto &spell = *it->second;

  if (spell.sounds()) spell.sounds()->playOnce("launch");

  if (spell.projectile()) {
    auto pointPastDest = extrapolate(src, dst, 2000);
    spell.projectile()->instantiate(src, pointPastDest, true);
  }
}

void Client::handle_SV_RANGED_NPC_HIT(const std::string &npcID,
                                      const MapPoint &src,
                                      const MapPoint &dst) {
  auto npcType = findNPCType(npcID);
  if (!npcType) return;
  if (npcType->projectile()) npcType->projectile()->instantiate(src, dst);
}

void Client::handle_SV_RANGED_NPC_MISS(const std::string &npcID,
                                       const MapPoint &src,
                                       const MapPoint &dst) {
  auto npcType = findNPCType(npcID);
  if (!npcType) return;

  if (npcType->projectile()) {
    auto pointPastDest = extrapolate(src, dst, 2000);
    npcType->projectile()->instantiate(src, pointPastDest, true);
  }
}

void Client::handle_SV_RANGED_WEAPON_HIT(const std::string &weaponID,
                                         const MapPoint &src,
                                         const MapPoint &dst) {
  auto it = _items.find(weaponID);
  if (it == _items.end()) return;
  const auto &item = it->second;

  if (item.projectile()) {
    item.projectile()->instantiate(src, dst);
  }
}

void Client::handle_SV_RANGED_WEAPON_MISS(const std::string &weaponID,
                                          const MapPoint &src,
                                          const MapPoint &dst) {
  auto it = _items.find(weaponID);
  if (it == _items.end()) return;
  const auto &item = it->second;

  if (item.projectile()) {
    auto pointPastDest = extrapolate(src, dst, 2000);
    item.projectile()->instantiate(src, pointPastDest, true);
  }
}

void Client::handle_SV_PLAYER_WAS_HIT(const std::string &username) {
  Avatar *victim = nullptr;
  if (username == _username)
    victim = &_character;
  else {
    auto it = _otherUsers.find(username);
    if (it == _otherUsers.end()) return;
    victim = it->second;
  }
  victim->playSoundWhenHit();
  victim->createDamageParticles();
}

void Client::handle_SV_ENTITY_WAS_HIT(size_t serial) {
  auto objIt = _objects.find(serial);
  if (objIt == _objects.end()) {
    return;
  }
  const ClientObject &victim = *objIt->second;
  const SoundProfile *sounds = victim.objectType()->sounds();
  victim.playSoundWhenHit();
  victim.createDamageParticles();
}

void Client::handle_SV_SHOW_OUTCOME_AT(int msgCode, const MapPoint &loc) {
  switch (msgCode) {
    case SV_SHOW_MISS_AT:
      addFloatingCombatText("MISS", loc, Color::FLOATING_MISS);
      break;
    case SV_SHOW_DODGE_AT:
      addFloatingCombatText("DODGE", loc, Color::FLOATING_MISS);
      break;
    case SV_SHOW_BLOCK_AT:
      addFloatingCombatText("BLOCK", loc, Color::FLOATING_MISS);
      break;
    case SV_SHOW_CRIT_AT:
      addFloatingCombatText("CRIT", loc, Color::FLOATING_CRIT);
      break;
  }
}

void Client::handle_SV_ENTITY_GOT_BUFF(int msgCode, size_t serial,
                                       const std::string &buffID) {
  auto objIt = _objects.find(serial);
  if (objIt == _objects.end()) {
    return;
  }

  objIt->second->addBuffOrDebuff(buffID, msgCode == SV_ENTITY_GOT_BUFF);

  if (objIt->second == _target.entity()) refreshTargetBuffs();
}

void Client::handle_SV_ENTITY_LOST_BUFF(int msgCode, size_t serial,
                                        const std::string &buffID) {
  auto objIt = _objects.find(serial);
  if (objIt == _objects.end()) {
    return;
  }

  objIt->second->removeBuffOrDebuff(buffID, msgCode == SV_ENTITY_LOST_BUFF);

  if (objIt->second == _target.entity()) refreshTargetBuffs();
}

void Client::handle_SV_PLAYER_GOT_BUFF(int msgCode, const std::string &username,
                                       const std::string &buffID) {
  Avatar *avatar = nullptr;
  if (username == _username)
    avatar = &_character;
  else {
    auto it = _otherUsers.find(username);
    if (it == _otherUsers.end()) return;
    avatar = it->second;
  }

  auto isBuff = msgCode == SV_PLAYER_GOT_BUFF;
  avatar->addBuffOrDebuff(buffID, isBuff);

  if (avatar == &_character) {
    auto it = buffTypes().find(buffID);
    auto buffExists = it != buffTypes().end();
    if (buffExists) {
      ms_t duration = it->second.duration() * 1000;

      if (isBuff)
        _buffTimeRemaining[buffID] = duration;
      else
        _debuffTimeRemaining[buffID] = duration;
    }

    refreshBuffsDisplay();
  }

  if (avatar == _target.entity()) refreshTargetBuffs();
}

void Client::handle_SV_PLAYER_LOST_BUFF(int msgCode,
                                        const std::string &username,
                                        const std::string &buffID) {
  Avatar *avatar = nullptr;
  if (username == _username)
    avatar = &_character;
  else {
    auto it = _otherUsers.find(username);
    if (it == _otherUsers.end()) return;
    avatar = it->second;
  }

  avatar->removeBuffOrDebuff(buffID, msgCode == SV_PLAYER_LOST_BUFF);

  if (avatar == &_character) refreshBuffsDisplay();

  if (avatar == _target.entity()) refreshTargetBuffs();
}

void Client::handle_SV_REMAINING_BUFF_TIME(const std::string &buffID,
                                           ms_t timeRemaining, bool isBuff) {
  if (isBuff)
    _buffTimeRemaining[buffID] = timeRemaining;
  else
    _debuffTimeRemaining[buffID] = timeRemaining;

  refreshBuffsDisplay();
}

void Client::handle_SV_KNOWN_SPELLS(
    const std::set<std::string> &&knownSpellIDs) {
  _knownSpells.clear();

  for (const auto &id : knownSpellIDs) {
    auto it = _spells.find(id);
    if (it == _spells.end()) continue;
    _knownSpells.insert(it->second);
  }

  populateHotbar();
}

void Client::handle_SV_LEARNED_SPELL(const std::string &spellID) {
  auto it = _spells.find(spellID);
  if (it == _spells.end()) return;
  _knownSpells.insert(it->second);

  populateHotbar();
}

void Client::handle_SV_UNLEARNED_SPELL(const std::string &spellID) {
  auto it = _spells.find(spellID);
  if (it == _spells.end()) return;
  _knownSpells.erase(it->second);

  populateHotbar();
}

void Client::handle_SV_LEVEL_UP(const std::string &username) {
  Avatar *avatar = nullptr;
  if (username == _username)
    avatar = &_character;
  else {
    auto it = _otherUsers.find(username);
    if (it == _otherUsers.end()) return;
    avatar = it->second;
  }

  avatar->levelUp();
  avatar->refreshTooltip();

  addParticles("levelUp", avatar->location());

  if (username == _username) {
    populateClassWindow();

    generalSounds()->playOnce("levelUp");

    auto message =
        "You have reached level "s + toString(avatar->level()) + "!"s;
    toast("light", message);
    _debug(message);
  }
}

void Client::handle_SV_NPC_LEVEL(size_t serial, Level level) {
  auto objIt = _objects.find(serial);
  if (objIt == _objects.end()) {
    return;
  }

  objIt->second->level(level);
}

void Client::handle_SV_PLAYER_DAMAGED(const std::string &username,
                                      Hitpoints amount) {
  Avatar *user{nullptr};
  if (username == _username)
    user = &_character;
  else {
    auto it = _otherUsers.find(username);
    if (it == _otherUsers.end()) return;
    user = it->second;
  }

  addFloatingCombatText("-"s + toString(amount), user->location(),
                        Color::FLOATING_DAMAGE);
}

void Client::handle_SV_PLAYER_HEALED(const std::string &username,
                                     Hitpoints amount) {
  Avatar *user{nullptr};
  if (username == _username)
    user = &_character;
  else {
    auto it = _otherUsers.find(username);
    if (it == _otherUsers.end()) return;
    user = it->second;
  }

  addFloatingCombatText("+"s + toString(amount), user->location(),
                        Color::FLOATING_HEAL);
}

void Client::handle_SV_OBJECT_DAMAGED(size_t serial, Hitpoints amount) {
  auto it = _objects.find(serial);
  if (it == _objects.end()) return;

  addFloatingCombatText("-"s + toString(amount), it->second->location(),
                        Color::FLOATING_DAMAGE);
}

void Client::handle_SV_OBJECT_HEALED(size_t serial, Hitpoints amount) {
  auto it = _objects.find(serial);
  if (it == _objects.end()) return;

  addFloatingCombatText("+"s + toString(amount), it->second->location(),
                        Color::FLOATING_HEAL);
}

void Client::handle_SV_QUEST_CAN_BE_STARTED(const std::string &questID) {
  auto it = _quests.find(questID);
  if (it == _quests.end()) return;

  it->second.state = CQuest::CAN_START;

  populateQuestLog();
  refreshQuestProgress();
}

void Client::handle_SV_QUEST_IN_PROGRESS(const std::string &questID) {
  auto it = _quests.find(questID);
  if (it == _quests.end()) return;

  it->second.state = CQuest::IN_PROGRESS;

  // Start node could start this, but now can't
  const auto &startNode = it->second.info().startsAt;
  for (auto pair : _objects) {
    auto &obj = *pair.second;
    if (obj.objectType()->id() == startNode) obj.assembleWindow(*this);
  }

  populateQuestLog();
  refreshQuestProgress();
}

void Client::handle_SV_QUEST_CAN_BE_FINISHED(const std::string &questID) {
  auto it = _quests.find(questID);
  if (it == _quests.end()) return;

  it->second.state = CQuest::CAN_FINISH;

  // Start node could start this, but now can't
  const auto &startNode = it->second.info().startsAt;
  for (auto pair : _objects) {
    auto &obj = *pair.second;
    if (obj.objectType()->id() == startNode) obj.assembleWindow(*this);
  }

  populateQuestLog();
  refreshQuestProgress();
}

void Client::handle_SV_QUEST_COMPLETED(const std::string &questID) {
  auto it = _quests.find(questID);
  if (it == _quests.end()) return;

  it->second.state = CQuest::NONE;

  // End node could finish this, but now can't
  const auto &endNode = it->second.info().endsAt;
  for (auto pair : _objects) {
    auto &obj = *pair.second;
    if (obj.objectType()->id() == endNode) obj.assembleWindow(*this);
  }

  generalSounds()->playOnce("quest");

  populateQuestLog();
  refreshQuestProgress();
}

void Client::handle_SV_QUEST_ACCEPTED() { generalSounds()->playOnce("quest"); }

void Client::handle_SV_QUEST_PROGRESS(const std::string &questID,
                                      size_t objectiveIndex, int progress) {
  auto it = _quests.find(questID);
  if (it == _quests.end()) return;

  it->second.state = CQuest::IN_PROGRESS;

  it->second.setProgress(objectiveIndex, progress);

  populateQuestLog();
  refreshQuestProgress();
}

void Client::sendRawMessage(const std::string &msg) const {
  _connection.socket().sendMessage(msg);
}

void Client::sendMessage(MessageCode msgCode, const std::string &args) const {
  auto message = compileMessage(msgCode, args);
  sendRawMessage(message);
}

std::string Client::compileMessage(MessageCode msgCode,
                                   const std::string &args) {
  std::ostringstream oss;
  oss << MSG_START << msgCode;
  if (args != "") oss << MSG_DELIM << args;
  oss << MSG_END;
  return oss.str();
}

void Client::sendRawMessageStatic(void *data) {
  auto &client = *_instance;
  auto *message = reinterpret_cast<const std::string *>(data);
  client.sendRawMessage(*message);
}

void Client::disconnect() {
  _connection.state(Connection::CONNECTION_ERROR);

  // Hide windows
  for (auto *window : _windows) window->hide();
}

void Client::initializeMessageNames() {
  _messageCommands["played"] = CL_REQUEST_TIME_PLAYED;
  _messageCommands["location"] = CL_LOCATION;
  _messageCommands["cancel"] = CL_CANCEL_ACTION;
  _messageCommands["craft"] = CL_CRAFT;
  _messageCommands["constructItem"] = CL_CONSTRUCT_ITEM;
  _messageCommands["construct"] = CL_CONSTRUCT;
  _messageCommands["gather"] = CL_GATHER;
  _messageCommands["deconstruct"] = CL_DECONSTRUCT;
  _messageCommands["drop"] = CL_DROP;
  _messageCommands["swap"] = CL_SWAP_ITEMS;
  _messageCommands["trade"] = CL_TRADE;
  _messageCommands["setMerchantSlot"] = CL_SET_MERCHANT_SLOT;
  _messageCommands["clearMerchantSlot"] = CL_CLEAR_MERCHANT_SLOT;
  _messageCommands["startWatching"] = CL_START_WATCHING;
  _messageCommands["stopWatching"] = CL_STOP_WATCHING;
  _messageCommands["targetNPC"] = CL_TARGET_ENTITY;
  _messageCommands["targetPlayer"] = CL_TARGET_PLAYER;
  _messageCommands["take"] = CL_TAKE_ITEM;
  _messageCommands["mount"] = CL_MOUNT;
  _messageCommands["dismount"] = CL_DISMOUNT;
  _messageCommands["war"] = CL_DECLARE_WAR_ON_PLAYER;
  _messageCommands["cityWar"] = CL_DECLARE_WAR_ON_CITY;
  _messageCommands["cede"] = CL_CEDE;
  _messageCommands["cquit"] = CL_LEAVE_CITY;
  _messageCommands["recruit"] = CL_RECRUIT;
  _messageCommands["cast"] = CL_CAST;

  _messageCommands["say"] = CL_SAY;
  _messageCommands["s"] = CL_SAY;
  _messageCommands["whisper"] = CL_WHISPER;
  _messageCommands["w"] = CL_WHISPER;

  _messageCommands["give"] = DG_GIVE;
  _messageCommands["unlock"] = DG_UNLOCK;
  _messageCommands["level"] = DG_LEVEL;
  _messageCommands["spells"] = DG_SPELLS;
  _messageCommands["die"] = DG_DIE;

  _errorMessages[WARNING_TOO_FAR] =
      "You are too far away to perform that action.";
  _errorMessages[WARNING_DOESNT_EXIST] = "That object doesn't exist.";
  _errorMessages[WARNING_INVENTORY_FULL] = "Your inventory is full.";
  _errorMessages[WARNING_NEED_MATERIALS] =
      "You do not have the materials neded to create that item.";
  _errorMessages[WARNING_NEED_TOOLS] =
      "You do not have the tools needed to create that item.";
  _errorMessages[WARNING_ACTION_INTERRUPTED] = "Action interrupted.";
  _errorMessages[ERROR_INVALID_USER] = "That user doesn't exist.";
  _errorMessages[ERROR_INVALID_ITEM] = "That is not a real item.";
  _errorMessages[ERROR_CANNOT_CRAFT] = "That item cannot be crafted.";
  _errorMessages[ERROR_EMPTY_SLOT] = "That inventory slot is empty.";
  _errorMessages[ERROR_INVALID_SLOT] = "That is not a valid inventory slot.";
  _errorMessages[ERROR_CANNOT_CONSTRUCT] = "That item cannot be constructed.";
  _errorMessages[WARNING_BLOCKED] =
      "That location is unsuitable for that action.";
  _errorMessages[WARNING_NO_PERMISSION] =
      "You do not have permission to do that.";
  _errorMessages[ERROR_NOT_MERCHANT] = "That is not a merchant object.";
  _errorMessages[ERROR_INVALID_MERCHANT_SLOT] =
      "That is not a valid merchant slot.";
  _errorMessages[WARNING_NO_WARE] =
      "The object does not have enough items in stock.";
  _errorMessages[WARNING_NO_PRICE] = "You cannot afford to buy that.";
  _errorMessages[WARNING_MERCHANT_INVENTORY_FULL] =
      "The object does not have enough inventory space for that exchange.";
  _errorMessages[WARNING_NOT_EMPTY] = "That object is not empty.";
  _errorMessages[ERROR_TARGET_DEAD] = "That target is dead.";
  _errorMessages[ERROR_NPC_SWAP] = "You can't put items inside an NPC.";
  _errorMessages[ERROR_TAKE_SELF] = "You can't take an item from yourself.";
  _errorMessages[ERROR_NOT_GEAR] =
      "That item can't be used in that equipment slot.";
  _errorMessages[ERROR_NOT_VEHICLE] = "That isn't a vehicle.";
  _errorMessages[WARNING_VEHICLE_OCCUPIED] =
      "You can't do that to an occupied vehicle.";
  _errorMessages[WARNING_NO_VEHICLE] = "You are not in a vehicle.";
  _errorMessages[ERROR_UNKNOWN_RECIPE] = "You don't know that recipe.";
  _errorMessages[ERROR_UNKNOWN_CONSTRUCTION] =
      "You don't know how to construct that object.";
  _errorMessages[WARNING_WRONG_MATERIAL] =
      "The construction site doesn't need that.";
  _errorMessages[ERROR_UNDER_CONSTRUCTION] =
      "That object is still under construction.";
  _errorMessages[ERROR_ATTACKED_PEACFUL_PLAYER] =
      "You are not at war with that player.";
  _errorMessages[WARNING_UNIQUE_OBJECT] = "There can be only one.";
  _errorMessages[ERROR_INVALID_OBJECT] = "That is not a valid object type.";
  _errorMessages[ERROR_ALREADY_AT_WAR] = "You are already at war with them.";
  _errorMessages[ERROR_NOT_IN_CITY] = "You are not in a city.";
  _errorMessages[ERROR_NO_INVENTORY] = "That object doesn't have an inventory.";
  _errorMessages[ERROR_DAMAGED_OBJECT] =
      "You can't do that while the object is damaged.";
  _errorMessages[ERROR_CANNOT_CEDE] = "You can't cede that to your city.";
  _errorMessages[ERROR_NO_ACTION] = "That object has no action to perform.";
  _errorMessages[ERROR_KING_CANNOT_LEAVE_CITY] =
      "A king cannot leave his city.";
  _errorMessages[ERROR_ALREADY_IN_CITY] = "That player is already in a city.";
  _errorMessages[WARNING_YOU_ARE_ALREADY_IN_CITY] = "You are already in a city";
  _errorMessages[ERROR_NOT_A_KING] = "Only a king can do that.";
  _errorMessages[WARNING_INVALID_SPELL_TARGET] = "Invalid spell target.";
  _errorMessages[WARNING_NOT_ENOUGH_ENERGY] = "You don't have enough energy.";
  _errorMessages[ERROR_INVALID_TALENT] = "That isn't a talent you can take.";
  _errorMessages[ERROR_ALREADY_KNOW_SPELL] = "You already know that spell.";
  _errorMessages[ERROR_DONT_KNOW_SPELL] = "You haven't learned that spell.";
  _errorMessages[WARNING_NO_TALENT_POINTS] =
      "You don't have any more talent points to allocate.";
  _errorMessages[WARNING_MISSING_ITEMS_FOR_TALENT] =
      "You don't have the items needed to learn that talent.";
  _errorMessages[WARNING_MISSING_REQ_FOR_TALENT] =
      "You don't meet the requirements for that talent.";
  _errorMessages[WARNING_STUNNED] = "You can't do that while stunned.";
  _errorMessages[WARNING_USER_DOESNT_EXIST] = "That account does not exist.";
  _errorMessages[WARNING_NAME_TAKEN] = "That name is taken.";
  _errorMessages[WARNING_ITEM_NEEDED] = "You are missing a required item.";
}

void Client::performCommand(const std::string &commandString) {
  std::istringstream iss(commandString);
  std::string token;
  char c;
  iss >> c;
  if (c != '/') {
    assert(false);
    showErrorMessage("Commands must begin with '/'.", Color::CHAT_WARNING);
    return;
  }

  const auto BUFFER_SIZE = 1023;
  static char buffer[BUFFER_SIZE + 1];
  iss.get(buffer, BUFFER_SIZE, ' ');
  std::string command(buffer);

  // std::vector<std::string> args;
  // while (!iss.eof()) {
  //    while (iss.peek() == ' ')
  //        iss.ignore(BUFFER_SIZE, ' ');
  //    if (iss.eof())
  //        break;
  //    iss.get(buffer, BUFFER_SIZE, ' ');
  //    args.push_back(buffer);
  //}

  // Messages to server
  if (_messageCommands.find(command) != _messageCommands.end()) {
    MessageCode code = static_cast<MessageCode>(_messageCommands[command]);
    std::string argsString;
    switch (code) {
      case CL_SAY:
        iss.get(buffer, BUFFER_SIZE);
        argsString = buffer;
        addChatMessage("[" + _username + "] " + argsString, SAY_COLOR);
        break;

      case CL_WHISPER: {
        while (iss.peek() == ' ') iss.ignore(BUFFER_SIZE, ' ');
        if (iss.eof()) break;
        iss.get(buffer, BUFFER_SIZE, ' ');
        std::string username(buffer);
        iss.get(buffer, BUFFER_SIZE);
        argsString = username + MSG_DELIM + buffer;
        addChatMessage("[To " + username + "] " + buffer, WHISPER_COLOR);
        break;
      }

      default:
        std::vector<std::string> args;
        while (!iss.eof()) {
          while (iss.peek() == ' ') iss.ignore(BUFFER_SIZE, ' ');
          if (iss.eof()) break;
          iss.get(buffer, BUFFER_SIZE, ' ');
          args.push_back(buffer);
        }
        std::ostringstream oss;
        for (size_t i = 0; i != args.size(); ++i) {
          oss << args[i];
          if (i < args.size() - 1) {
            if (code == CL_SAY ||  // Allow spaces in messages
                code == CL_WHISPER && i > 0)
              oss << ' ';
            else
              oss << MSG_DELIM;
          }
        }
        argsString = oss.str();
    }

    sendMessage(code, argsString);
    return;
  }

  showErrorMessage("Unknown command: "s + command, Color::CHAT_WARNING);
}

void Client::sendClearTargetMessage() const {
  sendMessage(CL_TARGET_ENTITY, makeArgs(0));
}
