#include "CDataLoader.h"

#include <set>

#include "../Podes.h"
#include "../TerrainList.h"
#include "../XmlReader.h"
#include "ClassInfo.h"
#include "Client.h"
#include "ClientBuff.h"
#include "ClientNPCType.h"
#include "ClientVehicleType.h"
#include "ParticleProfile.h"
#include "SoundProfile.h"
#include "Unlocks.h"

CDataLoader::CDataLoader(Client &client) : _client(client) {}

CDataLoader CDataLoader::FromPath(Client &client, const Directory &path) {
  auto loader = CDataLoader(client);
  loader._path = path;
  return loader;
}

CDataLoader CDataLoader::FromString(Client &client, const XML &data) {
  auto loader = CDataLoader(client);
  loader._data = data;
  return loader;
}

void CDataLoader::load(bool keepOldData) {
  if (!keepOldData) {
    _client.gameData.terrain.clear();
    _client._particleProfiles.clear();
    _client._soundProfiles.clear();
    _client._projectileTypes.clear();
    _client._objects.clear();
    Client::gameData.items.clear();
    _client._classes.clear();
    Client::gameData.recipes.clear();
  }

  _client.drawLoadingScreen("Loading data", 0.6);

  auto usingPath = !_path.empty();
  if (usingPath) {
    _files = findDataFiles();

    loadFromAllFiles(&CDataLoader::loadTerrain);
    loadFromAllFiles(&CDataLoader::loadTerrainLists);
    loadFromAllFiles(&CDataLoader::loadParticles);
    loadFromAllFiles(&CDataLoader::loadSounds);
    loadFromAllFiles(&CDataLoader::loadProjectiles);
    loadFromAllFiles(&CDataLoader::loadSpells);
    loadFromAllFiles(&CDataLoader::loadBuffs);

    for (const auto &file : _files) {
      auto xr = XmlReader::FromFile(file);
      _client._tagNames.readFromXML(xr);
    }

    loadFromAllFiles(&CDataLoader::loadObjectTypes);
    loadFromAllFiles(&CDataLoader::loadItems);
    loadFromAllFiles(&CDataLoader::loadClasses);
    loadFromAllFiles(&CDataLoader::loadRecipes);
    loadFromAllFiles(&CDataLoader::loadNPCTemplates);
    loadFromAllFiles(&CDataLoader::loadNPCTypes);
    loadFromAllFiles(&CDataLoader::loadQuests);

    _client.drawLoadingScreen("Loading map", 0.65);
    auto reader = XmlReader::FromFile(_path + "/map.xml");
    loadMap(reader);
  } else {
    auto reader = XmlReader::FromString(_data);
    if (!reader) {
      _client._dataLoaded = true;
      return;
    }
    loadTerrain(reader);
    loadTerrainLists(reader);
    loadParticles(reader);
    loadSounds(reader);
    loadProjectiles(reader);
    loadSpells(reader);
    loadBuffs(reader);
    _client._tagNames.readFromXML(reader);
    loadObjectTypes(reader);
    loadItems(reader);
    loadClasses(reader);
    loadRecipes(reader);
    loadNPCTemplates(reader);
    loadNPCTypes(reader);
    loadQuests(reader);
  }
  _client._dataLoaded = true;
}

void CDataLoader::loadFromAllFiles(LoadFunction load) {
  for (const auto &file : _files) {
    auto xr = XmlReader::FromFile(file);
    if (!xr) continue;
    (this->*load)(xr);
  }
}

