#include <SDL.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#undef main
int main(int argc, char *argv[]) {
  SDL_Init(SDL_INIT_VIDEO);

  auto window =
      SDL_CreateWindow("Hellas Editor", 100, 100, 800, 600, SDL_WINDOW_SHOWN);
  auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  while (true)
    ;

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();
  return 0;
}
