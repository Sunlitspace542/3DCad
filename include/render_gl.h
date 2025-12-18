#pragma once

#include <stdint.h>

typedef struct RG_Color {
    uint8_t r, g, b, a;
} RG_Color;

typedef struct RG_Texture {
    unsigned int gl_id;
    int w, h;
} RG_Texture;

void rg_begin_frame(int win_w, int win_h, RG_Color clear);
void rg_set_viewport_tl(int x, int y, int w, int h, int win_h);
void rg_reset_viewport(int win_w, int win_h, int fb_w, int fb_h);

void rg_fill_rect(int x, int y, int w, int h, RG_Color c);
void rg_stroke_rect(int x, int y, int w, int h, RG_Color c);
void rg_line(int x1, int y1, int x2, int y2, RG_Color c);
void rg_fill_polygon(const int* x_coords, const int* y_coords, int num_points, RG_Color c);

/* Texture loading/rendering */
RG_Texture* rg_load_texture(const char* path);
void rg_free_texture(RG_Texture* tex);
void rg_draw_texture(RG_Texture* tex, int x, int y, int w, int h);
void rg_draw_texture_inverted(RG_Texture* tex, int x, int y, int w, int h);


