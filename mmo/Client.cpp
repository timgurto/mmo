// (C) 2015 Tim Gurto

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <SDL.h>
#include <SDL_mixer.h>
#include <string>
#include <set>
#include <sstream>
#include <vector>

#include "Client.h"
#include "EntityType.h"
#include "Log.h"
#include "Renderer.h"
#include "Server.h"
#include "TooltipBuilder.h"
#include "User.h"
#include "XmlReader.h"
#include "messageCodes.h"
#include "util.h"

extern Args cmdLineArgs;
extern Renderer renderer;

// TODO: Move all client functionality to a namespace, rather than a class.
Client *Client::_instance = 0;

const int Client::SCREEN_X = 640;
const int Client::SCREEN_Y = 360;

const size_t Client::BUFFER_SIZE = 1023;

const Uint32 Client::MAX_TICK_LENGTH = 100;
const Uint32 Client::SERVER_TIMEOUT = 10000;
const Uint32 Client::CONNECT_RETRY_DELAY = 3000;
const Uint32 Client::PING_FREQUENCY = 5000;

const Uint32 Client::TIME_BETWEEN_LOCATION_UPDATES = 50;

const int Client::ICON_SIZE = 16;
const size_t Client::ICONS_X = 8;
SDL_Rect Client::INVENTORY_RECT;
const int Client::ITEM_HEIGHT = ICON_SIZE;
const int Client::TEXT_HEIGHT = 11;
const int Client::HEADING_HEIGHT = 14;
const int Client::LINE_GAP = 6;

const size_t Client::MAX_TEXT_ENTERED = 100;

const int Client::PLAYER_ACTION_CHANNEL = 0;

bool Client::isClient = false;

Client::Client():
_activeRecipe(0),
_recipeList(0),
_detailsPane(0),
_craftingWindow(0),	
_actionTimer(0),
_actionLength(0),
_loop(true),
_socket(),
_defaultFont(0),
_mouse(0,0),
_mouseMoved(false),
_leftMouseDown(false),
_time(SDL_GetTicks()),
_timeElapsed(0),
_lastPingReply(_time),
_lastPingSent(_time),
_latency(0),
_timeSinceConnectAttempt(CONNECT_RETRY_DELAY),
_invalidUsername(false),
_loggedIn(false),
_loaded(false),
_timeSinceLocUpdate(0),
_tooltipNeedsRefresh(false),
_mapX(0), _mapY(0),
_currentMouseOverEntity(0),
_debug(360/13, "client.log", "trebuc.ttf", 10){
    isClient = true;
    _instance = this;

    _debug << cmdLineArgs << Log::endl;
    Socket::debug = &_debug;

    int ret = (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 512) < 0);
    if (ret < 0){
        _debug("SDL_mixer failed to initialize.", Color::RED);
    } else {
        _debug("SDL_mixer initialized.");
    }

    renderer.setDrawColor();

    _entities.insert(&_character);

    _defaultFont = TTF_OpenFont("trebuc.ttf", 10);

    Avatar::image("Images/man.png");
    _tile[0] = Texture(std::string("Images/Terrain/grass.png"));
    _tile[1] = Texture(std::string("Images/Terrain/stone.png"));
    _tile[2] = Texture(std::string("Images/Terrain/road.png"));
    _tile[3] = Texture(std::string("Images/Terrain/deepWater.png"));
    _tile[4] = Texture(std::string("Images/Terrain/water.png"));
    for (size_t i = 0; i != 5; ++i) {
        _tile[i].setBlend(SDL_BLENDMODE_ADD);
        _tile[i].setAlpha(0x3f);
    }

    // Player's inventory
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i)
        _inventory.push_back(std::make_pair<const Item *, size_t>(0, 0));

    _invLabel = Texture(_defaultFont, "Inventory");
    int invW =  max(min(ICONS_X, _inventory.size()) * (ICON_SIZE + 1) + 1,
                    static_cast<unsigned>(_invLabel.width()));
    int invH = ICON_SIZE + _invLabel.height() + 1;
    INVENTORY_RECT = makeRect(SCREEN_X - invW, SCREEN_Y - invH, invW, invH);

    // Randomize player name if not supplied
    if (cmdLineArgs.contains("username"))
        _username = cmdLineArgs.getString("username");
    else
        for (int i = 0; i != 3; ++i)
            _username.push_back('a' + rand() % 26);
    _debug << "Player name: " << _username << Log::endl;
    _character.name(_username);

    SDL_StopTextInput();


    // First pass: empty items, to facilitate links between items in second pass
    Item &wood = const_cast<Item &>(*_items.insert(Item("wood", "wood", 5)).first);
    Item &axe = const_cast<Item &>(*_items.insert(Item("axe", "wooden axe")).first);
    Item &chest = const_cast<Item &>(*_items.insert(Item("chest", "wooden chest", 10)).first);

    // Load data
    axe.addClass("axe");
    axe.addMaterial(&wood, 3);
    axe.craftTime(10);

    chest.addClass("structure");
    chest.addClass("container");
    chest.addMaterial(&wood, 5);
    chest.craftTime(10);


    // Object types
    XmlReader doc("Data/objectTypesClient.xml", &_debug);
    for (auto elem : doc.getChildren("objectType")) {
        std::string s; int n;
        if (!doc.findAttr(elem, "id", s))
            continue;
        ClientObjectType cot(s);
        cot.image(std::string("Images/Objects/") + s + ".png");
        if (doc.findAttr(elem, "name", s)) cot.name(s);
        SDL_Rect drawRect = {0, 0, cot.width(), cot.height()};
        if (doc.findAttr(elem, "xDrawOffset", drawRect.x) ||
            doc.findAttr(elem, "yDrawOffset", drawRect.y))
            cot.drawRect(drawRect);
        if (doc.findAttr(elem, "canGather", n) && n != 0) cot.canGather(true);
        if (doc.findAttr(elem, "gatherSound", s))
            cot.gatherSound(std::string("Sounds/") + s + ".wav");
        _objectTypes.insert(cot);
    }


    Element::absMouse = &_mouse;
    Element::font(TTF_OpenFont("trebuc.ttf", 10));

    initializeCraftingWindow();
}