CDataLoader::FilesList CDataLoader::findDataFiles() const {
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

void CDataLoader::loadTerrain(XmlReader &xr) {
  for (auto elem : xr.getChildren("terrain")) {
    char index;
    if (!xr.findAttr(elem, "index", index)) continue;
    std::string fileName;
    if (!xr.findAttr(elem, "id", fileName)) continue;
    xr.findAttr(elem, "imageFile", fileName);  // Supercedes id if present.
    int isTraversable = 1;
    xr.findAttr(elem, "isTraversable", isTraversable);
    int frames = 1, frameTime = 0;
    xr.findAttr(elem, "frames", frames);
    xr.findAttr(elem, "frameTime", frameTime);

    auto &terrain = _client.gameData.terrain[index] =
        ClientTerrain(fileName, frames, frameTime);
    terrain.loadTagsFromXML(xr, elem);
  }
}

void CDataLoader::loadTerrainLists(XmlReader &xr) {
  TerrainList::loadFromXML(xr);
}

void CDataLoader::loadParticles(XmlReader &xr) {
  for (auto elem : xr.getChildren("particleProfile")) {
    std::string s;
    if (!xr.findAttr(elem, "id", s))  // No ID: skip
      continue;
    ParticleProfile *profile = new ParticleProfile(s);
    double mean, sd;
    if (xr.findAttr(elem, "particlesPerSecond", mean))
      profile->particlesPerSecond(mean);
    if (xr.findAttr(elem, "gravityModifier", mean))
      profile->gravityModifer(mean);
    if (xr.findNormVarChild("particlesPerHit", elem, mean, sd))
      profile->particlesPerHit(mean, sd);
    if (xr.findNormVarChild("distance", elem, mean, sd))
      profile->distance(mean, sd);
    if (xr.findNormVarChild("altitude", elem, mean, sd))
      profile->altitude(mean, sd);
    if (xr.findNormVarChild("velocity", elem, mean, sd))
      profile->velocity(mean, sd);
    if (xr.findNormVarChild("fallSpeed", elem, mean, sd))
      profile->fallSpeed(mean, sd);
    if (xr.findNormVarChild("lifespan", elem, mean, sd))
      profile->lifespan(mean, sd);
    int n;
    if (xr.findAttr(elem, "noZDimension", n) && n != 0) profile->noZDimension();
    if (xr.findAttr(elem, "canBeUnderground", n) && n != 0)
      profile->canBeUnderground();
    if (xr.findAttr(elem, "alpha", n) && n != 0xff) profile->alpha(n);

    auto dirE = xr.findChild("direction", elem);
    if (dirE) {
      MapPoint direction;
      xr.findAttr(dirE, "x", direction.x);
      xr.findAttr(dirE, "y", direction.y);
      profile->direction(direction);
    }

    for (auto variety : xr.getChildren("variety", elem)) {
      if (!xr.findAttr(variety, "imageFile", s))
        continue;  // No image file specified; skip
      size_t count = 1;
      xr.findAttr(variety, "count", count);
      auto drawRect = ScreenRect{};
      if (xr.findRectChild("drawRect", variety, drawRect))
        profile->addVariety(s, count, drawRect, _client);
      else
        profile->addVariety(s, count, _client);
    }

    _client._particleProfiles.insert(profile);
  }
}

void CDataLoader::loadSounds(XmlReader &xr) {
  for (auto elem : xr.getChildren("soundProfile")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id))  // No ID: skip
      continue;
    auto resultPair = _client._soundProfiles.insert(SoundProfile(id));
    SoundProfile &sp = const_cast<SoundProfile &>(*resultPair.first);
    if (id == "avatar")
      _client._avatarSounds = &sp;
    else if (id == "general")
      _client._generalSounds = &sp;

    ms_t period;
    if (xr.findAttr(elem, "period", period)) sp.period(period);

    for (auto sound : xr.getChildren("sound", elem)) {
      std::string type, file;
      if (!xr.findAttr(sound, "type", type) ||
          !xr.findAttr(sound, "file", file))
        continue;
      sp.add(type, file);
    }
  }
}

void CDataLoader::loadProjectiles(XmlReader &xr) {
  for (auto elem : xr.getChildren("projectile")) {
    auto id = ""s;
    if (!xr.findAttr(elem, "id", id)) continue;

    auto drawRect = ScreenRect{};
    if (!xr.findRectChild("drawRect", elem, drawRect)) continue;

    Projectile::Type *projectile = new Projectile::Type(id, drawRect, &_client);

    xr.findAttr(elem, "speed", projectile->speed);
    xr.findAttr(elem, "particlesAtEnd", projectile->particlesAtEnd);

    auto tailElem = xr.findChild("tail", elem);
    if (tailElem) {
      auto imageFile = ""s;
      xr.findAttr(tailElem, "image", imageFile);

      auto drawRect = ScreenRect{};
      xr.findAttr(tailElem, "x", drawRect.x);
      xr.findAttr(tailElem, "y", drawRect.y);
      xr.findAttr(tailElem, "w", drawRect.w);
      xr.findAttr(tailElem, "h", drawRect.h);

      auto length = 1;
      xr.findAttr(tailElem, "length", length);

      auto separation = 1;
      xr.findAttr(tailElem, "separation", separation);

      auto tailParticles = ""s;
      xr.findAttr(tailElem, "particles", tailParticles);

      projectile->tail(imageFile, drawRect, length, separation, tailParticles);
    }

    auto sounds = ""s;
    if (xr.findAttr(elem, "sounds", sounds)) projectile->sounds(sounds);

    _client._projectileTypes.insert(projectile);
  }
}

