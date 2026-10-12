// runtime/jet_metaball.h
//
// Soft glowing blobs that melt into one shape when they come close. Every blob
// adds heat to the pixels around it, most at its centre and less towards its
// edge, and where the heat is high enough the pixel lights up, so two blobs
// close together merge into one. This is an opt-in module that writes colours
// straight onto the screen; include "jet_metaball.h" yourself.
//
// A JetMetaballField only draws. Your cart still owns every blob's position,
// radius, faction, movement, collision and lifetime (often in a JetPool). Each
// frame, feed those objects to the field in this exact order: clear once,
// stamp every blob, then resolve once after you have drawn the background.
// Resolve leaves low-heat pixels untouched, so whatever you drew first stays
// visible around the glow.
//
// Set up one field that lasts the whole game, once, in _init(), and check the
// bool it returns. downscale is the number of screen pixels along one side of
// each heat cell: 1 is the finest and the most expensive;
// JET_METABALL_DEFAULT_DOWNSCALE (2) is the normal balance; a larger number is
// faster and chunkier. The WRAM it reserves lasts as long as the cart does.
//
// Each faction has its own heat field and its own colour ramp. Stamps from the
// same faction add together and grow bridges between blobs; where two factions
// meet, the boundary stays clean. A ramp turns a heat from 0 to 255 into a
// palette index, so choose a run of palette colours that sit next to each
// other and go from a dim halo up to a bright core. Build it from a few
// JetMetaballRampStop values in rising heat order, then install one ramp for
// each faction.
//
// Complete two-faction glow example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_metaball.h"
//
//   enum { FACTION_COUNT = 2 };
//   static JetMetaballField glow;
//   static bool glow_ready;
//   static float phase;
//
//   static void set_faction_ramp(int faction, const JetMetaballRampStop* stops, int stop_count) {
//       uint8_t ramp[JET_METABALL_RAMP_SIZE];
//       jet_metaball_build_ramp(ramp, stops, stop_count);
//       jet_metaball_set_ramp(&glow, faction, ramp);
//   }
//
//   void _init(void) {
//       static const JetMetaballRampStop blue[] = {
//           { 0, 0 }, { 40, 24 }, { 255, 31 }
//       };
//       static const JetMetaballRampStop red[] = {
//           { 0, 0 }, { 40, 40 }, { 255, 47 }
//       };
//       glow_ready = jet_metaball_init(
//           &glow, JET_WIDTH, JET_HEIGHT,
//           JET_METABALL_DEFAULT_DOWNSCALE, FACTION_COUNT
//       );
//       if (glow_ready) {
//           set_faction_ramp(0, blue, (int)(sizeof blue / sizeof blue[0]));
//           set_faction_ramp(1, red, (int)(sizeof red / sizeof red[0]));
//       }
//   }
//
//   void _update(float dt) {
//       if (!glow_ready) return;
//       phase += dt;
//       jet_metaball_clear(&glow);
//       jet_metaball_stamp(&glow, 0, 220.0f + 36.0f * sinf(phase),
//                          260.0f, 52.0f, JET_METABALL_INTENSITY_MAX);
//       jet_metaball_stamp(&glow, 0, 292.0f - 36.0f * sinf(phase),
//                          260.0f, 52.0f, JET_METABALL_INTENSITY_MAX);
//       jet_metaball_stamp(&glow, 1, 420.0f, 260.0f, 60.0f,
//                          JET_METABALL_INTENSITY_MAX);
//   }
//
//   void _draw(void) {
//       jet_cls(0);                         // draw the background first
//       jet_metaball_resolve(&glow, 0, 0); // resolve after every stamp
//   }
//
//   void _end(void) {
//       // The field's WRAM lasts as long as the cart does; there is nothing to release.
//   }
//
// <nocodex>
//
// METABALL GLOW FIELD — an OPT-IN runtime module and the visual heart of
// "Attack of the Sentient Plasmas" (A.S.P.). Like jet_plasma.h, jet_pool.h
// and jet_pathfind.h it is deliberately NOT pulled in by jet64.h: a cart that
// wants glowing, merging plasma blobs includes "jet_metaball.h" itself, and
// only then does the compiler's dependency resolver pull jet_metaball.c into
// the build. Carts that never use it pay zero compile cost. This header IS the
// sandbox boundary for the module.
//
// WHAT IT DOES: it renders a set of soft, glowing, continuously-morphing blobs
// of light directly into the 8bpp palette-indexed framebuffer — with NO
// additive-blend hardware and NO bloom post-process. The "glow" is authored as
// a brightness/colour RAMP in the palette; intensity is mapped through that ramp
// (heat -> palette index). The constraint becomes the whole aesthetic.
//
// THE ALGORITHM (the elegant, blazingly-fast core):
//   1. ONE 8-bit "heat" field PER FACTION, reserved ONCE from WRAM at init, at a
//      DOWNSCALED resolution (default half-res; the chunky upscale is on-brand).
//      No malloc, ever.
//   2. Each frame the cart clears the fields (bounded to the dirty bounding box
//      of last frame's stamps, NOT the whole screen) and stamps each blob.
//   3. STAMP: a precomputed radial-falloff kernel (baked into a LUT once at
//      init) is added into the blob's OWN faction field with a SATURATING 8-bit
//      add. Two nearby SAME-faction stamps sum past the iso-threshold in the gap
//      between them and grow a connective bridge — THIS is the metaball merge,
//      and it costs zero special-case code.
//   4. RESOLVE: composite the per-faction fields to VRAM. For each output pixel
//      the faction with the MAXIMUM heat wins, and that heat is mapped through
//      THAT faction's ramp. Heat below the iso-threshold is transparent (the
//      write is skipped, so the background shows through).
//   5. Upscale (nearest-neighbour) from the field to the VRAM region,
//      camera- and clip-aware.
//
// WHY THIS IS RIGHT — emergent properties to preserve:
//   * MERGING IS FREE  — two same-faction blobs grow a bridge automatically
//                        because their fields sum past the iso in the gap.
//   * SPLITTING IS FREE — the cart just spawns two blob entities; this renderer
//                        draws them apart with no merge between them.
//   * GLOW IS THE RAMP — core -> halo -> transparent is the ramp, not a blur.
//   * MIND-CONTROL IS FREE — when the cart flips a blob's faction, you simply
//                        stamp it into a DIFFERENT faction field next frame. Its
//                        connective tissue detaches from the old faction and
//                        re-forms with the new one automatically, and the clean
//                        max-heat boundary keeps enemies and allies visually
//                        distinct even while they touch.
//
// SEPARATION OF CONCERNS: this module does NOT own the blob entity list. The
// cart owns its blobs (e.g. in a jet_pool) and feeds positions/radii/factions
// in each frame via jet_metaball_stamp(). The field is a pure renderer.
//
// PERFORMANCE: half-res field + dirty-AABB-bounded clears + a baked stamp LUT +
// saturating adds + a clip-bounded resolve land comfortably inside a 120 fps
// (8.3 ms) budget even when the field fills most of the screen.
// Complete merging-glow example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_metaball.h"
//
//   static JetMetaballField glow;
//   static float phase;
//
//   void _init(void) {
//       static const JetMetaballRampStop stops[] = { { 0, 0 }, { 40, 24 }, { 255, 31 } };
//       uint8_t ramp[JET_METABALL_RAMP_SIZE];
//       if (jet_metaball_init(&glow, JET_WIDTH, JET_HEIGHT, JET_METABALL_DEFAULT_DOWNSCALE, 1)) {
//           jet_metaball_build_ramp(ramp, stops, (int)(sizeof stops / sizeof stops[0]));
//           jet_metaball_set_ramp(&glow, 0, ramp);
//       }
//   }
//
//   void _update(float dt) {
//       const float centre_x = (float)JET_WIDTH / 2.0f;
//       phase += dt;
//       jet_metaball_clear(&glow);
//       jet_metaball_stamp(&glow, 0, centre_x - 36.0f * sinf(phase), JET_HEIGHT / 2.0f, 52.0f, JET_METABALL_INTENSITY_MAX);
//       jet_metaball_stamp(&glow, 0, centre_x + 36.0f * sinf(phase), JET_HEIGHT / 2.0f, 52.0f, JET_METABALL_INTENSITY_MAX);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_metaball_resolve(&glow, 0, 0);
//   }
//
//   void _end(void) {}
// </nocodex>

