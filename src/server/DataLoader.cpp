#include "DataLoader.h"

#include <windows.h>

#include <algorithm>
#include <set>

#include "../Args.h"
#include "../XmlReader.h"
#include "ProgressLock.h"
#include "Server.h"
#include "VehicleType.h"

extern Args cmdLineArgs;

DataLoader::DataLoader(Server &server) : _server(server) {}

DataLoader DataLoader::FromPath(Server &server, const Directory &path) {
  auto loader = DataLoader(server);
  loader._path = path;
  return loader;
}

DataLoader DataLoader::FromString(Server &server, const XML &data) {
  auto loader = DataLoader(server);
  loader._data = data;
  return loader;
}

void DataLoader::load(bool keepOldData) {
  _server._debug("Loading data");

  if (!keepOldData) {
    _server._entities.clear();
    TerrainList::clearLists();
    _server._items.clear();
    _server._objectTypes.clear();
    _server._recipes.clear();
    _server._buffTypes.clear();
    _server._classes.clear();
    _server._tiers.clear();
  }

  auto usingPath = !_path.empty();
  if (usingPath) {
    _files = findDataFiles();

    if (isDebug()) {
      for (const auto &filename : _files) {
        auto xr = XmlReader::FromFile(filename);
        if (!xr) {
          _server._debug("Failed to load XML file "s + filename,
                         Color::CHAT_ERROR);
        }
      }
    }

    loadFromAllFiles(&DataLoader::loadTerrain);
    loadFromAllFiles(&DataLoader::loadTerrainLists);

    auto reader = XmlReader::FromFile(_path + "/map.xml");
    loadMap(reader);

    loadFromAllFiles(&DataLoader::loadLootTables);
    loadFromAllFiles(&DataLoader::loadObjectTypes);
    loadFromAllFiles(&DataLoader::loadNPCTemplates);
    loadFromAllFiles(&DataLoader::loadNPCTypes);
    loadFromAllFiles(&DataLoader::loadItems);
    loadFromAllFiles(&DataLoader::loadQuests);
    loadFromAllFiles(&DataLoader::loadRecipes);
    loadFromAllFiles(&DataLoader::loadSpells);
    loadFromAllFiles(&DataLoader::loadBuffs);
    loadFromAllFiles(&DataLoader::loadClasses);
    loadFromAllFiles(&DataLoader::loadSpawners);

  } else {
    auto data = XmlReader::FromString(_data);
    if (!data) {
      _server._dataLoaded = true;
      return;
    }
    loadTerrain(data);
    loadTerrainLists(data);
    loadMap(data);
    loadLootTables(data);
    loadObjectTypes(data);
    loadNPCTemplates(data);
    loadNPCTypes(data);
    loadItems(data);
    loadQuests(data);
    loadRecipes(data);
    loadSpells(data);
    loadBuffs(data);
    loadClasses(data);
    loadSpawners(data);
  }

  _server._dataLoaded = true;
}

void DataLoader::loadFromAllFiles(LoadFunction load) {
  for (const auto &file : _files) {
    auto xr = XmlReader::FromFile(file);
    if (!xr) continue;
    (this->*load)(xr);
  }
}

DataLoader::FilesList DataLoader::findDataFiles() const {
  auto list = FilesList{};

  WIN32_FIND_DATA fd;
  auto path = std::string{_path.begin(), _path.end()} + "/";
  std::replace(path.begin(), path.end(), '/', '\\');
  std::string filter = path + "*.xml";
  path.c_str();
  HANDLE hFind = FindFirstFile(filter.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (fd.cFileName == "map.xml"s) continue;
      auto file = path + fd.cFileName;
      list.insert(file);
    } while (FindNextFile(hFind, &fd));
    FindClose(hFind);
  }

  return list;
}

void DataLoader::loadTerrain(XmlReader &xr) {
  for (auto elem : xr.getChildren("terrain")) {
    char index;
    if (!xr.findAttr(elem, "index", index)) continue;

    auto t = new Terrain;

    t->loadTagsFromXML(xr, elem);

    _server._terrainTypes[index] = t;
  }
}

void DataLoader::loadTerrainLists(XmlReader &xr) {
  TerrainList::loadFromXML(xr);
}

