// runtime/jet_redraw.h
#ifndef JET_REDRAW_H
#define JET_REDRAW_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Repaint only the parts of the screen that changed. On a screen that mostly
// stays the same, a title screen, a card game, a board, clearing and redrawing
// everything each frame is wasted work. This module divides the screen into a
// grid of 32x32 cells and remembers which cells are dirty, meaning something
// there has changed. Each frame you tell it where every moving thing was and
// where it is now; it calls your repaint function once for each dirty stretch
// so you can put the background back there; then you clear it for the next
// frame. Call jet_redraw_init once in _init, and then call jet_redraw_add,
// jet_redraw_resolve and jet_redraw_clear every frame; the tracker follows a
// change of resolution by itself. This is an opt-in module: include
// jet_redraw.h yourself.
//
// Complete dirty-rectangle example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_redraw.h"
//
//   enum { ACTOR_SIZE = 16, BACKGROUND_COLOUR = 1, ACTOR_COLOUR = 12 };
//   static JetRedrawTracker dirty;
//   static float actor_x = 32.0f;
//   static bool first_frame = true;
//
//   static void restore_background(int x, int y, int w, int h, void* user) {
//       (void)user;
//       jet_rectfill(x, y, w, h, BACKGROUND_COLOUR);
//   }
//
//   void _init(void) {
//       jet_redraw_init(&dirty);
//   }
//
//   void _update(float dt) {
//       const float speed = 120.0f;
//       const int old_x = (int)actor_x;
//       if (btn(BTN_LEFT))  actor_x -= speed * dt;
//       if (btn(BTN_RIGHT)) actor_x += speed * dt;
//       actor_x = jet_clampf(actor_x, 0.0f, (float)(JET_WIDTH - ACTOR_SIZE));
//       jet_redraw_add(&dirty, old_x, JET_HEIGHT / 2, ACTOR_SIZE, ACTOR_SIZE);
//       jet_redraw_add(&dirty, (int)actor_x, JET_HEIGHT / 2, ACTOR_SIZE, ACTOR_SIZE);
//   }
//
//   void _draw(void) {
//       if (first_frame) { jet_cls(BACKGROUND_COLOUR); first_frame = false; }
//       jet_redraw_resolve(&dirty, restore_background, NULL);
//       jet_redraw_clear(&dirty);
//       jet_rectfill((int)actor_x, JET_HEIGHT / 2, ACTOR_SIZE, ACTOR_SIZE, ACTOR_COLOUR);
//   }
//
//   void _end(void) {}

// The side of one tracking cell, in screen pixels. The screen is divided into
// a grid of these cells, and the tracker works in whole cells, not pixels, so
// one dirty cell repaints a 32x32 block. 32 is a good balance: small enough
// that most of a still screen is skipped, and large enough that the grid stays
// tiny and the scan each frame stays cheap.
#define JET_REDRAW_CELL_SIZE 32

// The edge, in pixels, of the square display the grid is sized for. It covers
// 4K-class displays. The grid is reserved once at this largest size, so the
// tracker never has to reserve again if the resolution changes.
#define JET_REDRAW_MAX_DISPLAY_DIM 4096

// The grid's width and height, in cells, for the largest display supported.
// They bound the one-time reservation from WRAM.
#define JET_REDRAW_MAX_W ((JET_REDRAW_MAX_DISPLAY_DIM + JET_REDRAW_CELL_SIZE - 1) / JET_REDRAW_CELL_SIZE)
#define JET_REDRAW_MAX_H ((JET_REDRAW_MAX_DISPLAY_DIM + JET_REDRAW_CELL_SIZE - 1) / JET_REDRAW_CELL_SIZE)

// How many cells the grid holds, which is also the bytes jet_redraw_init
// reserves: 16 KiB. The cells are laid out at the live screen's width, so a
// screen of any shape is tracked whole while it has no more cells than this,
// which every screen the display allows does short of a strip more than
// 262,144 pixels long.
#define JET_REDRAW_MAX_CELLS (JET_REDRAW_MAX_W * JET_REDRAW_MAX_H)

