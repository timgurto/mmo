#ifndef DECONSTRUCTION_H
#define DECONSTRUCTION_H

#include "../types.h"

class Deconstruction;
class Object;
class ServerItem;

class DeconstructionType{
public:
    static DeconstructionType *ItemAndTime(const ServerItem *item, ms_t timeToDeconstruct);
    Deconstruction *instantiate(Object &parent) const;

    /*const ServerItem *deconstructsItem() const { return _itemThisBecomes; }
    void deconstructsItem(const ServerItem *item) { _itemThisBecomes = item; }
    ms_t deconstructionTime() const { return _timeToDeconstruct; }
    void deconstructionTime(ms_t t) { _timeToDeconstruct = t; }*/
    
private:
    DeconstructionType(const ServerItem *item, ms_t timeToDeconstruct);

    const ServerItem *_itemThisBecomes;
    ms_t _timeToDeconstruct;

    friend class Deconstruction;
};


class Deconstruction{
public:
    const ServerItem *becomes() const { return _type._itemThisBecomes; }
    ms_t timeToDeconstruct() const { return _type._timeToDeconstruct; }
private:
    Deconstruction(Object &parent, const DeconstructionType &type);
    const DeconstructionType &_type;
    Object &_parent;

    friend class DeconstructionType;
};

#endif
