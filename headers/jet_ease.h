// runtime/jet_ease.h
#ifndef JET_EASE_H
#define JET_EASE_H

#include <stdint.h>

// Easing makes a moving value speed up, slow down, bounce or overshoot instead
// of travelling at one steady rate. This header is opt-in: jet64.h does not
// bring it in, so include "jet_ease.h" yourself when a camera, a menu, a
// sprite, a colour, or any other number should move with some character.
//
// The curves are Robert Penner's, and every one of the direct functions takes
// the same four arguments:
//   t = elapsed time, b = beginning value, c = total change, d = duration.
// c is the CHANGE, not the final value. To move x from 20 to 100, pass b = 20
// and c = 80. To get a plain fraction from 0 to 1 back, use b = 0, c = 1 and
// d = 1.
//
// d must be greater than zero. Every curve divides by d, so a duration of zero
// divides by zero. t is not held between 0 and d; a t outside that span keeps
// following the curve past its ends. When you would rather be safe, jet_ease
// below takes a progress from 0 to 1, holds it in that range, and can never
// divide by zero.
//
// The recipe most carts want, moving from START_X to END_X over MOVE_SECONDS:
//   move_time += dt;
//   if (move_time > MOVE_SECONDS) move_time = MOVE_SECONDS;
//   x = jet_ease_sine_in_out(move_time, START_X, END_X - START_X, MOVE_SECONDS);
//
// If you already have a progress from 0 to 1, pick a curve by number instead:
//   float eased = jet_ease(progress, JET_CURVE_BOUNCE);
//
// Complete eased-motion example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_ease.h"
//
//   static float elapsed;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       const float duration = 2.0f;
//       elapsed = jet_wrapf(elapsed + dt, duration);
//   }
//
//   void _draw(void) {
//       const float duration = 2.0f;
//       const float start_x = 32.0f;
//       const float end_x = (float)JET_WIDTH - 32.0f;
//       const float x = jet_ease_sine_in_out(elapsed, start_x, end_x - start_x, duration);
//       jet_cls(0);
//       jet_circfill((int)x, JET_HEIGHT / 2, 12, 12);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_ease_linear
 * Summary: Moves a value from its start to its end at one steady rate, with no easing at all.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c. To go from 20 to 100, pass 80.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t, on the straight line from b to b + c.
 * Include: jet_ease.h
 * Notes: t is not held between 0 and d, so a t past d carries on past the end value. Keep t inside 0 to d yourself, as the banner's recipe does, when the value must stop at the end.
 * Example:
 *   float x = jet_ease_linear(timer, 0.0f, 128.0f, 1.0f);
 */
float jet_ease_linear(float t, float b, float c, float d);

/**
 * Function: jet_ease_sine_in
 * Summary: Starts slowly and speeds up, following the gentle rise of a sine wave.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Choose it for something that should get going gently before it picks up speed. The example fades a value in that way: nearly still at first, then quick.
 * Example:
 *   float alpha = jet_ease_sine_in(t, 0.0f, 1.0f, duration);
 */
float jet_ease_sine_in(float t, float b, float c, float d);

/**
 * Function: jet_ease_sine_out
 * Summary: Starts quickly and slows to a smooth stop, following a sine wave.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Choose it for something arriving: a fade that settles, or a panel that slides into place and stops softly. The example brings a value from start_y to end_y that way.
 * Example:
 *   float y = jet_ease_sine_out(t, start_y, end_y - start_y, duration);
 */
float jet_ease_sine_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_sine_in_out
 * Summary: Speeds up smoothly through the first half of the move and slows down smoothly through the second, following a sine wave.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: A good first choice for a camera move or a menu sliding in, because neither end snaps. The banner's recipe uses it.
 * Example:
 *   float cam_x = jet_ease_sine_in_out(t, from_x, to_x - from_x, 0.35f);
 */
