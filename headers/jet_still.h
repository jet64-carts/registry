// runtime/jet_still.h
#ifndef JET_STILL_H
#define JET_STILL_H

#include "jet_core.h"   // IWYU pragma: keep
#include "jet_redraw.h" // IWYU pragma: keep

// Keep a still screen instead of drawing it again. On a chess board, a title
// screen, a card table, a paused game behind an open page, hardly anything is
// different between one frame and the next: the same board, the same panel of
// text, the same background. Painting all of it a hundred and twenty times a
// second is work nobody asked for.
//
// So the screen is painted ONCE into a canvas kept in working memory, and
// every frame after that is a copy of the few rectangles that really did
// change, followed by the handful of things that really are moving: a piece
// under the pointer, a card following the mouse, a readout ticking over. A
// still frame then costs a few dozen microseconds instead of several hundred.
//
// jet_redraw.h is the damage tracker underneath this, and you could build the
// same thing on it yourself. Three cartridges did, and each had to rediscover
// the same four laws. Those laws are what this module is.
//
//   1. THE CANVAS IS BAKED FROM A FINGERPRINT, NEVER FROM A FLAG. What the
//      canvas shows is a pure function of a short list of things: the
//      position, what is selected, the score, the level. Digest that list into
//      one number every frame and hand it to jet_still_frame; a number that
//      moved bakes, and a number that did not restores. There is no "mark the
//      screen dirty" call for a new feature to forget to make, which is the
//      whole reason it is a digest and not a flag. jet_still_fold is the mixer
//      to build the number with.
//
//   2. EVERY BUFFER OWES ITS OWN DAMAGE. The machine draws into several screen
//      buffers in turn, and a frame's pixels survive in the buffer that drew
//      them until that buffer comes round again. So this module keeps one
//      tracker for each buffer: what the overlay paints is recorded against
//      THIS buffer, this buffer's next visit puts back exactly those pixels,
//      and a fresh bake owes every buffer one whole screen. A single counter
//      of "frames still owing a repaint" looks right, survives every test you
//      are likely to write for it, and tears on the third buffer.
//
//   3. THE OVERLAY IS WHAT STANDS BETWEEN THE CANVAS AND THE EYE. Anything
//      that moves every frame is drawn after jet_still_frame and marks its own
//      rectangle with jet_still_mark. Anything that changes only when the game
//      changes belongs in the fingerprint instead, and is drawn by the painter.
//
//   4. A MARK MUST COVER WHAT IT PAINTED. A rectangle a pixel too small leaves
//      a smear that outlives the frame that made it. The tracker works in
//      thirty-two pixel cells, so being generous costs nothing: add a few
//      pixels of margin and sleep at night.
//
// Set the display resolution first, call jet_still_init once in _init, and
// call jet_still_frame at the top of every _draw. If working memory has run
// out the still keeps no canvas and every frame is painted whole, which is
// slower and identical to look at. This is an opt-in module: include
// jet_still.h yourself.
//
// Complete still-screen example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_still.h"
//
//   enum { BACKGROUND = 1, SQUARE_SIZE = 32, BOARD = 12, POINTER = 15 };
//   static JetStill still;
//   static int score;
//   static int pointer_x, pointer_y;
//
//   static void paint_board(void* user_data) {
//       (void)user_data;
//       char line[32];
//       jet_cls(BACKGROUND);
//       for (int square = 0; square < 64; square++) {
//           if (((square + square / 8) & 1) == 0) continue;
//           jet_rectfill((square % 8) * SQUARE_SIZE, (square / 8) * SQUARE_SIZE,
//                        SQUARE_SIZE, SQUARE_SIZE, BOARD);
//       }
//       snprintf(line, sizeof line, "SCORE %d", score);
//       jet_print(line, 8, 8, POINTER);
//   }
//
//   void _init(void) {
//       jet_still_init(&still);
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       pointer_x = jet_mouse_x();
//       pointer_y = jet_mouse_y();
//       if (btnp(BTN_A)) score += 100;
//   }
//
//   void _draw(void) {
//       // The score is baked, so it is in the fingerprint. The pointer moves
//       // every frame, so it is the overlay and marks itself instead.
//       jet_still_frame(&still, jet_still_fold(0, (uint64_t)score), paint_board, NULL);
//       jet_circfill(pointer_x, pointer_y, 6, POINTER);
//       jet_still_mark(&still, pointer_x - 8, pointer_y - 8, 16, 16);
//   }
//
//   void _end(void) {}

