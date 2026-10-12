// runtime/jet_plasma.h
#ifndef JET_PLASMA_H
#define JET_PLASMA_H

#include <stdint.h>
#include <stdbool.h>

// The classic plasma: a field of colour that swirls and ripples on its own,
// the old demo-scene effect. It is built from lookup tables kept in WRAM, the
// cartridge's working memory. Make a JetPlasmaConfig if you want to tune it,
// set the JetPlasma up once in _init, move it on in _update with dt, and draw
// it in _draw. This is an opt-in module: include both jet64.h and
// jet_plasma.h.
//
// Complete plasma example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_plasma.h"
//
//   static JetPlasma plasma;
//
//   void _init(void) {
//       jet_plasma_init(&plasma, JET_WIDTH, JET_HEIGHT, NULL);
//   }
//
//   void _update(float dt) {
//       jet_plasma_update(&plasma, dt);
//   }
//
//   void _draw(void) {
//       jet_plasma_draw(&plasma, 0, 0);
//       jet_print("PLASMA", 16, 16, 15);
//   }
//
//   void _end(void) {}

// The default settings: what you get when you pass NULL to jet_plasma_init.
#define PLASMA_DEFAULT_RIPPLE_FREQUENCY 5.4f
#define PLASMA_DEFAULT_INTERFERENCE     9.5f
#define PLASMA_DEFAULT_COMPLEXITY       50
#define PLASMA_DEFAULT_CONTRAST         4
#define PLASMA_DEFAULT_SPEED            1.0f
#define PLASMA_DEFAULT_SWAY             1.6f
#define PLASMA_DEFAULT_FOLDS            2
#define PLASMA_DEFAULT_BRIGHTEN         0

// The fold table has one entry for every possible 8-bit value the draw loop
// can add up to.
#define PLASMA_FOLD_LUT_SIZE            256

/**
 * Struct: JetPlasmaConfig
 * Summary: The settings you can tune before jet_plasma_init. Start from the PLASMA_DEFAULT_ constants for the stock effect and change one value at a time.
 * Fields:
 *   - speed: How fast the whole pattern moves, as a multiplier. 1.0f is the default; 0.0f freezes it.
 *   - ripple_frequency: How tightly the ripples are packed; default 5.4f.
 *   - interference: How dense the interference pattern is, where the waves cross and cancel; default 9.5f.
 *   - sway: How much the pattern sways about; default 1.6f.
 *   - contrast: The contrast, as a multiplier; default 4.
 *   - complexity: How much fine detail the interference has; default 50.
 *   - folds: How many times the colour gradient is folded back on itself, from 1 to 4, for a denser pattern; default 2.
 *   - brighten: 0 for the normal colours, or 1 for the brighter set.
 * Include: jet_plasma.h
 * Notes: jet_plasma_init copies these values into its own state, so the config can be a temporary. Pass NULL to jet_plasma_init to use every default.
 * Example:
 *   JetPlasmaConfig cfg = jet_make_plasma_config(1.0f, 5.4f, 9.5f, 1.6f, 4, 50, 2, 0);
 */
typedef struct {
    float speed;
    float ripple_frequency;
    float interference;
    float sway;
    int contrast;
    int complexity;
    int folds;              // Number of times to fold the gradient (1-4) for density
    int brighten;           // 0 = normal 1 = brighten
} JetPlasmaConfig;

/**
 * Function: jet_make_plasma_config
 * Summary: Builds a JetPlasmaConfig from the eight settings, in one call.
 * Parameters:
 *   - speed: How fast the whole pattern moves, as a multiplier.
 *   - ripple_frequency: How tightly the ripples are packed.
 *   - interference: How dense the interference pattern is, where the waves cross and cancel.
 *   - sway: How much the pattern sways about.
 *   - contrast: The contrast, as a multiplier.
 *   - complexity: How much fine detail the interference has.
 *   - folds: How many times the colour gradient is folded back on itself, from 1 to 4.
 *   - brighten: 0 for the normal colours, or 1 for the brighter set.
 * Returns: A filled-in JetPlasmaConfig, handed back by value.
 * Include: jet_plasma.h
 * Notes: jet_plasma_init copies the value, so the config can be a local variable. When every default suits you, pass NULL to jet_plasma_init instead.
 * Example:
 *   JetPlasmaConfig cfg = jet_make_plasma_config(1.0f, 5.4f, 9.5f, 1.6f, 4, 50, 2, 0);
 */
static inline JetPlasmaConfig jet_make_plasma_config(float speed, float ripple_frequency, float interference, float sway, int contrast, int complexity, int folds, int brighten) {
    JetPlasmaConfig cfg;
    cfg.speed = speed;
    cfg.ripple_frequency = ripple_frequency;
    cfg.interference = interference;
    cfg.sway = sway;
    cfg.contrast = contrast;
    cfg.complexity = complexity;
    cfg.folds = folds;
    cfg.brighten = brighten;
    return cfg;
}

