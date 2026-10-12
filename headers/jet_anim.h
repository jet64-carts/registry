// runtime/jet_anim.h
#ifndef JET_ANIM_H
#define JET_ANIM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // IWYU pragma: keep (offsetof for ABI layout asserts)
#include <string.h>

// Make the pictures you drew in the Sprite Editor move. An animation is a list
// of cells shown one after another, and this header holds everything about one
// except the drawing: what an animation IS (JetAnimDef, and JetAnimFrame for one
// frame of a table), where a playing animation is up to (JetAnimState, the
// playhead), and every call that moves a playhead: play, pause, resume, stop,
// seek, switch and the rest. Drawing lives next door in jet_sprite.h, which
// includes this header, so a cart that has only ever included jet_sprite.h
// still sees every animation type it always did.
//
// Every animation is a JetAnimDef, a record of exactly 128 bytes that the
// Animation Editor writes and the runtime reads. Two kinds of animation share
// that one slot:
//
//   - RANGE (JET_ANIM_SOURCE_RANGE) -- the contiguous cells start_frame..end_frame,
//     every one held for the same 1/fps seconds. This is what the machine has
//     always had, and it steps on exactly the ticks it always has.
//   - TABLE (JET_ANIM_SOURCE_TABLE) -- frame_count JetAnimFrame records, each with
//     its own duration, flips, pixel offset, multi-cell span and event id.
//
// In plain words, a RANGE animation is a run of cells that sit next to each
// other in the Sprite Editor, from start_frame to end_frame, each shown for the
// same length of time, fps frames a second. A TABLE animation is a list of
// frame_count JetAnimFrame records, and each record picks its own cell, says
// how long it stays on screen, whether it is mirrored, how far to nudge it in
// pixels, how many cells it covers, and an event number your game can listen
// for. A slot that is all zeros reads back as a RANGE animation, which is why
// every animation authored before frame tables existed keeps playing unchanged.
//
// To start, include jet64.h. In _init, call jet_anim_load_ani once to get the
// animations the editor saved, find the one you want with jet_anim_find_by_name,
// and hand it to jet_anim_play with a JetAnimState of your own. Then call
// jet_anim_update every tick in _update and jet_anim_draw_grid every frame in
// _draw. Keep one JetAnimState for each thing that animates on its own. Time is
// counted in ticks, 120 a second, and a frame can never change more often than
// once a tick.
//
// Complete first-animation example (copy into main.c):
//
//   #include "jet64.h"
//
//   static JetAnimState walk;
//
//   void _init(void) {
//       uint32_t count = 0;
//       JetAnimDef* anims = jet_anim_load_ani("anims.ani", &count);
//       jet_set_sprite_grid(16, 16);
//       jet_anim_play(&walk, jet_anim_find_by_name(anims, count, "walk"), 16);
//   }
//   void _update(float dt) { jet_anim_update(&walk, dt); }
//   void _draw(void) { jet_cls(0); jet_anim_draw_grid(&walk, 100, 80, false, false, true); }
//   void _end(void) {}


/**
 * Constant: JET_ANIM_MODE_FORWARD
 * Summary: Plays the animation once, from its first frame to its last, then stops and stays on the last frame.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.mode. For a RANGE animation the first frame is start_frame and the last is end_frame. When the same run should repeat, use JET_ANIM_MODE_LOOP instead.
 * Example:
 *   def.mode = JET_ANIM_MODE_FORWARD;
 */
#define JET_ANIM_MODE_FORWARD  0
/**
 * Constant: JET_ANIM_MODE_REVERSE
 * Summary: Plays the animation once backwards, from its last frame to its first, then stops.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.mode. The frames are still start_frame through end_frame, exactly as you authored them; only the order they play in is turned round.
 * Example:
 *   def.mode = JET_ANIM_MODE_REVERSE;
 */
#define JET_ANIM_MODE_REVERSE  1
/**
 * Constant: JET_ANIM_MODE_PINGPONG
 * Summary: Plays the animation forwards, then backwards, then forwards again, for ever, so it never jumps from its last frame straight to its first.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.mode. Good for breathing, hovering, and any other motion that should turn round smoothly.
 * Example:
 *   def.mode = JET_ANIM_MODE_PINGPONG;
 */
#define JET_ANIM_MODE_PINGPONG 2
/**
 * Constant: JET_ANIM_MODE_LOOP
 * Summary: Plays the animation from its first frame to its last, then goes round again, for ever.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.mode. This is the usual choice for a walk, a run, or an idle cycle. If the definition's loop_start is not 0, each time round starts from that step instead of the first.
 * Example:
 *   def.mode = JET_ANIM_MODE_LOOP;
 */
#define JET_ANIM_MODE_LOOP     3
/**
 * Constant: JET_ANIM_MODE_RANDOM
 * Summary: Plays the animation's frames in a random order, picking a new frame from the whole range each time it steps.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.mode. Use it for a flicker, a spark, or anything that should look irregular on purpose.
 * Example:
 *   def.mode = JET_ANIM_MODE_RANDOM;
 */
#define JET_ANIM_MODE_RANDOM   4

/**
 * Constant: JET_ANIM_DIR_ROW
 * Summary: Steps the animation across the Sprite Editor, so each frame is the cell to the right of the one before.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.direction when you drew the frames side by side along one row of the Sprite Editor.
 * Example:
 *   def.direction = JET_ANIM_DIR_ROW;
 */
#define JET_ANIM_DIR_ROW 0
/**
 * Constant: JET_ANIM_DIR_COL
 * Summary: Steps the animation down the Sprite Editor, so each frame is the cell below the one before.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.direction when you drew the frames one under another down one column of the Sprite Editor.
 * Example:
 *   def.direction = JET_ANIM_DIR_COL;
 */
#define JET_ANIM_DIR_COL 1

/**
 * Constant: JET_ANIM_SOURCE_RANGE
 * Summary: Marks an animation as a RANGE animation: its frames are the cells from start_frame to end_frame, side by side in the Sprite Editor, each shown at the same rate, fps frames a second.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.frame_source. It is 0, the value a slot full of zeros already holds, so every animation authored before frame tables existed is a RANGE animation.
 * Example:
 *   def.frame_source = JET_ANIM_SOURCE_RANGE;
 */
#define JET_ANIM_SOURCE_RANGE 0
/**
 * Constant: JET_ANIM_SOURCE_TABLE
 * Summary: Marks an animation as a TABLE animation: its frames are frame_count JetAnimFrame records, read from the frame table starting at record frame_offset.
 * Include: jet64.h
 * Notes: Put it in JetAnimDef.frame_source. A table lets each frame have its own time on screen, its own mirroring, its own nudge in pixels, and cover more than one cell; a range gives every frame the same rate and one cell each.
 * Example:
 *   def.frame_source = JET_ANIM_SOURCE_TABLE;
 */
#define JET_ANIM_SOURCE_TABLE 1

/**
 * Constant: JET_ANIM_FRAME_FLIP_X
 * Summary: A bit for JetAnimFrame.flags that mirrors the frame left to right.
 * Include: jet64.h
 * Notes: It combines with the flip_x you pass to jet_anim_draw, so a frame you authored mirrored still turns to face the way your game asks.
 * Example:
 *   frame.flags |= JET_ANIM_FRAME_FLIP_X;
 */
#define JET_ANIM_FRAME_FLIP_X 0x01
/**
 * Constant: JET_ANIM_FRAME_FLIP_Y
 * Summary: A bit for JetAnimFrame.flags that mirrors the frame top to bottom.
 * Include: jet64.h
 * Notes: It combines with the flip_y you pass to jet_anim_draw.
 * Example:
 *   frame.flags |= JET_ANIM_FRAME_FLIP_Y;
 */