#ifndef JET_METABALL_H
#define JET_METABALL_H

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool (sandbox-safe)

/**
 * Constant: JET_METABALL_MAX_FACTIONS
 * Summary: The most factions one JetMetaballField can hold, each with its own heat field: four.
 * Include: jet_metaball.h
 * Notes: Give jet_metaball_init a faction_count from 1 up to this value. Resolve looks at every faction for every pixel it draws, so use only as many as the game needs.
 * Example:
 *   jet_metaball_init(&blobs, JET_WIDTH, JET_HEIGHT, 2, JET_METABALL_MAX_FACTIONS);
 */
#define JET_METABALL_MAX_FACTIONS 4

/**
 * Constant: JET_METABALL_DEFAULT_DOWNSCALE
 * Summary: The recommended size of a heat cell, in screen pixels, for a good balance of quality and speed: 2.
 * Include: jet_metaball.h
 * Notes: A value of 2 makes a field at half the screen's resolution, which quarters the work on the heat buffers while keeping the chunky glow the effect is meant to have. Pass 1 for full resolution, or a bigger whole number for a coarser, faster field.
 * Example:
 *   jet_metaball_init(&blobs, JET_WIDTH, JET_HEIGHT, JET_METABALL_DEFAULT_DOWNSCALE, 2);
 */