void DataLoader::loadObjectTypes(XmlReader &xr) {
  for (auto elem : xr.getChildren("objectType")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;
    ObjectType *ot = nullptr;
    int n;
    if (xr.findAttr(elem, "isVehicle", n) == 1)
      ot = new VehicleType(id);
    else
      ot = new ObjectType(id);

    std::string s;
    if (xr.findAttr(elem, "gatherTime", n)) ot->yield.gatherTime(n);
    if (xr.findAttr(elem, "constructionTime", n)) ot->constructionTime(n);
    if (xr.findAttr(elem, "gatherReq", s)) ot->yield.requiresTool(s);
    if (xr.findAttr(elem, "constructionReq", s)) ot->constructionReq(s);

    // Deconstruction
    if (xr.findAttr(elem, "deconstructs", s)) {
      ms_t deconstructionTime = 0;
      xr.findAttr(elem, "deconstructionTime", deconstructionTime);
      std::set<ServerItem>::const_iterator itemIt =
          _server._items.insert(ServerItem(s)).first;
      ot->addDeconstruction(
          DeconstructionType::ItemAndTime(&*itemIt, deconstructionTime));
    }

    // Quest exclusivity
    if (xr.findAttr(elem, "exclusiveToQuest", s)) ot->exclusiveToQuest(s);

    ot->yield.loadFromXML(xr, elem);

    // Merchant
    if (xr.findAttr(elem, "merchantSlots", n)) ot->merchantSlots(n);
    if (xr.findAttr(elem, "bottomlessMerchant", n))
      ot->bottomlessMerchant(n == 1);

    MapRect r;
    if (xr.findRectChild("collisionRect", elem, r)) ot->collisionRect(r);
    if (xr.findAttr(elem, "collides", n)) ot->collides(n != 0);

    ot->loadTagsFromXML(xr, elem);

    // Construction site
    bool needsMaterials = false;
    for (auto objMat : xr.getChildren("material", elem)) {
      if (!xr.findAttr(objMat, "id", s)) continue;
      std::set<ServerItem>::const_iterator itemIt =
          _server._items.insert(ServerItem(s)).first;
      n = 1;
      xr.findAttr(objMat, "quantity", n);
      ot->addMaterial(&*itemIt, n);
      needsMaterials = true;
    }
    if (xr.findAttr(elem, "isUnique", n) == 1) ot->makeUnique();
    bool isUnbuildable = xr.findAttr(elem, "isUnbuildable", n) == 1;
    if (isUnbuildable) ot->makeUnbuildable();

    if (xr.findAttr(elem, "playerUnique", s)) ot->makeUniquePerPlayer(s);

    // Construction locks
    bool requiresUnlock = false;
    for (auto unlockedBy : xr.getChildren("unlockedBy", elem)) {
      requiresUnlock = true;
      double chance = 1.0;
      xr.findAttr(unlockedBy, "chance", chance);
      ProgressLock::Type triggerType;
      if (xr.findAttr(unlockedBy, "item", s))
        triggerType = ProgressLock::ITEM;
      else if (xr.findAttr(unlockedBy, "construction", s))
        triggerType = ProgressLock::CONSTRUCTION;
      else if (xr.findAttr(unlockedBy, "gather", s))
        triggerType = ProgressLock::GATHER;
      else if (xr.findAttr(unlockedBy, "recipe", s))
        triggerType = ProgressLock::RECIPE;
      else
        continue;  // Not a real lock, but blocks being known by default
      ProgressLock(triggerType, s, ProgressLock::CONSTRUCTION, id, chance)
          .stage();
    }
    if (!requiresUnlock && !isUnbuildable && needsMaterials)
      ot->knownByDefault();
    if (needsMaterials && !isUnbuildable) ++_server._numBuildableObjects;

    // Container
    auto container = xr.findChild("container", elem);
    if (container != nullptr) {
      if (xr.findAttr(container, "slots", n)) {
        ot->addContainer(ContainerType::WithSlots(n));
      }
    }

    // Terrain restrictions
    if (xr.findAttr(elem, "allowedTerrain", s)) ot->allowedTerrain(s);

    ot->transformation.loadFromXML<ObjectType>(xr, elem);

    // Disappearance
    auto disappearTime = 0;
    if (xr.findAttr(elem, "disappearAfter", disappearTime))
      ot->disappearsAfter(disappearTime);

    // Durability
    auto durability = xr.findChild("durability", elem);
    if (durability) {
      if (xr.findAttr(durability, "item", s) &&
          xr.findAttr(durability, "quantity", n)) {
        std::set<ServerItem>::const_iterator itemIt =
            _server._items.insert(ServerItem(s)).first;
        ot->setHealthBasedOnItems(&*itemIt, n);
      } else
        _server._debug("Durability specified without item type; skipping.",
                       Color::CHAT_ERROR);
    }

    if (xr.findAttr(elem, "destroyIfUsedAsTool", n) && n == 1)
      ot->destroyIfUsedAsTool(true);

    // Repairing
    auto repairElem = xr.findChild("canBeRepaired", elem);
    if (repairElem) {
      ot->makeRepairable();
      if (xr.findAttr(repairElem, "cost", s)) ot->repairingCosts(s);
      if (xr.findAttr(repairElem, "tool", s)) ot->repairingRequiresTool(s);
    }

    // Action
    auto action = xr.findChild("action", elem);
    if (action != nullptr) {
      auto *pAction = new Action;

      std::string target;
      if (!xr.findAttr(action, "target", target)) {
        _server._debug("Skipping action with missing target",
                       Color::CHAT_ERROR);
        continue;
      }
      auto it = Action::functionMap.find(target);
      if (it == Action::functionMap.end()) {
        _server._debug << Color::CHAT_ERROR << "Action target " << target
                       << "() doesn't exist; skipping" << Log::endl;
        continue;
      }
      pAction->function = it->second;

      auto costID = ""s;
      if (xr.findAttr(action, "cost", costID)) {
        std::set<ServerItem>::const_iterator itemIt =
            _server._items.insert(ServerItem(costID)).first;
        auto pItem = &*itemIt;
        pAction->cost = pItem;
      }

      ot->action(pAction);
    }

    // onDestroy callback
    auto onDestroy = xr.findChild("onDestroy", elem);
    if (onDestroy != nullptr) {
      std::string target;
      if (!xr.findAttr(onDestroy, "target", target)) {
        _server._debug("Skipping onDestroy with missing target",
                       Color::CHAT_ERROR);
        continue;
      }
      auto it = CallbackAction::functionMap.find(target);
      if (it == CallbackAction::functionMap.end()) {
        _server._debug << Color::CHAT_ERROR << "CallbackAction target "
                       << target << "() doesn't exist; skipping" << Log::endl;
        continue;
      }

      auto *pAction = new CallbackAction;
      pAction->function = it->second;
      ot->onDestroy(pAction);
    }

    // Grants buff on overlap
    auto grantsBuff = xr.findChild("grantsBuff", elem);
    if (grantsBuff) {
      auto buffID = ""s;
      auto radius = 0.0;
      if (xr.findAttr(grantsBuff, "id", buffID) &&
          xr.findAttr(grantsBuff, "radius", radius)) {
      }
      const auto *buff = &_server._buffTypes[buffID];
      ot->grantsBuff(buff, radius);
    }

    bool foundInPlace = false;
    for (auto it = _server._objectTypes.begin();
         it != _server._objectTypes.end(); ++it) {
      if ((*it)->id() == ot->id()) {
        ObjectType &inPlace = *const_cast<ObjectType *>(*it);
        inPlace = *ot;
        delete ot;
        foundInPlace = true;
        break;
      }
    }
    if (!foundInPlace) _server._objectTypes.insert(ot);
  }
}

