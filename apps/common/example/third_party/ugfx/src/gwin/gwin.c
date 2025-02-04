/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

/**
 * @file	src/gwin/gwin.c
 * @brief	GWIN sub-system code
 */

#include "../../gfx.h"

#if GFX_USE_GWIN

#include "gwin_class.h"

#include <string.h>

/*-----------------------------------------------
 * Data
 *-----------------------------------------------*/

static const gwinVMT basegwinVMT = {
    "GWIN",					// The classname
    sizeof(GWindowObject),	// The object size
    0,						// The destroy routine
    0,						// The redraw routine
    0,						// The after-clear routine
};

static gColor	defaultFgColor = GFX_WHITE;
static gColor	defaultBgColor = GFX_BLACK;
#if GDISP_NEED_TEXT
static gFont	defaultFont;
#endif

/* These init functions are defined by each module but not published */
extern void _gwmInit(void);
extern void _gwmDeinit(void);
#if GWIN_NEED_WIDGET
extern void _gwidgetInit(void);
extern void _gwidgetDeinit(void);
#endif
#if GWIN_NEED_CONTAINERS
extern void _gcontainerInit(void);
extern void _gcontainerDeinit(void);
#endif

/*-----------------------------------------------
 * Helper Routines
 *-----------------------------------------------*/

/*-----------------------------------------------
 * Class Routines
 *-----------------------------------------------*/

void _gwinInit(void)
{
    _gwmInit();

#if GWIN_NEED_WIDGET
    _gwidgetInit();
#endif

#if GWIN_NEED_CONTAINERS
    _gcontainerInit();
#endif
}

void _gwinDeinit(void)
{
#if GWIN_NEED_CONTAINERS
    _gcontainerDeinit();
#endif

#if GWIN_NEED_WIDGET
    _gwidgetDeinit();
#endif

    _gwmDeinit();
}

// Internal routine for use by GWIN components only
// Initialise a window creating it dynamically if required.
GHandle _gwindowCreate(GDisplay *g, GWindowObject *pgw, const GWindowInit *pInit, const gwinVMT *vmt, gU32 flags)
{
    // Allocate the structure if necessary
    if (!pgw) {
        if (!(pgw = gfxAlloc(vmt->size))) {
            return 0;
        }
        pgw->flags = flags | GWIN_FLG_DYNAMIC;
    } else {
        pgw->flags = flags;
    }

    // Initialise all basic fields
    pgw->display = g;
    pgw->vmt = vmt;
    pgw->color = defaultFgColor;
    pgw->bgcolor = defaultBgColor;
#if GDISP_NEED_TEXT
    pgw->font = defaultFont;
#endif

    // Make sure we don't create nasty problems for ourselves
    if (vmt->size > sizeof(GWindowObject)) {
        memset(pgw + 1, 0, vmt->size - sizeof(GWindowObject));
    }

    if (!_gwinWMAdd(pgw, pInit)) {
        if ((pgw->flags & GWIN_FLG_DYNAMIC)) {
            gfxFree(pgw);
        }
        return 0;
    }

    return (GHandle)pgw;
}

// Internal routine for use by GWIN components only
void _gwinDestroy(GHandle gh, GRedrawMethod how)
{
    if (!gh) {
        return;
    }

    // Make the window invisible
    gwinSetVisible(gh, gFalse);

    // Make sure it is flushed first - must be REDRAW_WAIT or REDRAW_INSESSION
    _gwinFlushRedraws(how);

#if GWIN_NEED_CONTAINERS
    // Notify the parent it is about to be deleted
    if (gh->parent && ((gcontainerVMT *)gh->parent->vmt)->NotifyDelete) {
        ((gcontainerVMT *)gh->parent->vmt)->NotifyDelete(gh->parent, gh);
    }
#endif

    // Remove from the window manager
#if GWIN_NEED_WINDOWMANAGER
    _GWINwm->vmt->Delete(gh);
#endif

    // Class destroy routine
    if (gh->vmt->Destroy) {
        gh->vmt->Destroy(gh);
    }

    // Clean up the structure
    if (gh->flags & GWIN_FLG_DYNAMIC) {
        gh->flags = 0;							// To be sure, to be sure
        gfxFree((void *)gh);
    } else {
        gh->flags = 0;    // To be sure, to be sure
    }
}

/*-----------------------------------------------
 * Routines that affect all windows
 *-----------------------------------------------*/

void gwinClearInit(GWindowInit *pwi)
{
    char		*p;
    unsigned	len;

    for (p = (char *)pwi, len = sizeof(GWindowInit); len; len--) {
        *p++ = 0;
    }
}

