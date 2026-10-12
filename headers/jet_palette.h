// runtime/jet_palette.h
#ifndef JET_PALETTE_H
#define JET_PALETTE_H

#include <stdint.h>
#include <stdbool.h>

// The palette is the list of 256 colours the machine can show at once. Every
// pixel on the screen is a number from 0 to 255 that picks one of them, so
// changing one entry changes every pixel that wears it. Here you set and read
// those colours, build a gradient between colours you name, load a named
// palette from the cartridge, save the whole palette and put it back after a
// temporary effect, and cycle a run of entries with JetColorCycle so water
// ripples and lava glows. Two calls answer a question instead of changing
// anything: jet_palette_nearest tells you which entry is closest to a colour
// you name, and jet_palette_ink_map builds the table that TextFormat.palette_map
// prints a sprite font through, so one baked face can speak in every colour
// you build a table for (docs/sprite_fonts/design.md).
//
// Complete animated-gradient example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { RAMP_FIRST = 32, RAMP_LAST = 47, STRIPE_HEIGHT = 16 };
//   static JetColorCycle water;
//
//   void _init(void) {
//       static const JetRgb stops[] = { { 4, 16, 48 }, { 24, 120, 200 }, { 180, 240, 255 } };
//       jet_palette_gradient(RAMP_FIRST, RAMP_LAST, stops, (int)(sizeof stops / sizeof stops[0]));
//       jet_cycle_init(&water, RAMP_FIRST, RAMP_LAST, 8.0f, JET_CYCLE_FORWARD);
//   }
//
//   void _update(float dt) {
//       jet_cycle_update(&water, dt);
//   }
//
//   void _draw(void) {
//       jet_cls(RAMP_FIRST);
//       for (int y = 0; y < JET_HEIGHT; y += STRIPE_HEIGHT) {
//           const int colour = RAMP_FIRST + (y / STRIPE_HEIGHT) % (RAMP_LAST - RAMP_FIRST + 1);
//           jet_rectfill(0, y, JET_WIDTH, STRIPE_HEIGHT, colour);
//       }
//   }
//
//   void _end(void) {}

/**
 * Constant: JET_CYCLE_FORWARD
 * Summary: Runs a colour cycle forward, from its first index towards its last.
 * Include: jet64.h
 * Notes: Give it as the mode to jet_make_color_cycle or jet_cycle_init when the colours should keep flowing the same way.
 * Example:
 *   JetColorCycle water = jet_make_color_cycle(32, 39, 8.0f, JET_CYCLE_FORWARD);
 */
#define JET_CYCLE_FORWARD   0
/**
 * Constant: JET_CYCLE_REVERSE
 * Summary: Runs a colour cycle backward, from its last index towards its first.
 * Include: jet64.h
 * Notes: Give it as the mode to jet_make_color_cycle or jet_cycle_init when the colours should keep flowing the other way.
 * Example:
 *   JetColorCycle water = jet_make_color_cycle(32, 39, 8.0f, JET_CYCLE_REVERSE);
 */
#define JET_CYCLE_REVERSE   1
/**
 * Constant: JET_CYCLE_PINGPONG
 * Summary: Runs a colour cycle forward to the end of its range, then backward to the start, and so on.
 * Include: jet64.h
 * Notes: Give it as the mode to jet_make_color_cycle or jet_cycle_init when the flow should turn around smoothly instead of jumping back.
 * Example:
 *   JetColorCycle glow = jet_make_color_cycle(48, 55, 6.0f, JET_CYCLE_PINGPONG);
 */
#define JET_CYCLE_PINGPONG  2

// The shape of the live palette: 256 entries of 4 bytes each (red, green, blue, alpha).
#define JET_PALETTE_SIZE       256
#define JET_PALETTE_LUT_BYTES  (JET_PALETTE_SIZE * 4)

/**
 * Constant: JET_PALETTE_KEY_INDEX
 * Summary: The one palette index Jet64 sets aside as see-through: the colour key, which every drawing call that skips index 0 is skipping.
 * Include: jet64.h
 * Notes: It gives the reserved entry a name, so your code never writes a bare 0 for it. jet_spr, jet_map_draw_view and jet_triblit skip every source pixel that holds it, and jet_palette_ink_map maps it to itself, because a recolour must never invent a pixel where the artwork has none. By custom it is solid black. If you repaint it on purpose, you change what "empty" looks like everywhere at once.
 * Example:
 *   uint8_t gold = jet_palette_nearest(GOLD, JET_PALETTE_KEY_INDEX + 1, JET_PALETTE_SIZE - 1);
 */