void DataLoader::loadQuests(XmlReader &xr) {
  for (auto elem : xr.getChildren("quest")) {
    auto id = ""s;
    if (!xr.findAttr(elem, "id", id)) continue;
    // It may already exist, due to being a prerequisite.
    auto &q = _server._quests[id];
    q.id = id;

    auto startsAt = ""s;
    if (!xr.findAttr(elem, "startsAt", startsAt)) continue;
    auto startingObject = _server.findObjectTypeByID(startsAt);
    if (!startingObject) continue;

    auto endsAt = ""s;
    if (!xr.findAttr(elem, "endsAt", endsAt)) continue;
    auto endingObject = _server.findObjectTypeByID(endsAt);
    if (!endingObject) continue;

    xr.findAttr(elem, "timeLimit", q.timeLimit);

    for (auto objectiveElem : xr.getChildren("objective", elem)) {
      Quest::Objective objective;

      auto type = ""s;
      xr.findAttr(objectiveElem, "type", type);
      objective.setType(type);
      xr.findAttr(objectiveElem, "id", objective.id);

      if (objective.type == Quest::Objective::FETCH) {
        auto item = _server._items.find(objective.id);
        if (item == _server._items.end())
          objective.type = Quest::Objective::NONE;
        else
          objective.item = &*item;
      }

      xr.findAttr(objectiveElem, "qty", objective.qty);

      q.objectives.push_back(objective);
    }

    for (auto rewardElem : xr.getChildren("reward", elem)) {
      auto reward = Quest::Reward{};
      auto type = ""s;
      if (!xr.findAttr(rewardElem, "id", reward.id)) continue;
      xr.findAttr(rewardElem, "qty", reward.itemQuantity);

      if (!xr.findAttr(rewardElem, "type", type)) continue;
      if (type == "construction")
        reward.type = Quest::Reward::CONSTRUCTION;
      else if (type == "spell")
        reward.type = Quest::Reward::SPELL;
      else if (type == "item")
        reward.type = Quest::Reward::ITEM;
      else
        continue;

      q.rewards.push_back(reward);
    }

    for (auto prereq : xr.getChildren("prerequisite", elem)) {
      auto prereqID = ""s;
      if (xr.findAttr(prereq, "id", prereqID)) {
        q.prerequisiteQuests.insert(prereqID);
        auto &prerequisiteQuest = _server._quests[prereqID];
        prerequisiteQuest.otherQuestsWithThisAsPrerequisite.insert(id);
      }
    }

    for (auto startsWithItem : xr.getChildren("startsWithItem", elem)) {
      auto id = ""s;
      if (xr.findAttr(startsWithItem, "id", id)) q.startsWithItems.insert(id);
    }

    xr.findAttr(elem, "exclusiveToClass", q.exclusiveToClass);

    startingObject->addQuestStart(id);
    endingObject->addQuestEnd(id);
  }
}

