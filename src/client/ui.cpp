#include <cassert>

#include "Client.h"
#include "ui/Indicator.h"
#include "ui/OutlinedLabel.h"
#include "ui/TextBox.h"

void Client::showErrorMessage(const std::string &message, Color color) const {
  if (!_lastErrorMessage) return;
  _lastErrorMessage->changeText(message);
  _lastErrorMessage->setColor(color);

  const auto TIME_TO_SHOW_ERROR_MESSAGE = 5000;
  _errorMessageTimer = TIME_TO_SHOW_ERROR_MESSAGE;

  _lastErrorMessage->show();
}

void Client::initUI() {
  if (_defaultFont != nullptr) TTF_CloseFont(_defaultFont);
  _defaultFont = TTF_OpenFont(_config.fontFile.c_str(), _config.fontSize);
  assert(_defaultFont != nullptr);
  Element::font(_defaultFont);
  Element::textOffset = _config.fontOffset;
  Element::TEXT_HEIGHT = _config.textHeight;
  Element::absMouse = &_mouse;

  Element::initialize();
  ClientItem::init();

  initPerformanceDisplay();
  initChatLog();
  initWindows();
  initializeGearSlotNames();
  initCastBar();
  initHotbar();
  initBuffsDisplay();
  initMenuBar();
  initPlayerPanels();
  initTargetBuffs();
  initToasts();
  initQuestProgress();

  const auto ERROR_FROM_BOTTOM = 50;
  _lastErrorMessage = new OutlinedLabel(
      {0, SCREEN_Y - ERROR_FROM_BOTTOM, SCREEN_X, Element::TEXT_HEIGHT + 4}, {},
      Element::CENTER_JUSTIFIED);
  addUI(_lastErrorMessage);
}

void Client::initChatLog() {
  drawLoadingScreen("Initializing chat log", 0.4);
  _chatContainer =
      new Element({0, SCREEN_Y - _config.chatH, _config.chatW, _config.chatH});
  _chatTextBox = new TextBox({0, _config.chatH, _config.chatW});
  _chatLog =
      new List({0, 0, _config.chatW, _config.chatH - _chatTextBox->height()});
  _chatTextBox->setPosition(0, _chatLog->height());
  _chatTextBox->hide();

  auto background =
      new ColorBlock(_chatLog->rect(), Color::CHAT_LOG_BACKGROUND);
  background->setAlpha(0x7f);
  _chatContainer->addChild(background);

  _chatContainer->addChild(_chatLog);
  _chatContainer->addChild(_chatTextBox);

  addUI(_chatContainer);
  SAY_COLOR = Color::SAY;
  WHISPER_COLOR = Color::WHISPER;
}

void Client::initWindows() {
  drawLoadingScreen("Initializing UI", 0.8);

  initializeBuildWindow();
  addWindow(_buildWindow);

  _craftingWindow = Window::InitializeLater(initializeCraftingWindow,
                                            "Crafting (uninitialised)");
  addWindow(_craftingWindow);

  initializeInventoryWindow();
  addWindow(_inventoryWindow);

  _gearWindow =
      Window::InitializeLater(initializeGearWindow, "Gear (uninitialised)");
  addWindow(_gearWindow);

  initializeSocialWindow();
  addWindow(_socialWindow);

  initializeMapWindow();
  addWindow(_mapWindow);

  initializeHelpWindow();
  addWindow(_helpWindow);

  initializeClassWindow();
  addWindow(_classWindow);

  initializeEscapeWindow();
  addWindow(_escapeWindow);
}

void Client::initCastBar() {
  const ScreenRect CAST_BAR_RECT(SCREEN_X / 2 - _config.castBarW / 2,
                                 _config.castBarY, _config.castBarW,
                                 _config.castBarH),
      CAST_BAR_DIMENSIONS(0, 0, _config.castBarW, _config.castBarH);
  static const Color CAST_BAR_LABEL_COLOR = Color::CAST_BAR_FONT;
  _castBar = new Element(CAST_BAR_RECT);
  _castBar->addChild(
      new ProgressBar<ms_t>(CAST_BAR_DIMENSIONS, _actionTimer, _actionLength));
  LinkedLabel<std::string> *castBarLabel = new LinkedLabel<std::string>(
      CAST_BAR_DIMENSIONS, _actionMessage, "", "", Label::CENTER_JUSTIFIED);
  castBarLabel->setColor(CAST_BAR_LABEL_COLOR);
  _castBar->addChild(castBarLabel);
  _castBar->hide();
  addUI(_castBar);
}

