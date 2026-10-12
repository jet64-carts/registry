// runtime/jet_math.h
#ifndef JET_MATH_H
#define JET_MATH_H

// Small maths helpers for games. Keep a number inside a range, wrap it round,
// or move it toward a target one step at a time. Work with angles in radians.
// Add, scale and measure 2D and 3D vectors, where a vector is a pair (or a
// trio) of numbers that together make a position or a movement. And handle a
// playfield that wraps round at its edges, the way Asteroids does. Every
// helper lives in this header and hands its answer straight back as a value,
// so there is nothing to allocate, nothing to own and nothing to free. A
// normal cart gets all of it by including jet64.h.
//
// Complete vector-movement example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { SHIP_RADIUS = 8 };
//   static Vector2 ship = { 80.0f, 80.0f };
//   static Vector2 velocity = { 90.0f, 55.0f };
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       ship = jet_vector2_add(ship, jet_vector2_scale(velocity, dt));
//       ship = jet_vector2_wrap(ship, (float)JET_WIDTH, (float)JET_HEIGHT);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_circfill((int)ship.x, (int)ship.y, SHIP_RADIUS, 12);
//   }
//
//   void _end(void) {}

// The runtime's foundation header. Some modules (jet_ease.c, for example) reach jet_math.h on
// their own without jet64.h, so it includes jet_core.h itself rather than relying on the includer
// to have reached it first, the same discipline its sibling math-using headers (jet_gamepad.h,
// jet_spatial_hash.h) follow.
#include "jet_core.h" // IWYU pragma: keep (sandbox-safe runtime foundation)
#include <math.h>
#include <stdbool.h>

#ifndef JET_PI
/**
 * Constant: JET_PI
 * Summary: Pi, as a float: half a turn, or 180 degrees, measured in radians.
 * Include: jet64.h
 * Notes: Jet64's rotation and angle helpers work in radians, the unit where a full turn is 2 times pi instead of 360 degrees. To turn degrees into radians, multiply by JET_PI / 180.0f.
 * Example:
 *   fmt.angle = JET_PI; // turn it half way round, 180 degrees
 */
#define JET_PI 3.14159265358979323846f
#endif

#ifndef JET_TAU
/**
 * Constant: JET_TAU
 * Summary: One full turn in radians: 2 times pi, or 360 degrees.
 * Include: jet64.h
 * Notes: Use it to wrap an angle back into a single turn, as the example does, or as the period of anything that repeats once every revolution.
 * Example:
 *   angle = jet_wrapf(angle, JET_TAU);
 */
#define JET_TAU (2.0f * JET_PI) // a full turn in radians (2 * PI)
#endif

#ifndef JET_HALF_PI
/**
 * Constant: JET_HALF_PI
 * Summary: A quarter turn in radians: pi divided by 2, or 90 degrees.
 * Include: jet64.h
 * Notes: Give it to SpriteFormat.angle, or to a vector rotation helper such as jet_vector2_rotate, to turn something by exactly 90 degrees.
 * Example:
 *   fmt.angle = JET_HALF_PI;
 */
#define JET_HALF_PI (0.5f * JET_PI) // a quarter turn in radians (PI / 2)
#endif

// --- SINGLE NUMBERS ---

/**
 * Function: jet_clampf
 * Summary: Keeps a number inside a range, from lo to hi, with both ends allowed.
 * Parameters:
 *   - v: The number to hold in range.
 *   - lo: The lowest value allowed. You get lo back when v is below it.
 *   - hi: The highest value allowed. You get hi back when v is above it.
 * Returns: v unchanged when it already lies between lo and hi; otherwise whichever end it went past.
 * Include: jet_math.h
 * Notes: Give it lo and hi the right way round. It assumes lo <= hi and does not check. It is two comparisons in the header, so it costs almost nothing and is safe to call many times a tick.
 * Example:
 *   health = jet_clampf(health + regen, 0.0f, max_health);
 */
