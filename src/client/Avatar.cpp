#include "Avatar.h"

#include "../util.h"
#include "Client.h"
#include "Renderer.h"
#include "SoundProfile.h"
#include "Tooltip.h"
#include "ui/List.h"

extern Renderer renderer;

const ScreenRect Avatar::DRAW_RECT(-19, -49, 40, 60);
const MapRect Avatar::COLLISION_RECT(-5, -2, 10, 4);

Avatar::Avatar(const std::string &name, const MapPoint &location,
               Client &client)
    : Sprite(&client.avatarSpriteType, location, client),
      ClientCombatant(&client.avatarCombatantType),
      _name(name),
      _gear(Client::GEAR_SLOTS, std::make_pair(ClientItem::Instance{}, 0)) {}

void Avatar::draw() const {
  if (isDriving()) return;
  if (!_class) return;

  auto imagesToGenerate =
      std::vector<Texture *>{&_imageWithGear, &_highlightImageWithGear};
  for (auto *image : imagesToGenerate) {
    *image = {width(), height()};

    renderer.pushRenderTarget(*image);
    renderer.fillWithTransparency();
    image->setBlend();

    // Base image
    auto isHighlightImage = image == &_highlightImageWithGear;
    if (isHighlightImage)
      _class->highlightImage().draw();
    else
      _class->image().draw();

    // Gear
    const auto gearOffset =
        isHighlightImage ? ScreenPoint{1, 1} : ScreenPoint{};
    for (const auto &pair : ClientItem::drawOrder()) {
      const ClientItem *item = _gear[pair.second].first.type();
      if (item) {
        item->draw(ScreenPoint{-DRAW_RECT.x, -DRAW_RECT.y} + gearOffset);
      }
    }

    if (_pixelsToCutOffBottomWhenDrawn > 0) {
      auto transparency = Texture{DRAW_RECT.w, _pixelsToCutOffBottomWhenDrawn};
      transparency.setBlend(SDL_BLENDMODE_NONE);
      const auto TRANSPARENCY_TOP =
          DRAW_RECT.h - _pixelsToCutOffBottomWhenDrawn;
      transparency.draw(0, TRANSPARENCY_TOP);
    }

    renderer.popRenderTarget();
  }

  Sprite::draw();

  drawBuffEffects(location());

  if (isDebug()) {
    renderer.setDrawColor(Color::CYAN);
    renderer.drawRect(toScreenRect(COLLISION_RECT + location()) +
                      _client.offset());
  }
}

void Avatar::drawName() const {
  if (isCharacter()) return;

  const Texture nameLabel(_client.defaultFont(), _name, nameColor());
  const Texture nameOutline(_client.defaultFont(), _name, Color::UI_OUTLINE);
  Texture cityOutline, cityLabel;

  ScreenPoint namePosition = toScreenPoint(location()) + _client.offset();
  namePosition.y -= 60;
  namePosition.x -= nameLabel.width() / 2;

  ScreenPoint cityPosition;
  bool shouldDrawCityName =
      (!_city.empty()) /*&& (_name != _client->username())*/;
  if (shouldDrawCityName) {
    auto cityText = std::string{};
    if (_isKing) cityText = "King ";
    cityText += "of " + _city;
    cityOutline = Texture(_client.defaultFont(), cityText, Color::UI_OUTLINE);
    cityLabel = Texture(_client.defaultFont(), cityText, nameColor());
    cityPosition.x =
        toInt(location().x + _client.offset().x - cityLabel.width() / 2.0);
    cityPosition.y = namePosition.y;
    namePosition.y -= 11;
  }
  for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y) {
      nameOutline.draw(namePosition + ScreenPoint{x, y});
      if (shouldDrawCityName)
        cityOutline.draw(cityPosition + ScreenPoint{x, y});
    }
  nameLabel.draw(namePosition);
  if (shouldDrawCityName) cityLabel.draw(cityPosition);
}

void Avatar::update(double delta) {
  _client.drawGearParticles(_gear, location(), delta);

  ms_t timeElapsed = toInt(1000 * delta);
  if (_currentlyCrafting) {
    if (_craftingSoundTimer > timeElapsed)
      _craftingSoundTimer -= timeElapsed;
    else {
      _currentlyCrafting->playSoundOnce("crafting");
      _craftingSoundTimer += _currentlyCrafting->soundPeriod() - timeElapsed;
    }
  }

  Sprite::update(delta);
  ClientCombatant::update(delta);
}

