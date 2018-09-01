#include "CQuest.h"
#include "../Rect.h"
#include "CQuest.h"
#include "Client.h"
#include "WordWrapper.h"
#include "ui/Line.h"
#include "ui/Window.h"

CQuest::CQuest(const Info &info)
    : _info(info), _progress(info.objectives.size(), 0) {}

void CQuest::generateWindow(CQuest *quest, size_t startObjectSerial,
                            Transition pendingTransition) {
  const auto WIN_W = 200_px, WIN_H = 200_px;

  auto window = Window::WithRectAndTitle({0, 0, WIN_W, WIN_H}, "Quest");
  window->center();

  const auto BOTTOM = window->contentHeight();
  const auto GAP = 2_px, BUTTON_W = 90_px, BUTTON_H = 16_px,
             CONTENT_W = WIN_W - 2 * GAP;
  auto y = GAP;

  // Quest name
  auto name =
      new Label({GAP, y, WIN_W, Element::TEXT_HEIGHT}, quest->_info.name);
  name->setColor(Color::WINDOW_HEADING);
  window->addChild(name);
  y += name->height() + GAP;

  // Body: brief/debrief
  const auto BODY_H = BOTTOM - 2 * GAP - BUTTON_H - y;
  auto body = new List({GAP, y, CONTENT_W, BODY_H});
  window->addChild(body);
  auto ww = WordWrapper{Element::font(), body->contentWidth()};
  const auto &bodyText =
      pendingTransition == ACCEPT ? quest->_info.brief : quest->_info.debrief;
  auto lines = ww.wrap(bodyText);
  for (auto line : lines) {
    auto isHelpText = false;

    if (line.front() == '?') {
      isHelpText = true;
      line = line.substr(1);
    }
    auto label = new Label({}, line);
    body->addChild(label);
    if (isHelpText) {
      label->setColor(Color::TODO);
    }
  }

  // Body: objectives
  auto shouldShowObjectives =
      pendingTransition == ACCEPT && !quest->_info.objectives.empty();
  if (shouldShowObjectives) {
    body->addGap();
    auto heading = new Label({}, "Objectives:");
    heading->setColor(Color::WINDOW_HEADING);
    body->addChild(heading);

    for (auto &objective : quest->_info.objectives) {
      auto text = objective.text;
      if (objective.qty > 1) text += " ("s + toString(objective.qty) + ")"s;
      body->addChild(new Label({}, text));
    }
  }

  y += BODY_H + GAP;

  // Transition button
  const auto TRANSITION_BUTTON_RECT = ScreenRect{GAP, y, BUTTON_W, BUTTON_H};
  auto transitionName =
      pendingTransition == ACCEPT ? "Accept quest"s : "Complete quest"s;
  auto transitionFun =
      pendingTransition == ACCEPT ? acceptQuest : completeQuest;
  Button *transitionButton =
      new Button(TRANSITION_BUTTON_RECT, transitionName,
                 [=]() { transitionFun(quest, startObjectSerial); });
  if (pendingTransition == ACCEPT) transitionButton->id("accept");
  window->addChild(transitionButton);

  quest->_window = window;
  Client::instance().addWindow(quest->_window);
  quest->_window->show();
}

void CQuest::acceptQuest(CQuest *quest, size_t startObjectSerial) {
  auto &client = Client::instance();

  // Send message
  client.sendMessage(CL_ACCEPT_QUEST,
                     makeArgs(quest->_info.id, startObjectSerial));

  // Close and remove window
  quest->_window->hide();
  // TODO: better cleanup.  Lots of unused windows in the background may take
  // up significant memory.  Note that this function is called from a button
  // click (which subsequently changes the appearance of the button), meaning
  // it is unsafe to delete the window here.

  // Show specified help topic
  if (!quest->_info.helpTopicOnAccept.empty())
    client.showHelpTopic(quest->_info.helpTopicOnAccept);
}

void CQuest::completeQuest(CQuest *quest, size_t startObjectSerial) {
  auto &client = Client::instance();

  // Send message
  client.sendMessage(CL_COMPLETE_QUEST,
                     makeArgs(quest->_info.id, startObjectSerial));

  // Close and remove window
  quest->_window->hide();
  // TODO: better cleanup.  Lots of unused windows in the background may take
  // up significant memory.  Note that this function is called from a button
  // click (which subsequently changes the appearance of the button), meaning
  // it is unsafe to delete the window here.
}

void CQuest::setProgress(size_t objective, int progress) {
  _progress[objective] = progress;
}

int CQuest::getProgress(size_t objective) const {
  return _progress.at(objective);
}
