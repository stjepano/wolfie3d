#include "surface.h"

#include <stdlib.h>
#include <assert.h>

#include "common.h"

Rect intersect_rects(const Rect *a, const Rect *b)
{
    assert(a != nullptr && b != nullptr);
    if (RECT_IS_EMPTY(a) || RECT_IS_EMPTY(b))
    {
        return (Rect){0, 0, 0, 0};
    }

    Rect res = {0};
    res.x = MAX(a->x, b->x);
    res.y = MAX(a->y, b->y);
    res.w = MIN(a->x + a->w, b->x + b->w) - res.x;
    res.h = MIN(a->y + a->h, b->y + b->h) - res.y;
    return res;
}

Rect union_rects(const Rect *a, const Rect *b)
{
    assert(a != nullptr && b != nullptr);
    if (RECT_IS_EMPTY(a))
    {
        return *b;
    }
    if (RECT_IS_EMPTY(b))
    {
        return *a;
    }

    Rect res = {0};
    res.x = MIN(a->x, b->x);
    res.y = MIN(a->y, b->y);
    res.w = MAX(a->x + a->w, b->x + b->w) - res.x;
    res.h = MAX(a->y + a->h, b->y + b->h) - res.y;
    return res;
}

Surface *allocate_surface(uint32_t width, uint32_t height)
{
    assert(width > 0 && height > 0);
    Surface *res = calloc(1, sizeof(Surface));
    if (res == nullptr)
    {
        return nullptr;
    }
    res->pixels = malloc(sizeof(uint32_t) * width * height);
    if (res->pixels == nullptr)
    {
        free(res);
        return nullptr;
    }

    res->width = width;
    res->height = height;
    return res;
}

void fill_surface(Surface *surface, const Rect *rect, RGBA color)
{
    assert(surface && surface->pixels && surface->width && surface->height);
    Rect r = {0};
    Rect surface_rect = {0, 0, surface->width, surface->height};
    if (rect == nullptr)
    {
        // whole surface area will be filled
        r = surface_rect;
    }
    else
    {
        r = *rect;
    }

    Rect target_rect = intersect_rects(&r, &surface_rect);
    if (RECT_IS_EMPTY(&target_rect))
    {
        return;
    }

    uint32_t encoded_color = encode_pixel(color);
    for (int32_t row = 0; row < target_rect.h; row++)
    {
        uint32_t *row_ptr = ((uint32_t *)surface->pixels) + ((target_rect.y + row) * surface->width) + target_rect.x;
        for (int32_t col = 0; col < target_rect.w; col++)
        {
            row_ptr[col] = encoded_color;
        }
    }
}

#define PIXEL_ON_SURFACE(ps, x, y) ((x) >= 0 && (x) < (int32_t)(ps)->width && (y) >= 0 && (y) < (int32_t)(ps)->height)

static inline void plot_line_low(Surface *s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t col)
{
    // NOTE: both in this function and plot_line_high I decided to use normal mathematical functions (floating point)
    //       because on modern CPUs floating point arithmetic is of approximately same speed and is easier to think about.

    // How the algorithm works:
    // Calculations are derived from line equation f(x) = y = a * x + b, where a = delta_y / delta_x and b
    // is b = y0 - a * x0.
    // On each iteration:
    //  draw_pixel
    //  calculate what the y is for next x and if distance is >= 0.5 increment y.
    //  -- in plot_line_high we calculate what the x is for next y and if distance is >= 0.5 increment x
    //  Increment direction depends on slope so if slope is negative y_inc = -1, for positive y_inc = 1.
    //  -- if slope is positive we calculate distance as f(x) - y |
    //  -- if slope is negative we calculate distance as y - f(x) | These both are accomplished by multiplying with y_inc

    // PRECONDITIONS:
    // x1 >= x0
    // This function will not work correctly if abs(y1 - y0) > abs(x1 - x0), you need to use plot_line_high for that case
    

    assert(x1 >= x0);

    const float dx = (x1 - x0);
    float dy = (y1 - y0);
    int32_t y_inc = 1;
    if (dy < 0)
    {
        y_inc = -1;
    }
    const float a = dy / dx;
    const float b = ((float)y0) - a * ((float)x0);

    int32_t y = y0;
    for (int32_t x = x0; x <= x1; x++)
    {
        if (PIXEL_ON_SURFACE(s, x, y))
        {
            ((uint32_t *)s->pixels)[y * s->width + x] = col;
        }
        float ny = a * ((float)x + 1) + b;
        if ((ny - y) * y_inc >= 0.5f)
        {
            y += y_inc;
        }
    }
}

