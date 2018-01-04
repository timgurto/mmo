#include <cassert>

#include "Client.h"
#include "Sprite.h"
#include "Renderer.h"
#include "TooltipBuilder.h"

#include "../util.h"

extern Renderer renderer;

const std::string Sprite::EMPTY_NAME = "";

Sprite::Sprite(const SpriteType *type, const MapPoint &location):
_yChanged(false),
_type(type),
_location(location),
_toRemove(false){}

ScreenRect Sprite::drawRect() const {
    assert(_type != nullptr);
    auto typeDrawRect = _type->drawRect();
    auto drawRect = typeDrawRect + toScreenPoint(_location);
    return drawRect;
}

void Sprite::draw(const Client &client) const{
    const Texture &imageToDraw = client.currentMouseOverEntity() == this ? highlightImage() : image();
    if (imageToDraw)
        imageToDraw.draw(drawRect() + client.offset());

    if (shouldDrawName())
        drawName();
}

void Sprite::drawName() const {
    const auto &client = Client::instance();

    auto text = name();
    if (!additionalTextInName().empty())
        text += " "s + additionalTextInName();

    const auto nameLabel = Texture{ client.defaultFont(), text, nameColor() };
    const auto nameOutline = Texture{ client.defaultFont(), text, Color::PLAYER_NAME_OUTLINE };
    auto namePosition = toScreenPoint(location()) + client.offset();
    namePosition.y -= height();
    namePosition.y -= 16;
    namePosition.x -= nameLabel.width() / 2;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            nameOutline.draw(namePosition + ScreenPoint(x, y));
    nameLabel.draw(namePosition);
}

void Sprite::update(double delta) {
    auto &client = Client::instance();
    for (auto &p : _type->particles())
        client.addParticles(p.profile, _location + p.offset, delta);
}

double Sprite::bottomEdge() const{
    if (_type != nullptr)
        return _location.y + _type->drawRect().y + _type->height();
    else
        return _location.y;
}

void Sprite::location(const MapPoint &loc){
    const double oldY = _location.y;
    _location = loc;
    if (_location.y != oldY)
        _yChanged = true;
}

bool Sprite::collision(const MapPoint &p) const{
    return ::collision(toScreenPoint(p), drawRect());
}

const Texture &Sprite::cursor(const Client &client) const {
    return client.cursorNormal();
}
