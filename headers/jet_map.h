// runtime/jet_map.h
#ifndef JET_MAP_H
#define JET_MAP_H

#include "jet_sprite.h"
#include <string.h>

// Tile maps: the levels you paint in the Map Editor, drawn to the screen
// through the camera. A map is a grid of cells, and each cell holds one tile,
// a small square picture. A map can have up to four layers, one grid laid over
// another, so the ground can sit under a layer of trees and a layer of clouds.
// Tile positions are counted in cells; drawing positions and collision
// rectangles are counted in pixels. Each tile can also carry flags you set in
// the editor, so the map can tell you what is solid.
//
// To start: load the map once in _init with jet_map_init. Call jet_map_update
// each frame if any tiles animate or any layer drifts. Then draw what the
// camera sees: jet_map_draw_view draws one layer locked to the camera, and
// jet_map_draw_parallax draws every visible layer, each through its own
// scroll.
//
// Parallax is what makes a flat picture look deep: far layers move less than
// near ones as the camera pans, the way distant hills crawl past a train
// window while the fence beside the track flies by. Every layer carries a
// JetMapScroll, set in the Map Editor's Parallax card and saved with the map:
// a factor for each axis that multiplies the camera's movement, a drift in
// pixels per second so the layer can move on its own, and whether the layer
// repeats. For a camera at (cam_x, cam_y) a layer's own origin is
// cam * factor + drift * time, so scenery with a factor of 0.2 crawls while
// the playfield at 1.0 runs, and clouds with a drift move even when the camera
// stands still. jet_map_draw_parallax draws the whole stack in one call, and
// jet_map_layer_origin gives you the same origin, so you can draw your own
// sprites onto a far layer and have them move with it.
//
// Complete parallax-scrolling example (copy into main.c):
//
//   #include "jet64.h"
//
//   static JetMap world;
//   static float camera_x;
//
//   void _init(void) {
//       jet_map_init(&world, "world.map");
//   }
//
//   void _update(float dt) {
//       const float scroll_speed = 160.0f;
//       if (btn(BTN_LEFT))  camera_x -= scroll_speed * dt;
//       if (btn(BTN_RIGHT)) camera_x += scroll_speed * dt;
//       jet_map_update(&world, dt);   // moves animated tiles and drifting layers along
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       // Every visible layer, back to front, each scrolling its own way.
//       jet_map_draw_parallax(&world, camera_x, 0.0f, true);
//   }
//
//   void _end(void) {}

/**
 * Constant: JET_MAP_ANIM_LUT_SIZE
 * Summary: How many animation IDs one map can bind, and so how long JetMap.anim_lut is: 256.
 * Include: jet64.h
 * Notes: A tile entry keeps its animation ID in eight bits, and eight bits count from 0 to 255, so the table is 256 long. That is also why jet_map_bind_anims skips any definition whose id is larger.
 * Example:
 *   bool bindable = def.id < JET_MAP_ANIM_LUT_SIZE;
 */
#define JET_MAP_ANIM_LUT_SIZE 256u

/**
 * Constant: JET_MAP_MAX_LAYERS
 * Summary: The most layers one map file can hold, and so the length of JetMap.scroll: 4.
 * Include: jet64.h
 * Notes: The map file's per-layer tables have exactly this many records, and the visible and collision flag bytes hold one bit per layer. Every call in this module that takes a layer number treats a number at or past this as out of range.
 * Example:
 *   for (uint32_t i = 0; i < JET_MAP_MAX_LAYERS; i++) jet_map_set_layer_visible(&map, (int)i, true);
 */
#define JET_MAP_MAX_LAYERS 4u

/**
 * Struct: JetMapScroll
 * Summary: How one map layer scrolls against the camera, which is its parallax. For a camera at (cam_x, cam_y) after time seconds, the layer's origin on each axis is cam * factor + drift * time.
 * Fields:
 *   - factor_x: How much of the camera's movement across the layer follows, as a multiplier. 1.0 locks the layer to the world; 0.2 makes distant scenery crawl; above 1.0 the layer overtakes the playfield, like grass in the foreground.
 *   - factor_y: The same multiplier for the camera's movement down.
 *   - drift_x: How fast the layer moves across on its own, in map pixels per second, driven by the clock that jet_map_update advances. This is how clouds move when the camera does not.
 *   - drift_y: How fast the layer moves down on its own, in map pixels per second.
 *   - wrap_x: true to repeat the layer across, so a strip narrower than the camera's travel tiles forever; false to stop the layer at its own edges.
 *   - wrap_y: true to repeat the layer down.
 * Include: jet_map.h
 * Notes: Each layer's scroll is set in the Map Editor's Parallax card and loaded with the map, so you usually never build one. When you want scroll decided in code, make one with jet_make_map_scroll and give it to jet_map_set_layer_scroll. JET_MAP_SCROLL_LOCKED is the scroll every layer has until an author says otherwise.
 * Example:
 *   JetMapScroll clouds = jet_make_map_scroll(0.1f, 0.2f, 6.0f, 0.0f, true, false);
 */
