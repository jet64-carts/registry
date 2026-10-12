// runtime/jet_graphics.h
#ifndef JET_GRAPHICS_H
#define JET_GRAPHICS_H

#ifndef JET_CORE_H
#include "jet_core.h"
#endif

// Everything that puts a mark on the screen: single pixels, lines, rectangles,
// circles, triangles, sprites, and textured triangles for simple 3D. Positions
// are in pixels, counted from the top-left corner of the screen. Most of these
// calls respect the camera, so your world can scroll, and the clip rectangle,
// so drawing stays inside the area you allow.
//
// For sprites you drew in the Sprite Editor, start with the grid helpers in
// jet_sprite.h. jet_spr_grid draws a cell at the size you drew it. To draw it
// bigger, smaller, turned or faded, build a SpriteFormat and call
// jet_spr_ext_grid.
//
// Complete primitives example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { SKY = 1, GROUND = 3, SUN = 10, OUTLINE = 15 };
//
//   void _init(void) {}
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       jet_cls(SKY);
//       jet_circfill(JET_WIDTH - 96, 80, 36, SUN);
//       jet_rectfill(0, JET_HEIGHT / 2, JET_WIDTH, JET_HEIGHT / 2, GROUND);
//       jet_line(0, JET_HEIGHT / 2, JET_WIDTH - 1, JET_HEIGHT / 2, OUTLINE);
//   }
//
//   void _end(void) {}

// The size of the sprite sheet. The SPRITES bank holds GFX_BANK_COUNT (4) banks
// of 1024x1024 pixels stacked one above the other, so the whole sheet is one
// picture 1024 pixels wide and 4096 pixels tall. Keep these in step with
// jet64_core::hardware_map.
#define JET_SPRITE_SHEET_WIDTH  1024
#define JET_SPRITE_SHEET_HEIGHT 4096

// The biggest magnification jet_spr_scale_pow2 accepts, written as the shift
// that makes it: 5 means 32 times. At 32 times an 8x8 tile becomes a 256-pixel
// block and a 32x32 editor cell becomes 1024 pixels, the full width of the
// sheet and more than a 1280x720 screen can show at once.
#define JET_SPR_SCALE_SHIFT_MAX 5

/**
 * Function: jet_pset
 * Summary: Sets one pixel to a colour.
 * Parameters:
 *   - x: Where the pixel goes, across, in world pixels.
 *   - y: Where the pixel goes, down, in world pixels.
 *   - col: The colour, as a palette index: a number from 0 to 255 that picks one of the palette's colours.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: A world position is a position in your game's own coordinates. If you have moved the camera, its offset is taken off first, so the pixel lands where the world has scrolled to. A pixel outside the screen, or outside the clip rectangle, is skipped and nothing goes wrong.
 *
 *        jet_pset is a macro. Each argument is evaluated once, so jet_pset(x++, y, col) is safe.
 * Example:
 *   jet_pset(player_x, player_y, 15);
 */
#define jet_pset(x, y, col) do { \
    int _x = (x) - _jet_cam_x; \
    int _y = (y) - _jet_cam_y; \
    if (_x >= _jet_clip_x0 && _x <= _jet_clip_x1 && _y >= _jet_clip_y0 && _y <= _jet_clip_y1) { \
        VRAM[_y * JET_WIDTH + _x] = (col); \
    } \
} while(0)

/**
 * Function: jet_cls
 * Summary: Clears the whole screen to one colour.
 * Parameters:
 *   - col: The colour to fill with, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: This ignores the camera and the clip rectangle and fills every one of the JET_WIDTH * JET_HEIGHT pixels. While jet_set_draw_target is sending drawing into a buffer of your own, it clears that buffer instead of the screen.
 * Example:
 *   void _draw(void) { jet_cls(0); }
 */
#define jet_cls(col) memset(VRAM, col, JET_WIDTH * JET_HEIGHT)

// The live camera offset and clip rectangle that the drawing calls use. The
// camera is taken off every world position as it is drawn; the clip rectangle
// (screen positions, both edges included) bounds what is written. Set them
// through jet_camera.h and jet_clip.h rather than writing these directly.
extern int _jet_cam_x;
extern int _jet_cam_y;
extern int _jet_clip_x0;
extern int _jet_clip_y0;
extern int _jet_clip_x1;
extern int _jet_clip_y1;