Client::~Client(){
    Element::cleanup();
    if (_defaultFont)
        TTF_CloseFont(_defaultFont);
    if (Window::font())
        TTF_CloseFont(Window::font());
    delete _craftingWindow;
    Avatar::image("");
    for (const Entity *entityConst : _entities) {
        Entity *entity = const_cast<Entity *>(entityConst);
        if (entity != &_character)
            delete entity;
    }
    Mix_Quit();
}

void Client::checkSocket(){
    if (_invalidUsername)
        return;

    // Ensure connected to server
    if (!_loggedIn && _timeSinceConnectAttempt >= CONNECT_RETRY_DELAY) {
        _timeSinceConnectAttempt = 0;
        // Server details
        sockaddr_in serverAddr;
        serverAddr.sin_addr.s_addr = cmdLineArgs.contains("server-ip") ?
                                     inet_addr(cmdLineArgs.getString("server-ip").c_str()) :
                                     inet_addr("127.0.0.1");
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = cmdLineArgs.contains("server-port") ?
                              cmdLineArgs.getInt("server-port") :
                              htons(8888);
        if (connect(_socket.getRaw(), (sockaddr*)&serverAddr, Socket::sockAddrSize) < 0) {
            _debug << Color::RED << "Connection error: " << WSAGetLastError() << Log::endl;
        } else {
            _debug("Connected to server", Color::GREEN);
            // Announce player name
            sendMessage(CL_I_AM, _username);
            sendMessage(CL_PING, makeArgs(SDL_GetTicks()));
        }
    }

    static fd_set readFDs;
    FD_ZERO(&readFDs);
    FD_SET(_socket.getRaw(), &readFDs);
    static timeval selectTimeout = {0, 10000};
    int activity = select(0, &readFDs, 0, 0, &selectTimeout);
    if (activity == SOCKET_ERROR) {
        _debug << Color::RED << "Error polling sockets: " << WSAGetLastError() << Log::endl;
        return;
    }
    if (FD_ISSET(_socket.getRaw(), &readFDs)) {
        static char buffer[BUFFER_SIZE+1];
        int charsRead = recv(_socket.getRaw(), buffer, BUFFER_SIZE, 0);
        if (charsRead != SOCKET_ERROR && charsRead != 0){
            buffer[charsRead] = '\0';
            _messages.push(std::string(buffer));
        }
    }
}

