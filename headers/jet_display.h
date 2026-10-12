// runtime/jet_display.h
#ifndef JET_DISPLAY_H
#define JET_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// How big the screen is, which way round it is, and how finished frames are
// handed to the monitor. Jet64 starts at 1280x720, landscape. If your cart
// wants a different size, set it once near the start of _init, before you
// reserve any WRAM sized to the screen. When you draw, ask
// jet_get_screen_width and jet_get_screen_height for the size rather than
// writing the numbers into your code, so your drawing stays right when the
// orientation changes. The buffering mode changes how smooth the picture is
// and how soon a press shows on screen; it never changes where a pixel lands.
//
// Complete low-resolution-canvas example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { SCREEN_WIDTH = 640, SCREEN_HEIGHT = 360 };
//
//   void _init(void) {
//       jet_display_set_resolution(SCREEN_WIDTH, SCREEN_HEIGHT);
//       jet_set_buffering_mode(JET_BUFFERING_TRIPLE);
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_rectfill(0, JET_HEIGHT / 2, JET_WIDTH, JET_HEIGHT / 2, 3);
//       jet_print("640 x 360", 16, 16, 15);
//   }
//
//   void _end(void) {}

// The two buffering modes jet_set_buffering_mode accepts.
#define JET_BUFFERING_DOUBLE 2
#define JET_BUFFERING_TRIPLE 3

// The two orientations jet_set_orientation accepts.
#define JET_ORIENTATION_LANDSCAPE 0
#define JET_ORIENTATION_PORTRAIT 1

/**
 * Function: jet_display_set_resolution
 * Summary: Sets the size of the screen your cart draws on, in pixels. Call it once from _init if you want something other than the default 1280x720.
 * Parameters:
 *   - width: The width you want, in pixels.
 *   - height: The height you want, in pixels.
 * Returns: Nothing.
 * Include: jet_display.h
 * Notes: All of the screen buffers together must fit in 32 MiB, so width * height * 3 must not be more than that; a 4K screen with triple buffering still fits. A width or height of zero, or a size that is too big, is refused: the screen goes back to 1280x720 and a warning is printed. A size it accepts prints nothing, so a cart may call it again whenever its window settles at a new size. Set the size before you reserve anything sized to the screen, such as a still-screen canvas (jet_still_init).
 * Example:
 *   jet_display_set_resolution(640, 480);
 */
void jet_display_set_resolution(uint32_t width, uint32_t height);

/**
 * Function: jet_set_buffering_mode
 * Summary: Chooses double or triple buffering: whether there are two or three screen buffers, so one can be shown while the next is drawn. Triple gives a smoother picture; double shows a press on screen sooner.
 * Parameters:
 *   - mode: JET_BUFFERING_DOUBLE or JET_BUFFERING_TRIPLE.
 * Returns: Nothing.
 * Include: jet_display.h
 * Notes: Any other value is ignored and the mode stays as it was. Call it from _init, before the first frame is drawn.
 * Example:
 *   jet_set_buffering_mode(JET_BUFFERING_TRIPLE);
 */
void jet_set_buffering_mode(int mode);

/**
 * Function: jet_set_orientation
 * Summary: Turns the screen to landscape (wider than it is tall) or portrait (taller than it is wide). The two numbers of your resolution stay the same; they swap places when they need to.
 * Parameters:
 *   - orientation: JET_ORIENTATION_LANDSCAPE or JET_ORIENTATION_PORTRAIT.
 * Returns: Nothing.
 * Include: jet_display.h
 * Notes: Any other value is ignored. After this call, jet_get_screen_width and jet_get_screen_height give the new sizes, swapped if they had to be.
 * Example:
 *   jet_set_orientation(JET_ORIENTATION_PORTRAIT);
 */
void jet_set_orientation(int orientation);

/**
 * Function: jet_get_screen_width
 * Summary: Tells you how wide the screen is right now, in pixels. Use it instead of writing the number into your code, so your cart still works after a change of resolution or orientation.
 * Parameters:
 *   - None.
 * Returns: The screen's width in pixels.
 * Include: jet_display.h
 * Notes: The answer follows the latest jet_display_set_resolution or jet_set_orientation call.
 * Example:
 *   uint32_t w = jet_get_screen_width();
 */
uint32_t jet_get_screen_width(void);

/**
 * Function: jet_get_screen_height
 * Summary: Tells you how tall the screen is right now, in pixels. Use it instead of writing the number into your code, so your cart still works after a change of resolution or orientation.
 * Parameters:
 *   - None.
 * Returns: The screen's height in pixels.
 * Include: jet_display.h
 * Notes: The answer follows the latest jet_display_set_resolution or jet_set_orientation call.
 * Example:
 *   uint32_t h = jet_get_screen_height();
 */
uint32_t jet_get_screen_height(void);

/**
 * Function: jet_get_screen_resolution
 * Summary: Tells you the screen's width and height in one call.
 * Parameters:
 *   - width: Where to put the width, or NULL if you do not want it.
 *   - height: Where to put the height, or NULL if you do not want it.
 * Returns: Nothing.
 * Include: jet_display.h
 * Notes: Pass NULL for whichever of the two you do not need.
 * Example:
 *   uint32_t w, h;
 *   jet_get_screen_resolution(&w, &h);
 */
void jet_get_screen_resolution(uint32_t* width, uint32_t* height);

#endif // JET_DISPLAY_H
