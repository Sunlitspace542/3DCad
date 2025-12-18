#define _CRT_SECURE_NO_WARNINGS

#include "cad_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_INDEX -1

/* ----------------------------------------------------------------------------
   Initialization and cleanup
   ---------------------------------------------------------------------------- */

void CadCore_Init(CadCore* core) {
    if (!core) return;
    
    memset(core, 0, sizeof(CadCore));
    CadFile_Init(&core->data);
    
    core->editMode = CAD_MODE_SELECT_POINT;
    core->selectModeFlag = 1;  /* Default to point selection */
    core->newPoint = INVALID_INDEX;
    core->newPolygon = INVALID_INDEX;
    core->rootPolygon = INVALID_INDEX;
    core->creatingPoint = INVALID_INDEX;
    core->firstPoint = INVALID_INDEX;
    
    /* Initialize selection arrays to invalid */
    for (int i = 0; i < CAD_MAX_POINTS; i++) {
        core->selection.selectedPoints[i] = INVALID_INDEX;
    }
    for (int i = 0; i < CAD_MAX_POLYGONS; i++) {
        core->selection.selectedPolygons[i] = INVALID_INDEX;
    }
}

void CadCore_Destroy(CadCore* core) {
    if (!core) return;
    CadCore_Clear(core);
}

void CadCore_Clear(CadCore* core) {
    if (!core) return;
    CadFile_Clear(&core->data);
    CadCore_ClearSelection(core);
    core->isDirty = 0;
    core->newPoint = INVALID_INDEX;
    core->newPolygon = INVALID_INDEX;
    core->rootPolygon = INVALID_INDEX;
    core->creatingPoint = INVALID_INDEX;
    core->firstPoint = INVALID_INDEX;
}

/* ----------------------------------------------------------------------------
   File operations
   ---------------------------------------------------------------------------- */

int CadCore_LoadFile(CadCore* core, const char* filename) {
    if (!core || !filename) return 0;
    
    CadCore_Clear(core);
    
    if (!CadFile_Load(filename, &core->data)) {
        return 0;
    }
    
    core->isDirty = 0;
    return 1;
}

int CadCore_SaveFile(CadCore* core, const char* filename) {
    if (!core || !filename) return 0;
    
    if (!CadFile_Save(filename, &core->data)) {
        return 0;
    }
    
    core->isDirty = 0;
    return 1;
}

/* ----------------------------------------------------------------------------
   Point operations
   ---------------------------------------------------------------------------- */

int16_t CadCore_AddPoint(CadCore* core, double x, double y, double z) {
    if (!core) return INVALID_INDEX;
    
    /* Find first free slot */
    for (int16_t i = 0; i < CAD_MAX_POINTS; i++) {
        if (core->data.points[i].flags == 0) {
            CadPoint* pt = &core->data.points[i];
            pt->flags = 1;
            pt->selectFlag = 0;
            pt->nextPoint = INVALID_INDEX;
            pt->pointx = x;
            pt->pointy = y;
            pt->pointz = z;
            
            if (i >= core->data.pointCount) {
                core->data.pointCount = i + 1;
            }
            
            core->newPoint = i;
            core->isDirty = 1;
            return i;
        }
    }
    
    return INVALID_INDEX; /* No free slots */
}

int CadCore_DeletePoint(CadCore* core, int16_t pointIndex) {
    if (!core || !CadCore_IsPointValid(core, pointIndex)) return 0;
    
    /* Mark as deleted (set flags to 0) */
    core->data.points[pointIndex].flags = 0;
    core->data.points[pointIndex].selectFlag = 0;
    
    /* Remove from selection if selected */
    CadCore_DeselectPoint(core, pointIndex);
    
    core->isDirty = 1;
    return 1;
}

CadPoint* CadCore_GetPoint(CadCore* core, int16_t index) {
    if (!core || !CadCore_IsPointValid(core, index)) return NULL;
    return &core->data.points[index];
}

int CadCore_IsPointValid(CadCore* core, int16_t index) {
    if (!core || index < 0 || index >= CAD_MAX_POINTS) return 0;
    return core->data.points[index].flags != 0;
}

