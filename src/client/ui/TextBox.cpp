#include <cassert>

#include "ShadowBox.h"
#include "TextBox.h"
#include "../Renderer.h"

extern Renderer renderer;

TextBox *TextBox::currentFocus = nullptr;

TextBox::TextBox(const ScreenRect &rect, ValidInput validInput):
Element({rect.x, rect.y, rect.w, HEIGHT}),
_validInput(validInput)
{
    addChild(new ShadowBox({ 0, 0, rect.w, HEIGHT }, true));
    setLeftMouseDownFunction(&click);
}

void TextBox::text(const std::string &text){
    _text = text;
    markChanged();
}

void TextBox::refresh(){
    // Background
    renderer.setDrawColor(Element::BACKGROUND_COLOR);
    renderer.fill();

    // Text
    static const px_t
        TEXT_GAP = 2;
    Texture text(Element::font(), _text);
    text.draw(TEXT_GAP, TEXT_GAP);

    // Cursor
    const static px_t
        CURSOR_GAP = 0,
        CURSOR_WIDTH = 1;
    if (currentFocus == this) {
        renderer.setDrawColor(Element::FONT_COLOR);
        renderer.fillRect({ TEXT_GAP + text.width() + CURSOR_GAP, 1, CURSOR_WIDTH, HEIGHT - 2 });
    }
}

void TextBox::clearFocus(){
    if (currentFocus != nullptr)
        currentFocus->markChanged();
    currentFocus = nullptr;
}

void TextBox::click(Element &e, const ScreenPoint &mousePos){
    TextBox *newFocus = dynamic_cast<TextBox *>(&e);
    if (newFocus == currentFocus)
        return;

    // Mark changed, to (un)draw cursor
    e.markChanged();
    if (currentFocus != nullptr)
        currentFocus->markChanged();

    currentFocus = newFocus;
}

void TextBox::forcePascalCase() {
    if (_text.size()&& _text[0] >= 'a' && _text[0] <= 'z')
        _text[0] = 'A' + (_text[0] - 'a');

    for (size_t i = 1; i <= _text.size(); ++i) 
        if (_text[i] >= 'A' && _text[i] <= 'Z')
            _text[i] = 'a' + (_text[i] - 'A');
}

void TextBox::setOnChange(OnChangeFunction function, void * data) {
    _onChangeFunction = function;
    _onChangeData = data;
}

void TextBox::addText(const char *newText){
    assert(currentFocus);
    assert(newText[1] == '\0');

    const auto &newChar = newText[0];
    if (!currentFocus->isInputValid(newChar))
        return;

    std::string &text = currentFocus->_text;
    if (text.size() < MAX_TEXT_LENGTH) {
        text.append(newText);
        currentFocus->onChange();
        currentFocus->markChanged();
    }
}

bool TextBox::isInputValid(char c) const {
    switch (_validInput) {
    case NUMERALS:
        if (c < '0' || c > '9') return false;
        break;
    case LETTERS:
        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z')) return false;
        break;
    default:
        ; // All allowed
    }
    return true;
}

void TextBox::onChange() {
    if (_onChangeFunction)
        _onChangeFunction(_onChangeData);
}

void TextBox::backspace(){
    assert(currentFocus);

    std::string &text = currentFocus->_text;
    if (text.size() > 0) {
        text.erase(text.size() - 1);
        currentFocus->onChange();
        currentFocus->markChanged();
    }
}

size_t TextBox::textAsNum() const{
    std::istringstream iss(_text);
    int n;
    iss >> n;
    return n;
}