#define JET_METABALL_DEFAULT_DOWNSCALE 2

/**
 * Constant: JET_METABALL_DEFAULT_THRESHOLD
 * Summary: The heat a pixel needs before it lights up, unless you set your own: 32. Below it, a resolved pixel is left see-through.
 * Include: jet_metaball.h
 * Notes: A lower threshold makes blobs, and the bridges between them, fatter; a higher one shrinks them back towards their hot cores. jet_metaball_init installs this value, and jet_metaball_set_threshold changes it.
 * Example:
 *   jet_metaball_set_threshold(&blobs, JET_METABALL_DEFAULT_THRESHOLD);
 */
#define JET_METABALL_DEFAULT_THRESHOLD 32

/**
 * Constant: JET_METABALL_RAMP_SIZE
 * Summary: The number of entries in a metaball colour ramp, one for every possible heat value: 256.
 * Include: jet_metaball.h
 * Notes: A heat is an 8-bit number, 0 to 255, and the ramp holds one palette index for each. Use this constant when you declare a ramp to pass to jet_metaball_set_ramp or jet_metaball_build_ramp.
 * Example:
 *   uint8_t ramp[JET_METABALL_RAMP_SIZE];
 */
#define JET_METABALL_RAMP_SIZE 256

// The number of entries in the falloff table baked at init. It is indexed by
// the SQUARED distance from a stamp's centre, scaled so that the rim is 1,
// which is why the hot inner loop never has to call sqrtf.
#define JET_METABALL_KERNEL_LUT_SIZE 256

// The largest value a heat cell can hold. The saturating add stops here, so a
// summed field never wraps round past it (the muddy artefact a plain
// `(uint8_t)sum` would produce). It is also the top index into a ramp or into
// the falloff table.
#define JET_METABALL_HEAT_MAX 0xFF

/**
 * Constant: JET_METABALL_INTENSITY_MAX
 * Summary: Full strength for jet_metaball_stamp: 255.
 * Include: jet_metaball.h
 * Notes: A stamp's intensity is a number from 0 to 255. Passing this maximum lays the baked falloff down at full strength; a smaller value softens the blob in proportion.
 * Example:
 *   jet_metaball_stamp(&blobs, faction, x, y, radius, JET_METABALL_INTENSITY_MAX);
 */
#define JET_METABALL_INTENSITY_MAX 0xFF

