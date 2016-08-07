// (C) 2016 Tim Gurto

#ifndef SINGLE_THREAD
#include <mutex>
#include <thread>
#endif
#include "Server.h"
#include "../XmlReader.h"
#include "../XmlWriter.h"

extern Args cmdLineArgs;

bool Server::readUserData(User &user){
    XmlReader xr((std::string("Users/") + user.name() + ".usr").c_str());
    if (!xr)
        return false;

    auto elem = xr.findChild("location");
    Point p;
    if (elem == nullptr || !xr.findAttr(elem, "x", p.x) || !xr.findAttr(elem, "y", p.y)) {
            _debug("Invalid user data (location)", Color::RED);
            return false;
    }
    bool randomizedLocation = false;
    while (!isLocationValid(p, User::OBJECT_TYPE)) {
        p = mapRand();
        randomizedLocation = true;
    }
    if (randomizedLocation)
        _debug << Color::YELLOW << "Player " << user.name()
               << " was moved due to an invalid location." << Log::endl;
    user.location(p);

    for (auto elem : xr.getChildren("inventory")) {
        for (auto slotElem : xr.getChildren("slot", elem)) {
            int slot; std::string id; int qty;
            if (!xr.findAttr(slotElem, "slot", slot)) continue;
            if (!xr.findAttr(slotElem, "id", id)) continue;
            if (!xr.findAttr(slotElem, "quantity", qty)) continue;

            std::set<Item>::const_iterator it = _items.find(id);
            if (it == _items.end()) {
                _debug("Invalid user data (inventory item).  Removing item.", Color::RED);
                continue;
            }
            user.inventory(slot) =
                std::make_pair<const Item *, size_t>(&*it, static_cast<size_t>(qty));
        }
    }

    elem = xr.findChild("stats");
    unsigned n;
    if (xr.findAttr(elem, "health", n))
        user.health(n);

    return true;

}

void Server::writeUserData(const User &user) const{
    XmlWriter xw(std::string("Users/") + user.name() + ".usr");

    auto e = xw.addChild("location");
    xw.setAttr(e, "x", user.location().x);
    xw.setAttr(e, "y", user.location().y);

    e = xw.addChild("inventory");
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i) {
        const std::pair<const Item *, size_t> &slot = user.inventory(i);
        if (slot.first) {
            auto slotElement = xw.addChild("slot", e);
            xw.setAttr(slotElement, "slot", i);
            xw.setAttr(slotElement, "id", slot.first->id());
            xw.setAttr(slotElement, "quantity", slot.second);
        }
    }

    e = xw.addChild("stats");
    xw.setAttr(e, "health", user.health());

    xw.publish();
}