typedef struct {
    float factor_x;
    float factor_y;
    float drift_x;
    float drift_y;
    bool wrap_x;
    bool wrap_y;
} JetMapScroll;

/**
 * Function: jet_make_map_scroll
 * Summary: Builds a JetMapScroll from its six values, for scroll you decide in code rather than in the Map Editor.
 * Parameters:
 *   - factor_x: How much of the camera's movement across the layer follows, as a multiplier. 1.0 locks it to the world.
 *   - factor_y: The same multiplier for the camera's movement down.
 *   - drift_x: How fast the layer moves across on its own, in map pixels per second.
 *   - drift_y: How fast the layer moves down on its own, in map pixels per second.
 *   - wrap_x: true to repeat the layer across.
 *   - wrap_y: true to repeat the layer down.
 * Returns: A filled-in JetMapScroll, handed back by value.
 * Include: jet_map.h
 * Notes: Give the result to jet_map_set_layer_scroll. A factor of 0 pins the layer to the screen on that axis; add a drift and you have a backdrop that moves only on its own.
 * Example:
 *   jet_map_set_layer_scroll(&map, 0, jet_make_map_scroll(0.25f, 0.25f, 0.0f, 0.0f, true, false));
 */
static inline JetMapScroll jet_make_map_scroll(float factor_x, float factor_y, float drift_x, float drift_y, bool wrap_x, bool wrap_y) {
    JetMapScroll scroll;
    scroll.factor_x = factor_x;
    scroll.factor_y = factor_y;
    scroll.drift_x = drift_x;
    scroll.drift_y = drift_y;
    scroll.wrap_x = wrap_x;
    scroll.wrap_y = wrap_y;
    return scroll;
}

/**
 * Constant: JET_MAP_SCROLL_LOCKED
 * Summary: The JetMapScroll of a layer locked to the world: a factor of 1.0 on both axes, no drift, no wrap.
 * Include: jet_map.h
 * Notes: Every layer of a map with no parallax set in the editor scrolls this way, and jet_map_layer_scroll answers with it for a layer number that is out of range. Giving it to a layer through jet_map_set_layer_scroll turns that layer's parallax off.
 * Example:
 *   jet_map_set_layer_scroll(&map, 2, JET_MAP_SCROLL_LOCKED);
 */
#define JET_MAP_SCROLL_LOCKED jet_make_map_scroll(1.0f, 1.0f, 0.0f, 0.0f, false, false)