#define JET_ANIM_FRAME_FLIP_Y 0x02

/**
 * Constant: JET_ANIM_MIN_FRAME_US
 * Summary: The shortest time a frame can be on screen: one tick of the game, in whole microseconds. There are 120 ticks a second, so this is 8333.
 * Include: jet64.h
 * Notes: An animation can only change frame once per tick, so a JetAnimFrame.duration_us smaller than this still holds the frame for one whole tick. A duration of 0 is safe and means the same thing.
 * Example:
 *   frame.duration_us = JET_ANIM_MIN_FRAME_US; // one tick
 */
#define JET_ANIM_MIN_FRAME_US 8333u
/**
 * Constant: JET_ANIM_TU_PER_US
 * Summary: How many timeline units make one microsecond. A timeline unit, tu for short, is the whole-number clock the runtime counts animation time in.
 * Include: jet64.h
 * Notes: It is 120, the same as the number of ticks in a second, and that is what makes one tick a whole number of timeline units. The durations you author stay in microseconds; only the runtime counts in tu.
 * Example:
 *   uint64_t hold_tu = (uint64_t)frame.duration_us * JET_ANIM_TU_PER_US;
 */
#define JET_ANIM_TU_PER_US 120u
/**
 * Constant: JET_ANIM_TU_PER_TICK
 * Summary: One tick of the game in timeline units, the whole-number clock the runtime counts animation time in.
 * Include: jet64.h
 * Notes: Exactly 1 000 000 tu, because a tick is 1/120 of a second and there are 120 tu in a microsecond.
 * Example:
 *   uint64_t two_ticks = 2u * JET_ANIM_TU_PER_TICK;
 */
#define JET_ANIM_TU_PER_TICK 1000000u
/**
 * Constant: JET_ANIM_TU_PER_SECOND
 * Summary: One second in timeline units, the whole-number clock the runtime counts animation time in. It is a float, because it serves the one conversion that starts from seconds.
 * Include: jet64.h
 * Notes: 120 000 000.0f. Every other sum on the timeline is done in whole numbers.
 * Example:
 *   float seconds = (float)elapsed_tu / JET_ANIM_TU_PER_SECOND;
 */
#define JET_ANIM_TU_PER_SECOND 120000000.0f
/**
 * Constant: JET_ANIM_TICK_SNAP_TU
 * Summary: How close a frame's authored duration must be to a whole number of ticks for the runtime to treat it as exactly that many ticks.
 * Include: jet64.h
 * Notes: One microsecond. A tick is 8333.33 microseconds, so no whole number of microseconds can say "one tick" exactly. Without this snap the tiny leftover would pile up, and every few thousand frames one frame would be shown twice. With it, a duration within a microsecond of a whole number of ticks is read as exactly that many.
 * Example:
 *   frame.duration_us = 83333; // 10 ticks, snapped
 */
#define JET_ANIM_TICK_SNAP_TU JET_ANIM_TU_PER_US
/**
 * Constant: JET_ANIM_HASH_STEP
 * Summary: The number jet_anim_cell_at mixes into each step when it picks random frames without a playhead.
 * Include: jet64.h
 * Notes: A playing animation in JET_ANIM_MODE_RANDOM draws its frames from jet_rand_int. jet_anim_cell_at keeps no playhead and so has nowhere to keep a random seed, so it mixes the salt you give it with the step count instead. The two ways give different sequences, on purpose.
 * Example:
 *   uint32_t cell = jet_anim_cell_at(def, seconds, 16, tile_hash);
 */
#define JET_ANIM_HASH_STEP 251u
/**
 * Constant: JET_ANIM_MIN_SPEED
 * Summary: The slowest speed jet_anim_set_speed will accept, as a multiplier of the authored speed.
 * Include: jet64.h
 * Notes: A multiplier of zero would stop the clock and a negative one would run it backwards, so anything that low is raised to this value instead. To hold an animation still, use jet_anim_pause.
 * Example:
 *   jet_anim_set_speed(&state, 0.0f); // becomes JET_ANIM_MIN_SPEED
 */
#define JET_ANIM_MIN_SPEED 0.001f
/**
 * Constant: JET_ANIM_FRAME_COUNT
 * Summary: How many JetAnimFrame records the machine's frame table, the ANIM FRAMES region of memory, can hold.
 * Include: jet64.h
 * Notes: A TABLE animation whose frame_offset + frame_count reaches past this has no frames at all, so it draws nothing.
 * Example:
 *   bool fits = def.frame_offset + def.frame_count <= JET_ANIM_FRAME_COUNT;
 */
#define JET_ANIM_FRAME_COUNT 32768u

/**
 * Constant: JET_BASE_TILE_SIZE
 * Summary: The width and height in pixels of one hardware tile, the 8x8 square the sprite sheet is counted in.
 * Include: jet64.h
 * Notes: A Sprite Editor cell is always a whole number of tiles, so a cell that is grid pixels wide is grid / JET_BASE_TILE_SIZE tiles wide. It is also the cell size an animation falls back to when neither your cart nor the definition says what size its cells are.
 * Example:
 *   uint32_t tiles_across = cell_pixels / JET_BASE_TILE_SIZE;
 */
#define JET_BASE_TILE_SIZE 8u
/**
 * Constant: JET_TILES_PER_ROW
 * Summary: How many 8x8 hardware tiles fit across one row of the sprite sheet: 128.
 * Include: jet64.h
 * Notes: It is JET_SPRITE_BANK_SIZE / JET_BASE_TILE_SIZE. Tiles are numbered left to right along a row and then on to the next row, so tile 128 is the first tile of the second row.
 * Example:
 *   uint32_t tile_x = tile_index % JET_TILES_PER_ROW;
 */
#define JET_TILES_PER_ROW 128u
/**
 * Constant: JET_SPRITE_BANK_SIZE
 * Summary: The width in pixels of one row of the sprite sheet, and so of one sprite bank: 1024.
 * Include: jet64.h
 * Notes: It is the same as JET_SPRITE_SHEET_WIDTH. With cells that are grid pixels wide, one row holds JET_SPRITE_BANK_SIZE / grid cells, and that is how far a JET_ANIM_DIR_COL animation jumps to reach the cell below.
 * Example:
 *   uint32_t cells_per_row = JET_SPRITE_BANK_SIZE / 16;
 */
#define JET_SPRITE_BANK_SIZE 1024u