/**
 * Function: jet_get_current_buffer_index
 * Summary: Tells you which of the screen buffers you are drawing into right now.
 * Parameters:
 *   - None.
 * Returns: 0, 1 or 2, depending on the buffering mode the display is in.
 * Include: jet_graphics.h
 * Notes: With double or triple buffering the machine draws into one buffer while it shows another, and this is the number of the one you are drawing into this frame. It is useful when you keep separate bookkeeping for each buffer, as a redraw scheme does. The answer only means something while you are drawing to the screen; while jet_set_draw_target is sending drawing elsewhere it is not a real buffer number.
 * Example:
 *   uint32_t buf_idx = jet_get_current_buffer_index();
 */
static inline uint32_t jet_get_current_buffer_index(void) {
    return (uint32_t)((VRAM - (JET_BASE + JET_VRAM_OFFSET)) / JET_VRAM_SIZE);
}

/**
 * Function: _init_graphics_engine
 * Summary: Resets global graphics state for camera translation and clipping.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Called by Jet64 during runtime startup. Cartridge code should normally use jet_cam_reset() and jet_clip_reset() instead of calling this directly.
 * Example:
 *   // Usually handled by Jet64 startup.
 *   _init_graphics_engine();
 */
void _init_graphics_engine(void);

/**
 * Function: jet_set_draw_target
 * Summary: Sends all drawing into a buffer of your own instead of the screen, until you call jet_reset_draw_target.
 * Parameters:
 *   - target: The buffer to draw into, holding at least JET_WIDTH * JET_HEIGHT bytes, one byte per pixel. Pass NULL to go back to the previous target.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: While a target is set, jet_pset, jet_cls, jet_rect, jet_circfill, jet_spr, print and every other normal drawing call writes into your buffer. The first non-NULL call remembers which screen buffer was live at that moment, and jet_reset_draw_target goes back to that one. Remembering the live buffer, rather than a fixed one, is what keeps this safe under double and triple buffering.
 * Example:
 *   jet_set_draw_target(canvas);
 *   jet_circfill(mx, my, 8, 12);
 *   jet_reset_draw_target();
 */
void jet_set_draw_target(uint8_t* target);

/**
 * Function: jet_reset_draw_target
 * Summary: Sends drawing back to the screen buffer that was live before jet_set_draw_target.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Safe to call even when nothing was redirected. Call it as soon as you have finished drawing into your own buffer, so the rest of the frame lands on the screen again.
 * Example:
 *   jet_set_draw_target(canvas);
 *   jet_cls(0);
 *   jet_reset_draw_target();
 */
void jet_reset_draw_target(void);

/**
 * Function: jet_get_draw_target
 * Summary: Gives you the buffer that the drawing calls are writing into right now.
 * Parameters:
 *   - None.
 * Returns: A pointer to the current draw target, one byte per pixel.
 * Include: jet_graphics.h
 * Notes: Normally this is the live screen buffer. While jet_set_draw_target is in effect, it is the buffer you passed in.
 * Example:
 *   uint8_t* active_target = jet_get_draw_target();
 */
uint8_t* jet_get_draw_target(void);

// Shapes

/**
 * Function: jet_rect
 * Summary: Draws the outline of a rectangle, one pixel thick.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels. Zero or less draws nothing.
 *   - h: The height in pixels. Zero or less draws nothing.
 *   - col: The colour, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Respects the camera and the clip rectangle.
 * Example:
 *   jet_rect(16, 16, 64, 32, 7);
 */
void jet_rect(int x, int y, int w, int h, uint8_t col);

/**
 * Function: jet_rectfill
 * Summary: Draws a solid rectangle.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels. Zero or less draws nothing.
 *   - h: The height in pixels. Zero or less draws nothing.
 *   - col: The colour, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Respects the camera and the clip rectangle. It fills a whole row at a time, which is why it is the call to reach for when you want a solid panel, a health bar, or a tile-sized block.
 * Example:
 *   jet_rectfill(0, 0, JET_WIDTH, JET_HEIGHT, 0);
 */
void jet_rectfill(int x, int y, int w, int h, uint8_t col);

/**
 * Function: jet_circ
 * Summary: Draws the outline of a circle, one pixel thick.
 * Parameters:
 *   - cx: The centre, across, in world pixels.
 *   - cy: The centre, down, in world pixels.
 *   - r: The radius in pixels. Give it zero or more; a negative radius means nothing.
 *   - col: The colour, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Respects the camera and the clip rectangle, because every pixel goes through jet_pset.
 * Example:
 *   jet_circ(player_x, player_y, 12, 11);
 */
