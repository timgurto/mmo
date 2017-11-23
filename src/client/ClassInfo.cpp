#include "ClassInfo.h"
#include "Client.h"

ClassInfo::ClassInfo(const Name &name) : _name(name), _trees{} {
    _image = { "Images/Humans/" + name + ".png", Color::MAGENTA };
}

void ClassInfo::addSpell(const ClientTalent::Name & talentName, const Tree::Name & treeName,
        unsigned tier, const ClientSpell * spell) {
    auto &tree = findTree(treeName);

    auto t = ClientTalent{ talentName, ClientTalent::SPELL };
    t.spell = spell;
    t.icon = &spell->icon();

    tree.talents[tier].push_back(t);
}

void ClassInfo::addStats(const ClientTalent::Name & talentName, const Tree::Name & treeName,
        unsigned tier, const StatsMod & stats) {
    auto &tree = findTree(treeName);

    auto t = ClientTalent{ talentName, ClientTalent::STATS };
    t.stats = stats;

    tree.talents[tier].push_back(t);
}

void ClassInfo::ensureTreeExists(const Name & name) {
    for (const auto &tree : _trees) {
        if (tree.name == name)
            return;
    }
    auto tree = Tree{};
    tree.name = name;
    _trees.push_back(tree);
}

Tree & ClassInfo::findTree(const Tree::Name & name) {
    for (auto &tree : _trees) {
        if (tree.name == name)
            return tree;
    }
    assert(false);
}

ClientTalent::ClientTalent(const Name & talentName, Type type) :
name(talentName),
type(type){
    auto &client = Client::instance();
    learnMessage = Client::compileMessage(CL_TAKE_TALENT, talentName);
}

size_t Tree::numTiers() const {
    auto highestTier = size_t{ 0 };
    for (auto pair : talents)
        if (pair.first > highestTier)
            highestTier = pair.first;
    return highestTier + 1; // Assuming lowest is 0
}
