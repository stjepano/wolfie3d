#ifndef WOLFIE_SURFACE_H_
#define WOLFIE_SURFACE_H_

#include <stdint.h>

typedef struct {
    void* pixels;
    uint32_t width;
    uint32_t height;
} Surface;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} Rect;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RGBA;

#define RECT_IS_EMPTY(pr) ((pr)->w <= 0 || (pr)->h <= 0)
Rect intersect_rects(const Rect* a, const Rect* b);
Rect union_rects(const Rect* a, const Rect* b);

Surface *allocate_surface(uint32_t width, uint32_t height);
void fill_surface(Surface *surface, const Rect *rect, RGBA color);
void draw_line(Surface *surface, int32_t x0, int32_t y0, int32_t x1, int32_t y1, RGBA color);
void free_surface(Surface *surface);

static inline uint32_t encode_pixel(RGBA color) {
    return ((uint32_t)color.a << 24) | ((uint32_t)color.b << 16) | ((uint32_t)color.g << 8) | (uint32_t)color.r;
}


#endif // WOLFIE_SURFACE_H_