static inline float jet_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * Function: jet_wrapf
 * Summary: Wraps a number round so it lands between 0 and span, with span itself left out, and gets a negative number right too.
 * Parameters:
 *   - v: The number to wrap. It may be negative.
 *   - span: How wide the range is. Must be greater than zero.
 * Returns: A number from 0 up to, but not including, span. It differs from v by a whole number of spans.
 * Include: jet_math.h
 * Notes: C's own fmodf gives a negative answer for a negative input. This helper always gives 0 or more, which is what an angle that has turned backwards, a scrolling offset, or a position on a wrap-around playfield needs. A span of zero divides by zero and gives NaN, the float value that means "not a number", and a negative span gives a wrong answer without any warning, so always pass a span above zero.
 * Example:
 *   angle = jet_wrapf(angle + spin * dt, JET_TAU);
 */
static inline float jet_wrapf(float v, float span) {
    v = fmodf(v, span);
    if (v < 0.0f) v += span;
    if (v >= span) v = 0.0f; // a tiny-negative v + span can round UP to exactly span in
                             // float32; clamp so the result honours the half-open [0, span)
    return v;
}

/**
 * Function: jet_angle_delta
 * Summary: Tells you the shortest turn from one angle to another, and which way round to make it.
 * Parameters:
 *   - current: The angle you are facing now, in radians.
 *   - target: The angle you want to face, in radians.
 * Returns: target - current, folded so it lies between -JET_PI and JET_PI, with JET_PI itself left out. The sign says which way to turn. An exact half turn comes back as -JET_PI.
 * Include: jet_math.h
 * Notes: Without it, turning from 350 degrees to 10 degrees would go the long way round the circle; with it the turn is the short 20 degrees. Give the answer to jet_approachf to turn toward a heading at no more than a set speed.
 * Example:
 *   float steer = jet_angle_delta(ship_angle, desired_angle);
 */
static inline float jet_angle_delta(float current, float target) {
    float d = fmodf(target - current + JET_PI, JET_TAU);
    if (d < 0.0f) d += JET_TAU;
    return d - JET_PI;
}

/**
 * Function: jet_approachf
 * Summary: Moves a value toward a target by at most one step, so it never overshoots.
 * Parameters:
 *   - current: The value as it is now.
 *   - target: The value you want to reach.
 *   - max_step: The most the value may change in this one call, in either direction. Give it zero or more.
 * Returns: current moved toward target by up to max_step, or target itself once it is within one step.
 * Include: jet_math.h
 * Notes: Call it once a tick with max_step = rate * dt, as the example does, and the value ramps at the same speed on every machine. It works on anything that changes in a straight line: a position, a volume, a speed. For an angle, combine it with jet_angle_delta so the turn goes the short way round.
 * Example:
 *   volume = jet_approachf(volume, target_volume, fade_rate * dt);
 */
static inline float jet_approachf(float current, float target, float max_step) {
    float d = target - current;
    if (d > max_step) return current + max_step;
    if (d < -max_step) return current - max_step;
    return target;
}

// --- 2D VECTORS ---

/**
 * Struct: Vector2
 * Summary: A pair of numbers, an x and a y, that together make a position or a movement. The runtime uses it for positions, velocities and directions.
 * Fields:
 *   - x: The across part, as a float. On the screen, x grows to the right.
 *   - y: The down part, as a float. On the screen, y grows downward.
 * Include: jet_math.h
 * Notes: It is plain data: nothing to own, nothing to free. Make one with jet_make_vector2 or with braces, as the example does, and pass it by value, so each call gets its own copy.
 * Example:
 *   Vector2 velocity = { 120.0f, -40.0f };
 */
typedef struct {
    float x;
    float y;
} Vector2;

/**
 * Function: jet_make_vector2
 * Summary: Makes a Vector2 from an x and a y.
 * Parameters:
 *   - x: The across part.
 *   - y: The down part.
 * Returns: A Vector2 holding those two numbers, handed back by value.
 * Include: jet_math.h
 * Notes: The vector owns nothing and needs no cleanup. Keep it, copy it, or pass it on.
 * Example:
 *   Vector2 pos = jet_make_vector2(100.0f, 50.0f);
 */