/**
 * Struct: JetMetaballRampStop
 * Summary: One control point on the way from heat to colour. Give a few of these to jet_metaball_build_ramp and it fills in all 256 entries of a ramp for you, so you never write them by hand.
 * Fields:
 *   - heat: The heat, 0 to 255, at which this stop's colour applies.
 *   - index: The palette index, 0 to 255, the ramp gives at that heat.
 * Include: jet_metaball.h
 * Notes: Stops are read in rising heat order. Use a run of palette entries that sit next to each other and form a brightness or colour gradient, so the colours filled in between the stops read as a smooth glow.
 * Example:
 *   JetMetaballRampStop stops[] = { { 0, 0 }, { 48, 17 }, { 255, 23 } };
 */
typedef struct {
    uint8_t heat;
    uint8_t index;
} JetMetaballRampStop;

/**
 * Struct: JetMetaballField
 * Summary: A set of heat fields, one for each faction, at a reduced resolution, plus each faction's colour ramp and a falloff table baked at init. Its memory comes from WRAM, reserved once. Each frame you stamp blobs into it and resolve it onto the screen; blobs of one faction merge, blobs of different factions keep a clean boundary, and heat becomes a glow through the ramp.
 * Fields:
 *   - fields: One reduced-resolution heat buffer per faction, field_w*field_h bytes each, or NULL for a faction that is unused or failed to reserve.
 *   - ramps: Each faction's table from heat to palette index, 256 entries each.
 *   - dirty_x0/dirty_y0/dirty_x1/dirty_y1: For each faction, the box of cells stamped since the last clear, in FIELD cells with both edges included; empty when x0 > x1. It limits each frame's clear, so a mostly empty screen costs almost nothing.
 *   - kernel_lut: The baked falloff, indexed by squared distance from the centre, scaled so that 0 is the centre and KERNEL_LUT_SIZE-1 is the rim.
 *   - faction_count: How many factions are in use, 1 to JET_METABALL_MAX_FACTIONS.
 *   - field_w/field_h: The size of the reduced field, in cells.
 *   - screen_w/screen_h: The size of the full-resolution area the field is scaled up to, in pixels.
 *   - downscale: The size of one field cell, in screen pixels (1 or more).
 *   - threshold: The heat a pixel needs to light up; heat strictly below it stays see-through.
 * Include: jet_metaball.h
 * Notes: After jet_metaball_init the field never reserves more memory; clear, stamp and resolve all work on the same fixed buffers. If WRAM ran out at init the field is left inert, with every fields pointer NULL, and stamp and resolve safely do nothing. Do not look inside the fields; use the calls below.
 * Example:
 *   static JetMetaballField blobs;
 *   void _init(void) { jet_metaball_init(&blobs, JET_WIDTH, JET_HEIGHT, 2, 2); }
 */
typedef struct {
    uint8_t* fields[JET_METABALL_MAX_FACTIONS];
    uint8_t  ramps[JET_METABALL_MAX_FACTIONS][JET_METABALL_RAMP_SIZE];

    int dirty_x0[JET_METABALL_MAX_FACTIONS];
    int dirty_y0[JET_METABALL_MAX_FACTIONS];
    int dirty_x1[JET_METABALL_MAX_FACTIONS];
    int dirty_y1[JET_METABALL_MAX_FACTIONS];

    uint8_t kernel_lut[JET_METABALL_KERNEL_LUT_SIZE];

    int faction_count;
    int field_w;
    int field_h;
    int screen_w;
    int screen_h;
    int downscale;
    uint8_t threshold;
} JetMetaballField;