void Server::loadData(){

    // Load terrain
    XmlReader xr("Data/terrain.xml");
    for (auto elem : xr.getChildren("terrain")) {
        int index;
        if (!xr.findAttr(elem, "index", index))
            continue;
        int isTraversable = 1;
        xr.findAttr(elem, "isTraversable", isTraversable);
        if (index >= static_cast<int>(_terrain.size()))
            _terrain.resize(index+1);
        _terrain[index] = Terrain(isTraversable != 0);
    }

    // Object types
    xr.newFile("Data/objectTypes.xml");
    for (auto elem : xr.getChildren("objectType")) {
        std::string id;
        if (!xr.findAttr(elem, "id", id))
            continue;
        ObjectType ot(id);

        std::string s; int n;
        if (xr.findAttr(elem, "gatherTime", n)) ot.gatherTime(n);
        if (xr.findAttr(elem, "constructionTime", n)) ot.constructionTime(n);
        if (xr.findAttr(elem, "gatherReq", s)) ot.gatherReq(s);
        if (xr.findAttr(elem, "deconstructs", s)){
            std::set<Item>::const_iterator itemIt = _items.insert(Item(s)).first;
            ot.deconstructsItem(&*itemIt);
        }
        if (xr.findAttr(elem, "deconstructionTime", n)) ot.deconstructionTime(n);
        for (auto yield : xr.getChildren("yield", elem)) {
            if (!xr.findAttr(yield, "id", s))
                continue;
            double initMean = 1., initSD = 0, gatherMean = 1, gatherSD = 0;
            xr.findAttr(yield, "initialMean", initMean);
            xr.findAttr(yield, "initialSD", initSD);
            xr.findAttr(yield, "gatherMean", gatherMean);
            xr.findAttr(yield, "gatherSD", gatherSD);
            std::set<Item>::const_iterator itemIt = _items.insert(Item(s)).first;
            ot.addYield(&*itemIt, initMean, initSD, gatherMean, gatherSD);
        }
        if (xr.findAttr(elem, "merchantSlots", n)) ot.merchantSlots(n);
        auto collisionRect = xr.findChild("collisionRect", elem);
        if (collisionRect) {
            Rect r;
            xr.findAttr(collisionRect, "x", r.x);
            xr.findAttr(collisionRect, "y", r.y);
            xr.findAttr(collisionRect, "w", r.w);
            xr.findAttr(collisionRect, "h", r.h);
            ot.collisionRect(r);
        }
        for (auto objClass :xr.getChildren("class", elem))
            if (xr.findAttr(objClass, "name", s))
                ot.addClass(s);
        auto container = xr.findChild("container", elem);
        if (container != nullptr) {
            if (xr.findAttr(container, "slots", n)) ot.containerSlots(n);
        }
        
        _objectTypes.insert(ot);
    }

    // Items
    xr.newFile("Data/items.xml");
    for (auto elem : xr.getChildren("item")) {
        std::string id, name;
        if (!xr.findAttr(elem, "id", id) || !xr.findAttr(elem, "name", name))
            continue; // ID and name are mandatory.
        Item item(id);

        std::string s; int n;
        if (xr.findAttr(elem, "stackSize", n)) item.stackSize(n);
        if (xr.findAttr(elem, "constructs", s))
            // Create dummy ObjectType if necessary
            item.constructsObject(&*(_objectTypes.insert(ObjectType(s)).first));

        for (auto child : xr.getChildren("class", elem))
            if (xr.findAttr(child, "name", s)) item.addClass(s);
        
        std::pair<std::set<Item>::iterator, bool> ret = _items.insert(item);
        if (!ret.second) {
            Item &itemInPlace = const_cast<Item &>(*ret.first);
            itemInPlace = item;
        }
    }

    // Recipes
    xr.newFile("Data/recipes.xml");
    for (auto elem : xr.getChildren("recipe")) {
        std::string id, name;
        if (!xr.findAttr(elem, "id", id))
            continue; // ID is mandatory.
        Recipe recipe(id);

        std::string s; int n;
        if (!xr.findAttr(elem, "product", s))
            continue; // product is mandatory.
        auto it = _items.find(s);
        if (it == _items.end()) {
            _debug << Color::RED << "Skipping recipe with invalid product " << s << Log::endl;
            continue;
        }
        recipe.product(&*it);

        if (xr.findAttr(elem, "time", n)) recipe.time(n);

        for (auto child : xr.getChildren("material", elem)) {
            int matQty = 1;
            xr.findAttr(child, "quantity", matQty);
            if (xr.findAttr(child, "id", s)) {
                auto it = _items.find(Item(s));
                if (it == _items.end()) {
                    _debug << Color::RED << "Skipping invalid recipe material " << s << Log::endl;
                    continue;
                }
                recipe.addMaterial(&*it, matQty);
            }
        }

        for (auto child : xr.getChildren("tool", elem)) {
            if (xr.findAttr(child, "name", s)) {
                recipe.addTool(s);
            }
        }
        
        _recipes.insert(recipe);
    }

    std::ifstream fs;
    // Detect/load state
    do {
        if (cmdLineArgs.contains("new"))
            break;

        // Map
        xr.newFile("World/map.world");
        if (!xr)
            break;
        auto elem = xr.findChild("size");
        if (elem == nullptr || !xr.findAttr(elem, "x", _mapX) || !xr.findAttr(elem, "y", _mapY)) {
            _debug("Map size missing or incomplete.", Color::RED);
            break;
        }
        _map = std::vector<std::vector<size_t> >(_mapX);
        for (size_t x = 0; x != _mapX; ++x)
            _map[x] = std::vector<size_t>(_mapY, 0);
        for (auto row : xr.getChildren("row")) {
            size_t y;
            if (!xr.findAttr(row, "y", y) || y >= _mapY)
                break;
            for (auto tile : xr.getChildren("tile", row)) {
                size_t x;
                if (!xr.findAttr(tile, "x", x) || x >= _mapX)
                    break;
                int index;
                if (!xr.findAttr(tile, "terrain", index))
                    break;
                _map[x][y] = index;
            }
        }

        // Objects
        xr.newFile("World/objects.world");
        if (!xr)
            break;
        for (auto elem : xr.getChildren("object")) {
            std::string s;
            if (!xr.findAttr(elem, "id", s)) {
                _debug("Skipping importing object with no type.", Color::RED);
                continue;
            }

            Point p;
            auto loc = xr.findChild("location", elem);
            if (!xr.findAttr(loc, "x", p.x) || !xr.findAttr(loc, "y", p.y)) {
                _debug("Skipping importing object with invalid/no location", Color::RED);
                continue;
            }

            std::set<ObjectType>::const_iterator it = _objectTypes.find(s);
            if (it == _objectTypes.end()) {
                _debug << Color::RED << "Skipping importing object with unknown type \"" << s
                       << "\"." << Log::endl;
                continue;
            }

            Object &obj = addObject(&*it, p, nullptr);

            size_t n;
            if (xr.findAttr(elem, "owner", s)) obj.owner(s);

            ItemSet contents;
            for (auto content : xr.getChildren("gatherable", elem)) {
                if (!xr.findAttr(content, "id", s))
                    continue;
                n = 1;
                xr.findAttr(content, "quantity", n);
                contents.set(&*_items.find(s), n);
            }
            obj.contents(contents);

            size_t q;
            for (auto inventory : xr.getChildren("inventory", elem)) {
                if (!xr.findAttr(inventory, "item", s))
                    continue;
                if (!xr.findAttr(inventory, "slot", n))
                    continue;
                q = 1;
                xr.findAttr(inventory, "qty", q);
                if (obj.container().size() <= n) {
                    _debug << Color::RED << "Skipping object with invalid inventory slot." << Log::endl;
                    continue;
                }
                auto &invSlot = obj.container()[n];
                invSlot.first = &*_items.find(s);
                invSlot.second = q;
            }

            for (auto merchant : xr.getChildren("merchant", elem)) {
                size_t slot;
                if (!xr.findAttr(merchant, "slot", slot))
                    continue;
                std::string wareName, priceName;
                if (!xr.findAttr(merchant, "wareItem", wareName) ||
                    !xr.findAttr(merchant, "priceItem", priceName))
                    continue;
                auto wareIt = _items.find(wareName);
                if (wareIt == _items.end())
                    continue;
                auto priceIt = _items.find(priceName);
                if (priceIt == _items.end())
                    continue;
                size_t wareQty = 1, priceQty = 1;
                xr.findAttr(merchant, "wareQty", wareQty);
                xr.findAttr(merchant, "priceQty", priceQty);
                obj.merchantSlot(slot) = MerchantSlot(&*wareIt, wareQty, &*priceIt, priceQty);
            }
        }

        return;
    } while (false);

    _debug("No/invalid world data detected; generating new world.", Color::YELLOW);
    generateWorld();
}

