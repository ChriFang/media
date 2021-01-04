#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

void renderTexture(SDL_Texture *tex, SDL_Renderer *ren)
{
  SDL_Rect dst;
  dst.x = 0;
  dst.y = 0;
  SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
  SDL_RenderCopy(ren, tex, NULL, &dst);
}

int main(int argc, char* argv[])
{
  if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
  {
    return -1;
  }

  SDL_Window* win = SDL_CreateWindow("Show Image", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_ALLOW_HIGHDPI);
  if (NULL == win)
  {
    printf("SDL_CreateWindow failed!\n");
    return -2;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (NULL == renderer)
  {
    printf("SDL_CreateRenderer failed!\n");
    SDL_DestroyWindow(win);
    SDL_Quit();
    return -3;
  }

  SDL_Texture *image = IMG_LoadTexture(renderer, "res/image.jpg");
  if (NULL == image)
  {
    printf("IMG_LoadTexture failed!\n");
    SDL_DestroyWindow(win);
    SDL_Quit();
    return -4;
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
    SDL_RenderClear(renderer);
    renderTexture(image, renderer);
    SDL_RenderPresent(renderer);
  }

  IMG_Quit();

  SDL_DestroyWindow(win);

  SDL_Quit();

  return 0;
}