static inline Vector2 jet_make_vector2(float x, float y) {
    return (Vector2){ x, y };
}

/**
 * Function: jet_vector2_add
 * Summary: Adds two vectors: the x parts together and the y parts together.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: { v1.x + v2.x, v1.y + v2.y }.
 * Include: jet_math.h
 * Notes: Adding a velocity to a position moves the position by that velocity, which is the everyday use and what the example does. It lives in the header and is inlined, so it costs no more than writing the two additions yourself.
 * Example:
 *   position = jet_vector2_add(position, velocity);
 */
static inline Vector2 jet_vector2_add(Vector2 v1, Vector2 v2) {
    return (Vector2){ v1.x + v2.x, v1.y + v2.y };
}

/**
 * Function: jet_vector2_subtract
 * Summary: Takes one vector away from another: the x parts and the y parts separately.
 * Parameters:
 *   - v1: The vector to take from.
 *   - v2: The vector to take away.
 * Returns: { v1.x - v2.x, v1.y - v2.y }.
 * Include: jet_math.h
 * Notes: Take your position away from a target's position and you get the movement that would carry you to the target, as the example does. That is the usual way to find where something is relative to you, or which way to steer.
 * Example:
 *   Vector2 to_target = jet_vector2_subtract(target, position);
 */
static inline Vector2 jet_vector2_subtract(Vector2 v1, Vector2 v2) {
    return (Vector2){ v1.x - v2.x, v1.y - v2.y };
}

/**
 * Function: jet_vector2_scale
 * Summary: Multiplies both parts of a vector by one number, making it longer or shorter without changing its direction.
 * Parameters:
 *   - v: The vector to stretch or shrink.
 *   - scale: The number to multiply x and y by. 2.0f doubles the length and 0.5f halves it.
 * Returns: { v.x * scale, v.y * scale }.
 * Include: jet_math.h
 * Notes: The usual job is scaling a velocity by dt, the time one tick covers, to get how far to move this tick, as the example does. It also turns a direction of length 1 into a movement of any length, and converts units, such as tiles to pixels.
 * Example:
 *   position = jet_vector2_add(position, jet_vector2_scale(velocity, dt));
 */
static inline Vector2 jet_vector2_scale(Vector2 v, float scale) {
    return (Vector2){ v.x * scale, v.y * scale };
}

/**
 * Function: jet_vector2_multiply
 * Summary: Multiplies two vectors part by part: x times x, and y times y.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: { v1.x * v2.x, v1.y * v2.y }.
 * Include: jet_math.h
 * Notes: Use it when each axis needs its own multiplier: to stretch a shape more one way than the other, to zero one axis by multiplying it by 0, or to turn a tile position into pixels, as the example does. This is not the dot product; for that, use jet_vector2_dot_product.
 * Example:
 *   Vector2 world = jet_vector2_multiply(tile, jet_make_vector2(16.0f, 16.0f));
 */
static inline Vector2 jet_vector2_multiply(Vector2 v1, Vector2 v2) {
    return (Vector2){ v1.x * v2.x, v1.y * v2.y };
}

/**
 * Function: jet_vector2_length
 * Summary: Measures how long a vector is: the straight-line distance from (0, 0) to (x, y).
 * Parameters:
 *   - v: The vector to measure.
 * Returns: The length of v. For a velocity, that is its speed, as the example shows.
 * Include: jet_math.h
 * Notes: It takes a square root, which is slow next to plain arithmetic. When you only want to know whether something is within a certain distance, and you ask many times a tick, skip the square root: compare jet_vector2_distance_sq against your distance squared.
 * Example:
 *   float speed = jet_vector2_length(velocity);
 */
static inline float jet_vector2_length(Vector2 v) {
    return sqrtf((v.x * v.x) + (v.y * v.y));
}