float jet_ease_sine_in_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_quad_in
 * Summary: Starts slowly and speeds up, following a quadratic curve, where the distance covered grows with the square of the time.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: It is cheaper than the sine curves and suits a gentle, understated pick-up of speed. The example ramps a speed up from 0 to max_speed over one second.
 * Example:
 *   float speed = jet_ease_quad_in(t, 0.0f, max_speed, 1.0f);
 */
float jet_ease_quad_in(float t, float b, float c, float d);

/**
 * Function: jet_ease_quad_out
 * Summary: Starts quickly and slows down, following a quadratic curve.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Choose it for something that should respond at once and then soften as it reaches its target. The example pops a scale from 0.5 up to 1.0 that way.
 * Example:
 *   float scale = jet_ease_quad_out(t, 0.5f, 0.5f, pop_time);
 */
float jet_ease_quad_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_quad_in_out
 * Summary: Speeds up through the first half of the move and slows through the second, following a quadratic curve each way, in one smooth movement.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Balanced and cheap, which makes it a strong everyday curve for things that move during play. The example opens a door by 64 pixels in half a second.
 * Example:
 *   float door = jet_ease_quad_in_out(t, 0.0f, 64.0f, 0.5f);
 */
float jet_ease_quad_in_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_cubic_in
 * Summary: Starts very slowly and then speeds up hard, following a cubic curve, where the distance covered grows with the cube of the time.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: More dramatic than jet_ease_quad_in: it lingers longer near the start and rushes the end. The example builds a flash up over a fifth of a second.
 * Example:
 *   float flash = jet_ease_cubic_in(t, 0.0f, 1.0f, 0.2f);
 */
float jet_ease_cubic_in(float t, float b, float c, float d);

/**
 * Function: jet_ease_cubic_out
 * Summary: Starts quickly and glides to a stop, following a cubic curve.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Choose it for interface motion that must feel quick to respond and still smooth at the very end. The example slides a panel in from off the left edge in a quarter of a second.
 * Example:
 *   float panel_x = jet_ease_cubic_out(t, -160.0f, 160.0f, 0.25f);
 */
float jet_ease_cubic_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_cubic_in_out
 * Summary: Speeds up through the first half of the move and slows through the second, following a cubic curve each way, so both ends are held longer than the quadratic version holds them.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Stronger than jet_ease_quad_in_out, with a more marked pause at each end and a quicker middle, close to the S-shaped curve known as smoothstep. It suits a cinematic transition. The example fades a value from 0 to 255 over one second.
 * Example:
 *   float fade = jet_ease_cubic_in_out(t, 0.0f, 255.0f, 1.0f);
 */
float jet_ease_cubic_in_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_bounce_out
 * Summary: Arrives quickly, hits its target and bounces off it a few times, each bounce smaller than the last, before coming to rest.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: Choose it for playful interface motion, a landing, a hit, or a pickup dropping into place. The example drops a value from -40 down to 0 with a bounce at the end, over 0.6 seconds.
 * Example:
 *   float y = jet_ease_bounce_out(t, -40.0f, 40.0f, 0.6f);
 */
float jet_ease_bounce_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_bounce_in
 * Summary: Starts with a few small bounces, each bigger than the last, then leaves for its target in one rush.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: It is jet_ease_bounce_out played backwards. Use it for a wind-up, where something jiggles before it goes. The example squashes a scale from 1.0 down to 0.75 over a fifth of a second.
 * Example:
 *   float squash = jet_ease_bounce_in(t, 1.0f, -0.25f, 0.2f);
 */
float jet_ease_bounce_in(float t, float b, float c, float d);

/**
 * Function: jet_ease_bounce_in_out
 * Summary: Bounces at the start, as jet_ease_bounce_in does, and bounces again at the end, as jet_ease_bounce_out does.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: The two halves meet at the midpoint without a jump, so the value bounces away from where it started and bounces again as it arrives. The example bobs a value 20 pixels over 0.8 seconds.
 * Example:
 *   float icon_y = jet_ease_bounce_in_out(t, 0.0f, 20.0f, 0.8f);
 */