void jet_circ(int cx, int cy, int r, uint8_t col);

/**
 * Function: jet_circfill
 * Summary: Draws a solid circle.
 * Parameters:
 *   - cx: The centre, across, in world pixels.
 *   - cy: The centre, down, in world pixels.
 *   - r: The radius in pixels.
 *   - col: The colour, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Respects the camera and the clip rectangle. It fills a whole row at a time, so even a big circle is quick.
 * Example:
 *   jet_circfill(impact_x, impact_y, 6, 10);
 */
void jet_circfill(int cx, int cy, int r, uint8_t col);

/**
 * Function: jet_line
 * Summary: Draws a straight line, one pixel thick, between two points.
 * Parameters:
 *   - x0: Where the line starts, across, in world pixels.
 *   - y0: Where the line starts, down, in world pixels.
 *   - x1: Where the line ends, across, in world pixels.
 *   - y1: Where the line ends, down, in world pixels.
 *   - col: The colour, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: A line that is exactly horizontal or exactly vertical is filled as one run, which is quick. Any other line is stepped pixel by pixel with Bresenham's method, the classic way to draw a line using only whole numbers.
 * Example:
 *   jet_line(0, 0, 127, 63, 14);
 */
void jet_line(int x0, int y0, int x1, int y1, uint8_t col);

/**
 * Function: jet_trifill
 * Summary: Draws a filled triangle in one colour.
 * Parameters:
 *   - x1: The first corner, across, in world pixels.
 *   - y1: The first corner, down, in world pixels.
 *   - x2: The second corner, across, in world pixels.
 *   - y2: The second corner, down, in world pixels.
 *   - x3: The third corner, across, in world pixels.
 *   - y3: The third corner, down, in world pixels.
 *   - col: The colour, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: All three corners are world positions, so the camera's offset is taken off each one, and the triangle is cut to the clip rectangle. A triangle whose corners all sit on one horizontal line has no height and draws nothing. The edges are stepped in fixed-point arithmetic, whole numbers with a fraction tucked inside, rather than floating point. For a triangle with a picture on it, use jet_triblit.
 * Example:
 *   jet_trifill(64, 20, 24, 96, 104, 96, 12);
 */
void jet_trifill(int x1, int y1, int x2, int y2, int x3, int y3, uint8_t col);

// Sprites

/**
 * Function: jet_spr
 * Summary: Draws a sprite from the sprite sheet, with colour 0 left see-through.
 * Parameters:
 *   - n: Which sprite, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in 8x8 tiles.
 *   - h: The height in 8x8 tiles.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Every pixel of palette index 0 is skipped, so the background shows through it. If you drew the sprite in the Sprite Editor, spr_grid in jet_sprite.h counts in editor cells instead of 8x8 tiles and is easier to use. The sprite is cut to the screen before any pixel is copied, so a sprite half off the edge costs only what is visible.
 * Example:
 *   jet_spr(0, 32, 48, 2, 2, false, false);
 */
void jet_spr(uint32_t n, int x, int y, int w, int h, bool flip_x, bool flip_y);

/**
 * Function: jet_blit
 * Summary: Copies a solid rectangle of pixels from the sprite sheet to the screen, with nothing left see-through.
 * Parameters:
 *   - src_x: Where to copy from, across, in sprite-sheet pixels.
 *   - src_y: Where to copy from, down, in sprite-sheet pixels.
 *   - dst_x: Where to put it, across, in world pixels.
 *   - dst_y: Where to put it, down, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: It cuts the rectangle to the screen, then copies a whole row at a time, which makes it quick. Colour 0 is copied like any other colour, so use jet_spr or jet_blit_mask when you need see-through pixels.
 * Example:
 *   jet_blit(0, 0, 100, 40, 128, 64);
 */
void jet_blit(int src_x, int src_y, int dst_x, int dst_y, int w, int h);

/**
 * Function: jet_blit_mask
 * Summary: Copies a rectangle of pixels from the sprite sheet to the screen, letting the mask plane decide which pixels show.
 * Parameters:
 *   - src_x: Where to copy from, across, in sprite-sheet pixels.
 *   - src_y: Where to copy from, down, in sprite-sheet pixels.
 *   - dst_x: Where to put it, across, in world pixels.
 *   - dst_y: Where to put it, down, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Every sprite has a second picture behind it, the mask, with one bit per pixel that says whether that pixel is visible. A pixel is copied only where its mask bit is set. This is the Amiga way of drawing a BOB, a blitter object, and it lets palette index 0 be a real colour inside the sprite.
 * Example:
 *   jet_blit_mask(32, 0, enemy_x, enemy_y, 24, 24);
 */