#define JET_PALETTE_KEY_INDEX  0

/**
 * Struct: JetRgb
 * Summary: One colour as three bytes, red, green and blue (a 24-bit colour). It is the colour itself, the kind a gradient stop names, not the palette index that later picks it out.
 * Fields:
 *   - r: How much red, from 0 to 255.
 *   - g: How much green, from 0 to 255.
 *   - b: How much blue, from 0 to 255.
 * Include: jet_palette.h
 * Notes: You can write one with braces, so a list of stops reads as the colours it names, for example static const JetRgb fire[] = { {0,0,0}, {255,0,0}, {255,255,0} }. Do not mix it up with JetColorRamp in jet_particles.h, which holds a list of palette index numbers, not colours.
 * Example:
 *   JetRgb orange = { 255, 128, 0 };
 */
typedef struct {
    uint8_t r, g, b;
} JetRgb;

/**
 * Struct: JetPaletteSnapshot
 * Summary: A copy of all 256 palette entries that you keep, so you can put the palette back after a temporary effect.
 * Fields:
 *   - lut: The whole palette as 1,024 bytes: 256 entries of red, green, blue and alpha.
 * Include: jet64.h
 * Notes: Fill it with jet_palette_snapshot and put it back with jet_palette_restore. It owns its own bytes, so it can live on the stack, in a static variable, or in WRAM, the cartridge's working memory.
 * Example:
 *   JetPaletteSnapshot saved;
 *   jet_palette_snapshot(&saved);
 *   // ...temporary palette changes...
 *   jet_palette_restore(&saved);
 */
typedef struct {
    uint8_t lut[JET_PALETTE_LUT_BYTES];
} JetPaletteSnapshot;

/**
 * Struct: JetColorCycle
 * Summary: The running state of a colour cycle: a run of palette entries, first to last inclusive, that keeps shifting along by one so the colours appear to flow.
 * Fields:
 *   - start_idx: The first palette index in the run, included.
 *   - end_idx: The last palette index in the run, included.
 *   - speed: How many one-entry shifts happen each second. Zero or less means the cycle never moves.
 *   - mode: JET_CYCLE_FORWARD, JET_CYCLE_REVERSE or JET_CYCLE_PINGPONG.
 *   - timer: Kept by the runtime: the seconds gathered so far towards the next shift.
 *   - direction: Kept by the runtime: which way ping-pong mode is going right now.
 *   - step_count: Kept by the runtime: how many shifts the current ping-pong leg has made.
 *   - active: Set it to false to pause the cycle. Its timing is kept, so setting it true again carries on where it left off.
 * Include: jet64.h
 * Notes: Make one with jet_make_color_cycle or jet_cycle_init, then pass the same one to jet_cycle_update from _update. The cycle changes the live palette itself and owns no memory outside the struct.
 * Example:
 *   JetColorCycle water = jet_make_color_cycle(32, 39, 8.0f, JET_CYCLE_FORWARD);
 */
typedef struct {
    uint8_t start_idx;
    uint8_t end_idx;
    float speed;
    uint8_t mode;
    float timer;
    uint8_t direction;   // 0 = forward, 1 = reverse
    uint8_t step_count;
    bool active;
} JetColorCycle;

/**
 * Function: jet_make_color_cycle
 * Summary: Builds a JetColorCycle, ready to run.
 * Parameters:
 *   - start_idx: The first palette index in the run.
 *   - end_idx: The last palette index in the run.
 *   - speed: How many shifts each second.
 *   - mode: JET_CYCLE_FORWARD, JET_CYCLE_REVERSE or JET_CYCLE_PINGPONG.
 * Returns: The filled-in JetColorCycle, by value.
 * Include: jet_palette.h
 * Notes: The value owns no memory. Keep it somewhere that lasts, and pass it to jet_cycle_update from _update.
 * Example:
 *   JetColorCycle c = jet_make_color_cycle(16, 23, 10.0f, JET_CYCLE_FORWARD);
 */
