// runtime/jet_utils.h
#ifndef JET_UTILS_H
#define JET_UTILS_H

#include <stdbool.h>
#include "jet_text.h"
#include "jet_math.h" // JET_TAU for jet_rand_angle()

// Random numbers, and a way to see how fast your cart is running. The
// jet_rand family is a small random number generator for gameplay. It is
// deterministic, meaning the same every time you run it. Seeding it means
// choosing where in its long fixed sequence of numbers it starts. Seed it
// with jet_srand and a fixed number and every run deals the same cards, which
// is what a test or a demo wants; or call jet_randomise once in _init to start
// from a different place each launch, so runs differ. These numbers are not
// secure: never use them for a password, a token, or anything to do with
// security. jet_get_stats and jet_draw_fps tell you how much time your
// frames have to spare.
//
// Complete deterministic-stars example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { STAR_COUNT = 64 };
//   static Vector2 stars[STAR_COUNT];
//
//   void _init(void) {
//       jet_srand(0x4A455436u); // the same star field on every run, on every machine
//       for (int i = 0; i < STAR_COUNT; i++) {
//           stars[i] = jet_make_vector2(
//               (float)jet_rand_int(0, JET_WIDTH - 1),
//               (float)jet_rand_int(0, JET_HEIGHT - 1)
//           );
//       }
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       jet_cls(0);
//       for (int i = 0; i < STAR_COUNT; i++) jet_pset((int)stars[i].x, (int)stars[i].y, 15);
//       jet_draw_fps(NULL);
//   }
//
//   void _end(void) {}

// The classic 4x4 Bayer dither pattern: sixteen thresholds from 0 to 240 in
// the usual recursive order. The text and sprite calls use it to fake partial
// transparency with a pattern of dots. It is declared here and defined once in
// jet_utils.c, so every source file shares the same 16 bytes instead of
// carrying its own copy.
extern const uint8_t JET_BAYER_MATRIX_4X4[4][4];

/**
 * Struct: JetSystemStats
 * Summary: A reading of how fast the host is running your cart, as jet_get_stats gives it to you.
 * Fields:
 *   - current_fps: How many frames a second the host is actually showing. The frame limiter and vsync hold this down.
 *   - uncapped_fps: How many frames a second the cart could run at if every limit were taken away. It is worked out from how long each frame really takes to make: your hooks, plus the host's own cost to put the frame on screen. It is never below current_fps, and equals it when there is no limit left to take away because the machine is already flat out. This is the number that tells you how much headroom you have, or how far over you are.
 *   - vsync_enabled: true while vsync is holding frames in step with the monitor's refresh.
 * Include: jet_utils.h
 * Notes: The struct holds plain numbers and no pointers, so you can copy it freely and there is nothing to clean up. jet_get_stats fills one in with live values.
 * Example:
 *   JetSystemStats stats = jet_get_stats();
 *   if (stats.uncapped_fps < 120) { show_performance_warning(); }
 */
typedef struct {
    uint32_t current_fps;
    uint32_t uncapped_fps;
    bool vsync_enabled;
} JetSystemStats;

/**
 * Function: jet_make_system_stats
 * Summary: Makes a JetSystemStats from three values you give it.
 * Parameters:
 *   - current_fps: The frames per second being shown.
 *   - uncapped_fps: The frames per second the cart could reach with no limits.
 *   - vsync_enabled: true if vsync is holding frames to the monitor's refresh.
 * Returns: The filled-in JetSystemStats, by value.
 * Include: jet_utils.h
 * Example:
 *   JetSystemStats s = jet_make_system_stats(60, 240, true);
 */
static inline JetSystemStats jet_make_system_stats(uint32_t current_fps, uint32_t uncapped_fps, bool vsync_enabled) {
    JetSystemStats s;
    s.current_fps = current_fps;
    s.uncapped_fps = uncapped_fps;
    s.vsync_enabled = vsync_enabled;
    return s;
}

/**
 * Function: jet_get_stats
 * Summary: Reads how fast the host is running your cart right now.
 * Parameters:
 *   - None.
 * Returns: A JetSystemStats holding the frames per second being shown, the frames per second the cart could reach with no limits, and whether vsync is on.
 * Include: jet_utils.h
 * Notes: The host writes these numbers into the SYS registers, a block of memory both sides can see, so reading them is cheap and safe to do every frame, from gameplay or from a debug overlay. What you get back is a copy; the host's later updates do not change a copy you already hold.
 * Example:
 *   JetSystemStats stats = jet_get_stats();
 *   if (stats.uncapped_fps < 120) {
 *       jet_print("perf warning", 8, 8, 7);
 *   }
 */
JetSystemStats jet_get_stats(void);

/**
 * Function: jet_draw_fps
 * Summary: Prints one number on the screen: the frames per second your cart could reach with no limits.
 * Parameters:
 *   - fmt: How to draw the text, as a TextFormat: its colour, size, spacing, alpha and shadow. Pass NULL for the small default debugging style.
 * Returns: Nothing.
 * Include: jet_utils.h
 * Notes: The number is uncapped_fps from jet_get_stats, so it shows how much room your cart has to spare, not how fast the monitor is. Call it from _draw after jet_cls. Call it last to draw it on top of your scene, or earlier to draw the scene over it.
 * Example:
 *   TextFormat fps_fmt = { .color = 7, .size = 14, .spacing_x = 1, .spacing_y = 1,
 *                          .alpha = 1.0f, .shadow_offset = 1, .shadow_color = 0 };
 *   jet_draw_fps(&fps_fmt);
 */
void jet_draw_fps(const TextFormat* fmt);

