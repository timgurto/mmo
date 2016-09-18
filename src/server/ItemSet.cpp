// (C) 2015 Tim Gurto

#include <cassert>

#include "ItemSet.h"
#include "../util.h"

ItemSet::ItemSet():
_totalQty(0){}

const ItemSet &ItemSet::operator+=(const ItemSet &rhs){
    for (const auto &pair : rhs) {
        _set[pair.first] += pair.second;
        _totalQty += pair.second;
    }
    checkTotalQty();
    return *this;
}

const ItemSet &ItemSet::operator+=(const ServerItem *newItem){
    ++_set[newItem];
    ++_totalQty;
    checkTotalQty();
    return *this;
}

const ItemSet &ItemSet::operator-=(const ItemSet &rhs){
    for (const auto &pair : rhs) {
        auto it = _set.find(pair.first);
        size_t qtyToRemove = min(pair.second, it->second);
        it->second -= qtyToRemove;
        _totalQty -= qtyToRemove;
        if (it->second == 0)
            _set.erase(it);
    }
    checkTotalQty();
    return *this;
}

size_t ItemSet::operator[](const ServerItem *key) const{
    auto it = _set.find(key);
    if (it == _set.end())
        return 0;
    return it->second;
}

void ItemSet::set(const ServerItem *item, size_t quantity){
    size_t oldQty = _set[item];
    assert(_totalQty >= oldQty);
    auto it = _set.find(item);
    it->second = quantity;
    _totalQty = _totalQty - oldQty + quantity;
    if (quantity == 0)
        _set.erase(it);
    checkTotalQty();
}

bool ItemSet::contains(const ItemSet &rhs) const{
    ItemSet remaining = *this;
    for (const auto &pair : rhs) {
        auto it = remaining._set.find(pair.first);
        if (it == _set.end())
            return false;
        if (it->second < pair.second)
            return false;
        it->second -= pair.second;
        if (it->second == 0)
            remaining._set.erase(it);
    }
    return true;
}

bool ItemSet::contains(const ServerItem *item, size_t qty) const{
    if (qty == 0)
        return true;
    auto it = _set.find(item);
    if (it == _set.end())
        return false;
    return (it->second >= qty);
}

bool ItemSet::contains(const std::string &className){
    for (auto pair : _set)
        if (pair.first->classes().find(className) != pair.first->classes().end())
            return true;
    return false;
}

bool ItemSet::contains(const std::set<std::string> &classes){
    for (const std::string &name : classes)
        if (!contains(name))
            return false;
    return true;
}

void ItemSet::add(const ServerItem *item, size_t qty){
    if (qty == 0)
        return;
    _set[item] += qty;
    _totalQty += qty;
    checkTotalQty();
}

void ItemSet::remove(const ServerItem *item, size_t qty){
    if (qty == 0)
        return;
    auto it = _set.find(item);
    if (it == _set.end())
        return;
    size_t qtyToRemove = min(qty, it->second);
    it->second -= qtyToRemove;
    if (it->second == 0)
        _set.erase(it);
    _totalQty -= qtyToRemove;
    checkTotalQty();
}

void ItemSet::checkTotalQty() const{
    size_t total = 0;
    for (const auto &pair : _set)
        total += pair.second;
    assert(_totalQty == total);
};

bool operator<=(const ItemSet &itemSet, const ServerItem::vect_t &vect){
    ItemSet remaining = itemSet;
    for (size_t i = 0; i != vect.size(); ++i){
        const std::pair<const ServerItem *, size_t> &invSlot = vect[i];
        remaining.remove(invSlot.first, invSlot.second);
        if (remaining.isEmpty())
            return true;
    }
    return false;
}

bool operator<=(const ItemSet &lhs, const ItemSet &rhs) {
    return rhs.contains(lhs);
}

bool operator>(const ItemSet &itemSet, const ServerItem::vect_t &vect) {
    return ! (itemSet <= vect);
}

bool operator>(const ItemSet &lhs, const ItemSet &rhs) {
    return ! (lhs <= rhs);
};
