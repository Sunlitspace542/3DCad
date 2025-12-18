#define _CRT_SECURE_NO_WARNINGS

#include "cad_view.h"
#include "render_gl.h"
#include <SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ----------------------------------------------------------------------------
   View initialization
   ---------------------------------------------------------------------------- */

void CadView_Init(CadView* view, CadViewType type) {
    if (!view) return;
    view->type = type;
    view->zoom = 1.0;
    view->pan_x = 0.0;
    view->pan_y = 0.0;
    view->rot_x = 0.0;
    view->rot_y = 0.0;
    view->wireframe = 1; /* Default to wireframe */
}

void CadView_Reset(CadView* view) {
    if (!view) return;
    view->zoom = 1.0;
    view->pan_x = 0.0;
    view->pan_y = 0.0;
    view->rot_x = 0.0;
    view->rot_y = 0.0;
}

/* ----------------------------------------------------------------------------
   View transformations
   ---------------------------------------------------------------------------- */

void CadView_SetZoom(CadView* view, double zoom) {
    if (!view) return;
    if (zoom < 0.1) zoom = 0.1;
    if (zoom > 100.0) zoom = 100.0;
    view->zoom = zoom;
}

void CadView_Pan(CadView* view, double dx, double dy) {
    if (!view) return;
    view->pan_x += dx;
    view->pan_y += dy;
}

void CadView_Rotate(CadView* view, double dx, double dy) {
    if (!view) return;
    view->rot_x += dx;
    view->rot_y += dy;
    /* Clamp rotation */
    if (view->rot_x > 90.0) view->rot_x = 90.0;
    if (view->rot_x < -90.0) view->rot_x = -90.0;
}

/* ----------------------------------------------------------------------------
   3D to 2D projection
   ---------------------------------------------------------------------------- */

void CadView_ProjectPoint(const CadView* view, double x, double y, double z, 
                         int* out_x, int* out_y, int viewport_w, int viewport_h) {
    if (!view || !out_x || !out_y) return;
    
    double px, py, pz;
    
    /* Apply rotation for 3D view */
    if (view->type == CAD_VIEW_3D) {
        double rx = view->rot_x * M_PI / 180.0;
        double ry = view->rot_y * M_PI / 180.0;
        
        /* Rotate around X axis */
        double y1 = y * cos(rx) - z * sin(rx);
        double z1 = y * sin(rx) + z * cos(rx);
        
        /* Rotate around Y axis */
        px = x * cos(ry) + z1 * sin(ry);
        py = y1;
        pz = -x * sin(ry) + z1 * cos(ry);
    } else {
        /* Orthographic projections */
        switch (view->type) {
        case CAD_VIEW_TOP:
            px = x;
            py = -z;  /* Y becomes depth */
            pz = y;
            break;
        case CAD_VIEW_FRONT:
            /* Front view: looking from front, X is left/right, Y is up/down, Z is depth */
            px = x;  /* X is horizontal (left/right) */
            py = y;  /* Y is vertical (up/down, flipped) */
            pz = z;  /* Z is depth */
            break;
        case CAD_VIEW_RIGHT:
            /* Right view: looking from right, Z is forward/back, Y is up/down, X is depth */
            px = z;  /* Z becomes horizontal (forward/back) */
            py = y;  /* Y is vertical (up/down, flipped) */
            pz = -x; /* X is depth (negative because we're looking from right) */
            break;
        default:
            px = x;
            py = y;
            pz = z;
            break;
        }
    }
    
    /* Apply zoom and pan */
    px = px * view->zoom + view->pan_x;
    py = py * view->zoom + view->pan_y;
    
    /* Convert to viewport coordinates (center origin) */
    *out_x = (int)(viewport_w / 2 + px);
    *out_y = (int)(viewport_h / 2 - py); /* Flip Y for screen coordinates */
}

/* ----------------------------------------------------------------------------
   Rendering
   ---------------------------------------------------------------------------- */