void CDataLoader::loadSpells(XmlReader &xr) {
  for (auto elem : xr.getChildren("spell")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory.
    auto newSpell = new ClientSpell(id, _client);
    _client._spells[id] = newSpell;

    auto icon = ""s;
    if (xr.findAttr(elem, "icon", icon)) newSpell->icon(_client._icons[icon]);

    auto name = ""s;
    if (xr.findAttr(elem, "name", name)) newSpell->name(name);

    auto cost = Energy{};
    if (xr.findAttr(elem, "cost", cost)) newSpell->cost(cost);

    auto school = SpellSchool{};
    if (xr.findAttr(elem, "school", school)) newSpell->school(school);

    auto range = Podes{0};
    if (xr.findAttr(elem, "range", range))
      newSpell->range(range);
    else if (xr.findAttr(elem, "radius", range))
      newSpell->radius(range);

    auto cooldown = 0;
    if (xr.findAttr(elem, "cooldown", cooldown)) newSpell->cooldown(cooldown);

    auto customDescription = ""s;
    if (xr.findAttr(elem, "customDescription", customDescription))
      newSpell->customDescription(customDescription);

    auto functionElem = xr.findChild("function", elem);
    if (functionElem) {
      auto functionName = ""s;
      if (xr.findAttr(functionElem, "name", functionName))
        newSpell->effectName(functionName);

      auto effectArgs = ClientSpell::Args{};
      xr.findAttr(functionElem, "i1", effectArgs.i1);
      xr.findAttr(functionElem, "s1", effectArgs.s1);
      xr.findAttr(functionElem, "d1", effectArgs.d1);

      newSpell->effectArgs(effectArgs);
    }

    auto aesthetics = xr.findChild("aesthetics", elem);
    if (aesthetics) {
      auto profileName = ""s;
      if (xr.findAttr(aesthetics, "projectile", profileName)) {
        auto dummy = Projectile::Type{profileName, {}, &_client};
        auto it = _client._projectileTypes.find(&dummy);
        if (it != _client._projectileTypes.end()) newSpell->projectile(*it);
      }
      if (xr.findAttr(aesthetics, "sounds", profileName)) {
        auto profile = _client.findSoundProfile(profileName);
        if (profile != nullptr) newSpell->sounds(profile);
      }
      if (xr.findAttr(aesthetics, "impactParticles", profileName)) {
        auto profile = _client.findParticleProfile(profileName);
        if (profile != nullptr) newSpell->impactParticles(profile);
      }
    }

    // Contains many assumptions about allowed/forbidden combinations.
    auto validTargets = xr.findChild("targets", elem);
    assert(validTargets);
    if (validTargets) {
      auto val = 0;
      auto self = xr.findAttr(validTargets, "self", val) && val != 0;
      auto friendly = xr.findAttr(validTargets, "friendly", val) && val != 0;
      auto enemy = xr.findAttr(validTargets, "enemy", val) && val != 0;

      if (friendly) {
        assert(self);
        if (enemy)
          newSpell->targetType(ClientSpell::TargetType::ALL);
        else
          newSpell->targetType(ClientSpell::TargetType::FRIENDLY);
      } else if (enemy) {
        assert(!self);
        newSpell->targetType(ClientSpell::TargetType::ENEMY);
      } else {
        assert(self);
        newSpell->targetType(ClientSpell::TargetType::SELF);
      }
    }
  }
}

void CDataLoader::loadBuffs(XmlReader &xr) {
  for (auto elem : xr.getChildren("buff")) {
    auto id = ClientBuffType::ID{};
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory.

    auto iconFile = id;
    xr.findAttr(elem, "icon", iconFile);
    auto newBuff = ClientBuffType{iconFile};

    newBuff.id(id);

    auto name = ClientBuffType::Name{};
    if (xr.findAttr(elem, "name", name)) newBuff.name(name);

    auto description = ClientBuffType::Description{};
    if (xr.findAttr(elem, "description", description))
      newBuff.description(description);

    int n;
    if (xr.findAttr(elem, "duration", n)) newBuff.duration(n);

    if (xr.findAttr(elem, "cancelsOnOOE", n) && n == 1) newBuff.cancelOnOOE();

    auto particleProfileName = ""s;
    if (xr.findAttr(elem, "particles", particleProfileName))
      newBuff.particles(particleProfileName);

    auto effectElem = xr.findChild("effect", elem);
    if (effectElem) {
      auto filename = ""s;
      xr.findAttr(effectElem, "image", filename);
      filename = "Images/Effects/"s + filename + ".png";
      auto offset = ScreenPoint{};
      xr.findAttr(effectElem, "xOffset", offset.x);
      xr.findAttr(effectElem, "yOffset", offset.y);

      newBuff.effect({filename, Color::MAGENTA}, offset);
    }

    auto functionElem = xr.findChild("function", elem);
    if (functionElem) {
      auto effectName = ""s;
      if (xr.findAttr(functionElem, "name", effectName))
        newBuff.effectName(effectName);

      auto effectArgs = ClientSpell::Args{};
      xr.findAttr(functionElem, "i1", effectArgs.i1);
      xr.findAttr(functionElem, "s1", effectArgs.s1);
      xr.findAttr(functionElem, "d1", effectArgs.d1);

      newBuff.effectArgs(effectArgs);

      if (xr.findAttr(functionElem, "tickTime", n)) newBuff.tickTime(n);
      if (xr.findAttr(functionElem, "onHit", n) && n != 0)
        newBuff.hasHitEffect();
    }

    auto stats = StatsMod{};
    auto itemHasStats = xr.findStatsChild("stats", elem, stats);
    if (itemHasStats) newBuff.stats(stats);

    _client._buffTypes[id] = newBuff;
  }
}

