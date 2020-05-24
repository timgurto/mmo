#include "DroppedItem.h"

#include "Server.h"
#include "User.h"

DroppedItem::Type DroppedItem::commonType;

DroppedItem::Type::Type() : EntityType("droppedItem") {
  _baseStats.maxHealth = 1;
}

DroppedItem::DroppedItem(const ServerItem &itemType, size_t quantity,
                         const MapPoint &location)
    : Entity(&commonType, location), _quantity(quantity), _itemType(itemType) {}

void DroppedItem::sendInfoToClient(const User &targetUser) const {
  targetUser.sendMessage(
      {SV_DROPPED_ITEM, makeArgs(serial(), location().x, location().y,
                                 _itemType.id(), _quantity)});
}

void DroppedItem::getPickedUpBy(User &user) {
  auto remainder = user.giveItem(&_itemType, _quantity);

  if (remainder == _quantity) {
    return;
  }

  if (remainder > 0) {
    Server::instance().addEntity(
        new DroppedItem(_itemType, remainder, location()));
  }

  markForRemoval();
}