void CadView_Render(const CadView* view, const CadCore* core, 
                    int viewport_x, int viewport_y, int viewport_w, int viewport_h, int win_h) {
    if (!view || !core) return;
    
    /* Set viewport - this sets up 2D projection and scissor */
    rg_set_viewport_tl(viewport_x, viewport_y, viewport_w, viewport_h, win_h);
    
    /* Ensure scissor test stays enabled to clip rendering to viewport */
    glEnable(GL_SCISSOR_TEST);
    int gl_y = win_h - (viewport_y + viewport_h);
    if (gl_y < 0) gl_y = 0;
    glScissor(viewport_x, gl_y, viewport_w, viewport_h);
    
    /* Clear background - must be done in 2D coordinates with proper OpenGL state */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    RG_Color white = { 255, 255, 255, 255 };
    rg_fill_rect(0, 0, viewport_w, viewport_h, white);
    
    /* Draw grid in 2D coordinates BEFORE switching to 3D projection */
    RG_Color grid = { 200, 200, 200, 255 };
    int center_x = viewport_w / 2;
    int center_y = viewport_h / 2;
    rg_line(0, center_y, viewport_w, center_y, grid);
    rg_line(center_x, 0, center_x, viewport_h, grid);
    
    /* Set up 3D projection with depth for solid mode ONLY */
    /* Do this AFTER clearing background and drawing grid so 2D operations work */
    if (!view->wireframe) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double depth_range = 10000.0;
        glOrtho(-viewport_w/2.0, viewport_w/2.0, -viewport_h/2.0, viewport_h/2.0, -depth_range, depth_range);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        /* Enable lighting */
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        
        /* Set up a directional light from top-left */
        float light_pos[4] = { 1.0f, 1.0f, 1.0f, 0.0f }; /* Directional light */
        float light_ambient[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
        float light_diffuse[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
        float light_specular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
        
        /* Enable color material so we can set colors per-vertex */
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    }
    
    /* Draw points and polygons */
    const CadFileData* data = &core->data;
    
    /* Use single gray color for all polygons: #AAAAAA */
    RG_Color poly_gray = { 0xAA, 0xAA, 0xAA, 255 };
    
    /* Draw polygons */
    for (int i = 0; i < data->polygonCount; i++) {
        CadPolygon* poly = CadCore_GetPolygon((CadCore*)core, i);
        if (!poly || poly->flags == 0) continue;
        
        /* Skip polygons with invalid first point */
        int16_t point_idx = poly->firstPoint;
        if (point_idx < 0 || point_idx >= CAD_MAX_POINTS) continue;
        if (poly->npoints < 3) continue;
        
        /* Collect polygon vertices with 3D coordinates for depth testing */
        #define MAX_STACK_POINTS 64
        int stack_x[MAX_STACK_POINTS];
        int stack_y[MAX_STACK_POINTS];
        double stack_z[MAX_STACK_POINTS]; /* Depth values */
        int* x_coords = NULL;
        int* y_coords = NULL;
        double* z_coords = NULL;
        
        int npoints = poly->npoints;
        if (npoints > 256) npoints = 256; /* Safety limit */
        
        if (npoints <= MAX_STACK_POINTS) {
            x_coords = stack_x;
            y_coords = stack_y;
            z_coords = stack_z;
        } else {
            x_coords = (int*)calloc(npoints, sizeof(int));
            y_coords = (int*)calloc(npoints, sizeof(int));
            z_coords = (double*)calloc(npoints, sizeof(double));
            if (!x_coords || !y_coords || !z_coords) continue;
        }
        
        int16_t current = point_idx;
        int count = 0;
        while (current >= 0 && current < CAD_MAX_POINTS && count < npoints) {
            CadPoint* pt = CadCore_GetPoint((CadCore*)core, current);
            if (!pt) break;
            
            /* Project to 2D screen coordinates */
            CadView_ProjectPoint(view, pt->pointx, pt->pointy, pt->pointz,
                                &x_coords[count], &y_coords[count], viewport_w, viewport_h);
            
            /* Calculate depth value (pz from projection) */
            double px, py, pz;
            if (view->type == CAD_VIEW_3D) {
                double rx = view->rot_x * M_PI / 180.0;
                double ry = view->rot_y * M_PI / 180.0;
                double y1 = pt->pointy * cos(rx) - pt->pointz * sin(rx);
                double z1 = pt->pointy * sin(rx) + pt->pointz * cos(rx);
                px = pt->pointx * cos(ry) + z1 * sin(ry);
                py = y1;
                pz = -pt->pointx * sin(ry) + z1 * cos(ry);
            } else {
                switch (view->type) {
                case CAD_VIEW_TOP:
                    px = pt->pointx;
                    py = -pt->pointz;
                    pz = pt->pointy;
                    break;
                case CAD_VIEW_FRONT:
                    px = pt->pointx;
                    py = -pt->pointy;
                    pz = pt->pointz;
                    break;
                case CAD_VIEW_RIGHT:
                    px = pt->pointz;
                    py = -pt->pointy;
                    pz = -pt->pointx;
                    break;
                default:
                    px = pt->pointx;
                    py = pt->pointy;
                    pz = pt->pointz;
                    break;
                }
            }
            /* Normalize depth to [-1, 1] range for OpenGL */
            double depth_scale = 10000.0;
            z_coords[count] = pz / depth_scale;
            
            current = pt->nextPoint;
            count++;
            if (count > 1000) break; /* Safety */
        }
        
        if (count >= 3 && x_coords && y_coords && z_coords) {
            if (view->wireframe) {
                /* Wireframe mode: draw edges (2D is fine for wireframe) */
                RG_Color black = { 0, 0, 0, 255 };
                for (int j = 0; j < count; j++) {
                    int next = (j + 1) % count;
                    rg_line(x_coords[j], y_coords[j], x_coords[next], y_coords[next], black);
                }
            } else {
                /* Solid mode: draw filled polygon with 3D depth coordinates and lighting */
                /* Calculate polygon normal for lighting using 3D coordinates */
                double nx = 0.0, ny = 0.0, nz = 0.0;
                if (count >= 3) {
                    /* Get first 3 points in 3D space to calculate normal */
                    int16_t pt_idx1 = point_idx;
                    int16_t pt_idx2 = -1;
                    int16_t pt_idx3 = -1;
                    CadPoint* pt1 = CadCore_GetPoint((CadCore*)core, pt_idx1);
                    if (pt1 && pt1->nextPoint >= 0) {
                        pt_idx2 = pt1->nextPoint;
                        CadPoint* pt2 = CadCore_GetPoint((CadCore*)core, pt_idx2);
                        if (pt2 && pt2->nextPoint >= 0) {
                            pt_idx3 = pt2->nextPoint;
                        }
                    }
                    
                    if (pt1 && pt_idx2 >= 0 && pt_idx3 >= 0) {
                        CadPoint* pt2 = CadCore_GetPoint((CadCore*)core, pt_idx2);
                        CadPoint* pt3 = CadCore_GetPoint((CadCore*)core, pt_idx3);
                        if (pt2 && pt3) {
                            /* Calculate two edge vectors in 3D space */
                            double v1x = pt2->pointx - pt1->pointx;
                            double v1y = pt2->pointy - pt1->pointy;
                            double v1z = pt2->pointz - pt1->pointz;
                            
                            double v2x = pt3->pointx - pt1->pointx;
                            double v2y = pt3->pointy - pt1->pointy;
                            double v2z = pt3->pointz - pt1->pointz;
                            
                            /* Cross product for normal */
                            nx = v1y * v2z - v1z * v2y;
                            ny = v1z * v2x - v1x * v2z;
                            nz = v1x * v2y - v1y * v2x;
                            
                            /* Normalize */
                            double len = sqrt(nx*nx + ny*ny + nz*nz);
                            if (len > 0.0001) {
                                nx /= len;
                                ny /= len;
                                nz /= len;
                            }
                        }
                    }
                }
                
                /* Set normal for lighting */
                glNormal3d(nx, ny, nz);
                
                /* Use gray color: #AAAAAA */
                glColor4ub(poly_gray.r, poly_gray.g, poly_gray.b, poly_gray.a);
                glBegin(GL_POLYGON);
                for (int j = 0; j < count; j++) {
                    /* Convert screen Y to OpenGL Y (flip) */
                    int gl_y = viewport_h - y_coords[j];
                    glVertex3d((double)x_coords[j] - viewport_w/2.0, 
                              (double)gl_y - viewport_h/2.0, 
                              z_coords[j]);
                }
                glEnd();
                
                /* Draw edges in darker gray for definition */
                RG_Color edge_color = { 0x66, 0x66, 0x66, 255 }; /* Darker gray */
                glDisable(GL_LIGHTING); /* Disable lighting for edges */
                glColor4ub(edge_color.r, edge_color.g, edge_color.b, edge_color.a);
                glBegin(GL_LINES);
                for (int j = 0; j < count; j++) {
                    int next = (j + 1) % count;
                    int gl_y1 = viewport_h - y_coords[j];
                    int gl_y2 = viewport_h - y_coords[next];
                    glVertex3d((double)x_coords[j] - viewport_w/2.0, 
                              (double)gl_y1 - viewport_h/2.0, 
                              z_coords[j]);
                    glVertex3d((double)x_coords[next] - viewport_w/2.0, 
                              (double)gl_y2 - viewport_h/2.0, 
                              z_coords[next]);
                }
                glEnd();
                glEnable(GL_LIGHTING); /* Re-enable lighting */
            }
        }
        
        /* Free heap-allocated arrays */
        if (npoints > MAX_STACK_POINTS) {
            if (x_coords) free(x_coords);
            if (y_coords) free(y_coords);
            if (z_coords) free(z_coords);
        }
    }
    
    /* Draw selected points (highlight) */
    for (int i = 0; i < core->selection.pointCount; i++) {
        int16_t idx = core->selection.selectedPoints[i];
        if (idx < 0) continue;
        
        CadPoint* pt = CadCore_GetPoint((CadCore*)core, idx);
        if (!pt) continue;
        
        int x, y;
        CadView_ProjectPoint(view, pt->pointx, pt->pointy, pt->pointz,
                            &x, &y, viewport_w, viewport_h);
        
        /* Draw small circle/square for selected point */
        RG_Color red = { 255, 0, 0, 255 }; /* Red for selected */
        int size = 4;
        rg_fill_rect(x - size, y - size, size * 2, size * 2, red);
    }
    
    /* Restore 2D projection after rendering (for wireframe or cleanup) */
    if (!view->wireframe) {
        /* Reset to 2D projection after 3D rendering */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, (double)viewport_w, (double)viewport_h, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
    
    /* Ensure scissor is still enabled to prevent rendering outside viewport */
    glEnable(GL_SCISSOR_TEST);
    int final_gl_y = win_h - (viewport_y + viewport_h);
    if (final_gl_y < 0) final_gl_y = 0;
    glScissor(viewport_x, final_gl_y, viewport_w, viewport_h);
}