float jet_ease_bounce_in_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_elastic_in
 * Summary: Pulls back and wobbles like a stretched spring, then snaps to its target.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: It calls powf and sinf, which cost more than the quadratic and cubic curves, so save it for motion that is meant to be noticed. The example shrinks a scale from 1.0 by 0.4 with a springy pull-back first.
 * Example:
 *   float scale = jet_ease_elastic_in(t, 1.0f, -0.4f, 0.35f);
 */
float jet_ease_elastic_in(float t, float b, float c, float d);

/**
 * Function: jet_ease_elastic_out
 * Summary: Shoots past its target, springs back, and wobbles to rest, like a plucked ruler.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t. Because it overshoots, the value passes beyond b + c for a moment before it settles.
 * Include: jet_ease.h
 * Notes: A strong choice for a punchy menu reveal, a pickup, or a "well done" flourish. The example grows a scale from 0 to 1 with a springy overshoot.
 * Example:
 *   float scale = jet_ease_elastic_out(t, 0.0f, 1.0f, 0.45f);
 */
float jet_ease_elastic_out(float t, float b, float c, float d);

/**
 * Function: jet_ease_elastic_in_out
 * Summary: Winds back with a spring at the start and springs past the target at the end, with a smooth run through the middle.
 * Parameters:
 *   - t: How far into the move you are: the time elapsed, or a progress from 0 to 1 when d is 1.
 *   - b: The value at the start.
 *   - c: The total change, so the value at the end is b + c.
 *   - d: How long the whole move takes, in the same units as t, or 1 when t is a progress from 0 to 1. Must be greater than zero.
 * Returns: The value at time t.
 * Include: jet_ease.h
 * Notes: The most expressive curve in this header and the most expensive, so use it sparingly, for a title dropping in or another moment that deserves it. The example brings a title down from -32 to 32 over one second.
 * Example:
 *   float title_y = jet_ease_elastic_in_out(t, -32.0f, 64.0f, 1.0f);
 */
float jet_ease_elastic_in_out(float t, float b, float c, float d);

// The older raylib-style names, kept so carts that still use them build.
#define EaseLinear jet_ease_linear
#define EaseSineIn jet_ease_sine_in
#define EaseSineOut jet_ease_sine_out
#define EaseSineInOut jet_ease_sine_in_out
#define EaseQuadIn jet_ease_quad_in
#define EaseQuadOut jet_ease_quad_out
#define EaseQuadInOut jet_ease_quad_in_out
#define EaseCubicIn jet_ease_cubic_in
#define EaseCubicOut jet_ease_cubic_out
#define EaseCubicInOut jet_ease_cubic_in_out
#define EaseBounceOut jet_ease_bounce_out
#define EaseBounceIn jet_ease_bounce_in
#define EaseBounceInOut jet_ease_bounce_in_out
#define EaseElasticIn jet_ease_elastic_in
#define EaseElasticOut jet_ease_elastic_out
#define EaseElasticInOut jet_ease_elastic_in_out

// --- CURVES CHOSEN BY NUMBER ---
// A curve can also be named by a small number, which is how an animation
// stores the curve for its scale, rotation and tint. jet_ease turns that
// number and a progress from 0 to 1 into an eased progress.

/**
 * Constant: JET_CURVE_LINEAR
 * Summary: The curve number for jet_ease that means no easing: one steady rate from start to end.
 * Include: jet_ease.h
 * Notes: jet_ease gives back its clamped progress unchanged. Choose it when something takes a curve number but this move should stay uniform.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_LINEAR);
 */
#define JET_CURVE_LINEAR 0

/**
 * Constant: JET_CURVE_EASE_IN_QUAD
 * Summary: The curve number for jet_ease that starts slowly and speeds up, following a quadratic curve.
 * Include: jet_ease.h
 * Notes: Gentle at first and quickest at the end. It suits something leaving, or something starting to fall.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_EASE_IN_QUAD);
 */