void jet_blit_mask(int src_x, int src_y, int dst_x, int dst_y, int w, int h);

/**
 * Function: jet_blit_silhouette
 * Summary: Draws the SHAPE of a rectangle of the sprite sheet in one flat colour, using the mask plane, so you get a solid shadow of it rather than its own colours.
 * Parameters:
 *   - src_x: Where to read the shape from, across, in sprite-sheet pixels.
 *   - src_y: Where to read the shape from, down, in sprite-sheet pixels.
 *   - dst_x: Where to put it, across, in world pixels.
 *   - dst_y: Where to put it, down, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - col: The colour to fill the shape with, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: This is jet_blit_mask's flat-colour twin, and jet_spr_silhouette counted in sheet pixels rather than 8x8 tiles: where jet_blit_mask copies the sheet's own colours through the mask, this writes one colour through it and reads no colour plane at all. It is quicker than the masked blit for the same reason: the mask is one bit per pixel, so eight pixels are tested with a single byte, a byte of air is skipped without a write and a byte of solid is filled with a single write. Draw it a few pixels offset for a drop shadow that hugs the outline, or in place for a hit flash or a stealth silhouette. Use this rather than jet_spr_silhouette when the shape is not on an 8x8 tile boundary, as a sheet cut into arbitrary rectangles will not be.
 * Example:
 *   jet_blit_silhouette(32, 0, enemy_x, enemy_y, 24, 24, 63);
 */
void jet_blit_silhouette(int src_x, int src_y, int dst_x, int dst_y, int w, int h, uint8_t col);

/**
 * Function: jet_spr_mask
 * Summary: Draws a sprite from the sprite sheet, letting its mask plane decide which pixels show.
 * Parameters:
 *   - n: Which sprite, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in 8x8 tiles.
 *   - h: The height in 8x8 tiles.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Use this instead of jet_spr when you have painted the mask in the Sprite Editor, so that what shows comes from the mask and not from colour 0. That lets colour 0 be a real colour inside the sprite.
 * Example:
 *   jet_spr_mask(16, 80, 32, 2, 2, true, false);
 */
void jet_spr_mask(uint32_t n, int x, int y, int w, int h, bool flip_x, bool flip_y);

/**
 * Function: jet_spr_silhouette
 * Summary: Draws the SHAPE of a sprite in one flat colour, using its mask plane, so you get a solid shadow of it rather than the sprite itself.
 * Parameters:
 *   - n: Which sprite, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in 8x8 tiles.
 *   - h: The height in 8x8 tiles.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 *   - col: The colour to fill the shape with, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: This is the Amiga's flat-colour BOB shadow. Where jet_spr_mask draws the sprite's own colours through its mask, this draws one colour through it and reads no colour at all, which is why it is quicker than drawing the sprite a second time: the mask is one bit per pixel, so a whole run of eight pixels is tested with a single byte and filled with a single write. Draw it just before the sprite itself, offset by a few pixels, for a coloured drop shadow that hugs the outline exactly; draw it in place for a hit flash, a stealth silhouette, or a selection glow. Paint the mask in the Sprite Editor, and remember that a sprite with an empty mask draws nothing here. For a sprite you drew in the Sprite Editor, jet_spr_silhouette_grid in jet_sprite.h counts in editor cells.
 * Example:
 *   jet_spr_silhouette(16, 84, 36, 2, 2, false, false, 8);
 *   jet_spr(16, 80, 32, 2, 2, false, false);
 */
void jet_spr_silhouette(uint32_t n, int x, int y, int w, int h, bool flip_x, bool flip_y, uint8_t col);

/**
 * Function: jet_spr_scale_pow2
 * Summary: Draws a sprite from the sprite sheet at 2, 4, 8, 16 or 32 times its size, with colour 0 left see-through.
 * Parameters:
 *   - n: Which sprite, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x: The left edge of the enlarged sprite, in world pixels.
 *   - y: The top edge of the enlarged sprite, in world pixels.
 *   - w: The width in 8x8 tiles, before enlarging.
 *   - h: The height in 8x8 tiles, before enlarging.
 *   - shift: How much to enlarge, as a power of two: 0 draws at 1 times, 1 at 2 times, 2 at 4 times, and so on up to JET_SPR_SCALE_SHIFT_MAX (32 times). A larger value is held at that maximum.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: Each source pixel becomes a square block of pixels, with no smoothing, no rotation, and no floating point anywhere, so the same sprite draws to the same pixels on every runtime. When your scale is a power of two and the sprite is not turned, prefer this to jet_spr_ext: it finds each source pixel with a bit shift instead of a fixed-point step, fills each enlarged pixel as one run, and copies each row after the first from the row above. A shift of 0 is exactly jet_spr and is handed to it. For a sprite you drew in the Sprite Editor, jet_spr_scale_pow2_grid in jet_sprite.h counts in editor cells.
 * Example:
 *   jet_spr_scale_pow2(0, 32, 48, 2, 2, 2, false, false);
 */
