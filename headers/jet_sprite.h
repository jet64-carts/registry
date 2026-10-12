// runtime/jet_sprite.h
#ifndef JET_SPRITE_H
#define JET_SPRITE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // IWYU pragma: keep (offsetof for ABI layout asserts)
#include <string.h>
#include "jet_graphics.h" // IWYU pragma: keep (SpriteFormat, for jet_anim_draw_ext)

// Draw the pictures you made in the Sprite Editor, by the number the editor
// shows for each cell, and put an animation's current frame on the screen.
// First call jet_set_sprite_grid(cell_width, cell_height) in _init to say how
// big your cells are. From then on the grid helpers pick a sprite by its
// EDITOR CELL number, while the x and y you give them are still SCREEN PIXELS.
// jet_spr_grid draws a cell at the size you drew it; jet_spr_ext_grid takes a
// SpriteFormat when you want it scaled, turned, faded or mirrored.
//
// Animation itself lives next door in jet_anim.h, which this header includes:
// what an animation IS, where its playhead is, and every call that moves one.
// What lives HERE is the drawing half, three calls deep: jet_anim_draw_grid at
// the grid size you set, jet_anim_draw at a size you give, and
// jet_anim_draw_ext through a SpriteFormat.
//
// Complete scale example (copy into a cart's main.c):
//
//   #include "jet64.h"
//
//   static SpriteFormat big_sprite;
//
//   void _init(void) {
//       jet_set_sprite_grid(16, 16); // each Sprite Editor cell is 16x16 pixels
//       big_sprite = jet_make_sprite_format(
//           2.0f, 2.0f,  // draw at 32x32 pixels
//           0.0f, 1.0f,  // no rotation, fully opaque
//           false, false,
//           true         // use the Sprite Editor mask
//       );
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_spr_ext_grid(0, JET_WIDTH / 2, JET_HEIGHT / 2, &big_sprite);
//   }
//
//   void _update(float dt) { (void)dt; }
//   void _end(void) {}

// The animation types, and every animation call that does not draw, live in
// jet_anim.h. It is included here so a cart that has only ever included
// jet_sprite.h still sees JetAnimDef, JetAnimState and their friends.
#include "jet_anim.h" // IWYU pragma: keep


// --- SPRITE GRID ---
// The size of the sprite grid, set by jet_set_sprite_grid. The grid helpers
// below read these to turn an editor cell number into a place on the sprite
// sheet.
extern uint32_t _jet_grid_w;
extern uint32_t _jet_grid_h;
extern uint32_t _jet_tiles_w;
extern uint32_t _jet_tiles_h;
extern uint32_t _jet_cols_per_row;

/**
 * Function: jet_set_sprite_grid
 * Summary: Tells the runtime how big your Sprite Editor cells are, so the grid helpers can find them.
 * Parameters:
 *   - width: The width of one cell in pixels. A value below 8 is ignored.
 *   - height: The height of one cell in pixels. A value below 8 is ignored.
 * Returns: Nothing.
 * Include: jet_sprite.h
 * Notes: Call it once in _init, before any grid helper. The width and height are turned into a count of 8x8 hardware tiles for jet_spr_grid, jet_spr_mask_grid, jet_spr_ext_grid and the collision helpers. Choose sizes that divide the 1024-pixel width of the sprite sheet evenly.
 * Example:
 *   jet_set_sprite_grid(16, 16);
 *   jet_spr_grid(3, 40, 32, false, false);
 */
void jet_set_sprite_grid(uint32_t width, uint32_t height);

/**
 * Function: jet_get_hw_idx
 * Summary: Turns a Sprite Editor cell number into the 8x8 tile number the low-level sprite calls use.
 * Parameters:
 *   - editor_idx: The cell number the Sprite Editor shows, counting from 0, in the grid you set.
 * Returns: The tile number to give jet_spr, jet_spr_mask, jet_spr_ext, or a collision call.
 * Include: jet_sprite.h
 * Notes: It depends on the grid set by jet_set_sprite_grid. With the default 8x8 grid, cell numbers and tile numbers are the same.
 * Example:
 *   uint32_t hw = jet_get_hw_idx(10);
 *   jet_spr(hw, 100, 64, _jet_tiles_w, _jet_tiles_h, false, false);
 */
