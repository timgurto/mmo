#include "Tooltip.h"

#include <sstream>

#include "Client.h"
#include "ClientItem.h"
#include "Renderer.h"

extern Renderer renderer;

const px_t Tooltip::PADDING =
    4;  // Margins, and the height of gaps between lines.
TTF_Font *Tooltip::font = nullptr;
const px_t Tooltip::DEFAULT_MAX_WIDTH = 150;
const px_t Tooltip::NO_WRAP = 0;
std::unique_ptr<WordWrapper> Tooltip::wordWrapper;
ms_t Tooltip::timeThatTheLastRedrawWasOrdered{};
const Tooltip Tooltip::NO_TOOLTIP{};

Tooltip::Tooltip() {
  if (font == nullptr) font = TTF_OpenFont("AdvoCut.ttf", 10);
}

void Tooltip::setColor(const Color &color) { _color = color; }

void Tooltip::addLine(const std::string &line) {
  if (line == "") {
    addGap();
    return;
  }

  if (!wordWrapper) {
    wordWrapper =
        std::make_unique<WordWrapper>(WordWrapper(font, DEFAULT_MAX_WIDTH));
  }
  auto wrappedLines = wordWrapper->wrap(line);
  for (const auto &wrappedLine : wrappedLines)
    _content.push_back({font, wrappedLine, _color});
}

void Tooltip::addLines(const Lines &lines) {
  for (auto &line : lines) addLine(line);
}

void Tooltip::embed(const Tooltip &subTooltip) {
  subTooltip.generateIfNecessary();
  _content.push_back(subTooltip._generated);
}

void Tooltip::addItemGrid(const ClientItemVector &items) {
  static const size_t MAX_COLS = 5;
  static const auto GAP = 1_px;
  auto cols = max(items.size(), MAX_COLS);
  auto rows = (items.size() - 1) / MAX_COLS + 1;
  auto gridW = static_cast<px_t>(cols * (Client::ICON_SIZE + GAP) - GAP);
  auto gridH = static_cast<px_t>(rows * (Client::ICON_SIZE + GAP) - GAP);
  auto grid = Texture{gridW, gridH};

  auto xIndex = 0, yIndex = 0;
  renderer.pushRenderTarget(grid);
  for (auto slot : items) {
    auto x = xIndex * (Client::ICON_SIZE + GAP);
    auto y = yIndex * (Client::ICON_SIZE + GAP);

    // Background
    renderer.setDrawColor(Color::BLACK);
    renderer.fillRect({x, y, Client::ICON_SIZE, Client::ICON_SIZE});

    // Icon
    auto item = slot.first;
    if (item) item->icon().draw(x, y);

    ++xIndex;
    if (xIndex >= MAX_COLS) {
      xIndex = 0;
      ++yIndex;
    }
  }
  renderer.popRenderTarget();

  grid.setBlend();
  _content.push_back(grid);
}

void Tooltip::addMerchantSlots(const std::vector<ClientMerchantSlot> &slots) {
  static const auto GAP = 1_px, SPACE_BETWEEN_ICONS = 20,
                    WARE_X = Client::ICON_SIZE + SPACE_BETWEEN_ICONS,
                    TOTAL_W = 2 * Client::ICON_SIZE + SPACE_BETWEEN_ICONS;

  auto numActiveSlots = 0;
  for (const auto &slot : slots)
    if (slot.priceItem || slot.wareItem) ++numActiveSlots;

  auto textureHeight = numActiveSlots * (Client::ICON_SIZE + GAP) - GAP;
  auto texture = Texture{TOTAL_W, textureHeight};
  renderer.pushRenderTarget(texture);

  auto y = 0_px;
  for (const auto &slot : slots) {
    if (!slot.priceItem && !slot.wareItem) continue;

    if (slot.priceItem) slot.priceItem->icon().draw(0, y);
    if (slot.wareItem) slot.wareItem->icon().draw(WARE_X, y);

    y += Client::ICON_SIZE + GAP;
  }

  texture.setBlend();
  renderer.popRenderTarget();
  _content.push_back(texture);
}

px_t Tooltip::width() const {
  generateIfNecessary();
  return _generated.width();
}

px_t Tooltip::height() const {
  generateIfNecessary();
  return _generated.height();
}

void Tooltip::addGap() { _content.push_back(Texture()); }

void Tooltip::draw(ScreenPoint p) const {
  generateIfNecessary();
  if (this != &NO_TOOLTIP) _generated.draw(p.x, p.y);
}

void Tooltip::forceAllToRedraw() {
  timeThatTheLastRedrawWasOrdered = SDL_GetTicks();
}

void Tooltip::generateIfNecessary() const {
  if (!_generated || _timeGenerated < timeThatTheLastRedrawWasOrdered)
    generate();
}

void Tooltip::generate() const {
  // Calculate height and width of final tooltip
  px_t totalHeight = 2 * PADDING, totalWidth = 0;
  for (const Texture &item : _content) {
    if (item) {
      totalHeight += item.height();
      if (item.width() > totalWidth) totalWidth = item.width();
    } else {
      totalHeight += PADDING;
    }
  }
  totalWidth += 2 * PADDING;

  // Create background
  Texture background(totalWidth, totalHeight);
  renderer.pushRenderTarget(background);
  renderer.setDrawColor(Color::TOOLTIP_BACKGROUND);
  renderer.clear();
  background.setAlpha(0xdf);
  renderer.popRenderTarget();

  // Draw background
  _generated = Texture{totalWidth, totalHeight};
  renderer.pushRenderTarget(_generated);
  background.draw();

  // Draw border
  renderer.setDrawColor(Color::TOOLTIP_BORDER);
  renderer.drawRect({0, 0, totalWidth, totalHeight});

  // Draw text
  px_t y = PADDING;
  for (const Texture &item : _content) {
    if (!item)
      y += PADDING;
    else {
      item.draw(PADDING, y);
      y += item.height();
    }
  }

  _generated.setBlend(SDL_BLENDMODE_BLEND);
  renderer.popRenderTarget();

  _timeGenerated = SDL_GetTicks();
}

Tooltip Tooltip::basicTooltip(const std::string &text) {
  Tooltip tb;
  tb.addLine(text);
  return tb;
}