void CDataLoader::loadObjectTypes(XmlReader &xr) {
  for (auto elem : xr.getChildren("objectType")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;
    int n;
    ClientObjectType *cot;
    auto isVehicle = (xr.findAttr(elem, "isVehicle", n) == 1);
    if (isVehicle)
      cot = new ClientVehicleType(id, _client);
    else
      cot = new ClientObjectType(id, _client);

    auto imageFile = id;
    xr.findAttr(elem, "imageFile", imageFile);
    cot->imageFile(imageFile);
    cot->setImage(std::string("Images/Objects/") + imageFile);
    cot->setCorpseImage(std::string("Images/Objects/") + imageFile +
                        "-corpse.png");

    auto customShadowWidth = 0_px;
    if (xr.findAttr(elem, "customShadowWidth", customShadowWidth))
      cot->useCustomShadowWidth(customShadowWidth);

    auto name = id;
    xr.findAttr(elem, "name", name);
    cot->name(name);
    ScreenRect drawRect(0, 0, cot->width(), cot->height());
    bool xSet = xr.findAttr(elem, "xDrawOffset", drawRect.x),
         ySet = xr.findAttr(elem, "yDrawOffset", drawRect.y);
    if (xSet || ySet) cot->drawRect(drawRect);
    if (xr.getChildren("yield", elem).size() > 0) cot->canGather(true);
    auto s = ""s;
    if (xr.findAttr(elem, "deconstructs", s)) cot->canDeconstruct(true);

    auto container = xr.findChild("container", elem);
    if (container != nullptr) {
      if (xr.findAttr(container, "slots", n)) cot->containerSlots(n);
    }

    cot->loadTagsFromXML(xr, elem);

    if (xr.findAttr(elem, "allowedTerrain", s)) cot->allowedTerrain(s);
    if (xr.findAttr(elem, "merchantSlots", n)) cot->merchantSlots(n);
    if (xr.findAttr(elem, "isFlat", n) && n != 0) cot->isFlat(true);
    if (xr.findAttr(elem, "isDecoration", n) && n != 0) cot->isDecoration(true);
    if (xr.findAttr(elem, "sounds", s)) cot->setSoundProfile(s, _client);
    if (xr.findAttr(elem, "gatherParticles", s))
      cot->gatherParticles(_client.findParticleProfile(s));
    if (xr.findAttr(elem, "damageParticles", s))
      cot->damageParticles(_client.findParticleProfile(s));
    if (xr.findAttr(elem, "gatherReq", s)) cot->gatherReq(s);
    if (xr.findAttr(elem, "constructionReq", s)) cot->constructionReq(s);
    if (xr.findAttr(elem, "constructionText", s)) cot->constructionText(s);
    MapRect r;
    if (xr.findRectChild("collisionRect", elem, r)) cot->collisionRect(r);
    if (xr.findAttr(elem, "collides", n)) cot->collides(n != 0);
    if (xr.findAttr(elem, "isGate", n)) cot->collides(false);

    if (xr.findAttr(elem, "playerUnique", s)) {
      cot->makePlayerUnique();
      cot->addTag(s + " (1 per player)", 1.0);
    }

    if (isVehicle) {
      auto vehicleSpeed = 0.0;
      if (xr.findAttr(elem, "vehicleSpeed", vehicleSpeed)) {
        auto &vt = dynamic_cast<ClientVehicleType &>(*cot);
        vt.setSpeed(vehicleSpeed);
      }
    }

    auto action = xr.findChild("action", elem);
    if (action != nullptr) {
      auto pAction = new ClientObjectAction;

      xr.findAttr(action, "label", pAction->label);
      xr.findAttr(action, "tooltip", pAction->tooltip);
      xr.findAttr(action, "textInput", pAction->textInput);

      auto costID = ""s;
      if (xr.findAttr(action, "cost", costID)) {
        pAction->cost = &Client::gameData.items[costID];
      }

      cot->action(pAction);
    }

    bool canConstruct = false;
    for (auto objMat : xr.getChildren("material", elem)) {
      if (!xr.findAttr(objMat, "id", s)) continue;
      ClientItem &item = Client::gameData.items[s];
      n = 1;
      xr.findAttr(objMat, "quantity", n);
      cot->addMaterial(&item, n);
      canConstruct = true;
    }
    if (xr.findAttr(elem, "isUnbuildable", n) == 1) canConstruct = false;
    if (canConstruct) {
      bool hasLocks = xr.findChild("unlockedBy", elem) != nullptr;
      if (!hasLocks) _client._knownConstructions.insert(id);
    }

    if (isVehicle) {
      auto driver = xr.findChild("driver", elem);
      if (driver != nullptr) {
        ClientVehicleType &vt = dynamic_cast<ClientVehicleType &>(*cot);
        vt.drawDriver(true);

        ScreenPoint offset;
        xr.findAttr(driver, "x", offset.x);
        xr.findAttr(driver, "y", offset.y);
        vt.driverOffset(offset);

        auto bottomCutoff = 0_px;
        if (xr.findAttr(driver, "cutOffBottom", bottomCutoff))
          vt.cutOffBottomOfDriver(bottomCutoff);
      }
    }

    auto transform = xr.findChild("transform", elem);
    if (transform) {
      if (xr.findAttr(transform, "time", n)) cot->transformTime(n);
      for (auto progress : xr.getChildren("progress", transform)) {
        if (xr.findAttr(progress, "image", s)) cot->addTransformImage(s);
      }
    }

    // Strength
    auto strength = xr.findChild("durability", elem);
    if (strength) {
      if (xr.findAttr(strength, "item", s) &&
          xr.findAttr(strength, "quantity", n)) {
        ClientItem &item = Client::gameData.items[s];
        cot->durability(&item, n);
      } else
        _client.showErrorMessage(
            "Transformation specified without target id; skipping.",
            Color::CHAT_ERROR);
    }

    // Repairing
    auto repairElem = xr.findChild("canBeRepaired", elem);
    if (repairElem) {
      cot->makeRepairable();
      if (xr.findAttr(repairElem, "cost", s)) cot->repairingCosts(s);
      if (xr.findAttr(repairElem, "tool", s)) cot->repairingRequiresTool(s);
    }

    for (auto particles : xr.getChildren("particles", elem)) {
      auto profileName = ""s;
      if (!xr.findAttr(particles, "profile", profileName)) continue;
      MapPoint offset{};
      xr.findAttr(particles, "x", offset.x);
      xr.findAttr(particles, "y", offset.y);
      cot->addParticles(profileName, offset);
    }

    // Gathering yields (used to show unlock chances)
    for (auto yield : xr.getChildren("yield", elem)) {
      if (!xr.findAttr(yield, "id", s)) continue;
      auto chance = 1.0;
      xr.findAttr(yield, "gatherMean", chance);
      cot->chanceToGather(s, chance);
    }

    Client::gameData.objectTypes.insert(cot);

    // Construction locks
    for (auto unlockedBy : xr.getChildren("unlockedBy", elem)) {
      double chance = 1.0;
      xr.findAttr(unlockedBy, "chance", chance);

      auto triggerID = ""s;
      Unlocks::TriggerType triggerType;
      if (xr.findAttr(unlockedBy, "item", triggerID))
        triggerType = Unlocks::ACQUIRE;
      else if (xr.findAttr(unlockedBy, "construction", triggerID))
        triggerType = Unlocks::CONSTRUCT;
      else if (xr.findAttr(unlockedBy, "gather", triggerID))
        triggerType = Unlocks::GATHER;
      else if (xr.findAttr(unlockedBy, "recipe", triggerID))
        triggerType = Unlocks::CRAFT;
      else
        continue;  // Not a real lock, but blocks being known by default
      Unlocks::add({triggerType, triggerID}, {Unlocks::CONSTRUCTION, id},
                   chance);
    }

    auto questID = ""s;
    if (xr.findAttr(elem, "exclusiveToQuest", questID))
      cot->exclusiveToQuest(questID);
  }
}