/**
 * Function: jet_vector2_distance
 * Summary: Measures the straight-line distance between two points.
 * Parameters:
 *   - v1: The first point.
 *   - v2: The second point.
 * Returns: How far apart the two points are.
 * Include: jet_math.h
 * Notes: It takes a square root. For a plain "is it within range" test that runs many times a tick, jet_vector2_distance_sq skips the square root: compare its answer against your range squared.
 * Example:
 *   if (jet_vector2_distance(player, coin) < 12.0f) collect_coin();
 */
static inline float jet_vector2_distance(Vector2 v1, Vector2 v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return sqrtf((dx * dx) + (dy * dy));
}

/**
 * Function: jet_vector2_dot_product
 * Summary: Multiplies two vectors into one number, the dot product, which tells you how much they point the same way.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: v1.x * v2.x + v1.y * v2.y. It is positive when the two point roughly the same way, zero when they are at right angles, and negative when they point roughly opposite ways.
 * Include: jet_math.h
 * Notes: The example asks whether a target is in front: the dot of your facing direction and the direction to the target is positive when it is. The same number finds how far one vector reaches along another, works out how brightly a light falls on a surface, and, for two vectors of length 1, is the cosine of the angle between them.
 * Example:
 *   float facing = jet_vector2_dot_product(forward, to_target);
 */
static inline float jet_vector2_dot_product(Vector2 v1, Vector2 v2) {
    return (v1.x * v2.x) + (v1.y * v2.y);
}

/**
 * Function: jet_vector2_normalize
 * Summary: Shrinks or stretches a vector to length 1, keeping its direction, so it becomes a pure direction with no size.
 * Parameters:
 *   - v: The vector to make length 1.
 * Returns: A vector of length 1 pointing the same way as v, or { 0, 0 } when v is { 0, 0 } and has no direction to keep.
 * Include: jet_math.h
 * Notes: A vector of length zero is caught before it can cause a divide by zero. The call measures the length with jet_vector2_length, so it costs a square root. The example turns "where the target is from here" into "which way to go"; scale that with jet_vector2_scale to get a velocity.
 * Example:
 *   Vector2 direction = jet_vector2_normalize(jet_vector2_subtract(target, position));
 */
static inline Vector2 jet_vector2_normalize(Vector2 v) {
    Vector2 result = { 0 };
    float length = jet_vector2_length(v);
    if (length > 0.0f) {
        float ilength = 1.0f / length;
        result.x = v.x * ilength;
        result.y = v.y * ilength;
    }
    return result;
}

/**
 * Function: jet_vector2_lerp
 * Summary: Finds the point part of the way from one vector to another, in a straight line.
 * Parameters:
 *   - v1: Where to start. An amount of 0 gives this back.
 *   - v2: Where to end. An amount of 1 gives this back.
 *   - amount: How far along to go, from 0 to 1. 0.5f is half way.
 * Returns: The point that fraction of the way from v1 to v2.
 * Include: jet_math.h
 * Notes: amount is not held to 0 to 1. A value below 0 lands before v1 and a value above 1 lands past v2, on the same line. Clamp it first, with jet_clampf, when it must stay between the two. Calling it every tick with a small fixed amount, as the example does with 0.15f, makes a camera that chases its target and slows as it gets close.
 * Example:
 *   camera = jet_vector2_lerp(camera, target, 0.15f);
 */
static inline Vector2 jet_vector2_lerp(Vector2 v1, Vector2 v2, float amount) {
    Vector2 result;
    result.x = v1.x + amount * (v2.x - v1.x);
    result.y = v1.y + amount * (v2.y - v1.y);
    return result;
}

/**
 * Function: jet_vector2_rotate
 * Summary: Turns a vector about (0, 0) by an angle, keeping its length.
 * Parameters:
 *   - v: The vector to turn.
 *   - angle: How far to turn it, in radians. A positive angle turns anticlockwise on a graph where y grows upward; on the screen, where y grows downward, the same turn looks clockwise.
 * Returns: The turned vector.
 * Include: jet_math.h
 * Notes: It calls sinf and cosf, which are slow next to plain arithmetic. To turn many points by the same angle, turn one vector and keep the answer, or work out cosf(angle) and sinf(angle) once and reuse them.
 * Example:
 *   Vector2 muzzle = jet_vector2_rotate(jet_make_vector2(12.0f, 0.0f), aim_angle);
 */
