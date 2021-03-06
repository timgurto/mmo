#include "ItemSelector.h"

#include "../Client.h"
#include "Label.h"
#include "Line.h"
#include "Picture.h"
#include "TextBox.h"
#include "Window.h"

const px_t ItemSelector::GAP = 2;
const px_t ItemSelector::LABEL_WIDTH = 120;
const px_t ItemSelector::WIDTH = 80;
const px_t ItemSelector::LABEL_TOP = (ITEM_HEIGHT - TEXT_HEIGHT) / 2;
const px_t ItemSelector::LIST_WIDTH = LABEL_WIDTH + ITEM_HEIGHT + 2 + GAP;
const px_t ItemSelector::LIST_GAP = 0;
const px_t ItemSelector::WINDOW_WIDTH = LIST_WIDTH + 2 * GAP;
const px_t ItemSelector::WINDOW_HEIGHT = 200;
const px_t ItemSelector::SEARCH_BUTTON_WIDTH = 40;
const px_t ItemSelector::SEARCH_BUTTON_HEIGHT = 13;
const px_t ItemSelector::SEARCH_TEXT_WIDTH =
    LIST_WIDTH - GAP - SEARCH_BUTTON_WIDTH;
const px_t ItemSelector::LIST_HEIGHT =
    WINDOW_HEIGHT - SEARCH_BUTTON_HEIGHT - 4 * GAP - 2;

ClientItem **ItemSelector::_itemBeingSelected = nullptr;

ItemSelector::ItemSelector(Client &client, const ClientItem *&item, px_t x,
                           px_t y)
    : Button({x, y, WIDTH, Element::ITEM_HEIGHT + 2}),
      _item(item),
      _lastItem(item),
      _icon(new Picture({1, 1, ITEM_HEIGHT, ITEM_HEIGHT}, Texture())),
      _name(new Label({ITEM_HEIGHT + 1 + GAP, 1, WIDTH, ITEM_HEIGHT}, "",
                      Element::LEFT_JUSTIFIED, Element::CENTER_JUSTIFIED)) {
  setClient(client);
  addChild(_icon);
  addChild(_name);

  auto ppItem = &_item;
  clickFun([this, ppItem]() { openFindItemWindow(ppItem); });

  if (_findItemWindow == nullptr) {
    _findItemWindow = Window::WithRectAndTitle(

        {(Client::SCREEN_X - WINDOW_WIDTH) / 2,
         (Client::SCREEN_Y - WINDOW_HEIGHT) / 2, WINDOW_WIDTH, WINDOW_HEIGHT},
        "Find Item", client.mouse());
    px_t y = GAP;
    _filterText =
        new TextBox(client, {GAP, y, SEARCH_TEXT_WIDTH, SEARCH_BUTTON_HEIGHT},
                    TextBox::LETTERS);
    _findItemWindow->addChild(_filterText);
    _findItemWindow->addChild(
        new Button({SEARCH_TEXT_WIDTH + 2 * GAP, y, SEARCH_BUTTON_WIDTH,
                    SEARCH_BUTTON_HEIGHT},
                   "Search", [this]() { applyFilter(); }));
    y += SEARCH_BUTTON_HEIGHT + GAP;
    _findItemWindow->addChild(new Line(0, y, WINDOW_WIDTH));
    y += 2 + GAP;
    _itemList =
        new List({GAP, y, LIST_WIDTH, LIST_HEIGHT}, ITEM_HEIGHT + 2 + LIST_GAP);
    _findItemWindow->addChild(_itemList);

    _client->addWindow(_findItemWindow);
  }
}

void ItemSelector::openFindItemWindow(void *data) {
  _findItemWindow->show();
  applyFilter();
  _client->removeWindow(_findItemWindow);
  _client->addWindow(_findItemWindow);
  _itemBeingSelected = static_cast<ClientItem **>(data);
}

void ItemSelector::applyFilter() {
  _itemList->clearChildren();
  const std::string &filterText = _filterText->text();

  const auto &items = _client->gameData.items;
  for (const auto &pair : _client->gameData.items) {
    const ClientItem &item = pair.second;
    if (filterText == "" || itemMatchesFilter(item, filterText)) {
      // Add item to list
      Element *container = new Element();
      _itemList->addChild(container);
      auto pItem = const_cast<ClientItem *>(&item);
      Button *itemButton =
          new Button({0, 0, LIST_WIDTH - List::ARROW_W, ITEM_HEIGHT + 2}, "",
                     [this, pItem]() { selectItem(pItem); });
      container->addChild(itemButton);
      itemButton->addChild(
          new Picture({1, 1, ITEM_HEIGHT, ITEM_HEIGHT}, item.icon()));
      itemButton->addChild(
          new Label({ITEM_HEIGHT + GAP, LABEL_TOP, LABEL_WIDTH, TEXT_HEIGHT},
                    item.name()));
    }
  }

  // Add 'none' option
  Element *container = new Element();
  _itemList->addChild(container);
  Button *itemButton =
      new Button({0, 0, LIST_WIDTH - List::ARROW_W, ITEM_HEIGHT + 2}, "",
                 [this]() { selectItem(nullptr); });
  container->addChild(itemButton);
  itemButton->addChild(new Label(
      {ITEM_HEIGHT + GAP, LABEL_TOP, LABEL_WIDTH, TEXT_HEIGHT}, "[None]"));
}

void ItemSelector::selectItem(void *data) {
  *_itemBeingSelected = static_cast<ClientItem *>(data);
  _findItemWindow->hide();
}

bool ItemSelector::itemMatchesFilter(const ClientItem &item,
                                     const std::string &filter) {
  auto lowerCaseFilter = toLower(filter);

  // Name matches
  auto lowerCaseName = toLower(item.name());
  if (lowerCaseName.find(lowerCaseFilter) != std::string::npos) return true;

  // Tag matches
  for (const auto &pair : item.tags()) {
    const auto &tagName = pair.first;
    auto lowerCaseTag = toLower(tagName);
    if (lowerCaseTag.find(lowerCaseFilter) != std::string::npos) return true;
  }

  return false;
}

void ItemSelector::checkIfChanged() {
  if (_lastItem != _item) {
    _lastItem = _item;
    markChanged();
  }
  Element::checkIfChanged();
}

void ItemSelector::refresh() {
  if (_item == nullptr) {
    _icon->changeTexture();
    _name->changeText("[None]");
  } else {
    _icon->changeTexture(_item->icon());
    _name->changeText(_item->name());
  }
}
