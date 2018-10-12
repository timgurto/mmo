#include "Client.h"
#include "Texture.h"
#include "ui/Picture.h"
#include "ui/Window.h"

void Client::initializeMapWindow() {
  _mapImage = Texture(std::string("Images/map.png"));
  _mapWindow = Window::WithRectAndTitle(
      {(SCREEN_X - MAP_IMAGE_W) / 2, (SCREEN_Y - MAP_IMAGE_H) / 2,
       MAP_IMAGE_W + 1, MAP_IMAGE_H + 1},
      "Map");
  _mapWindow->addChild(
      new Picture(ScreenRect{0, 0, MAP_IMAGE_W, MAP_IMAGE_H}, _mapImage));

  _mapPinOutlines = new Element({0, 0, MAP_IMAGE_W, MAP_IMAGE_H});
  _mapPins = new Element({0, 0, MAP_IMAGE_W, MAP_IMAGE_H});
  _mapWindow->addChild(_mapPinOutlines);
  _mapWindow->addChild(_mapPins);

  _mapWindow->setPreRefreshFunction(updateMapWindow);
}

void Client::updateMapWindow(Element &) {
  Client &client = *Client::_instance;

  client._mapPins->clearChildren();
  client._mapPinOutlines->clearChildren();

  for (const auto &objPair : client._objects) {
    const auto &object = *objPair.second;
    client.addMapPin(object.location(), object.nameColor());
  }

  for (const auto &pair : client._otherUsers) {
    const auto &avatar = *pair.second;
    client.addMapPin(avatar.location(), avatar.nameColor());
  }

  client.addOutlinedMapPin(client._character.location(), Color::TODO);
}

void Client::addMapPin(const MapPoint &worldPosition, const Color &color) {
  static const ScreenRect PIN_RECT(0, 0, 1, 1), OUTLINE_RECT(-1, -1, 3, 3);

  auto mapPosition = convertToMapPosition(worldPosition);

  _mapPins->addChild(new ColorBlock(PIN_RECT + mapPosition, color));
  _mapPinOutlines->addChild(
      new ColorBlock(OUTLINE_RECT + mapPosition, Color::TODO));
}

void Client::addOutlinedMapPin(const MapPoint &worldPosition,
                               const Color &color) {
  static const ScreenRect PIN_RECT(0, 0, 1, 1), OUTLINE_RECT_H(-2, -1, 5, 3),
      OUTLINE_RECT_V(-1, -2, 3, 5), BORDER_RECT_H(-1, 0, 3, 1),
      BORDER_RECT_V(0, -1, 1, 3);

  auto mapPosition = convertToMapPosition(worldPosition);

  _mapPins->addChild(new ColorBlock(BORDER_RECT_H + mapPosition, Color::TODO));
  _mapPins->addChild(new ColorBlock(BORDER_RECT_V + mapPosition, Color::TODO));
  _mapPins->addChild(new ColorBlock(PIN_RECT + mapPosition, color));
  _mapPinOutlines->addChild(
      new ColorBlock(OUTLINE_RECT_H + mapPosition, Color::TODO));
  _mapPinOutlines->addChild(
      new ColorBlock(OUTLINE_RECT_V + mapPosition, Color::TODO));
}

ScreenRect Client::convertToMapPosition(const MapPoint &worldPosition) const {
  const double MAP_FACTOR_X = 1.0 * _mapX * TILE_W / MAP_IMAGE_W,
               MAP_FACTOR_Y = 1.0 * _mapY * TILE_H / MAP_IMAGE_H;

  px_t x = toInt(worldPosition.x / MAP_FACTOR_X),
       y = toInt(worldPosition.y / MAP_FACTOR_Y);
  return {x, y, 0, 0};
}