static inline Vector2 jet_vector2_rotate(Vector2 v, float angle) {
    Vector2 result;
    float cosres = cosf(angle);
    float sinres = sinf(angle);
    result.x = v.x * cosres - v.y * sinres;
    result.y = v.x * sinres + v.y * cosres;
    return result;
}

/**
 * Function: jet_vector2_angle
 * Summary: Tells you how far you would have to turn the first vector to point it along the second.
 * Parameters:
 *   - v1: The vector you start from.
 *   - v2: The vector you want to point along.
 * Returns: The angle from v1 to v2 in radians, worked out as atan2f(cross, dot). The sign says which way round to turn.
 * Include: jet_math.h
 * Notes: The sign comes from the 2D cross product of the two vectors, which is positive for one direction of turn and negative for the other. Make both vectors length 1 with jet_vector2_normalize first when you want to compare directions alone.
 * Example:
 *   float turn = jet_vector2_angle(forward, desired_direction);
 */
static inline float jet_vector2_angle(Vector2 v1, Vector2 v2) {
    return atan2f(v1.x * v2.y - v1.y * v2.x, v1.x * v2.x + v1.y * v2.y);
}

/**
 * Function: jet_vector2_from_angle
 * Summary: Makes a vector of a given length that points in a given direction, the way a ship's heading and speed make its velocity.
 * Parameters:
 *   - angle: The direction, in radians. 0 points right, along +x, and a bigger angle turns clockwise on the screen, because screen y grows downward.
 *   - len: How long the vector should be.
 * Returns: { cosf(angle) * len, sinf(angle) * len }.
 * Include: jet_math.h
 * Notes: It calls sinf and cosf. The clockwise convention is the screen's own, where y grows downward, and it is the same one jet_vector2_rotate uses, so the two agree.
 * Example:
 *   Vector2 muzzle_vel = jet_vector2_from_angle(ship_angle, BULLET_SPEED);
 */
static inline Vector2 jet_vector2_from_angle(float angle, float len) {
    return (Vector2){ cosf(angle) * len, sinf(angle) * len };
}

/**
 * Function: jet_vector2_distance_sq
 * Summary: Measures the distance between two points, squared, which is cheaper than the distance itself.
 * Parameters:
 *   - v1: The first point.
 *   - v2: The second point.
 * Returns: The distance from v1 to v2, multiplied by itself.
 * Include: jet_math.h
 * Notes: It skips the square root that jet_vector2_distance takes. To ask "is b within r of a", compare the answer against r * r, as the example does; this is the cheapest range test there is. Use jet_vector2_distance only when you need the distance itself.
 * Example:
 *   if (jet_vector2_distance_sq(a, b) < r * r) on_contact();
 */
static inline float jet_vector2_distance_sq(Vector2 v1, Vector2 v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return (dx * dx) + (dy * dy);
}

// --- WRAP-AROUND PLAYFIELD ---
// Helpers for a playfield like Asteroids, where a ship that flies off one edge
// of the screen comes back in from the opposite edge. Mathematicians call that
// shape a torus. Each helper takes the field's width and height (w, h), so it
// works for a field of any size, not only the screen.

/**
 * Function: jet_vector2_wrap
 * Summary: Brings a position that has left a wrap-around playfield back in from the opposite edge.
 * Parameters:
 *   - p: The position to wrap.
 *   - w: The field's width. x comes back between 0 and w, with w itself left out.
 *   - h: The field's height. y comes back between 0 and h, with h itself left out.
 * Returns: The same position folded back onto the field, each axis in its range.
 * Include: jet_math.h
 * Notes: Each axis goes through jet_wrapf, so a position that has drifted below zero comes back in from the far edge correctly. Call it right after you add the velocity each tick, as the example does, and nothing can leave the field.
 * Example:
 *   ship.pos = jet_vector2_wrap(jet_vector2_add(ship.pos, step), (float)JET_WIDTH, (float)JET_HEIGHT);
 */
