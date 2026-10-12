// runtime/jet_shader.h
#ifndef JET_SHADER_H
#define JET_SHADER_H

#include <stdbool.h>
#include <stdint.h>

// Drive your cartridge's own screen shader from its C. A screen shader is the
// file data/shader/screen.glsl, run over the whole picture every frame once
// Custom is ticked in the Shader Lab; it reads thirty-two numbers you set here
// as jet_param[0] to jet_param[7], four to a vec4. Set a number when the game
// moves (a shockwave's radius, the water's level, how hurt the player is) and
// the shader draws it on the next frame. You can also switch off, and back on,
// any effect the creator ticked (Glow, Vector Glow, Anti-Aliasing, CRT
// Scanlines, the Colour Grade and the custom stage itself), which is how an
// options page offers "Scanlines: ON / OFF".
//
// Include "jet_shader.h" yourself; jet64.h does not include it for you.
//
// Two rules come with it.
//
//   1. THE GAME MUST PLAY WITHOUT THE SHADER. A computer with no GPU, a lost
//      graphics context, or a shader the Workstation is holding after a crash
//      all show the plain picture instead, so never put something the player
//      needs to know only in the shader. jet_shader_status() says whether the
//      stage is on screen: when it is not JET_SHADER_PRESENTING, give the same
//      news in your own pixels (a flash of a palette entry, a line of text).
//   2. THE SHADER RUNS IN DEEP WATER. Your C runs in its own sandboxed process,
//      so a crash in it is a Guru Meditation and nothing worse. The shader runs
//      on the Workstation's own graphics device, outside that sandbox. The
//      Shader Lab guards against a shader that takes the Workstation down (it
//      holds that shader on the next launch until you choose to run it again),
//      but a fatal shader is made rare and recoverable, not impossible.
//
// The numbers and switches are plain stores into the machine's memory: set
// them once in _init and leave them, or every tick, at no cost either way.
// Everything here works whatever the host can do; on a host that presents
// without the shader, the numbers are simply not drawn.
//
// Complete example (copy into main.c, tick Custom, and put the shader in its
// comment into data/shader/screen.glsl):
//
//   #include "jet64.h"
//   #include "jet_shader.h"
//   // data/shader/screen.glsl:
//   //   vec3 jet_custom(vec2 vpix, vec3 colour) {
//   //       return mix(colour, vec3(1.0, 0.0, 0.0), jet_param[0].x); // red
//   //   }
//
//   #define HURT_PARAM   0    // jet_param[0].x
//   #define FADE_PER_SEC 3.0f
//   #define FLASH_INK    8    // a palette entry for the fallback
//
//   // 1 at the moment of a hit, fading to 0.
//   static float hurt;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       if (jet_key_pressed(JET_KEY_SPACE)) hurt = 1.0f; // stand-in for a hit
//       hurt = hurt > FADE_PER_SEC * dt ? hurt - FADE_PER_SEC * dt : 0.0f;
//       jet_shader_param(HURT_PARAM, hurt);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       // No shader on screen: flash the border in the cartridge's own pixels.
//       if (hurt > 0.0f && jet_shader_status() != JET_SHADER_PRESENTING) {
//           jet_rect(0, 0, JET_WIDTH, JET_HEIGHT, FLASH_INK);
//       }
//   }
//
//   void _end(void) {}

/**
 * Enum: JetShaderStatus
 * Summary: Whether your cartridge's screen shader is on screen, and if not, why not.
 * Include: jet_shader.h
 * Notes: JET_SHADER_ABSENT means there is no stage to show: Custom is not ticked, or this host presents the picture without a GPU. JET_SHADER_PRESENTING means the stage is compiled and on screen. JET_SHADER_DISABLED means the cartridge switched it off itself, with jet_shader_effect(JET_SHADER_CUSTOM, false). JET_SHADER_HELD means the Workstation is holding it after it took the Workstation down, until the creator chooses to run it again. JET_SHADER_FAILED means it did not compile on this computer, so the plain picture is showing. Anything other than PRESENTING means the player sees the plain picture: give them the news in your own pixels.
 * Example:
 *   if (jet_shader_status() != JET_SHADER_PRESENTING) draw_fallback_flash();
 */
