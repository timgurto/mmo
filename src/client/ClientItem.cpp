#include "Client.h"
#include "ClientItem.h"
#include "Tooltip.h"
#include "../XmlReader.h"

std::map<int, size_t> ClientItem::gearDrawOrder;
std::vector<ScreenPoint> ClientItem::gearOffsets(Client::GEAR_SLOTS);

ClientItem::ClientItem(const std::string &id, const std::string &name):
Item(id),
_name(name),
_constructsObject(nullptr),
_sounds(nullptr)
{}

void ClientItem::icon(const std::string &filename){
    static const std::string
        prefix = "Images/Items/",
        suffix = ".png";
    _icon = Texture(prefix + filename + suffix);
}

void ClientItem::gearImage(const std::string &filename){
    static const std::string
        prefix = "Images/Gear/",
        suffix = ".png";
    _gearImage = Texture(prefix + filename + suffix, Color::MAGENTA);
}

const ClientItem *toClientItem(const Item *item){
    return dynamic_cast<const ClientItem *>(item);
}

static ScreenPoint toScreenPoint(const MapPoint &rhs) {
    return{ toInt(rhs.x), toInt(rhs.y) };
}

void ClientItem::draw(const MapPoint &loc) const{
    if (_gearSlot <= Client::GEAR_SLOTS && _gearImage){
        ScreenPoint drawLoc =
            _drawLoc +                   // The item's offset
            gearOffsets[_gearSlot] +     // The slot's offset
            toScreenPoint(loc) +      // The avatar's location
            Client::_instance->offset(); // The overall map offset
        _gearImage.draw(drawLoc);
    }
}

void ClientItem::init(){
    XmlReader xr("client-config.xml");
    auto elem = xr.findChild("gearDisplay");
    if (elem == nullptr)
        return;
    for (auto slot : xr.getChildren("slot", elem)){
        size_t slotNum;
        if (!xr.findAttr(slot, "num", slotNum))
            continue;
        
        // Offsets
        xr.findAttr(slot, "midX", gearOffsets[slotNum].x);
        xr.findAttr(slot, "midY", gearOffsets[slotNum].y);
        
        // Draw order.  Without this, gear for this slot won't be drawn.
        size_t order;
        if (xr.findAttr(slot, "drawOrder", order))
            gearDrawOrder[order] = slotNum;
    }
}

const Tooltip &ClientItem::tooltip() const{
    if (_tooltip.hasValue())
        return _tooltip.value();

    const auto &client = *Client::_instance;

    _tooltip = Tooltip{};
    auto &tooltip = _tooltip.value();

    tooltip.setColor(Color::ITEM_NAME);
    tooltip.addLine(_name);

    // Gear slot/stats
    if (_gearSlot != Client::GEAR_SLOTS){
        tooltip.addGap();
        tooltip.setColor(Color::ITEM_STATS);
        tooltip.addLine("Gear: "s + Client::GEAR_SLOT_NAMES[_gearSlot]);

        if (weaponRange() > Podes::MELEE_RANGE.toPixels())
            tooltip.addLine("Range: "s + toString(Podes::FromPixels(weaponRange())) + " podes");

        if (usesAmmo()) {
            auto &ammoType = dynamic_cast<const ClientItem &>(*weaponAmmo());
            tooltip.addLine("Each attack consumes a "s + ammoType.name());
        }

        tooltip.addLines(_stats.toStrings());
    }

    // Tags
    if (hasTags()){
        tooltip.addGap();
        tooltip.setColor(Color::ITEM_TAGS);
        for (const std::string &tag : tags())
            tooltip.addLine(client.tagName(tag));
    }
    
    // Construction
    if (_constructsObject != nullptr){
        tooltip.addGap();
        tooltip.setColor(Color::ITEM_INSTRUCTIONS);
        tooltip.addLine(std::string("Right-click to place ") + _constructsObject->name());
        if (!_constructsObject->constructionReq().empty())
            tooltip.addLine("(Requires " + client.tagName(_constructsObject->constructionReq()) + ")");

        // Vehicle?
        if (_constructsObject->classTag() == 'v'){
            tooltip.setColor(Color::ITEM_STATS);
            tooltip.addLine("  Vehicle");
        }

        if (_constructsObject->containerSlots() > 0){
            tooltip.setColor(Color::ITEM_STATS);
            tooltip.addLine("  Container: " + toString(_constructsObject->containerSlots()) + " slots");
        }

        if (_constructsObject->merchantSlots() > 0){
            tooltip.setColor(Color::ITEM_STATS);
            tooltip.addLine("  Merchant: " + toString(_constructsObject->merchantSlots()) + " slots");
        }

        // Tags
        if (_constructsObject->hasTags()){
            tooltip.setColor(Color::ITEM_TAGS);
            for (const std::string &tag : _constructsObject->tags())
                tooltip.addLine("  " + client.tagName(tag));
        }
    }

    // Spell
    if (castsSpellOnUse()) {
        auto it = client._spells.find(spellToCastOnUse());
        if (it == client._spells.end()) {
            client.debug() << Color::FAILURE << "Can't find spell: " << spellToCastOnUse() << Log::endl;
        } else {
            tooltip.setColor(Color::ITEM_STATS);
            tooltip.addLine("Right-click: "s + it->second->createEffectDescription());
        }
    }

    return tooltip;
}

void ClientItem::sounds(const std::string &id){
    const Client &client = *Client::_instance;
    _sounds = client.findSoundProfile(id);
}

bool ClientItem::canUse() const {
    return
        _constructsObject != nullptr ||
        castsSpellOnUse();
}

void ClientItem::fetchAmmoItem() const {
    if (_weaponAmmoID.empty())
        return;

    const Client &client = *Client::_instance;
    auto it = client._items.find(_weaponAmmoID);
    if (it == client._items.end()) {
        client.debug() << Color::FAILURE << "Unknown item "s << _weaponAmmoID
            << " specified as ammo"s << Log::endl;
        return;
    }
    _weaponAmmo = &(it->second);
}

void ClientItem::addParticles(const std::string & profileName, const MapPoint & offset) {
    Particles p;
    p.profile = profileName;
    p.offset = offset;
    _particles.push_back(p);
}