void Client::run(){

    Uint32 timeAtLastTick = SDL_GetTicks();
    while (_loop) {
        _time = SDL_GetTicks();

        // Send ping
        if (_loggedIn && _time - _lastPingSent > PING_FREQUENCY) {
            sendMessage(CL_PING, makeArgs(_time));
            _lastPingSent = _time;
        }

        _timeElapsed = _time - timeAtLastTick;
        if (_timeElapsed > MAX_TICK_LENGTH)
            _timeElapsed = MAX_TICK_LENGTH;
        const double delta = _timeElapsed / 1000.0;
        timeAtLastTick = _time;

        // Ensure server connectivity
        if (_loggedIn && _time - _lastPingReply > SERVER_TIMEOUT) {
            _debug("Disconnected from server", Color::RED);
            _socket = Socket();
            _loggedIn = false;
        }

        if (!_loggedIn) {
            _timeSinceConnectAttempt += _timeElapsed;

        } else { // Update server with current location
            const bool atTarget = _pendingCharLoc == _character.location();
            if (atTarget)
                _timeSinceLocUpdate = 0;
            else {
                _timeSinceLocUpdate += _timeElapsed;
                if (_timeSinceLocUpdate > TIME_BETWEEN_LOCATION_UPDATES){
                    sendMessage(CL_LOCATION, makeArgs(_pendingCharLoc.x, _pendingCharLoc.y));
                    _tooltipNeedsRefresh = true;
                    _timeSinceLocUpdate = 0;
                }
            }
        }

        // Deal with any messages from the server
        if (!_messages.empty()){
            handleMessage(_messages.front());
            _messages.pop();
        }

        // Handle user events
        static SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            std::ostringstream oss;
            switch(e.type) {
            case SDL_QUIT:
                _loop = false;
                break;

            case SDL_TEXTINPUT:
                if (_enteredText.size() < MAX_TEXT_ENTERED)
                    _enteredText.append(e.text.text);
                break;

            case SDL_KEYDOWN:
                if (SDL_IsTextInputActive()) {
                    // Text input

                    switch(e.key.keysym.sym) {

                    case SDLK_ESCAPE:
                        SDL_StopTextInput();
                        _enteredText = "";
                        break;

                    case SDLK_BACKSPACE:
                        if (_enteredText.size() > 0) {
                            _enteredText.erase(_enteredText.size() - 1);
                        }
                        break;

                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        SDL_StopTextInput();
                        if (_enteredText != "") {
                            if (_enteredText.at(0) == '[') {
                                // Send message to server
                                sendRawMessage(_enteredText);
                                _debug(_enteredText, Color::YELLOW);
                            } else {
                                _debug(_enteredText, Color::WHITE);
                            }
                            _enteredText = "";
                        }
                        break;
                    }

                } else {
                    // Regular key input

                    switch(e.key.keysym.sym) {

                    case SDLK_ESCAPE:
                        if (_actionLength != 0)
                            sendMessage(CL_CANCEL_ACTION);
                        else if (_craftingWindow->visible())
                            _craftingWindow->hide();
                        else
                            _loop = false;
                        break;

                    case SDLK_LEFTBRACKET:
                        SDL_StartTextInput();
                        _enteredText = "[";
                        break;

                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        SDL_StartTextInput();
                        break;

                    case SDLK_c:
                        _craftingWindow->toggleVisibility();
                        break;
                    }
                }
                break;

            case SDL_MOUSEMOTION: {
                _mouse.x = e.motion.x * SCREEN_X / static_cast<double>(renderer.width());
                _mouse.y = e.motion.y * SCREEN_Y / static_cast<double>(renderer.height());
                _mouseMoved = true;
                
                if (_craftingWindow->visible())
                    _craftingWindow->onMouseMove(_mouse);

                if (!_loaded)
                    break;

                break;
            }

            case SDL_MOUSEBUTTONDOWN:
                _leftMouseDown = true;

                _craftingWindow->onMouseDown(_mouse);
                break;

            case SDL_MOUSEBUTTONUP:
                if (!_loaded)
                    break;

                _leftMouseDown = false;

                _craftingWindow->onMouseUp(_mouse);
                if (_craftingWindow->visible() && collision(_mouse, _craftingWindow->rect()))
                    break;

                if (_currentMouseOverEntity)
                    _currentMouseOverEntity->onLeftClick(*this);

                break;

            case SDL_MOUSEWHEEL:
                if (e.wheel.y < 0)
                    _craftingWindow->onScrollDown(_mouse);
                else if (e.wheel.y > 0)
                    _craftingWindow->onScrollUp(_mouse);
                break;

            case SDL_WINDOWEVENT:
                switch(e.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED:
                    renderer.updateSize();
                    renderer.setScale(static_cast<float>(renderer.width()) / SCREEN_X,
                                      static_cast<float>(renderer.height()) / SCREEN_Y);
                    _craftingWindow->forceRefresh();
                    break;
                }

            default:
                // Unhandled event
                ;
            }
        }
        // Poll keys (whether they are currently pressed; not key events)
        static const Uint8 *keyboardState = SDL_GetKeyboardState(0);
        if (_loggedIn && !SDL_IsTextInputActive()) {
            bool
                up = keyboardState[SDL_SCANCODE_UP] == SDL_PRESSED ||
                     keyboardState[SDL_SCANCODE_W] == SDL_PRESSED,
                down = keyboardState[SDL_SCANCODE_DOWN] == SDL_PRESSED ||
                       keyboardState[SDL_SCANCODE_S] == SDL_PRESSED,
                left = keyboardState[SDL_SCANCODE_LEFT] == SDL_PRESSED ||
                       keyboardState[SDL_SCANCODE_A] == SDL_PRESSED,
                right = keyboardState[SDL_SCANCODE_RIGHT] == SDL_PRESSED ||
                        keyboardState[SDL_SCANCODE_D] == SDL_PRESSED;
            if (up != down || left != right) {
                static const double DIAG_SPEED = Server::MOVEMENT_SPEED / SQRT_2;
                const double
                    dist = delta * Server::MOVEMENT_SPEED,
                    diagDist = delta * DIAG_SPEED;
                Point newLoc = _pendingCharLoc;
                if (up != down) {
                    if (up && !down)
                        newLoc.y -= (left != right) ? diagDist : dist;
                    else if (down && !up)
                        newLoc.y += (left != right) ? diagDist : dist;
                }
                if (left && !right)
                    newLoc.x -= (up != down) ? diagDist : dist;
                else if (right && !left)
                    newLoc.x += (up != down) ? diagDist : dist;

                const int
                    xLimit = _mapX * Server::TILE_W - Server::TILE_W/2,
                    yLimit = _mapY * Server::TILE_H;
                if (newLoc.x < 0)
                    newLoc.x = 0;
                else if (newLoc.x > xLimit)
                    newLoc.x = xLimit;
                if (newLoc.y < 0)
                    newLoc.y = 0;
                else if (newLoc.y > yLimit)
                    newLoc.y = yLimit;

                _pendingCharLoc = newLoc;
                _mouseMoved = true;
            }
        }

        // Update entities
        std::vector<Entity *> entitiesToReorder;
        for (Entity::set_t::iterator it = _entities.begin(); it != _entities.end(); ) {
            Entity::set_t::iterator next = it;
            ++next;
            Entity *const toUpdate = *it;
            toUpdate->update(delta);
            if (toUpdate->yChanged()) {
                // Entity has moved up or down, and must be re-ordered in set.
                entitiesToReorder.push_back(toUpdate);
                _entities.erase(it);
                toUpdate->yChanged(false);
            }
            it = next;
        }
        for (Entity *entity : entitiesToReorder)
            _entities.insert(entity);
        entitiesToReorder.clear();

        updateOffset();

        // Update cast bar
        if (_actionLength > 0)
            _actionTimer = min(_actionTimer + _timeElapsed, _actionLength);

        if (_mouseMoved)
            checkMouseOver();

        checkSocket();
        // Draw
        draw();
        SDL_Delay(5);
    }
}