typedef enum JetShaderStatus {
    JET_SHADER_ABSENT = 0,     // Custom is not ticked, or this host presents on the CPU
    JET_SHADER_PRESENTING,     // the stage is compiled and on screen
    JET_SHADER_DISABLED,       // the cart switched it off: jet_shader_effect(JET_SHADER_CUSTOM, false)
    JET_SHADER_HELD,           // the Workstation's guard is holding it
    JET_SHADER_FAILED,         // it did not compile here; the plain picture is showing
} JetShaderStatus;

/**
 * Enum: JetShaderEffect
 * Summary: The effects a cartridge can switch off and on while it runs: the Shader Lab's built-in effects and the cartridge's own screen shader.
 * Include: jet_shader.h
 * Notes: Only an effect the creator ticked in the Shader Lab is in the picture at all, so only that one can be switched; ask jet_shader_effect_available before you offer it. A ticked effect starts on. Switching never recompiles anything, so it is instant and free.
 * Example:
 *   jet_shader_effect(JET_SHADER_CRT, options.scanlines);
 */
typedef enum JetShaderEffect {
    JET_SHADER_GLOW = 0,
    JET_SHADER_VECTOR_GLOW,
    JET_SHADER_AA,
    JET_SHADER_CRT,
    JET_SHADER_GRADE,
    JET_SHADER_CUSTOM,
} JetShaderEffect;

/**
 * Function: jet_shader_param
 * Summary: Sets one of the thirty-two numbers your screen shader reads.
 * Parameters:
 *   - index: Which number, 0 to 31. Number i is component i % 4 of jet_param[i / 4], so 0 is jet_param[0].x and 5 is jet_param[1].y. Any other index is ignored.
 *   - value: The number.
 * Returns: Nothing.
 * Include: jet_shader.h
 * Notes: The shader sees the new value from the next frame drawn. Every number starts at zero when the cartridge cold boots; a Warm Boot keeps what you last set. Name your indices once (a #define per number) so the C and the shader agree on which is which.
 * Example:
 *   jet_shader_param(SHOCK_RADIUS, radius);
 */
void jet_shader_param(int index, float value);

/**
 * Function: jet_shader_param4
 * Summary: Sets four of the shader's numbers at once: the whole of one jet_param vec4.
 * Parameters:
 *   - slot: Which vec4, 0 to 7: jet_param[slot]. Any other slot is ignored.
 *   - x: The vec4's .x, number slot * 4.
 *   - y: Its .y.
 *   - z: Its .z.
 *   - w: Its .w.
 * Returns: Nothing.
 * Include: jet_shader.h
 * Notes: The same as four jet_shader_param calls, for numbers that belong together, such as a position and a size.
 * Example:
 *   jet_shader_param4(SHOCK_SLOT, centre_x, centre_y, radius, strength);
 */
void jet_shader_param4(int slot, float x, float y, float z, float w);

/**
 * Function: jet_shader_param_get
 * Summary: Reads back one of the thirty-two numbers your screen shader reads.
 * Parameters:
 *   - index: Which number, 0 to 31.
 * Returns: The number last set at that index, 0.0f if it was never set since a cold boot, and 0.0f for any index out of range.
 * Include: jet_shader.h
 * Notes: What the shader reads is exactly this value: nothing else writes the numbers.
 * Example:
 *   float level = jet_shader_param_get(WATER_LEVEL);
 */
float jet_shader_param_get(int index);