/**
 * Struct: JetAnimDef
 * Summary: One animation as the Animation Editor saved it: which cells it shows, in what order, how fast, and any scale, rotation or tint effect it plays with. This is the 128-byte record the runtime reads.
 * Fields:
 *   - id: The number the editor gave this animation.
 *   - name: The name you typed in the editor, up to 15 characters, ending in a zero byte.
 *   - start_frame: The Sprite Editor cell number of the first frame.
 *   - end_frame: The Sprite Editor cell number of the last frame. That cell is included.
 *   - fps: How fast it plays, in animation frames a second.
 *   - mode: What happens when it reaches the end: JET_ANIM_MODE_FORWARD, JET_ANIM_MODE_REVERSE, JET_ANIM_MODE_PINGPONG, JET_ANIM_MODE_LOOP or JET_ANIM_MODE_RANDOM.
 *   - direction: JET_ANIM_DIR_ROW when the frames run across the Sprite Editor, JET_ANIM_DIR_COL when they run down it.
 *   - random_start: Anything other than 0 makes each play begin on a frame picked at random.
 *   - scale_curve: The easing curve number for the scale. An easing curve is the shape of a change over time: steady, speeding up, slowing down, and so on.
 *   - scale_start_x: How wide the frame is drawn at the start (progress 0), as a multiplier. 1.0f is the width you drew.
 *   - scale_start_y: How tall the frame is drawn at the start (progress 0), as a multiplier. 1.0f is the height you drew.
 *   - scale_end_x: How wide it is drawn at the end (progress 1), as a multiplier.
 *   - scale_end_y: How tall it is drawn at the end (progress 1), as a multiplier.
 *   - rot_start: How far it is turned at the start (progress 0), in degrees.
 *   - rot_end: How far it is turned at the end (progress 1), in degrees.
 *   - rot_curve: The easing curve number for the rotation.
 *   - tint_curve: The easing curve number for the tint's strength.
 *   - tint_color: The colour of the tint, as a palette index from 0 to 255. The tint colours the frame's whole shape, like a silhouette.
 *   - tint_start: How strong the tint is at the start, from 0 (none) to 255 (solid).
 *   - tint_end: How strong the tint is at the end, from 0 (none) to 255 (solid).
 *   - frame_source: JET_ANIM_SOURCE_RANGE to play the cells from start_frame to end_frame, or JET_ANIM_SOURCE_TABLE to play frames from the frame table instead.
 *   - flags: Kept for options that may come later. Must be zero.
 *   - _reserved0: Padding that keeps frame_offset on a 4-byte boundary. Must be zero.
 *   - frame_offset: TABLE only. The number of this animation's first JetAnimFrame record in the frame table.
 *   - frame_count: TABLE only. How many JetAnimFrame records the animation has.
 *   - loop_start: The step the repeating part begins at, where a step is one position along the animation counted from 0. 0 repeats the whole animation.
 *   - cell_px: The size of the square cell the animation was drawn for, in pixels. 0 means the file is from before this field existed, and then your cart supplies the size.
 *   - _reserved1: Spare bytes that keep the record at exactly 128 bytes. Do not change them.
 * Include: jet64.h
 * Notes: The definitions jet_anim_load_ani hands you point straight into the machine's animation memory, the ANIM bank. Never free them. Author animations in the editor when you can; jet_make_anim_def is there for the rare animation you want to build in code.
 * Example:
 *   JetAnimDef* idle = jet_anim_find_by_name(anims, anim_count, "idle");
 */
typedef struct {
    uint32_t id;
    char name[16];
    uint32_t start_frame;
    uint32_t end_frame;
    float fps;

    uint8_t mode;
    uint8_t direction; // 0 = Row, 1 = Column
    uint8_t random_start;
    uint8_t scale_curve;

    float scale_start_x;
    float scale_start_y;
    float scale_end_x;
    float scale_end_y;

    float rot_start;
    float rot_end;

    uint8_t rot_curve;
    uint8_t tint_curve;
    uint8_t tint_color;
    uint8_t tint_start;

    uint8_t tint_end;

    // v2 fields. Every one of these bytes was padding in v1, and a zeroed pad
    // reads back as "a RANGE animation over the whole sequence, cell size not
    // recorded" -- which is precisely v1's meaning. No migration, no flag day.
    uint8_t  frame_source;   // JET_ANIM_SOURCE_RANGE or JET_ANIM_SOURCE_TABLE.
    uint8_t  flags;          // Reserved for future per-definition options; must be zero.
    uint8_t  _reserved0;     // Keeps frame_offset 4-byte aligned; must be zero.
    uint32_t frame_offset;   // TABLE only: first JetAnimFrame index in the ANIM FRAMES region.
    uint32_t frame_count;    // TABLE only: number of frames in the table.
    uint32_t loop_start;     // Step the loop section begins at (0 = the whole sequence).
    uint16_t cell_px;        // Cell size in pixels the animation was authored at; 0 = not recorded.
    uint8_t  _reserved1[46]; // Pads the slot to exactly 128 bytes; must be zero.
} JetAnimDef;

// These checks run when the cartridge is compiled. The 128-byte slot must keep
// exactly this layout, byte for byte, because the workstation writes it with the
// same layout from jet64_core/src/anim.rs, which carries the matching checks.
_Static_assert(sizeof(JetAnimDef) == 128, "JetAnimDef size mismatch (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, name) == 4, "JetAnimDef name offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, start_frame) == 20, "JetAnimDef start_frame offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, mode) == 32, "JetAnimDef mode offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, scale_start_x) == 36, "JetAnimDef scale_start_x offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, rot_start) == 52, "JetAnimDef rot_start offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, tint_end) == 64, "JetAnimDef tint_end must stay the final v1 authored byte (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, frame_source) == 65, "JetAnimDef frame_source offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, flags) == 66, "JetAnimDef flags offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, _reserved0) == 67, "JetAnimDef _reserved0 offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, frame_offset) == 68, "JetAnimDef frame_offset offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, frame_count) == 72, "JetAnimDef frame_count offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, loop_start) == 76, "JetAnimDef loop_start offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, cell_px) == 80, "JetAnimDef cell_px offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimDef, _reserved1) == 82, "JetAnimDef _reserved1 offset (Rust mirror contract)");

/**
 * Struct: JetAnimFrame
 * Summary: One frame of a TABLE animation: which cell to show, how long to show it, how many cells it covers, and where to place it.
 * Fields:
 *   - cell: The Sprite Editor cell number to show, counted at the definition's cell size. When the frame covers more than one cell, this is the top-left one.
 *   - duration_us: How long the frame stays on screen, in microseconds. 0 means one tick of the game. The time is rounded to the nearest tick, 120 a second, and a value within one microsecond of a whole number of ticks counts as exactly that many (see JET_ANIM_TICK_SNAP_TU).
 *   - flags: JET_ANIM_FRAME_FLIP_X, JET_ANIM_FRAME_FLIP_Y, both (joined with |), or 0. They combine with the flips you pass to jet_anim_draw.
 *   - event: 0 for no event, or any other number your game chooses. The number is remembered when this frame comes on screen, until you read it with jet_anim_take_event.
 *   - offset_x: A nudge across, in pixels, added when this frame is drawn. It is measured in the drawn frame's own space, so it is mirrored when you draw the frame flipped left to right.
 *   - offset_y: A nudge down, in pixels, added when this frame is drawn. It is measured in the drawn frame's own space, so it is mirrored when you draw the frame flipped top to bottom.
 *   - span_w: How many cells across the frame covers, starting at cell. 0 and 1 both mean one cell.
 *   - span_h: How many cells down the frame covers, starting at cell. 0 and 1 both mean one cell.
 *   - _reserved: Must be zero. Kept for frame data that may come later.
 * Include: jet64.h
 * Notes: The frames of an animation authored in the editor live in the machine's frame table, the ANIM FRAMES region of memory, and a pointer to them points straight into that memory. Your cart may also supply its own array of frames through jet_anim_play_frames.
 *
 *        A frame that covers more than one cell is drawn span_w by span_h cells large. Use offset_x and offset_y to keep it lined up with the one-cell frames around it, so a wide swing stays lined up with the idle pose it belongs to.
 * Example:
 *   JetAnimFrame swing[2] = { { .cell = 8, .duration_us = 100000 }, { .cell = 9, .duration_us = 50000, .event = 1 } };
 */
typedef struct {
    uint32_t cell;
    uint32_t duration_us;
    uint8_t  flags;
    uint8_t  event;
    int8_t   offset_x;
    int8_t   offset_y;
    uint8_t  span_w;
    uint8_t  span_h;
    uint8_t  _reserved[2];
} JetAnimFrame;

