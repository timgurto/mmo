#include "Client.h"

static Window *assigner{nullptr};
static List *spellList{nullptr};
using Actions = std::vector<const ClientSpell *>;
Actions actions;
using Icons = std::vector<Picture *>;
Icons icons;

static const int NO_BUTTON_BEING_ASSIGNED{-1};
static int buttonBeingAssigned{NO_BUTTON_BEING_ASSIGNED};

static void performAction(int i) {
  auto &client = Client::instance();
  if (actions[i]) client.sendMessage(CL_CAST, actions[i]->id());
}

void Client::initHotbar() {
  actions = Actions(NUM_HOTBAR_BUTTONS, nullptr);
  icons = Icons(NUM_HOTBAR_BUTTONS, nullptr);
  _hotbar = new Element({0, 0, NUM_HOTBAR_BUTTONS * 18, 18});
  _hotbar->setPosition((SCREEN_X - _hotbar->width()) / 2,
                       SCREEN_Y - _hotbar->height());

  for (auto i = 0; i != NUM_HOTBAR_BUTTONS; ++i) {
    auto button =
        new Button({i * 18, 0, 18, 18}, {}, [i]() { performAction(i); });

    // Icons
    auto picture = new Picture({0, 0, ICON_SIZE, ICON_SIZE}, {});
    icons[i] = picture;
    button->addChild(picture);

    // Hotkey glyph
    static const auto HOTKEY_GLYPHS = std::vector<std::string>{
        "'", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
    button->addChild(new OutlinedLabel({0, -1, 15, 18}, HOTKEY_GLYPHS[i],
                                       Element::RIGHT_JUSTIFIED));

    // RMB: Assign
    button->setRightMouseUpFunction(
        [i](Element &e, const ScreenPoint &mousePos) {
          if (!collision(mousePos, {0, 0, e.rect().w, e.rect().h})) return;

          Client::instance().populateAssignerWindow();
          assigner->show();
          buttonBeingAssigned = i;
          Client::debug() << "Assigning button " << i << Log::endl;
        },
        nullptr);

    _hotbar->addChild(button);
    _hotbarButtons[i] = button;
  }

  addUI(_hotbar);
  initAssignerWindow();
}

void Client::refreshHotbar() {
  for (auto i = 0; i != NUM_HOTBAR_BUTTONS; ++i) {
    if (actions[i]) {
      _hotbarButtons[i]->enable();
      const auto &spell = *actions[i];

      icons[i]->changeTexture(spell.icon());

      _hotbarButtons[i]->setTooltip(spell.tooltip());

      auto it = _spellCooldowns.find(spell.id());
      auto spellIsCoolingDown = it != _spellCooldowns.end() && it->second > 0;
      if (spellIsCoolingDown) _hotbarButtons[i]->disable();

    } else {
      _hotbarButtons[i]->setTooltip(
          "Right-click to assign an action to this button."s);
      _hotbarButtons[i]->disable();
    }
  }
}

void Client::initAssignerWindow() {
  static const auto LIST_WIDTH = 150_px, LIST_HEIGHT = 250_px;
  assigner = Window::WithRectAndTitle({0, 0, LIST_WIDTH, LIST_HEIGHT},
                                      "Assign action"s);
  auto x = (SCREEN_X - assigner->rect().w) / 2;
  auto y = _hotbar->rect().y - assigner->rect().h;
  assigner->rect({x, y, LIST_WIDTH, LIST_HEIGHT});
  spellList = new List({0, 0, LIST_WIDTH, LIST_HEIGHT}, Client::ICON_SIZE + 2);
  assigner->addChild(spellList);

  addWindow(assigner);
}

void Client::populateAssignerWindow() {
  spellList->clearChildren();
  for (const auto *spell : _knownSpells) {
    auto button = new Button({}, {}, [this, spell]() {
      if (buttonBeingAssigned == NO_BUTTON_BEING_ASSIGNED) return;
      actions[buttonBeingAssigned] = spell;
      assigner->hide();
      buttonBeingAssigned = NO_BUTTON_BEING_ASSIGNED;
      refreshHotbar();
    });
    button->addChild(new Picture(0, 0, spell->icon()));
    button->addChild(new Label({ICON_SIZE + 3, 0, 200, ICON_SIZE},
                               spell->name(), Element::LEFT_JUSTIFIED,
                               Element::CENTER_JUSTIFIED));
    button->id(spell->id());
    button->setTooltip(spell->tooltip());
    spellList->addChild(button);
  }
}
