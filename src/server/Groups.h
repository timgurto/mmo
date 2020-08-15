#pragma once

#include <set>
#include <vector>

class User;

class Groups {
 public:
  using Group = std::set<User*>;

  void createGroup(User& founder);
  void addToGroup(User& newMember, User& leader);

  Group getMembersOfPlayersGroup(User& aMember) const;
  int getGroupSize(const User& u) const;
  bool isUserInAGroup(const User& u) const;

  bool _invitationSent{false};
  bool _aUserIsInAGroup{false};

 private:
  std::vector<Group> _groups;
};