// The frame record is the other half of the shared layout. The Rust mirror in
// jet64_core::anim carries the matching checks.
_Static_assert(sizeof(JetAnimFrame) == 16, "JetAnimFrame size (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, duration_us) == 4, "JetAnimFrame duration_us offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, flags) == 8, "JetAnimFrame flags offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, event) == 9, "JetAnimFrame event offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, offset_x) == 10, "JetAnimFrame offset_x offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, offset_y) == 11, "JetAnimFrame offset_y offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, span_w) == 12, "JetAnimFrame span_w offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, span_h) == 13, "JetAnimFrame span_h offset (Rust mirror contract)");
_Static_assert(offsetof(JetAnimFrame, _reserved) == 14, "JetAnimFrame _reserved offset (Rust mirror contract)");

/**
 * Function: jet_make_anim_def
 * Summary: Builds a RANGE animation definition in code, filling in every field of a JetAnimDef in one call.
 * Parameters:
 *   - id: A number of your choosing that names this animation. Give each animation its own.
 *   - name: The animation's name, up to 15 characters, ending in a zero byte. NULL leaves the name empty.
 *   - start_frame: The Sprite Editor cell number of the first frame.
 *   - end_frame: The Sprite Editor cell number of the last frame. That cell is included.
 *   - fps: How fast it plays, in animation frames a second.
 *   - mode: What happens at the end: JET_ANIM_MODE_FORWARD, JET_ANIM_MODE_REVERSE, JET_ANIM_MODE_PINGPONG, JET_ANIM_MODE_LOOP or JET_ANIM_MODE_RANDOM.
 *   - direction: JET_ANIM_DIR_ROW when the frames run across the Sprite Editor, JET_ANIM_DIR_COL when they run down it.
 *   - random_start: true to begin each play on a frame picked at random.
 *   - scale_curve: The easing curve number for the scale, an easing curve being the shape of a change over time. One curve serves both the width and the height.
 *   - scale_start_x: How wide the frame is drawn at the start, as a multiplier. 1.0f is the width you drew.
 *   - scale_start_y: How tall the frame is drawn at the start, as a multiplier. 1.0f is the height you drew.
 *   - scale_end_x: How wide it is drawn at the end, as a multiplier.
 *   - scale_end_y: How tall it is drawn at the end, as a multiplier.
 *   - rot_start: How far it is turned at the start, in degrees.
 *   - rot_end: How far it is turned at the end, in degrees.
 *   - rot_curve: The easing curve number for the rotation.
 *   - tint_curve: The easing curve number for the tint's strength.
 *   - tint_color: The colour of the tint laid over the frame, as a palette index from 0 to 255.
 *   - tint_start: How strong the tint is at the start, from 0 (none) to 255 (solid). The tint is dithered in, and this number is the dither's threshold.
 *   - tint_end: How strong the tint is at the end, from 0 (none) to 255 (solid), the same way.
 * Returns: A filled-in JetAnimDef, handed back by value.
 * Include: jet_anim.h
 * Notes: The name is copied in, and cut to 15 characters if it is longer. For an ordinary cart, author animations in the Animation Editor and fetch them from the ANIM bank with jet_anim_load_ani instead; this call is for the animation you want to build in code.
 * Example:
 *   JetAnimDef d = jet_make_anim_def(
 *       1, "jump", 0, 3, 10.0f,
 *       JET_ANIM_MODE_LOOP, JET_ANIM_DIR_ROW, false,
 *       0, 1.0f, 1.0f, 1.0f, 1.0f,
 *       0.0f, 0.0f, 0, 0, 0, 0, 0
 *   );
 */
static inline JetAnimDef jet_make_anim_def(
    uint32_t id, const char* name, uint32_t start_frame, uint32_t end_frame, float fps,
    uint8_t mode, uint8_t direction, uint8_t random_start, uint8_t scale_curve,
    float scale_start_x, float scale_start_y, float scale_end_x, float scale_end_y,
    float rot_start, float rot_end, uint8_t rot_curve, uint8_t tint_curve,
    uint8_t tint_color, uint8_t tint_start, uint8_t tint_end
) {
    JetAnimDef def;
    memset(&def, 0, sizeof(JetAnimDef));
    def.id = id;
    if (name) {
        strncpy(def.name, name, 15);
        def.name[15] = '\0';
    }
    def.start_frame = start_frame;
    def.end_frame = end_frame;
    def.fps = fps;
    def.mode = mode;
    def.direction = direction;
    def.random_start = random_start;
    def.scale_curve = scale_curve;
    def.scale_start_x = scale_start_x;
    def.scale_start_y = scale_start_y;
    def.scale_end_x = scale_end_x;
    def.scale_end_y = scale_end_y;
    def.rot_start = rot_start;
    def.rot_end = rot_end;
    def.rot_curve = rot_curve;
    def.tint_curve = tint_curve;
    def.tint_color = tint_color;
    def.tint_start = tint_start;
    def.tint_end = tint_end;
    return def;
}

/**
 * Function: jet_make_anim_table_def
 * Summary: Builds a TABLE animation definition in code, with no scale, rotation or tint effect, ready to play with jet_anim_play_frames.
 * Parameters:
 *   - id: A number of your choosing that names this animation. Give each animation its own.
 *   - name: The animation's name, up to 15 characters, ending in a zero byte. NULL leaves the name empty.
 *   - mode: What happens at the end: JET_ANIM_MODE_FORWARD, JET_ANIM_MODE_REVERSE, JET_ANIM_MODE_PINGPONG, JET_ANIM_MODE_LOOP or JET_ANIM_MODE_RANDOM.
 *   - loop_start: The step the repeating part begins at, where a step is one position along the animation counted from 0. 0 repeats the whole animation.
 *   - cell_px: The size of the square cell the frames are drawn at, in pixels. 0 leaves it to your cart to say the size when it plays the animation.
 * Returns: A filled-in JetAnimDef, handed back by value.
 * Include: jet_anim.h
 * Notes: The frames themselves are not part of the definition. Pass them to jet_anim_play_frames, which keeps only the address of your array, so the array must stay alive while the animation plays. Scale, rotation and tint are left at their neutral values; set those fields yourself if the animation wants them.
 * Example:
 *   static const JetAnimFrame swing[] = {
 *       { .cell = 8, .duration_us = 100000 },
 *       { .cell = 9, .duration_us =  50000, .event = 1 },
 *   };
 *   static JetAnimDef swing_def;
 *   swing_def = jet_make_anim_table_def(1, "swing", JET_ANIM_MODE_FORWARD, 0, 16);
 *   jet_anim_play_frames(&hero.anim, &swing_def, swing, 2, 0);
 */
static inline JetAnimDef jet_make_anim_table_def(
    uint32_t id, const char* name, uint8_t mode, uint32_t loop_start, uint16_t cell_px
) {
    JetAnimDef def;
    memset(&def, 0, sizeof(JetAnimDef));
    def.id = id;
    if (name) {
        strncpy(def.name, name, 15);
        def.name[15] = '\0';
    }
    def.fps = 0.0f;
    def.mode = mode;
    def.scale_start_x = 1.0f;
    def.scale_start_y = 1.0f;
    def.scale_end_x = 1.0f;
    def.scale_end_y = 1.0f;
    def.frame_source = JET_ANIM_SOURCE_TABLE;
    def.loop_start = loop_start;
    def.cell_px = cell_px;
    return def;
}

