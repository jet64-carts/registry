// runtime/jet_collision.h
#ifndef JET_COLLISION_H
#define JET_COLLISION_H

#include <stdbool.h>
#include <stdint.h>
#include "jet_graphics.h"

// Find out whether things in your game are touching, with no memory set aside
// for it. The sprite tests are pixel-perfect: they compare the masks you
// painted in the Sprite Editor, so two sprites count as touching only where a
// solid pixel of one sits on a solid pixel of the other. The shape tests are
// the everyday ones: a point in a rectangle, a point in a circle, two
// rectangles, two circles, a circle and a rectangle, and a capsule against a
// circle. A capsule is a line with a radius around it, the shape a moving
// circle sweeps out. Positions and sizes are in pixels.
//
// Complete circle-collision example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { PLAYER_RADIUS = 12, TARGET_RADIUS = 24 };
//   static float player_x = 80.0f;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       const float speed = 160.0f;
//       if (btn(BTN_LEFT))  player_x -= speed * dt;
//       if (btn(BTN_RIGHT)) player_x += speed * dt;
//   }
//
//   void _draw(void) {
//       const int target_x = JET_WIDTH / 2;
//       const int centre_y = JET_HEIGHT / 2;
//       const bool hit = jet_circ_overlap(
//           player_x, (float)centre_y, PLAYER_RADIUS,
//           (float)target_x, (float)centre_y, TARGET_RADIUS
//       );
//       jet_cls(0);
//       jet_circfill((int)player_x, centre_y, PLAYER_RADIUS, hit ? 8 : 12);
//       jet_circ(target_x, centre_y, TARGET_RADIUS, 15);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_collide
 * Summary: Tells you whether two plain sprites, neither scaled nor turned, overlap pixel for pixel.
 * Parameters:
 *   - n1: Which sprite the first one is, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x1: The first sprite's left edge, in world pixels, the same position you give jet_spr.
 *   - y1: The first sprite's top edge, in world pixels.
 *   - w1: The first sprite's width in 8x8 tiles.
 *   - h1: The first sprite's height in 8x8 tiles.
 *   - flip_x1: true if the first sprite is mirrored left to right.
 *   - flip_y1: true if the first sprite is mirrored top to bottom.
 *   - n2: Which sprite the second one is, counted in 8x8 tiles across the sprite sheet.
 *   - x2: The second sprite's left edge, in world pixels.
 *   - y2: The second sprite's top edge, in world pixels.
 *   - w2: The second sprite's width in 8x8 tiles.
 *   - h2: The second sprite's height in 8x8 tiles.
 *   - flip_x2: true if the second sprite is mirrored left to right.
 *   - flip_y2: true if the second sprite is mirrored top to bottom.
 * Returns: true when a solid pixel of one sits on a solid pixel of the other, judged by their masks; otherwise false.
 * Include: jet_collision.h
 * Notes: This is the quickest exact test for sprites that are not scaled or turned. Each sprite is placed by its top-left corner, just as jet_spr places it, so give it the same positions and flips you draw with. If either sprite is drawn through a SpriteFormat, use jet_collide_ext instead.
 * Example:
 *   bool hit = jet_collide(player_hw, px, py, 2, 2, false, false,
 *                           enemy_hw, ex, ey, 2, 2, enemy_flip, false);
 */
bool jet_collide(
    uint32_t n1, int x1, int y1, int w1, int h1, bool flip_x1, bool flip_y1,
    uint32_t n2, int x2, int y2, int w2, int h2, bool flip_x2, bool flip_y2
);