/**
 * Struct: JetRedrawTracker
 * Summary: The grid of dirty cells for one frame. Each cell covers a 32x32 block of screen pixels and is either clean (0) or dirty (1). With it a cartridge repaints only the parts of the screen that actually changed, instead of clearing and redrawing everything, which is the trick behind very high frame rates on a screen that mostly stays still.
 * Fields:
 *   - cells: The grid, one byte per cell, reserved once from WRAM, the cartridge's working memory, by jet_redraw_init. It holds JET_REDRAW_MAX_CELLS cells whatever the resolution.
 *   - grid_w: How many cells wide the grid is: the screen width divided by the cell size, rounded up.
 *   - grid_h: How many cells tall the grid is: the screen height divided by the cell size, rounded up.
 *   - dirty_first_row: The first row of cells that may hold a dirty cell.
 *   - dirty_end_row: One past the last row of cells that may hold a dirty cell. When it is not above dirty_first_row, no cell is dirty.
 * Include: jet_redraw.h
 * Notes: After jet_redraw_init the tracker never reserves memory again; add, clear and resolve all work in the same fixed buffer. grid_w and grid_h follow the screen: add and resolve fit them to a resolution that has changed. The two row fields are the span of rows jet_redraw_add has marked since the last clear, so resolve and clear visit only those rows, and a frame that marked nothing costs them a compare rather than a walk of the whole grid.
 * Example:
 *   static JetRedrawTracker tracker;
 *   jet_redraw_init(&tracker);
 */
typedef struct {
    uint8_t* cells;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t dirty_first_row;
    uint32_t dirty_end_row;
} JetRedrawTracker;

/**
 * Function: jet_make_redraw_tracker
 * Summary: Gives you an empty tracker that is safe to store. You must still call jet_redraw_init on it before you use it.
 * Parameters:
 *   - None.
 * Returns: A JetRedrawTracker with every field zero, handed back by value.
 * Include: jet_redraw.h
 * Notes: The tracker owns no memory until jet_redraw_init reserves its grid of cells from WRAM.
 * Example:
 *   JetRedrawTracker tracker = jet_make_redraw_tracker();
 */
static inline JetRedrawTracker jet_make_redraw_tracker(void) {
    JetRedrawTracker t;
    t.cells = NULL;
    t.grid_w = 0;
    t.grid_h = 0;
    t.dirty_first_row = 0;
    t.dirty_end_row = 0;
    return t;
}

/**
 * Function: jet_redraw_init
 * Summary: Reserves the tracker's grid once, big enough for a 4096x4096 screen, fits it to the current resolution, and marks every cell clean.
 * Parameters:
 *   - tracker: The tracker to set up.
 * Returns: Nothing.
 * Include: jet_redraw.h
 * Notes: Call it once from _init. jet_memory_reserve works in _init only and there is no way to give memory back, so calling this again would use up more WRAM each time. The grid takes JET_REDRAW_MAX_CELLS bytes (16 KiB) whatever the resolution, so a resolution set before or after this call is followed without another reservation: once the screen has changed size, the next jet_redraw_add or jet_redraw_resolve marks the whole new screen dirty, since nothing drawn at the old size describes it. If WRAM has run out, the tracker stays empty and every other redraw call does nothing.
 *
 *        It does not redraw anything, and it does not free a grid the tracker already holds in WRAM.
 * Example:
 *   jet_redraw_init(&tracker);
 */
void jet_redraw_init(JetRedrawTracker* tracker);

