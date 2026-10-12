// runtime/jet_copper.h
#ifndef JET_COPPER_H
#define JET_COPPER_H

// Give the background a different colour on every row of the screen. Palette
// index 0, the colour a cleared screen is filled with, normally shows one
// colour everywhere. With the Copper on, each scanline (one horizontal row of
// pixels) can show its own colour instead, picked from the full 24-bit range,
// any mix of red, green and blue rather than one of the 256 in the palette.
// That is how a sky fades from deep blue at the top to orange at the bottom.
// The trick, and the name, come from the Amiga, whose Copper chip changed
// colours as the beam moved down the screen. The core runtime can set one row
// at a time; this opt-in header adds the handy painters: a gradient, a raster
// bar, a hue, and a full-screen rainbow. Include "jet_copper.h", and paint any
// effect that moves again each frame in _draw.
//
// Three things are required before the result can appear:
//   * Call jet_copper_enable(true), normally once in _init().
//   * Enable the project's Copper capability in Shader Lab.
//   * Clear or draw palette index 0 everywhere the scanline background should
//     show through. Non-zero framebuffer pixels remain ordinary foreground.
//
// <nocodex>
// ============================================================================
// Jet64 Copper generators — opt-in per-scanline background-colour painters
// ============================================================================
//
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// The Copper accessors a cart strictly needs (jet_rgb, jet_copper_enable/
// enabled/set/set_rgb/fill) already live in jet_core.h; this header adds the
// ergonomic list-painting *generators* — gradients, bars, a rainbow sweep — so the
// classic "regenerate the Copper list every frame" idiom stays one line. Every
// function here is a pure composition over jet_copper_set, so including the
// header costs nothing unless a cart actually calls one.
//
//   #include "jet64.h"
//   #include "jet_copper.h"   // <-- explicit opt-in
//
//   void _draw(void) {
//       jet_copper_gradient(0, (uint32_t)JET_HEIGHT, jet_rgb(20,10,60), jet_rgb(255,140,40));
//       jet_copper_bar((uint32_t)((float)JET_HEIGHT*0.5f + 40.0f*sinf(t)), 6, jet_rgb(255,255,255));
//   }
//
// A Copper colour is 24-bit truecolour (0x00RRGGBB), so a gradient painted here
// is smooth by construction: every one of the 256 levels per channel is
// reachable, and over a 720-row screen a channel steps roughly every other
// scanline — below the eye's banding threshold, with no dither to fight. The
// generators interpolate in that same 8-bit space, so what you ask for is what
// the scanline gets.
//
// See docs/copper.md for the model (index-0 background shines through) and the
// two-gate activation (this byte + the project's Shader Lab Copper capability).
//
// </nocodex>
// Complete animated-sky example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_copper.h"
//
//   static float time;
//
//   void _init(void) {
//       jet_copper_enable(true);
//   }
//
//   void _update(float dt) {
//       time += dt;
//   }
//
//   void _draw(void) {
//       const uint32_t top = jet_rgb(8, 12, 48);
//       const uint32_t bottom = jet_rgb(240, 96, 40);
//       const uint32_t bar_y = (uint32_t)((float)JET_HEIGHT * 0.5f + 32.0f * sinf(time));
//       jet_copper_gradient(0, (uint32_t)JET_HEIGHT, top, bottom);
//       jet_copper_bar(bar_y, 5, jet_rgb(255, 255, 255));
//       jet_cls(0);
//   }
//
//   void _end(void) {
//       jet_copper_enable(false);
//   }
// ============================================================================

#include "jet_core.h" // jet_copper_set/fill, jet_rgb, JET_COPPER_*_SHIFT, SYS, bool/stdint

/**
 * Function: jet_copper_channel
 * Summary: Pulls one 8-bit colour channel, the red, green or blue part, out of a packed Copper colour.
 * Parameters:
 *   - color: A Copper colour as jet_rgb makes it, packed as 0x00RRGGBB.
 *   - shift: Which channel to read: JET_COPPER_R_SHIFT, JET_COPPER_G_SHIFT or JET_COPPER_B_SHIFT.
 * Returns: The channel's value, from 0 to JET_COPPER_CHANNEL_MAX.
 * Include: jet_copper.h
 * Notes: This is jet_rgb run backwards, one channel at a time. jet_copper_gradient uses it to split its two end colours before blending them channel by channel; in your own code it is handy for tinting or blending a Copper colour you already have.
 * Example:
 *   uint32_t red = jet_copper_channel(jet_rgb(200, 40, 10), JET_COPPER_R_SHIFT); // 200
 */
