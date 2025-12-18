#pragma once

/* ============================================================================
   cad_view.h
   CAD view rendering and camera system
   ============================================================================ */

#include "cad_core.h"
#include <stdint.h>

/* ----------------------------------------------------------------------------
   View types
   ---------------------------------------------------------------------------- */
typedef enum {
    CAD_VIEW_TOP = 0,
    CAD_VIEW_FRONT = 1,
    CAD_VIEW_RIGHT = 2,
    CAD_VIEW_3D = 3
} CadViewType;

/* ----------------------------------------------------------------------------
   View state
   ---------------------------------------------------------------------------- */
typedef struct {
    CadViewType type;
    double zoom;           /* Zoom factor */
    double pan_x, pan_y;   /* Pan offset */
    double rot_x, rot_y;   /* Rotation (for 3D view) */
    int wireframe;         /* 1 = wireframe, 0 = solid */
} CadView;

/* ----------------------------------------------------------------------------
   View initialization
   ---------------------------------------------------------------------------- */
void CadView_Init(CadView* view, CadViewType type);
void CadView_Reset(CadView* view);

/* ----------------------------------------------------------------------------
   View transformations
   ---------------------------------------------------------------------------- */
void CadView_SetZoom(CadView* view, double zoom);
void CadView_Pan(CadView* view, double dx, double dy);
void CadView_Rotate(CadView* view, double dx, double dy);

/* ----------------------------------------------------------------------------
   3D to 2D projection
   ---------------------------------------------------------------------------- */
void CadView_ProjectPoint(const CadView* view, double x, double y, double z, 
                         int* out_x, int* out_y, int viewport_w, int viewport_h);

/* ----------------------------------------------------------------------------
   Rendering
   ---------------------------------------------------------------------------- */
void CadView_Render(const CadView* view, const CadCore* core, 
                    int viewport_x, int viewport_y, int viewport_w, int viewport_h, int win_h);

