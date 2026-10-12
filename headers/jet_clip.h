// runtime/jet_clip.h
#ifndef JET_CLIP_H
#define JET_CLIP_H

#include "jet_core.h"
#include "jet_graphics.h"

// Keep drawing inside one rectangle of the screen. Useful for a viewport, a
// split screen, a minimap, or a panel with its own scrolling contents. Set the
// rectangle, draw what belongs inside it, then reset it so later drawing can
// use the whole screen again. The camera still applies inside the rectangle.
//
// Complete viewport example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_clip.h"
//
//   enum { VIEW_X = 32, VIEW_Y = 32, VIEW_W = 320, VIEW_H = 180 };
//
//   void _init(void) {}
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_clip(VIEW_X, VIEW_Y, VIEW_W, VIEW_H);
//       jet_circfill(VIEW_X + VIEW_W, VIEW_Y + VIEW_H / 2, VIEW_H / 2, 12);
//       jet_clip_reset();
//       jet_rect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, 15);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_clip
 * Summary: Limits all drawing to one rectangle of the screen, until you call jet_clip_reset.
 * Parameters:
 *   - x: The left edge, in screen pixels.
 *   - y: The top edge, in screen pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 * Returns: Nothing.
 * Include: jet_clip.h
 * Notes: The rectangle is trimmed to the screen. The camera still moves what you draw inside it; the rectangle itself is fixed to the screen. A width or height of zero or less makes an empty rectangle, and nothing at all draws until you call jet_clip_reset or set a rectangle with some size.
 * Example:
 *   jet_clip(32, 32, 320, 180);
 *   jet_map_draw_view(&map, cam_x, cam_y, 0, true);
 *   jet_clip_reset();
 */
static inline void jet_clip(int x, int y, int w, int h) {
    _jet_clip_x0 = x;
    _jet_clip_y0 = y;
    _jet_clip_x1 = x + w - 1;
    _jet_clip_y1 = y + h - 1;

    // Clamp the rectangle into the screen. The low edges never go below 0 and the
    // high edges never exceed the last pixel, so the primitives always see bounds
    // inside VRAM; a non-positive w/h leaves x1 < x0 (an empty clip), which every
    // primitive treats as "draw nothing".
    if (_jet_clip_x0 < 0) _jet_clip_x0 = 0;
    if (_jet_clip_y0 < 0) _jet_clip_y0 = 0;
    if (_jet_clip_x1 >= JET_WIDTH) _jet_clip_x1 = JET_WIDTH - 1;
    if (_jet_clip_y1 >= JET_HEIGHT) _jet_clip_y1 = JET_HEIGHT - 1;
}

/**
 * Function: jet_clip_reset
 * Summary: Lets drawing use the whole screen again.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_clip.h
 * Notes: Call it as soon as you have finished drawing the viewport, panel, minimap, or split-screen half, so nothing later is clipped by mistake.
 * Example:
 *   jet_clip_reset();
 *   print("ready", 8, 8, 7);
 */
static inline void jet_clip_reset(void) {
    _jet_clip_x0 = 0;
    _jet_clip_y0 = 0;
    _jet_clip_x1 = JET_WIDTH - 1;
    _jet_clip_y1 = JET_HEIGHT - 1;
}

#endif // JET_CLIP_H