void Client::initHotbar() {
  const auto NUM_BUTTONS = 10;
  _hotbar = new Element({0, 0, NUM_BUTTONS * 18, 18});
  _hotbar->setPosition((SCREEN_X - _hotbar->width()) / 2,
                       SCREEN_Y - _hotbar->height());
  addUI(_hotbar);
}

void Client::populateHotbar() {
  _hotbar->clearChildren();

  auto i = 0;
  for (auto *spell : _knownSpells) {
    void *castMessageVoidPtr = const_cast<void *>(
        reinterpret_cast<const void *>(&spell->castMessage()));
    auto button = new Button({i * 18, 0, 18, 18}, {}, [castMessageVoidPtr]() {
      sendRawMessageStatic(castMessageVoidPtr);
    });
    button->addChild(new ColorBlock({0, 0, 18, 18}, Color::OUTLINE));
    button->addChild(new Picture(1, 1, spell->icon()));
    button->addChild(new OutlinedLabel({0, -1, 19, 18}, toString((i + 1) % 10),
                                       Element::RIGHT_JUSTIFIED));
    button->setTooltip(spell->tooltip());
    _hotbar->addChild(button);
    _hotbarButtons[i] = button;
    ++i;
  }
}

void Client::initBuffsDisplay() {
  const px_t WIDTH = 30, HEIGHT = 300, ROW_HEIGHT = 20;
  _buffsDisplay = new List({SCREEN_X - WIDTH, 0, WIDTH, HEIGHT}, ROW_HEIGHT);

  addUI(_buffsDisplay);

  refreshBuffsDisplay();
}

void Client::refreshBuffsDisplay() {
  _buffsDisplay->clearChildren();

  for (auto buff : _character.buffs())
    _buffsDisplay->addChild(assembleBuffEntry(*buff));
  for (auto buff : _character.debuffs())
    _buffsDisplay->addChild(assembleBuffEntry(*buff, true));
}

Element *Client::assembleBuffEntry(const ClientBuffType &type, bool isDebuff) {
  auto e = new Element();
  if (isDebuff)
    e->addChild(new ColorBlock({1, 1, 18, 18}, Color::COMBATANT_ENEMY));
  e->addChild(new Picture({2, 2, 16, 16}, type.icon()));
  e->setTooltip(type.name());
  return e;
}

void Client::initTargetBuffs() {
  const auto &targetPanelRect = _target.panel()->rect();
  const auto GAP = 1_px, X = targetPanelRect.x,
             Y = targetPanelRect.y + targetPanelRect.h + GAP, W = 400, H = 16;
  _targetBuffs = new Element({X, Y, W, H});

  addUI(_targetBuffs);

  refreshTargetBuffs();
}

void Client::refreshTargetBuffs() {
  _targetBuffs->clearChildren();

  if (!_target.exists()) return;

  auto rect = _targetBuffs->rect();
  rect.y = _target.panel()->rect().y + _target.panel()->rect().h + 1;
  _targetBuffs->rect(rect);

  auto x = 0_px;
  for (const auto buff : _target.combatant()->buffs()) {
    auto icon = new Picture({x, 0, 16, 16}, buff->icon());
    icon->setTooltip(buff->name());
    _targetBuffs->addChild(icon);
    x += 17;
  }
  for (const auto buff : _target.combatant()->debuffs()) {
    auto icon = new Picture({x, 0, 16, 16}, buff->icon());
    icon->setTooltip(buff->name());
    _targetBuffs->addChild(icon);
    x += 17;
  }
}