void CDataLoader::loadItems(XmlReader &xr) {
  for (auto elem : xr.getChildren("item")) {
    std::string id, name;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory.
    if (!xr.findAttr(elem, "name", name)) name = id;
    ClientItem item(_client, id, name);
    std::string s;

    item.loadTagsFromXML(xr, elem);

    if (xr.findAttr(elem, "iconFile", s))
      item.icon(s);
    else
      item.icon(id);
    if (xr.findAttr(elem, "gearFile", s))
      item.gearImage(s);
    else
      item.gearImage(id);

    if (xr.findAttr(elem, "sounds", s)) item.setSoundProfile(s, _client);

    Hitpoints durability;
    if (xr.findAttr(elem, "durability", durability))
      item.durability(durability);

    auto quality = 0;
    if (xr.findAttr(elem, "quality", quality)) item.quality(quality);

    auto offset = xr.findChild("offset", elem);
    if (offset != nullptr) {
      ScreenPoint drawLoc;
      xr.findAttr(offset, "x", drawLoc.x);
      xr.findAttr(offset, "y", drawLoc.y);
      item.drawLoc(drawLoc);
    }

    auto stats = StatsMod{};
    auto itemHasStats = xr.findStatsChild("stats", elem, stats);
    if (itemHasStats) item.stats(stats);

    auto weaponElem = xr.findChild("weapon", elem);
    if (weaponElem != nullptr) {
      auto damage = Hitpoints{0};
      xr.findAttr(weaponElem, "damage", damage);

      auto speedInS = 0.0;
      xr.findAttr(weaponElem, "speed", speedInS);

      auto school = ""s;
      xr.findAttr(weaponElem, "school", school);

      item.makeWeapon(damage, speedInS, {school});

      auto range = Podes{};
      if (xr.findAttr(weaponElem, "range", range)) item.weaponRange(range);

      auto ammoType = ""s;
      if (xr.findAttr(weaponElem, "consumes", ammoType))
        item.weaponAmmo({ammoType});

      auto projectile = ""s;
      if (xr.findAttr(weaponElem, "projectile", projectile))
        item.projectile(_client.findProjectileType(projectile));
    }

    size_t gearSlot = _client.GEAR_SLOTS;  // Default; won't match any slot.
    xr.findAttr(elem, "gearSlot", gearSlot);
    item.gearSlot(gearSlot);

    auto lvlReq = Level{};
    if (xr.findAttr(elem, "lvlReq", lvlReq)) item.lvlReq(lvlReq);

    if (xr.findAttr(elem, "constructs", s)) {
      // Create dummy ObjectType if necessary
      auto pair =
          Client::gameData.objectTypes.insert(new ClientObjectType(s, _client));
      item.constructsObject(*pair.first);
    }

    if (xr.findAttr(elem, "castsSpellOnUse", s)) item.castsSpellOnUse(s);

    for (auto particles : xr.getChildren("particles", elem)) {
      auto profileName = ""s;
      if (!xr.findAttr(particles, "profile", profileName)) continue;
      MapPoint offset{};
      xr.findAttr(particles, "x", offset.x);
      xr.findAttr(particles, "y", offset.y);
      item.addParticles(profileName, offset);
    }

    auto repairElem = xr.findChild("canBeRepaired", elem);
    if (repairElem) {
      item.makeRepairable();
      if (xr.findAttr(repairElem, "cost", s)) item.repairingCosts(s);
      if (xr.findAttr(repairElem, "tool", s)) item.repairingRequiresTool(s);
    }

    Client::gameData.items[id] = item;
  }
}