/* ----------------------------------------------------------------------------
   Polygon operations
   ---------------------------------------------------------------------------- */

int16_t CadCore_AddPolygon(CadCore* core, int16_t firstPoint, uint8_t color, uint8_t npoints) {
    if (!core || !CadCore_IsPointValid(core, firstPoint) || npoints < 3) {
        return INVALID_INDEX;
    }
    
    /* Find first free slot */
    for (int16_t i = 0; i < CAD_MAX_POLYGONS; i++) {
        if (core->data.polygons[i].flags == 0) {
            CadPolygon* poly = &core->data.polygons[i];
            poly->flags = 1;
            poly->selectFlag = 0;
            poly->nextPolygon = INVALID_INDEX;
            poly->firstPoint = firstPoint;
            poly->animation = 0;
            poly->both = INVALID_INDEX;
            poly->side = 0;
            poly->color = color;
            poly->npoints = npoints;
            
            if (i >= core->data.polygonCount) {
                core->data.polygonCount = i + 1;
            }
            
            core->newPolygon = i;
            core->isDirty = 1;
            return i;
        }
    }
    
    return INVALID_INDEX; /* No free slots */
}

int CadCore_DeletePolygon(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return 0;
    
    /* Mark as deleted */
    core->data.polygons[polygonIndex].flags = 0;
    core->data.polygons[polygonIndex].selectFlag = 0;
    
    /* Remove from selection if selected */
    CadCore_DeselectPolygon(core, polygonIndex);
    
    core->isDirty = 1;
    return 1;
}

CadPolygon* CadCore_GetPolygon(CadCore* core, int16_t index) {
    if (!core || !CadCore_IsPolygonValid(core, index)) return NULL;
    return &core->data.polygons[index];
}

int CadCore_IsPolygonValid(CadCore* core, int16_t index) {
    if (!core || index < 0 || index >= CAD_MAX_POLYGONS) return 0;
    return core->data.polygons[index].flags != 0;
}

int CadCore_AddPointToPolygon(CadCore* core, int16_t polygonIndex, int16_t pointIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex) || !CadCore_IsPointValid(core, pointIndex)) {
        return 0;
    }
    
    CadPolygon* poly = &core->data.polygons[polygonIndex];
    
    /* Find the last point in the polygon's chain */
    int16_t current = poly->firstPoint;
    if (current == INVALID_INDEX) {
        /* First point */
        poly->firstPoint = pointIndex;
        poly->npoints = 1;
    } else {
        /* Traverse to end of chain */
        while (core->data.points[current].nextPoint != INVALID_INDEX) {
            current = core->data.points[current].nextPoint;
        }
        /* Link new point */
        core->data.points[current].nextPoint = pointIndex;
        poly->npoints++;
    }
    
    core->isDirty = 1;
    return 1;
}

/* ----------------------------------------------------------------------------
   Object operations
   ---------------------------------------------------------------------------- */

int16_t CadCore_AddObject(CadCore* core, int16_t parentObject, double ox, double oy, double oz) {
    if (!core) return INVALID_INDEX;
    
    /* Find first free slot */
    for (int16_t i = 0; i < CAD_MAX_OBJECTS; i++) {
        if (core->data.objects[i].flags == 0) {
            CadObject* obj = &core->data.objects[i];
            obj->flags = 1;
            obj->selectFlag = 0;
            obj->parentObject = parentObject;
            obj->nextBrother = INVALID_INDEX;
            obj->childObject = INVALID_INDEX;
            obj->firstPolygon = INVALID_INDEX;
            obj->offsetx = ox;
            obj->offsety = oy;
            obj->offsetz = oz;
            
            if (i >= core->data.objectCount) {
                core->data.objectCount = i + 1;
            }
            
            core->isDirty = 1;
            return i;
        }
    }
    
    return INVALID_INDEX;
}

int CadCore_DeleteObject(CadCore* core, int16_t objectIndex) {
    if (!core || !CadCore_IsObjectValid(core, objectIndex)) return 0;
    
    /* Mark as deleted */
    core->data.objects[objectIndex].flags = 0;
    core->data.objects[objectIndex].selectFlag = 0;
    
    core->isDirty = 1;
    return 1;
}

