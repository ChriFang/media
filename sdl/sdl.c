#include "SDL2/SDL.h"

int main(int argc, char* argv[])
{
  if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
  {
    return -1;
  }

  SDL_Window* win = SDL_CreateWindow("Hello SDL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_ALLOW_HIGHDPI);
  if (NULL == win)
  {
    printf("SDL_CreateWindow failed!\n");
    return -2;
  }

  SDL_Event evt;
  while (1)
  {
    if (SDL_PollEvent(&evt))
    {
      if (SDL_QUIT == evt.type)
      {
        printf("SDL quit!\n");
        break;
      }
    }
  }

  SDL_DestroyWindow(win);

  SDL_Quit();

  return 0;
}
