#include "LootTable.h"

#include "../util.h"
#include "Loot.h"
#include "Server.h"
#include "User.h"

bool LootTable::operator==(const LootTable &rhs) const {
  if (_entries.size() != rhs._entries.size()) return false;
  auto other = rhs._entries;  // copy to modify
  for (const auto entry : _entries) {
    auto entryFound = false;
    for (auto i = 0; i != other.size(); ++i) {
      if (*other[i] != *entry) continue;

      // Remove from vector
      other[i] = other.back();
      other.pop_back();
      entryFound = true;
      break;
    }

    if (!entryFound) return false;
  }
  return true;
}

void LootTable::addNormalItem(const ServerItem *item, double mean, double sd) {
  auto entry = std::shared_ptr<NormalEntry>{new NormalEntry};
  entry->item = item;
  entry->normalDist = {mean, sd};
  _entries.push_back(entry);
}

void LootTable::addSimpleItem(const ServerItem *item, double chance) {
  auto entry = std::shared_ptr<SimpleEntry>{new SimpleEntry};
  entry->item = item;
  entry->chance = chance;
  _entries.push_back(entry);
}

void LootTable::addChoiceOfItems(
    const std::vector<std::pair<const ServerItem *, int>> choices) {
  auto entry = std::shared_ptr<ChoiceEntry>{new ChoiceEntry};
  entry->choices = choices;
  _entries.push_back(entry);
}

void LootTable::addAllFrom(const LootTable &rhs) {
  for (const auto &entry : rhs._entries) _entries.push_back(entry);
}

void LootTable::instantiate(Loot &loot, const User *killer) const {
  if (!loot.empty()) {
    SERVER_ERROR("Loot object provided was not empty");
  }
  for (const auto entry : _entries) {
    auto pair = entry->instantiate();
    auto item = pair.first;
    auto quantity = pair.second;

    if (quantity == 0) continue;

    const auto NO_LIMIT = -1;
    auto qtyLimit = NO_LIMIT;

    if (killer) {
      // Enforce quest exclusivity
      if (item->isQuestExclusive()) {
        if (!killer->isOnQuest(item->exclusiveToQuest())) return;
      }

      // Limit quantity based on killer's quest progress
      if (item->isQuestExclusive()) {
        const auto &server = Server::instance();
        auto quest = server.findQuest(item->exclusiveToQuest());
        for (const auto &objective : quest->objectives) {
          if (objective.type != Quest::Objective::FETCH) continue;
          if (objective.id != item->id()) continue;
          auto numNeededForQuest = objective.qty;
          auto numHeld = killer->countItems(item);
          qtyLimit = numNeededForQuest - numHeld;
          break;
        }
      }
    }

    if (qtyLimit != NO_LIMIT) quantity = min(quantity, qtyLimit);

    loot.add(item, quantity);
  }
}

std::pair<const ServerItem *, int> LootTable::SimpleEntry::instantiate() const {
  auto quantity = 0;
  if (randDouble() < chance) quantity = 1;
  return {item, quantity};
}

std::pair<const ServerItem *, int> LootTable::NormalEntry::instantiate() const {
  double rawQuantity = normalDist.generate();
  auto quantity = toInt(max<double>(0, rawQuantity));
  return {item, quantity};
}

std::pair<const ServerItem *, int> LootTable::ChoiceEntry::instantiate() const {
  const auto numChoices = choices.size();
  const auto randomIndex = rand() % numChoices;
  const auto chosenItem = choices[randomIndex];
  return chosenItem;
}

bool LootTable::SimpleEntry::operator==(const LootEntry &rhs) const {
  auto *simpleRHS = dynamic_cast<const SimpleEntry *>(&rhs);
  if (!simpleRHS) return false;
  if (item != simpleRHS->item) return false;
  if (chance != simpleRHS->chance) return false;
  return true;
}

bool LootTable::NormalEntry::operator==(const LootEntry &rhs) const {
  auto *normalRHS = dynamic_cast<const NormalEntry *>(&rhs);
  if (!normalRHS) return false;
  if (item != normalRHS->item) return false;
  if (!(normalDist == normalRHS->normalDist)) return false;
  return true;
}

bool LootTable::ChoiceEntry::operator==(const LootEntry &rhs) const {
  // These == operators were added for testability.  No tests cover this.
  return false;
}