/**
 * Struct: JetAnimState
 * Summary: The playhead for one playing animation: which definition it is playing, which frame it is on, and how far into that frame it is. Keep one state for each thing on screen that animates on its own.
 * Fields:
 *   - def: The definition being played. The state only points at it; it never frees it.
 *   - timer: RANGE only. The seconds built up so far towards the next frame.
 *   - current_step: Which step the playhead is on, where a step is one position along the animation, counted from 0.
 *   - grid_size: The cell size in pixels, settled by jet_anim_play. Never 0 once the animation has been played.
 *   - speed_multiplier: How fast it plays compared with the authored speed. 1.0f is the definition's own timing.
 *   - reversed: Which way it is travelling: true while the steps are counting down. Ping-pong turns this round at each end.
 *   - playing: true while the animation should move on and be drawn.
 *   - progress: How far through the animation it is, from 0.0f at the start to 1.0f at the end. The authored scale, rotation and tint effects read this too.
 *   - frames: TABLE only. The array of frames in use, in the machine's frame table or in your cart. NULL for a RANGE animation.
 *   - frame_count: How many steps the animation has, settled when it was played.
 *   - loop_start: The step the repeating part begins at, already checked against frame_count.
 *   - timer_tu: TABLE only. How far into the current frame the playhead is, in timeline units, the whole-number clock the runtime counts animation time in. It is signed, because rounding a frame's time to the nearest tick leaves a remainder that may be positive or negative.
 *   - sequence_tu: TABLE only. How long one pass through the whole animation takes as authored, in timeline units.
 *   - frame_start_tu: TABLE only. The total time of every step before the current one, in timeline units. This is what turns the clock into progress.
 *   - cycles: How many times the repeating part has been completed. For JET_ANIM_MODE_RANDOM, how many steps have been taken.
 *   - event: The event number remembered from the frame most recently entered, or 0 when there is none. Read it with jet_anim_take_event.
 *   - finished: true once a one-shot animation, one in FORWARD or REVERSE mode, has reached its end, and it stays true until something moves the playhead.
 *   - just_finished: true only during the jet_anim_update call in which a one-shot animation ended.
 * Include: jet64.h
 * Notes: Make one with jet_make_anim_state, or hand any existing state to jet_anim_play. Move it along with jet_anim_update in _update, and draw it in _draw. The definition must stay alive for as long as this state points at it. The length of the animation is worked out when you play it, so if a definition's length is edited while it runs, the change shows on the next play.
 * Example:
 *   JetAnimState player_anim = jet_make_anim_state(idle, 16);
 */
typedef struct {
    JetAnimDef* def;
    float timer;
    uint32_t current_step;
    uint32_t grid_size; // Sprite-grid cell size in pixels, resolved on play.
    float speed_multiplier; // Playback rate multiplier (1.0 = the def's own timing).
    bool reversed;
    bool playing;
    float progress; // Playback progress through the sequence, 0.0 to 1.0.

    // v2 fields. Every one is initialised by jet_make_anim_state and by every
    // form of jet_anim_play, so a cursor is never partly resolved.
    const JetAnimFrame* frames; // TABLE: the frame array in use; RANGE: NULL.
    uint32_t frame_count;       // Sequence length n, resolved at play.
    uint32_t loop_start;        // Effective loop start, already validated against n.
    int64_t  timer_tu;          // TABLE: timeline units inside the current frame.
    uint64_t sequence_tu;       // TABLE: the whole sequence's hold, for progress.
    uint64_t frame_start_tu;    // TABLE: hold accumulated before the current step.
    uint32_t cycles;            // Completed loops (or steps taken, for RANDOM).
    uint8_t  event;             // Latched event id of the current frame; 0 = none.
    bool     finished;          // A one-shot ended and nothing has moved since.
    bool     just_finished;     // True only in the update that ended it.
} JetAnimState;

/**
 * Function: jet_make_anim_state
 * Summary: Makes a fresh JetAnimState, the playhead for one animation, pointed at a definition and not yet playing.
 * Parameters:
 *   - def: The JetAnimDef this state will play.
 *   - grid_size: The size of one sprite cell in pixels, such as 8, 16 or 32, or 0 to use the size the animation was drawn for.
 * Returns: A filled-in JetAnimState, handed back by value.
 * Include: jet_anim.h
 * Notes: The state only points at def, so keep that definition alive for as long as the animation plays. The state starts paused, and it is jet_anim_play that works out the animation's length, so call jet_anim_play before you update or draw it.
 * Example:
 *   JetAnimState st = jet_make_anim_state(my_anim, 16);
 */
static inline JetAnimState jet_make_anim_state(JetAnimDef* def, uint32_t grid_size) {
    JetAnimState state;
    memset(&state, 0, sizeof(JetAnimState));
    state.def = def;
    state.grid_size = grid_size;
    state.speed_multiplier = 1.0f;
    return state;
}

/**
 * Function: jet_anim_load_ani
 * Summary: Gives you every animation the Animation Editor saved, as a pointer to the machine's animation memory, the ANIM bank. The bank is already filled from your project, so nothing is read from disk.
 * Parameters:
 *   - filepath: Ignored. The animations come from the ANIM bank in memory, not from a file.
 *   - out_count: Where to put the number of slots that hold an animation, or NULL if you do not want the count.
 * Returns: A pointer to the first of the 1024 JetAnimDef slots in the ANIM bank.
 * Include: jet_anim.h
 * Notes: The pointer points straight into the machine's memory, so never free it. The filepath argument is there only so that code written for a loader that reads a file still compiles.
 * Example:
 *   uint32_t anim_count = 0;
 *   JetAnimDef* anims = jet_anim_load_ani("anims.ani", &anim_count);
 */
JetAnimDef* jet_anim_load_ani(const char* filepath, uint32_t* out_count);

/**
 * Function: jet_anim_find_by_id
 * Summary: Finds an animation by the number the Animation Editor gave it, searching the machine's animation memory, the ANIM bank.
 * Parameters:
 *   - anims: Ignored. The search always runs over the ANIM bank.
 *   - count: Ignored.
 *   - id: The animation number to look for.
 * Returns: A pointer to the matching JetAnimDef, or NULL when no animation has that number.
 * Include: jet_anim.h
 * Notes: The pointer points straight into the machine's memory, so never free it.
 * Example:
 *   JetAnimDef* run = jet_anim_find_by_id(anims, anim_count, 2);
 */
JetAnimDef* jet_anim_find_by_id(JetAnimDef* anims, uint32_t count, uint32_t id);

/**
 * Function: jet_anim_find_by_name
 * Summary: Finds an animation by the name you gave it in the Animation Editor.
 * Parameters:
 *   - anims: Ignored. The search always runs over the ANIM bank.
 *   - count: Ignored.
 *   - name: The name to look for, ending in a zero byte.
 * Returns: A pointer to the matching JetAnimDef, or NULL when no animation has that name.
 * Include: jet_anim.h
 * Notes: Up to 16 bytes of the name are compared with each slot's name field. The pointer points straight into the machine's memory, so never free it.
 * Example:
 *   JetAnimDef* idle = jet_anim_find_by_name(anims, anim_count, "idle");
 */
JetAnimDef* jet_anim_find_by_name(JetAnimDef* anims, uint32_t count, const char* name);