void CDataLoader::loadClasses(XmlReader &xr) {
  for (auto elem : xr.getChildren("class")) {
    auto className = ClassInfo::Name{};
    if (!xr.findAttr(elem, "name", className)) continue;  // Name is mandatory

    auto newClass = ClassInfo{className};

    auto description = ""s;
    if (xr.findAttr(elem, "description", description))
      newClass.description(description);

    for (auto tree : xr.getChildren("tree", elem)) {
      auto treeName = ClassInfo::Name{};
      if (!xr.findAttr(tree, "name", treeName)) continue;  // Name is mandatory
      newClass.ensureTreeExists(treeName);

      auto currentTier = 0;
      for (auto tier : xr.getChildren("tier", tree)) {
        auto cost = xr.findChild("cost", tier);
        auto costTag = ""s;
        auto costQty = 0;
        auto reqTool = ""s;
        if (cost) {
          xr.findAttr(cost, "tag", costTag);
          xr.findAttr(cost, "quantity", costQty);
        }
        auto req = xr.findChild("requires", tier);
        auto reqPointsInTree = 0;
        if (req) {
          xr.findAttr(req, "pointsInTree", reqPointsInTree);
          xr.findAttr(req, "tool", reqTool);
        }

        for (auto talent : xr.getChildren("talent", tier)) {
          auto t = ClientTalent{};

          auto typeName = ""s;
          if (!xr.findAttr(talent, "type", typeName)) continue;

          xr.findAttr(talent, "name", t.name);
          xr.findAttr(talent, "flavourText", t.flavourText);

          t.costTag = costTag;
          t.costQuantity = costQty;
          t.reqTool = reqTool;
          t.reqPointsInTree = reqPointsInTree;
          t.tree = treeName;

          if (typeName == "spell") {
            t.type = ClientTalent::SPELL;

            auto spellID = ""s;
            if (!xr.findAttr(talent, "id", spellID)) continue;
            auto it = _client._spells.find(spellID);
            if (it == _client._spells.end()) continue;
            t.spell = it->second;

            t.icon = t.spell->icon();

            if (t.name.empty()) t.name = t.spell->name();

          } else if (typeName == "stats") {
            if (t.name.empty()) continue;

            t.type = ClientTalent::STATS;

            auto icon = ""s;
            if (xr.findAttr(talent, "icon", icon))
              t.icon = _client._icons[icon];

            auto stats = StatsMod{};
            if (!xr.findStatsChild("stats", talent, stats)) continue;
            t.stats = stats;
          }

          t.generateLearnMessage();
          newClass.addTalentToTree(t, treeName, currentTier);
        }

        ++currentTier;
      }
    }

    _client._classes[className] = std::move(newClass);
  }
}

void CDataLoader::loadRecipes(XmlReader &xr) {
  for (auto elem : xr.getChildren("recipe")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id)) continue;  // ID is mandatory.
    CRecipe recipe(id);

    std::string s = id;
    xr.findAttr(elem, "product", s);
    auto it = Client::gameData.items.find(s);
    if (it == Client::gameData.items.end()) {
      _client.showErrorMessage("Skipping recipe with invalid product "s + s,
                               Color::CHAT_ERROR);
      continue;
    }
    const ClientItem *item = &it->second;
    recipe.product(item);

    auto name = item->name();
    xr.findAttr(elem, "name", name);
    recipe.name(name);

    if (xr.findAttr(elem, "sounds", s)) recipe.setSoundProfile(s, _client);

    size_t n;
    if (xr.findAttr(elem, "quantity", n)) recipe.quantity(n);

    for (auto child : xr.getChildren("material", elem)) {
      int matQty = 1;
      xr.findAttr(child, "quantity", matQty);
      if (xr.findAttr(child, "id", s)) {
        auto it = Client::gameData.items.find(s);
        if (it == Client::gameData.items.end()) {
          _client.showErrorMessage("Skipping invalid recipe material "s + s,
                                   Color::CHAT_ERROR);
          continue;
        }
        const ClientItem *material = &it->second;
        recipe.addMaterial(material, matQty);
      }
    }

    for (auto child : xr.getChildren("tool", elem)) {
      if (xr.findAttr(child, "class", s)) {
        recipe.addTool(s);
      }
    }

    if (xr.getChildren("unlockedBy", elem).empty())
      _client._knownRecipes.insert(id);

    Client::gameData.recipes.insert(recipe);

    // Crafting locks
    for (auto unlockedBy : xr.getChildren("unlockedBy", elem)) {
      double chance = 1.0;
      xr.findAttr(unlockedBy, "chance", chance);

      auto triggerID = ""s;
      Unlocks::TriggerType triggerType;
      if (xr.findAttr(unlockedBy, "item", triggerID))
        triggerType = Unlocks::ACQUIRE;
      else if (xr.findAttr(unlockedBy, "construction", triggerID))
        triggerType = Unlocks::CONSTRUCT;
      else if (xr.findAttr(unlockedBy, "gather", triggerID))
        triggerType = Unlocks::GATHER;
      else if (xr.findAttr(unlockedBy, "recipe", triggerID))
        triggerType = Unlocks::CRAFT;
      else
        continue;  // Not a real lock, but blocks being known by default
      Unlocks::add({triggerType, triggerID}, {Unlocks::RECIPE, id}, chance);
    }
  }
}

