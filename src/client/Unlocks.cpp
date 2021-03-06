#include "Unlocks.h"

#include "../util.h"

using namespace std::string_literals;

bool operator<(const Unlocks::Trigger &lhs, const Unlocks::Trigger &rhs) {
  if (lhs.type != rhs.type) return lhs.type < rhs.type;
  return lhs.id < rhs.id;
}

bool operator<(const Unlocks::Effect &lhs, const Unlocks::Effect &rhs) {
  if (lhs.type != rhs.type) return lhs.type < rhs.type;
  return lhs.id < rhs.id;
}

void Unlocks::linkToKnownRecipes(const KnownEffects &knownRecipes) {
  _knownRecipes = &knownRecipes;
}

void Unlocks::linkToKnownConstructions(const KnownEffects &knownConstructions) {
  _knownConstructions = &knownConstructions;
}

void Unlocks::add(const Trigger &trigger, const Effect &effect, double chance) {
  _container[trigger][effect] = chance;
}

Unlocks::EffectInfo Unlocks::getEffectInfo(const Trigger &trigger) const {
  auto ret = EffectInfo{};

  auto it = _container.find(trigger);
  if (it == _container.end()) return ret;

  // Check against what has already been unlocked
  auto highestChance = 0.0;
  for (const auto &effectPair : it->second) {
    const auto &effect = effectPair.first;
    const auto &knownEffects =
        effect.type == RECIPE ? *_knownRecipes : *_knownConstructions;
    auto alreadyKnown = knownEffects.find(effect.id) != knownEffects.end();
    if (!alreadyKnown) {
      ret.hasEffect = true;
      highestChance = max(highestChance, effectPair.second);
    }
  }

  if (!ret.hasEffect) return ret;

  ret.chance = highestChance;

  auto chanceDescription = "";
  if (ret.chance <= 0.05) {
    chanceDescription = "small";
    ret.color = Color::CHANCE_SMALL;
  } else if (ret.chance <= 0.4) {
    chanceDescription = "moderate";
    ret.color = Color::CHANCE_MODERATE;
  } else {
    chanceDescription = "high";
    ret.color = Color::CHANCE_HIGH;
  }

  auto actionDescription = ""s;
  switch (trigger.type) {
    case CRAFT:
      actionDescription = "Crafting this recipe";
      break;
    case ACQUIRE:
      actionDescription = "Picking up this item";
      break;
    case GATHER:
      actionDescription = "Gathering from this object";
      break;
    case CONSTRUCT:
      actionDescription = "Constructing this object";
      break;
  }

  ret.message = actionDescription + " has a "s + chanceDescription +
                " chance to unlock something.";
  return ret;
}
