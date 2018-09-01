#ifndef COLOR_H
#define COLOR_H

#include <iostream>

#include "SDL.h"

class Color {
 public:
  static const Color TODO;

  static const Color BLUE_HELL;
  static const Color NO_KEY;
  static const Color DEBUG_TEXT;

  static const Color BLACK;
  static const Color BLUE;
  static const Color GREEN;
  static const Color CYAN;
  static const Color RED;
  static const Color MAGENTA;
  static const Color YELLOW;
  static const Color WHITE;

  static const Color CHAT_BACKGROUND;
  static const Color CHAT_DEFAULT;
  static const Color CHAT_SAY;
  static const Color CHAT_WHISPER;
  static const Color CHAT_ERROR;
  static const Color CHAT_SUCCESS;

  static const Color WINDOW_BACKGROUND;
  static const Color WINDOW_DARK;
  static const Color WINDOW_LIGHT;
  static const Color WINDOW_FONT;
  static const Color WINDOW_HEADING;

  static const Color COMBATANT_SELF;
  static const Color COMBATANT_ALLY;
  static const Color COMBATANT_DEFENSIVE;
  static const Color COMBATANT_ENEMY;
  static const Color COMBATANT_NEUTRAL;

  static const Color TOOLTIP_BACKGROUND;
  static const Color TOOLTIP_BORDER;
  static const Color TOOLTIP_NAME;
  static const Color TOOLTIP_BODY;

  static const Color STAT_HEALTH;
  static const Color STAT_ENERGY;
  static const Color STAT_AIR;
  static const Color STAT_EARTH;
  static const Color STAT_FIRE;
  static const Color STAT_WATER;

  static const Color FLOATING_CORE;
  static const Color FLOATING_DAMAGE;
  static const Color FLOATING_HEAL;
  static const Color FLOATING_MISS;
  static const Color FLOATING_CRIT;

  static const Color UI_OUTLINE;
  static const Color UI_TEXT;
  static const Color UI_DISABLED;

  Color(Uint8 r, Uint8 g, Uint8 b);
  Color(const SDL_Color &rhs);
  Color(Uint32 rhs = 0);

  operator SDL_Color() const;
  operator Uint32() const;  // Used by some SDL functions

  // Multiply or divide color by a scalar
  Color operator/(double s) const;
  Color operator/(int s) const;  // to avoid ambiguity with implicit Uint32 cast
  Color operator*(double s) const;
  Color operator*(int s) const;  // to avoid ambiguity with implicit Uint32 cast

  Uint8 r() const { return _r; }
  Uint8 g() const { return _g; }
  Uint8 b() const { return _b; }

 private:
  Uint8 _r, _g, _b;
};

Color operator+(const Color &lhs, const Color &rhs);

std::ostream &operator<<(std::ostream &lhs, const Color &rhs);

#endif
