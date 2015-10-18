// (C) 2015 Tim Gurto

#include <cassert>

#include "Object.h"
#include "../util.h"

Object::Object(const ObjectType *type, const Point &loc):
_serial(generateSerial()),
_location(loc),
_type(type){
    assert(type);
    if (type->yield())
        type->yield().instantiate(_contents);
}

Object::Object(size_t serial): // For set/map lookup
_serial (serial),
_type(0){}

size_t Object::generateSerial() {
    static size_t currentSerial = 0;
    return currentSerial++;
}

void Object::contents(const ItemSet &contents){
    _contents = contents;
}

void Object::removeItem(const Item *item, size_t qty){
    assert (_contents[item] >= qty);
    assert (_contents.totalQuantity() >= qty);
    _contents.remove(item, qty);
}

const Item *Object::chooseGatherItem() const{
    assert(!_contents.isEmpty());
    assert(_contents.totalQuantity() > 0);
    
    // Choose random item, weighted by remaining quantity of each item type.
    size_t i = rand() % _contents.totalQuantity();
    for (auto item : _contents) {
        if (i <= item.second)
            return item.first;
        else
            i -= item.second;
    }
    assert(false);
    return 0;
}

size_t Object::chooseGatherQuantity(const Item *item) const{
    size_t randomQty = _type->yield().generateGatherQuantity(item);
    return min<size_t>(randomQty, _contents[item]);
}
