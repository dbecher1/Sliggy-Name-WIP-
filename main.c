#include <stdio.h>
#include "SDL.h"
#include "Slippry.h"

int main(int argc, char** argv) {

    // This is a test

    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 720, 480, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Event e;

    SL_Init(renderer, NULL, 720, 480, 0);

    SL_UIElementBuilder* b = SL_CreateBuilder();
    const float temp = 0.5f;
    const float temp2 = 0.25f;
    SL_BuilderSetDimensions(b, &temp2, &temp, &temp, &temp2);
    SL_BuilderSetCurve(b, 0.03f);
    SL_BuilderSetName(b, "Hi");
    SL_UIElement* ele = SL_CreateElement(&b);

    int quit = 0;

    while (!quit) {
        while (SDL_PollEvent(&e) > 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        SDL_RenderClear(renderer);

        // Render
        SL_DrawElement(ele);

        SDL_RenderPresent(renderer);
    }

    SL_FreeElement(ele);
    SL_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