/**
 * Struct: JetStillCounters
 * Summary: What the still did, counted for you: how many frames it served, how many whole paints into the canvas those cost, and how many stretches it copied back out of it. Nothing here changes what is drawn. It is here so you can watch the still working, and a screen nobody touches should show one bake and then nothing but frames.
 * Fields:
 *   - frames: How many times jet_still_frame has been called.
 *   - bakes: How many of those frames painted the whole screen into the canvas, because the fingerprint had moved.
 *   - spans: How many stretches have been copied back from the canvas into a screen buffer. A whole-screen restore counts as one.
 * Include: jet_still.h
 * Notes: Print bakes beside frames while you are tuning a fingerprint. A bake on nearly every frame means the fingerprint carries something that moves every frame, and that something belongs in the overlay instead.
 * Example:
 *   printf("%llu frames, %llu bakes\n", (unsigned long long)still.counters.frames,
 *          (unsigned long long)still.counters.bakes);
 */
typedef struct {
    uint64_t frames;
    uint64_t bakes;
    uint64_t spans;
} JetStillCounters;

/**
 * Struct: JetStill
 * Summary: One still screen: the canvas the scene is painted into, one damage tracker for each screen buffer, and the fingerprint the canvas was painted from. Keep one in a cartridge-wide variable, set it up with jet_still_init, and drive it with jet_still_frame once a frame.
 * Fields:
 *   - canvas: One screen's worth of working memory, holding the last whole paint. NULL when there was no room for it, in which case every frame is painted whole instead.
 *   - tracker: One dirty-cell tracker for each screen buffer, because a buffer keeps its own pixels between visits and so owes its own repair.
 *   - owed: Which buffers have yet to receive the current canvas whole. A fresh bake owes every one of them.
 *   - width: The screen width, in pixels, that the canvas was reserved for.
 *   - height: The screen height, in pixels, that the canvas was reserved for.
 *   - standing: Whether a still is standing: a canvas has been painted and not released.
 *   - fingerprint: The number the standing canvas was painted from.
 *   - counters: What the still did, for your own readouts.
 * Include: jet_still.h
 * Notes: Every field is the module's own to write. Read counters if you want to see it working, and treat the rest as private.
 * Example:
 *   static JetStill still;
 *   jet_still_init(&still);
 */
typedef struct {
    uint8_t* canvas;
    JetRedrawTracker tracker[JET_VRAM_BUFFER_COUNT];
    uint8_t owed[JET_VRAM_BUFFER_COUNT];
    int width;
    int height;
    bool standing;
    uint64_t fingerprint;
    JetStillCounters counters;
} JetStill;

/**
 * Callback: JetStillPaint
 * Summary: Your function that paints the whole still screen. jet_still_frame calls it when the fingerprint has moved, with every drawing call quietly redirected into the canvas, so write it exactly as you would write an ordinary whole-screen _draw.
 * Parameters:
 *   - user_data: The pointer you gave jet_still_frame, passed on unchanged.
 * Returns: Nothing.
 * Include: jet_still.h
 * Notes: Clear the screen first, with jet_cls or a full-screen rectangle: the canvas still holds whatever the last paint left in it. The camera and the clip rectangle are reset before you are called and again afterwards, so you start from the state a fresh frame starts from and leave none of your own behind. Paint only what the fingerprint carries. Anything that moves every frame belongs in the overlay, after jet_still_frame returns.
 * Example:
 *   void paint_board(void* user_data) {
 *       const Game* game = (const Game*)user_data;
 *       jet_cls(0);
 *       board_draw(game);
 *   }
 */