void Client::initializeEscapeWindow() {
  const auto BUTTON_WIDTH = 80, BUTTON_HEIGHT = 20, GAP = 3,
             WIN_WIDTH = BUTTON_WIDTH + 2 * GAP, NUM_BUTTONS = 2,
             WIN_HEIGHT = NUM_BUTTONS * BUTTON_HEIGHT + (NUM_BUTTONS + 1) * GAP;
  _escapeWindow =
      Window::WithRectAndTitle({0, 0, WIN_WIDTH, WIN_HEIGHT}, "System Menu"s);
  _escapeWindow->center();
  auto y = GAP;
  auto x = GAP;

  _escapeWindow->addChild(new Button({x, y, BUTTON_WIDTH, BUTTON_HEIGHT},
                                     "Exit to desktop"s,
                                     [this]() { exitGame(this); }));
  y += BUTTON_HEIGHT + GAP;

  _escapeWindow->addChild(
      new Button({x, y, BUTTON_WIDTH, BUTTON_HEIGHT}, "Return to game"s,
                 [this]() { Window::hideWindow(_escapeWindow); }));
}

void Client::updateUI() {
  if (_timeElapsed >= _errorMessageTimer) {
    _errorMessageTimer = 0;
    _lastErrorMessage->hide();
  } else
    _errorMessageTimer -= _timeElapsed;
  updateToasts();
}

void Client::initMenuBar() {
  static const px_t MENU_BUTTON_W = 12, MENU_BUTTON_H = 12, NUM_BUTTONS = 8;
  Element *menuBar = new Element({_hotbar->rect().x + _hotbar->width(),
                                  SCREEN_Y - MENU_BUTTON_H,
                                  MENU_BUTTON_W * NUM_BUTTONS, MENU_BUTTON_H});

  addButtonToMenu(menuBar, 0, _buildWindow, "icon-build.png",
                  "Build window (B)");
  addButtonToMenu(menuBar, 1, _craftingWindow, "icon-crafting.png",
                  "Crafting window (C)");
  addButtonToMenu(menuBar, 2, _inventoryWindow, "icon-inventory.png",
                  "Inventory window (I)");
  addButtonToMenu(menuBar, 3, _gearWindow, "icon-gear.png", "Gear window (G)");
  addButtonToMenu(menuBar, 4, _classWindow, "icon-class.png",
                  "Class window (K)");
  addButtonToMenu(menuBar, 5, _socialWindow, "icon-social.png",
                  "Social window (O)");
  addButtonToMenu(menuBar, 6, _chatContainer, "icon-chat.png",
                  "Toggle chat log");
  addButtonToMenu(menuBar, 7, _mapWindow, "icon-map.png", "Map (M)");
  addButtonToMenu(menuBar, 8, _helpWindow, "icon-help.png", "Help (H)");

  addUI(menuBar);
}

void Client::addButtonToMenu(Element *menuBar, size_t index, Element *toToggle,
                             const std::string iconFile,
                             const std::string tooltip) {
  static const px_t MENU_BUTTON_W = 12, MENU_BUTTON_H = 12;

  Button *button =
      new Button({MENU_BUTTON_W * static_cast<px_t>(index), 0, MENU_BUTTON_W,
                  MENU_BUTTON_H},
                 "", [toToggle]() { Element::toggleVisibilityOf(toToggle); });
  button->addChild(
      new Picture(2, 2, {"Images/UI/" + iconFile, Color::MAGENTA}));
  button->setTooltip(tooltip);
  menuBar->addChild(button);
}