/**
 * Function: jet_collide_ext
 * Summary: Tells you whether two sprites overlap pixel for pixel, when either may be scaled, turned, mirrored or masked through a SpriteFormat.
 * Parameters:
 *   - n1: Which sprite the first one is, counted in 8x8 tiles across the sprite sheet. The sheet is 1024 pixels wide, so tile 128 is the first tile of the second row.
 *   - x1: The first sprite's centre, across, in pixels, when fmt1 is given; its left edge when fmt1 is NULL.
 *   - y1: The first sprite's centre, down, in pixels, when fmt1 is given; its top edge when fmt1 is NULL.
 *   - w1: The first sprite's width in 8x8 tiles.
 *   - h1: The first sprite's height in 8x8 tiles.
 *   - fmt1: How the first sprite is drawn, or NULL for a plain sprite, which takes the quicker untransformed path.
 *   - n2: Which sprite the second one is, counted in 8x8 tiles across the sprite sheet.
 *   - x2: The second sprite's centre, across, in pixels, when fmt2 is given; its left edge when fmt2 is NULL.
 *   - y2: The second sprite's centre, down, in pixels, when fmt2 is given; its top edge when fmt2 is NULL.
 *   - w2: The second sprite's width in 8x8 tiles.
 *   - h2: The second sprite's height in 8x8 tiles.
 *   - fmt2: How the second sprite is drawn, or NULL for a plain sprite, which takes the quicker untransformed path.
 * Returns: true when a visible pixel of one, after its transform, sits on a visible pixel of the other; otherwise false.
 * Include: jet_collision.h
 * Notes: The test maps pixels exactly as spr_ext draws them, so scale, rotation, flips and use_mask all count the way they look on screen. Give it the same positions and formats you draw with, so the picture and the hit test agree. A SpriteFormat, even one with a scale of 1 and an angle of 0, places its sprite by its centre rather than by its top-left corner. For plain sprites placed by their top-left corner, jet_collide is the better call.
 * Example:
 *   SpriteFormat spin = { .scale_x = 1.0f, .scale_y = 1.0f, .angle = angle,
 *                         .alpha = 1.0f, .flip_x = false, .flip_y = false, .use_mask = true };
 *   bool hit = jet_collide_ext(ship_hw, sx, sy, 2, 2, &spin,
 *                               rock_hw, rx, ry, 2, 2, NULL);
 */
bool jet_collide_ext(
    uint32_t n1, int x1, int y1, int w1, int h1, const SpriteFormat* fmt1,
    uint32_t n2, int x2, int y2, int w2, int h2, const SpriteFormat* fmt2
);

// --- COLLISION SHAPES ---

/**
 * Function: jet_point_in_rect
 * Summary: Tells you whether a point is inside a rectangle.
 * Parameters:
 *   - px: The point, across.
 *   - py: The point, down.
 *   - rx: The rectangle's left edge.
 *   - ry: The rectangle's top edge.
 *   - rw: The rectangle's width.
 *   - rh: The rectangle's height.
 * Returns: true when the point is inside the rectangle or exactly on one of its edges; otherwise false.
 * Include: jet_collision.h
 * Notes: A point on an edge counts as inside. The rectangle is upright, not turned. Give it a width and height of zero or more; with a negative size the answer means nothing.
 * Example:
 *   bool over_button = jet_point_in_rect(mouse_x, mouse_y, 10, 10, 50, 50);
 */