/**
 * Struct: JetPlasma
 * Summary: Everything one plasma effect needs: its two lookup tables in WRAM, its size, its settings, and the phases of the oscillators that move it.
 * Fields:
 *   - table1: The first lookup table, worked out in advance and reserved from WRAM by jet_plasma_init.
 *   - table2: The second lookup table, worked out in advance and reserved from WRAM by jet_plasma_init.
 *   - width: The width of the effect, in pixels.
 *   - height: The height of the effect, in pixels.
 *   - circle1: One of the eight oscillator phases that move the pattern; internal.
 *   - circle2: One of the eight oscillator phases that move the pattern; internal.
 *   - circle3: One of the eight oscillator phases that move the pattern; internal.
 *   - circle4: One of the eight oscillator phases that move the pattern; internal.
 *   - circle5: One of the eight oscillator phases that move the pattern; internal.
 *   - circle6: One of the eight oscillator phases that move the pattern; internal.
 *   - circle7: One of the eight oscillator phases that move the pattern; internal.
 *   - circle8: One of the eight oscillator phases that move the pattern; internal.
 *   - speed: The speed multiplier, copied from the config.
 *   - ripple_frequency: The ripple spacing, copied from the config.
 *   - interference: The interference density, copied from the config.
 *   - sway: The sway amount, copied from the config.
 *   - contrast: The contrast multiplier, copied from the config.
 *   - complexity: The interference detail, copied from the config.
 *   - brighten: true when the brighter colours are in use.
 *   - folds: How many times the gradient is folded, from 1 to 4.
 *   - fold_lut: A 256-entry table of fold results, worked out in advance, that the draw loop reads.
 * Include: jet_plasma.h
 * Notes: Zero it, or keep it in static storage, before jet_plasma_init. The two table pointers borrow WRAM that lasts as long as the cart does; nothing ever gives it back, and setting up the same JetPlasma again takes more WRAM each time.
 * Example:
 *   static JetPlasma plasma;
 *   bool ready = jet_plasma_init(&plasma, JET_WIDTH, JET_HEIGHT, NULL);
 */
typedef struct {
    uint8_t* table1;
    uint8_t* table2;
    int width;
    int height;

    float circle1, circle2, circle3, circle4;
    float circle5, circle6, circle7, circle8;

    float speed;
    float ripple_frequency;
    float interference;
    float sway;
    int contrast;
    int complexity;
    bool brighten;

    int folds; // Number of times to fold the gradient (1-4) for density

    uint8_t fold_lut[PLASMA_FOLD_LUT_SIZE]; // Precomputed fold result per input byte, so the inner
                           // draw loop is a single table lookup with no branches.
} JetPlasma;

/**
 * Function: jet_plasma_init
 * Summary: Sets up a plasma effect: reserves its lookup tables from WRAM and fills them in, mirrored, ready to draw.
 * Parameters:
 *   - plasma: The JetPlasma to set up. It must stay valid for every later update and draw call.
 *   - width: The width of the drawn plasma, in pixels.
 *   - height: The height of the drawn plasma, in pixels.
 *   - config: Your tuning values, or NULL to use the PLASMA_DEFAULT_ settings.
 * Returns: true when it worked; false if plasma is NULL, width or height is not above 0, or the two tables do not fit what is left of WRAM, in which case not one byte of WRAM has been taken.
 * Include: jet_plasma.h
 * Notes: It reserves two lookup tables from the console's bump allocator, each width*height*4 bytes. The allocator hands memory out and never takes it back, so call this once, from _init(). Make width and height match the area you mean to draw.
 * Example:
 *   JetPlasma plasma;
 *   bool ok = jet_plasma_init(&plasma, 320, 180, NULL);
 */
bool jet_plasma_init(JetPlasma* plasma, int width, int height, const JetPlasmaConfig* config);

/**
 * Function: jet_plasma_update
 * Summary: Moves the plasma on by dt, advancing the oscillators that swirl the pattern.
 * Parameters:
 *   - plasma: A JetPlasma set up with jet_plasma_init().
 *   - dt: The time since the last update, in seconds.
 * Returns: Nothing.
 * Include: jet_plasma.h
 * Notes: Call it once in every _update, before jet_plasma_draw(); a NULL plasma does nothing. How fast it moves is set by the JetPlasma's own settings, which came from the JetPlasmaConfig you gave jet_plasma_init, or from the defaults.
 * Example:
 *   jet_plasma_update(&plasma, dt);
 */
void jet_plasma_update(JetPlasma* plasma, float dt);

/**
 * Function: jet_plasma_draw
 * Summary: Draws the plasma onto the screen with its top-left corner at the position you give, respecting the camera.
 * Parameters:
 *   - plasma: A JetPlasma set up with jet_plasma_init().
 *   - x: The left edge, in world pixels, or screen pixels if you have not moved the camera.
 *   - y: The top edge, in world pixels, or screen pixels if you have not moved the camera.
 * Returns: Nothing.
 * Include: jet_plasma.h
 * Notes: The camera's offset is taken off the position, and the drawing is cut to the clip rectangle. A NULL plasma, or one that was never set up, draws nothing. Every pixel written is a palette index, one byte per pixel.
 * Example:
 *   jet_plasma_draw(&plasma, 0, 0);
 */
void jet_plasma_draw(JetPlasma* plasma, int x, int y);

#endif // JET_PLASMA_H