void Avatar::setClass(const ClassInfo::Name &newClass) {
  const auto it = _client._classes.find(newClass);
  if (it == _client._classes.end()) return;
  _class = &(it->second);
}

double Avatar::vehicleSpeed() const {
  if (!_vehicle) return 0;
  return _vehicle->speed();
}

const Tooltip &Avatar::tooltip() const {
  if (_tooltip.hasValue()) return _tooltip.value();
  _tooltip = Tooltip{};
  auto &tooltip = _tooltip.value();

  // Name
  tooltip.setColor(Color::TOOLTIP_NAME);
  tooltip.addLine(_name);

  // Class
  auto className = getClass() ? getClass()->name() : "Unknown";
  tooltip.addGap();
  tooltip.setColor(Color::TOOLTIP_BODY);
  tooltip.addLine("Level "s + toString(level()) + " "s + className);

  return tooltip;
}

void Avatar::playAttackSound() const {
  static const size_t WEAPON_SLOT = 6;
  const ClientItem *weapon = _gear[WEAPON_SLOT].first.type();

  if (weapon)
    weapon->playSoundOnce("attack");
  else
    _client.avatarSounds()->playOnce("attack");
}

void Avatar::playDefendSound() const {
  const ClientItem *armor = getRandomArmor();

  if (armor)
    armor->playSoundOnce("defend");
  else
    _client.avatarSounds()->playOnce("defend");
}

void Avatar::playDeathSound() const {
  const Client &client = *Client::_instance;
  client.avatarSounds()->playOnce("death");
}

void Avatar::onLeftClick() {
  _client.setTarget(*this);
  // Note: parent class's onLeftClick() not called.
}

void Avatar::onRightClick() {
  _client.setTarget(*this, true);
  // Note: parent class's onRightClick() not called.
}

double Avatar::speed() const {
  if (_vehicle) return _vehicle->speed();
  return Sprite::speed();
}

void Avatar::onNewLocationFromServer() {
  if (!isCharacter()) return;
  if (_vehicle) _vehicle->newLocationFromServer(locationOnServer());
}

void Avatar::onLocationChange() {
  if (!isCharacter()) return;
  if (_vehicle) _vehicle->location(location());
}

void Avatar::sendTargetMessage() const {
  const Client &client = *Client::_instance;
  client.sendMessage({CL_TARGET_PLAYER, _name});
}

void Avatar::sendSelectMessage() const {
  const Client &client = *Client::_instance;
  client.sendMessage({CL_SELECT_PLAYER, _name});
}

bool Avatar::canBeAttackedByPlayer() const {
  if (!ClientCombatant::canBeAttackedByPlayer()) return false;
  const Client &client = *Client::_instance;
  return client.isAtWarWith(*this);
}

const Texture &Avatar::cursor() const {
  if (canBeAttackedByPlayer()) return _client.cursorAttack();
  return _client.cursorNormal();
}

bool Avatar::isInPlayersCity() const {
  bool hasNoCity = _city.empty();
  if (hasNoCity) return false;

  const Avatar &playerCharacter = Client::_instance->character();
  if (_city == playerCharacter._city) return true;

  return false;
}

const Color &Avatar::nameColor() const {
  if (this == &Client::_instance->character()) return Color::COMBATANT_SELF;

  if (canBeAttackedByPlayer()) return Color::COMBATANT_ENEMY;

  return Sprite::nameColor();
}