/**
 * Function: _jet_clock_micros
 * Summary: Internal monotonic microsecond clock, the runtime's one platform stopwatch source.
 * Parameters:
 *   - None.
 * Returns: Microseconds from an arbitrary, host-chosen epoch (boot, page load), only ever counting up.
 * Include: jet_utils.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Cartridge code must never call this directly — jet_time_micros in the opt-in jet_time.h is the cart-facing stopwatch, and it re-bases this reading against its own first one so a cart always starts at zero. This verb is firmware because two callers need it and only one of them may reach an opt-in module: jet_time.c is the other, and the workstation guest's ping-pong loop reads it three times a tick to publish SYS_UPDATE_MICROS and SYS_DRAW_MICROS.
 * Example:
 *   // Cartridge code uses the re-based stopwatch instead.
 *   uint64_t elapsed = jet_time_micros();
 */
uint64_t _jet_clock_micros(void);

// --- FANTASY CONSOLE RNG ---
// The random number generator. It is a 32-bit MINSTD linear congruential
// generator, a classic and very small kind, and it is deterministic: the same
// seed always replays the same sequence.

/**
 * Function: jet_randomise
 * Summary: Seeds the random number generator from the clock, so each run of your cart gets different random numbers.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_utils.h
 * Notes: Call it once in _init when you want runs to differ. The seed is mixed from the C library's wall clock and process clock, a value that differs from one run of the program to the next, and a count of how many times it has been called, and then the generator carries on in its usual fixed way from there. It is for variety in a game, not for anything to do with security.
 * Example:
 *   void _init(void) {
 *       jet_randomise();
 *   }
 */
void jet_randomise(void);

// The same call under its US spelling.
#define jet_randomize jet_randomise

/**
 * Function: jet_srand
 * Summary: Seeds the random number generator: chooses where in its fixed sequence the next numbers come from.
 * Parameters:
 *   - seed: Any 32-bit number. The same seed always gives the same sequence.
 * Returns: Nothing.
 * Include: jet_utils.h
 * Notes: Use a fixed seed when you want every run to be the same: a demo, a test, or a level built from random numbers that must come out the same each time. Use a changing seed only when you mean runs to differ; jet_randomise does that for you. Every 32-bit value works. The seed is folded into the range the generator needs, so an awkward seed such as 0 still gives a full-length sequence.
 * Example:
 *   jet_srand(0xC0FFEE42u);
 *   int first_roll = jet_rand_int(1, 6);
 */
void jet_srand(uint32_t seed);

/**
 * Function: jet_rand
 * Summary: Gives you the next raw number from the random number generator.
 * Parameters:
 *   - None.
 * Returns: A number from 1 to 2147483646 (that is 2 to the power 31, minus 2). Bit 31, the top bit, is always 0, so it carries no randomness.
 * Include: jet_utils.h
 * Notes: For a number in a range you choose, use jet_rand_int or jet_randf instead. Use jet_rand itself when you are building your own distribution, mixing bits the way a hash does, or masking bits. Because the range is not a full 32 bits, mask only within the low 31 bits.
 * Example:
 *   uint32_t bits = jet_rand();
 *   bool spawn_bonus = (bits & 7u) == 0u;
 */
uint32_t jet_rand(void);

/**
 * Function: jet_rand_int
 * Summary: Gives you a random whole number between min and max, with both ends possible.
 * Parameters:
 *   - min: The smallest number that can come back.
 *   - max: The largest number that can come back.
 * Returns: A number from min to max, both included; or just min if min is greater than or equal to max.
 * Include: jet_utils.h
 * Notes: Good for dice rolls, picking a tile, choosing a lane, and other small ranges. The distance from min to max must fit in an int. The number is made with a remainder (modulo), so over a very wide range the low end comes up very slightly more often; if that matters to you, build your own distribution on jet_rand.
 * Example:
 *   int tile_variant = jet_rand_int(0, 3);
 *   int damage = jet_rand_int(4, 9);
 */
int jet_rand_int(int min, int max);

/**
 * Function: jet_randf
 * Summary: Gives you a random decimal number from min up to, but not including, max.
 * Parameters:
 *   - min: The smallest value that can come back.
 *   - max: The top of the range. The result stays below it.
 * Returns: A float that is min or more, and normally less than max.
 * Include: jet_utils.h
 * Notes: The number is picked from one of 1000 evenly spaced steps between min and max. That is plenty for particles, screen shake, drift and decoration, but not fine enough for a scientific simulation, and it is not secure.
 * Example:
 *   float angle = jet_randf(-0.25f, 0.25f);
 *   float speed = jet_randf(40.0f, 80.0f);
 */
float jet_randf(float min, float max);

/**
 * Function: jet_rand_angle
 * Summary: Gives you a random direction, as an angle in radians.
 * Parameters:
 *   - None.
 * Returns: An angle from 0 up to, but not including, JET_TAU (a full turn, 2 pi).
 * Include: jet_utils.h
 * Notes: The same as jet_randf(0.0f, JET_TAU). Use it to aim a new rock, scatter particles, or pick a direction to drift in. jet_vector2_from_angle turns the angle into a velocity.
 * Example:
 *   Vector2 drift = jet_vector2_from_angle(jet_rand_angle(), speed);
 */
static inline float jet_rand_angle(void) {
    return jet_randf(0.0f, JET_TAU);
}

// Short names kept so older carts still build. There is no plain rand or
// srand here, on purpose: those names belong to the C standard library.
#define get_stats jet_get_stats
#define draw_fps jet_draw_fps
#define randomise jet_randomise
#define randomize jet_randomise
#define rand_int jet_rand_int
#define randf jet_randf
#define rand_angle jet_rand_angle

#endif // JET_UTILS_H