void DataLoader::loadLootTables(XmlReader &xr) {
  for (auto elem : xr.getChildren("lootTable")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id))  // No ID: skip
      continue;

    auto lt = LootTable{};

    auto itemID = ""s;
    for (auto loot : xr.getChildren("loot", elem)) {
      if (!xr.findAttr(loot, "id", itemID)) continue;

      double mean = 1.0, sd = 0;
      std::set<ServerItem>::const_iterator itemIt =
          _server._items.insert(ServerItem(itemID)).first;
      if (xr.findAttr(loot, "chance", mean)) {
        lt.addSimpleItem(&*itemIt, mean);
      } else if (xr.findNormVarChild("normal", loot, mean, sd)) {
        lt.addNormalItem(&*itemIt, mean, sd);
      } else {
        lt.addSimpleItem(&*itemIt, 1.0);
      }
    }

    _server._standaloneLootTables[id] = lt;
  }
}

void DataLoader::loadNPCTemplates(XmlReader &xr) {
  for (auto elem : xr.getChildren("npcTemplate")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id))  // No ID: skip
      continue;
    auto collisionRect = MapRect{};
    xr.findRectChild("collisionRect", elem, collisionRect);

    _server._npcTemplates[id] = collisionRect;
  }
}

void DataLoader::loadNPCTypes(XmlReader &xr) {
  for (auto elem : xr.getChildren("npcType")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id))  // No ID: skip
      continue;

    NPCType *nt = dynamic_cast<NPCType *>(_server.findObjectTypeByID(id));
    if (!nt) {
      nt = new NPCType(id);
      _server._objectTypes.insert(nt);
    }

    auto templateID = ""s;
    if (xr.findAttr(elem, "template", templateID))
      nt->applyTemplate(templateID);

    auto humanoid = xr.findChild("humanoid", elem);

    // Collision rect
    if (humanoid) nt->collisionRect(User::OBJECT_TYPE.collisionRect());
    MapRect r;
    if (xr.findRectChild("collisionRect", elem, r)) nt->collisionRect(r);

    nt->loadTagsFromXML(xr, elem);

    auto s = ""s;
    if (xr.findAttr(elem, "allowedTerrain", s)) nt->allowedTerrain(s);

    auto level = Level{1};
    xr.findAttr(elem, "level", level);
    nt->level(level);

    auto school = ""s;
    if (xr.findAttr(elem, "school", school)) nt->school(school);

    auto n = 0;
    if (xr.findAttr(elem, "isRanged", n) && n != 0) nt->makeRanged();
    if (xr.findAttr(elem, "isCivilian", n) && n != 0)
      nt->makeCivilian();
    else if (xr.findAttr(elem, "isNeutral", n) && n != 0)
      nt->makeNeutral();

    // Taming
    auto canBeTamed = xr.findChild("canBeTamed", elem);
    if (canBeTamed) {
      nt->canBeTamed(true);
      if (xr.findAttr(canBeTamed, "consumes", s)) nt->tamingRequiresItem(s);
    }

    // Gathering
    if (xr.findAttr(elem, "gatherReq", s)) nt->yield.requiresTool(s);
    if (xr.findAttr(elem, "gatherTime", n)) nt->yield.gatherTime(n);

    if (xr.findAttr(elem, "maxDistanceFromSpawner", n))
      nt->maxDistanceFromSpawner(n);

    Stats baseStats = NPCType::BASE_STATS;
    xr.findAttr(elem, "maxHealth", baseStats.maxHealth);
    xr.findAttr(elem, "attack", baseStats.physicalDamage);
    xr.findAttr(elem, "attackTime", baseStats.attackTime);
    xr.findAttr(elem, "speed", baseStats.speed);
    xr.findAttr(elem, "armor", baseStats.armor);
    nt->baseStats(baseStats);

    auto elemSpell = xr.findChild("spell", elem);
    if (elemSpell) {
      auto spellID = ""s;
      if (xr.findAttr(elemSpell, "id", spellID)) nt->knowsSpell(spellID);
    }

    nt->yield.loadFromXML(xr, elem);
    nt->transformation.loadFromXML<NPCType>(xr, elem);

    for (auto loot : xr.getChildren("loot", elem)) {
      if (!xr.findAttr(loot, "id", s)) continue;

      double mean = 1.0, sd = 0;
      std::set<ServerItem>::const_iterator itemIt =
          _server._items.insert(ServerItem(s)).first;
      if (xr.findAttr(loot, "chance", mean)) {
        nt->addSimpleLoot(&*itemIt, mean);
      } else if (xr.findNormVarChild("normal", loot, mean, sd)) {
        nt->addNormalLoot(&*itemIt, mean, sd);
      } else {
        nt->addSimpleLoot(&*itemIt, 1.0);
      }
    }

    for (auto loot : xr.getChildren("lootTable", elem)) {
      if (!xr.findAttr(loot, "id", s)) continue;
      auto it = _server._standaloneLootTables.find(s);
      if (it == _server._standaloneLootTables.end()) {
        _server._debug << Color::CHAT_ERROR << "Standalone loot table " << s
                       << " doesn't exist; skipping." << Log::endl;
        continue;
      }
      nt->addLootTable(it->second);
    }
  }
}

