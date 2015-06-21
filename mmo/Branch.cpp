#include "Branch.h"
#include "Client.h"
#include "Color.h"
#include "Renderer.h"
#include "Server.h"
#include "util.h"

extern Renderer renderer;

EntityType Branch::_entityType(makeRect(-10, -5));

Branch::Branch(const Branch &rhs):
Entity(rhs),
_serial(rhs._serial){}

Branch::Branch(size_t serialArg, const Point &loc):
Entity(_entityType, loc),
_serial(serialArg){}

void Branch::onLeftClick(const Client &client) const{
    std::ostringstream oss;
    oss << '[' << CL_COLLECT_BRANCH << ',' << _serial << ']';
    client.socket().sendMessage(oss.str());
}

Texture Branch::tooltip(const Client &client) const{
    static const int PADDING = 10;
    static const Color titleColor = Color::BLUE / 2 + Color::WHITE / 2;
    Texture title(client.defaultFont(), "Branch", titleColor);
    int totalHeight = title.height() + 2*PADDING;
    int totalWidth = title.width() + 2*PADDING;

    Texture extra;
    if (distance(_location, client.character().location()) > Server::ACTION_DISTANCE) {
        extra = Texture(client.defaultFont(), "Out of range", Color::WHITE);
        totalHeight += extra.height() + PADDING;
        int tempWidth = extra.width() + 2*PADDING;
        if (tempWidth > totalWidth)
            totalWidth = tempWidth;
    }

    Texture tooltipTexture(totalWidth, totalHeight);
    
    // Draw background
    Texture background(totalWidth, totalHeight);
    static const Color backgroundColor = Color::WHITE/8 + Color::BLUE/6;
    background.setRenderTarget();
    renderer.setDrawColor(backgroundColor);
    renderer.clear();
    background.setBlend(SDL_BLENDMODE_NONE, 0xbf);

    tooltipTexture.setRenderTarget();
    background.draw();
    renderer.setDrawColor(Color::WHITE);
    renderer.drawRect(makeRect(0, 0, totalWidth-1, totalHeight-1));

    // Draw text
    int y = PADDING;
    title.draw(PADDING, y);
    if (extra) {
        y += title.height() + PADDING;
        extra.draw(PADDING, y);
    }
    tooltipTexture.setBlend(SDL_BLENDMODE_BLEND);

    renderer.setRenderTarget();
    
    return tooltipTexture;
}
