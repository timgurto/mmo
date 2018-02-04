#ifndef CLIENT_ITEM_H
#define CLIENT_ITEM_H

#include <map>

#include "SoundProfile.h"
#include "Texture.h"
#include "Tooltip.h"
#include "../Item.h"
#include "../Optional.h"

class ClientObjectType;
class SoundProfile;

// The client-side representation of an item type
class ClientItem : public Item{
    std::string _name;
    Texture _icon;
    Texture _gearImage;
    ScreenPoint _drawLoc;
    mutable Optional<Tooltip> _tooltip;
    const SoundProfile *_sounds;
    
    struct Particles {
        std::string profile;
        MapPoint offset; // Relative to gear offset for that slot.
    };
    std::vector<Particles> _particles;

    // The object that this item can construct
    const ClientObjectType *_constructsObject;

    static std::map<int, size_t> gearDrawOrder;
    static std::vector<ScreenPoint> gearOffsets;

public:
    ClientItem(const std::string &id = "", const std::string &name = "");

    const std::string &name() const { return _name; }
    const Texture &icon() const { return _icon; }
    void icon(const std::string &filename);
    void gearImage(const std::string &filename);
    void drawLoc(const ScreenPoint &loc) { _drawLoc = loc; }
    static const std::map<int, size_t> &drawOrder() { return gearDrawOrder; }
    void sounds(const std::string &id);
    const SoundProfile *sounds() const { return _sounds; }
    bool canUse() const;

    static const ScreenPoint &gearOffset(size_t slot) { return gearOffsets[slot]; }

    void fetchAmmoItem() const override;

    void addParticles(const std::string &profileName, const MapPoint &offset);
    const std::vector<Particles> &particles() const { return _particles; }

    typedef std::vector<std::pair<const ClientItem *, size_t> > vect_t;

    void constructsObject(const ClientObjectType *obj) { _constructsObject = obj; }
    const ClientObjectType *constructsObject() const { return _constructsObject; }

    const Tooltip &tooltip() const; // Getter; creates tooltip on first call.

    void draw(const MapPoint &loc) const;

    static void init();
};

const ClientItem *toClientItem(const Item *item);

#endif