/**
 * Function: jet_anim_play
 * Summary: Starts an animation playing from the beginning, in the state you give it.
 * Parameters:
 *   - state: The JetAnimState to play it in. Whatever it was doing before is replaced.
 *   - def: The animation to play, usually found with jet_anim_find_by_id or jet_anim_find_by_name.
 *   - grid_size_pixels: The width and height of one sprite cell in pixels, or 0 to use the size the animation was drawn for.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: This works out the animation's length and puts everything back to the start: the clock, the step, the direction of travel, the cycle count, the finished flags, and the speed multiplier. Because the speed goes back to 1.0f, call jet_anim_set_speed after jet_anim_play, not before.
 *
 *        The starting frame depends on the mode. An animation in JET_ANIM_MODE_REVERSE starts on its last step, because that mode plays from the end back to the start. Then call jet_anim_update every tick in _update, and jet_anim_draw or jet_anim_draw_grid in _draw.
 * Example:
 *   jet_anim_play(&player_anim, idle, 16);
 */
void jet_anim_play(JetAnimState* state, JetAnimDef* def, uint32_t grid_size_pixels);

/**
 * Function: jet_anim_update
 * Summary: Moves an animation forward by dt seconds, changing frame when it is time to.
 * Parameters:
 *   - state: The animation, started with jet_anim_play.
 *   - dt: How much time has passed since the last update, in seconds. In _update, pass the dt you were given.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: Call it once every tick, from _update. It follows the definition's mode at the ends (forward, reverse, ping-pong, loop or random) and keeps the progress value, 0.0f to 1.0f, that the authored scale, rotation and tint effects read.
 *
 *        A RANGE animation keeps time by adding up seconds against its fps. A TABLE animation keeps time in whole numbers on an exact timeline, so a frame authored as a whole number of ticks changes on that tick every time, with no drift building up; a frame whose time is not a whole number of ticks changes on the nearest tick, and the little bit left over is carried into the next frame rather than thrown away.
 * Example:
 *   jet_anim_update(&player_anim, dt);
 */
void jet_anim_update(JetAnimState* state, float dt);


/**
 * Function: jet_anim_cell_to_tile
 * Summary: Turns a Sprite Editor cell number into the number of the 8x8 hardware tile at the same place on the sprite sheet.
 * Parameters:
 *   - cell: The Sprite Editor cell number, counted from 0, at the cell size you give.
 *   - grid_size_pixels: The size of one square cell in pixels. 0 counts as JET_BASE_TILE_SIZE, that is 8.
 * Returns: The tile number to give jet_spr, jet_spr_ext or jet_collide.
 * Include: jet64.h
 * Notes: This is the one place where the editor's cells meet the hardware's tiles, and jet_anim_current_frame does it for you. Unlike jet_get_hw_idx, which reads the cell size you set with jet_set_sprite_grid, this takes the cell size as an argument, so an animation can find its cells at the size it was drawn for even when your cart's grid is different.
 * Example:
 *   uint32_t tile = jet_anim_cell_to_tile(5, 16);
 */
static inline uint32_t jet_anim_cell_to_tile(uint32_t cell, uint32_t grid_size_pixels) {
    const uint32_t grid = grid_size_pixels ? grid_size_pixels : JET_BASE_TILE_SIZE;
    uint32_t cells_per_row = JET_SPRITE_BANK_SIZE / grid;
    if (cells_per_row == 0) cells_per_row = 1; // Cell wider than the atlas: one per row.
    const uint32_t tiles_per_cell = grid / JET_BASE_TILE_SIZE;
    return ((cell / cells_per_row) * tiles_per_cell * JET_TILES_PER_ROW)
         + ((cell % cells_per_row) * tiles_per_cell);
}

/**
 * Struct: JetAnimFrameView
 * Summary: Everything you need to draw, or test for a hit on, the frame an animation is showing right now, with the flips you mean to draw it with already worked in.
 * Fields:
 *   - cell: The Sprite Editor cell number. When the frame covers more than one cell, this is the top-left one.
 *   - tile: The same cell as an 8x8 hardware tile number, which is what jet_spr, jet_spr_ext and jet_collide take.
 *   - tiles_w: How wide the frame is drawn, in hardware tiles: its width in cells, times the tiles in a cell.
 *   - tiles_h: How tall the frame is drawn, in hardware tiles.
 *   - span_w: How many cells across the frame covers. Never 0 in a view.
 *   - span_h: How many cells down the frame covers. Never 0 in a view.
 *   - dx: The pixels to add to the anchor cell's x, already mirrored about the anchor cell for the flip_x you asked for.
 *   - dy: The pixels to add to the anchor cell's y, already mirrored the same way for flip_y.
 *   - flip_x: Whether to draw it mirrored left to right: the frame's own flag combined with the flip_x you asked for. Pass it straight on.
 *   - flip_y: Whether to draw it mirrored top to bottom, worked out the same way.
 *   - event: The event number the frame was authored with, or 0 for none. It is only for looking at; jet_anim_take_event is the call that reads and clears the event.
 * Include: jet64.h
 * Notes: Fill one in with jet_anim_current_frame. The offsets and spans are measured from the anchor cell, the single cell the animation is positioned by. That is what keeps a one-cell idle pose and a two-cell-wide swing of the same character lined up with each other, whichever way they face.
 * Example:
 *   JetAnimFrameView view;
 *   if (jet_anim_current_frame(&hero.anim, facing_left, false, &view)) {
 *       jet_spr(view.tile, hx + view.dx, hy + view.dy, view.tiles_w, view.tiles_h, view.flip_x, view.flip_y);
 *   }
 */
typedef struct {
    uint32_t cell;
    uint32_t tile;
    uint8_t  tiles_w;
    uint8_t  tiles_h;
    uint8_t  span_w;
    uint8_t  span_h;
    int16_t  dx;
    int16_t  dy;
    bool     flip_x;
    bool     flip_y;
    uint8_t  event;
} JetAnimFrameView;

/**
 * Function: jet_anim_play_frames
 * Summary: Starts an animation whose frames are an array of JetAnimFrame records in your own cart, rather than in the machine's frame table.
 * Parameters:
 *   - state: The JetAnimState to play it in. Whatever it was doing before is replaced.
 *   - def: A TABLE definition, usually made with jet_make_anim_table_def. It supplies the mode, the loop start and the effects.
 *   - frames: The array of frames to play. Only its address is kept, so the array must stay alive for as long as the state uses it.
 *   - count: How many frames are in the array.
 *   - grid_size_pixels: The size of one cell in pixels, or 0 to use the size recorded in the definition.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: The same as jet_anim_play in every way except where the frames come from.
 *
 *        Because only the array's address is kept, a state that lives through a Warm-Boot must be played again against the rebuilt cart's array. A Warm-Boot re-links your cart's static data but leaves WRAM, the cartridge's working memory, as it was, so a state kept in WRAM would still point at the old array.
 * Example:
 *   jet_anim_play_frames(&hero.anim, &swing_def, swing, 2, 16);
 */
void jet_anim_play_frames(JetAnimState* state, JetAnimDef* def, const JetAnimFrame* frames, uint32_t count, uint32_t grid_size_pixels);

/**
 * Function: jet_anim_switch
 * Summary: Plays an animation only if the state is not already playing it, so you can call it every tick with "walk while moving, else idle" and the walk does not restart on every tick.
 * Parameters:
 *   - state: The JetAnimState to check and, if need be, play in.
 *   - def: The animation the state should be showing.
 *   - grid_size_pixels: The size of one cell in pixels, or 0 to use the size recorded in the definition.
 * Returns: true when the animation was started afresh; false when the state was already playing that definition.
 * Include: jet_anim.h
 * Notes: When the state is already on that definition, this does exactly what jet_anim_resume does. So a one-shot animation that has finished, switched to itself, stays finished and quiet instead of playing again.
 * Example:
 *   jet_anim_switch(&player.anim, moving ? def_run : def_idle, 0);
 */
