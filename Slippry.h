//
// Created by Daniel Becher on 3/19/24.
//

#ifndef SLIPPRY_H
#define SLIPPRY_H

#include <SDL_render.h>

// #define SL_INIT_FLAG_

typedef struct SL_UIE_INNER_ SL_UIElement;
typedef struct SL_UIEB_INNER_ SL_UIElementBuilder;

// Builder

SL_UIElementBuilder* SL_CreateBuilder();
void SL_BuilderSetDimensions(SL_UIElementBuilder* builder, const float* x, const float* y, const float* w, const float* h);
void SL_BuilderSetName(SL_UIElementBuilder* builder, const char* name);
void SL_BuilderSetCurve(SL_UIElementBuilder* builder, float curve);
void SL_BuilderSetActive(SL_UIElementBuilder* builder, int active);
void SL_BuilderSetBorder(SL_UIElementBuilder* builder, int border);
void SL_BuilderSetDrawLayer(SL_UIElementBuilder* builder, int draw_layer);
void SL_BuilderSetAA(SL_UIElementBuilder* builder, int aa);
void SL_BuilderSetColor(SL_UIElementBuilder* builder, SDL_Color color);

// Element/Core

void SL_Init(SDL_Renderer* renderer, const int* num_draw_layers, int w_, int h_, int flags);
void SL_Quit();

SL_UIElement* SL_CreateElement(SL_UIElementBuilder** builder_);
void SL_FreeElement(SL_UIElement* element);

void SL_DrawElement(const SL_UIElement* element);
void SL_SubmitDraw(); // Needs to be called before RenderPresent

int SL_ElementIsActive(const SL_UIElement* element);
void SL_ActivateElement(SL_UIElement* element);
void SL_DeactivateElement(SL_UIElement* element);

#endif //SLIPPRY_H
