#include <cassert>

#include "Client.h"
#include "ClientNPC.h"

extern Renderer renderer;

ClientNPC::ClientNPC(size_t serial, const ClientNPCType *type,
                     const MapPoint &loc)
    : ClientObject(serial, type, loc) {}

bool ClientNPC::canBeAttackedByPlayer() const {
  if (!ClientCombatant::canBeAttackedByPlayer()) return false;
  if (npcType()->isCivilian()) return false;
  return true;
}

const Color &ClientNPC::nameColor() const {
  if (npcType()->isCivilian()) return Color::COMBATANT_NEUTRAL;
  if (npcType()->isNeutral()) return Color::COMBATANT_DEFENSIVE;
  return Color::COMBATANT_ENEMY;
}

void ClientNPC::update(double delta) {
  auto &client = Client::instance();
  if (npcType()->hasGear())
    client.drawGearParticles(npcType()->gear(), location(), delta);

  ClientObject::update(delta);
}

void ClientNPC::draw(const Client &client) const {
  ClientObject::draw(client);

  // Draw gear
  if (npcType()->hasGear())
    for (const auto &pair : ClientItem::drawOrder()) {
      const ClientItem *item = npcType()->gear(pair.second);
      if (item) item->draw(location());
    }

  drawBuffEffects(location());
}

bool ClientNPC::shouldDrawName() const {
  if (isDead()) return false;
  return true;
}
