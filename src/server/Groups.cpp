#include "Groups.h"

#include <cassert>

#include "Server.h"
#include "User.h"

#ifdef TESTING
#define LOCK_GROUPS_BY_USER _groupsByUserMutex.lock();
#define UNLOCK_GROUPS_BY_USER _groupsByUserMutex.unlock();
#else
#define LOCK_GROUPS_BY_USER
#define UNLOCK_GROUPS_BY_USER
#endif

Groups::Group* Groups::getGroupAndMakeIfNeeded(Username inviter) {
  LOCK_GROUPS_BY_USER
  auto it = _groupsByUser.find(inviter);
  auto inviterIsInAGroup = it != _groupsByUser.end();
  UNLOCK_GROUPS_BY_USER

  if (inviterIsInAGroup)
    return it->second;
  else {
    return createGroup(inviter);
  }
}

Groups::Group* Groups::createGroup(Username founder) {
  auto newGroup = new Group;
  newGroup->insert(founder);
  LOCK_GROUPS_BY_USER
  _groupsByUser[founder] = newGroup;
  UNLOCK_GROUPS_BY_USER
  ++_numGroups;
  return newGroup;
}

void Groups::addToGroup(Username newMember, Username inviter) {
  auto* group = getGroupAndMakeIfNeeded(inviter);

  group->insert(newMember);
  LOCK_GROUPS_BY_USER
  _groupsByUser[newMember] = group;
  UNLOCK_GROUPS_BY_USER
  sendGroupMakeupToAllMembers(*group);
}

Groups::Group Groups::getUsersGroup(Username player) const {
  LOCK_GROUPS_BY_USER
  auto it = _groupsByUser.find(player);
  auto userIsInAGroup = it != _groupsByUser.end();
  UNLOCK_GROUPS_BY_USER

  if (userIsInAGroup) return *it->second;

  auto soloResult = Group{player};
  return soloResult;
}

int Groups::getGroupSize(Username u) const {
  LOCK_GROUPS_BY_USER
  auto it = _groupsByUser.find(u);
  auto userIsInAGroup = it != _groupsByUser.end();
  UNLOCK_GROUPS_BY_USER

  if (userIsInAGroup)
    return it->second->size();
  else
    return 1;
}

bool Groups::isUserInAGroup(Username u) const {
  LOCK_GROUPS_BY_USER
  auto ret = _groupsByUser.count(u) == 1;
  UNLOCK_GROUPS_BY_USER
  return ret;
}

bool Groups::areUsersInSameGroup(Username lhs, Username rhs) const {
  auto lhsGroup = getUsersGroup(lhs);
  return lhsGroup.count(rhs) == 1;
}

void Groups::registerInvitation(Username existingMember, Username newMember) {
  _inviterOf[newMember] = existingMember;
}

bool Groups::userHasAnInvitation(Username u) const {
  return _inviterOf.count(u) == 1;
}

void Groups::acceptInvitation(Username newMember) {
  auto inviter = _inviterOf[newMember];
  addToGroup(newMember, inviter);
}

void Groups::removeUserFromHisGroup(Username quitter) {
  auto formerGroup = getUsersGroup(quitter);
  formerGroup.erase(quitter);
  for (auto remainingMember : formerGroup) {
    LOCK_GROUPS_BY_USER
    auto it = _groupsByUser.find(remainingMember);
    auto quitterNotInGroup = it == _groupsByUser.end();
    UNLOCK_GROUPS_BY_USER
    if (quitterNotInGroup) continue;
    auto& membersPersonalGroupView = *it->second;
    membersPersonalGroupView.erase(quitter);
  }
  sendGroupMakeupToAllMembers(formerGroup);

  auto* quitterUser = Server::instance().getUserByName(quitter);
  if (quitterUser) quitterUser->sendMessage({SV_GROUPMATES, "0"});

  LOCK_GROUPS_BY_USER
  _groupsByUser.erase(quitter);
  if (formerGroup.size() == 1) _groupsByUser.erase(*formerGroup.begin());
  UNLOCK_GROUPS_BY_USER
}

void Groups::sendGroupMakeupToAllMembers(const Group& g) {
  for (auto memberName : g) {
    auto* onlineUser = Server::instance().getUserByName(memberName);
    if (onlineUser) sendGroupMakeupTo(g, *onlineUser);
  }
}

void Groups::sendGroupMakeupTo(const Group& g, const User& recipient) {
  auto args = makeArgs(g.size() - 1);

  for (auto memberName : g) {
    if (memberName == recipient.name()) continue;
    args = makeArgs(args, memberName);
  }

  recipient.sendMessage({SV_GROUPMATES, args});
}
