#ifndef OBJECT_TYPE_H
#define OBJECT_TYPE_H

#include <string>

#include "../Rect.h"
#include "../types.h"
#include "TerrainList.h"
#include "Yield.h"

class ServerItem;

// Describes a class of Objects, the "instances" of which share common properties
class ObjectType{
    std::string _id;
    ms_t _gatherTime;
    ms_t _constructionTime;

    const ServerItem *_deconstructsItem; // Item gained when this object is deconstructed
    ms_t _deconstructionTime;

    // To gather from objects of this type, a user must have an item of the following class.
    std::string _gatherReq;

    size_t _containerSlots;
    size_t _merchantSlots;

    Yield _yield; // If gatherable.

    bool _collides; // false by default; true if any collisionRect is specified.
    Rect _collisionRect; // Relative to position

    std::set<std::string> _tags;

    const TerrainList *_allowedTerrain;

public:
    ObjectType(const std::string &id);

    virtual ~ObjectType(){}

    void gatherTime(ms_t t) { _gatherTime = t; }
    const std::string &gatherReq() const { return _gatherReq; }
    void gatherReq(const std::string &req) { _gatherReq = req; }
    size_t containerSlots() const { return _containerSlots; }
    void containerSlots(size_t n) { _containerSlots = n; }
    size_t merchantSlots() const { return _merchantSlots; }
    void merchantSlots(size_t n) { _merchantSlots = n; }

    virtual char classTag() const { return 'o'; }

    const std::string &id() const { return _id; }
    ms_t gatherTime() const { return _gatherTime; }
    ms_t constructionTime() const { return _constructionTime; }
    void constructionTime(ms_t t) { _constructionTime = t; }
    const Yield &yield() const { return _yield; }
    bool collides() const { return _collides; }
    const Rect &collisionRect() const { return _collisionRect; }
    void collisionRect(const Rect &r) { _collisionRect = r; _collides = true; }
    bool isTag( const std::string &tagName) const;
    const ServerItem *deconstructsItem() const { return _deconstructsItem; }
    void deconstructsItem(const ServerItem *item) { _deconstructsItem = item; }
    ms_t deconstructionTime() const { return _deconstructionTime; }
    void deconstructionTime(ms_t t) { _deconstructionTime = t; }
    const TerrainList &allowedTerrain() const;

    bool operator<(const ObjectType &rhs) const { return _id < rhs._id; }

    void addYield(const ServerItem *item,
                  double initMean, double initSD,
                  double gatherMean, double gatherSD);
    void addTag(const std::string &tagName);
};

#endif