/**
 * Function: jet_redraw_add
 * Summary: Marks a rectangle of the screen as dirty, so that jet_redraw_resolve will later ask you to repaint it. Positions are in screen pixels, the same space your finished frame is drawn in. The rectangle is widened to whole cells and trimmed to the screen; a rectangle wholly off the screen is ignored.
 * Parameters:
 *   - tracker: The tracker to mark.
 *   - x: The rectangle's left edge, in screen pixels.
 *   - y: The rectangle's top edge, in screen pixels.
 *   - w: The rectangle's width in pixels.
 *   - h: The rectangle's height in pixels.
 * Returns: Nothing.
 * Include: jet_redraw.h
 * Notes: It works in screen pixels only and does not apply the camera. If your world scrolls, take the camera's offset off yourself before calling: mark where the object appears on the screen, not where it lives in the world. Add a few pixels of margin around a moving object, so its shadow and any soft edge are covered too. If the screen has changed size since the tracker last looked, every cell of the new screen is marked dirty first.
 * Example:
 *   jet_redraw_add(&tracker, player_screen_x, player_screen_y, 32, 32);
 */
void jet_redraw_add(JetRedrawTracker* tracker, int x, int y, int w, int h);

/**
 * Function: jet_redraw_clear
 * Summary: Marks every cell clean, ready for the next frame. Call it once a frame, after jet_redraw_resolve has finished.
 * Parameters:
 *   - tracker: The tracker to reset.
 * Returns: Nothing.
 * Include: jet_redraw.h
 * Notes: It only resets the dirty marks. It draws nothing, and it does not free the tracker's grid in WRAM. It touches only the rows marked since the last clear, so after a frame that marked nothing it does no work at all.
 * Example:
 *   jet_redraw_clear(&tracker);
 */
void jet_redraw_clear(JetRedrawTracker* tracker);

/**
 * Callback: JetRedrawCallback
 * Summary: Your function that repaints one dirty stretch of the screen. jet_redraw_resolve calls it once for each run of dirty cells; draw the clean background, and anything that belongs there, for exactly that rectangle.
 * Parameters:
 *   - x: The left edge of the stretch to repaint, in screen pixels.
 *   - y: The top edge of the stretch to repaint, in screen pixels.
 *   - w: The width in pixels, already trimmed to the screen.
 *   - h: The height in pixels, already trimmed to the screen.
 *   - user_data: The pointer you gave jet_redraw_resolve, passed on unchanged.
 * Returns: Nothing.
 * Include: jet_redraw.h
 * Notes: Do not keep user_data after the call unless your cartridge owns what it points to. Inside the function you may use the normal drawing calls on the rectangle you are given, which is in screen pixels.
 * Example:
 *   void restore_background(int x, int y, int w, int h, void* user_data) {
 *       (void)user_data;
 *       jet_rectfill(x, y, w, h, 0);
 *   }
 */
typedef void (*JetRedrawCallback)(int x, int y, int w, int h, void* user_data);

/**
 * Function: jet_redraw_resolve
 * Summary: Walks the grid and, for each run of dirty cells side by side in a row, calls your repaint function once with the screen rectangle to restore. Joining neighbouring cells into one run keeps the number of calls, and so the number of draws, small.
 * Parameters:
 *   - tracker: The tracker to walk.
 *   - restorer: Your repaint function. It is called once for each dirty run.
 *   - user_data: A pointer of your own, passed on to every restorer call. Pass NULL if you have no use for it.
 * Returns: Nothing.
 * Include: jet_redraw.h
 * Notes: Runs are joined along a row only, never down a column, so a tall dirty block arrives as one call for each row of cells. Only the rows marked since the last clear are walked, so a frame that marked nothing costs a compare. Every rectangle lies on the current screen. If the screen has changed size since the tracker last looked, the whole new screen is repainted. This does not clear the grid; call jet_redraw_clear afterwards to make it ready for the next frame.
 * Example:
 *   jet_redraw_resolve(&tracker, my_bg_drawer, NULL);
 *   jet_redraw_clear(&tracker);
 */
void jet_redraw_resolve(JetRedrawTracker* tracker, JetRedrawCallback restorer, void* user_data);

#endif // JET_REDRAW_H