void jet_spr_scale_pow2(uint32_t n, int x, int y, int w, int h, uint8_t shift, bool flip_x, bool flip_y);

/**
 * Struct: SpriteFormat
 * Summary: Describes how jet_spr_ext and jet_spr_ext_grid change a sprite as they draw it: its size, its rotation, how faded it is, whether it is mirrored, and whether its mask plane decides what shows. Use one when you want a sprite drawn larger or smaller than the size you drew it, or turned, or faded.
 * Fields:
 *   - scale_x: How wide to draw it, as a multiplier. 1.0f keeps the width you drew, 2.0f doubles it, and 0.5f halves it.
 *   - scale_y: How tall to draw it, as a multiplier. 1.0f keeps the height you drew, 2.0f doubles it, and 0.5f halves it.
 *   - angle: How far to turn it clockwise, in radians. 0.0f is no turn. Constants such as JET_HALF_PI in jet_math.h name the common angles.
 *   - alpha: How solid it is, from 0.0f (invisible) to 1.0f (solid). A value in between is dithered: a pattern of drawn and skipped pixels that reads as a fade.
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 *   - use_mask: true to let the Sprite Editor's mask plane decide which pixels show; false to leave palette index 0 see-through instead.
 * Include: jet64.h
 * Notes: Make one with jet_make_sprite_format rather than filling in the fields yourself. That way no field is left uninitialised, and an uninitialised field can draw something strange. The scale changes only the size on the screen, never which cell is picked from the sprite sheet.
 * Example:
 *   // Draw editor sprite 0 at twice the width and height you drew it.
 *   SpriteFormat big = jet_make_sprite_format(
 *       2.0f, 2.0f,  // scale_x, scale_y
 *       0.0f,        // no rotation
 *       1.0f,        // fully solid
 *       false, false,
 *       true         // use the mask you painted
 *   );
 *   jet_spr_ext_grid(0, JET_WIDTH / 2, JET_HEIGHT / 2, &big);
 */
typedef struct {
    float scale_x;
    float scale_y;
    float angle; // Radians
    float alpha; // 0.0 to 1.0 (Dithered transparency)
    bool flip_x;
    bool flip_y;
    bool use_mask; // Uses Amiga BOB mask plane for transparency instead of index 0
} SpriteFormat;

/**
 * Function: jet_make_sprite_format
 * Summary: Builds the SpriteFormat that jet_spr_ext and jet_spr_ext_grid use to draw a sprite at a new size, turned, faded, mirrored, or masked.
 * Parameters:
 *   - scale_x: How wide to draw it, as a multiplier. 1.0f keeps the width you drew and 2.0f doubles it.
 *   - scale_y: How tall to draw it, as a multiplier. 1.0f keeps the height you drew and 2.0f doubles it.
 *   - angle: How far to turn it clockwise, in radians. 0.0f is no turn.
 *   - alpha: How solid it is, from 0.0f (invisible) to 1.0f (solid).
 *   - flip_x: true to mirror it left to right.
 *   - flip_y: true to mirror it top to bottom.
 *   - use_mask: true to let the Sprite Editor's mask plane decide which pixels show; false to leave palette index 0 see-through.
 * Returns: A filled-in SpriteFormat, handed back by value.
 * Include: jet64.h
 * Notes: The value owns nothing, so you may keep it, copy it, or change its fields whenever you like. Pass its address to jet_spr_ext or jet_spr_ext_grid. The scale sets the size on the screen: a 16x16 editor cell at 2.0f is drawn 32x32 pixels.
 * Example:
 *   SpriteFormat twice_as_large = jet_make_sprite_format(
 *       2.0f, 2.0f, 0.0f, 1.0f, false, false, true
 *   );
 *   jet_spr_ext_grid(0, JET_WIDTH / 2, JET_HEIGHT / 2, &twice_as_large);
 */