static inline void plot_line_high(Surface *s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t col)
{
    // see plot_line_low
    
    assert(y1 >= y0);

    const float dx = (x1 - x0);
    const float dy = (y1 - y0);
    int32_t x_inc = 1;
    if (dx < 0)
    {
        x_inc = -1;
    }
    const float a = dy / dx;
    const float one_over_a = 1.0f / a;
    const float b = ((float)y0) - a * ((float)x0);

    int32_t x = x0;
    for (int32_t y = y0; y <= y1; y++)
    {
        if (PIXEL_ON_SURFACE(s, x, y))
        {
            ((uint32_t *)s->pixels)[y * s->width + x] = col;
        }
        float nx = one_over_a * (((float)y + 1) - b);
        if ((nx - x) * x_inc >= 0.5f)
        {
            x += x_inc;
        }
    }
}

void draw_line(Surface *surface, int32_t x0, int32_t y0, int32_t x1, int32_t y1, RGBA color)
{
    assert(surface && surface->pixels && surface->width > 0 && surface->height > 0);

    uint32_t encoded_color = encode_pixel(color);
    if (x0 == x1 && y0 == y1)
    {
        // draw single pixel
        if (PIXEL_ON_SURFACE(surface, x0, y0))
        {
            uint32_t *pixel_ptr = ((uint32_t *)surface->pixels) + (y0 * surface->width) + x0;
            *pixel_ptr = encoded_color;
        }
        return;
    }
    if (y0 == y1)
    {
        // draw horizontal line
        if (!PIXEL_ON_SURFACE(surface, 0, y0))
        {
            // y outside of surface, don't draw
            return;
        }
        int32_t min = MIN(x0, x1);
        int32_t max = MAX(x0, x1);
        for (int32_t col = min; col <= max; col++)
        {
            if (PIXEL_ON_SURFACE(surface, col, y0))
            {
                ((uint32_t *)surface->pixels)[y0 * surface->width + col] = encoded_color;
            }
        }
    }
    else if (x0 == x1)
    {
        // draw vertical line
        if (!PIXEL_ON_SURFACE(surface, x0, 0))
        {
            // x outside
            return;
        }
        int32_t min = MIN(y0, y1);
        int32_t max = MAX(y0, y1);
        for (int32_t row = min; row <= max; row++)
        {
            if (PIXEL_ON_SURFACE(surface, x0, row))
            {
                ((uint32_t *)surface->pixels)[row * surface->width + x0] = encoded_color;
            }
        }
    }
    else
    {
        if (ABS(y1 - y0) <= ABS(x1 - x0))
        {

            if (x0 < x1)
            {
                plot_line_low(surface, x0, y0, x1, y1, encoded_color);
            }
            else
            {
                plot_line_low(surface, x1, y1, x0, y0, encoded_color);
            }
        }
        else
        {
            if (y0 < y1)
            {
                plot_line_high(surface, x0, y0, x1, y1, encoded_color);
            }
            else
            {
                plot_line_high(surface, x1, y1, x0, y0, encoded_color);
            }
        }
    }
}

void free_surface(Surface *surface)
{
    if (surface)
    {
        free(surface->pixels);
        free(surface);
    }
}
