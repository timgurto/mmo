#pragma once

#include <list>
#include <unordered_map>

#include "Spell.h"

// A single tier of a single tree.  3 class * 3 trees * 5 tiers = 45 Tiers total
struct Tier {
  std::string costTag;
  size_t costQuantity{0};
  std::string requiredTool{};
  size_t reqPointsInTree{0};

  bool hasItemCost() const { return !costTag.empty() && costQuantity > 0; }
};
using Tiers = std::list<Tier>;

class Talent {
 public:
  using Name = std::string;

  enum Type {
    SPELL,
    STATS,

    DUMMY
  };

  static Talent Dummy(const Name &name);
  static Talent Spell(const Name &name, const Spell::ID &id, const Tier &tier);
  static Talent Stats(const Name &name, const StatsMod &stats,
                      const Tier &tier);

  bool operator<(const Talent &rhs) const;

  Type type() const { return _type; }
  const Spell::ID &spellID() const { return _spellID; }
  const StatsMod &stats() const { return _stats; }
  const Tier &tier() const { return _tier; }
  void tree(const std::string &treeName) { _tree = treeName; }
  const std::string &tree() const { return _tree; }
  const Name &name() const { return _name; }

 private:
  Talent(const Name &name, Type type, const Tier &tier);

  Name _name{};
  Type _type{DUMMY};
  Spell::ID _spellID{};
  StatsMod _stats{};
  const Tier &_tier;
  std::string _tree;

  static const Tier DUMMY_TIER;
};

class ClassType {
 public:
  using ID = std::string;

  ClassType(const ID &id = {}) : _id(id) {}

  const ID &id() const { return _id; }

  Talent &addSpell(const Talent::Name &name, const Spell::ID &spellID,
                   const Tier &tier);
  Talent &addStats(const Talent::Name &name, const StatsMod &stats,
                   const Tier &tier);
  const Talent *findTalent(const Talent::Name &name) const;

  void setFreeSpell(const Spell::ID &id) { _freeSpell = id; }
  bool hasFreeSpell() const { return !_freeSpell.empty(); }
  const Spell::ID &freeSpell() const { return _freeSpell; }

 private:
  ID _id;
  std::set<Talent> _talents;
  Spell::ID _freeSpell;
};

using ClassTypes = std::map<ClassType::ID, ClassType>;

class User;

// A single user's instance of a ClassType
class Class {
 public:
  using TalentRanks = std::map<const Talent *, unsigned>;
  bool canTakeATalent() const;
  int talentPointsAvailable() const;
  void unlearnAll();

  Class() = default;
  Class(const ClassType &type, const User &owner);
  const ClassType &type() const { return *_type; }

  bool hasTalent(const Talent *talent) const;
  void takeTalent(const Talent *talent);
  bool knowsSpell(const Spell::ID &spell) const;
  void markSpellAsKnown(const Spell::ID &spell);
  void teachSpell(
      const Spell::ID &spell);  // Teach spell outside of talent system
   Spell::ID teachFreeSpellIfAny();
  const std::set<Spell::ID> &otherKnownSpells() const {
    return _otherKnownSpells;
  }
  std::string generateKnownSpellsString() const;
  void applyStatsTo(Stats &baseStats) const;
  size_t pointsInTree(const std::string &treeName) const;
  const TalentRanks &talentRanks() const { return _talentRanks; }
  void loadTalentRank(const Talent &talent, unsigned rank);
  Talent::Name loseARandomLeafTalent();
  bool isLeafTalent(
      const Talent &talent);  // Is it a necessary prereq for something else
  static bool allTalentsAreSupported(TalentRanks &talentRanks);
  static bool talentIsSupported(const Talent *talent, TalentRanks &talentRanks);

 private:
  const ClassType *_type{nullptr};
  TalentRanks _talentRanks{};
  int _talentPointsAllocated{0};  // Updated in takeTalent()
  const User *_owner{nullptr};
  std::set<Spell::ID> _otherKnownSpells{};
};