void gwinSetDefaultColor(gColor clr)
{
    defaultFgColor = clr;
}

gColor gwinGetDefaultColor(void)
{
    return defaultFgColor;
}

void gwinSetDefaultBgColor(gColor bgclr)
{
    defaultBgColor = bgclr;
}

gColor gwinGetDefaultBgColor(void)
{
    return defaultBgColor;
}

#if GDISP_NEED_TEXT
void gwinSetDefaultFont(gFont font)
{
    defaultFont = font;
}

gFont gwinGetDefaultFont(void)
{
    return defaultFont;
}
#endif

/*-----------------------------------------------
 * The GWindow Routines
 *-----------------------------------------------*/

GHandle gwinGWindowCreate(GDisplay *g, GWindowObject *pgw, const GWindowInit *pInit)
{
    if (!(pgw = _gwindowCreate(g, pgw, pInit, &basegwinVMT, 0))) {
        return 0;
    }

    gwinSetVisible(pgw, pInit->show);
    _gwinFlushRedraws(REDRAW_WAIT);

    return pgw;
}

void gwinDestroy(GHandle gh)
{
    _gwinDestroy(gh, REDRAW_WAIT);
}

const char *gwinGetClassName(GHandle gh)
{
    return gh->vmt->classname;
}

gBool gwinGetVisible(GHandle gh)
{
    return (gh->flags & GWIN_FLG_SYSVISIBLE) ? gTrue : gFalse;
}

gBool gwinGetEnabled(GHandle gh)
{
    return (gh->flags & GWIN_FLG_SYSENABLED) ? gTrue : gFalse;
}

#if GDISP_NEED_TEXT
void gwinSetFont(GHandle gh, gFont font)
{
    gh->font = font;
}
#endif

void gwinClear(GHandle gh)
{
    /*
     * Don't render anything when the window is not visible but
     * still call the AfterClear() routine as some widgets will
     * need this to clear internal buffers or similar
     */
    if (_gwinDrawStart(gh)) {
        gdispGFillArea(gh->display, gh->x, gh->y, gh->width, gh->height, gh->bgcolor);
        _gwinDrawEnd(gh);
    }
    if (gh->vmt->AfterClear) {
        gh->vmt->AfterClear(gh);
    }
}

void gwinDrawPixel(GHandle gh, gCoord x, gCoord y)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawPixel(gh->display, gh->x + x, gh->y + y, gh->color);
    _gwinDrawEnd(gh);
}

void gwinDrawLine(GHandle gh, gCoord x0, gCoord y0, gCoord x1, gCoord y1)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawLine(gh->display, gh->x + x0, gh->y + y0, gh->x + x1, gh->y + y1, gh->color);
    _gwinDrawEnd(gh);
}

void gwinDrawBox(GHandle gh, gCoord x, gCoord y, gCoord cx, gCoord cy)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawBox(gh->display, gh->x + x, gh->y + y, cx, cy, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillArea(GHandle gh, gCoord x, gCoord y, gCoord cx, gCoord cy)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillArea(gh->display, gh->x + x, gh->y + y, cx, cy, gh->color);
    _gwinDrawEnd(gh);
}

void gwinBlitArea(GHandle gh, gCoord x, gCoord y, gCoord cx, gCoord cy, gCoord srcx, gCoord srcy, gCoord srccx, const gPixel *buffer)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGBlitArea(gh->display, gh->x + x, gh->y + y, cx, cy, srcx, srcy, srccx, buffer);
    _gwinDrawEnd(gh);
}