void Client::checkMouseOver(){
    // Check if mouse is over a UI element
    _uiTooltip = Texture();
    if (collision(_mouse, INVENTORY_RECT))
        _uiTooltip = getInventoryTooltip();
    /*else if (collision(_mouse, _craftingRect))
        _uiTooltip = getCraftingTooltip();*/

    // Check if mouse is over an entity
    const Point mouseOffset = _mouse - _offset;
    const Entity *const oldMouseOverEntity = _currentMouseOverEntity;
    Entity::set_t::iterator mouseOverIt = _entities.end();
    Entity lookupEntity(0, mouseOffset);
    for (auto it = _entities.lower_bound(&lookupEntity); it != _entities.end(); ++it) {
        if (*it != &_character &&(*it)->collision(mouseOffset))
            mouseOverIt = it;
    }
    if (mouseOverIt != _entities.end()) {
        _currentMouseOverEntity = *mouseOverIt;
        if (_currentMouseOverEntity != oldMouseOverEntity ||
            _currentMouseOverEntity->needsTooltipRefresh() ||
            _tooltipNeedsRefresh) {
            _currentMouseOverEntity->refreshTooltip(*this);
            _tooltipNeedsRefresh = false;
        }
            
    } else
        _currentMouseOverEntity = 0;
}

void Client::draw() const{
    if (!_loggedIn || !_loaded){
        renderer.setDrawColor(Color::BLACK);
        renderer.clear();
        _debug.draw();
        renderer.present();
        return;
    }

    // Background
    renderer.setDrawColor(Color::BLUE_HELL);
    renderer.clear();

    // Map
    for (size_t y = 0; y != _mapY; ++y) {
        const int yLoc = y * Server::TILE_H + toInt(offset().y);
        for (size_t x = 0; x != _mapX; ++x){
            int xLoc = x * Server::TILE_W + toInt(offset().x);
            if (y % 2 == 1)
                xLoc -= Server::TILE_W/2;
            drawTile(x, y, xLoc, yLoc);
        }
    }

    // Character's target and actual location
    if (isDebug()) {
        renderer.setDrawColor(Color::CYAN);
        const Point &actualLoc = _character.destination() + offset();
        renderer.drawRect(makeRect(actualLoc.x - 1, actualLoc.y - 1, 3, 3));

        renderer.setDrawColor(Color::WHITE);
        Point pendingLoc(_pendingCharLoc.x + offset().x, _pendingCharLoc.y + offset().y);
        renderer.drawRect(makeRect(pendingLoc.x, pendingLoc.y, 1, 1));
        renderer.drawRect(makeRect(pendingLoc.x - 2, pendingLoc.y - 2, 5, 5));
    }

    // Entities, sorted from back to front
    for (const Entity *entity : _entities)
        entity->draw(*this);

    // Rectangle around user
    //renderer.setDrawColor(Color::WHITE);
    //SDL_Rect drawLoc = _character.drawRect() + offset();
    //renderer.drawRect(drawLoc);

    // Inventory
    renderer.setDrawColor(Color::GREY_4);
    renderer.fillRect(INVENTORY_RECT);
    _invLabel.draw(INVENTORY_RECT.x, INVENTORY_RECT.y);
    renderer.setDrawColor(Color::BLACK);
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i){
        const SDL_Rect iconRect = makeRect(SCREEN_X - INVENTORY_RECT.w + 1 + i*(ICON_SIZE+1),
                                           SCREEN_Y - ICON_SIZE,
                                           ICON_SIZE, ICON_SIZE);
        renderer.fillRect(iconRect);
        const Item *item = _inventory[i].first;
        if (item) {
            const size_t qty = _inventory[i].second;
            item->icon().draw(iconRect);
            if (item->stackSize() > 1) {
                // Display stack size
                const Texture qtyLabel(_defaultFont, makeArgs(makeArgs(qty)));
                qtyLabel.draw(iconRect.x + ICON_SIZE - qtyLabel.width() + 1,
                              iconRect.y + ICON_SIZE - qtyLabel.height() + 3);
            }
        }
    }

    // Cast bar
    if (_actionTimer > 0) {
        static const int
            CAST_BAR_Y = 300,
            CAST_BAR_HEIGHT = 9,
            CAST_BAR_WIDTH = 150,
            CAST_BAR_PADDING = 1;
        static const Color
            CAST_BAR_BACKGROUND = Color::BLUE / 2 + Color::GREY_2,
            CAST_BAR_COLOR = Color::RED * 0.75;
        const SDL_Rect
            castBarBackgroundRect = makeRect(toInt((SCREEN_X - CAST_BAR_WIDTH) / 2.0 -
                                                              CAST_BAR_PADDING),
                                             CAST_BAR_Y - CAST_BAR_PADDING,
                                             CAST_BAR_WIDTH + 2 * CAST_BAR_PADDING,
                                             CAST_BAR_HEIGHT + 2 * CAST_BAR_PADDING),
            castBarRect = makeRect(toInt((SCREEN_X - CAST_BAR_WIDTH) / 2.0),
                                   CAST_BAR_Y,
                                   toInt(CAST_BAR_WIDTH * 1.0 * _actionTimer / _actionLength),
                                   CAST_BAR_HEIGHT);
        renderer.setDrawColor(CAST_BAR_BACKGROUND);
        renderer.fillRect(castBarBackgroundRect);
        renderer.setDrawColor(CAST_BAR_COLOR);
        renderer.fillRect(castBarRect);

        renderer.setDrawColor(Color::WHITE);
        Texture castBarLabel(_defaultFont, _actionMessage, Color::WHITE);
        castBarLabel.draw(Point((SCREEN_X - castBarLabel.width()) / 2.0,
                                CAST_BAR_Y + (CAST_BAR_HEIGHT - castBarLabel.height()) / 2.0));
    }

    // Tooltip
    drawTooltip();

    // FPS/latency
    std::ostringstream oss;
    if (_timeElapsed > 0)
        oss << toInt(1000.0/_timeElapsed);
    else
        oss << "infinite ";
    oss << "fps " << _latency << "ms";
    const Texture statsDisplay(_defaultFont, oss.str(), Color::YELLOW);
    statsDisplay.draw(SCREEN_X - statsDisplay.width(), 0);

    // Text box
    if (SDL_IsTextInputActive()) {
        static const int
            TEXT_BOX_HEIGHT = 13,
            TEXT_BOX_WIDTH = 300;
        static const SDL_Rect TEXT_BOX_RECT = makeRect((SCREEN_X - TEXT_BOX_WIDTH) / 2,
                                                       (SCREEN_Y - TEXT_BOX_HEIGHT) / 2,
                                                       TEXT_BOX_WIDTH, TEXT_BOX_HEIGHT);
        static const Color TEXT_BOX_BORDER = Color::GREY_4;
        renderer.setDrawColor(TEXT_BOX_BORDER);
        renderer.drawRect(TEXT_BOX_RECT + makeRect(-1, -1, 2, 2));
        renderer.setDrawColor(Color::BLACK);
        renderer.fillRect(TEXT_BOX_RECT);
        const Texture text(_defaultFont, _enteredText);
        static const int MAX_TEXT_WIDTH = TEXT_BOX_WIDTH - 2;
        int cursorX;
        if (text.width() < MAX_TEXT_WIDTH) {
            text.draw(TEXT_BOX_RECT.x + 1, TEXT_BOX_RECT.y);
            cursorX = TEXT_BOX_RECT.x + text.width() + 1;
        } else {
            const SDL_Rect
                dstRect = makeRect(TEXT_BOX_RECT.x + 1, TEXT_BOX_RECT.y,
                                   MAX_TEXT_WIDTH, text.height()),
                srcRect = makeRect(text.width() - MAX_TEXT_WIDTH, 0,
                                   MAX_TEXT_WIDTH, text.height());
            text.draw(dstRect, srcRect);
            cursorX = TEXT_BOX_RECT.x + TEXT_BOX_WIDTH;
        }
        renderer.setDrawColor(Color::WHITE);
        renderer.fillRect(makeRect(cursorX, TEXT_BOX_RECT.y + 1, 1, TEXT_BOX_HEIGHT - 2));
    }

    _craftingWindow->draw();

    _debug.draw();
    renderer.present();
}