void DataLoader::loadItems(XmlReader &xr) {
  for (auto elem : xr.getChildren("item")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID and name are mandatory.
    ServerItem item(id);

    auto stackSize = size_t{};
    if (xr.findAttr(elem, "stackSize", stackSize)) item.stackSize(stackSize);

    std::string s;
    if (xr.findAttr(elem, "constructs", s)) {
      // Create dummy ObjectType if necessary
      const ObjectType *ot = _server.findObjectTypeByID(s);
      if (ot != nullptr)
        item.constructsObject(ot);
      else
        item.constructsObject(
            *(_server._objectTypes.insert(new ObjectType(s)).first));
    }

    // Assumption: any item that returns on construction cannot be stacked.
    // Until there's a use case to the contrary, this simplifies the code.
    if (xr.findAttr(elem, "returnsOnConstruction", s)) {
      // Create dummy Item if necessary
      auto dummy = ServerItem{s};
      const auto *itemToReturn = &*_server._items.insert(dummy).first;
      item.returnsOnConstruction(itemToReturn);
    }

    if (xr.findAttr(elem, "castsSpellOnUse", s)) item.castsSpellOnUse(s);

    if (xr.findAttr(elem, "returnsOnCast", s)) {
      // Create dummy Item if necessary
      auto dummy = ServerItem{s};
      const auto *itemToReturn = &*_server._items.insert(dummy).first;
      item.returnsOnCast(itemToReturn);
    }

    auto stats = StatsMod{};
    if (xr.findStatsChild("stats", elem, stats)) item.stats(stats);

    if (xr.findAttr(elem, "exclusiveToQuest", s)) item.exclusiveToQuest(s);

    auto weaponElem = xr.findChild("weapon", elem);
    if (weaponElem != nullptr) {
      auto damage = Hitpoints{0};
      if (!xr.findAttr(weaponElem, "damage", damage)) {
        _server._debug("Item "s + id + " is missing weapon damage"s,
                       Color::CHAT_ERROR);
        continue;
      }

      auto speedInS = 0.0;
      if (!xr.findAttr(weaponElem, "speed", speedInS)) {
        _server._debug("Item "s + id + " is missing weapon speed"s,
                       Color::CHAT_ERROR);
        continue;
      }

      auto school = ""s;
      xr.findAttr(weaponElem, "school", school);

      item.makeWeapon(damage, speedInS, {school});

      auto range = Podes{};
      if (xr.findAttr(weaponElem, "range", range)) item.weaponRange(range);

      auto ammoType = ""s;
      if (xr.findAttr(weaponElem, "consumes", ammoType))
        item.weaponAmmo({ammoType});
    }

    int n;
    n = User::GEAR_SLOTS;  // Default; won't match any slot.
    xr.findAttr(elem, "gearSlot", n);
    item.gearSlot(n);

    item.loadTagsFromXML(xr, elem);

    if (xr.findAttr(elem, "durability", n)) item.durability(n);

    auto repairElem = xr.findChild("canBeRepaired", elem);
    if (repairElem) {
      item.makeRepairable();
      if (xr.findAttr(repairElem, "cost", s)) item.repairingCosts(s);
      if (xr.findAttr(repairElem, "tool", s)) item.repairingRequiresTool(s);
    }

    item.loaded();

    std::pair<std::set<ServerItem>::iterator, bool> ret =
        _server._items.insert(item);
    if (!ret.second) {
      ServerItem &itemInPlace = const_cast<ServerItem &>(*ret.first);
      itemInPlace = item;
    }
  }
}

