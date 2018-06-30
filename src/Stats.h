#ifndef STATS_H
#define STATS_H

#include <vector>

#include "SpellSchool.h"
#include "types.h"

struct StatsMod;

// Describes base-level player stats.
struct Stats {
  Hitpoints maxHealth = 0, blockValue = 0;

  Energy maxEnergy = 0;

  Regen hps = 0, eps = 0;

  Percentage armor = 0, hit = 0, crit = 0, critResist = 0, dodge = 0, block = 0,
             airResist = 0, earthResist = 0, fireResist = 0, waterResist = 0,
             gatherBonus = 0;

  BonusDamage magicDamage = 0, physicalDamage = 0, healing = 0;

  ms_t attackTime = 0;

  double speed = 0;

  bool stunned = false;

  Stats &operator+=(const Stats &rhs);
  const Stats operator+(const Stats &rhs) const;

  const Stats &operator&=(const StatsMod &rhs);
  Stats operator&(const StatsMod &mod) const;

  Percentage resistanceByType(SpellSchool school) const;
};

// Describes modifiers for player stats, e.g. for gear.
struct StatsMod {
  std::vector<std::string> toStrings() const;
  std::string buffDescription() const;  // e.g. "Grant 1 health to ", "Stun "

  // Additive
  int armor = 0, maxHealth = 0, hps = 0, maxEnergy = 0, eps = 0, blockValue = 0;
  BonusDamage magicDamage = 0, physicalDamage = 0, healing = 0;
  Percentage hit = 0, crit = 0, critResist = 0, dodge = 0, block = 0,
             airResist = 0, earthResist = 0, fireResist = 0, waterResist = 0,
             gatherBonus = 0;

  // Multiplicative
  double attackTime = 1.0, speed = 1.0;

  // Flag
  bool stuns = false;
};

#endif