static inline Vector2 jet_vector2_wrap(Vector2 p, float w, float h) {
    return (Vector2){ jet_wrapf(p.x, w), jet_wrapf(p.y, h) };
}

/**
 * Function: jet_vector2_wrap_delta
 * Summary: Finds the shortest movement from one point to another on a wrap-around playfield, going through an edge when that is shorter.
 * Parameters:
 *   - a: Where you start.
 *   - b: Where you want to get to.
 *   - w: The field's width, the distance at which x wraps.
 *   - h: The field's height, the distance at which y wraps.
 * Returns: b - a, but on each axis taken the short way, through the nearer edge if need be, so x lies between -w/2 and w/2 and y between -h/2 and h/2, both ends allowed.
 * Include: jet_math.h
 * Notes: This is the one helper every other wrap-around test rests on. Two objects that sit either side of an edge, one at the far left and one at the far right, are close neighbours on such a field, and this call says so. The length of the answer is their true distance apart, and its direction is the line between them, which is just what a collision needs to push them apart. jet_circles_overlap_wrapped is built on it.
 * Example:
 *   Vector2 normal = jet_vector2_wrap_delta(rock_a.pos, rock_b.pos, (float)JET_WIDTH, (float)JET_HEIGHT);
 */
static inline Vector2 jet_vector2_wrap_delta(Vector2 a, Vector2 b, float w, float h) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    if (dx > w * 0.5f)       dx -= w;
    else if (dx < -w * 0.5f) dx += w;
    if (dy > h * 0.5f)       dy -= h;
    else if (dy < -h * 0.5f) dy += h;
    return (Vector2){ dx, dy };
}

/**
 * Function: jet_circles_overlap_wrapped
 * Summary: Tells you whether two circles overlap on a wrap-around playfield, counting the case where they meet through an edge.
 * Parameters:
 *   - a: The centre of the first circle.
 *   - ra: The radius of the first circle.
 *   - b: The centre of the second circle.
 *   - rb: The radius of the second circle.
 *   - w: The field's width, the distance at which x wraps.
 *   - h: The field's height, the distance at which y wraps.
 * Returns: true when the circles overlap, whether directly or by reaching through an edge; otherwise false.
 * Include: jet_math.h
 * Notes: It measures the gap with jet_vector2_wrap_delta, the short way round, so a rock sitting on the right edge still hits a bullet coming in from the left edge. It compares squared distances and takes no square root, so it is cheap.
 * Example:
 *   if (jet_circles_overlap_wrapped(bullet.pos, 3.0f, rock.pos, rock.radius, (float)JET_WIDTH, (float)JET_HEIGHT)) hit();
 */
static inline bool jet_circles_overlap_wrapped(
    Vector2 a, float ra, Vector2 b, float rb, float w, float h) {
    Vector2 d = jet_vector2_wrap_delta(a, b, w, h);
    float r = ra + rb;
    return (d.x * d.x + d.y * d.y) <= (r * r);
}

// --- 3D VECTORS ---

/**
 * Struct: Vector3
 * Summary: Three numbers, an x, a y and a z, that together make a position or a movement in three dimensions.
 * Fields:
 *   - x: The first part, as a float.
 *   - y: The second part, as a float.
 *   - z: The third part, as a float.
 * Include: jet_math.h
 * Notes: It is plain data: nothing to own, nothing to free. Make one with jet_make_vector3 or with braces, as the example does, and pass it by value.
 * Example:
 *   Vector3 light_direction = { 0.0f, -1.0f, 0.0f };
 */
typedef struct {
    float x;
    float y;
    float z;
} Vector3;

/**
 * Function: jet_make_vector3
 * Summary: Makes a Vector3 from an x, a y and a z.
 * Parameters:
 *   - x: The first part.
 *   - y: The second part.
 *   - z: The third part.
 * Returns: A Vector3 holding those three numbers, handed back by value.
 * Include: jet_math.h
 * Notes: The vector owns nothing and needs no cleanup. Keep it, copy it, or pass it on.
 * Example:
 *   Vector3 pos = jet_make_vector3(100.0f, 50.0f, 0.0f);
 */