static inline uint32_t jet_copper_channel(uint32_t color, uint32_t shift) {
    return (color >> shift) & (uint32_t)JET_COPPER_CHANNEL_MAX;
}

/**
 * Function: jet_copper_lerp_channel
 * Summary: Blends between two channel values in a straight line, giving the value that is num/span of the way from a to b.
 * Parameters:
 *   - a: The channel value at the start, where num is 0.
 *   - b: The channel value at the end, where num equals span.
 *   - num: How far along you are, from 0 at a up to span at b.
 *   - span: The length of the range. It must be more than 0, because a span of zero would divide by zero.
 * Returns: The blended channel value, num/span of the way from a to b.
 * Include: jet_copper.h
 * Notes: It uses whole-number arithmetic only, no floats, so a row-by-row gradient stays exact and takes no branches. jet_copper_gradient uses it on each 8-bit channel separately, and makes sure span is more than 0 before calling.
 * Example:
 *   uint32_t mid_green = jet_copper_lerp_channel(0, JET_COPPER_CHANNEL_MAX, 1, 2);
 */
static inline uint32_t jet_copper_lerp_channel(uint32_t a, uint32_t b, uint32_t num, uint32_t span) {
    int32_t delta = (int32_t)b - (int32_t)a;
    return (uint32_t)((int32_t)a + delta * (int32_t)num / (int32_t)span);
}

/**
 * Function: jet_copper_gradient
 * Summary: Paints a smooth vertical colour ramp into the Copper list, over rows y0 up to but not including y1, fading from c0 at the top to c1 at the bottom. This is the call for a sky or a sunset.
 * Parameters:
 *   - y0: The first (top) scanline of the ramp. This row is painted.
 *   - y1: One past the last (bottom) scanline of the ramp. This row is not painted.
 *   - c0: The Copper colour at row y0, made with jet_rgb.
 *   - c1: The Copper colour the ramp reaches at row y1.
 * Returns: Nothing.
 * Include: jet_copper.h
 * Notes: Each channel is blended separately in full 24-bit colour, so the ramp shows no bands: every in-between level exists and nothing is lost to rounding. A range that is empty or upside down (y1 at or before y0) does nothing and is safe. Every row written is checked against the table's bounds by jet_copper_set. The ramp shows only where the screen holds palette index 0.
 * Example:
 *   // A dusk sky filling the whole screen.
 *   jet_copper_gradient(0, (uint32_t)JET_HEIGHT, jet_rgb(20, 10, 60), jet_rgb(255, 140, 40));
 */
static inline void jet_copper_gradient(uint32_t y0, uint32_t y1, uint32_t c0, uint32_t c1) {
    if (y1 <= y0) {
        return;
    }
    uint32_t r0 = jet_copper_channel(c0, JET_COPPER_R_SHIFT);
    uint32_t g0 = jet_copper_channel(c0, JET_COPPER_G_SHIFT);
    uint32_t b0 = jet_copper_channel(c0, JET_COPPER_B_SHIFT);
    uint32_t r1 = jet_copper_channel(c1, JET_COPPER_R_SHIFT);
    uint32_t g1 = jet_copper_channel(c1, JET_COPPER_G_SHIFT);
    uint32_t b1 = jet_copper_channel(c1, JET_COPPER_B_SHIFT);

    uint32_t span = y1 - y0;
    for (uint32_t y = y0; y < y1; ++y) {
        uint32_t num = y - y0;
        jet_copper_set(y, (jet_copper_lerp_channel(r0, r1, num, span) << JET_COPPER_R_SHIFT) |
                               (jet_copper_lerp_channel(g0, g1, num, span) << JET_COPPER_G_SHIFT) |
                               (jet_copper_lerp_channel(b0, b1, num, span) << JET_COPPER_B_SHIFT));
    }
}

