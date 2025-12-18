#pragma once

#include <stdint.h>

typedef struct FontWin32 FontWin32;

/* Creates a bitmap-font in the current OpenGL context using wglUseFontBitmaps.
   faceName: "Helvetica" (falls back to Arial)
   pointSize: e.g. 12 */
FontWin32* font_create_helvetica_12(void* sdl_window);
void font_destroy(FontWin32* f);

int font_height(const FontWin32* f);
int font_measure(const FontWin32* f, const char* text);
void font_draw(const FontWin32* f, int x, int y, const char* text, uint8_t gray);