void DataLoader::loadRecipes(XmlReader &xr) {
  for (auto elem : xr.getChildren("recipe")) {
    std::string id, name;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory.
    SRecipe recipe(id);

    std::string product = id;
    xr.findAttr(elem, "product", product);
    auto it = _server._items.find(product);
    if (it == _server._items.end()) {
      _server._debug << Color::CHAT_ERROR
                     << "Skipping recipe with invalid product " << product
                     << Log::endl;
      continue;
    }
    recipe.product(&*it);

    int n;
    if (xr.findAttr(elem, "quantity", n)) recipe.quantity(n);
    if (xr.findAttr(elem, "time", n)) recipe.time(n);

    for (auto child : xr.getChildren("material", elem)) {
      int quantity = 1;
      std::string matID;
      // Quantity
      xr.findAttr(child, "quantity", quantity);
      if (xr.findAttr(child, "id", matID)) {
        auto it = _server._items.find(ServerItem(matID));
        if (it == _server._items.end()) {
          _server._debug << Color::CHAT_ERROR
                         << "Skipping invalid recipe material " << matID
                         << Log::endl;
          continue;
        }
        recipe.addMaterial(&*it, quantity);
      }
    }

    std::string tag;
    for (auto child : xr.getChildren("tool", elem)) {
      if (xr.findAttr(child, "class", tag)) {
        recipe.addTool(tag);
      }
    }

    bool requiresUnlock = false;
    for (auto unlockedBy : xr.getChildren("unlockedBy", elem)) {
      double chance = 1.0;
      xr.findAttr(unlockedBy, "chance", chance);
      ProgressLock::Type triggerType;
      std::string triggerID;
      if (xr.findAttr(unlockedBy, "item", triggerID))
        triggerType = ProgressLock::ITEM;
      else if (xr.findAttr(unlockedBy, "construction", triggerID))
        triggerType = ProgressLock::CONSTRUCTION;
      else if (xr.findAttr(unlockedBy, "gather", triggerID))
        triggerType = ProgressLock::GATHER;
      else if (xr.findAttr(unlockedBy, "recipe", triggerID))
        triggerType = ProgressLock::RECIPE;
      else
        assert(false);
      ProgressLock(triggerType, triggerID, ProgressLock::RECIPE, id, chance)
          .stage();
      requiresUnlock = true;
    }
    if (!requiresUnlock) recipe.knownByDefault();

    _server._recipes.insert(recipe);
  }
}