bool jet_anim_switch(JetAnimState* state, JetAnimDef* def, uint32_t grid_size_pixels);

/**
 * Function: jet_anim_pause
 * Summary: Holds an animation still, exactly where it is.
 * Parameters:
 *   - state: The animation to hold.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: You can still draw the frame it is on; only jet_anim_update stops moving it. Let it go again with jet_anim_resume.
 * Example:
 *   if (game_paused) jet_anim_pause(&player.anim);
 */
void jet_anim_pause(JetAnimState* state);

/**
 * Function: jet_anim_resume
 * Summary: Lets a paused animation move again, from where it stopped.
 * Parameters:
 *   - state: The animation to let go.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: A one-shot animation that has finished stays finished and still, because letting it go would only finish it again on the next update. To move one of those, use jet_anim_play, jet_anim_seek or jet_anim_set_reversed instead.
 * Example:
 *   jet_anim_resume(&player.anim);
 */
void jet_anim_resume(JetAnimState* state);

/**
 * Function: jet_anim_set_reversed
 * Summary: Turns an animation round, so it travels the other way from the frame it is on.
 * Parameters:
 *   - state: The animation to turn round.
 *   - reversed: true to travel back towards the first frame, false to travel forward towards the last.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: The time already spent in the current frame is mirrored, so the turn happens exactly where the playhead is, not at the next change of frame. It also clears the finished flag and sets the animation playing, which is what lets a one-shot that has finished run backwards: a hole that was dug healing shut, say. If the animation is already travelling that way, nothing changes.
 * Example:
 *   jet_anim_set_reversed(&hole->anim, true); // heal the hole shut
 */
void jet_anim_set_reversed(JetAnimState* state, bool reversed);

/**
 * Function: jet_anim_seek
 * Summary: Jumps an animation to a given step.
 * Parameters:
 *   - state: The animation to move.
 *   - step: Which step to jump to, where a step is one position along the animation, counted from 0. A number past the end lands on the last step.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: The time spent in the current frame is thrown away, the event of the frame you land on is remembered for jet_anim_take_event, and the finished flag is cleared. Whether the animation is playing or paused is left exactly as it was.
 * Example:
 *   jet_anim_seek(&player.anim, 0);
 */
void jet_anim_seek(JetAnimState* state, uint32_t step);

/**
 * Function: jet_anim_seek_seconds
 * Summary: Puts an animation where it would be if it had been playing for a given number of seconds.
 * Parameters:
 *   - state: The animation to move.
 *   - seconds: The time since the start of the animation, in seconds. A negative value counts as 0.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: Looping and ping-pong are taken into account, so the seek can land on the way back as well as on the way out, and the direction of travel is set to match. The time inside the frame is set too, so seeking to T/120 seconds leaves the state exactly as T calls of jet_anim_update at the machine's fixed tick would have. Whether the animation is playing or paused is left as it was.
 * Example:
 *   jet_anim_seek_seconds(&torch.anim, phase_seconds);
 */
void jet_anim_seek_seconds(JetAnimState* state, float seconds);

/**
 * Function: jet_anim_stop
 * Summary: Stops an animation and takes it back to its first step.
 * Parameters:
 *   - state: The animation to stop.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: Stopping is not the same as finishing. jet_anim_is_finished stays false, because a stop is something your game did, not something the animation reached on its own.
 * Example:
 *   jet_anim_stop(&player.anim);
 */
void jet_anim_stop(JetAnimState* state);

/**
 * Function: jet_anim_set_speed
 * Summary: Makes an animation play faster or slower than it was authored.
 * Parameters:
 *   - state: The animation to change.
 *   - multiplier: How fast, as a multiple of the authored speed. 1.0f is the authored speed, 2.0f is twice as fast, 0.5f is half. A value of zero or below is raised to JET_ANIM_MIN_SPEED.
 * Returns: Nothing.
 * Include: jet_anim.h
 * Notes: jet_anim_play puts the multiplier back to 1.0f, so set the speed after playing, not before. To hold an animation completely still, use jet_anim_pause.
 * Example:
 *   jet_anim_play(&ninja->anim, def, 16);
 *   jet_anim_set_speed(&ninja->anim, 0.8f + jet_randf() * 0.4f);
 */
void jet_anim_set_speed(JetAnimState* state, float multiplier);

/**
 * Function: jet_anim_is_finished
 * Summary: Tells you whether a one-shot animation, one in FORWARD or REVERSE mode, has reached its end and is still there.
 * Parameters:
 *   - state: The animation to ask about.
 * Returns: true while the animation is finished; false for a looping animation, and false for a finished one that has since been moved.
 * Include: jet_anim.h
 * Notes: Unlike jet_anim_just_finished, the answer stays true until something moves the playhead: playing, seeking or turning it round all clear it. Use it to test a condition that lasts, such as "the explosion is over, so remove it".
 * Example:
 *   if (jet_anim_is_finished(&explosion.anim)) explosion.alive = false;
 */
bool jet_anim_is_finished(const JetAnimState* state);

/**
 * Function: jet_anim_just_finished
 * Summary: Tells you whether a one-shot animation, one in FORWARD or REVERSE mode, ended during the most recent jet_anim_update.
 * Parameters:
 *   - state: The animation to ask about.
 * Returns: true only in the one update in which the animation ended; false at every other time.
 * Include: jet_anim.h
 * Notes: This is the call for something that should happen once, at the moment the animation ends: spawn the debris, deal the damage. For a condition that lasts, such as "is it over yet", use jet_anim_is_finished.
 * Example:
 *   if (jet_anim_just_finished(&swing.anim)) apply_damage();
 */
bool jet_anim_just_finished(const JetAnimState* state);

/**
 * Function: jet_anim_cycles
 * Summary: Tells you how many times an animation has been round its repeating part since it was played.
 * Parameters:
 *   - state: The animation to ask about.
 * Returns: The number of complete loops since jet_anim_play. For ping-pong, a loop is one trip there and back; for random, the count is the number of steps taken.
 * Include: jet_anim.h
 * Notes: jet_anim_play sets it back to 0. It is handy for "flash three times, then stop" without keeping a timer of your own.
 * Example:
 *   if (jet_anim_cycles(&pickup.anim) >= 3) pickup.alive = false;
 */
uint32_t jet_anim_cycles(const JetAnimState* state);

/**
 * Function: jet_anim_take_event
 * Summary: Reads the event number left by the frame the animation most recently entered, and clears it so you see each event once.
 * Parameters:
 *   - state: The animation to ask.
 * Returns: The event number the frame was authored with, or 0 when no event is waiting.
 * Include: jet_anim.h
 * Notes: A state holds one event at a time, so read it once every update. At the machine's fixed tick an update passes through at most one frame, unless it is catching up after an unusually large dt, so in ordinary play no event is lost. Only TABLE animations carry events; a RANGE animation never has one.
 * Example:
 *   uint8_t e = jet_anim_take_event(&hero.anim);
 *   if (e == EVENT_FOOTSTEP) jet_sfx_play(step_sound); // jet_audio.h
 */
uint8_t jet_anim_take_event(JetAnimState* state);

/**
 * Function: jet_anim_step_count
 * Summary: Tells you how many steps the animation a state is playing has, where a step is one position along the animation.
 * Parameters:
 *   - state: The animation to ask about.
 * Returns: The number of steps, worked out when the animation was played, or 0 when there is nothing to play.
 * Include: jet_anim.h
 * Notes: Because it is worked out at play, it does not follow a definition edited while the animation runs until the animation is played again.
 * Example:
 *   uint32_t n = jet_anim_step_count(&hero.anim);
 */
