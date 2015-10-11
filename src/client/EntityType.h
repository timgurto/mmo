// (C) 2015 Tim Gurto

#ifndef ENTITY_TYPE_H
#define ENTITY_TYPE_H

#include <string>

#include "Texture.h"
#include "../Point.h"
#include "../util.h"

// Describes a class of Entities, the "instances" of which share common properties
class EntityType{
    Texture _image;
    SDL_Rect _drawRect; // Where to draw the image, relative to its location

public:
    EntityType(const SDL_Rect &drawRect = makeRect(), const std::string &imageFile = "");
    EntityType(const std::string &id); // For set lookup

    virtual ~EntityType(){}

    void image(const std::string &filename);
    const SDL_Rect &drawRect() const { return _drawRect; }
    void drawRect(const SDL_Rect &rect) { _drawRect = rect; }
    int width() const { return _drawRect.w; }
    int height() const { return _drawRect.h; }

    void drawAt(const Point &loc) const;
};

#endif