void CDataLoader::loadNPCTemplates(XmlReader &xr) {
  for (auto elem : xr.getChildren("npcTemplate")) {
    std::string id;
    if (!xr.findAttr(elem, "id", id))  // No ID: skip
      continue;

    auto nt = CNPCTemplate{};
    xr.findRectChild("collisionRect", elem, nt.collisionRect);
    xr.findAttr(elem, "imageFile", nt.imageFile);
    xr.findAttr(elem, "sounds", nt.soundProfile);
    xr.findAttr(elem, "xDrawOffset", nt.xDrawOffset);
    xr.findAttr(elem, "yDrawOffset", nt.yDrawOffset);

    Client::gameData.npcTemplates[id] = nt;
  }
}

void CDataLoader::loadNPCTypes(XmlReader &xr) {
  for (auto elem : xr.getChildren("npcType")) {
    auto id = ""s;
    if (!xr.findAttr(elem, "id", id))  // No ID: skip
      continue;

    const CNPCTemplate *npcTemplate = nullptr;
    auto templateID = ""s;
    if (xr.findAttr(elem, "template", templateID))
      npcTemplate = _client.findNPCTemplate(templateID);

    auto humanoid = xr.findChild("humanoid", elem);

    auto maxHealth = 1;
    xr.findAttr(elem, "maxHealth", maxHealth);

    auto imagePath = ""s;
    if (humanoid) {
      auto baseImage = "default"s;
      xr.findAttr(humanoid, "base", baseImage);
      imagePath = "Images/Humans/"s + baseImage;
    } else {
      auto imageFile = id;
      if (npcTemplate && !npcTemplate->imageFile.empty())
        imageFile = npcTemplate->imageFile;
      xr.findAttr(elem, "imageFile",
                  imageFile);  // If no explicit imageFile, will still == id
      imagePath = "Images/NPCs/"s + imageFile;
    }

    ClientNPCType *nt = new ClientNPCType(id, imagePath, maxHealth, _client);
    auto drawRect = ScreenRect{0, 0, nt->width(), nt->height()};

    if (npcTemplate) {
      nt->applyTemplate(npcTemplate);
      drawRect.x = npcTemplate->xDrawOffset;
      drawRect.y = npcTemplate->yDrawOffset;
    }

    auto s = id;
    xr.findAttr(elem, "name", s);
    nt->name(s);

    // Draw rect
    if (humanoid) {
      nt->drawRect(Avatar::DRAW_RECT);
      nt->useCustomShadowWidth(16);
      nt->useCustomDrawHeight(50);
    } else {
      xr.findAttr(elem, "xDrawOffset", drawRect.x);
      xr.findAttr(elem, "yDrawOffset", drawRect.y);
      nt->drawRect(drawRect);
    }

    auto customShadowDiameter = 0_px;
    if (xr.findAttr(elem, "customShadowWidth", customShadowDiameter))
      nt->useCustomShadowWidth(customShadowDiameter);

    auto n = 0;
    if (xr.findAttr(elem, "isFlat", n) && n != 0) nt->isFlat(true);

    // Collision rect
    if (humanoid) nt->collisionRect(Avatar::COLLISION_RECT);
    auto r = MapRect{};
    if (xr.findRectChild("collisionRect", elem, r)) nt->collisionRect(r);

    if (xr.findAttr(elem, "sounds", s))
      nt->setSoundProfile(s, _client);
    else if (humanoid)
      nt->setSoundProfile("humanEnemy", _client);

    if (xr.findAttr(elem, "projectile", s)) {
      auto dummy = Projectile::Type{s, {}, &_client};
      auto it = _client._projectileTypes.find(&dummy);
      if (it != _client._projectileTypes.end()) nt->projectile(**it);
    }

    nt->loadTagsFromXML(xr, elem);

    for (auto particles : xr.getChildren("particles", elem)) {
      auto profileName = ""s;
      if (!xr.findAttr(particles, "profile", profileName)) continue;
      MapPoint offset{};
      xr.findAttr(particles, "x", offset.x);
      xr.findAttr(particles, "y", offset.y);
      nt->addParticles(profileName, offset);
    }

    for (auto gearElem : xr.getChildren("gear", humanoid)) {
      auto id = ""s;
      if (!xr.findAttr(gearElem, "id", id)) continue;

      auto it = Client::gameData.items.find(id);
      if (it == Client::gameData.items.end()) {
        _client.showErrorMessage("Skipping invalid NPC gear "s + id,
                                 Color::CHAT_ERROR);
        continue;
      }

      nt->addGear(it->second);
    }

    if (xr.findAttr(elem, "isCivilian", n) && n != 0)
      nt->makeCivilian();
    else if (xr.findAttr(elem, "isNeutral", n) && n != 0)
      nt->makeNeutral();

    auto canBeTamed = xr.findChild("canBeTamed", elem);
    if (canBeTamed) {
      nt->canBeTamed(true);
      if (xr.findAttr(canBeTamed, "consumes", s)) {
        auto it = Client::gameData.items.find(s);
        if (it == Client::gameData.items.end()) {
          _client.showErrorMessage("Skipping invalid taming consumable "s + s,
                                   Color::CHAT_ERROR);
          continue;
        }

        nt->tamingRequiresItem(&it->second);
      }
    }

    // Gathering
    if (xr.getChildren("yield", elem).size() > 0) nt->canGather(true);
    if (xr.findAttr(elem, "gatherReq", s)) nt->gatherReq(s);
    // Gathering yields (used to show unlock chances)
    for (auto yield : xr.getChildren("yield", elem)) {
      if (!xr.findAttr(yield, "id", s)) continue;
      auto chance = 1.0;
      xr.findAttr(yield, "gatherMean", chance);
      nt->chanceToGather(s, chance);
    }

    if (xr.findAttr(elem, "elite", n) && n != 0) {
      nt->makeElite();
    }

    // Insert
    auto pair = Client::gameData.objectTypes.insert(nt);
    if (!pair.second) {
      // A ClientObjectType is being pointed to by items; they need to point to
      // this instead.
      const ClientObjectType *dummy = *pair.first;
      for (const auto &pair : Client::gameData.items) {
        const ClientItem &item = pair.second;
        if (item.constructsObject() == dummy) {
          ClientItem &nonConstItem = const_cast<ClientItem &>(item);
          nonConstItem.constructsObject(nt);
        }
      }
      Client::gameData.objectTypes.erase(dummy);
      delete dummy;
      Client::gameData.objectTypes.insert(nt);
    }
  }
}

