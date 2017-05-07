#ifndef TARGET_HEADER
#define TARGET_HEADER

#include <string>

#include "ClientCombatant.h"
#include "Entity.h"
#include "ui/CombatantPanel.h"
#include "../types.h"

/*
Both pointers should contain the same value.  Having both is necessary because reinterpret_cast
doesn't appear to work.
*/
class Target{
public:
    Target();

    template<typename T>
    void setAndAlertServer(const T &newTarget, bool nowAggressive){
        setAndAlertServer(newTarget, newTarget, nowAggressive);
    }

    void clear();

    const Entity *entity() const { return _entity; }
    const ClientCombatant *combatant() const { return _combatant; }
    bool exists() const { return _entity != nullptr; }
    bool isAggressive() const { return _aggressive; }
    void makeAggressive() { _aggressive = true; }
    void makePassive() { _aggressive = false; }
    
    const std::string &name() const { return _name; }
    const health_t &health() const { return _health; }
    const health_t &maxHealth() const { return _maxHealth; }
    void refreshHealthBarColor();

    void updateHealth(health_t newHealth){ _health = newHealth; }

    void initializePanel();
    CombatantPanel *panel() { return _panel; }

private:
    const Entity *_entity;
    const ClientCombatant *_combatant;
    bool _aggressive; // True: will attack when in range.  False: mere selection, client-side only.

    /*
    Updated on set().  These are attributes, not functions, because the UI uses LinkedLabels that
    contain data references.
    */
    std::string _name;
    health_t _health, _maxHealth;

    void setAndAlertServer(
            const Entity &asEntity, const ClientCombatant &asCombatant, bool nowAggressive);
    bool targetIsDifferentFromServer(const Entity &newTarget, bool nowAggressive);

    CombatantPanel *_panel;
};

#endif