void DataLoader::loadSpells(XmlReader &xr) {
  for (auto elem : xr.getChildren("spell")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory.
    auto newSpell = new Spell;
    _server._spells[id] = newSpell;
    newSpell->id(id);

    auto name = Spell::Name{};
    if (xr.findAttr(elem, "name", name)) newSpell->name(name);

    auto cost = Energy{};
    if (xr.findAttr(elem, "cost", cost)) newSpell->cost(cost);

    auto school = SpellSchool{};
    if (xr.findAttr(elem, "school", school)) newSpell->effect().school(school);

    auto range = Podes{0};
    if (xr.findAttr(elem, "range", range)) {
      newSpell->range(range);
      newSpell->effect().range(range);
    } else if (xr.findAttr(elem, "radius", range)) {
      newSpell->range(range);
      newSpell->effect().radius(range);
    }

    auto cooldownInSec = 0;
    if (xr.findAttr(elem, "cooldown", cooldownInSec))
      newSpell->cooldown(cooldownInSec * 1000);

    auto functionElem = xr.findChild("function", elem);
    if (functionElem) {
      auto functionName = ""s;
      if (xr.findAttr(functionElem, "name", functionName))
        newSpell->effect().setFunction(functionName);

      auto args = SpellEffect::Args{};
      xr.findAttr(functionElem, "i1", args.i1);
      xr.findAttr(functionElem, "s1", args.s1);
      xr.findAttr(functionElem, "d1", args.d1);

      newSpell->effect().args(args);
    }

    auto validTargets = xr.findChild("targets", elem);
    if (validTargets) {
      auto val = 0;
      if (xr.findAttr(validTargets, "self", val) && val != 0)
        newSpell->canTarget(Spell::SELF);
      if (xr.findAttr(validTargets, "friendly", val) && val != 0)
        newSpell->canTarget(Spell::FRIENDLY);
      if (xr.findAttr(validTargets, "enemy", val) && val != 0)
        newSpell->canTarget(Spell::ENEMY);
    }
  }
}

void DataLoader::loadBuffs(XmlReader &xr) {
  for (auto elem : xr.getChildren("buff")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory
    auto newBuff = BuffType{id};

    auto stats = StatsMod{};
    if (xr.findStatsChild("stats", elem, stats)) {
      newBuff.stats(stats);
    }

    auto durationInSeconds = 0;
    if (xr.findAttr(elem, "duration", durationInSeconds)) {
      newBuff.duration(durationInSeconds * 1000);
    }

    auto n = 0;
    if (xr.findAttr(elem, "canBeInterrupted", n) && n == 1)
      newBuff.makeInterruptible();
    if (xr.findAttr(elem, "cancelsOnOOE", n) && n == 1) newBuff.cancelOnOOE();

    auto school = SpellSchool{SpellSchool::PHYSICAL};
    auto schoolString = ""s;
    if (xr.findAttr(elem, "school", schoolString)) school = {schoolString};

    if (xr.findAttr(elem, "onNewPlayers", n) && n != 0)
      newBuff.giveToNewPlayers();

    auto nonStacking = xr.findChild("nonStacking", elem);
    if (nonStacking) {
      auto category = ""s;
      if (xr.findAttr(nonStacking, "category", category))
        newBuff.doesntStack(category);
    }

    do {
      auto changeAllowedTerrain = xr.findChild("changeAllowedTerrain", elem);
      if (!changeAllowedTerrain) break;

      auto listName = ""s;
      if (!xr.findAttr(changeAllowedTerrain, "terrainList", listName)) break;

      auto list = TerrainList::findList(listName);
      if (!list) break;

      newBuff.changesAllowedTerrain(list);

    } while (false);

    auto functionElem = xr.findChild("function", elem);
    if (functionElem) {
      auto functionName = ""s;
      if (xr.findAttr(functionElem, "name", functionName))
        newBuff.effect().setFunction(functionName);
      newBuff.effect().school(school);

      auto args = SpellEffect::Args{};
      xr.findAttr(functionElem, "i1", args.i1);
      xr.findAttr(functionElem, "s1", args.s1);

      auto tickTime = ms_t{};
      if (xr.findAttr(functionElem, "tickTime", tickTime)) {
        newBuff.effectOverTime();
        newBuff.tickTime(tickTime);
      }

      auto n = 0;
      if (xr.findAttr(functionElem, "onHit", n) && n != 0) {
        newBuff.effectOnHit();
      }

      assert(newBuff.hasType());

      newBuff.effect().args(args);
    }

    _server._buffTypes[id] = newBuff;
  }
}