typedef void (*JetStillPaint)(void* user_data);

/**
 * Function: jet_make_still
 * Summary: Gives you an empty still that is safe to store. You must still call jet_still_init on it before you use it.
 * Parameters:
 *   - None.
 * Returns: A JetStill with every field zero, handed back by value.
 * Include: jet_still.h
 * Notes: The still owns no memory until jet_still_init reserves its canvas and its trackers from WRAM, the cartridge's working memory.
 * Example:
 *   JetStill still = jet_make_still();
 */
static inline JetStill jet_make_still(void) {
    JetStill still;
    memset(&still, 0, sizeof still);
    return still;
}

/**
 * Function: jet_still_init
 * Summary: Reserves the canvas, one screen at the current resolution, and a damage tracker for each screen buffer.
 * Parameters:
 *   - still: The still to set up.
 * Returns: Nothing.
 * Include: jet_still.h
 * Notes: Call it once from _init, and only after you have set the final resolution. jet_memory_reserve works in _init only and there is no way to give memory back, so calling this again would use up another whole screen of WRAM. If WRAM has run out the canvas stays empty, every frame is painted whole, and nothing else changes: a still screen is then slower to draw and identical to look at.
 *
 *        The canvas is the size the screen was when you called this. If you change the resolution afterwards it no longer describes the screen, and for as long as that is so every frame is painted whole rather than copied out of a canvas of the wrong shape. Go back to the size it was reserved at and it serves again.
 * Example:
 *   void _init(void) {
 *       jet_display_set_resolution(640, 360);
 *       jet_still_init(&still);
 *   }
 */
void jet_still_init(JetStill* still);

/**
 * Function: jet_still_ready
 * Summary: Tells you whether the still has a canvas, meaning there was room for one in working memory.
 * Parameters:
 *   - still: The still to ask.
 * Returns: true when the still has a canvas to bake into, false when every frame will be painted whole instead.
 * Include: jet_still.h
 * Notes: You never have to check this. A still without a canvas paints every frame whole, which is correct and only slower. Check it when you want to say so on a debug readout.
 * Example:
 *   if (!jet_still_ready(&still)) printf("no canvas: painting every frame whole\n");
 */
static inline bool jet_still_ready(const JetStill* still) {
    return still->canvas != NULL;
}

/**
 * Function: jet_still_standing
 * Summary: Tells you whether a still is standing, meaning a canvas has been painted and not released.
 * Parameters:
 *   - still: The still to ask.
 * Returns: true while the screen is being served from the canvas, false before the first bake and after jet_still_release.
 * Include: jet_still.h
 * Notes: While no still stands, jet_still_mark does nothing. That is what lets one overlay function serve both a still frame and an ordinary moving frame, without a second spelling of it.
 * Example:
 *   if (jet_still_standing(&still)) printf("serving from the canvas\n");
 */
static inline bool jet_still_standing(const JetStill* still) {
    return still->standing;
}

/**
 * Function: jet_still_fold
 * Summary: Folds one more value into a fingerprint. Start from 0 and fold in every piece of state your painter draws from; the number you end with is the fingerprint to hand jet_still_frame.
 * Parameters:
 *   - fingerprint: The number so far. Pass 0 for the first value.
 *   - value: The next piece of state to fold in.
 * Returns: The fingerprint with that value folded into it.
 * Include: jet_still.h
 * Notes: Every bit of the input reaches every bit of the output, so two different scenes practically never end up with the same number. The order matters, which is what you want: the same two values folded the other way round are a different scene. Fold in only what the painter draws from. Something that moves every frame, such as the pointer, would bake the screen every frame and belongs in the overlay instead.
 * Example:
 *   uint64_t fingerprint = jet_still_fold(0, (uint64_t)score);
 *   fingerprint = jet_still_fold(fingerprint, (uint64_t)level);
 *   fingerprint = jet_still_fold(fingerprint, (uint64_t)selected_square);
 */
