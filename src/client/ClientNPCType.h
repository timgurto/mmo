#ifndef CLIENT_NPC_TYPE_H
#define CLIENT_NPC_TYPE_H

#include "ClientObject.h"
#include "Projectile.h"

class SoundProfile;

struct CNPCTemplate {
  MapRect collisionRect;
  std::string imageFile;
  std::string soundProfile;
  px_t xDrawOffset{0};
  px_t yDrawOffset{0};
};

class ClientNPCType : public ClientObjectType {
 public:
  ClientNPCType(const std::string &id, const std::string &imagePath,
                Hitpoints maxHealth, const Client &client);
  virtual ~ClientNPCType() override {}

  void projectile(const Projectile::Type &type) { _projectile = &type; }
  const Projectile::Type *projectile() const { return _projectile; }

  void applyTemplate(const CNPCTemplate *nt, const Client &client);

  void addGear(const ClientItem &item);
  const ClientItem *gear(size_t slot) const;
  const ClientItem::vect_t &gear() const { return _gear; }
  bool hasGear() const { return !_gear.empty(); }

  void makeCivilian() { _isCivilian = true; }
  bool isCivilian() const { return _isCivilian; }
  void makeNeutral() { _isNeutral = true; }
  bool isNeutral() const { return _isNeutral; }
  void canBeTamed(bool b) { _canBeTamed = b; }
  bool canBeTamed() const { return _canBeTamed; }
  void tamingRequiresItem(const ClientItem *item) {
    _itemRequiredForTaming = item;
  }
  const ClientItem *itemRequiredForTaming() const {
    return _itemRequiredForTaming;
  }

  virtual char classTag() const override { return 'n'; }

 private:
  const Projectile::Type *_projectile = nullptr;
  ClientItem::vect_t _gear;  // For humanoid NPCs
  bool _isCivilian{false};
  bool _isNeutral{false};
  bool _canBeTamed{false};
  const ClientItem *_itemRequiredForTaming{nullptr};
  ScreenRect _corpseDrawRect;  // By default, equal to SpriteType::_drawRect.
};

#endif