static inline SpriteFormat jet_make_sprite_format(float scale_x, float scale_y, float angle, float alpha, bool flip_x, bool flip_y, bool use_mask) {
    SpriteFormat fmt;
    fmt.scale_x = scale_x;
    fmt.scale_y = scale_y;
    fmt.angle = angle;
    fmt.alpha = alpha;
    fmt.flip_x = flip_x;
    fmt.flip_y = flip_y;
    fmt.use_mask = use_mask;
    return fmt;
}

/**
 * Function: jet_sprite_set_angle
 * Summary: Sets the rotation on a SpriteFormat and hands the same pointer back, so you can chain calls.
 * Parameters:
 *   - fmt: The SpriteFormat to change. NULL is allowed and comes back unchanged.
 *   - angle: How far to turn the sprite clockwise, in radians.
 * Returns: The same fmt pointer you passed in.
 * Include: jet_graphics.h
 * Notes: Handy for setting up a format in one line just before jet_spr_ext.
 * Example:
 *   SpriteFormat fmt = jet_make_sprite_format(1.0f, 1.0f, 0.0f, 1.0f, false, false, false);
 *   jet_sprite_set_angle(&fmt, JET_PI * 0.25f);
 */
static inline SpriteFormat* jet_sprite_set_angle(SpriteFormat* fmt, float angle) {
    if (fmt) fmt->angle = angle;
    return fmt;
}

/**
 * Function: jet_sprite_set_scale
 * Summary: Sets the width and height multipliers on a SpriteFormat and hands the same pointer back, so you can chain calls.
 * Parameters:
 *   - fmt: The SpriteFormat to change. NULL is allowed and comes back unchanged.
 *   - scale_x: How wide to draw the sprite, as a multiplier.
 *   - scale_y: How tall to draw the sprite, as a multiplier.
 * Returns: The same fmt pointer you passed in.
 * Include: jet_graphics.h
 * Notes: jet_spr_ext protects itself against a scale of exactly zero, but give it values above zero all the same.
 * Example:
 *   jet_sprite_set_scale(&fmt, 2.0f, 2.0f);
 */
static inline SpriteFormat* jet_sprite_set_scale(SpriteFormat* fmt, float scale_x, float scale_y) {
    if (fmt) {
        fmt->scale_x = scale_x;
        fmt->scale_y = scale_y;
    }
    return fmt;
}

/**
 * Function: jet_sprite_set_alpha
 * Summary: Sets how solid a sprite is on a SpriteFormat and hands the same pointer back, so you can chain calls.
 * Parameters:
 *   - fmt: The SpriteFormat to change. NULL is allowed and comes back unchanged.
 *   - alpha: How solid, from 0.0f (invisible) to 1.0f (solid).
 * Returns: The same fmt pointer you passed in.
 * Include: jet_graphics.h
 * Notes: The machine has 256 colours and no blending, so a fade is dithered: a 4x4 Bayer pattern decides which pixels are drawn and which are skipped, and from a little distance it reads as transparency.
 * Example:
 *   jet_sprite_set_alpha(&fmt, 0.5f);
 */
static inline SpriteFormat* jet_sprite_set_alpha(SpriteFormat* fmt, float alpha) {
    if (fmt) fmt->alpha = alpha;
    return fmt;
}

/**
 * Function: jet_sprite_set_flip
 * Summary: Sets mirroring on a SpriteFormat and hands the same pointer back, so you can chain calls.
 * Parameters:
 *   - fmt: The SpriteFormat to change. NULL is allowed and comes back unchanged.
 *   - flip_x: true to mirror the sprite left to right.
 *   - flip_y: true to mirror the sprite top to bottom.
 * Returns: The same fmt pointer you passed in.
 * Include: jet_graphics.h
 * Notes: Inside jet_spr_ext each screen pixel works out which source pixel it came from, undoing the turn and the scale; the mirror is applied to that source pixel afterwards.
 * Example:
 *   jet_sprite_set_flip(&fmt, facing_left, false);
 */
static inline SpriteFormat* jet_sprite_set_flip(SpriteFormat* fmt, bool flip_x, bool flip_y) {
    if (fmt) {
        fmt->flip_x = flip_x;
        fmt->flip_y = flip_y;
    }
    return fmt;
}