CadObject* CadCore_GetObject(CadCore* core, int16_t index) {
    if (!core || !CadCore_IsObjectValid(core, index)) return NULL;
    return &core->data.objects[index];
}

int CadCore_IsObjectValid(CadCore* core, int16_t index) {
    if (!core || index < 0 || index >= CAD_MAX_OBJECTS) return 0;
    return core->data.objects[index].flags != 0;
}

/* ----------------------------------------------------------------------------
   Selection operations
   ---------------------------------------------------------------------------- */

void CadCore_ClearSelection(CadCore* core) {
    if (!core) return;
    
    /* Clear all selection flags */
    for (int i = 0; i < core->data.pointCount; i++) {
        if (CadCore_IsPointValid(core, i)) {
            core->data.points[i].selectFlag = 0;
        }
    }
    for (int i = 0; i < core->data.polygonCount; i++) {
        if (CadCore_IsPolygonValid(core, i)) {
            core->data.polygons[i].selectFlag = 0;
        }
    }
    
    /* Clear selection arrays */
    for (int i = 0; i < CAD_MAX_POINTS; i++) {
        core->selection.selectedPoints[i] = INVALID_INDEX;
    }
    for (int i = 0; i < CAD_MAX_POLYGONS; i++) {
        core->selection.selectedPolygons[i] = INVALID_INDEX;
    }
    
    core->selection.pointCount = 0;
    core->selection.polygonCount = 0;
}

void CadCore_SelectPoint(CadCore* core, int16_t pointIndex) {
    if (!core || !CadCore_IsPointValid(core, pointIndex)) return;
    if (CadCore_IsPointSelected(core, pointIndex)) return; /* Already selected */
    
    core->data.points[pointIndex].selectFlag = 1;
    
    /* Add to selection array */
    if (core->selection.pointCount < CAD_MAX_POINTS) {
        core->selection.selectedPoints[core->selection.pointCount++] = pointIndex;
    }
}

void CadCore_SelectPolygon(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return;
    if (CadCore_IsPolygonSelected(core, polygonIndex)) return; /* Already selected */
    
    core->data.polygons[polygonIndex].selectFlag = 1;
    
    /* Add to selection array */
    if (core->selection.polygonCount < CAD_MAX_POLYGONS) {
        core->selection.selectedPolygons[core->selection.polygonCount++] = polygonIndex;
    }
}

void CadCore_DeselectPoint(CadCore* core, int16_t pointIndex) {
    if (!core || !CadCore_IsPointValid(core, pointIndex)) return;
    
    core->data.points[pointIndex].selectFlag = 0;
    
    /* Remove from selection array */
    for (int i = 0; i < core->selection.pointCount; i++) {
        if (core->selection.selectedPoints[i] == pointIndex) {
            /* Shift remaining elements */
            for (int j = i; j < core->selection.pointCount - 1; j++) {
                core->selection.selectedPoints[j] = core->selection.selectedPoints[j + 1];
            }
            core->selection.selectedPoints[core->selection.pointCount - 1] = INVALID_INDEX;
            core->selection.pointCount--;
            break;
        }
    }
}

void CadCore_DeselectPolygon(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return;
    
    core->data.polygons[polygonIndex].selectFlag = 0;
    
    /* Remove from selection array */
    for (int i = 0; i < core->selection.polygonCount; i++) {
        if (core->selection.selectedPolygons[i] == polygonIndex) {
            /* Shift remaining elements */
            for (int j = i; j < core->selection.polygonCount - 1; j++) {
                core->selection.selectedPolygons[j] = core->selection.selectedPolygons[j + 1];
            }
            core->selection.selectedPolygons[core->selection.polygonCount - 1] = INVALID_INDEX;
            core->selection.polygonCount--;
            break;
        }
    }
}

int CadCore_IsPointSelected(CadCore* core, int16_t pointIndex) {
    if (!core || !CadCore_IsPointValid(core, pointIndex)) return 0;
    return core->data.points[pointIndex].selectFlag != 0;
}

int CadCore_IsPolygonSelected(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return 0;
    return core->data.polygons[polygonIndex].selectFlag != 0;
}

