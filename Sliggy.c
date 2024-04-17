//
// Created by Daniel Becher on 3/19/24.
//

#include "Sliggy.h"
#include <stdlib.h>
#include <stdio.h>

// Inner flags
#define SL_INNERFLAG_ACTIVE 0b1
#define SL_INNERFLAG_ABSOLUTE 0b1000

// Other ~fun~ macros

#define NUM_INDICES 54
#define NUM_VERTICES 16
#define MAP_INIT 32
#define MAX_TEXT_OBJS 16 // revisit this?

#define WHITE { 0xFF, 0xFF, 0xFF, 0xFF }
#define RED { 0xFF, 0x00, 0x00, 0xFF }
#define YELLOW { 0xFF, 0xFF, 0x00, 0xFF }

struct SL_UIEB_INNER_;
struct SL_UIE_INNER_;
struct SL_Glyph;
struct SL_TextObject;
struct SL_TextObjBuilder;

struct SL_UIEB_INNER_ {
    const char* name;
    float x, y, w, h; // Expected as screen coordinates, 0-1
    int xab, yab, wab, hab; // absolute values
    int flags;
    SDL_Texture* skin;
    SDL_Texture* font;
    struct SL_TextObjectBuilder* text_builders;
    int num_text_objects;
};

struct SL_UIE_INNER_ {
    const char* name;
    SDL_Texture* texture_skin;
    int skin_step_x;
    int skin_step_y;

    SDL_Rect src_rect;
    unsigned short flags;
    SDL_Texture* font;

    struct SL_TextObject* TextObjectMap;
    int* TextObjectIterator;
    int textMapLimit;
    int textMapCount;
};

typedef struct SL_Glyph {
    SDL_Rect src;
    char x_offset;
    char y_offset;
    unsigned char x_advance;
    char raw_char;
    float u_min;
    float u_max;
    float v_min;
    float v_max;
} SL_Glyph;

typedef struct SL_TextObjectBuilder {
    const char* id;
    const char* text;
    int x;
    int y;
    float size;
} SL_TextObjBuilder;

typedef struct SL_TextObject {
    const char* id;
    SL_Glyph* text;
    int* word_widths; // width of each word respectively - used for word wrapping
    short length; // length in characters
    short num_words; // # of words separated by whitespace - includes punctuation
    float scale; // precalculated scale factor - desired size of the text / font pt size
    SDL_Color c;
    int x_start;
    int y_start;
} SL_TextObject;

typedef enum HashMapType {
    HASHMAP_TYPE_UI_ELEMENT,
    HASHMAP_TYPE_TEXT_ELEMENT
} HashMapType;

// Font stuff
static int font_start = 0;
static int font_count;
static SL_Glyph* Glyphs = NULL;
static float font_size;
static float font_tex_width;
static float font_tex_height;
static int lineHeight;

static SL_TextObject
CreateTextObject(const char *raw_text, float desired_size, int x_, int y_);
static void DestroyTextObject(SL_TextObject* ptr);

// Static members

static int screen_width;
static int screen_height;
static int global_flags = 0;
static SDL_Renderer* render_context;

// Element hash map
static int mapLimit;
static int mapCount;
static SL_UIElement* ElementHashMap;

// some helpers for hash stuff... weird pointer crap
static void *getVoidPtrOffset(void *ptr, HashMapType type, int idx);
static int itemAtAddressExists(void* ptr);


static void *addItemToMap(void *map_, const char *name, HashMapType type, int *count, int *limit, int *index);
void *getItemFromMap(void *map_, const char *name, const int *limit, enum HashMapType type);
static int hashName(const char* name);

const static SDL_Color Color_White = WHITE;
const static SDL_Color Color_Red = RED;