Texture Client::getInventoryTooltip() const{
    if (_mouse.y < INVENTORY_RECT.w + _invLabel.height())
        return Texture();
    const int slot = static_cast<size_t>((_mouse.x - INVENTORY_RECT.x) / (ICON_SIZE + 1));
    if (slot < 0 || static_cast<size_t>(slot) >= _inventory.size())
        return Texture();

    const Item *const item = _inventory[static_cast<size_t>(slot)].first;
    if (!item)
        return Texture();
    TooltipBuilder tb;
    tb.setColor(Color::WHITE);
    tb.addLine(item->name());
    bool isStructure = false;
    if (item->hasClasses()) {
        tb.setColor();
        tb.addGap();
        for (const std::string &className : item->classes()) {
            tb.addLine(className);
            if (!isStructure && className == "structure")
                isStructure = true;
        }
    }
    if (isStructure) {
        tb.addGap();
        tb.setColor(Color::YELLOW);
        tb.addLine("Right-click to construct");
    }
    return tb.publish();
}

void Client::drawTooltip() const{
    Texture tooltip;
    if (_uiTooltip)
        tooltip = _uiTooltip;
    else if (_currentMouseOverEntity)
        tooltip = _currentMouseOverEntity->tooltip();
    else
        return;

    if (tooltip) {
        static const int EDGE_GAP = 2; // Gap from screen edges
        static const int CURSOR_GAP = 10; // Horizontal gap from cursor
        int x, y;
        const int mouseX = toInt(_mouse.x);
        const int mouseY = toInt(_mouse.y);

        // y: below cursor, unless too close to the bottom of the screen
        if (SCREEN_Y > mouseY + tooltip.height() + EDGE_GAP)
            y = mouseY;
        else
            y = SCREEN_Y - tooltip.height() - EDGE_GAP;

        // x: to the right of the cursor, unless too close to the right of the screen
        if (SCREEN_X > mouseX + tooltip.width() + EDGE_GAP + CURSOR_GAP)
            x = mouseX + CURSOR_GAP;
        else
            x = mouseX - tooltip.width() - CURSOR_GAP;
        tooltip.draw(x, y);
    }
}