/**
 * Function: jet_shader_status
 * Summary: Tells you whether your screen shader is on screen right now.
 * Parameters:
 *   - None.
 * Returns: A JetShaderStatus: JET_SHADER_PRESENTING when the player sees the stage, and otherwise why they do not.
 * Include: jet_shader.h
 * Notes: The host writes it: an exported player before _init and whenever its picture changes path, the Workstation with every frame it presents. A game must stay playable without its shader, so when this is not JET_SHADER_PRESENTING, show anything the shader would have told the player in your own pixels instead.
 * Example:
 *   if (jet_shader_status() != JET_SHADER_PRESENTING) jet_palette_put(FLASH_INK, 255, 0, 0);
 */
JetShaderStatus jet_shader_status(void);

/**
 * Function: jet_shader_effect
 * Summary: Switches one of the picture's effects off, or back on, while the cartridge runs.
 * Parameters:
 *   - effect: Which effect, a JetShaderEffect. Any other value is ignored.
 *   - on: true to show it, false to switch it off.
 * Returns: Nothing.
 * Include: jet_shader.h
 * Notes: Only an effect the creator ticked can be shown; switching on one that was not ticked changes nothing (see jet_shader_effect_available). Every ticked effect starts on; to start with one off, switch it off in _init. The switch takes effect on the next frame drawn, with no pause. Saving the player's choice is your cartridge's job: write it with jet_io like any other setting.
 * Example:
 *   jet_shader_effect(JET_SHADER_CRT, false); // the player chose Scanlines: OFF
 */
void jet_shader_effect(JetShaderEffect effect, bool on);

/**
 * Function: jet_shader_effect_on
 * Summary: Tells you whether an effect is in the picture right now: ticked, compiled on this host, and not switched off.
 * Parameters:
 *   - effect: Which effect, a JetShaderEffect.
 * Returns: true when the effect is showing; false when it is switched off, not available here, or not a JetShaderEffect.
 * Include: jet_shader.h
 * Notes: Use it to draw an options page's ON / OFF truthfully.
 * Example:
 *   const char* label = jet_shader_effect_on(JET_SHADER_GLOW) ? "ON" : "OFF";
 */
bool jet_shader_effect_on(JetShaderEffect effect);

/**
 * Function: jet_shader_effect_available
 * Summary: Tells you whether an effect can be switched here: the creator ticked it, and this host compiled it in.
 * Parameters:
 *   - effect: Which effect, a JetShaderEffect.
 * Returns: true when the effect is in this host's picture to be switched; false otherwise, and for anything that is not a JetShaderEffect.
 * Include: jet_shader.h
 * Notes: Offer an effect on an options page only when this answers true. The host writes the answer: an exported player before _init, the Workstation as it presents its first frame. It is false on a host that presents without a GPU, and for the custom stage while it is not on screen.
 * Example:
 *   if (jet_shader_effect_available(JET_SHADER_CRT)) add_option("Scanlines", JET_SHADER_CRT);
 */
bool jet_shader_effect_available(JetShaderEffect effect);

/**
 * Function: jet_shader_colormap
 * Summary: Hands your screen shader a light table: how each palette colour looks under each level of light.
 * Parameters:
 *   - colormap: The table, light_levels rows of 256 palette indices, row-major: colormap[level * 256 + index] is colour index as it looks at that level. It is the very table jet_triblit_lit takes. NULL clears the table.
 *   - light_levels: How many rows, 1 to 32. 0 clears the table; more than 32 is refused and the table is left as it was.
 * Returns: true when the table was taken (or cleared), false when it was refused.
 * Include: jet_shader.h
 * Notes: The table is copied, so you may change or free yours afterwards. Call it once, in _init, and again only when the table itself changes: each call makes every host upload it afresh. The shader reads it through jet_colormap(index, level) and jet_colormap_levels, from its jet_custom_index hook, so every pixel it relights is still one of your 256 colours.
 * Example:
 *   jet_shader_colormap(s_colormap, COLORMAP_LEVELS); // the same table the walls use
 */
bool jet_shader_colormap(const uint8_t* colormap, uint32_t light_levels);

#endif // JET_SHADER_H