const static int indices[NUM_INDICES] = {
    0, 4, 5, 0, 1, 5, // Upper left
    1, 5, 6, 1, 2, 6, // Upper mid
    2, 6, 7, 2, 3, 7, // Upper right

    4, 5, 9, 4, 8, 9, // Mid left
    5, 6, 10, 5, 9, 10, // Mid mid
    6, 7, 11, 6, 10, 11, // Mid right

    8, 9, 13, 8, 12, 13, // Lower left
    9, 10, 14, 9, 13, 14, // Lower mid
    10, 11, 15, 10, 14, 15 // Lower right
};

// Builder function definitions

SL_UIElementBuilder* SL_CreateBuilder(SDL_Texture* skin) {

    SL_UIElementBuilder* ptr = malloc(sizeof(SL_UIElementBuilder));

    ptr->x = 0;
    ptr->y = 0;
    ptr->w = 0;
    ptr->h = 0;
    ptr->xab = 0;
    ptr->yab = 0;
    ptr->wab = 0;
    ptr->hab = 0;
    ptr->num_text_objects = 0;

    if (skin != NULL) {
        ptr->skin = skin;
    }
    else {
        ptr->skin = NULL;
    }

    ptr->flags = SL_INNERFLAG_ACTIVE;
    ptr->text_builders = calloc(MAX_TEXT_OBJS, sizeof(SL_TextObjBuilder));

    return ptr;
}

void SL_BuilderAddTextObject(SL_UIElementBuilder *builder, const char *text, int x_, int y_, float size, const char *id_) {
    const char* id;
    if (!id_) {
        id = text;
    }
    else {
        id = id_;
    }
    SL_TextObjBuilder b = {
            .id = id,
            .size = size,
            .text = text,
            .x = x_,
            .y = y_
    };
    builder->text_builders[builder->num_text_objects++] = b;
}

void SL_BuilderSetDimensionsAbsolute(SL_UIElementBuilder* builder, const int* x, const int* y, const int* w, const int* h) {
    if (x != NULL)
        builder->xab = *x;
    if (y != NULL)
        builder->yab = *y;
    if (w != NULL)
        builder->wab = *w;
    if (h != NULL)
        builder->hab = *h;
    builder->flags |= SL_INNERFLAG_ABSOLUTE;
};

