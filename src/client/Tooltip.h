#ifndef TOOLTIP_BUILDER_H
#define TOOLTIP_BUILDER_H

#include <SDL_ttf.h>

#include <memory>
#include <string>
#include <vector>

#include "../Color.h"
#include "../types.h"
#include "Texture.h"
#include "WordWrapper.h"

class ClientItem;
using ClientItemVector = std::vector<std::pair<const ClientItem *, size_t> >;
struct ClientMerchantSlot;

class Tooltip {
  Color _color{Color::TOOLTIP_BODY};
  std::vector<Texture>
      _content;  // The lines of text; an empty texture implies a gap.

  static const px_t PADDING;
  static TTF_Font *font;
  const static px_t DEFAULT_MAX_WIDTH;

  static std::unique_ptr<WordWrapper> wordWrapper;

  mutable Texture _generated{};
  void generateIfNecessary() const;
  void generate() const;
  static ms_t timeThatTheLastRedrawWasOrdered;
  mutable ms_t _timeGenerated{};

  static const Tooltip NO_TOOLTIP;

 public:
  Tooltip();

  const static px_t NO_WRAP;

  void setColor(const Color &color = Color::TOOLTIP_BODY);

  void addLine(const std::string &line);
  using Lines = std::vector<std::string>;
  void addLines(const Lines &lines);
  void embed(const Tooltip &subTooltip);
  void addItemGrid(const ClientItemVector &items);
  void addMerchantSlots(const std::vector<ClientMerchantSlot> &slots);

  px_t width() const;
  px_t height() const;

  void addGap();

  void draw(ScreenPoint p) const;

  // const Texture &get();
  static void forceAllToRedraw();

  // Create a basic tooltip containing a single string.
  static Tooltip basicTooltip(const std::string &text);

  static const Tooltip &noTooltip() { return NO_TOOLTIP; }
};

#endif