#define JET_CURVE_EASE_IN_QUAD 1

/**
 * Constant: JET_CURVE_EASE_OUT_QUAD
 * Summary: The curve number for jet_ease that starts quickly and slows to a stop, following a quadratic curve.
 * Include: jet_ease.h
 * Notes: Quick to respond and gentle to settle. A strong first choice for a button, a camera arriving, or a panel sliding in.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_EASE_OUT_QUAD);
 */
#define JET_CURVE_EASE_OUT_QUAD 2

/**
 * Constant: JET_CURVE_EASE_IN_OUT_QUAD
 * Summary: The curve number for jet_ease that speeds up through the first half and slows through the second, following a quadratic curve each way.
 * Include: jet_ease.h
 * Notes: Smooth at both ends. It suits a whole move from one place to another where neither the start nor the stop should snap.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_EASE_IN_OUT_QUAD);
 */
#define JET_CURVE_EASE_IN_OUT_QUAD 3

/**
 * Constant: JET_CURVE_EASE_OUT_BACK
 * Summary: The curve number for jet_ease called "ease out back", kept for compatibility.
 * Include: jet_ease.h
 * Notes: For now it gives the same motion as a cubic ease-out, starting quickly and gliding to a stop, and it does not overshoot. When you want a visible overshoot, use JET_CURVE_ELASTIC.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_EASE_OUT_BACK);
 */
#define JET_CURVE_EASE_OUT_BACK 4

/**
 * Constant: JET_CURVE_SINE
 * Summary: The curve number for jet_ease that speeds up and slows down smoothly, following a sine wave.
 * Include: jet_ease.h
 * Notes: A polished all-round curve, soft at both ends.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_SINE);
 */
#define JET_CURVE_SINE 5

/**
 * Constant: JET_CURVE_BOUNCE
 * Summary: The curve number for jet_ease that lands and bounces at the end, each bounce smaller than the last.
 * Include: jet_ease.h
 * Notes: Best for a playful landing or a hit. The answer stays between 0 and 1, but it runs backwards for a moment on each bounce.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_BOUNCE);
 */
#define JET_CURVE_BOUNCE 6

/**
 * Constant: JET_CURVE_ELASTIC
 * Summary: The curve number for jet_ease that arrives like a spring, overshooting and wobbling to rest.
 * Include: jet_ease.h
 * Notes: The answer can stray a little below 0 or above 1 near the ends. Clamp it, with jet_clampf, when you use it for a fade, a volume, or anything else that must stay in bounds.
 * Example:
 *   float value = jet_ease(progress, JET_CURVE_ELASTIC);
 */
#define JET_CURVE_ELASTIC 7

/**
 * Function: jet_ease
 * Summary: Turns a progress from 0 to 1 into an eased progress from 0 to 1, using a curve you pick by its JET_CURVE_ number.
 * Parameters:
 *   - t: How far along the move is, from 0 to 1. A value outside that range is held at the nearer end before the curve is applied.
 *   - curve: Which curve, as one of the JET_CURVE_ numbers.
 * Returns: The eased progress, from 0 to 1. JET_CURVE_ELASTIC can stray a little outside that range near its ends. A curve number the runtime does not know gives back the clamped t, as JET_CURVE_LINEAR does.
 * Include: jet_ease.h
 * Notes: This is the safe way in. It fills in b = 0, c = 1 and d = 1 itself, so it can never divide by zero whatever t you give it. Multiply the answer by your own change and add your own start to get a real value. The same numbers are what an animation stores for its scale, rotation and tint curves. JET_CURVE_EASE_OUT_BACK gives a cubic ease-out for now, not a true overshoot.
 * Example:
 *   float scale = jet_ease(progress, JET_CURVE_BOUNCE);
 */
float jet_ease(float t, uint8_t curve);

#endif // JET_EASE_H
