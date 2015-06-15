#ifndef RENDERER_H
#define RENDERER_H

#include <SDL.h>

#include "Color.h"

// Wrapper class for SDL_Renderer and SDL_Window, plus related convenience functions.
class Renderer{
    SDL_Renderer *_renderer;
    SDL_Window *_window;
    int _w, _h;
    bool _valid; // Whether everything has been properly initialized
    static size_t _count;

public:
    Renderer();
    ~Renderer();

    /*
    Some construction takes place here, to be called manually, as a Renderer may be static.
    Specifically, this creates the window and renderer objects, and requires that cmdLineArgs has been populated.
    */
    void init();

    inline operator bool() const { return _valid; }

    inline int width() const { return _w; }
    inline int height() const { return _h; }

    SDL_Texture *createTextureFromSurface(SDL_Surface *surface) const;
    void drawTexture(SDL_Texture *srcTex, const SDL_Rect &dstRect);

    void setDrawColor(const Color &color = Color::BLACK);
    void clear();
    void present();

    void drawRect(const SDL_Rect &dstRect);
    void fillRect(const SDL_Rect &dstRect);
};

#endif