void Client::drawTile(size_t x, size_t y, int xLoc, int yLoc) const{
    /*
          H | E
      L | tileID| R
          G | F
    */
    const SDL_Rect drawLoc = {xLoc, yLoc, 0, 0};
    const bool yOdd = (y % 2 == 1);
    size_t tileID, L, R, E, F, G, H;
    tileID = _map[x][y];
    R = x == _mapX-1 ? tileID : _map[x+1][y];
    L = x == 0 ? tileID : _map[x-1][y];
    if (y == 0) {
        H = E = tileID;
    } else {
        if (yOdd) {
            E = _map[x][y-1];
            H = x == 0 ? tileID : _map[x-1][y-1];
        } else {
            E = x == _mapX-1 ? tileID : _map[x+1][y-1];
            H = _map[x][y-1];
        }
    }
    if (y == _mapY-1) {
        G = F = tileID;
    } else {
        if (!yOdd) {
            F = x == _mapX-1 ? tileID : _map[x+1][y+1];
            G = _map[x][y+1];
        } else {
            F = _map[x][y+1];
            G = x == 0 ? tileID : _map[x-1][y+1];
        }
    }

    static const SDL_Rect
        TOP_LEFT     = {0,                0,                Server::TILE_W/2, Server::TILE_H/2},
        TOP_RIGHT    = {Server::TILE_W/2, 0,                Server::TILE_W/2, Server::TILE_H/2},
        BOTTOM_LEFT  = {0,                Server::TILE_H/2, Server::TILE_W/2, Server::TILE_H/2},
        BOTTOM_RIGHT = {Server::TILE_W/2, Server::TILE_H/2, Server::TILE_W/2, Server::TILE_H/2},
        LEFT_HALF    = {0,                0,                Server::TILE_W/2, Server::TILE_H},
        RIGHT_HALF   = {Server::TILE_W/2, 0,                Server::TILE_W/2, Server::TILE_H},
        FULL         = {0,                0,                Server::TILE_W,   Server::TILE_H};

    // Black background
    // Assuming all tile images are set to SDL_BLENDMODE_ADD and 0x3f alpha
    renderer.setDrawColor(Color::BLACK);
    if (yOdd && x == 0) {
        renderer.fillRect(drawLoc + RIGHT_HALF);
    }
    else if (!yOdd && x == _mapX-1) {
        renderer.fillRect(drawLoc + LEFT_HALF);
    }
    else {
        renderer.fillRect(drawLoc + FULL);
    }

    // Half-alpha base tile
    _tile[tileID].setAlpha(0x7f);
    if (yOdd && x == 0) {
        _tile[tileID].draw(drawLoc + TOP_RIGHT, TOP_RIGHT);
        _tile[tileID].draw(drawLoc + BOTTOM_RIGHT, BOTTOM_RIGHT);
    } else if (!yOdd && x == _mapX-1) {
        _tile[tileID].draw(drawLoc + BOTTOM_LEFT, BOTTOM_LEFT);
        _tile[tileID].draw(drawLoc + TOP_LEFT, TOP_LEFT);
    } else {
        _tile[tileID].draw(drawLoc + TOP_RIGHT, TOP_RIGHT);
        _tile[tileID].draw(drawLoc + BOTTOM_RIGHT, BOTTOM_RIGHT);
        _tile[tileID].draw(drawLoc + BOTTOM_LEFT, BOTTOM_LEFT);
        _tile[tileID].draw(drawLoc + TOP_LEFT, TOP_LEFT);
    }
    _tile[tileID].setAlpha(0x3f);

    // Quarter-alpha L, R, E, F, G, H tiles
    if (!yOdd || x != 0) {
        _tile[L].draw(drawLoc + BOTTOM_LEFT, BOTTOM_LEFT);
        _tile[L].draw(drawLoc + TOP_LEFT, TOP_LEFT);
        _tile[G].draw(drawLoc + BOTTOM_LEFT, BOTTOM_LEFT);
        _tile[H].draw(drawLoc + TOP_LEFT, TOP_LEFT);
    }
    if (yOdd || x != _mapX-1) {
        _tile[R].draw(drawLoc + TOP_RIGHT, TOP_RIGHT);
        _tile[R].draw(drawLoc + BOTTOM_RIGHT, BOTTOM_RIGHT);
        _tile[E].draw(drawLoc + TOP_RIGHT, TOP_RIGHT);
        _tile[F].draw(drawLoc + BOTTOM_RIGHT, BOTTOM_RIGHT);
    }

    /*if (tileID == 3 || tileID == 4) {
        renderer.setDrawColor(Color::RED);
        renderer.drawRect(drawLoc + FULL);
    }*/
}

void Client::startCrafting(void *data){
    if (_instance->_activeRecipe) {
        _instance->sendMessage(CL_CRAFT, _instance->_activeRecipe->id());
        _instance->prepareAction("Crafting " + _instance->_activeRecipe->name());
    }
}

bool Client::playerHasItem(const Item *item, size_t quantity) const{
    for (size_t i = 0; i != User::INVENTORY_SIZE; ++i) {
        const std::pair<const Item *, size_t> slot = _inventory[i];
        if (slot.first == item) {
            if (slot.second >= quantity)
                return true;
            else
                quantity -= slot.second;
        }
    }
    return false;
}

