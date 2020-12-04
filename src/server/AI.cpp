#include "AI.h"

#include "NPC.h"
#include "Server.h"
#include "User.h"

AI::AI(NPC &owner) : _owner(owner) { _homeLocation = _owner.location(); }

void AI::process(ms_t timeElapsed) {
  _owner.target(nullptr);

  _owner.getNewTargetsFromProximity(timeElapsed);

  _owner.target(_owner._threatTable.getTarget());

  const auto previousState = state;

  transitionIfNecessary();
  if (state != previousState) onTransition(previousState);
  act();
}

void AI::transitionIfNecessary() {
  const auto ATTACK_RANGE = _owner.attackRange() - Podes{1}.toPixels();

  auto distToTarget =
      _owner.target()
          ? distance(_owner.collisionRect(), _owner.target()->collisionRect())
          : 0;

  switch (state) {
    case IDLE:
      // There's a target to attack
      {
        auto canAttack =
            _owner.combatDamage() > 0 || _owner.npcType()->knownSpell();
        if (canAttack && _owner.target()) {
          if (distToTarget > ATTACK_RANGE) {
            state = CHASE;
            _owner.resetLocationUpdateTimer();
          } else
            state = ATTACK;
          break;
        }
      }

      // Follow owner, if nearby
      {
        if (order != ORDER_TO_FOLLOW) break;
        if (_owner.owner().type != Permissions::Owner::PLAYER) break;
        const auto *ownerPlayer =
            Server::instance().getUserByName(_owner.owner().name);
        if (ownerPlayer == nullptr) break;
        if (distance(ownerPlayer->collisionRect(), _owner.collisionRect()) <=
            FOLLOW_DISTANCE)
          break;
        state = PET_FOLLOW_OWNER;
        _owner._followTarget = ownerPlayer;
        break;
      }

      break;

    case PET_FOLLOW_OWNER: {
      if (order == ORDER_TO_STAY) {
        state = IDLE;
        break;
      }

      // There's a target to attack
      if (_owner.target()) {
        if (distToTarget > ATTACK_RANGE) {
          state = CHASE;
          _owner.resetLocationUpdateTimer();
        } else
          state = ATTACK;
        break;
      }

      const auto distanceFromOwner = distance(
          _owner._followTarget->collisionRect(), _owner.collisionRect());

      // Owner is close enough
      if (distanceFromOwner <= AI::FOLLOW_DISTANCE) {
        state = IDLE;
        _owner._followTarget = nullptr;
        break;
      }

      // Owner is too far away
      if (distanceFromOwner >= MAX_FOLLOW_RANGE) {
        state = IDLE;
        order = ORDER_TO_STAY;

        if (_owner.owner().type == Permissions::Owner::PLAYER) {
          auto *ownerPlayer =
              Server::instance().getUserByName(_owner.owner().name);
          if (ownerPlayer) ownerPlayer->followers.remove();
        }

        _owner._followTarget = nullptr;
        break;
      }
      break;
    }

    case CHASE:
      // Target has disappeared
      if (!_owner.target()) {
        state = IDLE;
        break;
      }

      // Target is dead
      if (_owner.target()->isDead()) {
        state = IDLE;
        break;
      }

      // NPC has gone too far from home location
      {
        auto distFromHome = distance(_homeLocation, _owner.location());
        if (distFromHome > _owner.npcType()->maxDistanceFromHome()) {
          state = IDLE;
          clearPath();
          _owner.teleportTo(_homeLocation);
          break;
        }
      }

      // Target has run out of range: give up
      if (distToTarget > PURSUIT_RANGE) {
        state = IDLE;
        _owner.tagger.clear();
        break;
      }

      // Target is close enough to attack
      if (distToTarget <= ATTACK_RANGE) {
        state = ATTACK;
        break;
      }

      break;

    case AI::ATTACK:
      // Target has disappeared
      if (_owner.target() == nullptr) {
        state = IDLE;
        break;
      }

      // Target is dead
      if (_owner.target()->isDead()) {
        state = IDLE;
        break;
      }

      // Target has run out of attack range: chase
      if (distToTarget > ATTACK_RANGE) {
        state = CHASE;
        _owner.resetLocationUpdateTimer();
        break;
      }

      break;
  }
}

void AI::onTransition(State previousState) {
  auto previousLocation = _owner.location();

  switch (state) {
    case IDLE: {
      if (previousState != CHASE && previousState != ATTACK) break;
      _owner.target(nullptr);
      _owner._threatTable.clear();
      _owner.tagger.clear();

      if (_owner.owner().type != Permissions::Owner::ALL_HAVE_ACCESS) return;

      const auto ATTEMPTS = 20;
      for (auto i = 0; i != ATTEMPTS; ++i) {
        if (!_owner.spawner()) break;

        auto dest = _owner.spawner()->getRandomPoint();
        if (Server::instance().isLocationValid(dest, *_owner.type())) {
          clearPath();
          _owner.teleportTo(dest);
          break;
        }
      }

      auto maxHealth = _owner.type()->baseStats().maxHealth;
      if (_owner.health() < maxHealth) {
        _owner.health(maxHealth);
        _owner.onHealthChange();  // Only broadcasts to the new location, not
                                  // the old.

        for (const User *user :
             Server::instance().findUsersInArea(previousLocation))
          user->sendMessage(
              {SV_ENTITY_HEALTH, makeArgs(_owner.serial(), _owner.health())});
      }
      break;
    }

    case CHASE:
      clearPath();
      setDirectPathTo(_owner.target()->location());
      break;

    case PET_FOLLOW_OWNER:
      clearPath();
      setDirectPathTo(_owner.followTarget()->location());
      break;
  }
}

void AI::act() {
  switch (state) {
    case IDLE:
      _owner.target(nullptr);
      break;

    case PET_FOLLOW_OWNER:
    case CHASE: {
      // Move towards target
      auto outcome = _owner.moveLegallyTowards(_pathToDestination.front());
      if (outcome == Entity::DID_NOT_MOVE) {
        clearPath();
        _pathToDestination.push({95, 50});
        _pathToDestination.push({10, 50});
        _pathToDestination.push(_owner.target()->location());
      } else if (_owner.location() == _pathToDestination.front())
        _pathToDestination.pop();
    }

    break;

    case ATTACK:
      // Cast any spells it knows
      auto knownSpell = _owner.npcType()->knownSpell();
      if (knownSpell) {
        if (_owner.isSpellCoolingDown(knownSpell->id())) break;
        _owner.castSpell(*knownSpell);
      }
      break;  // Entity::update() will handle combat
  }
}

void AI::giveOrder(PetOrder newOrder) {
  order = newOrder;
  _homeLocation = _owner.location();

  // Send order confirmation to owner
  auto owner = _owner.permissions.getPlayerOwner();
  if (!owner) return;
  auto serialArg = makeArgs(_owner.serial());
  switch (order) {
    case ORDER_TO_STAY:
      owner->sendMessage({SV_PET_IS_NOW_STAYING, serialArg});
      break;

    case ORDER_TO_FOLLOW:
      owner->sendMessage({SV_PET_IS_NOW_FOLLOWING, serialArg});
      break;

    default:
      break;
  }
}

void AI::setDirectPathTo(const MapPoint &destination) {
  clearPath();
  _pathToDestination.push(destination);
}

void AI::clearPath() { _pathToDestination = {}; }
