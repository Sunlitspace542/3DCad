#define _CRT_SECURE_NO_WARNINGS
#include "render_gl.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "SDL2_image.lib")
#endif

static void set_color(RG_Color c) {
    glColor4ub(c.r, c.g, c.b, c.a);
}

void rg_begin_frame(int win_w, int win_h, RG_Color clear) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_SCISSOR_TEST);

    glViewport(0, 0, win_w, win_h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)win_w, (double)win_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void rg_set_viewport_tl(int x, int y, int w, int h, int win_h) {
    if (w <= 0 || h <= 0) return;
    int gl_y = win_h - (y + h);
    if (gl_y < 0) gl_y = 0;
    glViewport(x, gl_y, w, h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, gl_y, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)w, (double)h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void rg_reset_viewport(int win_w, int win_h) {
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, win_w, win_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)win_w, (double)win_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void rg_fill_rect(int x, int y, int w, int h, RG_Color c) {
    if (w <= 0 || h <= 0) return;
    set_color(c);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

void rg_stroke_rect(int x, int y, int w, int h, RG_Color c) {
    if (w <= 0 || h <= 0) return;
    set_color(c);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

void rg_line(int x1, int y1, int x2, int y2, RG_Color c) {
    set_color(c);
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
}

void rg_fill_polygon(const int* x_coords, const int* y_coords, int num_points, RG_Color c) {
    if (!x_coords || !y_coords || num_points < 3) return;
    set_color(c);
    glBegin(GL_POLYGON);
    for (int i = 0; i < num_points; i++) {
        glVertex2i(x_coords[i], y_coords[i]);
    }
    glEnd();
}

RG_Texture* rg_load_texture(const char* path) {
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        fprintf(stderr, "Failed to load image %s: %s\n", path, IMG_GetError());
        return NULL;
    }

    /* Convert to RGBA format */
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!rgba) {
        fprintf(stderr, "Failed to convert image %s: %s\n", path, SDL_GetError());
        return NULL;
    }

    RG_Texture* tex = (RG_Texture*)calloc(1, sizeof(RG_Texture));
    if (!tex) {
        SDL_FreeSurface(rgba);
        return NULL;
    }

    tex->w = rgba->w;
    tex->h = rgba->h;

    glGenTextures(1, &tex->gl_id);
    glBindTexture(GL_TEXTURE_2D, tex->gl_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(rgba);
    return tex;
}

void rg_free_texture(RG_Texture* tex) {
    if (!tex) return;
    if (tex->gl_id) glDeleteTextures(1, &tex->gl_id);
    free(tex);
}

void rg_draw_texture(RG_Texture* tex, int x, int y, int w, int h) {
    if (!tex || !tex->gl_id) return;
    if (w <= 0 || h <= 0) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex->gl_id);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(x + w, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + h);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void rg_draw_texture_inverted(RG_Texture* tex, int x, int y, int w, int h) {
    if (!tex || !tex->gl_id) return;
    if (w <= 0 || h <= 0) return;

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, tex->gl_id);
    
    /* Use GL_COMBINE to compute: result = constant - texture = white - texture */
    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, white);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_SUBTRACT);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(x + w, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + h);
    glEnd();
    
    /* Restore defaults */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}