static inline uint32_t jet_get_hw_idx(uint32_t editor_idx) {
    uint32_t row = editor_idx / _jet_cols_per_row;
    uint32_t col = editor_idx % _jet_cols_per_row;
    return ((row * _jet_tiles_h) * 128) + (col * _jet_tiles_w);
}

/**
 * Function: jet_spr_grid
 * Summary: Draws one Sprite Editor cell at the size you drew it.
 * Parameters:
 *   - idx: The cell number the Sprite Editor shows, counting from 0.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Call jet_set_sprite_grid in _init first. This macro fills in the cell's width and height for you. To draw the cell at another size, use jet_spr_ext_grid with a SpriteFormat.
 * Example:
 *   jet_spr_grid(3, 100, 80, false, false);
 */
#define jet_spr_grid(idx, x, y, flip_x, flip_y) \
    jet_spr(jet_get_hw_idx(idx), x, y, _jet_tiles_w, _jet_tiles_h, flip_x, flip_y)

/**
 * Function: jet_spr_scale_pow2_grid
 * Summary: Draws one Sprite Editor cell at 2, 4, 8, 16 or 32 times the size you drew it.
 * Parameters:
 *   - idx: The cell number the Sprite Editor shows, counting from 0.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - shift: How much to enlarge, as a power of two: 0 draws at 1 times, 1 at 2 times, 2 at 4 times, up to JET_SPR_SCALE_SHIFT_MAX (32 times).
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Call jet_set_sprite_grid in _init first. This is the whole-number, no-rotation partner of jet_spr_ext_grid. Use it whenever the size you want is the drawn size doubled, quadrupled and so on, and keep jet_spr_ext_grid for the sizes and turns that only a SpriteFormat can describe. A 32x32 cell at a shift of 2 is drawn 128x128 pixels with its top-left corner at (x, y).
 * Example:
 *   jet_spr_scale_pow2_grid(3, 100, 80, 2, false, false);
 */
#define jet_spr_scale_pow2_grid(idx, x, y, shift, flip_x, flip_y) \
    jet_spr_scale_pow2(jet_get_hw_idx(idx), x, y, _jet_tiles_w, _jet_tiles_h, shift, flip_x, flip_y)

/**
 * Function: jet_spr_mask_grid
 * Summary: Draws one Sprite Editor cell at the size you drew it, letting the mask you painted decide which pixels show.
 * Parameters:
 *   - idx: The cell number the Sprite Editor shows, counting from 0.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Unlike jet_spr_grid, palette index 0 stays visible wherever you painted the mask, so colour 0 can be a real colour inside the sprite.
 * Example:
 *   jet_spr_mask_grid(3, 100, 80, false, false);
 */
#define jet_spr_mask_grid(idx, x, y, flip_x, flip_y) \
    jet_spr_mask(jet_get_hw_idx(idx), x, y, _jet_tiles_w, _jet_tiles_h, flip_x, flip_y)

/**
 * Function: jet_spr_silhouette_grid
 * Summary: Draws the SHAPE of one Sprite Editor cell in a single flat colour, using the mask you painted.
 * Parameters:
 *   - idx: The cell number the Sprite Editor shows, counting from 0.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 *   - col: The colour to fill the shape with, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Call jet_set_sprite_grid in _init first. This is the flat-colour shadow of jet_spr_mask_grid: same cell, same mask, one colour instead of the sprite's own. Draw it a few pixels down and across from the cell, then draw the cell over it, for a coloured drop shadow that follows the outline exactly.
 * Example:
 *   jet_spr_silhouette_grid(3, 104, 84, false, false, 8);
 *   jet_spr_grid(3, 100, 80, false, false);
 */
#define jet_spr_silhouette_grid(idx, x, y, flip_x, flip_y, col) \
    jet_spr_silhouette(jet_get_hw_idx(idx), x, y, _jet_tiles_w, _jet_tiles_h, flip_x, flip_y, col)