/**
 * Function: jet_spr_ext
 * Summary: Draws a sprite scaled, turned, faded, mirrored, or masked, as a SpriteFormat describes.
 * Parameters:
 *   - n: Which sprite, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x: The centre of the drawn sprite, across, in world pixels.
 *   - y: The centre of the drawn sprite, down, in world pixels.
 *   - w: The width in 8x8 tiles.
 *   - h: The height in 8x8 tiles.
 *   - fmt: How to draw it. NULL draws it plainly through jet_spr, with no scale, turn, or mirror.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: With a format, the sprite scales and turns about its own centre, and (x, y) is where that centre goes. Set fmt->use_mask when the Sprite Editor's mask plane should decide what shows, rather than colour 0. With NULL, the call falls back to jet_spr, and (x, y) becomes the top-left corner instead; when you do not need a transform, call jet_spr yourself.
 * Example:
 *   SpriteFormat fmt = jet_make_sprite_format(1.5f, 1.5f, 0.2f, 1.0f, false, false, true);
 *   jet_spr_ext(0, JET_WIDTH / 2, JET_HEIGHT / 2, 2, 2, &fmt);
 */
void jet_spr_ext(uint32_t n, int x, int y, int w, int h, const SpriteFormat* fmt);

/**
 * Function: jet_blit_quadrant_mirror
 * Summary: Copies the top-left quarter of the screen into the other three quarters, mirrored.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: It works on the whole screen and ignores the camera and the clip rectangle. Draw a plasma, a kaleidoscope, or any symmetrical pattern into the top-left quarter only, then call this once, and the picture is finished at a quarter of the cost. If the screen has an odd width or height, the middle row or column is left as it is.
 * Example:
 *   // Draw into the top-left quadrant, then mirror it.
 *   jet_blit_quadrant_mirror();
 */
void jet_blit_quadrant_mirror(void);

/**
 * Struct: JetVertex
 * Summary: One corner of a textured triangle drawn by jet_triblit or jet_triblit_lit: where it is on the screen, which point of the texture it carries, and how far away it is.
 * Fields:
 *   - x: Where it is on the screen, across, in pixels.
 *   - y: Where it is on the screen, down, in pixels.
 *   - u: Which pixel of the texture it carries, across, counted from atlas_sx.
 *   - v: Which pixel of the texture it carries, down, counted from atlas_sy.
 *   - inv_z: One divided by the depth (1.0f / z). A nearer corner normally has the larger value. This is what makes the texture look right in perspective.
 * Include: jet64.h
 * Notes: Fill in every field with jet_make_vertex. The triangle calls read the value on the spot and keep no pointer to it.
 * Example:
 *   JetVertex top = jet_make_vertex(160.0f, 40.0f, 8.0f, 0.0f, 1.0f);
 */
typedef struct {
    float x, y;          // Screen coordinates
    float u, v;          // Texture coordinates (scaled to atlas dimensions before calling)
    float inv_z;         // 1.0 / Z for perspective-correct interpolation
} JetVertex;

/**
 * Function: jet_make_vertex
 * Summary: Builds a JetVertex, one corner of a textured triangle, in a single call.
 * Parameters:
 *   - x: Where it is on the screen, across, in pixels.
 *   - y: Where it is on the screen, down, in pixels.
 *   - u: Which pixel of the texture it carries, across.
 *   - v: Which pixel of the texture it carries, down.
 *   - inv_z: One divided by the depth (1.0f / z), for correct perspective.
 * Returns: A filled-in JetVertex, handed back by value.
 * Include: jet_graphics.h
 * Notes: The vertex owns nothing. Pass three of them by value to jet_triblit or jet_triblit_lit.
 * Example:
 *   JetVertex v1 = jet_make_vertex(100.0f, 100.0f, 0.0f, 0.0f, 0.1f);
 */
static inline JetVertex jet_make_vertex(float x, float y, float u, float v, float inv_z) {
    JetVertex vertex;
    vertex.x = x;
    vertex.y = y;
    vertex.u = u;
    vertex.v = v;
    vertex.inv_z = inv_z;
    return vertex;
}

