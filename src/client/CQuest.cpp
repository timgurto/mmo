#include "CQuest.h"
#include "../Rect.h"
#include "CQuest.h"
#include "Client.h"
#include "ui/Window.h"

void CQuest::generateWindow(CQuest *quest, size_t startObjectSerial,
                            Transition pendingTransition) {
  const auto WIN_W = 200_px, WIN_H = 200_px;

  auto window = Window::WithRectAndTitle({0, 0, WIN_W, WIN_H}, "Quest");
  window->center();

  const auto BOTTOM = window->contentHeight();
  const auto GAP = 2_px;
  auto y = GAP;

  // Quest name
  auto name = new Label({GAP, y, WIN_W, Element::TEXT_HEIGHT}, quest->name());
  name->setColor(Color::HELP_TEXT_HEADING);
  window->addChild(name);

  // Transition button
  const auto BUTTON_W = 90_px, BUTTON_H = 16_px,
             BUTTON_Y = BOTTOM - GAP - BUTTON_H;
  const auto TRANSITION_BUTTON_RECT =
      ScreenRect{GAP, BUTTON_Y, BUTTON_W, BUTTON_H};
  Button *button = nullptr;
  switch (pendingTransition) {
    case ACCEPT:
      button = new Button(TRANSITION_BUTTON_RECT, "Accept quest",
                          [=]() { acceptQuest(quest, startObjectSerial); });
      button->id("accept");
      break;
    case COMPLETE:
      button = new Button(TRANSITION_BUTTON_RECT, "Complete quest",
                          [=]() { completeQuest(quest, startObjectSerial); });
      break;
    default:
      button = new Button{{}};
  }
  window->addChild(button);

  quest->_window = window;
  Client::instance().addWindow(quest->_window);
  quest->_window->show();
}

void CQuest::acceptQuest(CQuest *quest, size_t startObjectSerial) {
  auto &client = Client::instance();

  // Send message
  client.sendMessage(CL_ACCEPT_QUEST, makeArgs(quest->id(), startObjectSerial));

  // Close and remove window
  quest->_window->hide();
  // TODO: better cleanup.  Lots of unused windows in the background may take up
  // significant memory.  Note that this function is called from a button click
  // (which subsequently changes the appearance of the button), meaning it is
  // unsafe to delete the window here.
}

void CQuest::completeQuest(CQuest *quest, size_t startObjectSerial) {
  auto &client = Client::instance();

  // Send message
  client.sendMessage(CL_COMPLETE_QUEST,
                     makeArgs(quest->id(), startObjectSerial));

  // Close and remove window
  quest->_window->hide();
  // TODO: better cleanup.  Lots of unused windows in the background may take up
  // significant memory.  Note that this function is called from a button click
  // (which subsequently changes the appearance of the button), meaning it is
  // unsafe to delete the window here.
}