static inline JetColorCycle jet_make_color_cycle(uint8_t start_idx, uint8_t end_idx, float speed, uint8_t mode) {
    JetColorCycle c;
    c.start_idx = start_idx;
    c.end_idx = end_idx;
    c.speed = speed;
    c.mode = mode;
    c.timer = 0.0f;
    c.direction = 0;
    c.step_count = 0;
    c.active = true;
    return c;
}

/**
 * Function: jet_palette_set
 * Summary: Sets one palette entry to a colour you give as red, green and blue, and tells the host the palette has changed.
 * Parameters:
 *   - index: Which entry to change, from 0 to 255.
 *   - r: How much red, from 0 to 255.
 *   - g: How much green, from 0 to 255.
 *   - b: How much blue, from 0 to 255.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: The entry's alpha is set to fully solid for you. Index 0 is solid black by custom in Jet64, so leave it alone unless your cartridge is changing it on purpose.
 * Example:
 *   jet_palette_set(32, 255, 128, 0);
 */
void jet_palette_set(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Function: jet_palette_put
 * Summary: Sets one palette entry like jet_palette_set, but only when it does not already hold that colour — so re-asserting a colour every frame tells the host nothing has changed.
 * Parameters:
 *   - index: Which entry to change, from 0 to 255.
 *   - r: How much red, from 0 to 255.
 *   - g: How much green, from 0 to 255.
 *   - b: How much blue, from 0 to 255.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: Every jet_palette_set tells the host the palette has changed, and the Workstation answers by uploading the whole palette to the GPU — so a cart that re-sets its colours every frame pays for an upload every frame, even when nothing moved. jet_palette_put compares first: an entry already holding the colour, fully solid, is left alone and the host is told nothing; any other entry is written exactly as jet_palette_set writes it. Reach for it whenever you set colours from _update or _draw, and keep jet_palette_set for a change you mean the host to hear every time. An entry left alone does not outrank a jet_load_palette arriving in the same frame the way a write does; if you re-assert every frame, the entry is back on the next one.
 * Example:
 *   // Keep the HUD's ink true every frame, for free while it already is.
 *   jet_palette_put(HUD_INK, 255, 128, 0);
 */
void jet_palette_put(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Function: jet_palette_get
 * Summary: Reads the colour of one palette entry, as red, green and blue.
 * Parameters:
 *   - index: Which entry to read, from 0 to 255.
 *   - r: Where to put the red part, or NULL if you do not want it.
 *   - g: Where to put the green part, or NULL if you do not want it.
 *   - b: Where to put the blue part, or NULL if you do not want it.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: A NULL pointer is skipped without harm. It reads the live palette, so it sees any change a colour cycle has made.
 * Example:
 *   uint8_t r, g, b;
 *   jet_palette_get(32, &r, &g, &b);
 */
void jet_palette_get(uint8_t index, uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * Function: jet_palette_gradient
 * Summary: Fills a run of palette entries with a smooth gradient, blending from each colour you give to the next.
 * Parameters:
 *   - first_index: The entry the first colour is written to, from 0 to 255.
 *   - last_index: The entry the last colour is written to, from 0 to 255. It may be BELOW first_index, which lays the same gradient the other way, downward through the entries.
 *   - stops: The colours to blend between, in order, starting at first_index. NULL writes nothing.
 *   - stop_count: How many colours the list holds. More than JET_PALETTE_SIZE is cut down to JET_PALETTE_SIZE. Below 1 writes nothing; exactly 1 fills the whole run with that one colour.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: The stops are spread evenly along the run. The first lands exactly on first_index and the last exactly on last_index, byte for byte, and every entry between two stops is a straight blend of the pair. Two stops give a plain ramp; three pass through the middle colour halfway along; each further stop adds another leg. Entries outside first_index to last_index are never touched, so you can build a palette one band at a time. If the run is too short to hold every stop, the stops with no entry to land on are skipped. The maths is all whole numbers, so the result is the same bit for bit on every host, and it is cheap enough to rebuild every frame for a moving ramp.
 * Example:
 *   // A fire ramp over the whole palette: black at 0, red at index 127, yellow at 255.
 *   static const JetRgb fire[] = { {0, 0, 0}, {255, 0, 0}, {255, 255, 0} };
 *   jet_palette_gradient(0, JET_PALETTE_SIZE - 1, fire, (int)(sizeof(fire) / sizeof(fire[0])));
 *
 *   // Or one band of 16 entries, sky blue into white, leaving the rest of the palette alone.
 *   static const JetRgb sky[] = { {40, 90, 200}, {255, 255, 255} };
 *   jet_palette_gradient(32, 47, sky, 2);
 */
void jet_palette_gradient(uint8_t first_index, uint8_t last_index, const JetRgb* stops, int stop_count);

/**
 * Function: jet_palette_nearest
 * Summary: Tells you which palette entry, within a range of indexes, is closest to a colour you give.
 * Parameters:
 *   - color: The colour you want, as red, green and blue, rather than a palette index guessed at.
 *   - first_index: The first entry the answer may be, from 0 to 255.
 *   - last_index: The last entry the answer may be, included. If the pair is the wrong way round it names the same range.
 * Returns: The index in the range whose colour is closest to color, measured by the squared distance between the red, green and blue parts. On a tie the lower index wins.
 * Include: jet_palette.h
 * Notes: This is how a cartridge names a colour instead of remembering an index number. Write the colour you mean, let the palette say which entry holds it, and if you import a new palette the answer updates by itself. The range keeps a reserved entry out of the answer: start at JET_PALETTE_KEY_INDEX + 1 so the answer can never be the see-through key, or narrow it further to keep the answer inside one band. The maths is all whole numbers, so the same palette gives the same index on every runtime. The search looks at every entry in the range, which makes it a cost for building tables: find your colours once in _init, not once for every pixel.
 * Example:
 *   static const JetRgb GOLD = { 255, 200, 64 };
 *   uint8_t gold = jet_palette_nearest(GOLD, JET_PALETTE_KEY_INDEX + 1, JET_PALETTE_SIZE - 1);
 */
uint8_t jet_palette_nearest(JetRgb color, uint8_t first_index, uint8_t last_index);

/**
 * Function: jet_palette_ink_map
 * Summary: Builds the table of 256 entries that prints a baked sprite font in a new ink colour, keeping its smooth edges and its outline ring.
 * Parameters:
 *   - out_map: Where to write the table, JET_PALETTE_SIZE entries long; every entry is filled. NULL is ignored without harm.
 *   - foot: The palette index of the background the face was baked over: the foot of its ink ramp, the run of colours from background to solid fill.
 *   - head: The palette index of the face's solid fill: the head of that same ramp.
 *   - ink: The colour that head is to become, as red, green and blue.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: Point TextFormat.palette_map at the table and every text call prints the same letter cells in the new colour. One baked face can speak in as many voices as you build tables for, with no second bake, no second sprite-font slot, and not one sprite cell spent on a colour.
 *
 *        It swaps colours; it never paints over them. Stamping one colour onto every visible pixel is what the drop shadow does, and that would flatten both the soft anti-aliased edge and the outline ring that a Sprite Font Lab bake spends its quality on. Instead, each palette entry is projected onto the line from foot to head, keeps its position along that line, and is replaced by the palette entry nearest the colour at that same position on the line towards ink (found with jet_palette_nearest, searched from JET_PALETTE_KEY_INDEX + 1 upward). So the ring stays the colour it was baked in, and the edge stays a smooth run.
 *
 *        foot and head are the two colours the bake declared. In the Sprite Font Lab they are the recipe's blend_background and fill_top. JET_PALETTE_KEY_INDEX maps to itself.
 *
 *        The table is plain data that belongs to you, so it can live in a static variable or in WRAM, the cartridge's working memory, and it must stay alive for as long as any jet_print_ext call points at it. Building one searches the whole palette for every entry, so build your cartridge's voices once in _init. A colour cycle that runs afterwards moves the bake, never the ink, because the table has already been worked out.
 * Example:
 *   static const JetRgb RUBY = { 255, 40, 40 };
 *   static uint8_t ruby_map[JET_PALETTE_SIZE];
 *
 *   void _init(void) {
 *       jet_sprite_font(0, 512, 16, 16, JET_SFONT_DEFAULTS);
 *       jet_palette_ink_map(ruby_map, JET_PALETTE_KEY_INDEX, 15, RUBY);
 *   }
 *
 *   void _draw(void) {
 *       TextFormat fmt = jet_make_text_format(JET_FONT_SPRITE(0), 0, 16, 1, 2, 1.0f, 0, 0);
 *       fmt.palette_map = ruby_map;
 *       jet_print_ext("RUBY", 16, 16, &fmt);
 *   }
 */
void jet_palette_ink_map(uint8_t* out_map, uint8_t foot, uint8_t head, JetRgb ink);

/**
 * Function: jet_cycle_init
 * Summary: Sets up a colour cycle over a run of palette entries, first to last inclusive, in a JetColorCycle you already have.
 * Parameters:
 *   - cycle: The JetColorCycle to set up.
 *   - start: The first palette index in the run.
 *   - end: The last palette index in the run.
 *   - speed: How many shifts each second.
 *   - mode: JET_CYCLE_FORWARD, JET_CYCLE_REVERSE or JET_CYCLE_PINGPONG.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: If start and end are the wrong way round they are swapped for you. The cycle is active straight away; set cycle->active to false to pause it.
 * Example:
 *   JetColorCycle water;
 *   jet_cycle_init(&water, 48, 55, 8.0f, JET_CYCLE_PINGPONG);
 */
void jet_cycle_init(JetColorCycle* cycle, uint8_t start, uint8_t end, float speed, uint8_t mode);

/**
 * Function: jet_cycle_update
 * Summary: Moves a colour cycle on by dt seconds, and shifts the live palette each time enough time has gathered for a step.
 * Parameters:
 *   - cycle: The JetColorCycle, set up earlier with jet_cycle_init or jet_make_color_cycle.
 *   - dt: The time since the last call, in seconds.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: Nothing happens when the cycle is paused, covers fewer than two entries, or has a speed of zero or less. Each shift tells the host the palette has changed.
 * Example:
 *   jet_cycle_update(&water, dt);
 */
void jet_cycle_update(JetColorCycle* cycle, float dt);

/**
 * Function: jet_load_palette
 * Summary: Asks the host to load a whole 256-colour palette from the cartridge's own palette files.
 * Parameters:
 *   - name: The palette's name in the cartridge's palettes/ folder, without the .pal ending.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: The host finds the file and applies it in its own time, not before this call returns; the request travels through the palette command block the cartridge shares with the host. A name longer than 254 bytes is cut short. NULL is ignored.
 * Example:
 *   jet_load_palette("dungeon_crawl");
 */
void jet_load_palette(const char* name);

/**
 * Function: jet_palette_snapshot
 * Summary: Copies the whole live palette into a snapshot that you keep.
 * Parameters:
 *   - out: The snapshot to fill with all 256 entries of red, green, blue and alpha. NULL is ignored without harm.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: It goes with jet_palette_restore. Take a snapshot before you switch to a temporary look, such as a full-screen map or a menu skin, so the original colours can be put back exactly when it ends. The snapshot is plain data with nothing hidden behind it, so it is safe to call again at any time, and safe to keep on the stack or in WRAM, the cartridge's working memory.
 * Example:
 *   JetPaletteSnapshot saved;
 *   jet_palette_snapshot(&saved);
 *   jet_palette_set(1, 255, 255, 255);
 *   jet_palette_restore(&saved);
 */
void jet_palette_snapshot(JetPaletteSnapshot* out);

/**
 * Function: jet_palette_restore
 * Summary: Puts the whole live palette back from a snapshot.
 * Parameters:
 *   - in: A snapshot filled earlier by jet_palette_snapshot. NULL is ignored without harm.
 * Returns: Nothing.
 * Include: jet_palette.h
 * Notes: It writes all 256 entries back and tells the host the palette has changed, so the palette is sent to the screen again on the next frame. A snapshot that was never filled holds whatever bytes happened to be there, and restoring it writes those into the palette, so only restore a snapshot you have taken.
 * Example:
 *   jet_palette_restore(&saved);
 */
void jet_palette_restore(const JetPaletteSnapshot* in);

#endif // JET_PALETTE_H