/**
 * Function: jet_metaball_init
 * Summary: Sets up a metaball field: reserves one reduced-resolution heat field per faction from WRAM's bump allocator, bakes the falloff table, and gives every faction a plain starting ramp.
 * Parameters:
 *   - field: The field to set up. It must stay valid for every later call.
 *   - screen_w: The width, in pixels, of the full-resolution area the field is scaled up to. Must be above 0.
 *   - screen_h: The height, in pixels, of that area. Must be above 0.
 *   - downscale: The size of one field cell, in screen pixels. Pass 0 for JET_METABALL_DEFAULT_DOWNSCALE. A value below 1 is treated as 1.
 *   - faction_count: How many factions, 1 to JET_METABALL_MAX_FACTIONS. A value outside that range is pulled back into it.
 * Returns: true when it worked; false if field is NULL, a dimension is not above 0, the field is too big for the allocator's 32-bit request, or WRAM ran out.
 * Include: jet_metaball.h
 * Notes: It reserves faction_count buffers, each ceil(screen_w/downscale)*ceil(screen_h/downscale) bytes. The allocator hands memory out and never takes it back, so call this once, from _init(). On failure every field pointer is left NULL, so stamp and resolve do nothing even if you ignore the false answer (the same rule jet_redraw_init follows). Every faction starts with an identity ramp, where each heat maps to the palette index of the same number; call jet_metaball_set_ramp to give it a real glow.
 * Example:
 *   JetMetaballField mb;
 *   bool ok = jet_metaball_init(&mb, JET_WIDTH, JET_HEIGHT, JET_METABALL_DEFAULT_DOWNSCALE, 2);
 */
bool jet_metaball_init(JetMetaballField* field, int screen_w, int screen_h, int downscale, int faction_count);

/**
 * Function: jet_metaball_set_ramp
 * Summary: Installs a faction's colour ramp: the table that says which palette index resolve writes for each heat value.
 * Parameters:
 *   - field: A field set up with jet_metaball_init.
 *   - faction: Which faction, 0 to faction_count-1.
 *   - ramp: 256 palette indices; ramp[h] is the colour written for heat h.
 * Returns: Nothing.
 * Include: jet_metaball.h
 * Notes: A NULL field, a NULL ramp, or a faction out of range does nothing. Heat strictly below the threshold is left see-through whatever the ramp says, so ramp[0] normally does not matter. Make the ramp a run of palette entries that sit next to each other and get brighter up to ramp[255], the hot core; jet_metaball_build_ramp makes one from a few stops.
 * Example:
 *   uint8_t ramp[JET_METABALL_RAMP_SIZE];
 *   JetMetaballRampStop stops[] = {{32,16},{160,20},{255,23}};
 *   jet_metaball_build_ramp(ramp, stops, 3);
 *   jet_metaball_set_ramp(&mb, 0, ramp);
 */
void jet_metaball_set_ramp(JetMetaballField* field, int faction, const uint8_t ramp[JET_METABALL_RAMP_SIZE]);

/**
 * Function: jet_metaball_build_ramp
 * Summary: Fills in a 256-entry colour ramp from a few stops, blending the palette index in a straight line from each stop to the next.
 * Parameters:
 *   - out: The 256-entry ramp to fill.
 *   - stops: The control points, in rising heat order. Each pairs a heat with a palette index.
 *   - stop_count: How many stops, 1 or more.
 * Returns: Nothing.
 * Include: jet_metaball.h
 * Notes: A NULL out, NULL stops, or a stop_count below 1 does nothing. A heat below the first stop takes the first stop's index; a heat above the last takes the last stop's index. The blend is between palette INDEX numbers, so it assumes your glow occupies a run of palette entries that sit next to each other in order, which is the usual way to lay one out. The result belongs to no field, so you can pass one ramp to jet_metaball_set_ramp for several factions.
 * Example:
 *   uint8_t ramp[JET_METABALL_RAMP_SIZE];
 *   JetMetaballRampStop stops[] = {{0,0},{32,17},{255,23}};
 *   jet_metaball_build_ramp(ramp, stops, 3);
 */
void jet_metaball_build_ramp(uint8_t out[JET_METABALL_RAMP_SIZE], const JetMetaballRampStop* stops, int stop_count);

