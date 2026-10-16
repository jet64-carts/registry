// runtime/jet_spatial_hash.h
//
// A quick way to find the objects near a point without checking every object
// in the world. The world is cut into square cells. Each frame you file every
// object in the cell its position falls in, and a question about one spot only
// looks in the few cells around that spot. Programmers call this a broadphase:
// the cheap first pass that lists the possible neighbours, so the exact test
// only has to run on those few. Use it for collisions, flocking, picking a
// target, and any other "what is near me?" question with many moving objects.
// This is an opt-in module: include "jet_spatial_hash.h" yourself.
//
// To start, declare one JetSpatialHash that lasts the whole game and call
// jet_spatial_hash_init on it once, in _init. Give it the world's width and
// height, a cell size, and max_points, the most objects any one frame can
// hold, and check the true or false it hands back. Then rebuild it every
// _update: jet_spatial_hash_clear, one jet_spatial_hash_insert for each
// object, then your queries. The rebuild is needed because moving an object
// does not move the copy already filed in the hash.
//
// Each insert stores a position and a uint32_t ID of your choosing, and a
// query hands those IDs back. The answer is a list of candidates, not proven
// neighbours: it holds every point in the cells the query circle touches,
// which can include a few points just outside the radius, and it never misses
// a point inside. Look up each ID and do the exact distance or collision test
// yourself.
//
// Choose cell_size close to the diameter of your usual query. Smaller cells
// make a finer grid, with more cells to store and clear; larger cells hand
// back more candidates for the exact test. The example's 96-pixel cells suit
// its 48-pixel radius.
//
// A full hash drops any further inserts. If jet_spatial_hash_overflow returns
// more than zero, real neighbours may be missing, so raise max_points. A full
// output array cuts a query short too: make the array bigger, or use the
// callback form, which needs no array, when you cannot say how many
// candidates there might be.
//
// For a world that wraps around, where going off one edge brings you back in
// at the other, keep the positions you insert inside the world, query with
// jet_spatial_hash_query_wrapped, and finish with a distance test that wraps
// as well. The plain query treats the world's edges as walls.
//
// This module works in float. If your world is in whole numbers, tiles, or a
// fixed-point coordinate that a game must reproduce exactly on every machine,
// use its integer sibling jet_grid_hash.h instead: a trip through a float
// would change the very coordinates you are trying to keep identical.
//
// Complete neighbour-query example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_spatial_hash.h"
//
//   enum {
//       POINT_COUNT = 3,
//       QUERY_RADIUS = 48,
//       CELL_SIZE = QUERY_RADIUS * 2
//   };
//   static const Vector2 points[POINT_COUNT] = {
//       { 80.0f, 80.0f }, { 128.0f, 96.0f }, { 240.0f, 120.0f }
//   };
//   static JetSpatialHash neighbours;
//   static bool hash_ready;
//   static bool is_nearby[POINT_COUNT];
//
//   void _init(void) {
//       hash_ready = jet_spatial_hash_init(
//           &neighbours, (float)JET_WIDTH, (float)JET_HEIGHT,
//           (float)CELL_SIZE, POINT_COUNT
//       );
//   }
//
//   void _update(float dt) {
//       uint32_t candidates[POINT_COUNT];
//       const float query_x = (float)jet_mouse_x();
//       const float query_y = (float)jet_mouse_y();
//       (void)dt;
//
//       for (int i = 0; i < POINT_COUNT; i++) is_nearby[i] = false;
//       if (!hash_ready) return;
//
//       jet_spatial_hash_clear(&neighbours);
//       for (uint32_t i = 0; i < POINT_COUNT; i++) {
//           jet_spatial_hash_insert(&neighbours, points[i].x, points[i].y, i);
//       }
//       const int count = jet_spatial_hash_query(
//           &neighbours, query_x, query_y, (float)QUERY_RADIUS,
//           candidates, POINT_COUNT
//       );
//       for (int i = 0; i < count; i++) {
//           const uint32_t id = candidates[i];
//           const float dx = points[id].x - query_x;
//           const float dy = points[id].y - query_y;
//           is_nearby[id] = dx * dx + dy * dy <=
//                           (float)(QUERY_RADIUS * QUERY_RADIUS);
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       for (int i = 0; i < POINT_COUNT; i++) {
//           const uint8_t colour = is_nearby[i] ? 12 : 6;
//           jet_circfill((int)points[i].x, (int)points[i].y, 4, colour);
//       }
//       jet_circ(jet_mouse_x(), jet_mouse_y(), QUERY_RADIUS, 15);
//   }
//
//   void _end(void) {
//       // The hash's WRAM lasts as long as the cart does; nothing is released.
//   }
//
// <nocodex>
//
// A uniform-grid SPATIAL HASH (broadphase) — an OPT-IN runtime module. Like
// jet_pool.h, jet_pathfind.h and jet_redraw.h it is deliberately NOT pulled
// in by jet64.h: a cart that needs neighbour queries includes
// "jet_spatial_hash.h" itself, and only then does the compiler's dependency
// resolver pull jet_spatial_hash.c into the build. Carts that never query pay
// zero compile cost. This header IS the sandbox boundary for the hash; a cart
// uses only what it exposes.
//
// THE PROBLEM IT REPLACES: today every cart that asks "what is near this point?"
// does a brute-force O(n^2) pairwise distance sweep — rock-on-rock collisions
// resolved by testing every pair, or 500 flocking boids that each scan all the
// others. That is fine at small n and
// quadratic at large n. A spatial hash buckets points into a uniform grid so a
// radius query only visits the handful of cells the query disc can touch,
// turning a dense-field neighbour search from O(n^2) into ~O(n).
//
// THE INTENDED USE — FACTION-VS-FACTION COMBAT ("Attack of the Sentient
// Plasmas"): the hash is deliberately FACTION-AGNOSTIC. It stores points plus an
// opaque uint32 payload (user_id) and nothing more. The A.S.P. cart passes each
// blob's POOL INDEX (see jet_pool.h) as the payload, then — for every candidate
// the query returns — looks the blob up in its pool to read its faction and do
// the exact test. Because allegiance is mutable (a mind-controlled blob switches
// sides and attacks its former peers), collision is genuinely many-to-many
// across factions: bullets<->blobs, blob<->blob (both merge detection AND
// combat), player-ram<->blobs. Keeping faction OUT of the hash means one rebuilt
// grid serves every one of those interaction classes; the cart filters by
// faction after the query, where the rules live.
//
// HOW A CART USES IT, PER FRAME:
//   jet_spatial_hash_clear(&hash);
//   for each entity e:  jet_spatial_hash_insert(&hash, e.x, e.y, e.pool_index);
//   for each query q:   n = jet_spatial_hash_query(&hash, q.x, q.y, q.r, ids, MAX);
//                       for each id in ids[0..n): exact test against entity[id].
// Rebuilding the whole grid every frame is intentional: it is cheap, perfectly
// cache-friendly, and sidesteps all stale-cell bookkeeping that incremental
// updates would need.
//
// CONSERVATIVE BY DESIGN: a query returns every point in the grid cells its disc
// overlaps, so the candidate set is a SUPERSET of the true neighbours — it may
// include points just outside the radius (and never misses one inside it). The
// CALLER does the exact distance test. The runtime already has everything for
// that test: jet_circ_overlap / jet_point_in_circ (jet_collision.h) and
// jet_vector2_distance_sq / jet_circles_overlap_wrapped (jet_math.h).
//
// THE SIBLING, AND WHERE THE LINE BETWEEN THEM FALLS: jet_grid_hash.h is this
// module's INTEGER counterpart, written from Jet Quest's own broadphase (its
// D26). It differs on exactly two axes. Its coordinates are int32 — which is not
// a preference but a requirement for a deterministic sim, since a Q16.16 world
// coordinate does not survive a round trip through a 24-bit mantissa. And it
// COUNTING-SORTS rather than chains, so a query gets a contiguous run of the
// caller's own item type to stream instead of a chain of uint32 payloads to
// chase, at the cost of a second pass over the objects; it can also cover a
// window that follows a point, so its per-build cost follows the crowd rather
// than the ground. Everything else — the uniform grid, the conservative
// superset, the caller's exact test, the honest-degradation rule — is shared,
// deliberately. Float world, sparse queries, one-pass insert: stay here.
// Integer world, dense crowd, streamed candidates: go there. Neither replaces
// the other, and a cart that reaches only one compiles only one.
//
// Design notes (the choices that matter):
//   * WRAM bump-allocator backing            -> reserved ONCE at init, never freed
//                                              (the allocator has no free), so the
//                                              cost is visible in WRAM telemetry
//                                              and there is no malloc anywhere.
//   * Intrusive per-cell singly-linked lists -> O(1) insert with a single flat
//                                              node pool bounded by max_points, so
//                                              storage is independent of how many
//                                              points happen to share a cell (no
//                                              fixed per-cell sub-array to size or
//                                              waste). Overflow is global, not
//                                              per-cell: only inserting MORE than
//                                              max_points drops a point.
//   * Clamp-not-overrun on overflow          -> past max_points, insert drops the
//                                              point and bumps an overflow counter
//                                              the cart can read (and log); it
//                                              NEVER writes out of bounds.
//   * World tiled by an EXACT integer cell    -> the grid uses grid_w =
//     count on each axis                        floor(world_w / cell_size) cells,
//                                              so an integer number of cells spans
//                                              the world precisely. This makes the
//                                              optional toroidal wrap seam-exact
//                                              (cell index k and k+grid_w map to
//                                              the same column), which a ragged
//                                              last partial cell could not. The
//                                              effective cell size is therefore
//                                              >= the size you request; pick your
//                                              cell_size ~= the largest query
//                                              DIAMETER so a query touches only a
//                                              2x2 block of cells.
//   * Graceful degradation                   -> if WRAM is exhausted at init the
//                                              hash is left inert: queries return
//                                              0 and inserts no-op, mirroring
//                                              jet_redraw_init / jet_pool_init.
// </nocodex>

#ifndef JET_SPATIAL_HASH_H
#define JET_SPATIAL_HASH_H

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memset / floorf (sandbox-safe)

// The value that means "no node here". It marks an empty cell and the end of
// a cell's chain of points. A real node index is always below the capacity
// (at most max_points), so it can never be mistaken for one. It is also the
// byte pattern jet_spatial_hash_clear fills the cell heads with: a memset of
// 0xFF writes 0xFFFFFFFF into every uint32 head, so this value must stay
// all-ones for that fast clear to be right.
#define JET_SPATIAL_HASH_NULL ((uint32_t)0xFFFFFFFFu)

/**
 * Constant: JET_SPATIAL_HASH_MAX_GRID_DIM
 * Summary: The most cells a spatial hash may have across, or down.
 * Include: jet_spatial_hash.h
 * Notes: jet_spatial_hash_init refuses a world and cell size whose grid would be wider or taller than this, and leaves the hash inert: inserts do nothing and every query returns 0. The cartridge's working memory, WRAM, usually runs out before this limit does, so if you are anywhere near it, choose a larger cell_size rather than build a huge grid.
 * Example:
 *   if (world_width / cell_size > JET_SPATIAL_HASH_MAX_GRID_DIM) cell_size = world_width / JET_SPATIAL_HASH_MAX_GRID_DIM;
 */
#define JET_SPATIAL_HASH_MAX_GRID_DIM 65536u

/**
 * Struct: JetSpatialHash
 * Summary: The grid that files 2D points into square cells, so that a question about a circle looks only in the cells the circle touches instead of at every point. You rebuild it every frame: clear it, then insert every object. A query hands back the IDs of every point in the cells the circle touches, a list that can include a few points just outside the radius and never misses one inside, and you do the exact distance test yourself. Its memory is reserved from the cartridge's working memory, WRAM, once at init, and it never asks for more.
 * Fields:
 *   - cell_head: One entry per cell (grid_w times grid_h of them): the index of the first point filed in that cell, or JET_SPATIAL_HASH_NULL when the cell is empty. Reset to all-ones every frame.
 *   - node_next: One entry per inserted point (capacity of them): the index of the next point in the same cell, so the points in a cell form a chain. JET_SPATIAL_HASH_NULL ends the chain.
 *   - node_id: One entry per inserted point (capacity of them): the uint32 ID you gave the point, usually its index in your entity list or pool, handed back unchanged by queries.
 *   - grid_w: How many cells across: world_w divided by cell_size, rounded down, and never less than 1.
 *   - grid_h: How many cells down: world_h divided by cell_size, rounded down, and never less than 1.
 *   - cell_count: grid_w times grid_h, the number of entries in cell_head.
 *   - capacity: The most points one frame can hold, the max_points you gave init. Inserts beyond it are dropped.
 *   - count: How many points have been inserted since the last clear. It is also the index of the next free node.
 *   - overflow: How many points have been dropped since the last clear because the hash was full. You may read this and log it.
 *   - world_w: The world's width in world units, as you gave it to init.
 *   - world_h: The world's height in world units, as you gave it to init.
 *   - cell_size: The cell size you asked for at init. The size actually used, world_w divided by grid_w, may be a little larger, so that a whole number of cells covers the world exactly.
 *   - inv_cell_x: grid_w divided by world_w, worked out once, so a point finds its column with a multiply, cx = floor(x * inv_cell_x), instead of a slower divide.
 *   - inv_cell_y: grid_h divided by world_h, which finds the row the same way.
 * Include: jet_spatial_hash.h
 * Notes: After jet_spatial_hash_init the hash never asks for memory again; clear, insert and every query work in the same fixed arrays. If WRAM ran out at init the hash is left inert, with cell_head NULL, and every query returns 0. Do not read or write the fields yourself; use the functions below.
 * Example:
 *   static JetSpatialHash neighbours;
 *   void _init(void) { jet_spatial_hash_init(&neighbours, JET_WIDTH, JET_HEIGHT, 64.0f, 2048); }
 */
typedef struct {
    uint32_t* cell_head;
    uint32_t* node_next;
    uint32_t* node_id;
    uint32_t  grid_w;
    uint32_t  grid_h;
    uint32_t  cell_count;
    uint32_t  capacity;
    uint32_t  count;
    uint32_t  overflow;
    float     world_w;
    float     world_h;
    float     cell_size;
    float     inv_cell_x;
    float     inv_cell_y;
} JetSpatialHash;

/**
 * Callback: JetSpatialHashCallback
 * Summary: A function you write to receive one candidate at a time from a callback-form query. It is called once for every point in the cells the query circle touches, and inside it you do the exact distance test, and any test of your own, such as which side the point is on. This form needs no output array.
 * Parameters:
 *   - user_id: The ID the point was inserted with, usually its index in your entity list or pool.
 *   - user: The pointer you gave the query, passed on unchanged.
 * Returns: Nothing.
 * Include: jet_spatial_hash.h
 * Notes: A candidate is not a proven neighbour; it may sit just outside the radius, so do the final distance test here. Keep the function quick, and do not insert into the hash or clear it while a query is still calling your function.
 * Example:
 *   static void test_candidate(uint32_t id, void* user) {
 *       QueryContext* q = (QueryContext*)user;
 *       if (entity_overlaps(q->self, &entities[id])) collide(q->self, &entities[id]);
 *   }
 */
typedef void (*JetSpatialHashCallback)(uint32_t user_id, void* user);

/**
 * Function: jet_make_spatial_hash
 * Summary: Gives you an empty hash value that is safe to store before you set it up. You still have to call jet_spatial_hash_init on it before it can hold anything.
 * Parameters:
 *   - None.
 * Returns: A JetSpatialHash with every pointer NULL and every size 0, handed back by value.
 * Include: jet_spatial_hash.h
 * Notes: A hash in this state is inert but safe: insert does nothing and every query returns 0. That lets a cartridge keep one in a global before _init runs.
 * Example:
 *   JetSpatialHash hash = jet_make_spatial_hash();
 */
static inline JetSpatialHash jet_make_spatial_hash(void) {
    JetSpatialHash h;
    h.cell_head  = NULL;
    h.node_next  = NULL;
    h.node_id    = NULL;
    h.grid_w     = 0;
    h.grid_h     = 0;
    h.cell_count = 0;
    h.capacity   = 0;
    h.count      = 0;
    h.overflow   = 0;
    h.world_w    = 0.0f;
    h.world_h    = 0.0f;
    h.cell_size  = 0.0f;
    h.inv_cell_x = 0.0f;
    h.inv_cell_y = 0.0f;
    return h;
}

/**
 * Function: jet_spatial_hash_storage_bytes
 * Summary: Tells you how much of the cartridge's working memory, WRAM, a hash of a given shape will take, so you can budget before you commit to it.
 * Parameters:
 *   - world_w: The world's width in world units. Must be more than 0.
 *   - world_h: The world's height in world units. Must be more than 0.
 *   - cell_size: The cell size you want, in world units. Must be more than 0. The grid uses the largest whole number of cells whose cell is at least this big.
 *   - max_points: The most points the hash will hold in one frame. Must be more than 0.
 * Returns: The number of bytes jet_spatial_hash_init would reserve for that shape, or 0 when the request makes no sense: a width, height or cell size that is zero, negative or NaN, a grid with more than JET_SPATIAL_HASH_MAX_GRID_DIM cells across or down, or a total bigger than the 64 MiB of WRAM.
 * Include: jet_spatial_hash.h
 * Notes: The answer matches the real reservation exactly, including the padding that rounds each of the three arrays (the cell heads and the two node arrays) up to an 8-byte boundary.
 * Example:
 *   uint32_t cost = jet_spatial_hash_storage_bytes(1280.0f, 720.0f, 64.0f, 4096);
 */
uint32_t jet_spatial_hash_storage_bytes(float world_w, float world_h, float cell_size, uint32_t max_points);

/**
 * Function: jet_spatial_hash_init
 * Summary: Sets a hash up: reserves its grid and its point storage from the cartridge's working memory, WRAM, and makes it ready to use.
 * Parameters:
 *   - hash: The hash to set up.
 *   - world_w: The world's width in world units. Must be more than 0. Points are expected from 0 up to, but not including, world_w.
 *   - world_h: The world's height in world units. Must be more than 0. Points are expected from 0 up to, but not including, world_h.
 *   - cell_size: The cell size you want, in world units. Must be more than 0. Choose it close to the DIAMETER of the largest query you will make, so a query touches only a 2x2 block of cells. The size actually used is world_w divided by the number of cells across, which is at least cell_size, so that a whole number of cells covers the world exactly.
 *   - max_points: The most points you will insert between one clear and the next. Must be more than 0. Inserts beyond it are dropped, never written past the end of the arrays.
 * Returns: true when the hash is ready; false when the request makes no sense or WRAM has run out, in which case the hash is left zeroed and inert and not one byte of WRAM has been taken.
 * Include: jet_spatial_hash.h
 * Notes: Call it only from _init. It takes its memory from WRAM, which is handed out once and never given back, so calling it again and again leaks WRAM.
 *
 *        If the reservation fails the hash still behaves: insert does nothing and every query returns 0, just as jet_redraw_init and jet_pool_init behave when they fail. Size max_points for your fullest possible frame (every blob, every bullet and the player), once, for the busiest stage of your game.
 * Example:
 *   JetSpatialHash hash;
 *   jet_spatial_hash_init(&hash, (float)JET_WIDTH, (float)JET_HEIGHT, 64.0f, 4096);
 */
bool jet_spatial_hash_init(JetSpatialHash* hash, float world_w, float world_h, float cell_size, uint32_t max_points);

/**
 * Function: jet_spatial_hash_clear
 * Summary: Empties every cell, ready for this frame's points. Call it once a frame, before you insert.
 * Parameters:
 *   - hash: The hash to clear.
 * Returns: Nothing.
 * Include: jet_spatial_hash.h
 * Notes: The cost grows with the number of cells: one memset over the cell heads, plus a reset of the counters. The node arrays are left alone, because count goes back to 0 and an old link is never followed. The overflow counter goes back to 0 as well. On an inert hash, one that was never set up, it does nothing.
 * Example:
 *   jet_spatial_hash_clear(&hash);
 */
void jet_spatial_hash_clear(JetSpatialHash* hash);

/**
 * Function: jet_spatial_hash_insert
 * Summary: Files one point, with an ID of your choosing, into its cell. It takes the same small time however many points there are.
 * Parameters:
 *   - hash: The hash to insert into.
 *   - x: The point's X in world units. It is clamped into the grid, so a point a little outside the range 0 to world_w still lands in the nearest edge cell and can still be found.
 *   - y: The point's Y in world units, clamped into the grid the same way.
 *   - user_id: The ID a query will hand back for this point. Pass the object's index in its pool (see jet_pool.h), so you can look up its side and its exact position afterwards.
 * Returns: true when the point was stored; false when it was dropped because the hash was full, or the hash is inert (never set up).
 * Include: jet_spatial_hash.h
 * Notes: Once capacity (the max_points you gave init) is reached, a point is dropped and the overflow counter goes up by one; nothing is ever written past the end of the arrays. Read jet_spatial_hash_overflow after you have inserted everything if you want to notice, and log, a hash that is too small.
 *
 *        In a world that wraps around, insert positions that are already wrapped into the world. The clamp is a safety net, not a wrap.
 * Example:
 *   jet_spatial_hash_insert(&hash, blob.pos.x, blob.pos.y, blob_index);
 */
bool jet_spatial_hash_insert(JetSpatialHash* hash, float x, float y, uint32_t user_id);

/**
 * Function: jet_spatial_hash_query
 * Summary: Collects the IDs of every point in the cells a circle touches into an array you provide, and tells you how many it wrote. This is the array form; jet_spatial_hash_query_cb is the callback form.
 * Parameters:
 *   - hash: The hash to search.
 *   - x: The centre of the circle, across, in world units.
 *   - y: The centre of the circle, down, in world units.
 *   - radius: The circle's radius in world units. A negative radius finds nothing.
 *   - out_ids: The array the IDs are written into.
 *   - max_out: How many IDs out_ids can hold. No more than this many are written.
 * Returns: How many IDs were written, never more than max_out.
 * Include: jet_spatial_hash.h
 * Notes: The answer is a list of candidates, not proven neighbours. Because the grid works a whole cell at a time, it can include points just outside the radius, and it never leaves out a point inside it. Do the exact test yourself, with jet_circ_overlap or jet_vector2_distance_sq. Each candidate appears once.
 *
 *        If the answer equals max_out, the array filled up and there may be more. Make the array bigger, or use jet_spatial_hash_query_cb, which needs no array at all.
 * Example:
 *   uint32_t ids[64];
 *   int n = jet_spatial_hash_query(&hash, px, py, 40.0f, ids, 64);
 *   for (int i = 0; i < n; i++) { Blob* b = pool_blob(ids[i]); if (jet_circ_overlap(px, py, 40.0f, b->x, b->y, b->r)) hit(b); }
 */
int jet_spatial_hash_query(const JetSpatialHash* hash, float x, float y, float radius, uint32_t* out_ids, int max_out);

/**
 * Function: jet_spatial_hash_query_cb
 * Summary: Calls your function once for every point in the cells a circle touches, so you never have to size an output array. This is the callback form; jet_spatial_hash_query is the array form.
 * Parameters:
 *   - hash: The hash to search.
 *   - x: The centre of the circle, across, in world units.
 *   - y: The centre of the circle, down, in world units.
 *   - radius: The circle's radius in world units. A negative radius finds nothing.
 *   - cb: Your function, called once for each candidate ID.
 *   - user: A pointer of your own, passed on unchanged to every call of cb. Pass NULL if you have no use for it.
 * Returns: How many candidates were visited, which is how many times cb was called.
 * Include: jet_spatial_hash.h
 * Notes: The answer has the same shape as jet_spatial_hash_query's: candidates that may sit just outside the radius, and never a missing one inside it, so do the exact test inside your function. Each candidate is visited once. Prefer this form for a dense sweep in which every side tests against every other and the number of candidates has no ceiling.
 * Example:
 *   int seen = jet_spatial_hash_query_cb(&hash, px, py, 40.0f, on_candidate, &ctx);
 */
int jet_spatial_hash_query_cb(const JetSpatialHash* hash, float x, float y, float radius, JetSpatialHashCallback cb, void* user);

/**
 * Function: jet_spatial_hash_query_wrapped
 * Summary: The wrap-around version of jet_spatial_hash_query: collects candidates in a world whose edges join up, so a circle near one edge also sees the points poking through from the opposite edge.
 * Parameters:
 *   - hash: The hash to search.
 *   - x: The centre of the circle, across, in world units.
 *   - y: The centre of the circle, down, in world units.
 *   - radius: The circle's radius in world units. A negative radius finds nothing.
 *   - out_ids: The array the IDs are written into.
 *   - max_out: How many IDs out_ids can hold. No more than this many are written.
 * Returns: How many IDs were written, never more than max_out.
 * Include: jet_spatial_hash.h
 * Notes: It wraps the same way jet_circles_overlap_wrapped in jet_math.h does; use it on the stages where the playfield wraps, as A.S.P. does. The wrap is exact at the seam, because a whole number of cells covers the world. Each cell is visited at most once, even when the radius is so large that it wraps the whole world, so no candidate appears twice. The list is still only candidates: do the exact test with jet_circles_overlap_wrapped, giving it the same world_w and world_h. Insert positions that are already wrapped into the world, from 0 up to but not including its size.
 * Example:
 *   int n = jet_spatial_hash_query_wrapped(&hash, px, py, 40.0f, ids, 64);
 */
int jet_spatial_hash_query_wrapped(const JetSpatialHash* hash, float x, float y, float radius, uint32_t* out_ids, int max_out);

/**
 * Function: jet_spatial_hash_query_wrapped_cb
 * Summary: The wrap-around version of jet_spatial_hash_query_cb: calls your function for every candidate in a world whose edges join up, with no output array to size.
 * Parameters:
 *   - hash: The hash to search.
 *   - x: The centre of the circle, across, in world units.
 *   - y: The centre of the circle, down, in world units.
 *   - radius: The circle's radius in world units. A negative radius finds nothing.
 *   - cb: Your function, called once for each candidate ID.
 *   - user: A pointer of your own, passed on unchanged to every call of cb. Pass NULL if you have no use for it.
 * Returns: How many candidates were visited, which is how many times cb was called.
 * Include: jet_spatial_hash.h
 * Notes: See jet_spatial_hash_query_wrapped for the promises about the seam and about visiting each cell once. Do the exact wrapped test, jet_circles_overlap_wrapped, inside your function.
 * Example:
 *   int seen = jet_spatial_hash_query_wrapped_cb(&hash, px, py, 40.0f, on_candidate, &ctx);
 */
int jet_spatial_hash_query_wrapped_cb(const JetSpatialHash* hash, float x, float y, float radius, JetSpatialHashCallback cb, void* user);

/**
 * Function: jet_spatial_hash_count
 * Summary: Tells you how many points have been inserted since the last clear.
 * Parameters:
 *   - hash: The hash to ask.
 * Returns: The number of points in the hash right now: 0 for a hash that was never set up, or one that has just been cleared.
 * Include: jet_spatial_hash.h
 * Notes: Only points that were stored are counted. A point that was dropped because the hash was full is counted by jet_spatial_hash_overflow instead.
 * Example:
 *   uint32_t n = jet_spatial_hash_count(&hash);
 */
static inline uint32_t jet_spatial_hash_count(const JetSpatialHash* hash) {
    return hash ? hash->count : 0u;
}

/**
 * Function: jet_spatial_hash_overflow
 * Summary: Tells you how many points were dropped since the last clear because the hash was full.
 * Parameters:
 *   - hash: The hash to ask.
 * Returns: The number of dropped inserts this frame: 0 when none were dropped, or the hash was never set up.
 * Include: jet_spatial_hash.h
 * Notes: More than zero means max_points is too small for this scene. Some points are missing from the grid, so a query can miss a real neighbour. Read it after you have inserted everything if you want to notice, and log, a hash that is too small.
 * Example:
 *   if (jet_spatial_hash_overflow(&hash) > 0u) log_warn("spatial hash undersized this frame");
 */
static inline uint32_t jet_spatial_hash_overflow(const JetSpatialHash* hash) {
    return hash ? hash->overflow : 0u;
}

/**
 * Function: jet_spatial_hash_grid_w
 * Summary: Tells you how many cells wide the grid is.
 * Parameters:
 *   - hash: The hash to ask.
 * Returns: The number of columns of cells: 0 for a hash that was never set up.
 * Include: jet_spatial_hash.h
 * Notes: The number is fixed when jet_spatial_hash_init works it out and never changes. It is world_w divided by cell_size, rounded down for safety, so it can be a little less than the division suggests.
 * Example:
 *   uint32_t cols = jet_spatial_hash_grid_w(&hash);
 */
static inline uint32_t jet_spatial_hash_grid_w(const JetSpatialHash* hash) {
    return hash ? hash->grid_w : 0u;
}

/**
 * Function: jet_spatial_hash_grid_h
 * Summary: Tells you how many cells tall the grid is.
 * Parameters:
 *   - hash: The hash to ask.
 * Returns: The number of rows of cells: 0 for a hash that was never set up.
 * Include: jet_spatial_hash.h
 * Notes: The number is fixed when jet_spatial_hash_init works it out and never changes. It is world_h divided by cell_size, rounded down for safety, so it can be a little less than the division suggests.
 * Example:
 *   uint32_t rows = jet_spatial_hash_grid_h(&hash);
 */
static inline uint32_t jet_spatial_hash_grid_h(const JetSpatialHash* hash) {
    return hash ? hash->grid_h : 0u;
}

#endif // JET_SPATIAL_HASH_H