/**
 * Struct: JetMap
 * Summary: One tile map with its layers, loaded by jet_map_init into WRAM, the cartridge's working memory. Each tile entry is 32 bits holding the tile ID, its flip and rotation, your own flags, and an animation ID. A map has up to JET_MAP_MAX_LAYERS layers, and each layer is width times height cells. The struct holds only pointers and sizes; the tiles themselves live in the block of WRAM the loader reserved.
 * Fields:
 *   - width: How many cells wide the map is.
 *   - height: How many cells tall the map is.
 *   - layers: How many layers the map has, from 1 to JET_MAP_MAX_LAYERS.
 *   - tile_size: The side of one tile in pixels, for example 8, 16 or 32.
 *   - data: The tile entries in WRAM, one layer after another (all of layer 0's cells, then all of layer 1's, and so on), or NULL when no map is loaded.
 *   - layer_visible_flags: One bit per layer. Bit i is set when layer i is marked visible in the editor, and jet_map_draw_parallax draws exactly these layers.
 *   - layer_collision_flags: One bit per layer. Bit i is set when layer i is marked as a collision layer in the editor, and jet_map_check_solid tests exactly these layers.
 *   - scroll: Each layer's parallax, read from the map file's scroll table. A map saved without one gets JET_MAP_SCROLL_LOCKED for every layer.
 *   - anim_lut: A table of JET_MAP_ANIM_LUT_SIZE entries that turns an animation ID into its JetAnimDef, filled by jet_map_bind_anims. An ID with nothing bound is NULL.
 *   - time: The seconds jet_map_update has added up so far. Animated tiles and drifting layers read it.
 * Include: jet_map.h
 * Notes: Treat the fields as read-only. Use jet_map_get and jet_map_set to reach the tiles, and the jet_map_layer_scroll and jet_map_layer_visible calls, with their set_ partners, for each layer's settings. The anim_lut entries point into the JetAnimDef array you gave jet_map_bind_anims, so keep that array alive for as long as the map is drawn.
 * Example:
 *   static JetMap map;
 *   bool loaded = jet_map_init(&map, "0.map");
 */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t layers;
    uint32_t tile_size;
    uint32_t* data;

    // Per-layer traits decoded from the map asset's metadata header.
    uint8_t layer_visible_flags;
    uint8_t layer_collision_flags;

    // Per-layer parallax, decoded from the asset's scroll table.
    JetMapScroll scroll[JET_MAP_MAX_LAYERS];

    // O(1) lookup from a tile entry's animation ID (bits 24-31) to its definition.
    JetAnimDef* anim_lut[JET_MAP_ANIM_LUT_SIZE];
    float time;
} JetMap;

/**
 * Function: jet_make_map
 * Summary: Gives you an empty JetMap that is safe to store. Call jet_map_init on it before you use it.
 * Parameters:
 *   - None.
 * Returns: A JetMap with every field zero, handed back by value.
 * Include: jet_map.h
 * Notes: An empty map has a NULL data pointer, so jet_map_get, the draw calls and the collision calls all treat it as empty until jet_map_init succeeds.
 * Example:
 *   JetMap map = jet_make_map();
 */
static inline JetMap jet_make_map(void) {
    JetMap m;
    memset(&m, 0, sizeof(JetMap));
    return m;
}

/**
 * Function: jet_map_init
 * Summary: Loads one of your cartridge's map files into WRAM, the cartridge's working memory, and fills in the JetMap so it can be drawn.
 * Parameters:
 *   - map: The JetMap to fill in.
 *   - filename: The map's file name, for example "0.map". The host, the program running your cartridge, finds the file for you.
 * Returns: true when the map loaded; false on any failure, in which case the map is left zeroed and empty.
 * Include: jet_map.h
 * Notes: Call it from _init only. The tiles are reserved from WRAM, which is handed out from one end and never given back, so loading a map every frame would eat WRAM until it ran out. The host streams the file into WRAM through the map IPC command block. If anything fails, a missing file, a bad header, or WRAM running out, the call returns false and every field of the map is zeroed.
 * Example:
 *   JetMap map;
 *   bool ok = jet_map_init(&map, "0.map");
 */
bool jet_map_init(JetMap* map, const char* filename);

/**
 * Function: jet_map_bind_anims
 * Summary: Gives a map the animation definitions its animated tiles use, so each animated tile can find its current frame when it is drawn.
 * Parameters:
 *   - map: The JetMap whose animation table is rebuilt.
 *   - anims: Your array of JetAnimDef records, or NULL to clear every binding.
 *   - count: How many records are in anims.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: Each record is filed under its own anims[i].id; an id of 256 or more is skipped. The map keeps pointers into your anims array rather than copies, so the array must stay alive for as long as the map is drawn.
 * Example:
 *   JetAnimDef anims[32];
 *   uint32_t anim_count = jet_load_anims(anims, 32);
 *   jet_map_bind_anims(&map, anims, anim_count);
 */
void jet_map_bind_anims(JetMap* map, JetAnimDef* anims, uint32_t count);

/**
 * Function: jet_map_update
 * Summary: Moves the map's clock forward by dt seconds. Animated tiles and drifting layers both read this clock.
 * Parameters:
 *   - map: The JetMap whose clock moves.
 *   - dt: The seconds that have passed since the last call.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: Call it once each frame, before you draw a map that has animated tiles or a layer whose JetMapScroll has a drift. On a map with neither it does no harm.
 * Example:
 *   jet_map_update(&map, dt);
 */
void jet_map_update(JetMap* map, float dt);