static inline bool jet_point_in_rect(float px, float py, float rx, float ry, float rw, float rh) {
    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

/**
 * Function: jet_point_in_circ
 * Summary: Tells you whether a point is inside a circle.
 * Parameters:
 *   - px: The point, across.
 *   - py: The point, down.
 *   - cx: The circle's centre, across.
 *   - cy: The circle's centre, down.
 *   - cr: The circle's radius.
 * Returns: true when the point is inside the circle or exactly on its edge; otherwise false.
 * Include: jet_collision.h
 * Notes: It compares squared distances, so it never needs a square root and is quick. Give it a radius of zero or more.
 * Example:
 *   bool in_range = jet_point_in_circ(player_x, player_y, 100, 100, 30);
 */
static inline bool jet_point_in_circ(float px, float py, float cx, float cy, float cr) {
    float dx = px - cx;
    float dy = py - cy;
    return (dx * dx + dy * dy) <= (cr * cr);
}

/**
 * Function: jet_rect_overlap
 * Summary: Tells you whether two rectangles overlap.
 * Parameters:
 *   - rx1: The first rectangle's left edge.
 *   - ry1: The first rectangle's top edge.
 *   - rw1: The first rectangle's width.
 *   - rh1: The first rectangle's height.
 *   - rx2: The second rectangle's left edge.
 *   - ry2: The second rectangle's top edge.
 *   - rw2: The second rectangle's width.
 *   - rh2: The second rectangle's height.
 * Returns: true when the rectangles share some area; otherwise false. Two rectangles that only touch along an edge, with no area in common, do not count as overlapping.
 * Include: jet_collision.h
 * Notes: Both rectangles are upright, not turned. Give them widths and heights of zero or more.
 * Example:
 *   bool hit = jet_rect_overlap(px, py, 16, 16, ex, ey, 32, 32);
 */
static inline bool jet_rect_overlap(float rx1, float ry1, float rw1, float rh1, float rx2, float ry2, float rw2, float rh2) {
    return rx1 < rx2 + rw2 && rx1 + rw1 > rx2 && ry1 < ry2 + rh2 && ry1 + rh1 > ry2;
}

/**
 * Function: jet_circ_overlap
 * Summary: Tells you whether two circles overlap.
 * Parameters:
 *   - cx1: The first circle's centre, across.
 *   - cy1: The first circle's centre, down.
 *   - cr1: The first circle's radius.
 *   - cx2: The second circle's centre, across.
 *   - cy2: The second circle's centre, down.
 *   - cr2: The second circle's radius.
 * Returns: true when the circles overlap or exactly touch; otherwise false.
 * Include: jet_collision.h
 * Notes: It compares squared distances, so it never needs a square root. Give it radii of zero or more.
 * Example:
 *   bool hit = jet_circ_overlap(px, py, 10, ex, ey, 15);
 */
static inline bool jet_circ_overlap(float cx1, float cy1, float cr1, float cx2, float cy2, float cr2) {
    float dx = cx1 - cx2;
    float dy = cy1 - cy2;
    float rad = cr1 + cr2;
    return (dx * dx + dy * dy) <= (rad * rad);
}

/**
 * Function: jet_circ_rect_overlap
 * Summary: Tells you whether a circle overlaps a rectangle.
 * Parameters:
 *   - cx: The circle's centre, across.
 *   - cy: The circle's centre, down.
 *   - cr: The circle's radius.
 *   - rx: The rectangle's left edge.
 *   - ry: The rectangle's top edge.
 *   - rw: The rectangle's width.
 *   - rh: The rectangle's height.
 * Returns: true when the circle and the rectangle overlap or exactly touch; otherwise false.
 * Include: jet_collision.h
 * Notes: It finds the point of the rectangle nearest the circle's centre, then compares squared distances, so there is no square root. The rectangle is upright, not turned. Give it a width, a height and a radius of zero or more.
 * Example:
 *   bool hit = jet_circ_rect_overlap(cx, cy, 12, rx, ry, 64, 32);
 */
static inline bool jet_circ_rect_overlap(float cx, float cy, float cr, float rx, float ry, float rw, float rh) {
    float test_x = cx;
    float test_y = cy;

    if (cx < rx) test_x = rx;
    else if (cx > rx + rw) test_x = rx + rw;

    if (cy < ry) test_y = ry;
    else if (cy > ry + rh) test_y = ry + rh;

    float dx = cx - test_x;
    float dy = cy - test_y;

    return (dx * dx + dy * dy) <= (cr * cr);
}

/**
 * Function: jet_capsule_circ_overlap
 * Summary: Tells you whether a capsule overlaps a circle. A capsule is a line segment thickened by a radius, the shape a moving circle sweeps out, and the test measures the nearest distance between the segment and the circle's centre.
 * Parameters:
 *   - ax: Where the segment starts, across. The segment is the capsule's spine.
 *   - ay: Where the segment starts, down.
 *   - bx: Where the segment ends, across.
 *   - by: Where the segment ends, down.
 *   - cap_r: The capsule's radius: how far it reaches out from the segment on every side, such as the radius of the circle being swept along it.
 *   - cx: The circle's centre, across.
 *   - cy: The circle's centre, down.
 *   - cr: The circle's radius.
 * Returns: true when the capsule and the circle overlap or exactly touch; otherwise false.
 * Include: jet_collision.h
 * Notes: This is the exact test for a moving circle against a still one: a circle of radius cap_r travelling from (ax, ay) to (bx, by) touches the circle at (cx, cy) with radius cr somewhere along the way if, and only if, this returns true. Use it for a dash, a ram or a laser sweep, where testing a fast mover only at the positions it lands on would let it pass straight through a thin target.
 *
 *        The nearest point on the segment is found by projecting the circle's centre onto it and holding the result between the segment's two ends, and only squared distances are compared, so there is no square root. A segment of zero length becomes a plain circle-against-circle test at (ax, ay), the same answer jet_circ_overlap would give.
 * Example:
 *   // Did the ship, swept from last frame to this frame, carve through the blob?
 *   if (jet_capsule_circ_overlap(prev_x, prev_y, x, y, ship_r, blob_x, blob_y, blob_r)) split(blob);
 */
static inline bool jet_capsule_circ_overlap(float ax, float ay, float bx, float by, float cap_r,
                                             float cx, float cy, float cr) {
    float abx = bx - ax;
    float aby = by - ay;
    float acx = cx - ax;
    float acy = cy - ay;
    float ab_len2 = abx * abx + aby * aby;

    // Project C onto the segment, clamping the parameter to [0,1] so the closest
    // point stays on the segment (not its infinite line). A zero-length segment
    // pins t to the start point, degrading gracefully to a circle-circle test.
    float t = (ab_len2 > 0.0f) ? (acx * abx + acy * aby) / ab_len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;

    float closest_x = ax + abx * t;
    float closest_y = ay + aby * t;
    float dx = cx - closest_x;
    float dy = cy - closest_y;
    float rad = cap_r + cr;

    return (dx * dx + dy * dy) <= (rad * rad);
}


#endif // JET_COLLISION_H
