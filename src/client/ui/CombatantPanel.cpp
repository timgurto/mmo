#include "CombatantPanel.h"
#include "ColorBlock.h"
#include "LinkedLabel.h"
#include "ShadowBox.h"

CombatantPanel::CombatantPanel(px_t panelX, px_t panelY,
                               const std::string &name, const Hitpoints &health,
                               const Hitpoints &maxHealth, const Energy &energy,
                               const Energy &maxEnergy)
    : Element({panelX, panelY, WIDTH, HEIGHT}) {
  _background = new ColorBlock({0, 0, WIDTH, HEIGHT});
  addChild(_background);

  _outline = new ShadowBox({0, 0, WIDTH, HEIGHT});
  addChild(_outline);

  auto y = GAP;
  addChild(new LinkedLabel<std::string>(
      {GAP, y, ELEMENT_WIDTH, Element::TEXT_HEIGHT}, name, {}, {},
      Element::CENTER_JUSTIFIED));
  y += Element::TEXT_HEIGHT + GAP;

  _healthBar =
      new ProgressBar<Hitpoints>({GAP, y, ELEMENT_WIDTH, BAR_HEIGHT}, health,
                                 maxHealth, Color::STAT_HEALTH);
  addChild(_healthBar);
  _healthBar->showValuesInTooltip(" health");
  y += BAR_HEIGHT + GAP;

  _energyBar = new ProgressBar<Energy>({GAP, y, ELEMENT_WIDTH, BAR_HEIGHT},
                                       energy, maxEnergy, Color::STAT_ENERGY);
  _energyBar->showValuesInTooltip(" energy");
  addChild(_energyBar);
  y += BAR_HEIGHT + GAP;

  this->height(y);
}

void CombatantPanel::showEnergyBar() {
  if (!_energyBar->visible()) {
    _energyBar->show();
    height(Element::height() + BAR_HEIGHT + GAP);
  }
}

void CombatantPanel::hideEnergyBar() {
  if (_energyBar->visible()) {
    _energyBar->hide();
    height(Element::height() - BAR_HEIGHT - GAP);
  }
}

void CombatantPanel::addXPBar(const XP &xp, const XP &maxXP) {
  _xpBar = new ProgressBar<Energy>(
      {GAP, Element::height(), ELEMENT_WIDTH, BAR_HEIGHT}, xp, maxXP,
      Color::STAT_XP);
  height(Element::height() + BAR_HEIGHT + GAP);
  _xpBar->showValuesInTooltip(" experience");
  addChild(_xpBar);
}

void CombatantPanel::height(px_t h) {
  Element::height(h);
  _background->height(h);
  _outline->height(h);
}