/**
 * Function: jet_map_get
 * Summary: Reads one tile entry, a 32-bit value, from one layer of a map.
 * Parameters:
 *   - map: The JetMap to read.
 *   - x: The column, counted in cells from the left.
 *   - y: The row, counted in cells from the top.
 *   - layer: Which layer, counting from 0.
 * Returns: The 32-bit tile entry, or 0 when the cell or the layer is out of range or the map is empty.
 * Include: jet_map.h
 * Notes: The bits of an entry are laid out as the editor lays them out: the tile ID in bits 0 to 11, the horizontal and vertical flips in bits 12 and 13, the rotation in bits 14 and 15, your own flags in bits 16 to 23, and the animation ID in bits 24 to 31.
 * Example:
 *   uint32_t entry = jet_map_get(&map, tx, ty, 0);
 */
uint32_t jet_map_get(JetMap* map, int x, int y, int layer);

/**
 * Function: jet_map_set
 * Summary: Writes one tile entry, a 32-bit value, into one layer of a map.
 * Parameters:
 *   - map: The JetMap to change.
 *   - x: The column, counted in cells from the left.
 *   - y: The row, counted in cells from the top.
 *   - layer: Which layer, counting from 0.
 *   - entry: The 32-bit tile entry to store, with its bits laid out as jet_map_get describes.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: A write outside the map, or to an empty map, is ignored. The change is made straight in the tile data in WRAM.
 * Example:
 *   uint32_t solid_grass = 17u | (1u << 16);
 *   jet_map_set(&map, tx, ty, 0, solid_grass);
 */
void jet_map_set(JetMap* map, int x, int y, int layer, uint32_t entry);

/**
 * Function: jet_map_draw
 * Summary: Draws a rectangle of cells from one layer onto the screen.
 * Parameters:
 *   - map: The JetMap to draw.
 *   - cel_x: The column of the first cell to draw, counted in cells.
 *   - cel_y: The row of the first cell to draw, counted in cells.
 *   - screen_x: Where the first cell goes, across, in world pixels. The camera's offset is taken off.
 *   - screen_y: Where the first cell goes, down, in world pixels. The camera's offset is taken off.
 *   - cel_w: How many cells wide the block is.
 *   - cel_h: How many cells tall the block is.
 *   - layer: Which layer to draw, counting from 0.
 *   - use_mask: true to let each tile's mask, the one-bit-per-pixel picture painted in the Sprite Editor, decide which pixels show; false to leave colour 0 see-through.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: Cells outside the clip rectangle are skipped. A tile's flips, its quarter-turn rotations and its bound animation are worked out before it is drawn. screen_x and screen_y are world positions, so the camera's offset is taken off each tile as it is drawn. For a view that scrolls with the camera, jet_map_draw_view is the easier call.
 * Example:
 *   jet_map_draw(&map, 0, 0, 0, 0, 40, 23, 0, true);
 */
void jet_map_draw(JetMap* map, int cel_x, int cel_y, int screen_x, int screen_y, int cel_w, int cel_h, int layer, bool use_mask);

/**
 * Function: jet_map_draw_view
 * Summary: Draws one layer as a full-screen view that scrolls, given where the camera is in map pixels.
 * Parameters:
 *   - map: The JetMap to draw.
 *   - cam_x: The camera's position across, in map pixels: the map pixel at the left edge of the screen.
 *   - cam_y: The camera's position down, in map pixels: the map pixel at the top edge of the screen.
 *   - layer: Which layer to draw, counting from 0.
 *   - use_mask: true to let each tile's mask, the one-bit-per-pixel picture painted in the Sprite Editor, decide which pixels show; false to leave colour 0 see-through.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: It works out which cells the camera can see and draws two extra cells past each edge, so the view stays whole while the camera sits part-way across a tile. The tiles are placed straight from cam_x and cam_y, so do not also move the camera with jet_cam_set, or the two offsets add up.
 * Example:
 *   jet_map_draw_view(&map, player_x - 320.0f, player_y - 180.0f, 0, true);
 */
void jet_map_draw_view(JetMap* map, float cam_x, float cam_y, int layer, bool use_mask);