void Client::initPerformanceDisplay() {
#ifndef _DEBUG
  return;
#endif
  static const px_t HARDWARE_STATS_W = 100, HARDWARE_STATS_H = 44,
                    HARDWARE_STATS_LABEL_HEIGHT = 11;
  static const ScreenRect HARDWARE_STATS_RECT(
      SCREEN_X - HARDWARE_STATS_W, 0, HARDWARE_STATS_W, HARDWARE_STATS_H);
  Element *hardwareStats = new Element(
      {SCREEN_X - HARDWARE_STATS_W, 0, HARDWARE_STATS_W, HARDWARE_STATS_H});
  LinkedLabel<unsigned> *fps = new LinkedLabel<unsigned>(
      {0, 0, HARDWARE_STATS_W, HARDWARE_STATS_LABEL_HEIGHT}, _fps, "", "fps",
      Label::RIGHT_JUSTIFIED);
  LinkedLabel<ms_t> *lat =
      new LinkedLabel<ms_t>({0, HARDWARE_STATS_LABEL_HEIGHT, HARDWARE_STATS_W,
                             HARDWARE_STATS_LABEL_HEIGHT},
                            _latency, "", "ms", Label::RIGHT_JUSTIFIED);
  LinkedLabel<size_t> *numEntities = new LinkedLabel<size_t>(
      {0, HARDWARE_STATS_LABEL_HEIGHT * 2, HARDWARE_STATS_W,
       HARDWARE_STATS_LABEL_HEIGHT},
      _numEntities, "", " sprites", Label::RIGHT_JUSTIFIED);
  LinkedLabel<int> *channels = new LinkedLabel<int>(
      {0, HARDWARE_STATS_LABEL_HEIGHT * 3, HARDWARE_STATS_W,
       HARDWARE_STATS_LABEL_HEIGHT},
      _channelsPlaying, "", "/" + makeArgs(MIXING_CHANNELS) + " channels",
      Label::RIGHT_JUSTIFIED);
  fps->setColor(Color::PERFORMANCE_FONT);
  lat->setColor(Color::PERFORMANCE_FONT);
  numEntities->setColor(Color::PERFORMANCE_FONT);
  channels->setColor(Color::PERFORMANCE_FONT);
  hardwareStats->addChild(fps);
  hardwareStats->addChild(lat);
  hardwareStats->addChild(numEntities);
  hardwareStats->addChild(channels);
  addUI(hardwareStats);
}

void Client::initPlayerPanels() {
  px_t playerPanelX = CombatantPanel::GAP, playerPanelY = CombatantPanel::GAP;
  CombatantPanel *playerPanel = new CombatantPanel(
      playerPanelX, playerPanelY, _username, _character.health(),
      _character.maxHealth(), _character.energy(), _character.maxEnergy());
  playerPanel->changeColor(Color::COMBATANT_SELF);
  addUI(playerPanel);
  /*
  initializeMenu() must be called before initializePanel(), otherwise the
  right-click menu won't work.
  */
  _target.initializeMenu();
  _target.initializePanel();
  addUI(_target.panel());
  addUI(_target.menu());
}

void Client::initQuestProgress() {
  const auto WIDTH = 100_px, Y = 100_px, HEIGHT = 200_px;
  _questProgress =
      new List{{SCREEN_X - WIDTH, Y, WIDTH, HEIGHT}, Element::TEXT_HEIGHT + 2};
  addUI(_questProgress);
  _questProgress->ignoreMouseEvents();
}

void Client::refreshQuestProgress() {
  _questProgress->clearChildren();

  auto isEmpty = true;
  for (const auto &pair : _quests) {
    const auto &quest = pair.second;
    if (!quest.userIsOnQuest()) continue;

    if (!isEmpty) _questProgress->addGap();
    isEmpty = false;

    auto questName = new OutlinedLabel({}, quest.info().name);
    questName->setColor(Color::QUEST_NAME);
    _questProgress->addChild(questName);

    for (auto i = 0; i != quest.info().objectives.size(); ++i) {
      const auto &objective = quest.info().objectives[i];

      auto objectiveText = " "s + objective.text;

      if (objective.qty > 1) {
        objectiveText += " ("s + toString(quest.getProgress(i)) + "/"s +
                         toString(objective.qty) + ")";
      }
      auto objectiveLabel = new OutlinedLabel({}, objectiveText);
      auto objectiveComplete = quest.getProgress(i) == objective.qty;
      objectiveLabel->setColor(objectiveComplete ? Color::QUEST_COMPLETE
                                                 : Color::QUEST_OBJECTIVE);
      _questProgress->addChild(objectiveLabel);
    }
  }
}
