#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include "gui.h"
#include "font_win32.h"

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glfw3.lib")
#endif

static void clamp_and_center(int* w, int* h, int* x, int* y) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode) {
            int maxW = mode->width > 80 ? mode->width - 40 : mode->width;
            int maxH = mode->height > 120 ? mode->height - 80 : mode->height;
            if (maxW > 0 && *w > maxW) *w = maxW;
            if (maxH > 0 && *h > maxH) *h = maxH;
        }
    }
    if (*x == 0 && *y == 0) {
        *x = -1; /* GLFW will center if x/y are -1 */
        *y = -1;
    }
}

/* Mouse wheel callback */
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset; /* unused */
    int* delta = (int*)glfwGetWindowUserPointer(window);
    if (delta) {
        *delta = (int)yoffset; /* GLFW: positive y = scroll up (zoom in) */
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    if (!glfwInit()) {
        fprintf(stderr, "GLFW initialization failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    int w = 1258, h = 983;
    int x = 0, y = 0;
    clamp_and_center(&w, &h, &x, &y);

    GLFWwindow* win = glfwCreateWindow(w, h, "3Ddraw (GUI repro)", NULL, NULL);
    if (!win) {
        fprintf(stderr, "GLFW window creation failed\n");
        glfwTerminate();
        return 1;
    }

    if (x != -1 && y != -1) {
        glfwSetWindowPos(win, x, y);
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    
    /* Verify depth buffer was created */
    int depthBits = 0;
    glGetIntegerv(GL_DEPTH_BITS, &depthBits);
    fprintf(stdout, "GL_DEPTH_BITS = %d\n", depthBits);
    if (depthBits == 0) {
        fprintf(stderr, "WARNING: No depth buffer! Depth testing will not work.\n");
    }

    /* Get initial framebuffer size to calculate DPI scale */
    int initial_fb_w, initial_fb_h;
    glfwGetFramebufferSize(win, &initial_fb_w, &initial_fb_h);
    float dpi_scale = (w > 0 && initial_fb_w > 0) ? (float)initial_fb_w / (float)w : 1.0f;
    
    /* If DPI scaling is active (scale > 1.0), reduce font size to compensate.
       Windows font rendering already accounts for DPI, so we need to use a smaller
       logical point size to get the same visual size relative to the window. */
    int font_size = 12;
    if (dpi_scale > 1.1f) {
        /* Scale down font size - use inverse of DPI scale */
        font_size = (int)(12.0f / dpi_scale);
        if (font_size < 6) font_size = 6; /* Minimum 6pt */
    }
    
    fprintf(stdout, "DPI scale: %.2f, Font size: %dpt (window: %dx%d, fb: %dx%d)\n", 
            dpi_scale, font_size, w, h, initial_fb_w, initial_fb_h);
    
    FontWin32* font = font_create_helvetica(win, font_size);

    GuiState* gui = gui_create();
    gui_set_font(gui, font);
    gui_load_tool_icons(gui, "resources");
    gui_load_anim_icons(gui, "resources");

    int mouse_down = 0;
    int mx = 0, my = 0;
    static int last_mouse_down = 0;
    int wheel_delta = 0;

    glfwSetScrollCallback(win, scroll_callback);
    glfwSetWindowUserPointer(win, &wheel_delta);

    while (!glfwWindowShouldClose(win)) {
        int pressed = 0, released = 0;
        int current_wheel_delta = wheel_delta;
        wheel_delta = 0; /* Reset after reading each frame */

        glfwPollEvents();

        /* Mouse position */
        double dmx, dmy;
        glfwGetCursorPos(win, &dmx, &dmy);
        mx = (int)dmx;
        my = (int)dmy;

        /* Mouse button state */
        int current_mouse_down = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (current_mouse_down && !last_mouse_down) {
            pressed = 1;
        }
        if (!current_mouse_down && last_mouse_down) {
            released = 1;
        }
        mouse_down = current_mouse_down;
        last_mouse_down = current_mouse_down;
        
        /* Right mouse button state */
        static int last_mouse_right_down = 0;
        int current_mouse_right_down = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        int right_pressed = 0, right_released = 0;
        if (current_mouse_right_down && !last_mouse_right_down) {
            right_pressed = 1;
        }
        if (!current_mouse_right_down && last_mouse_right_down) {
            right_released = 1;
        }
        last_mouse_right_down = current_mouse_right_down;

        /* Get window size (logical pixels) - use for layout/coordinates */
        glfwGetWindowSize(win, &w, &h);
        
        /* Get framebuffer size (physical pixels) - use for OpenGL viewport */
        int fb_w, fb_h;
        glfwGetFramebufferSize(win, &fb_w, &fb_h);

        GuiInput in = { 0 };
        in.mouse_x = mx;
        in.mouse_y = my;
        in.mouse_down = mouse_down;
        in.mouse_pressed = pressed;
        in.mouse_released = released;
        in.mouse_right_down = current_mouse_right_down;
        in.mouse_right_pressed = right_pressed;
        in.mouse_right_released = right_released;
        in.wheel_delta = current_wheel_delta;

        /* Use window size for layout, framebuffer size for viewport */
        gui_update(gui, &in, w, h);
        gui_draw(gui, &in, w, h, fb_w, fb_h);

        glfwSwapBuffers(win);
    }

    gui_destroy(gui);
    font_destroy(font);

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}


