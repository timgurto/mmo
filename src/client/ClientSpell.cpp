#include <cassert>

#include "Client.h"
#include "ClientSpell.h"
#include "TooltipBuilder.h"

ClientSpell::ClientSpell(const std::string &id) :
    _castMessage(Client::compileMessage(CL_CAST, id)),
    _icon("Images/Spells/"s + id + ".png"s){
    
    assert(_icon);
}

const Texture &ClientSpell::tooltip() const {
    if (_tooltip)
        return _tooltip;

    const auto &client = Client::instance();

    auto tb = TooltipBuilder{};
    tb.setColor(Color::ITEM_NAME);
    tb.addLine(_name);

    _tooltip = tb.publish();
    return _tooltip;
}