/**
 * Function: jet_spr_ext_grid
 * Summary: Draws one Sprite Editor cell at the size, turn, fade, mirror and mask choice a SpriteFormat describes. This is the call for a sprite drawn larger or smaller than the size you drew it.
 * Parameters:
 *   - idx: The cell number the Sprite Editor shows, counting from 0.
 *   - x: The centre of the drawn sprite, across, in world pixels.
 *   - y: The centre of the drawn sprite, down, in world pixels.
 *   - fmt: A pointer to a SpriteFormat, usually made with jet_make_sprite_format. NULL draws the cell plainly.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Call jet_set_sprite_grid in _init first. The grid picks WHICH cell is drawn; fmt->scale_x and fmt->scale_y set HOW BIG it is drawn. A 16x16 cell with both scales at 2.0f is drawn 32x32 pixels, centred on (x, y). The format is only read during this call and stays yours. With NULL the call behaves like jet_spr_grid, and (x, y) becomes the top-left corner instead of the centre; when you need no transform, call jet_spr_grid yourself.
 * Example:
 *   SpriteFormat big = jet_make_sprite_format(
 *       2.0f, 2.0f, 0.0f, 1.0f, false, false, true
 *   );
 *   jet_spr_ext_grid(0, JET_WIDTH / 2, JET_HEIGHT / 2, &big);
 */
#define jet_spr_ext_grid(idx, x, y, fmt) \
    jet_spr_ext(jet_get_hw_idx(idx), x, y, _jet_tiles_w, _jet_tiles_h, fmt)

/**
 * Function: jet_collide_grid
 * Summary: Tells you whether two Sprite Editor cells, drawn at the size you drew them, overlap pixel for pixel.
 * Parameters:
 *   - idx1: The first cell's number.
 *   - x1: The first cell's left edge, in world pixels.
 *   - y1: The first cell's top edge, in world pixels.
 *   - flip_x1: true if the first cell is mirrored left to right.
 *   - flip_y1: true if the first cell is mirrored top to bottom.
 *   - idx2: The second cell's number.
 *   - x2: The second cell's left edge, in world pixels.
 *   - y2: The second cell's top edge, in world pixels.
 *   - flip_x2: true if the second cell is mirrored left to right.
 *   - flip_y2: true if the second cell is mirrored top to bottom.
 * Returns: true when a visible pixel of one sits on a visible pixel of the other; otherwise false.
 * Include: jet64.h
 * Notes: Uses the cell size set by jet_set_sprite_grid, and each cell's mask to decide which pixels count. Give it the same positions and flips you draw with, so the picture and the hit test agree. If either sprite is scaled or turned, use jet_collide_ext_grid instead.
 * Example:
 *   bool hit = jet_collide_grid(0, px, py, false, false, 1, ex, ey, false, false);
 */
#define jet_collide_grid(idx1, x1, y1, flip_x1, flip_y1, idx2, x2, y2, flip_x2, flip_y2) \
    jet_collide(jet_get_hw_idx(idx1), x1, y1, _jet_tiles_w, _jet_tiles_h, flip_x1, flip_y1, \
                 jet_get_hw_idx(idx2), x2, y2, _jet_tiles_w, _jet_tiles_h, flip_x2, flip_y2)

/**
 * Function: jet_collide_ext_grid
 * Summary: Tells you whether two Sprite Editor cells, each drawn through its own SpriteFormat, overlap pixel for pixel.
 * Parameters:
 *   - idx1: The first cell's number.
 *   - x1: The first cell's position, across, in world pixels.
 *   - y1: The first cell's position, down, in world pixels.
 *   - fmt1: The first cell's SpriteFormat, or NULL for no transform.
 *   - idx2: The second cell's number.
 *   - x2: The second cell's position, across, in world pixels.
 *   - y2: The second cell's position, down, in world pixels.
 *   - fmt2: The second cell's SpriteFormat, or NULL for no transform.
 * Returns: true when a visible pixel of one, after its transform, sits on a visible pixel of the other; otherwise false.
 * Include: jet64.h
 * Notes: Give it the same positions and formats you draw with, so the picture and the hit test agree. With a format, (x, y) is the sprite's centre, just as it is for jet_spr_ext_grid. With NULL, (x, y) is the top-left corner, as it is for jet_spr_grid.
 * Example:
 *   bool hit = jet_collide_ext_grid(0, px, py, &player_fmt, 1, ex, ey, &enemy_fmt);
 */
#define jet_collide_ext_grid(idx1, x1, y1, fmt1, idx2, x2, y2, fmt2) \
    jet_collide_ext(jet_get_hw_idx(idx1), x1, y1, _jet_tiles_w, _jet_tiles_h, fmt1, \
                     jet_get_hw_idx(idx2), x2, y2, _jet_tiles_w, _jet_tiles_h, fmt2)