void DataLoader::loadClasses(XmlReader &xr) {
  for (auto elem : xr.getChildren("class")) {
    std::string className;
    if (!xr.findAttr(elem, "name", className)) continue;  // ID is mandatory
    auto newClass = ClassType{className};

    for (auto tree : xr.getChildren("tree", elem)) {
      auto treeName = ""s;
      if (!xr.findAttr(tree, "name", treeName)) continue;

      for (auto tierElem : xr.getChildren("tier", tree)) {
        _server._tiers.push_back({});
        Tier &tier = _server._tiers.back();

        auto cost = xr.findChild("cost", tierElem);
        if (cost) {
          xr.findAttr(cost, "tag", tier.costTag);
          xr.findAttr(cost, "quantity", tier.costQuantity);
        }

        auto req = xr.findChild("requires", tierElem);
        if (req) {
          xr.findAttr(req, "pointsInTree", tier.reqPointsInTree);
          xr.findAttr(req, "tool", tier.requiredTool);
        }

        for (auto talent : xr.getChildren("talent", tierElem)) {
          auto type = ""s;
          if (!xr.findAttr(talent, "type", type)) continue;

          auto talentName = ""s;
          xr.findAttr(talent, "name", talentName);

          if (type == "spell") {
            auto spellID = Spell::ID{};
            if (!xr.findAttr(talent, "id", spellID)) continue;

            auto it = _server._spells.find(spellID);
            if (it == _server._spells.end()) continue;
            const auto &spell = *it->second;
            if (talentName.empty()) talentName = spell.name();

            Talent &t = newClass.addSpell(talentName, spellID, tier);

            t.tree(treeName);

          } else if (type == "stats") {
            if (talentName.empty()) continue;
            auto stats = StatsMod{};
            if (!xr.findStatsChild("stats", talent, stats)) continue;

            Talent &t = newClass.addStats(talentName, stats, tier);

            t.tree(treeName);
          }
        }
      }
    }

    _server._classes[className] = newClass;
  }
}

void DataLoader::loadSpawners(XmlReader &xr) {
  if (cmdLineArgs.contains("nospawn")) return;

  for (auto elem : xr.getChildren("spawnPoint")) {
    std::string id;
    if (!xr.findAttr(elem, "type", id)) {
      _server._debug("Skipping importing spawner with no type.",
                     Color::CHAT_ERROR);
      continue;
    }

    MapPoint p;
    if (!xr.findAttr(elem, "x", p.x) || !xr.findAttr(elem, "y", p.y)) {
      _server._debug("Skipping importing spawner with invalid/no location",
                     Color::CHAT_ERROR);
      continue;
    }

    const ObjectType *type = _server.findObjectTypeByID(id);
    if (type == nullptr) {
      _server._debug << Color::CHAT_ERROR
                     << "Skipping importing spawner for unknown objects \""
                     << id << "\"." << Log::endl;
      continue;
    }

    Spawner s(p, type);

    size_t n;
    if (xr.findAttr(elem, "quantity", n)) s.quantity(n);
    if (xr.findAttr(elem, "respawnTime", n)) s.respawnTime(n);
    double d;
    if (xr.findAttr(elem, "radius", d)) s.radius(d);

    char c;
    for (auto terrain : xr.getChildren("allowedTerrain", elem))
      if (xr.findAttr(terrain, "index", c)) s.allowTerrain(c);

    if (xr.findAttr(elem, "useCachedTerrain", n) && n == 1) s.useTerrainCache();

    s.initialise();
    _server._spawners.push_back(s);
  }
}

void DataLoader::loadMap(XmlReader &xr) {
  auto elem = xr.findChild("newPlayerSpawn");
  if (!xr.findAttr(elem, "x", User::newPlayerSpawn.x) ||
      !xr.findAttr(elem, "y", User::newPlayerSpawn.y)) {
    _server._debug("New-player spawn point missing or incomplete.",
                   Color::CHAT_ERROR);
  }
  if (!xr.findAttr(elem, "range", User::spawnRadius)) User::spawnRadius = 0;

  elem = xr.findChild("postTutorialSpawn");
  if (!xr.findAttr(elem, "x", User::postTutorialSpawn.x) ||
      !xr.findAttr(elem, "y", User::postTutorialSpawn.y)) {
    _server._debug("Post-tutorial spawn point missing or incomplete.",
                   Color::CHAT_ERROR);
  }

  _server._map.loadFromXML(xr);
}
