#ifndef LOOT_TABLE_H
#define LOOT_TABLE_H

#include <map>
#include <memory>

#include "../NormalVariable.h"
#include "ServerItem.h"

class Loot;
class User;

// Defines the loot chances for a single NPC type, and generates loot.
class LootTable {
  struct LootEntry {
    virtual bool operator==(const LootEntry &rhs) const = 0;
    bool operator!=(const LootEntry &rhs) const { return !((*this) == rhs); }
    virtual std::pair<const ServerItem *, int> instantiate() const = 0;
  };

  struct SimpleEntry : public LootEntry {  // x% chance to receive y item
    const ServerItem *item{nullptr};
    double chance{0};

    bool operator==(const LootEntry &rhs) const override;
    std::pair<const ServerItem *, int> instantiate() const;
  };

  struct NormalEntry : public LootEntry {  // normal distribution
    const ServerItem *item;
    NormalVariable normalDist;

    bool operator==(const LootEntry &rhs) const override;
    std::pair<const ServerItem *, int> instantiate() const;
  };

  struct ChoiceEntry : public LootEntry {  // receive one of these items
    std::vector<std::pair<const ServerItem *, int>> choices;

    bool operator==(const LootEntry &rhs) const override;
    std::pair<const ServerItem *, int> instantiate() const;
  };

  std::vector<std::shared_ptr<LootEntry>> _entries;

 public:
  bool operator==(const LootTable &rhs) const;
  bool operator!=(const LootTable &rhs) const { return !((*this) == rhs); }

  void addNormalItem(const ServerItem *item, double mean, double sd = 0);
  void addSimpleItem(const ServerItem *item, double chance);
  void addChoiceOfItems(
      const std::vector<std::pair<const ServerItem *, int>> choices);

  void addAllFrom(const LootTable &rhs);

  // Creates a new instance of this Yield, with random init values, in the
  // specified ItemSet
  void instantiate(Loot &container, const User *killer) const;
};

#endif