void Avatar::addMenuButtons(List &menu) const {
  const Client &client = *Client::_instance;
  std::string tooltipText;

  void *pUsername = const_cast<std::string *>(&_name);
  auto *playerWarButton = new Button(
      0, "Declare war", [pUsername]() { declareWarAgainstPlayer(pUsername); });
  if (client.isAtWarWithPlayerDirectly(_name)) {
    playerWarButton->disable();
    tooltipText = "You are already at war with " + _name + ".";
  } else {
    tooltipText = "Declare war on " + _name + ".";
    if (!client.character().cityName().empty())
      tooltipText +=
          " While you are a member of a city, this personal war will not be "
          "active.";
    if (!_city.empty())
      tooltipText +=
          " While " + _name +
          " is a member of a city, this personal war will not be active.";
  }
  playerWarButton->setTooltip(tooltipText);
  menu.addChild(playerWarButton);

  void *pCityName = const_cast<std::string *>(&_city);
  auto *cityWarButton =
      new Button(0, "Declare war against city",
                 [pCityName]() { declareWarAgainstCity(pCityName); });
  if (_city.empty()) {
    cityWarButton->disable();
    tooltipText = _name + " is not a member of a city.";
  } else if (client.isAtWarWithCityDirectly(_city)) {
    cityWarButton->disable();
    tooltipText = "You are already at war with the city of " + _city + ".";
  } else {
    tooltipText = "Declare war on the city of " + _city + ".";
    if (!client.character().cityName().empty())
      tooltipText +=
          " While you are a member of a city, this personal war will not be "
          "active.";
  }
  cityWarButton->setTooltip(tooltipText);
  menu.addChild(cityWarButton);

  if (client.character().isKing()) {
    auto *playerWarButton = new Button(0, "Declare city war", [pUsername]() {
      declareCityWarAgainstPlayer(pUsername);
    });
    if (client.isCityAtWarWithPlayerDirectly(_name)) {
      playerWarButton->disable();
      tooltipText = "Your city is already at war with " + _name + ".";
    } else {
      tooltipText = "Declare war on " + _name + " on behalf of your city.";
    }
    if (!_city.empty()) {
      tooltipText +=
          " While " + _name +
          " is a member of a city, this personal war will not be active.";
    }
    playerWarButton->setTooltip(tooltipText);
    menu.addChild(playerWarButton);

    auto *cityWarButton =
        new Button(0, "Declare city war against city",
                   [pCityName]() { declareCityWarAgainstCity(pCityName); });
    if (_city.empty()) {
      cityWarButton->disable();
      tooltipText = _name + " is not a member of a city.";
    } else if (client.isCityAtWarWithCityDirectly(_city)) {
      cityWarButton->disable();
      tooltipText = "You are already at war with the city of " + _city + ".";
    } else {
      tooltipText =
          "Declare war on the city of " + _city + " on behalf of your city.";
    }
    cityWarButton->setTooltip(tooltipText);
    menu.addChild(cityWarButton);
  }

  auto *recruitButton =
      new Button(0, "Recruit", [pUsername]() { recruit(pUsername); });
  if (!_city.empty()) {
    recruitButton->disable();
    tooltipText = _name + " is already in a city.";
  } else if (client.character().cityName().empty()) {
    recruitButton->disable();
    tooltipText = "You are not in a city.";
  } else {
    tooltipText = "Recruit " + _name + " into the city of " +
                  client.character().cityName() + ".";
  }
  recruitButton->setTooltip(tooltipText);
  menu.addChild(recruitButton);
}

void Avatar::declareWarAgainstPlayer(void *pUsername) {
  const std::string &username =
      *reinterpret_cast<const std::string *>(pUsername);
  Client &client = *Client::_instance;
  client.sendMessage({CL_DECLARE_WAR_ON_PLAYER, username});
}

void Avatar::declareWarAgainstCity(void *pCityName) {
  const std::string &cityName =
      *reinterpret_cast<const std::string *>(pCityName);
  Client &client = *Client::_instance;
  client.sendMessage({CL_DECLARE_WAR_ON_CITY, cityName});
}

void Avatar::declareCityWarAgainstPlayer(void *pUsername) {
  const std::string &username =
      *reinterpret_cast<const std::string *>(pUsername);
  Client &client = *Client::_instance;
  client.sendMessage({CL_DECLARE_WAR_ON_PLAYER_AS_CITY, username});
}

void Avatar::declareCityWarAgainstCity(void *pCityName) {
  const std::string &cityName =
      *reinterpret_cast<const std::string *>(pCityName);
  Client &client = *Client::_instance;
  client.sendMessage({CL_DECLARE_WAR_ON_CITY_AS_CITY, cityName});
}

void Avatar::recruit(void *pUsername) {
  const std::string &username =
      *reinterpret_cast<const std::string *>(pUsername);
  Client &client = *Client::_instance;
  client.sendMessage({CL_RECRUIT, username});
}