void CDataLoader::loadQuests(XmlReader &xr) {
  for (auto elem : xr.getChildren("quest")) {
    CQuest::Info questInfo;

    if (!xr.findAttr(elem, "id", questInfo.id)) continue;  // ID is mandatory.

    if (!xr.findAttr(elem, "startsAt", questInfo.startsAt))
      continue;  // Start node is mandatory

    if (!xr.findAttr(elem, "endsAt", questInfo.endsAt))
      continue;  // End node is mandatory

    xr.findAttr(elem, "name", questInfo.name);
    xr.findAttr(elem, "brief", questInfo.brief);
    xr.findAttr(elem, "debrief", questInfo.debrief);
    xr.findAttr(elem, "helpTopicOnAccept", questInfo.helpTopicOnAccept);
    xr.findAttr(elem, "helpTopicOnComplete", questInfo.helpTopicOnComplete);

    for (auto objectiveElem : xr.getChildren("objective", elem)) {
      auto objective = CQuest::Info::Objective{};

      auto id = ""s;
      xr.findAttr(objectiveElem, "id", id);

      auto type = ""s;
      xr.findAttr(objectiveElem, "type", type);

      if (type == "kill") {
        auto npcType = _client.findNPCType(id);
        objective.text = "Kill "s + (npcType ? npcType->name() : "???"s);

      } else if (type == "construct") {
        auto objType = _client.findObjectType(id);
        objective.text = "Construct "s + (objType ? objType->name() : "???"s);

      } else if (type == "fetch") {
        auto &it = Client::gameData.items.find(id);
        objective.text = it->second.name();

      } else if (type == "cast") {
        auto &it = _client._spells.find(id);
        objective.text = it->second->name();
      }

      objective.qty = 1;
      xr.findAttr(objectiveElem, "qty", objective.qty);

      questInfo.objectives.push_back(objective);
    }

    for (auto rewardElem : xr.getChildren("reward", elem)) {
      auto typeString = ""s;
      xr.findAttr(rewardElem, "type", typeString);
      auto type = CQuest::Info::Reward::Type{};
      if (typeString == "spell")
        type = CQuest::Info::Reward::LEARN_SPELL;
      else if (typeString == "construction")
        type = CQuest::Info::Reward::LEARN_CONSTRUCTION;
      else if (typeString == "item")
        type = CQuest::Info::Reward::RECEIVE_ITEM;

      auto reward = CQuest::Info::Reward{};
      reward.type = type;

      xr.findAttr(rewardElem, "id", reward.id);
      xr.findAttr(rewardElem, "qty", reward.itemQuantity);

      questInfo.rewards.push_back(reward);
    }

    _client._quests.insert(
        std::make_pair(questInfo.id, CQuest{_client, questInfo}));
  }
}

void CDataLoader::loadMap(XmlReader &xr) {
  _client._map.loadFromXML(xr);

  auto chunksX = _client._map.width() / Client::TILES_PER_CHUNK;
  auto chunksY = _client._map.height() / Client::TILES_PER_CHUNK;
  _client._mapExplored = {chunksX, std::vector<bool>(chunksY, false)};
  _client._fogOfWar = {static_cast<px_t>(chunksX), static_cast<px_t>(chunksY)};
  _client._fogOfWar.setBlend();
  _client.redrawFogOfWar();
}
