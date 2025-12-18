#define _CRT_SECURE_NO_WARNINGS

#include "font_win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <GL/gl.h>

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
    /* pointSize is already adjusted for DPI in font_create_helvetica */
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    int height = -MulDiv(pointSize, dpi, 72);
    return CreateFontA(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FF_DONTCARE, face);
}

FontWin32* font_create_helvetica_12(void* glfw_window) {
    return font_create_helvetica(glfw_window, 12);
}

FontWin32* font_create_helvetica(void* glfw_window, int pointSize) {
#ifndef _WIN32
    (void)glfw_window;
    (void)pointSize;
    return NULL;
#else
    GLFWwindow* win = (GLFWwindow*)glfw_window;
    if (!win) return NULL;

    /* Ensure context current, then use wglGetCurrentDC */
    HDC hdc = wglGetCurrentDC();
    HWND hwnd = NULL;
    int gotHdcFromWgl = (hdc != NULL);

    if (!hdc) {
        hwnd = glfwGetWin32Window(win);
        if (!hwnd) return NULL;
        hdc = GetDC(hwnd);
        if (!hdc) return NULL;
    }

    /* Get actual DPI from device context - Windows might return scaled DPI */
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    int base_dpi = 96;
    float dpi_ratio = (float)dpi / (float)base_dpi;
    
    /* Scale down point size to compensate for Windows DPI scaling */
    /* If DPI is 192 (200% scaling), we need to pass 6pt to get 12pt visual size */
    int adjusted_point_size = (int)((float)pointSize / dpi_ratio);
    if (adjusted_point_size < 1) adjusted_point_size = 1;
    
    fprintf(stdout, "Font DPI: %d, ratio: %.2f, requested: %dpt, adjusted: %dpt\n", 
            dpi, dpi_ratio, pointSize, adjusted_point_size);

    HFONT font = create_font(hdc, "Helvetica", adjusted_point_size);
    if (!font) font = create_font(hdc, "Arial", adjusted_point_size);
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