uint32_t jet_anim_step_count(const JetAnimState* state);

/**
 * Function: jet_anim_sequence_seconds
 * Summary: Tells you how long one pass through the animation a state is playing takes, at the speed it was authored.
 * Parameters:
 *   - state: The animation to ask about.
 * Returns: The length of one pass in seconds, ignoring the speed multiplier, or 0 when there is nothing to play.
 * Include: jet_anim.h
 * Notes: For a TABLE animation this is all the frame durations added up; for a RANGE animation it is the number of steps divided by fps. If you want the real time the pass takes at the speed it is actually playing, divide this by the speed multiplier.
 * Example:
 *   float period = jet_anim_sequence_seconds(&torch.anim);
 */
float jet_anim_sequence_seconds(const JetAnimState* state);

/**
 * Function: jet_anim_current_frame
 * Summary: Works out the frame an animation is showing right now, with the flips you mean to draw it with already worked in, and fills a JetAnimFrameView with it.
 * Parameters:
 *   - state: The animation to ask about.
 *   - flip_x: true if you are going to draw the frame mirrored left to right.
 *   - flip_y: true if you are going to draw the frame mirrored top to bottom.
 *   - out: The JetAnimFrameView to fill in. It is left untouched when the call returns false.
 * Returns: true when there is a frame to draw; false when the state has no definition or the animation has no frames.
 * Include: jet_anim.h
 * Notes: The view's dx, dy, flip_x and flip_y are the finished values. Pass them straight to jet_spr, jet_spr_ext or jet_collide, with no sums of your own.
 *
 *        The offsets are mirrored about the ANCHOR CELL, the single cell the animation is positioned by. That is what keeps a one-cell idle pose and a two-cell-wide swing lined up when a character turns round. The mirroring uses the cell size the animation was played at, so draw the frame at that size.
 *
 *        The three animation drawing calls in jet_sprite.h call this with their own flips, so this is the one place the flips are combined.
 * Example:
 *   JetAnimFrameView v;
 *   if (jet_anim_current_frame(&hero.anim, facing_left, false, &v))
 *       hit = jet_collide(v.tile, hx + v.dx, hy + v.dy, v.tiles_w, v.tiles_h, v.flip_x, v.flip_y,
 *                         bullet_tile, bx, by, 1, 1, false, false);
 */
bool jet_anim_current_frame(const JetAnimState* state, bool flip_x, bool flip_y, JetAnimFrameView* out);

/**
 * Function: jet_anim_current_cell
 * Summary: Tells you which Sprite Editor cell an animation is showing right now.
 * Parameters:
 *   - state: The animation to ask about.
 * Returns: The cell number, or 0 when there is nothing to draw.
 * Include: jet_anim.h
 * Notes: This is the short answer, for when all you need is the cell. jet_anim_current_frame gives the full one, with the frame's span, offsets and flips as well.
 * Example:
 *   uint32_t cell = jet_anim_current_cell(&hero.anim);
 */
uint32_t jet_anim_current_cell(const JetAnimState* state);

/**
 * Function: jet_anim_def_step_count
 * Summary: Tells you how many steps an animation definition has, without playing it.
 * Parameters:
 *   - def: The definition to ask about. NULL gives 0.
 *   - grid_size_pixels: The size of one cell in pixels, or 0 to use the size recorded in the definition.
 * Returns: The number of steps, where a step is one position along the animation.
 * Include: jet_anim.h
 * Notes: For a RANGE animation the cell size decides how far apart the cells of a column are, so it can change the answer. For a TABLE animation the answer is just the frame count.
 * Example:
 *   uint32_t n = jet_anim_def_step_count(def, 16);
 */
uint32_t jet_anim_def_step_count(const JetAnimDef* def, uint32_t grid_size_pixels);

/**
 * Function: jet_anim_def_seconds
 * Summary: Tells you how long one pass through an animation definition takes, without playing it.
 * Parameters:
 *   - def: The definition to ask about. NULL gives 0.
 *   - grid_size_pixels: The size of one cell in pixels, or 0 to use the size recorded in the definition.
 * Returns: The length of one pass in seconds, at the authored speed.
 * Include: jet_anim.h
 * Notes: Handy for planning something to happen when an effect will have finished, without having to play it first.
 * Example:
 *   float lifetime = jet_anim_def_seconds(def_explosion, 16);
 */
float jet_anim_def_seconds(const JetAnimDef* def, uint32_t grid_size_pixels);

/**
 * Function: jet_anim_def_is_static
 * Summary: Tells you whether an animation definition ever changes cell at all.
 * Parameters:
 *   - def: The definition to ask about. NULL counts as still.
 *   - grid_size_pixels: The size of one cell in pixels, or 0 to use the size recorded in the definition.
 * Returns: true when the animation has one step or none, or is a RANGE animation with no frame rate (an fps of 0 or less); otherwise false.
 * Include: jet_anim.h
 * Notes: This is the cheap question to ask before the costly one. The map engine asks it once for each animation a map uses, every draw, so that a tile whose animation never changes keeps the tile that was painted instead of working out a frame at all.
 * Example:
 *   if (!jet_anim_def_is_static(def, 16)) cell = jet_anim_cell_at(def, t, 16, 0);
 */
bool jet_anim_def_is_static(const JetAnimDef* def, uint32_t grid_size_pixels);

/**
 * Function: jet_anim_def_frames
 * Summary: Gives you a TABLE animation's frame records, where they sit in the machine's frame table.
 * Parameters:
 *   - def: The definition to ask about.
 * Returns: A pointer to its first JetAnimFrame, or NULL for a RANGE definition, or for a table that reaches past the end of the frame table.
 * Include: jet_anim.h
 * Notes: The pointer points straight into the machine's memory. Never free it, and expect what it points at to change if the editor sends the cartridge a new bank.
 * Example:
 *   const JetAnimFrame* frames = jet_anim_def_frames(def);
 */
const JetAnimFrame* jet_anim_def_frames(const JetAnimDef* def);

/**
 * Function: jet_anim_cell_at
 * Summary: Tells you which cell an animation would be showing after a given number of seconds, without keeping a JetAnimState for it.
 * Parameters:
 *   - def: The definition to ask about. NULL gives 0.
 *   - seconds: The time since the animation started, in seconds. A negative value counts as 0.
 *   - grid_size_pixels: The size of one cell in pixels, or 0 to use the size recorded in the definition.
 *   - salt: A number of your own for this one copy of the animation. It spreads copies with random_start apart in time and picks the frames of a random animation. 0 for no spreading.
 * Returns: The Sprite Editor cell number to draw.
 * Include: jet_anim.h
 * Notes: This is how the map engine animates its tiles: thousands of copies, and not one JetAnimState between them. At the normal speed, a multiplier of 1.0f, a TABLE animation asked this way shows exactly the cell a playing state would show on the same tick.
 *
 *        A RANGE animation keeps the map's old arithmetic: a forward animation goes round again and a reverse one goes round again backwards, because a tile has no state that could finish. Random frames are picked from the salt rather than from jet_rand_int, so a playing state and a tile roll different sequences, on purpose.
 * Example:
 *   uint32_t cell = jet_anim_cell_at(def, map.time, map.tile_size, 0);
 */
uint32_t jet_anim_cell_at(const JetAnimDef* def, float seconds, uint32_t grid_size_pixels, uint32_t salt);

#endif // JET_ANIM_H
