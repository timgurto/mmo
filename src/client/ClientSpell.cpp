#include <cassert>
#include <sstream>

#include "Client.h"
#include "ClientSpell.h"
#include "TooltipBuilder.h"

ClientSpell::ClientSpell(const std::string &id) :
    _id(id),
    _castMessage(Client::compileMessage(CL_CAST, id)),
    _learnMessage(Client::compileMessage(CL_LEARN_SPELL, id)),
    _icon("Images/Spells/"s + id + ".png"s){
}

const Texture &ClientSpell::tooltip() const {
    if (_tooltip)
        return _tooltip;

    const auto &client = Client::instance();

    auto tb = TooltipBuilder{};
    tb.setColor(Color::ITEM_NAME);
    tb.addLine(_name);

    tb.addGap();

    if (_school.isMagic()) {
        tb.setColor(_school.color());
        tb.addLine(_school);
    }

    tb.setColor(Color::ITEM_STATS);
    tb.addLine("Energy cost: "s + toString(_cost));
    
    if (!_isAoE)
        tb.addLine("Range: "s + toString(_range) + " podes"s);

    tb.setColor(Color::ITEM_INSTRUCTIONS);
    tb.addLine(createEffectDescription());

    _tooltip = tb.publish();
    return _tooltip;
}

std::string ClientSpell::createEffectDescription() const {
    std::ostringstream oss;

    auto targetString = _isAoE ?
        "all targets within "s + toString(_range) + " podes"s :
        "target"s;

    if (_effectName == "doDirectDamage")
        oss << "Deals " << _effectArgs.i1 << " damage to " << targetString << ".";

    else if (_effectName == "heal")
        oss << "Restores " << _effectArgs.i1 << " health to target.";

    return oss.str();
}
