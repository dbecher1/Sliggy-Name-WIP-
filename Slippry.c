//
// Created by Daniel Becher on 3/19/24.
//

#include "SDL_render.h"
#include "Slippry.h"
#include <stdlib.h>

// Inner flags
#define SL_INNERFLAG_BOOL_ACTIVE 0b1
#define SL_INNERFLAG_BOOL_BORDER 0b10
#define SL_INNERFLAG_BOOL_AA 0b100

// Other constants
#define INIT_ARRAY_SIZE 128
#define DEFAULT_DRAW_LAYERS 3
#define NUM_RECTS 3
#define NUM_CURVED_EDGES 4
#define NUM_OUTLINE_POINTS 8

typedef struct SL_Rect {
    short x, y, w, h;
} SL_Rect;

typedef struct SL_Point {
    short x, y;
} SL_Point;

struct SL_UIEB_INNER_ {
    const char* name;
    float x, y, w, h; // Expected as screen coordinates, 0-1
    float curve;
    int is_active;
    // SL_UIElement* parent = NULL;
    int border;
    int draw_layer;
    int anti_alias;
    SDL_Color color;
};

struct SL_UIE_INNER_ {
    SDL_Color color;
    SDL_Rect raw_rect;
    SDL_Rect rects[NUM_RECTS];
    SDL_Point* curved_edges[NUM_CURVED_EDGES];
    SDL_Point outline_straight[NUM_OUTLINE_POINTS];
    SDL_Point* outline_curves;
    short curved_edge_counts[NUM_CURVED_EDGES];
    int draw_layer;
    const char* name;
    short outline_curve_count;
    unsigned char bool_flags;

    SDL_Vertex* test_vertices;
    int test_vertices_count;
    int* test_indices;
    int test_indices_count;
};

typedef enum SL_CircleCorner {
    SL_CC_UPPER_LEFT = 0,
    SL_CC_UPPER_RIGHT,
    SL_CC_LOWER_LEFT,
    SL_CC_LOWER_RIGHT
} SL_CircleCorner;

// Forward declarations of private functions

static void GenerateCircle(SDL_Point** points, short* count, int radius, int origin_x, int origin_y, SL_CircleCorner corner);
static void addAndCheckBounds(SDL_Point p, SDL_Point** points, short* count, int* bounds);
static void convertPointArrayToVertices(SDL_Point** points, int point_count, SDL_Vertex** vertices, int* vertex_count, int** indices, int* index_count);

// Static members

static int draw_layer_count;
static int width;
static int height;
static SDL_Texture** drawlayers;
static SDL_Renderer* render_context;

// Builder function definitions

SL_UIElementBuilder* SL_CreateBuilder() {

    SL_UIElementBuilder* ptr = malloc(sizeof(SL_UIElementBuilder));

    ptr->x = 0;
    ptr->y = 0;
    ptr->w = 0;
    ptr->h = 0;
    ptr->curve = 0.03f;
    ptr->is_active = 0;
    ptr->border = 1;
    ptr->draw_layer = 0;
    ptr->anti_alias = 0;
    ptr->color.a = 0xFF;
    ptr->color.r = 0x00;
    ptr->color.g = 0x00;
    ptr->color.b = 0xAC;

    return ptr;
}

void SL_BuilderSetDimensions(SL_UIElementBuilder* builder, const float* x, const float* y, const float* w, const float* h) {
    if (x != NULL)
        builder->x = *x;
    if (y != NULL)
        builder->y = *y;
    if (w != NULL)
        builder->w = *w;
    if (h != NULL)
        builder->h = *h;
}

void SL_BuilderSetName(SL_UIElementBuilder* builder, const char* name) {
    builder->name = name;
}

void SL_BuilderSetCurve(SL_UIElementBuilder* builder, const float curve) {
    builder->curve = curve;
}

void SL_BuilderSetActive(SL_UIElementBuilder* builder, const int active) {
    builder->is_active = active;
}

void SL_BuilderSetBorder(SL_UIElementBuilder* builder, const int border) {
    builder->border = border;
}

void SL_BuilderSetDrawLayer(SL_UIElementBuilder* builder, const int draw_layer) {
    builder->draw_layer = draw_layer;
}

void SL_BuilderSetAA(SL_UIElementBuilder* builder, const int aa) {
    builder->anti_alias = aa;
}

void SL_BuilderSetColor(SL_UIElementBuilder* builder, const SDL_Color color) {
    builder->color = color;
}

// Core / element definitions