static inline Vector3 jet_make_vector3(float x, float y, float z) {
    return (Vector3){ x, y, z };
}

/**
 * Function: jet_vector3_add
 * Summary: Adds two 3D vectors: the x parts together, the y parts together and the z parts together.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z }.
 * Include: jet_math.h
 * Notes: Plain arithmetic on the three parts. The vectors you passed in are left as they were.
 * Example:
 *   Vector3 result = jet_vector3_add(v1, v2);
 */
static inline Vector3 jet_vector3_add(Vector3 v1, Vector3 v2) {
    return (Vector3){ v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}

/**
 * Function: jet_vector3_subtract
 * Summary: Takes one 3D vector away from another, part by part.
 * Parameters:
 *   - v1: The vector to take from.
 *   - v2: The vector to take away.
 * Returns: { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z }.
 * Include: jet_math.h
 * Notes: Plain arithmetic on the three parts. The vectors you passed in are left as they were.
 * Example:
 *   Vector3 result = jet_vector3_subtract(v1, v2);
 */
static inline Vector3 jet_vector3_subtract(Vector3 v1, Vector3 v2) {
    return (Vector3){ v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

/**
 * Function: jet_vector3_scale
 * Summary: Multiplies all three parts of a 3D vector by one number, making it longer or shorter without changing its direction.
 * Parameters:
 *   - v: The vector to stretch or shrink.
 *   - scale: The number to multiply x, y and z by.
 * Returns: { v.x * scale, v.y * scale, v.z * scale }.
 * Include: jet_math.h
 * Notes: Plain arithmetic. The vector you passed in is left as it was.
 * Example:
 *   Vector3 result = jet_vector3_scale(v, 2.0f);
 */
static inline Vector3 jet_vector3_scale(Vector3 v, float scale) {
    return (Vector3){ v.x * scale, v.y * scale, v.z * scale };
}

/**
 * Function: jet_vector3_multiply
 * Summary: Multiplies two 3D vectors part by part: x times x, y times y, and z times z.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: { v1.x * v2.x, v1.y * v2.y, v1.z * v2.z }.
 * Include: jet_math.h
 * Notes: Use it when each axis needs its own multiplier. It is not the dot product and not the cross product; those are jet_vector3_dot_product and jet_vector3_cross_product.
 * Example:
 *   Vector3 result = jet_vector3_multiply(v1, v2);
 */
static inline Vector3 jet_vector3_multiply(Vector3 v1, Vector3 v2) {
    return (Vector3){ v1.x * v2.x, v1.y * v2.y, v1.z * v2.z };
}

/**
 * Function: jet_vector3_length
 * Summary: Measures how long a 3D vector is: the straight-line distance from (0, 0, 0) to (x, y, z).
 * Parameters:
 *   - v: The vector to measure.
 * Returns: The length of v.
 * Include: jet_math.h
 * Notes: It takes a square root. When you only need to know which of two vectors is longer, or whether something lies within a radius, compare squared lengths instead and skip the square root.
 * Example:
 *   float dist = jet_vector3_length(v);
 */
static inline float jet_vector3_length(Vector3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/**
 * Function: jet_vector3_distance
 * Summary: Measures the straight-line distance between two points in three dimensions.
 * Parameters:
 *   - v1: The first point.
 *   - v2: The second point.
 * Returns: How far apart the two points are, or 0 when they are the same point.
 * Include: jet_math.h
 * Notes: It takes a square root.
 * Example:
 *   float dist = jet_vector3_distance(a, b);
 */
static inline float jet_vector3_distance(Vector3 v1, Vector3 v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    float dz = v1.z - v2.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * Function: jet_vector3_dot_product
 * Summary: Multiplies two 3D vectors into one number, the dot product, which tells you how much they point the same way.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: v1.x * v2.x + v1.y * v2.y + v1.z * v2.z.
 * Include: jet_math.h
 * Notes: When both vectors have length 1 (see jet_vector3_normalize), the answer is the cosine of the angle between them: 1 when they point the same way, 0 when they are at right angles, and -1 when they point opposite ways.
 * Example:
 *   float dot = jet_vector3_dot_product(n1, n2);
 */
static inline float jet_vector3_dot_product(Vector3 v1, Vector3 v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

/**
 * Function: jet_vector3_cross_product
 * Summary: Makes a new 3D vector that stands at right angles to both of the vectors you give it: the cross product.
 * Parameters:
 *   - v1: The first vector.
 *   - v2: The second vector.
 * Returns: The cross product of v1 and v2, at right angles to both.
 * Include: jet_math.h
 * Notes: The order matters. Swap v1 and v2 and you get the same vector pointing the opposite way. The example finds a surface's normal, the direction that points straight out of it, from two directions that lie along it.
 * Example:
 *   Vector3 normal = jet_vector3_cross_product(v1, v2);
 */
static inline Vector3 jet_vector3_cross_product(Vector3 v1, Vector3 v2) {
    return (Vector3){
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    };
}

/**
 * Function: jet_vector3_normalize
 * Summary: Shrinks or stretches a 3D vector to length 1, keeping its direction.
 * Parameters:
 *   - v: The vector to make length 1.
 * Returns: A vector of length 1 pointing the same way as v.
 * Include: jet_math.h
 * Notes: A vector of length zero has no direction to keep. It comes back as {0, 0, 0} rather than causing a divide by zero.
 * Example:
 *   Vector3 n = jet_vector3_normalize(dir);
 */
static inline Vector3 jet_vector3_normalize(Vector3 v) {
    Vector3 result = { 0 };
    float length = jet_vector3_length(v);
    if (length > 0.0f) {
        float ilength = 1.0f / length;
        result.x = v.x * ilength;
        result.y = v.y * ilength;
        result.z = v.z * ilength;
    }
    return result;
}

/**
 * Function: jet_vector3_lerp
 * Summary: Finds the point part of the way from one 3D vector to another, in a straight line.
 * Parameters:
 *   - v1: Where to start. An amount of 0 gives this back.
 *   - v2: Where to end. An amount of 1 gives this back.
 *   - amount: How far along to go, from 0.0 to 1.0. 0.5f is half way, as in the example.
 * Returns: The point that fraction of the way from v1 to v2.
 * Include: jet_math.h
 * Notes: amount is not held to 0 to 1. A value below 0 lands before v1 and a value above 1 lands past v2, on the same line.
 * Example:
 *   Vector3 result = jet_vector3_lerp(a, b, 0.5f);
 */
static inline Vector3 jet_vector3_lerp(Vector3 v1, Vector3 v2, float amount) {
    Vector3 result;
    result.x = v1.x + amount * (v2.x - v1.x);
    result.y = v1.y + amount * (v2.y - v1.y);
    result.z = v1.z + amount * (v2.z - v1.z);
    return result;
}

// The older raylib-style names, kept so carts that still use them build.
#define Vector2Add jet_vector2_add
#define Vector2Subtract jet_vector2_subtract
#define Vector2Scale jet_vector2_scale
#define Vector2Multiply jet_vector2_multiply
#define Vector2Length jet_vector2_length
#define Vector2Distance jet_vector2_distance
#define Vector2DotProduct jet_vector2_dot_product
#define Vector2Normalize jet_vector2_normalize
#define Vector2Lerp jet_vector2_lerp
#define Vector2Rotate jet_vector2_rotate
#define Vector2Angle jet_vector2_angle

#define Vector3Add jet_vector3_add
#define Vector3Subtract jet_vector3_subtract
#define Vector3Scale jet_vector3_scale
#define Vector3Multiply jet_vector3_multiply
#define Vector3Length jet_vector3_length
#define Vector3Distance jet_vector3_distance
#define Vector3DotProduct jet_vector3_dot_product
#define Vector3CrossProduct jet_vector3_cross_product
#define Vector3Normalize jet_vector3_normalize
#define Vector3Lerp jet_vector3_lerp

#endif // JET_MATH_H
