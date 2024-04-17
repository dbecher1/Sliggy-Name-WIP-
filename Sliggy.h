//
// Created by Daniel Becher on 3/19/24.
//

#ifndef SLIPPRY_H
#define SLIPPRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL_render.h>

#define SL_FLAGS_MANAGE_MEMORY 0b10

typedef struct SL_UIE_INNER_ SL_UIElement;
typedef struct SL_UIEB_INNER_ SL_UIElementBuilder;

// Builder

SL_UIElementBuilder* SL_CreateBuilder(SDL_Texture* skin);

void SL_BuilderSetDimensionsAbsolute(SL_UIElementBuilder* builder, const int* x, const int* y, const int* w, const int* h);
void SL_BuilderSetDimensionsRelative(SL_UIElementBuilder* builder, const float* x, const float* y, const float* w, const float* h);
void SL_BuilderSetName(SL_UIElementBuilder* builder, const char* path);
void SL_BuilderSetFont(SL_UIElementBuilder* builder, SDL_Texture* tex, const char* name);
void SL_BuilderSetActive(SL_UIElementBuilder* builder, int active);
void SL_BuilderSetTexture(SL_UIElementBuilder* builder, SDL_Texture* texture);
void SL_BuilderAddTextObject(SL_UIElementBuilder *builder, const char *text, int x_, int y_, float size, const char *id_);

// Element/Core

void SL_Init(SDL_Renderer* renderer, int screen_width_, int screen_height_, int flags);
void SL_Quit();

SL_UIElement* SL_CreateElement(SL_UIElementBuilder** builder_);
void SL_FreeElement(SL_UIElement* element);

SL_UIElement* getElementFromMap(const char* name);

void SL_DrawElement(const SL_UIElement* element);

int SL_ElementIsActive(const SL_UIElement* element);
void SL_ActivateElement(SL_UIElement* element);
void SL_DeactivateElement(SL_UIElement* element);

#ifdef __cplusplus
}
#endif

#endif //SLIPPRY_H
