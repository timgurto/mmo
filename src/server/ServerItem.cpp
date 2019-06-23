#include "ServerItem.h"

#include "Server.h"
#include "objects/ObjectType.h"

ServerItem::ServerItem(const std::string &idArg) : Item(idArg) {}

void ServerItem::fetchAmmoItem() const {
  if (_weaponAmmoID.empty()) return;

  const auto &server = Server::instance();
  auto it = server._items.find(_weaponAmmoID);
  if (it == server._items.end()) {
    server.debug() << Color::CHAT_ERROR << "Unknown item "s << _weaponAmmoID
                   << " specified as ammo"s << Log::endl;
    return;
  }
  _weaponAmmo = &*it;
}

bool vectHasSpace(const ServerItem::vect_t &vect, const ServerItem *item,
                  size_t qty) {
  for (const auto &slot : vect) {
    auto itemInSlot = slot.first.type;
    auto qtyInSlot = slot.second;

    if (!itemInSlot) {
      if (qty <= item->stackSize()) return true;
      qty -= item->stackSize();
    } else if (itemInSlot == item) {
      auto roomInSlot = item->stackSize() - qtyInSlot;
      if (qty <= roomInSlot) return true;
      qty -= roomInSlot;
    } else
      continue;
  }
  return false;
}

bool vectHasSpaceAfterRemovingItems(const ServerItem::vect_t &vect,
                                    const ServerItem *item, size_t qty,
                                    const ServerItem *itemThatWillBeRemoved,
                                    size_t qtyThatWillBeRemoved) {
  // Make copy
  auto v = vect;

  // Remove items from copy
  auto qtyLeft = qtyThatWillBeRemoved;
  for (auto &slot : v) {
    if (slot.first.type != itemThatWillBeRemoved) continue;
    if (slot.second > qtyLeft) {
      slot.first = {};
      slot.second = 0;
      break;
    }
    qtyLeft -= slot.second;
    slot.first = {};
    slot.second = 0;
  }

  // Call normal function on copy
  return vectHasSpace(v, item, qty);
}

bool operator<=(const ItemSet &itemSet, const ServerItem::vect_t &vect) {
  ItemSet remaining = itemSet;
  for (size_t i = 0; i != vect.size(); ++i) {
    const auto &invSlot = vect[i];
    remaining.remove(invSlot.first.type, invSlot.second);
    if (remaining.isEmpty()) return true;
  }
  return false;
}

bool operator>(const ItemSet &itemSet, const ServerItem::vect_t &vect) {
  return !(itemSet <= vect);
}

bool operator>(const ServerItem::vect_t &vect, const ItemSet &itemSet) {
  return !(itemSet <= vect);
}

const ServerItem *toServerItem(const Item *item) {
  return dynamic_cast<const ServerItem *>(item);
}

ServerItem::Instance::Instance(const ServerItem *typeArg) : type(typeArg) {
  if (type) health = MAX_HEALTH;
}