#if GDISP_NEED_CIRCLE
void gwinDrawCircle(GHandle gh, gCoord x, gCoord y, gCoord radius)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawCircle(gh->display, gh->x + x, gh->y + y, radius, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillCircle(GHandle gh, gCoord x, gCoord y, gCoord radius)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillCircle(gh->display, gh->x + x, gh->y + y, radius, gh->color);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_DUALCIRCLE
void gwinFillDualCircle(GHandle gh, gCoord x, gCoord y, gCoord radius1, gCoord radius2)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillDualCircle(gh->display, gh->x + x, gh->y + y, radius1, gh->bgcolor, radius2, gh->color);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_ELLIPSE
void gwinDrawEllipse(GHandle gh, gCoord x, gCoord y, gCoord a, gCoord b)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawEllipse(gh->display, gh->x + x, gh->y + y, a, b, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillEllipse(GHandle gh, gCoord x, gCoord y, gCoord a, gCoord b)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillEllipse(gh->display, gh->x + x, gh->y + y, a, b, gh->color);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_ARC
void gwinDrawArc(GHandle gh, gCoord x, gCoord y, gCoord radius, gCoord startangle, gCoord endangle)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawArc(gh->display, gh->x + x, gh->y + y, radius, startangle, endangle, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillArc(GHandle gh, gCoord x, gCoord y, gCoord radius, gCoord startangle, gCoord endangle)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillArc(gh->display, gh->x + x, gh->y + y, radius, startangle, endangle, gh->color);
    _gwinDrawEnd(gh);
}

void gwinDrawThickArc(GHandle gh, gCoord x, gCoord y, gCoord startradius, gCoord endradius, gCoord startangle, gCoord endangle)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawThickArc(gh->display, gh->x + x, gh->y + y, startradius, endradius, startangle, endangle, gh->color);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_ARCSECTORS
void gwinDrawArcSectors(GHandle gh, gCoord x, gCoord y, gCoord radius, gU8 sectors)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawArcSectors(gh->display, gh->x + x, gh->y + y, radius, sectors, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillArcSectors(GHandle gh, gCoord x, gCoord y, gCoord radius, gU8 sectors)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillArcSectors(gh->display, gh->x + x, gh->y + y, radius, sectors, gh->color);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_PIXELREAD
gColor gwinGetPixelColor(GHandle gh, gCoord x, gCoord y)
{
    if (!_gwinDrawStart(gh)) {
        return (gColor)0;
    }
    return gdispGGetPixelColor(gh->display, gh->x + x, gh->y + y);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_TEXT
void gwinDrawChar(GHandle gh, gCoord x, gCoord y, char c)
{
    if (!gh->font || !_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawChar(gh->display, gh->x + x, gh->y + y, c, gh->font, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillChar(GHandle gh, gCoord x, gCoord y, char c)
{
    if (!gh->font || !_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillChar(gh->display, gh->x + x, gh->y + y, c, gh->font, gh->color, gh->bgcolor);
    _gwinDrawEnd(gh);
}

void gwinDrawString(GHandle gh, gCoord x, gCoord y, const char *str)
{
    if (!gh->font || !_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawString(gh->display, gh->x + x, gh->y + y, str, gh->font, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillString(GHandle gh, gCoord x, gCoord y, const char *str)
{
    if (!gh->font || !_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillString(gh->display, gh->x + x, gh->y + y, str, gh->font, gh->color, gh->bgcolor);
    _gwinDrawEnd(gh);
}

void gwinDrawStringBox(GHandle gh, gCoord x, gCoord y, gCoord cx, gCoord cy, const char *str, gJustify justify)
{
    if (!gh->font || !_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawStringBox(gh->display, gh->x + x, gh->y + y, cx, cy, str, gh->font, gh->color, justify);
    _gwinDrawEnd(gh);
}

void gwinFillStringBox(GHandle gh, gCoord x, gCoord y, gCoord cx, gCoord cy, const char *str, gJustify justify)
{
    if (!gh->font || !_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillStringBox(gh->display, gh->x + x, gh->y + y, cx, cy, str, gh->font, gh->color, gh->bgcolor, justify);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_CONVEX_POLYGON
void gwinDrawPoly(GHandle gh, gCoord tx, gCoord ty, const gPoint *pntarray, unsigned cnt)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawPoly(gh->display, tx + gh->x, ty + gh->y, pntarray, cnt, gh->color);
    _gwinDrawEnd(gh);
}

void gwinFillConvexPoly(GHandle gh, gCoord tx, gCoord ty, const gPoint *pntarray, unsigned cnt)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGFillConvexPoly(gh->display, tx + gh->x, ty + gh->y, pntarray, cnt, gh->color);
    _gwinDrawEnd(gh);
}
void gwinDrawThickLine(GHandle gh, gCoord x0, gCoord y0, gCoord x1, gCoord y1, gCoord width, gBool round)
{
    if (!_gwinDrawStart(gh)) {
        return;
    }
    gdispGDrawThickLine(gh->display, gh->x + x0, gh->y + y0, gh->x + x1, gh->y + y1, gh->color, width, round);
    _gwinDrawEnd(gh);
}
#endif

#if GDISP_NEED_IMAGE
gdispImageError gwinDrawImage(GHandle gh, gdispImage *img, gCoord x, gCoord y, gCoord cx, gCoord cy, gCoord sx, gCoord sy)
{
    gdispImageError		ret;

    if (!_gwinDrawStart(gh)) {
        return GDISP_IMAGE_ERR_OK;
    }
    ret = gdispGImageDraw(gh->display, img, gh->x + x, gh->y + y, cx, cy, sx, sy);
    _gwinDrawEnd(gh);
    return ret;
}
#endif

#endif /* GFX_USE_GWIN */
/** @} */

