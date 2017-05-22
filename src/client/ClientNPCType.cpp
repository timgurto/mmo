#include "Client.h"
#include "ClientNPC.h"
#include "ClientNPCType.h"
#include "Surface.h"

ClientNPCType::ClientNPCType(const std::string &id, health_t maxHealthArg):
ClientObjectType(id)
{
    containerSlots(ClientNPC::LOOT_CAPACITY);
    maxHealth(maxHealthArg);
}

void ClientNPCType::corpseImage(const std::string &filename){
    _corpseImage = Texture(filename, Color::MAGENTA);

    // Set corpse highlight image
    Surface corpseHighlightSurface(filename, Color::MAGENTA);
    if (!corpseHighlightSurface)
        return;
    corpseHighlightSurface.swapColors(Color::OUTLINE, Color::HIGHLIGHT_OUTLINE);
    _corpseHighlightImage = Texture(corpseHighlightSurface);
}