/**
 * Function: jet_copper_bar
 * Summary: Paints a solid horizontal band of one colour, the classic demoscene raster bar. Change y each frame to sweep it up and down the screen.
 * Parameters:
 *   - y: The top scanline of the band.
 *   - thickness: How tall the band is, in scanlines.
 *   - color: The Copper colour for every row of the band, made with jet_rgb.
 * Returns: Nothing.
 * Include: jet_copper.h
 * Notes: Each row is checked against the table's bounds by jet_copper_set, so a band that runs off the bottom of the Copper table is cut short rather than wrapping round to the top. Like every Copper colour, it shows only where the screen holds palette index 0.
 * Example:
 *   uint32_t y = (uint32_t)((float)JET_HEIGHT * 0.5f + 40.0f * sinf(t));
 *   jet_copper_bar(y, 6, jet_rgb(255, 255, 255));
 */
static inline void jet_copper_bar(uint32_t y, uint32_t thickness, uint32_t color) {
    for (uint32_t row = y; row < y + thickness; ++row) {
        jet_copper_set(row, color);
    }
}

/**
 * Function: jet_copper_hue
 * Summary: Turns a position around the colour wheel into a Copper colour as vivid and as bright as it can be: one slice of the rainbow.
 * Parameters:
 *   - h: Where on the colour wheel, from 0.0 (red) through yellow, green, cyan, blue and magenta and back to red at 1.0. A value outside 0 to 1 is wrapped round.
 * Returns: The 24-bit Copper colour for that hue, fully saturated and at full brightness.
 * Include: jet_copper.h
 * Notes: It walks the six segments of the HSV colour wheel at the Copper's full 8 bits per channel, so neighbouring hues are truly different rather than landing on the same level. jet_copper_rainbow uses it to colour each scanline; call it yourself to map any single value onto a spectrum.
 * Example:
 *   uint32_t green = jet_copper_hue(1.0f / 3.0f);
 */
static inline uint32_t jet_copper_hue(float h) {
    h -= floorf(h);
    float sextant = h * 6.0f;
    int segment = (int)sextant;
    float frac = sextant - (float)segment;
    float up = frac;
    float down = 1.0f - frac;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    switch (segment) {
        case 0:  r = 1.0f; g = up;   b = 0.0f; break;
        case 1:  r = down; g = 1.0f; b = 0.0f; break;
        case 2:  r = 0.0f; g = 1.0f; b = up;   break;
        case 3:  r = 0.0f; g = down; b = 1.0f; break;
        case 4:  r = up;   g = 0.0f; b = 1.0f; break;
        default: r = 1.0f; g = 0.0f; b = down; break; // segment 5
    }
    const float full = (float)JET_COPPER_CHANNEL_MAX;
    return jet_rgb((uint8_t)(r * full + 0.5f), (uint8_t)(g * full + 0.5f),
                    (uint8_t)(b * full + 0.5f));
}

/**
 * Function: jet_copper_rainbow
 * Summary: Fills the Copper list with a full-screen spectrum, spreading one whole turn of the colour wheel over the height of the screen. Move phase a little each frame to make the rainbow scroll.
 * Parameters:
 *   - phase: A hue offset added to every row, in the same 0 to 1 units as jet_copper_hue. Add a little to it each frame to animate the sweep; it wraps round by itself.
 * Returns: Nothing.
 * Include: jet_copper.h
 * Notes: It covers rows 0 up to the screen's live height, held to JET_COPPER_MAX_LINES so an oversized screen never runs past the end of the Copper list. A screen of zero height does nothing and is safe. As always, the spectrum shows only where the screen holds palette index 0.
 * Example:
 *   static float phase = 0.0f;
 *   void _draw(void) { jet_copper_rainbow(phase); phase += 0.002f; }
 */
static inline void jet_copper_rainbow(float phase) {
    uint32_t height = SYS[SYS_SCREEN_HEIGHT];
    if (height > (uint32_t)JET_COPPER_MAX_LINES) {
        height = (uint32_t)JET_COPPER_MAX_LINES;
    }
    if (height == 0) {
        return;
    }
    for (uint32_t y = 0; y < height; ++y) {
        jet_copper_set(y, jet_copper_hue((float)y / (float)height + phase));
    }
}

#endif // JET_COPPER_H