/**
 * Function: jet_map_layer_scroll
 * Summary: Reads one layer's parallax: the JetMapScroll loaded with the map, or the last one given to jet_map_set_layer_scroll.
 * Parameters:
 *   - map: The JetMap to read.
 *   - layer: Which layer, counting from 0.
 * Returns: The layer's JetMapScroll, by value, or JET_MAP_SCROLL_LOCKED when the map is empty or the layer is out of range.
 * Include: jet_map.h
 * Notes: Handy for remembering the scroll the map came with before you change it, so you can put it back later.
 * Example:
 *   JetMapScroll authored = jet_map_layer_scroll(&map, 0);
 */
JetMapScroll jet_map_layer_scroll(const JetMap* map, int layer);

/**
 * Function: jet_map_set_layer_scroll
 * Summary: Replaces one layer's parallax for every draw that follows.
 * Parameters:
 *   - map: The JetMap to change.
 *   - layer: Which layer, counting from 0.
 *   - scroll: The JetMapScroll to use. Make one with jet_make_map_scroll, or pass JET_MAP_SCROLL_LOCKED.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: A layer number out of range is ignored. The change lives in the loaded JetMap only; the map file on disk keeps what the Map Editor saved.
 * Example:
 *   jet_map_set_layer_scroll(&map, 0, jet_make_map_scroll(0.1f, 0.2f, 6.0f, 0.0f, true, false));
 */
void jet_map_set_layer_scroll(JetMap* map, int layer, JetMapScroll scroll);

/**
 * Function: jet_map_layer_visible
 * Summary: Tells you whether one layer is visible: the Map Editor's eye toggle, or the last value given to jet_map_set_layer_visible.
 * Parameters:
 *   - map: The JetMap to read.
 *   - layer: Which layer, counting from 0.
 * Returns: true when the layer is visible; false when it is hidden, out of range, or the map is empty.
 * Include: jet_map.h
 * Notes: jet_map_draw_parallax draws exactly the visible layers. The single-layer draw calls draw whatever layer you ask for, visible or not.
 * Example:
 *   if (jet_map_layer_visible(&map, 2)) jet_map_draw_layer(&map, cam_x, cam_y, 2, true);
 */
bool jet_map_layer_visible(const JetMap* map, int layer);

/**
 * Function: jet_map_set_layer_visible
 * Summary: Shows or hides one layer for jet_map_draw_parallax.
 * Parameters:
 *   - map: The JetMap to change.
 *   - layer: Which layer, counting from 0.
 *   - visible: true to draw the layer, false to skip it.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: A layer number out of range is ignored. Collision is not changed: a hidden collision layer still blocks.
 * Example:
 *   if (jet_key_pressed(JET_KEY_1)) jet_map_set_layer_visible(&map, 0, !jet_map_layer_visible(&map, 0));
 */
void jet_map_set_layer_visible(JetMap* map, int layer, bool visible);

/**
 * Function: jet_map_layer_origin
 * Summary: Works out where one layer's top-left corner sits for a camera position, cam * factor + drift * time on each axis, so you can place your own sprites on a parallax layer.
 * Parameters:
 *   - map: The JetMap to read.
 *   - layer: Which layer, counting from 0.
 *   - cam_x: The camera's position across, in map pixels.
 *   - cam_y: The camera's position down, in map pixels.
 *   - origin_x: Where the answer across is written, in that layer's own pixels. May be NULL if you do not want it.
 *   - origin_y: Where the answer down is written. May be NULL.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: This is exactly the origin jet_map_draw_layer scrolls the layer to, so a sprite drawn at (world_x - origin_x, world_y - origin_y) moves with the layer. For a layer out of range the answer is the camera position unchanged. For a layer that wraps, wrap the sprite's world position by the layer's width in pixels yourself.
 * Example:
 *   float ox, oy;
 *   jet_map_layer_origin(&map, 1, cam_x, cam_y, &ox, &oy);
 *   jet_spr_mask(SUN_SPRITE, (int)(SUN_X - ox), (int)(SUN_Y - oy), 2, 2, false, false);
 */
void jet_map_layer_origin(const JetMap* map, int layer, float cam_x, float cam_y, float* origin_x, float* origin_y);

