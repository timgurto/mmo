#pragma once

#include <set>
#include <string>

#include "../types.h"

class List;
class Client;

struct GroupUI {
  GroupUI(Client& client);
  void clear();
  void refresh();
  void addMember(const std::string name);

  void onPlayerLevelChange(Username name, Level newLevel);
  void onPlayerHealthChange(Username name, Hitpoints newHealth);
  void onPlayerEnergyChange(Username name, Energy newEnergy);
  void onPlayerMaxHealthChange(Username name, Hitpoints newMaxHealth);
  void onPlayerMaxEnergyChange(Username name, Energy newMaxEnergy);

  struct Member {
    std::string name;
    std::string level{"?"};
    Hitpoints health{1}, maxHealth{1};
    Energy energy{1}, maxEnergy{1};

    Member(std::string name) : name(name) {}
    bool operator<(const Member& rhs) const { return name < rhs.name; }
  };

  Member* findMember(Username name);

  std::set<Member> otherMembers;

  List* container{nullptr};

  Client& _client;
};