/**
 * Function: jet_metaball_set_threshold
 * Summary: Sets the heat a pixel needs before it lights up, which decides how fat the visible blob is compared with the radius you stamped.
 * Parameters:
 *   - field: A field set up with jet_metaball_init.
 *   - iso: The threshold heat. A pixel whose highest heat is strictly below it is skipped and left see-through.
 * Returns: Nothing.
 * Include: jet_metaball.h
 * Notes: A NULL field does nothing. A lower value grows the visible blob and lets bridges form across wider gaps; a higher value shrinks it to the hot core. The starting value is JET_METABALL_DEFAULT_THRESHOLD.
 * Example:
 *   jet_metaball_set_threshold(&mb, 24);
 */
void jet_metaball_set_threshold(JetMetaballField* field, uint8_t iso);

/**
 * Function: jet_metaball_clear
 * Summary: Sets every faction's heat back to zero, ready for this frame's stamps. It touches only the box that last frame's stamps reached.
 * Parameters:
 *   - field: A field set up with jet_metaball_init.
 * Returns: Nothing.
 * Include: jet_metaball.h
 * Notes: A NULL or inert field does nothing. Only the cells stamped since the previous clear are cleared, so a mostly empty screen costs almost nothing. Call it once a frame, before you stamp.
 * Example:
 *   jet_metaball_clear(&mb);
 */
void jet_metaball_clear(JetMetaballField* field);

/**
 * Function: jet_metaball_stamp
 * Summary: Adds one blob's glow into its faction's heat field. The baked falloff is scaled to the blob's radius and intensity and added on cell by cell, and the add stops at the top value instead of wrapping round.
 * Parameters:
 *   - field: A field set up with jet_metaball_init.
 *   - faction: Which faction's field to stamp into, 0 to faction_count-1. This is the whole of mind control: stamp a converted blob into a different faction.
 *   - cx: The blob's centre, across, in full-resolution pixels measured from the field's top-left corner (jet_metaball_resolve decides where that corner lands).
 *   - cy: The blob's centre, down, in the same pixels.
 *   - radius: The blob's radius, in the same pixels. The falloff is stretched to reach it.
 *   - intensity: The peak heat laid down at the centre, 0 to JET_METABALL_INTENSITY_MAX. The falloff drops to zero at the rim.
 * Returns: Nothing.
 * Include: jet_metaball.h
 * Notes: A NULL or inert field, a faction out of range, a radius of zero or less, or an intensity of zero does nothing. The add SATURATES at JET_METABALL_HEAT_MAX and never wraps, so same-faction stamps that overlap pile up into a bridge instead of rolling over into a dark muddy band. Any part of the blob outside the field is cut off. The area stamped widens the faction's dirty box, so the next clear covers it. The camera is NOT applied here; it is applied once, at resolve.
 * Example:
 *   jet_metaball_stamp(&mb, blob.faction, blob.x, blob.y, blob.radius, 255);
 */
void jet_metaball_stamp(JetMetaballField* field, int faction, float cx, float cy, float radius, uint8_t intensity);

/**
 * Function: jet_metaball_resolve
 * Summary: Draws the heat fields onto the screen, scaled up to full size. For each pixel the faction with the most heat wins, and its heat becomes a colour through that faction's ramp; a pixel below the threshold is left see-through. It respects the camera and the clip rectangle.
 * Parameters:
 *   - field: A field set up with jet_metaball_init.
 *   - dst_x: The world position, across, where the field's top-left corner lands on the screen. The camera's offset is taken off.
 *   - dst_y: The world position, down, where the field's top-left corner lands.
 * Returns: Nothing.
 * Include: jet_metaball.h
 * Notes: A NULL or inert field does nothing. The field's (0,0) cell lands at world (dst_x, dst_y); the camera's offset is taken off, and writing is kept inside the clip rectangle, both edges included, so even a boss blob that fills the screen touches only live pixels. Where two factions touch, the boundary is a clean line between the two heats, with no muddy merge; blobs of one faction merge. Call it after every stamp for the frame. For one full-screen field with no camera, pass 0 for both dst_x and dst_y and stamp in plain screen pixels.
 * Example:
 *   jet_metaball_resolve(&mb, 0, 0);
 */
void jet_metaball_resolve(JetMetaballField* field, int dst_x, int dst_y);

#endif // JET_METABALL_H