/**
 * Function: jet_map_draw_layer
 * Summary: Draws one layer as a full-screen view through that layer's own parallax: scrolled to the origin jet_map_layer_origin gives, and repeated wherever its JetMapScroll wraps.
 * Parameters:
 *   - map: The JetMap to draw.
 *   - cam_x: The camera's position across, in map pixels.
 *   - cam_y: The camera's position down, in map pixels.
 *   - layer: Which layer to draw, counting from 0.
 *   - use_mask: true to let each tile's mask, the one-bit-per-pixel picture painted in the Sprite Editor, decide which pixels show; false to leave colour 0 see-through.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: This is the one-layer half of jet_map_draw_parallax, for when you want to draw your own things between layers, say your actors between the ground and a foreground layer. It draws the layer whether or not it is marked visible. A locked layer draws exactly as jet_map_draw_view does.
 * Example:
 *   jet_map_draw_layer(&map, cam_x, cam_y, 0, true);
 *   draw_actors(cam_x, cam_y);
 *   jet_map_draw_layer(&map, cam_x, cam_y, 1, true);
 */
void jet_map_draw_layer(JetMap* map, float cam_x, float cam_y, int layer, bool use_mask);

/**
 * Function: jet_map_draw_parallax
 * Summary: Draws every visible layer, back to front, each through its own parallax. This is the one call that draws a whole scrolling scene.
 * Parameters:
 *   - map: The JetMap to draw.
 *   - cam_x: The camera's position across, in map pixels: the map pixel at the left edge of the playfield's view.
 *   - cam_y: The camera's position down, in map pixels.
 *   - use_mask: true to let each tile's mask, the one-bit-per-pixel picture painted in the Sprite Editor, decide which pixels show; false to leave colour 0 see-through.
 * Returns: Nothing.
 * Include: jet_map.h
 * Notes: It is the same as calling jet_map_draw_layer for each layer jet_map_layer_visible reports, in layer order, so layer 0 is the furthest away. Call jet_map_update each frame, or drifting layers stand still. The tiles are placed straight from the camera position, so do not also move the camera with jet_cam_set, or the two offsets add up.
 * Example:
 *   jet_cls(0);
 *   jet_map_draw_parallax(&map, cam_x, cam_y, true);
 */
void jet_map_draw_parallax(JetMap* map, float cam_x, float cam_y, bool use_mask);

/**
 * Function: jet_map_collide
 * Summary: Tests a rectangle, in pixels, against the flagged cells of one map layer.
 * Parameters:
 *   - map: The JetMap to test.
 *   - x: The rectangle's left edge, in map pixels.
 *   - y: The rectangle's top edge, in map pixels.
 *   - w: The rectangle's width in pixels.
 *   - h: The rectangle's height in pixels.
 *   - layer: Which layer to test, counting from 0.
 *   - user_flag_mask: Which of your own flags to look for, as a mask over tile bits 16 to 23. Pass 0 to hit any tile that is not empty.
 * Returns: The first tile entry (32 bits) that hits, or 0 if none did.
 * Include: jet_map.h
 * Notes: The rectangle is upright, not turned. A negative position is turned into a cell by rounding down, so a rectangle that straddles x = 0 still tests the cell it partly covers. This is a broad test of tiles and flags, not a pixel-perfect sprite test; for that, see jet_collide in jet_collision.h.
 * Example:
 *   uint32_t hit = jet_map_collide(&map, px, py, 12, 16, 0, 0x01);
 */
uint32_t jet_map_collide(JetMap* map, int x, int y, int w, int h, int layer, uint8_t user_flag_mask);

/**
 * Function: jet_map_check_solid
 * Summary: Tests a rectangle, in pixels, against every layer marked as a collision layer.
 * Parameters:
 *   - map: The JetMap to test.
 *   - x: The rectangle's left edge, in map pixels.
 *   - y: The rectangle's top edge, in map pixels.
 *   - w: The rectangle's width in pixels.
 *   - h: The rectangle's height in pixels.
 *   - user_flag_mask: Which of your own flags to look for, as a mask over tile bits 16 to 23. Pass 0 to hit any tile that is not empty.
 * Returns: The first tile entry (32 bits) on a collision layer that hits, or 0 if none did.
 * Include: jet_map.h
 * Notes: Which layers count as collision layers comes from the Map Editor's settings, loaded by jet_map_init. Use jet_map_collide to test one particular layer.
 * Example:
 *   if (jet_map_check_solid(&map, next_x, next_y, 12, 16, 0x01)) {
 *       block_movement();
 *   }
 */
uint32_t jet_map_check_solid(JetMap* map, int x, int y, int w, int h, uint8_t user_flag_mask);

#endif // JET_MAP_H