void Server::saveData(const std::set<Object> &objects){
    // Map // TODO: Only save map once, on generation.
#ifndef SINGLE_THREAD
    static std::mutex mapFileMutex;
    mapFileMutex.lock();
#endif
    XmlWriter xw("World/map.world");
    auto e = xw.addChild("size");
    const Server &server = Server::instance();
    xw.setAttr(e, "x", server._mapX);
    xw.setAttr(e, "y", server._mapY);
    for (size_t y = 0; y != server._mapY; ++y){
        auto row = xw.addChild("row");
        xw.setAttr(row, "y", y);
        for (size_t x = 0; x != server._mapX; ++x){
            auto tile = xw.addChild("tile", row);
            xw.setAttr(tile, "x", x);
            xw.setAttr(tile, "terrain", server._map[x][y]);
        }
    }
    xw.publish();
#ifndef SINGLE_THREAD
    mapFileMutex.unlock();
#endif

    // Objects
#ifndef SINGLE_THREAD
    static std::mutex objectsFileMutex;
    objectsFileMutex.lock();
#endif
    xw.newFile("World/objects.world");
    for (const Object &obj : objects) {
        if (obj.type() == nullptr)
            continue;
        auto e = xw.addChild("object");

        xw.setAttr(e, "id", obj.type()->id());

        for (auto &content : obj.contents()) {
            auto contentE = xw.addChild("gatherable", e);
            xw.setAttr(contentE, "id", content.first->id());
            xw.setAttr(contentE, "quantity", content.second);
        }

        if (!obj.owner().empty())
            xw.setAttr(e, "owner", obj.owner());

        auto loc = xw.addChild("location", e);
        xw.setAttr(loc, "x", obj.location().x);
        xw.setAttr(loc, "y", obj.location().y);

        const auto container = obj.container();
        for (size_t i = 0; i != container.size(); ++i) {
            if (container[i].second == 0)
                continue;
            auto invSlotE = xw.addChild("inventory", e);
            xw.setAttr(invSlotE, "slot", i);
            xw.setAttr(invSlotE, "item", container[i].first->id());
            xw.setAttr(invSlotE, "qty", container[i].second);
        }

        const auto mSlots = obj.merchantSlots();
        for (size_t i = 0; i != mSlots.size(); ++i){
            if (!mSlots[i])
                continue;
            auto mSlotE = xw.addChild("merchant", e);
            xw.setAttr(mSlotE, "slot", i);
            xw.setAttr(mSlotE, "wareItem", mSlots[i].wareItem->id());
            xw.setAttr(mSlotE, "wareQty", mSlots[i].wareQty);
            xw.setAttr(mSlotE, "priceItem", mSlots[i].priceItem->id());
            xw.setAttr(mSlotE, "priceQty", mSlots[i].priceQty);
        }
    }
    xw.publish();
#ifndef SINGLE_THREAD
    objectsFileMutex.unlock();
#endif
}