void CadCore_SelectAll(CadCore* core) {
    if (!core) return;
    
    if (core->selectModeFlag == 1) {
        /* Select all points */
        for (int i = 0; i < core->data.pointCount; i++) {
            if (CadCore_IsPointValid(core, i)) {
                CadCore_SelectPoint(core, i);
            }
        }
    } else {
        /* Select all polygons */
        for (int i = 0; i < core->data.polygonCount; i++) {
            if (CadCore_IsPolygonValid(core, i)) {
                CadCore_SelectPolygon(core, i);
            }
        }
    }
}

/* ----------------------------------------------------------------------------
   Edit mode
   ---------------------------------------------------------------------------- */

void CadCore_SetEditMode(CadCore* core, CadEditMode mode) {
    if (!core) return;
    core->editMode = mode;
    
    /* Update selectModeFlag based on mode */
    if (mode == CAD_MODE_SELECT_POINT || mode == CAD_MODE_EDIT_POINT) {
        core->selectModeFlag = 1;
    } else if (mode == CAD_MODE_SELECT_POLYGON || mode == CAD_MODE_EDIT_POLYGON) {
        core->selectModeFlag = 0;
    }
}

CadEditMode CadCore_GetEditMode(CadCore* core) {
    if (!core) return CAD_MODE_SELECT_POINT;
    return core->editMode;
}

/* ----------------------------------------------------------------------------
   Linked list helpers
   ---------------------------------------------------------------------------- */

int16_t CadCore_GetFirstPointOfPolygon(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return INVALID_INDEX;
    return core->data.polygons[polygonIndex].firstPoint;
}

int16_t CadCore_GetNextPoint(CadCore* core, int16_t pointIndex) {
    if (!core || !CadCore_IsPointValid(core, pointIndex)) return INVALID_INDEX;
    return core->data.points[pointIndex].nextPoint;
}

int16_t CadCore_GetNextPolygon(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return INVALID_INDEX;
    return core->data.polygons[polygonIndex].nextPolygon;
}

int16_t CadCore_GetFirstPolygonOfObject(CadCore* core, int16_t objectIndex) {
    if (!core || !CadCore_IsObjectValid(core, objectIndex)) return INVALID_INDEX;
    return core->data.objects[objectIndex].firstPolygon;
}

/* ----------------------------------------------------------------------------
   Validation
   ---------------------------------------------------------------------------- */

int CadCore_ValidatePolygon(CadCore* core, int16_t polygonIndex) {
    if (!core || !CadCore_IsPolygonValid(core, polygonIndex)) return 0;
    
    CadPolygon* poly = &core->data.polygons[polygonIndex];
    
    /* Check minimum vertex count */
    if (poly->npoints < 3) return 0;
    
    /* Verify all points in chain are valid */
    int16_t current = poly->firstPoint;
    int count = 0;
    while (current != INVALID_INDEX && count < poly->npoints) {
        if (!CadCore_IsPointValid(core, current)) return 0;
        current = core->data.points[current].nextPoint;
        count++;
    }
    
    /* Check if we got the expected number of points */
    return count == poly->npoints;
}

int CadCore_ValidatePoint(CadCore* core, int16_t pointIndex) {
    if (!core || !CadCore_IsPointValid(core, pointIndex)) return 0;
    return 1; /* Point is valid if it exists */
}

/* ----------------------------------------------------------------------------
   Statistics
   ---------------------------------------------------------------------------- */

int CadCore_GetActivePointCount(CadCore* core) {
    if (!core) return 0;
    
    int count = 0;
    for (int i = 0; i < core->data.pointCount; i++) {
        if (CadCore_IsPointValid(core, i)) count++;
    }
    return count;
}

int CadCore_GetActivePolygonCount(CadCore* core) {
    if (!core) return 0;
    
    int count = 0;
    for (int i = 0; i < core->data.polygonCount; i++) {
        if (CadCore_IsPolygonValid(core, i)) count++;
    }
    return count;
}

int CadCore_GetActiveObjectCount(CadCore* core) {
    if (!core) return 0;
    
    int count = 0;
    for (int i = 0; i < core->data.objectCount; i++) {
        if (CadCore_IsObjectValid(core, i)) count++;
    }
    return count;
}

