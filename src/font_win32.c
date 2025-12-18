#define _CRT_SECURE_NO_WARNINGS

#include "font_win32.h"

#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wingdi.h>
#endif

struct FontWin32 {
    GLuint base;
    int ascent;
    int height;
    int widths[96]; /* ASCII 32..127 */
};

static int get_widths(HDC hdc, int* out_widths, int fallback) {
    int w[96] = { 0 };
    if (!GetCharWidth32A(hdc, 32, 127, w)) {
        for (int i = 0; i < 96; i++) out_widths[i] = fallback;
        return 0;
    }
    for (int i = 0; i < 96; i++) out_widths[i] = w[i];
    return 1;
}

static HFONT create_font(HDC hdc, const char* face, int pointSize) {
    int height = -MulDiv(pointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    return CreateFontA(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FF_DONTCARE, face);
}

FontWin32* font_create_helvetica_12(void* sdl_window) {
#ifndef _WIN32
    (void)sdl_window;
    return NULL;
#else
    SDL_Window* win = (SDL_Window*)sdl_window;
    if (!win) return NULL;

    /* Ensure context current, then use wglGetCurrentDC */
    HDC hdc = wglGetCurrentDC();
    HWND hwnd = NULL;
    int gotHdcFromWgl = (hdc != NULL);

    if (!hdc) {
        SDL_SysWMinfo wm;
        SDL_VERSION(&wm.version);
        if (!SDL_GetWindowWMInfo(win, &wm)) return NULL;
        hwnd = wm.info.win.window;
        if (!hwnd) return NULL;
        hdc = GetDC(hwnd);
        if (!hdc) return NULL;
    }

    HFONT font = create_font(hdc, "Helvetica", 12);
    if (!font) font = create_font(hdc, "Arial", 12);
    if (!font) {
        if (!gotHdcFromWgl && hwnd) ReleaseDC(hwnd, hdc);
        return NULL;
    }

    HFONT old = (HFONT)SelectObject(hdc, font);
    TEXTMETRICA tm;
    memset(&tm, 0, sizeof(tm));
    GetTextMetricsA(hdc, &tm);

    FontWin32* f = (FontWin32*)calloc(1, sizeof(FontWin32));
    if (!f) {
        SelectObject(hdc, old);
        DeleteObject(font);
        if (!gotHdcFromWgl && hwnd) ReleaseDC(hwnd, hdc);
        return NULL;
    }

    f->base = glGenLists(96);
    f->ascent = (int)tm.tmAscent;
    f->height = (int)(tm.tmHeight + tm.tmExternalLeading);
    get_widths(hdc, f->widths, (int)tm.tmAveCharWidth);

    if (f->base == 0 || !wglUseFontBitmapsA(hdc, 32, 96, f->base)) {
        if (f->base) glDeleteLists(f->base, 96);
        free(f);
        SelectObject(hdc, old);
        DeleteObject(font);
        if (!gotHdcFromWgl && hwnd) ReleaseDC(hwnd, hdc);
        return NULL;
    }

    SelectObject(hdc, old);
    DeleteObject(font);
    if (!gotHdcFromWgl && hwnd) ReleaseDC(hwnd, hdc);
    return f;
#endif
}

void font_destroy(FontWin32* f) {
    if (!f) return;
    if (f->base) glDeleteLists(f->base, 96);
    free(f);
}

int font_height(const FontWin32* f) {
    return (f && f->height > 0) ? f->height : 16;
}

int font_measure(const FontWin32* f, const char* text) {
    if (!text) return 0;
    if (!f) return (int)strlen(text) * 8;
    int w = 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
        unsigned c = *p;
        if (c < 32 || c > 127) { w += f->widths['?' - 32]; continue; }
        w += f->widths[c - 32];
    }
    return w;
}

void font_draw(const FontWin32* f, int x, int y, const char* text, uint8_t gray) {
    if (!f || !text) return;
    float g = gray / 255.0f;
    glColor3f(g, g, g);
    glRasterPos2i(x, y + f->ascent);
    glListBase(f->base - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, (const GLubyte*)text);
}