/**
 * Function: jet_anim_draw_grid
 * Summary: Draws an animation's current frame at the cell size you set with jet_set_sprite_grid.
 * Parameters:
 *   - state: The animation, started with jet_anim_play and moved along each tick with jet_anim_update.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 *   - use_mask: true to let the mask you painted decide which pixels show; false to leave palette index 0 see-through.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: The animation may carry its own scale, rotation and tint, authored in the Sprite Editor; those still apply. This macro only fills in the cell size for you.
 * Example:
 *   jet_anim_draw_grid(&player_anim, px, py, facing_left, false, true);
 */
#define jet_anim_draw_grid(state, x, y, flip_x, flip_y, use_mask) \
    jet_anim_draw(state, x, y, _jet_tiles_w, _jet_tiles_h, flip_x, flip_y, use_mask)


/**
 * Function: jet_anim_draw
 * Summary: Draws an animation's current frame, applying the scale, rotation and tint the animation was authored with.
 * Parameters:
 *   - state: The animation whose current frame you want drawn.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width of ONE cell, in 8x8 hardware tiles.
 *   - h: The height of ONE cell, in 8x8 hardware tiles.
 *   - flip_x: true to mirror the frame left to right.
 *   - flip_y: true to mirror the frame top to bottom.
 *   - use_mask: true to let the mask decide which pixels show, as jet_spr_mask does; false to leave palette index 0 see-through, as jet_spr does.
 * Returns: Nothing.
 * Include: jet_sprite.h
 * Notes: w and h describe one cell. A frame that spans several cells is drawn w times its span across by h times its span down. The frame's own flips combine with the ones you pass here, and its pixel offset is mirrored about the anchor cell when you flip, so a wide swing stays lined up with the idle pose it belongs to. Respects the camera and the clip rectangle.
 *
 *        When the frame's scale is 1, its rotation is 0 and its tint is 0, the frame goes straight through jet_spr or jet_spr_mask. Otherwise each screen pixel looks up the source pixel it came from, and the tint is dithered in with a Bayer pattern. The authored scale and rotation turn the frame about the centre of its anchor cell.
 * Example:
 *   jet_anim_draw(&player_anim, px, py, 2, 2, facing_left, false, true);
 */
void jet_anim_draw(JetAnimState* state, int x, int y, int w, int h, bool flip_x, bool flip_y, bool use_mask);

/**
 * Function: jet_anim_draw_ext
 * Summary: Draws an animation's current frame through a SpriteFormat, so your own scale, rotation, fade, mirror and mask choice combine with what the animation was authored with.
 * Parameters:
 *   - state: The animation whose current frame you want drawn.
 *   - x: The ANCHOR POINT, across, in world pixels. The anchor point is the centre of the frame's anchor cell.
 *   - y: The anchor point, down, in world pixels.
 *   - fmt: A pointer to a SpriteFormat, usually made with jet_make_sprite_format. NULL draws the frame plainly.
 * Returns: Nothing.
 * Include: jet_sprite.h
 * Notes: A frame that is one cell behaves exactly as jet_spr_ext_grid does. A frame that spans several cells, or carries an offset, turns about the anchor point rather than about its own middle, which is what keeps a spinning character's feet where they belong.
 *
 *        The animation's own eased scale and rotation combine with the format's. The animation holds degrees and a SpriteFormat holds radians; the conversion happens once, inside. The animation's silhouette tint does not combine, because the drawing path this shares with jet_spr_ext has no tint stage; use jet_anim_draw when the tint matters.
 *
 *        With NULL the call falls back to jet_anim_draw_grid, and (x, y) then names the TOP-LEFT of the anchor cell instead of its centre, the same switch of convention that jet_spr_ext describes for itself.
 * Example:
 *   SpriteFormat fmt = jet_make_sprite_format(2.0f, 2.0f, angle, 1.0f, false, false, true);
 *   jet_anim_draw_ext(&player_anim, px, py, &fmt);
 */
void jet_anim_draw_ext(JetAnimState* state, int x, int y, const SpriteFormat* fmt);

// Short names for the sprite-grid helpers, in the PICO-8 style.
#define spr_grid jet_spr_grid
#define spr_mask_grid jet_spr_mask_grid
#define spr_ext_grid jet_spr_ext_grid

#endif // JET_SPRITE_H