void SL_Init(SDL_Renderer* renderer, const int* num_draw_layers, const int w_, const int h_, int flags) {
    render_context = renderer;

    if (num_draw_layers != NULL)
        draw_layer_count = *num_draw_layers;
    else
        draw_layer_count = DEFAULT_DRAW_LAYERS;

    width = w_;
    height = h_;
    // TODO flags

    drawlayers = calloc(draw_layer_count, sizeof(SDL_Texture*));

    for (int i = 0; i < draw_layer_count; i++) {
        drawlayers[i] = SDL_CreateTexture(render_context, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    }
}

void SL_Quit() {
    for (int i = 0; i < draw_layer_count; i++) {
        SDL_DestroyTexture(drawlayers[i]);
    }
    free(drawlayers);
}

/**
 * 
 * @param builder_ 
 * @return 
 */
SL_UIElement* SL_CreateElement(SL_UIElementBuilder** builder_) {
    const SL_UIElementBuilder builder = **builder_;

    SL_UIElement* ptr = malloc(sizeof(SL_UIElement));

    ptr->name = builder.name;
    ptr->raw_rect.x = (int)(builder.x * width);
    ptr->raw_rect.y = (int)(builder.y * height);
    ptr->raw_rect.w = (int)(builder.w * width);
    ptr->raw_rect.h = (int)(builder.h * height);
    ptr->draw_layer = builder.draw_layer;
    ptr->color = builder.color;
    ptr->bool_flags = 0;
    for (int i = 0; i < 4; i++) {
        ptr->curved_edge_counts[i] = 0;
    }
    ptr->outline_curve_count = 0;

    if (builder.is_active)
        ptr->bool_flags |= SL_INNERFLAG_BOOL_ACTIVE;

    if (builder.border)
        ptr->bool_flags |= SL_INNERFLAG_BOOL_BORDER;

    if (builder.anti_alias)
        ptr->bool_flags |= SL_INNERFLAG_BOOL_AA;

    const int radius = (int)(builder.curve * width);

    SDL_Rect r = ptr->raw_rect;

    for (int i = 0; i < NUM_CURVED_EDGES; i++) {
        const int x_ = r.x + ((i % 2) * r.w);
        const int y_ = r.y + ((i / 2) * r.h);
        ptr->curved_edges[i] = calloc(INIT_ARRAY_SIZE, sizeof(SDL_Point));
        GenerateCircle(&ptr->curved_edges[i], &ptr->curved_edge_counts[i], radius, x_, y_, i);

        for (int j = 0; j < ptr->curved_edge_counts[i]; j++) {
            // printf("%d %d\n", ptr->curved_edges[i][j].x, ptr->curved_edges[i][j].y);
        }
        // printf("Number of points: %d\n", ptr->curved_edge_counts[i]);
    }

    SDL_Rect left_side = r;
    left_side.x += 1;
    left_side.y += radius;
    left_side.w = radius;
    left_side.h -= radius * 2;

    SDL_Rect right_side = left_side;
    right_side.x += r.w - radius;
    right_side.w -= 2;
    // y / h is the same

    r.x += radius;
    r.y += 1;
    r.w -= (radius * 2) - 2;
    r.h -= 2;
    // SDL_Rect inner_rect = r; // Currently unused
    ptr->rects[0] = r;
    ptr->rects[1] = left_side;
    ptr->rects[2] = right_side;

    SDL_Point p1 = {r.x, r.y};
    SDL_Point p2 = {r.x + r.w, r.y};
    SDL_Point p3 = {r.x, r.y + r.h};
    SDL_Point p4 = {r.x + r.w, r.y + r.h};
    SDL_Point p5 = {left_side.x, left_side.y};
    SDL_Point p6 = {left_side.x, left_side.y + left_side.h};
    SDL_Point p7 = {right_side.x + right_side.w, right_side.y};
    SDL_Point p8 = {right_side.x + right_side.w, right_side.y + right_side.h};

    int _ = 0;

    ptr->outline_straight[_++] = p1;
    ptr->outline_straight[_++] = p2;
    ptr->outline_straight[_++] = p3;
    ptr->outline_straight[_++] = p4;
    ptr->outline_straight[_++] = p5;
    ptr->outline_straight[_++] = p6;
    ptr->outline_straight[_++] = p7;
    ptr->outline_straight[_++] = p8;

    for (int i = 0; i < ptr->curved_edge_counts[0]; i++) {
        const SDL_Point p = ptr->curved_edges[0][i];
        if (p.x <= r.x && p.y <= left_side.y) {
            ptr->outline_curve_count++;
        }
    }
    for (int i = 0; i < ptr->curved_edge_counts[2]; i++) {
        const SDL_Point p = ptr->curved_edges[2][i];
        if (p.x <= r.x && p.y >= left_side.y + left_side.h) {
            ptr->outline_curve_count++;
        }
    }

    for (int i = 0; i < ptr->curved_edge_counts[1]; i++) {
        const SDL_Point p = ptr->curved_edges[1][i];
        if (p.x >= right_side.x && p.y <= right_side.y) {
            ptr->outline_curve_count++;
        }
    }
    for (int i = 0; i < ptr->curved_edge_counts[3]; i++) {
        const SDL_Point p = ptr->curved_edges[3][i];
        if (p.x >= right_side.x && p.y >= right_side.y + right_side.h) {
            ptr->outline_curve_count++;
        }
    }
    ptr->outline_curves = calloc(ptr->outline_curve_count, sizeof(SDL_Point));

    _ = 0;
    int pts = 0;
    for (int i = 0; i < 4; i++) {
        pts += ptr->curved_edge_counts[i];
    }

    for (int i = 0; i < ptr->curved_edge_counts[0]; i++) {
        const SDL_Point p = ptr->curved_edges[0][i];
        if (p.x <= r.x && p.y <= left_side.y) {
            ptr->outline_curves[_++] = p;
        }
    }
    for (int i = 0; i < ptr->curved_edge_counts[2]; i++) {
        const SDL_Point p = ptr->curved_edges[2][i];
        if (p.x <= r.x && p.y >= left_side.y + left_side.h) {
            ptr->outline_curves[_++] = p;
        }
    }

    for (int i = 0; i < ptr->curved_edge_counts[1]; i++) {
        const SDL_Point p = ptr->curved_edges[1][i];
        if (p.x >= right_side.x && p.y <= right_side.y) {
            ptr->outline_curves[_++] = p;
        }
    }
    for (int i = 0; i < ptr->curved_edge_counts[3]; i++) {
        const SDL_Point p = ptr->curved_edges[3][i];
        if (p.x >= right_side.x && p.y >= right_side.y + right_side.h) {
            ptr->outline_curves[_++] = p;
        }
    }

    convertPointArrayToVertices((SDL_Point**)&ptr->curved_edges, pts, &ptr->test_vertices, &ptr->test_vertices_count, &ptr->test_indices, &ptr->test_indices_count);

    free(*builder_);

    return ptr;
}
void SL_FreeElement(SL_UIElement* element) {
    for (int i = 0; i < 4; i++) {
        free(element->curved_edges[i]);
    }
    free(element->outline_curves);
    free(element);
}

void SL_DrawElement(const SL_UIElement* element) {
    SDL_SetRenderDrawColor(render_context, 0xFF, 0xFF, 0xFF, 0xFF);

    SDL_RenderFillRects(render_context, element->rects, 3);

    for (int i = 0; i < NUM_CURVED_EDGES; i++) {
        SDL_RenderDrawLines(render_context, element->curved_edges[i], element->curved_edge_counts[i]);
    }

    if (element->bool_flags & SL_INNERFLAG_BOOL_BORDER) {
        SDL_SetRenderDrawColor(render_context, 0, 0, 0, 0xFF);

        for (int i = 0; i < NUM_OUTLINE_POINTS - 1; i += 2) {
            SDL_RenderDrawLine(
                render_context,
                element->outline_straight[i].x,
                element->outline_straight[i].y,
                element->outline_straight[i + 1].x,
                element->outline_straight[i + 1].y
                );
        }
        // SDL_RenderDrawPoints(render_context, element->outline_curves, element->outline_curve_count);
    }

    SDL_SetRenderDrawColor(render_context, 0, 0, 0xFF, 0xFF);
}

// Needs to be called before RenderPresent
void SL_SubmitDraw() {

}

int SL_ElementIsActive(const SL_UIElement* element) {
    return element->bool_flags & SL_INNERFLAG_BOOL_ACTIVE ? 1 : 0;
}

void SL_ActivateElement(SL_UIElement* element) {
    element->bool_flags |= SL_INNERFLAG_BOOL_ACTIVE;
}

void SL_DeactivateElement(SL_UIElement* element) {
    element->bool_flags &= ~SL_INNERFLAG_BOOL_ACTIVE;
}

static void GenerateCircle(SDL_Point** points, short* count, int radius, int origin_x, int origin_y, SL_CircleCorner corner) {

    int bound = INIT_ARRAY_SIZE;

    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius * 2);

    int x_offset = radius, y_offset = radius;
    // 0 - upper left (default)
    // 1 - upper right
    // 2 - lower left
    // 3 - lower right
    switch (corner) {
        case 1: {
            x_offset *= -1;
            break;
        }
        case 2: {
            y_offset *= -1;
            break;
        }
        case 3: {
            x_offset *= -1;
            y_offset *= -1;
            break;
        }
        default: break;
    }
    origin_x += x_offset;
    origin_y += y_offset;

    while (x >= y) {

        const SDL_Point p1 = {origin_x + x, origin_y + y};
        const SDL_Point p2 = {origin_x + x, origin_y - y};
        const SDL_Point p3 = {origin_x + y, origin_y + x};
        const SDL_Point p4 = {origin_x + y, origin_y - x};

        const SDL_Point p5 = {origin_x - x, origin_y + y};
        const SDL_Point p6 = {origin_x - x, origin_y - y};
        const SDL_Point p7 = {origin_x - y, origin_y + x};
        const SDL_Point p8 = {origin_x - y, origin_y - x};

        addAndCheckBounds(p1, points, count, &bound);
        addAndCheckBounds(p2, points, count, &bound);
        addAndCheckBounds(p3, points, count, &bound);
        addAndCheckBounds(p4, points, count, &bound);

        addAndCheckBounds(p5, points, count, &bound);
        addAndCheckBounds(p6, points, count, &bound);
        addAndCheckBounds(p7, points, count, &bound);
        addAndCheckBounds(p8, points, count, &bound);

        if (err <= 0) {
            y++;
            err += dy;
            dy += 2;
        }
        if (err > 0) {
            x--;
            dx += 2;
            err += dx - (radius * 2);
        }
    }
    // final resize
    *points = realloc(*points, *count * sizeof(SDL_Point));
}