static inline uint64_t jet_still_fold(uint64_t fingerprint, uint64_t value) {
    // SplitMix64's finaliser over the classic hash-combine step: cheap enough
    // to fold a dozen values every frame, and thorough enough that a one-bit
    // change anywhere moves about half the bits of the answer.
    uint64_t mixed =
        fingerprint ^ (value + 0x9E3779B97F4A7C15ULL + (fingerprint << 6) + (fingerprint >> 2));
    mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
    return mixed ^ (mixed >> 31);
}

/**
 * Function: jet_still_frame
 * Summary: One frame of the still: paint the canvas again if the fingerprint has moved, then put back whatever the overlay disturbed in this screen buffer last time round. Call it at the top of _draw, then draw your overlay on top and mark what it covered.
 * Parameters:
 *   - still: The still to serve this frame from.
 *   - fingerprint: One number digesting everything the painter draws from. Build it with jet_still_fold.
 *   - paint: Your whole-screen painter, called only when the canvas needs baking again.
 *   - user_data: A pointer of your own, passed on to the painter. Pass NULL if you have no use for it.
 * Returns: true when the canvas was painted again this frame, which you may want to know and never have to act on.
 * Include: jet_still.h
 * Notes: Do not clear the screen before calling this. Clearing throws away the very pixels the still is about to put back, and the painter clears the canvas itself. Without a canvas, or while the screen is not the size the canvas was reserved at, this simply calls your painter for the whole screen on every frame.
 * Example:
 *   void _draw(void) {
 *       jet_still_frame(&still, fingerprint(&game), paint_board, &game);
 *       jet_spr(cursor, pointer_x, pointer_y);
 *       jet_still_mark(&still, pointer_x, pointer_y, 16, 16);
 *   }
 */
bool jet_still_frame(JetStill* still, uint64_t fingerprint, JetStillPaint paint, void* user_data);

/**
 * Function: jet_still_mark
 * Summary: Says "I painted here, over the still". Call it for every rectangle your overlay touched, right after drawing it, so the next visit to this screen buffer puts the canvas back there.
 * Parameters:
 *   - still: The still to mark.
 *   - x: The rectangle's left edge, in screen pixels.
 *   - y: The rectangle's top edge, in screen pixels.
 *   - w: The rectangle's width in pixels.
 *   - h: The rectangle's height in pixels.
 * Returns: Nothing.
 * Include: jet_still.h
 * Notes: Cover everything you painted, including a shadow, a glow or a soft edge. A rectangle a pixel too small leaves a smear behind it. The tracker works in thirty-two pixel cells, so a few pixels of margin usually cost nothing at all. It works in screen pixels and does not apply the camera, so mark where the thing appeared on the screen, not where it lives in the world. While no still stands this does nothing, so the same overlay code can also run on an ordinary moving frame.
 * Example:
 *   jet_print(clock, 8, 8, 15);
 *   jet_still_mark(&still, 8, 8, jet_text_width(clock, NULL), 8);
 */
void jet_still_mark(JetStill* still, int x, int y, int w, int h);

/**
 * Function: jet_still_release
 * Summary: Ends the still: no canvas is standing, and every buffer's damage is forgotten. Call it on the first frame you go back to drawing the whole screen yourself, such as when a pause ends.
 * Parameters:
 *   - still: The still to end.
 * Returns: Nothing.
 * Include: jet_still.h
 * Notes: Forgetting the damage is the point. A frame painted whole has already covered everything the still owed, so putting any of it back would paint over the new frame. The next call to jet_still_frame bakes again from scratch. Calling this while no still stands does nothing.
 * Example:
 *   if (paused) {
 *       jet_still_frame(&still, fingerprint(&game), paint_board, &game);
 *   } else {
 *       jet_still_release(&still);
 *       draw_whole_screen(&game);
 *   }
 */
void jet_still_release(JetStill* still);

#endif // JET_STILL_H