void Client::handleMessage(const std::string &msg){
    _partialMessage.append(msg);
    std::istringstream iss(_partialMessage);
    _partialMessage = "";
    int msgCode;
    char del;
    static char buffer[BUFFER_SIZE+1];

    // Read while there are new messages
    while (!iss.eof()) {
        // Discard malformed data
        if (iss.peek() != '[') {
            iss.get(buffer, BUFFER_SIZE, '[');
            _debug << "Read " << iss.gcount() << " characters." << Log::endl;
            _debug << Color::RED << "Malformed message; discarded \""
                   << buffer << "\"" << Log::endl;
            if (iss.eof()) {
                break;
            }
        }

        // Get next message
        iss.get(buffer, BUFFER_SIZE, ']');
        if (iss.eof()){
            _partialMessage = buffer;
            break;
        } else {
            std::streamsize charsRead = iss.gcount();
            buffer[charsRead] = ']';
            buffer[charsRead+1] = '\0';
            iss.ignore(); // Throw away ']'
        }
        std::istringstream singleMsg(buffer);
        singleMsg >> del >> msgCode >> del;
        switch(msgCode) {

        case SV_WELCOME:
        {
            if (del != ']')
                break;
            _loggedIn = true;
            _timeSinceConnectAttempt = 0;
            _lastPingSent = _lastPingReply = _time;
            _debug("Successfully logged in to server", Color::GREEN);
            break;
        }

        case SV_PING_REPLY:
        {
            Uint32 timeSent;
            singleMsg >> timeSent >> del;
            if (del != ']')
                break;
            _lastPingReply = _time;
            _latency = (_time - timeSent) / 2;
            break;
        }

        case SV_USER_DISCONNECTED:
        {
            std::string name;
            singleMsg.get(buffer, BUFFER_SIZE, ']');
            name = std::string(buffer);
            singleMsg >> del;
            if (del != ']')
                break;
            const std::map<std::string, Avatar*>::iterator it = _otherUsers.find(name);
            if (it != _otherUsers.end()) {
                removeEntity(it->second);
                _otherUsers.erase(it);
            }
            _debug << name << " disconnected." << Log::endl;
            break;
        }

        case SV_DUPLICATE_USERNAME:
            if (del != ']')
                break;
            _invalidUsername = true;
            _debug << Color::RED << "The user " << _username
                   << " is already connected to the server." << Log::endl;
            break;

        case SV_INVALID_USERNAME:
            if (del != ']')
                break;
            _invalidUsername = true;
            _debug << Color::RED << "The username " << _username << " is invalid." << Log::endl;
            break;

        case SV_SERVER_FULL:
            if (del != ']')
                break;
            _debug("The server is full.  Attempting reconnection...", Color::YELLOW);
            _socket = Socket();
            _loggedIn = false;
            break;

        case SV_TOO_FAR:
            if (del != ']')
                break;
            _debug("You are too far away to perform that action.", Color::YELLOW);
            startAction(0);
            break;

        case SV_DOESNT_EXIST:
            if (del != ']')
                break;
            _debug("That object doesn't exist.", Color::YELLOW);
            startAction(0);
            break;

        case SV_INVENTORY_FULL:
            if (del != ']')
                break;
            _debug("Your inventory is full.", Color::RED);
            startAction(0);
            break;

        case SV_NEED_MATERIALS:
            if (del != ']')
                break;
            _debug("You do not have the necessary materials to create that item.", Color::YELLOW);
            startAction(0);
            break;

        case SV_INVALID_ITEM:
            if (del != ']')
                break;
            _debug("That is not a real item.", Color::RED);
            startAction(0);
            break;

        case SV_CANNOT_CRAFT:
            if (del != ']')
                break;
            _debug("That item cannot be crafted.", Color::RED);
            startAction(0);
            break;

        case SV_ACTION_INTERRUPTED:
            if (del != ']')
                break;
            _debug("Action interrupted.", Color::YELLOW);
            startAction(0);
            break;

        case SV_INVALID_SLOT:
            if (del != ']')
                break;
            _debug("That is not a valid inventory slot.", Color::RED);
            startAction(0);
            break;

        case SV_EMPTY_SLOT:
            if (del != ']')
                break;
            _debug("That inventory slot is empty.", Color::RED);
            startAction(0);
            break;

        case SV_CANNOT_CONSTRUCT:
            if (del != ']')
                break;
            _debug("Only 'structure'-class items can be constructed.", Color::RED);
            startAction(0);
            break;

        case SV_ITEM_NEEDED:
        {
            std::string reqItemClass;
            singleMsg.get(buffer, BUFFER_SIZE, ']');
            reqItemClass = std::string(buffer);
            singleMsg >> del;
            if (del != ']')
                break;
            std::string msg = "You need a";
            const char first = reqItemClass.front();
            if (first == 'a' || first == 'e' || first == 'i' ||
                first == 'o' || first == 'u')
                msg += 'n';
            _debug(msg + ' ' + reqItemClass + " to do that.", Color::YELLOW);
            startAction(0);
            break;
        }

        case SV_ACTION_STARTED:
            Uint32 time;
            singleMsg >> time >> del;
            if (del != ']')
                break;
            startAction(time);
            break;

        case SV_ACTION_FINISHED:
            if (del != ']')
                break;
            startAction(0); // Effectively, hide the cast bar.
            break;

        case SV_MAP_SIZE:
        {
            size_t x, y;
            singleMsg >> x >> del >> y >> del;
            if (del != ']')
                break;
            _mapX = x;
            _mapY = y;
            _map = std::vector<std::vector<size_t> >(_mapX);
            for (size_t x = 0; x != _mapX; ++x)
                _map[x] = std::vector<size_t>(_mapY, 0);
            break;
        }

        case SV_TERRAIN:
        {
            size_t x, y, n;
            singleMsg >> x >> del >> y >> del >> n >> del;
            if (x + n > _mapX)
                break;
            if (y > _mapY)
                break;
            std::vector<size_t> terrain;
            for (size_t i = 0; i != n; ++i) {
                size_t value;
                singleMsg >> value >> del;
                terrain.push_back(value);
            }
            if (del != ']')
                break;
            if (terrain.size() != n)
                break;
            for (size_t i = 0; i != n; ++i)
                _map[x+i][y] = terrain[i];
            break;
        }

        case SV_LOCATION:
        {
            std::string name;
            double x, y;
            singleMsg.get(buffer, BUFFER_SIZE, ',');
            name = std::string(buffer);
            singleMsg >> del >> x >> del >> y >> del;
            if (del != ']')
                break;
            const Point p(x, y);
            if (name == _username) {
                _character.destination(p);
                if (!_loaded) {
                    setEntityLocation(&_character, p);
                    _pendingCharLoc = p;
                }
                updateOffset();
                _loaded = true;
                _tooltipNeedsRefresh = true;
                _mouseMoved = true;
            } else {
                if (_otherUsers.find(name) == _otherUsers.end()) {
                    // Create new Avatar
                    Avatar *newUser = new Avatar(name, p);
                    _otherUsers[name] = newUser;
                    _entities.insert(newUser);
                }
                _otherUsers[name]->destination(p);
            }
            break;
        }

        case SV_INVENTORY:
        {
            int slot, quantity;
            std::string itemID;
            singleMsg >> slot >> del;
            singleMsg.get(buffer, BUFFER_SIZE, ',');
            itemID = std::string(buffer);
            singleMsg >> del >> quantity >> del;
            if (del != ']')
                break;
            std::set<Item>::const_iterator it = _items.find(itemID);
            if (it == _items.end())
                _inventory[slot] = std::make_pair<const Item *, size_t>(0, 0);
            else
                _inventory[slot] = std::make_pair(&*it, quantity);
            _recipeList->markChanged();
            break;
        }

        case SV_OBJECT:
        {
            int serial;
            double x, y;
            std::string type;
            singleMsg >> serial >> del >> x >> del >> y >> del;
            singleMsg.get(buffer, BUFFER_SIZE, ']');
            type = std::string(buffer);
            singleMsg >> del;
            if (del != ']')
                break;
            std::map<size_t, ClientObject*>::iterator it = _objects.find(serial);
            if (it == _objects.end()) {
                // A new object was added; add entity to list
                const std::set<ClientObjectType>::const_iterator it = _objectTypes.find(type);
                if (it == _objectTypes.end())
                    break;
                ClientObject *const obj = new ClientObject(serial, &*it, Point(x, y));
                _entities.insert(obj);
                _objects[serial] = obj;
            }
            break;
        }

        case SV_REMOVE_OBJECT:
        {
            int serial;
            singleMsg >> serial >> del;
            if (del != ']')
                break;
            const std::map<size_t, ClientObject*>::const_iterator it = _objects.find(serial);
            if (it == _objects.end()){
                _debug("Server removed an object we didn't know about.", Color::YELLOW);
                assert(false);
                break; // We didn't know about this object
            }
            if (it->second == _currentMouseOverEntity)
                _currentMouseOverEntity = 0;
            removeEntity(it->second);
            _objects.erase(it);
            break;
        }

        default:
            _debug << Color::RED << "Unhandled message: " << msg << Log::endl;
        }

        if (del != ']' && !iss.eof()) {
            _debug("Bad message ending", Color::RED);
        }

        iss.peek();
    }
}