static void addAndCheckBounds(const SDL_Point p, SDL_Point** points, short* count, int* bounds) {

    SDL_Point* arr = *points;

    // check for duplicates
    if (*count > 0 && arr[*count - 1].x == p.x && arr[*count - 1].y == p.y)
        return;

    arr[*count] = p;
    *count += 1;
    if (*count >= *bounds) {
        *bounds *= 2;
        *points = realloc(*points, *bounds * sizeof(SDL_Point));
    }
}

/**
 * Converts (and consumes) an array of SDL Points into SDL_Vertex structs
 * @param points A pointer to an array of SDL_Points. Consumes the array, WILL BE DEALLOCATED.
 * @param count The size of the array
 * @return A corresponding array of SDL_Vertex structs
 */
static void convertPointArrayToVertices(SDL_Point** points, int point_count, SDL_Vertex** vertices, int* vertex_count, int** indices, int* index_count) {

    *vertices = calloc(INIT_ARRAY_SIZE, sizeof(SDL_Vertex));
    *indices = calloc(INIT_ARRAY_SIZE, sizeof(int));

    *vertex_count = 0;
    *index_count = 0;

    SDL_Point* point_arr_ptr = *points;
    SDL_Vertex* vert_arr_ptr = *vertices;
    int* idx_arr_ptr = *indices;

    int limit_v = INIT_ARRAY_SIZE;
    int limit_i = INIT_ARRAY_SIZE;

    for (int i = 0; i < point_count; i++) {

        for (int k = 0; k < 6; k++) {
            // Convert points into quads
            // 0 1 2 3 4 5

            int found = -1;
            float dx = (float)((k % 3) / 2);
            float dy = dx;
            if (k <= 2)
                dy += k % 2;
            else
                dx += k % 2;
            //float dy = (float)(((k % 3) / 2) + (k % 2));

            // First, search the vertex array for the point

            for (int j = 0; j < *vertex_count; j++) {
                const SDL_FPoint fp = {(float)point_arr_ptr[i].x + dx, (float)point_arr_ptr[i].y + dy};
                if (fp.x == vert_arr_ptr[j].position.x && fp.y == vert_arr_ptr[j].position.y) {
                    found = j;
                    break;
                }
            }
            if (found >= 0) {
                // Point is in array, register index
                //*indices[*index_count++] = found;
                idx_arr_ptr[*index_count] = found;
                *index_count += 1;
            }
            else {
                const SDL_Point p = point_arr_ptr[i];

                // Create new point
                const SDL_Vertex v = {
                    .position = {p.x, p.y},
                    .color = {0, 0, 0},
                    .tex_coord = {0.0f, 0.0f}
                };

                //*vertices[*vertex_count] = v;
                vert_arr_ptr[*vertex_count] = v;
                //*indices[*index_count] = *vertex_count;
                idx_arr_ptr[*index_count] = *vertex_count;

                *vertex_count += 1;
                *index_count += 1;
            }

            // check bounds (8 is a buffer for safety)

            if (*vertex_count >= (limit_v - 8)) {
                limit_v *= 2;
                SDL_Vertex* temp_ptr = realloc(*vertices, limit_v * sizeof(SDL_Vertex));
                if (temp_ptr) {
                    *vertices = temp_ptr;
                } else {
                    printf("!!!!!ERROR!!!! VOID PTR AHH (VERTEX)\n");
                }

            }

            if (*index_count >= (limit_i - 8)) {
                limit_i *= 2;
                int* temp_ptr = realloc(*indices, limit_i * sizeof(int));
                if (temp_ptr) {
                    *indices = temp_ptr;
                } else {
                    printf("!!!!!ERROR!!!! VOID PTR AHH (INDEX)\n");
                }
            }
        }
    }


}