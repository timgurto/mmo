#include <cassert>

#include "Stats.h"
#include "util.h"

Stats &Stats::operator+=(const Stats &rhs){
    health += rhs.health;
    energy += rhs.energy;
    hps += rhs.hps;
    eps += rhs.eps;
    hit += rhs.hit;
    crit += rhs.crit;
    dodge += rhs.dodge;
    magicDamage += rhs.magicDamage;
    airResist += rhs.airResist;
    earthResist += rhs.earthResist;
    fireResist += rhs.fireResist;
    waterResist += rhs.waterResist;
    attack += rhs.attack;
    attackTime += rhs.attackTime;
    speed += rhs.speed;

    return *this;
}

const Stats Stats::operator+(const Stats &rhs) const{
    Stats ret = *this;
    ret += rhs;
    return ret;
}

const Stats &Stats::operator&=(const StatsMod &mod){
    if (mod.health < 0 && -mod.health > static_cast<int>(health))
        health = 0;
    else
        health += mod.health;

    if (mod.energy < 0 && -mod.energy > static_cast<int>(energy))
        energy = 0;
    else
        energy += mod.energy;

    if (mod.hps < 0 && -mod.hps > static_cast<int>(hps))
        hps = 0;
    else
        hps += mod.hps;

    if (mod.eps < 0 && -mod.eps > static_cast<int>(eps))
        eps = 0;
    else
        eps += mod.eps;

    hit += mod.hit;
    if (hit < 0) hit = 0;

    crit += mod.crit;
    if (crit < 0) crit = 0;

    dodge += mod.dodge;
    if (dodge < 0) dodge = 0;

    magicDamage += mod.magicDamage; // Can be negative.

    airResist += mod.airResist;
    if (airResist < 0) airResist = 0;

    earthResist += mod.earthResist;
    if (earthResist < 0) earthResist = 0;

    fireResist += mod.fireResist;
    if (fireResist < 0) fireResist = 0;

    waterResist += mod.waterResist;
    if (waterResist < 0) waterResist = 0;

    if (mod.attack < 0 && -mod.attack > static_cast<int>(attack))
        attack = 0;
    else
        attack += mod.attack;

    assert(mod.attackTime >= 0);
    if (mod.attackTime != 1.0)
        attackTime = toInt(attackTime * mod.attackTime);
    
    assert(mod.speed >= 0);
    if (mod.speed != 1.0)
        speed *= mod.speed;

    return *this;
}

Stats Stats::operator&(const StatsMod &mod) const{
    Stats stats = *this;
    stats &= mod;
    return stats;
}
