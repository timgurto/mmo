#pragma once

#include <map>
#include <set>
#include <string>

#include "SpellEffect.h"
#include "../Stats.h"


class BuffType {
public:
    enum Type {
        UNKNOWN,
        
        STATS,
        SPELL_OVER_TIME
    };

    using ID = std::string;

    BuffType() {}
    BuffType(const ID &id) : _id(id) {}

    const ID &id() const { return _id; }

    void stats(const StatsMod &stats);
    const StatsMod &stats() const { return _stats; }

    SpellEffect &effect();
    const SpellEffect &effect() const { return _effect; }
    void tickTime(ms_t t) { _tickTime = t; }
    ms_t tickTime() const { return _tickTime; }
    void duration(ms_t t) { _duration = t; }
    ms_t duration() const { return _duration; }

private:
    ID _id{};
    Type _type = UNKNOWN;

    StatsMod _stats{};

    SpellEffect _effect{};
    ms_t _tickTime{ 0 };
    ms_t _duration{ 0 }; // 0: Never ends
};

// An instance of a buff type, on a specific target
class Buff {
public:
    using ID = std::string;

    Buff(const BuffType &type, Entity &owner, Entity &caster);

    const ID &type() const { return _type.id(); }
    bool hasExpired() const { return _expired; }

    bool operator<(const Buff &rhs) const { return &_type < &rhs._type; }

    void applyStatsTo(Stats &stats) const { stats &= _type.stats(); }

    void update(ms_t timeElapsed);

private:
    const BuffType &_type;
    Entity &_owner;
    Entity &_caster;

    ms_t _timeSinceLastProc{ 0 };
    ms_t _timeRemaining{ 0 };

    bool _expired{ false }; // A signal that the buff should be removed before the next update()
};

using BuffTypes = std::map<Buff::ID, BuffType>;
using Buffs = std::set<Buff>;
