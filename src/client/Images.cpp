#include "Images.h"

Texture Images::defaultTexture;

void Images::initialise(const std::string &directory) {
  _directory = directory;

  if (!defaultTexture) createDefaultTexture();
}

const Texture &Images::operator[](const std::string key) {
  auto it = _container.find(key);

  // Image exists: return it
  if (it != _container.end()) return it->second;

  // Image doesn't exist: load it
  auto tex = Texture{_directory + "/" + key + ".png"};
  if (tex) {
    _container[key] = tex;
    return _container[key];
  }

  // File doesn't exist: return default
  return defaultTexture;
}

void Images::createDefaultTexture() {
  defaultTexture = {16, 16};

  renderer.pushRenderTarget(defaultTexture);

  renderer.setDrawColor(Color::MAGENTA);
  renderer.fill();

  renderer.popRenderTarget();
}