void Client::sendRawMessage(const std::string &msg) const{
    _socket.sendMessage(msg);
}

void Client::sendMessage(MessageCode msgCode, const std::string &args) const{
    // Compile message
    std::ostringstream oss;
    oss << '[' << msgCode;
    if (args != "")
        oss << ',' << args;
    oss << ']';

    // Send message
    sendRawMessage(oss.str());
}

const Socket &Client::socket() const{
    return _socket;
}

void Client::removeEntity(Entity *const toRemove){
    const Entity::set_t::iterator it = _entities.find(toRemove);
    if (it != _entities.end())
        _entities.erase(it);
    delete toRemove;
}

TTF_Font *Client::defaultFont() const{
    return _defaultFont;
}

void Client::setEntityLocation(Entity *entity, const Point &location){
    const Entity::set_t::iterator it = _entities.find(entity);
    if (it == _entities.end()){
        assert(false); // Entity is not in set.
        return;
    }
    entity->location(location);
    if (entity->yChanged()) {
        _entities.erase(it);
        _entities.insert(entity);
        entity->yChanged(false);
    }
}

void Client::updateOffset(){
    _offset = Point(SCREEN_X / 2 - _character.location().x,
                    SCREEN_Y / 2 - _character.location().y);
    _intOffset = Point(toInt(_offset.x),
                       toInt(_offset.y));
}

void Client::prepareAction(const std::string &msg){
    _actionMessage = msg;
}

void Client::startAction(Uint32 actionLength){
    _actionTimer = 0;
    _actionLength = actionLength;
    if (actionLength == 0)
        Mix_HaltChannel(PLAYER_ACTION_CHANNEL);
}
