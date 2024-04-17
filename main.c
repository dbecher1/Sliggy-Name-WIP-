
#include "SDL.h"
#include "SDL2/SDL_image.h"
#include "Sliggy.h"

static unsigned long long count = 0;

int main(int argc, char** argv) {

    // This is a test

    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);
    SDL_Window* window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 720, 480, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Event e;

    int flags = SL_FLAGS_MANAGE_MEMORY;
    //int flags = 0;
    //flags = SL_INIT_FLAG_EXPERIMENTS;

    SL_Init(renderer, 720, 480, flags);

    SDL_Texture* t = IMG_LoadTexture(renderer, "../bad_aa_9.png");
    SDL_Texture* f = IMG_LoadTexture(renderer, "../Font2.png");
    SL_UIElementBuilder* b = SL_CreateBuilder(t);
    const float temp = 0.5f;
    const float temp2 = 0.25f;
    SL_BuilderSetDimensionsRelative(b, &temp2, &temp, &temp, &temp2);
    SL_BuilderSetName(b, "Hi");
    // SL_CreateElement(&b);

    SL_UIElementBuilder* b2 = SL_CreateBuilder(t);
    const int temp4 = 300;
    const int temp5 = 50;
    SL_BuilderSetDimensionsAbsolute(b2, &temp5, &temp5, &temp4, &temp4);
    SL_BuilderSetName(b2, "Mom?");
    SL_BuilderSetFont(b2, f, "../Font2.fnt");

    const char* test = "Hello World! Hello! World? I love my mom and my dad and peaces and mangos and yu yum yum yum I love you too";
    float desired_size = 16;
    SL_BuilderAddTextObject(b2, test, 8, 16, desired_size, "text1");

    SL_CreateElement(&b2);

    // SL_UIElement* ele = getElementFromMap("Hi");
    SL_UIElement* ele2 = getElementFromMap("Mom?");
    // SL_DeactivateElement(ele);

    int quit = 0;

    while (!quit) {
        while (SDL_PollEvent(&e) > 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        Uint8 color = 100;
        SDL_SetRenderDrawColor(renderer, color, color, color, 0xFF);
        SDL_RenderClear(renderer);

        // Render
        // SL_DrawElement(ele);
        SL_DrawElement(ele2);

        SDL_RenderPresent(renderer);
    }

    // SL_FreeElement(ele);
    SL_FreeElement(ele2);
    SL_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