void SL_BuilderSetDimensionsRelative(SL_UIElementBuilder* builder, const float* x, const float* y, const float* w, const float* h) {
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

void SL_BuilderSetTexture(SL_UIElementBuilder* builder, SDL_Texture* texture) {
    builder-> skin = texture;
}

void SL_BuilderSetActive(SL_UIElementBuilder* builder, int active) {
    if (active) {
        builder->flags |= SL_INNERFLAG_ACTIVE;
    }
    else {
        builder->flags &= ~SL_INNERFLAG_ACTIVE;
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err34-c"
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
// Only supports one font stored statically right now
void SL_BuilderSetFont(SL_UIElementBuilder* builder, SDL_Texture* tex, const char* path) {
    FILE* file = fopen(path, "r");

    char* str = malloc(128);

    while (fgets(str, 128, file)) {
        // printf("%s\n", str);
        char* c = strtok(str, " ");
        if (strcmp(c, "chars") == 0) {
            c = strtok(NULL, " ");
            c = strtok(c, "=");
            c = strtok(NULL, "=");
            font_count = atoi(c);
            // printf("Font count: %d\n", font_count);
            Glyphs = calloc(font_count, sizeof(SL_Glyph));
            continue;
        }
        else if (strcmp(c, "info") == 0) {
            c = strtok(NULL, " =");
            do {
                // THIS BREAKS IF THE FONT NAME HAS A SPACE
                char* key = c;
                char* val = strtok(NULL, " =");
                if (strcmp(key, "size") == 0) {
                    font_size = atof(val);
                }
            } while ((c = strtok(NULL, " =")));
        }
        else if (strcmp(c, "common") == 0) {
            c = strtok(NULL, " =");
            do {
                char* key = c;
                char* val = strtok(NULL, " =");
                if (strcmp(key, "scaleW") == 0) {
                    font_tex_width = atof(val);
                }
                else if (strcmp(key, "scaleH") == 0) {
                    font_tex_height = atof(val);
                }
                else if (strcmp(key, "lineHeight") == 0) {
                    lineHeight = atoi(val);
                }
            } while ((c = strtok(NULL, " =")));
        }
        else if (strcmp(c, "char") == 0) {
            int id;
            int x;
            int y;
            int w;
            int h;
            char xoff;
            char yoff;
            unsigned char xadv;
            char raw;

            c = strtok(NULL, " =");
            do {
                char* key = c;
                char* val = strtok(NULL, " =");
                // printf("Key: %s Val: %s\n", key, val);
                if (strcmp(key, "id") == 0) {
                    id = atoi(val);
                    raw = (char)id;
                    if (!font_start) {
                        font_start = id;
                    }
                }
                else if (strcmp(key, "x") == 0) {
                    x = atoi(val);
                }
                else if (strcmp(key, "y") == 0) {
                    y = atoi(val);
                }
                else if (strcmp(key, "width") == 0) {
                    w = atoi(val);
                }
                else if (strcmp(key, "height") == 0) {
                    h = atoi(val);
                }
                else if (strcmp(key, "xoffset") == 0) {
                    xoff = atoi(val);
                }
                else if (strcmp(key, "yoffset") == 0) {
                    yoff = atoi(val);
                }
                else if (strcmp(key, "xadvance") == 0) {
                    xadv = atoi(val);
                }
            } while ((c = strtok(NULL, " =")));
            SDL_Rect r = {x, y, w, h};
            SL_Glyph g = {r, xoff, yoff, xadv};

            // TODO test UV
            g.u_min = (float)x / font_tex_width;
            g.u_max = (float)(x + w) / font_tex_width;
            g.v_min = (float)(y + h) / font_tex_height;
            g.v_max = (float)y / font_tex_height;
            g.raw_char = raw;
            Glyphs[id - font_start] = g;
        }
    }

    builder->font = tex;

    free(str);
    fclose(file);

}
#pragma clang diagnostic pop

// Core / element definitions

void SL_Init(SDL_Renderer* renderer, int screen_width_, int screen_height_, int flags) {
    render_context = renderer;
    screen_width = screen_width_;
    screen_height = screen_height_;
    global_flags = flags;

    if ((flags & SL_FLAGS_MANAGE_MEMORY) == SL_FLAGS_MANAGE_MEMORY) {

        ElementHashMap = calloc(MAP_INIT, sizeof(SL_UIElement));

        mapCount = 0;
        mapLimit = MAP_INIT;
    }
}


void SL_Quit() {
    if ((global_flags & SL_FLAGS_MANAGE_MEMORY) == SL_FLAGS_MANAGE_MEMORY) {
        free(ElementHashMap);
    }
    if (Glyphs != NULL) {
        free(Glyphs);
    }
}

/**
 * 
 * @param
 * @return 
 */
SL_UIElement* SL_CreateElement(SL_UIElementBuilder** builder_) {
    const SL_UIElementBuilder builder = **builder_;

    SL_UIElement* ptr;
    if ((global_flags & SL_FLAGS_MANAGE_MEMORY) == SL_FLAGS_MANAGE_MEMORY) {
        ptr = addItemToMap(ElementHashMap, builder.name, HASHMAP_TYPE_UI_ELEMENT, &mapCount, &mapLimit, NULL);
    }
    else {
        ptr = malloc(sizeof(SL_UIElement));
    }
    ptr->name = builder.name;

    if ((builder.flags & SL_INNERFLAG_ABSOLUTE) == SL_INNERFLAG_ABSOLUTE) {
        ptr->src_rect.x = builder.xab;
        ptr->src_rect.y = builder.yab;
        ptr->src_rect.w = builder.wab;
        ptr->src_rect.h = builder.hab;
    }
    else {
        ptr->src_rect.x = (int)(builder.x * (float)screen_width);
        ptr->src_rect.y = (int)(builder.y * (float)screen_height);
        ptr->src_rect.w = (int)(builder.w * (float)screen_width);
        ptr->src_rect.h = (int)(builder.h * (float)screen_height);
    }
    ptr->flags = builder.flags;

    if (builder.skin != NULL) {
        // TODO error handle
        ptr->texture_skin = builder.skin;
        int tex_w;
        int tex_h;
        SDL_QueryTexture(builder.skin, NULL, NULL, &tex_w, &tex_h);
        ptr->skin_step_x = tex_w / 3;
        ptr->skin_step_y = tex_h / 3;
    }

    ptr->TextObjectMap = calloc(MAP_INIT, sizeof(SL_TextObject));
    ptr->textMapCount = 0;
    ptr->textMapLimit = MAP_INIT;
    ptr->TextObjectIterator = calloc(MAX_TEXT_OBJS, sizeof(int));

    for (int i = 0; i < builder.num_text_objects; i++) {
        int idx;
        SL_TextObject* obj_ptr = addItemToMap(ptr->TextObjectMap, builder.text_builders[i].id,
                                              HASHMAP_TYPE_TEXT_ELEMENT, &ptr->textMapCount, &ptr->textMapLimit, &idx);
        *obj_ptr = CreateTextObject(
                builder.text_builders[i].text,
                builder.text_builders[i].size,
                builder.text_builders[i].x,
                builder.text_builders[i].y
        );
        ptr->TextObjectIterator[ptr->textMapCount - 1] = idx;

    }

    ptr->font = builder.font;
    SDL_SetTextureScaleMode(ptr->font, SDL_ScaleModeNearest);

    free(builder.text_builders);
    free(*builder_);

    return ptr;
}

void SL_FreeElement(SL_UIElement* element) {
    if (!element) return;
    free(element->TextObjectMap);
    free(element->TextObjectIterator);
    if ((global_flags & SL_FLAGS_MANAGE_MEMORY) != SL_FLAGS_MANAGE_MEMORY) {
        free(element);
    }
}

static SDL_Rect getGlyphSrc(char c) {
    return Glyphs[c - font_start].src;
}

static SL_Glyph getGlyph(char c) {
    return Glyphs[c - font_start];
}

static int hashName(const char* name) {
    unsigned long long hash = 0;
    char* c = (char*) name;
    while (*c != '\0') {
        //hash += *c * 0x1F; // everybody loves mersenne primes!
        hash += *c;
        hash <<= 1;
        c++;
    }
    return ((int)(hash % UINT32_MAX)) % mapLimit;
}

static void *getVoidPtrOffset(void *ptr, HashMapType type, int idx) {
    void* addr = NULL;
    switch (type) {
        case HASHMAP_TYPE_UI_ELEMENT: {
            SL_UIElement* temp = (SL_UIElement*) ptr;
            addr = &(temp[idx]);
            break;
        }
        case HASHMAP_TYPE_TEXT_ELEMENT: {
            SL_TextObject* temp = (SL_TextObject*) ptr;
            addr = &(temp[idx]);
            break;
        }
    }
    return addr;
}

// Takes in a name of an element to be created and returns the memory address to store it at
static void *addItemToMap(void *map_, const char *name, HashMapType type, int *count, int *limit, int *index) {
    int idx = hashName(name);

    while (itemAtAddressExists(getVoidPtrOffset(map_, type, idx))) {
        idx = (idx + 1) % *limit;
    }
    void* ptr = getVoidPtrOffset(map_, type, idx);
    if (index) {
        *index = idx;
    }
    *count = *count + 1;
    if (*count >= *limit) {
        // TODO something about this is broken when the limit is reached...
        // TODO also if i ever fix this we have to rehash
        *limit *= 2;
        size_t size = 0;
        switch (type) {
            case HASHMAP_TYPE_UI_ELEMENT: {
                size = sizeof(SL_UIElement);
                break;
            }
            case HASHMAP_TYPE_TEXT_ELEMENT: {
                size = sizeof(SL_TextObject);
                break;
            }
        }
        void* temp1 = realloc(map_, *limit * size);
        if (temp1 != NULL) *(&map_) = temp1;
    }
    return ptr;
}

SL_UIElement* getElementFromMap(const char* name) {
    return (SL_UIElement*) getItemFromMap(ElementHashMap, name, &mapLimit, HASHMAP_TYPE_UI_ELEMENT);
}

void *getItemFromMap(void *map_, const char *name, const int *limit, enum HashMapType type) {
    int idx = hashName(name);

    int start_idx = idx;
    void* ptr = NULL;
    do {

        void* offset_ptr = getVoidPtrOffset(map_, type, idx);
        int exists = itemAtAddressExists(offset_ptr);

        const char* n_;

        switch (type) {
            case HASHMAP_TYPE_UI_ELEMENT: {
                SL_UIElement* e = (SL_UIElement*) offset_ptr;
                n_ = e->name;
                break;
            }
            case HASHMAP_TYPE_TEXT_ELEMENT: {
                SL_TextObject* obj = (SL_TextObject*) offset_ptr;
                n_ = obj->id;
                break;
            }
        }

        if (exists && strcmp(name, n_) == 0) {
            ptr = offset_ptr;
            break;
        }
        idx = (idx + 1) % *limit;
    } while (idx != start_idx);

    return ptr;
}

void SL_DrawElement(const SL_UIElement* element) {

    if (!SL_ElementIsActive(element)) return;

    int start_x = element->src_rect.x;
    int start_y = element->src_rect.y;
    int width = element->src_rect.w;
    int height = element->src_rect.h;
    SDL_Vertex vertices[NUM_VERTICES];

    for (int y = 0; y < 4; y++) {
        int step_y;
        switch (y) {
            default: step_y = 0; break;
            case 1: step_y = element->skin_step_y; break;
            case 2: step_y = height - element->skin_step_y; break;
            case 3: step_y = height; break;
        }
        for (int x = 0; x < 4; x++) {
            int step_x;
            switch (x) {
                default: step_x = 0; break;
                case 1: step_x = element->skin_step_x; break;
                case 2: step_x = width - element->skin_step_x; break;
                case 3: step_x = width; break;
            }
            int rx = start_x + step_x;
            int ry = start_y + step_y;
            float uvx = (float)x / 3.0f;
            float uvy = (float)y / 3.0f;

            int idx = (y * 4) + x;
            SDL_Vertex v = {
                {(float)rx, (float)ry},
                {0xFF, 0xFF, 0xFF, 0xFF},
                {uvx, uvy},
            };
            vertices[idx] = v;
        }
    }
    SDL_RenderGeometry(render_context, element->texture_skin, vertices, NUM_VERTICES, indices, NUM_INDICES);

    for (int k = 0; k < element->textMapCount; k++) {
        SL_TextObject t = (element->TextObjectMap[element->TextObjectIterator[k]]);

        int textx = t.x_start + start_x;
        int texty = t.y_start + start_y;
        int line_break_limit = start_x + width;

        static const int quad_pts = 4;
        static const int quad_idx = 6;

        int num_text_vertices = t.length * quad_pts;
        int num_text_indices = t.length * quad_idx;

        SDL_Vertex text_vertices[num_text_vertices];
        int idxs[num_text_indices];

        const int idxs_raw[6] = {0, 1, 2, 1, 2, 3};

        int curr_word = 0;

        for (int i = 0; i < t.length; i++) {

            for (int j = i * quad_idx; j < (i * quad_idx) + quad_idx; j++) {
                idxs[j] = idxs_raw[j % quad_idx] + (i * quad_pts);
            }

            SL_Glyph g = t.text[i];

            // Lower Left
            SDL_Vertex v1 = {
                    {(float)textx + (float)(g.x_offset) * t.scale, (float)texty + ((float)(g.src.h + g.y_offset) * t.scale)},
                    t.c,
                    {g.u_min, g.v_min}
            };
            // Upper Left
            SDL_Vertex v2 = {
                    {(float)textx + ((float)g.x_offset * t.scale), (float)texty + (float)g.y_offset * t.scale},
                    t.c,
                    {g.u_min, g.v_max}
            };
            // Lower Right
            SDL_Vertex v3 = {
                    {(float)textx + ((float)(g.src.w + g.x_offset) * t.scale), (float)texty + ((float)(g.src.h + g.y_offset) * t.scale)},
                    t.c,
                    {g.u_max, g.v_min}
            };
            // Upper Right
            SDL_Vertex v4 = {
                    {(float)textx + ((float)(g.src.w + g.x_offset) * t.scale), (float)texty + ((float)g.y_offset * t.scale)},
                    t.c,
                    {g.u_max, g.v_max}
            };
            text_vertices[i * quad_pts] = v1;
            text_vertices[(i * quad_pts) + 1] = v2;
            text_vertices[(i * quad_pts) + 2] = v3;
            text_vertices[(i * quad_pts) + 3] = v4;
            textx += (int)((float)g.x_advance * t.scale);

            if (g.raw_char == ' ') {
                curr_word++;
                if (textx > line_break_limit || textx + t.word_widths[curr_word] > line_break_limit) {
                    textx = t.x_start + start_x;
                    texty += (int) ((float) lineHeight * t.scale);
                }
            }
        }

        // TODO With a little more work I can make this one draw call all together
        SDL_RenderGeometry(render_context, element->font, text_vertices, num_text_vertices, idxs, num_text_indices);
    }
}

static SL_TextObject CreateTextObject(const char *raw_text, float desired_size, int x_, int y_) {
    SL_TextObject obj;
    // TODO set color
    obj.c = Color_White;
    obj.x_start = x_;
    obj.y_start = y_;
    obj.scale = desired_size / font_size;
    obj.length = 0;
    obj.num_words = 1;

    for (char* c_tmp = (char*) raw_text; *c_tmp != '\0'; c_tmp++) {
        obj.length++;
        if (*c_tmp == ' ') {
            obj.num_words++;
        }
    }
    obj.word_widths = calloc(obj.num_words, sizeof(int));
    obj.text = calloc(obj.length, sizeof(SL_Glyph));
    int curr_word_width = 0;
    int curr_word = 0;
    for (int i = 0; i < obj.length; i++) {
        obj.text[i] = getGlyph(raw_text[i]);
        if (raw_text[i] == ' ') {
            obj.word_widths[curr_word++] = curr_word_width;
            curr_word_width = 0;
        }
        else {
            curr_word_width += (int)((float)obj.text[i].x_advance * obj.scale);
        }
    }
    return obj;
}

static void DestroyTextObject(SL_TextObject* ptr) {
    free(ptr->text);
    free(ptr->word_widths);
    free(ptr);
}


int SL_ElementIsActive(const SL_UIElement* element) {
    if (!element) return 0;
    return element->flags & SL_INNERFLAG_ACTIVE ? 1 : 0;
}

void SL_ActivateElement(SL_UIElement* element) {
    if (!element) return;
    element->flags |= SL_INNERFLAG_ACTIVE;
}

void SL_DeactivateElement(SL_UIElement* element) {
    if (!element) return;
    element->flags &= ~SL_INNERFLAG_ACTIVE;
}

/*
 * Check to see if an elements exists within calloc-allocated memory
 * Useful to see if an element exists at a given index in a hashmap
 */
static int itemAtAddressExists(void* ptr) {
    return *((int*)ptr) == 0 ? 0 : 1;
}