#ifndef CLIENT_NPC_H
#define CLIENT_NPC_H

#include "../Point.h"
#include "ClientNPCType.h"
#include "ClientObject.h"

class ClientNPC : public ClientObject {
 public:
  ClientNPC(size_t serial, const ClientNPCType *type = nullptr,
            const MapPoint &loc = MapPoint{});
  ~ClientNPC() {}

  const ClientNPCType *npcType() const {
    return dynamic_cast<const ClientNPCType *>(type());
  }

  char classTag() const override { return 'n'; }

  // From ClientCombatant:
  bool canBeAttackedByPlayer() const override;
  const Color &nameColor() const override;

  // From Sprite:
  void update(double delta) override;
  void draw(const Client &client) const override;
  bool shouldDrawName() const override { return true; }
};

#endif
