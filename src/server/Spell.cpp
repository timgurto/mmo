#include <string>

#include "Server.h"
#include "Spell.h"
#include "User.h"

CombatResult Spell::performAction(Entity &caster, Entity &target) const {
    if (!_effect.exists())
        return FAIL;

    const Server &server = Server::instance();
    const auto *casterAsUser = dynamic_cast<const User *>(&caster);

    auto skipWarnings = _effect.isAoE(); // Since there may be many targets, not all valid.

    // Target check
    if (!isTargetValid(caster, target)) {
        if (casterAsUser && !skipWarnings)
            server.sendMessage(casterAsUser->socket(), SV_INVALID_SPELL_TARGET);
        return FAIL;
    }

    if (target.isDead()){
        if (casterAsUser && !skipWarnings)
            server.sendMessage(casterAsUser->socket(), SV_TARGET_DEAD);
        return FAIL;
    }

    // Range check
    if (distance(caster.location(), target.location()) > _range) {
        if (casterAsUser && !skipWarnings)
            server.sendMessage(casterAsUser->socket(), SV_TOO_FAR);
        return FAIL;
    }

    return _effect.execute(caster, target);
}

bool Spell::isTargetValid(const Entity &caster, const Entity &target) const {

    if (caster.classTag() != 'u')
        return false; // For now, forbid non-user casters

    if (&caster == &target)
        return _validTargets[SELF];

    const auto &casterAsUser = dynamic_cast<const User &>(caster);

    if (target.canBeAttackedBy(casterAsUser))
        return _validTargets[ENEMY];

    return _validTargets[FRIENDLY];
}