/**
 * Function: jet_triblit
 * Summary: Draws a triangle with a picture from the sprite sheet stretched over it, in correct perspective.
 * Parameters:
 *   - v1: The first corner: its screen position (before the camera), its texture point, and its 1/Z.
 *   - v2: The second corner, the same way.
 *   - v3: The third corner, the same way.
 *   - atlas_sx: The left edge of the texture in the sprite sheet, in pixels.
 *   - atlas_sy: The top edge of the texture in the sprite sheet, in pixels.
 *   - tex_w: The width of the texture in pixels.
 *   - tex_h: The height of the texture in pixels.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: The camera's offset is taken off each corner, and each row is cut to the clip rectangle. Each pixel's texture point is held inside the texture, and the 1/Z values are blended across the triangle so the picture does not swim as it recedes. Colour 0 in the texture is see-through. Keep the texture inside the sprite sheet; the call does not check that for you.
 * Example:
 *   JetVertex v1 = jet_make_vertex(100.0f, 100.0f, 0.0f, 0.0f, 1.0f/10.0f);
 *   JetVertex v2 = jet_make_vertex(150.0f, 100.0f, 31.99f, 0.0f, 1.0f/10.0f);
 *   JetVertex v3 = jet_make_vertex(100.0f, 150.0f, 0.0f, 31.99f, 1.0f/10.0f);
 *   jet_triblit(v1, v2, v3, 0, 0, 32, 32);
 */
void jet_triblit(JetVertex v1, JetVertex v2, JetVertex v3, uint32_t atlas_sx, uint32_t atlas_sy, uint32_t tex_w, uint32_t tex_h);

/**
 * Function: jet_triblit_lit
 * Summary: Draws a textured triangle like jet_triblit, and shades every pixel by a light level that blends smoothly between the three corners.
 * Parameters:
 *   - v1: The first corner: its screen position (before the camera), its texture point, and its 1/Z.
 *   - v2: The second corner, the same way.
 *   - v3: The third corner, the same way.
 *   - atlas_sx: The left edge of the texture in the sprite sheet, in pixels.
 *   - atlas_sy: The top edge of the texture in the sprite sheet, in pixels.
 *   - tex_w: The width of the texture in pixels.
 *   - tex_h: The height of the texture in pixels.
 *   - colormap: The light table: light_levels rows of JET_PALETTE_SIZE (256) bytes each. colormap[L * 256 + c] is colour c as it looks under light level L. Pass NULL to draw with plain jet_triblit and no shading.
 *   - light_levels: How many rows the table has. A blended level outside 0 to light_levels - 1 is held at the edge.
 *   - light1: The light level at v1, as a row of the table. It may be a fraction.
 *   - light2: The light level at v2.
 *   - light3: The light level at v3.
 * Returns: Nothing.
 * Include: jet_graphics.h
 * Notes: This is jet_triblit with one extra step per pixel: the texture colour is looked up in the table row for that pixel's light. It shares jet_triblit's edge stepping, camera, clipping, and colour-0 see-through exactly, so the two can never disagree. The light level travels across the triangle in perspective, weighted by 1/Z like the texture point, which gives smooth gradients of light. To make a material glow, give its palette ramp table rows that hand every colour back unchanged at every level. More rows means finer bands of light. Work each corner's light level out from its position in the world and you get distance fading and torchlight with no dithering at all.
 * Example:
 *   // colormap: 32 rows x 256 bytes; lit[i] computed per quad corner from its world position.
 *   JetVertex v1 = jet_make_vertex(100.0f, 100.0f, 0.0f, 0.0f, 1.0f/10.0f);
 *   JetVertex v2 = jet_make_vertex(150.0f, 100.0f, 31.99f, 0.0f, 1.0f/10.0f);
 *   JetVertex v3 = jet_make_vertex(100.0f, 150.0f, 0.0f, 31.99f, 1.0f/12.0f);
 *   jet_triblit_lit(v1, v2, v3, 0, 0, 32, 32, colormap, 32, 4.0f, 4.0f, 9.0f);
 */
void jet_triblit_lit(JetVertex v1, JetVertex v2, JetVertex v3, uint32_t atlas_sx, uint32_t atlas_sy, uint32_t tex_w, uint32_t tex_h, const uint8_t* colormap, uint32_t light_levels, float light1, float light2, float light3);

// Short names for the drawing calls, in the PICO-8 style.
#define pset jet_pset
#define cls jet_cls
#define line jet_line
#define rect jet_rect
#define rectfill jet_rectfill
#define circ jet_circ
#define circfill jet_circfill
#define trifill jet_trifill
#define spr jet_spr
#define blit jet_blit
#define blit_mask jet_blit_mask
#define blit_silhouette jet_blit_silhouette
#define spr_mask jet_spr_mask
#define spr_ext jet_spr_ext
#define blit_quadrant_mirror jet_blit_quadrant_mirror
#define triblit jet_triblit
#define triblit_lit jet_triblit_lit

#endif // JET_GRAPHICS_H
