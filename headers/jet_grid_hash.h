// runtime/jet_grid_hash.h
//
// A grid hash quickly finds objects near a point without checking every object
// in the world, using whole-number coordinates only. It is the INTEGER sibling
// of jet_spatial_hash.h: same job, no float anywhere, so a cart whose world is
// fixed-point or tile-based gets a broadphase whose answers are bit-identical
// on every machine. Include "jet_grid_hash.h" yourself; this is an opt-in
// module.
//
// Essential rules:
//   * You own the memory. Declare the cell-index array yourself, sized with
//     JET_GRID_HASH_START_ENTRIES, and call jet_grid_hash_init once with it and
//     with the length of your own item array. Nothing here allocates.
//   * You own the items. This module sorts INDICES, not objects: it tells you
//     which slot of your array each object belongs in, so your items stay your
//     type, contiguous per cell, and cheap to scan.
//   * A build is five steps in order, every tick: begin, tally every object,
//     prefix_sum, place every object, finish. Tally and place must visit the
//     SAME objects; place them in the REVERSE of the tally order and each cell
//     keeps your original order.
//   * A query answers only after finish. jet_grid_hash_cell_range returns false
//     for a cell outside the grid, so sweeping a small block of cells clips
//     itself.
//   * Query results are candidates, not confirmed neighbours. A cell holds
//     everything bucketed in it, so test the exact distance yourself.
//   * Size your item array for the most objects you can ever tally. A build
//     that tallies more is refused whole — every cell reads empty and
//     jet_grid_hash_overflow() says by how much — rather than answering about
//     objects it could not hold.
//   * Choose cell_size near the diameter of your largest normal query, so a
//     query touches only a 3x3 block of cells.
//
// Complete neighbour-query cart (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_grid_hash.h"
//
//   enum {
//       DOT_COUNT = 64,      // objects in the field
//       DOT_SPACING = 40,    // pixels between them, laid on a lattice
//       DOT_RADIUS = 3,
//       CELL_PIXELS = 32,    // one cell, ~= the query diameter
//       WINDOW_CELLS = 8,    // the grid follows the mouse: 8 x 8 cells
//       DOT_COLOUR = 6,
//       NEAR_COLOUR = 12
//   };
//
//   typedef struct { int32_t x, y; } Dot;   // YOUR item type, whatever it is
//
//   // dots is the world; bucketed is the same dots grouped by cell, which is
//   // what a query hands you a run of. start is the grid's own cell index.
//   static Dot dots[DOT_COUNT];
//   static Dot bucketed[DOT_COUNT];
//   static uint32_t start[JET_GRID_HASH_START_ENTRIES(WINDOW_CELLS, WINDOW_CELLS)];
//   static JetGridHash grid;
//
//   void _init(void) {
//       jet_grid_hash_init(&grid, start, JET_GRID_HASH_START_ENTRIES(WINDOW_CELLS, WINDOW_CELLS),
//                          DOT_COUNT);
//       for (int i = 0; i < DOT_COUNT; i++) {
//           dots[i].x = (i % 8) * DOT_SPACING + DOT_SPACING;
//           dots[i].y = (i / 8) * DOT_SPACING + DOT_SPACING;
//       }
//   }
//
//   void _update(float dt) {
//       const int32_t mouse_x = (int32_t)jet_mouse_x();
//       const int32_t mouse_y = (int32_t)jet_mouse_y();
//       (void)dt;
//
//       jet_grid_hash_begin_window(&grid, WINDOW_CELLS, WINDOW_CELLS, CELL_PIXELS,
//                                  mouse_x, mouse_y);
//       for (int i = 0; i < DOT_COUNT; i++) {
//           jet_grid_hash_tally(&grid, dots[i].x, dots[i].y);
//       }
//       jet_grid_hash_prefix_sum(&grid);
//       for (int i = DOT_COUNT - 1; i >= 0; i--) {   // reverse: each cell keeps dot order
//           const uint32_t at = jet_grid_hash_place(&grid, dots[i].x, dots[i].y);
//           if (at != JET_GRID_HASH_NONE) bucketed[at] = dots[i];
//       }
//       jet_grid_hash_finish(&grid);
//   }
//
//   void _draw(void) {
//       const int32_t mouse_x = (int32_t)jet_mouse_x();
//       const int32_t mouse_y = (int32_t)jet_mouse_y();
//       const int32_t cell_x = jet_grid_hash_cell_x(&grid, mouse_x);
//       const int32_t cell_y = jet_grid_hash_cell_y(&grid, mouse_y);
//
//       jet_cls(0);
//       for (int i = 0; i < DOT_COUNT; i++) {
//           jet_circfill(dots[i].x, dots[i].y, DOT_RADIUS, DOT_COLOUR);
//       }
//       for (int32_t offset_y = -1; offset_y <= 1; offset_y++) {
//           for (int32_t offset_x = -1; offset_x <= 1; offset_x++) {
//               uint32_t first, count;
//               if (!jet_grid_hash_cell_range(&grid, cell_x + offset_x, cell_y + offset_y,
//                                             &first, &count)) {
//                   continue;   // that cell is outside the window
//               }
//               for (uint32_t k = 0; k < count; k++) {
//                   jet_circfill(bucketed[first + k].x, bucketed[first + k].y,
//                                DOT_RADIUS, NEAR_COLOUR);
//               }
//           }
//       }
//       jet_circ(mouse_x, mouse_y, CELL_PIXELS, 15);
//   }
//
//   void _end(void) {
//       // The arrays are the cart's own; nothing is released.
//   }
//
// <nocodex>
//
// An INTEGER uniform-grid broadphase built by COUNTING SORT — an OPT-IN runtime
// module. Like jet_pool.h, jet_spatial_hash.h and jet_redraw.h it is
// deliberately NOT pulled in by jet64.h: a cart that needs it includes
// "jet_grid_hash.h" itself, and only then does the compiler's dependency
// resolver pull jet_grid_hash.c into the build. Carts that never bucket pay
// zero compile time and zero binary size for it.
//
// WHY IT EXISTS BESIDE jet_spatial_hash — THE TWO ARE HONEST SIBLINGS, NOT
// RIVALS. They differ on the two axes that actually matter:
//
//   * ARITHMETIC. jet_spatial_hash's whole surface is float: init takes a float
//     world and cell size, insert takes float x/y. That is exactly right for the
//     float-space carts it was written for (A.S.P.'s blobs, boids, particles).
//     It is unusable by a cart whose world is FIXED-POINT, because a Q16.16
//     coordinate does not survive a round trip through a 24-bit mantissa — and a
//     deterministic simulation, whose digest must match byte for byte across
//     three runtimes, cannot be built on a coordinate that changes when it is
//     looked at. This module admits no float at any point, in or out.
//   * STRUCTURE. jet_spatial_hash chains points into per-cell singly-linked
//     lists: one pass, O(1) insert, and a query hops through memory following
//     `next` links. This module counting-sorts them: two passes, and a query
//     gets a CONTIGUOUS RUN of items it can stream. Where the candidate scan is
//     the hot loop — a dense crowd, thousands of bodies, every tick — the
//     contiguous run is worth the second pass by a wide margin, and it also
//     lets the caller keep its own item type instead of a uint32 payload it must
//     then chase back into its component arrays.
//
// So: float world, sparse queries, one-pass insert -> jet_spatial_hash. Integer
// world, dense crowd, streamed candidates -> jet_grid_hash. A cart that needs
// neither pays for neither, and one that reaches only one compiles only one.
// The same paragraph, written from the other side, is in jet_spatial_hash.h.
//
// One thing this module deliberately does NOT have: a toroidal query. A
// wrap-around playfield is a float-space idea in this runtime (jet_math.h's
// wrapped distance, the A.S.P. stages), and a deterministic tile world seals
// its edges instead. Adding it here would be speculation, not a need.
//
// WHY THE CALLER OWNS BOTH ARRAYS. This module reserves nothing (contrast
// jet_pool_init / jet_spatial_hash_init, which carve from the WRAM bump
// allocator). Two reasons, both structural:
//   * The item array is the caller's TYPE, not an opaque payload. Copying the
//     handful of facts a scan reads into one packed struct is the point of a
//     counting-sorted broadphase: the scan walks one contiguous stream instead
//     of chasing several component arrays through the cache per candidate. A
//     module that owned the items could only own bytes, and would have to
//     memcpy a run-time stride where a typed store belongs.
//   * A grid that allocates nothing can live inside a caller's own struct, be
//     reset with a memset, and be re-bound for free — which is what a
//     deterministic simulation, a save/restore, and a headless test harness all
//     want. The one buffer the module does need (the cell index) is small, and
//     JET_GRID_HASH_START_ENTRIES sizes it as a constant expression, so it can
//     be a plain static array with no init-order question at all.
//
// THE ALGORITHM, IN ITS OWN VOCABULARY. A counting sort has exactly three
// passes, and the five verbs are those three passes plus their brackets:
//   begin       zero one counter per cell; fix the lattice for this build.
//   tally       pass one: ++counter[cell of this object].
//   prefix_sum  pass two: a running sum turns the counts into ENDS, so cell c
//               ends at start[c] and the total lands in start[cell_count].
//   place       pass three: --start[cell] is this object's slot. Walking the
//               objects BACKWARDS therefore walks each cell's entries down to
//               its own start, which leaves start[] holding exactly the starts
//               the query wants — no second cursor array, no fix-up pass, and
//               each cell in the caller's original order (a stable sort).
//   finish      the build is closed and queryable; placed must equal tallied.
//
// WHY THE WINDOW. jet_grid_hash_begin_window is the verb a moving world wants: a
// grid that covers the whole map costs one counter per map cell EVERY BUILD,
// whatever the population — a cost proportional to the ground rather than to the
// crowd. A grid that covers a fixed square centred on the character costs a
// constant, and is exact rather than approximate whenever the caller can bound
// how far from that centre anything can matter. The window's origin is snapped
// DOWN to the cell lattice, so the lattice is a property of the world rather
// than of where the centre happened to be: an object changes cell only when it
// crosses a lattice line, never merely because the window slid under it.
//
// Design notes (the choices that matter):
//   * Caller-owned storage, nothing reserved  -> see above; also why init is
//                                                free to call again on a struct
//                                                that has been memset.
//   * Refuse, never truncate, on overflow     -> a build that tallies more items
//                                                than the caller's array can
//                                                hold is refused WHOLE: every
//                                                cell reads empty and
//                                                jet_grid_hash_overflow() states
//                                                the excess. Dropping the excess
//                                                instead cannot be done
//                                                consistently across two passes
//                                                that visit the objects in
//                                                opposite orders, and a grid
//                                                that is silently wrong about
//                                                which objects it holds is worse
//                                                than one that says it holds
//                                                none.
//   * A phase field, checked on every verb    -> a build is a five-step
//                                                sequence, so the states are
//                                                real. Checking them turns every
//                                                misordering into a safe,
//                                                countable refusal instead of
//                                                nonsense read out of a
//                                                half-summed array. The checks
//                                                are register compares on a
//                                                perfectly predicted path.
//   * Cell arithmetic in 64-bit, saturating   -> the difference of two extreme
//                                                int32 coordinates does not fit
//                                                an int32. The lattice is
//                                                therefore computed in int64 and
//                                                the RESULT saturated, which is
//                                                exact for every in-range
//                                                coordinate and keeps a wildly
//                                                out-of-range one wildly out of
//                                                range — all a bounds test needs.
//   * Floor division toward negative infinity -> so the lattice is continuous
//                                                across the origin: one unit
//                                                west of cell 0 is cell -1, never
//                                                cell 0 again.
//   * The hot verbs are static inline         -> tally, place, the two lattice
//                                                verbs and the query are each a
//                                                handful of integer operations
//                                                called once per object per pass
//                                                (or once per cell per query).
//                                                Only the five cold verbs — the
//                                                ones that validate a shape —
//                                                live out of line in the .c.
// </nocodex>

#ifndef JET_GRID_HASH_H
#define JET_GRID_HASH_H

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memset (sandbox-safe)

/**
 * Constant: JET_GRID_HASH_NONE
 * Summary: The "no slot" answer from jet_grid_hash_place — the object was outside the grid, or the build cannot hold it.
 * Include: jet_grid_hash.h
 * Notes: A real slot is always less than the item capacity you passed to jet_grid_hash_init, so this all-ones value can never collide with one. Always compare a place() result against it before writing your item.
 * Example:
 *   uint32_t at = jet_grid_hash_place(&grid, dot.x, dot.y);
 *   if (at != JET_GRID_HASH_NONE) bucketed[at] = dot;
 */
#define JET_GRID_HASH_NONE ((uint32_t)0xFFFFFFFFu)

/**
 * Constant: JET_GRID_HASH_START_ENTRIES
 * Summary: The number of uint32_t entries a grid of the given cell dimensions needs in its cell-index array. Use it to declare that array.
 * Include: jet_grid_hash.h
 * Notes: One entry per cell plus one, so the last cell needs no special case: cell c spans start[c] to start[c+1]. A constant expression, so it sizes a static array directly. Declare it for the LARGEST grid you will ever build; jet_grid_hash_begin refuses a shape that would not fit.
 * Example:
 *   static uint32_t start[JET_GRID_HASH_START_ENTRIES(64, 64)];
 */
#define JET_GRID_HASH_START_ENTRIES(cells_x, cells_y) ((cells_x) * (cells_y) + 1)

/**
 * Enum: JetGridHashPhase
 * Summary: Which step of a build a grid is in. A build runs begin, tally, prefix_sum, place, finish, and each verb accepts only the phase it belongs to.
 * Include: jet_grid_hash.h
 * Notes: JET_GRID_HASH_INERT covers every unusable state — never initialised, a geometry jet_grid_hash_begin refused, an overflowed build, or a build whose place pass did not match its tally pass. Queries answer empty in all of them, so an unusable grid is honestly empty rather than wrong. Read it with jet_grid_hash_phase if you want to assert the sequence in a test.
 * Example:
 *   if (jet_grid_hash_phase(&grid) != JET_GRID_HASH_BUILT) return; // nothing to query yet
 */
typedef enum {
    JET_GRID_HASH_INERT = 0,  // unusable: uninitialised, refused, or incomplete.
    JET_GRID_HASH_TALLYING,   // between jet_grid_hash_begin and _prefix_sum.
    JET_GRID_HASH_PLACING,    // between jet_grid_hash_prefix_sum and _finish.
    JET_GRID_HASH_BUILT       // closed by jet_grid_hash_finish; queryable.
} JetGridHashPhase;

/**
 * Struct: JetGridHash
 * Summary: An integer uniform-grid broadphase that counting-sorts your objects into cells so a query reads one contiguous run per cell instead of scanning everything. It owns no memory: you supply the cell-index array and your own item array, and it hands back the slot each object belongs in. Rebuilt each tick (begin, tally, prefix_sum, place, finish).
 * Fields:
 *   - start: Your cell-index array (cell_count + 1 entries). After finish, cell c holds items start[c] up to start[c+1].
 *   - start_capacity: Entries in that array, as passed to init; jet_grid_hash_begin refuses a grid needing more.
 *   - item_capacity: Length of YOUR item array, as passed to init. No slot this module returns is ever outside it.
 *   - cells_x: Grid width in cells, fixed for the current build.
 *   - cells_y: Grid height in cells, fixed for the current build.
 *   - cell_count: cells_x * cells_y, the number of cells this build indexes.
 *   - cell_size: Width and height of one cell, in world units.
 *   - origin_x: World X of cell (0, 0)'s low corner.
 *   - origin_y: World Y of cell (0, 0)'s low corner.
 *   - count: Objects tallied into this build — the number that must be placed, and after finish the number the grid holds.
 *   - placed: Objects placed so far; finish requires it to equal count.
 *   - overflow: Objects a refused build could not hold, over item_capacity; 0 whenever the build succeeded.
 *   - phase: Which build step the grid is in (see JetGridHashPhase).
 * Include: jet_grid_hash.h
 * Notes: Nothing here allocates, so the struct may live in a cart's own state, be zeroed with memset, and be re-initialised for free. Do not read or write the fields directly — use the accessors below.
 * Example:
 *   static uint32_t start[JET_GRID_HASH_START_ENTRIES(64, 64)];
 *   static JetGridHash grid;
 *   void _init(void) { jet_grid_hash_init(&grid, start, JET_GRID_HASH_START_ENTRIES(64, 64), BODY_MAX); }
 */
typedef struct {
    uint32_t*        start;
    uint32_t         start_capacity;
    uint32_t         item_capacity;
    uint32_t         cells_x;
    uint32_t         cells_y;
    uint32_t         cell_count;
    int32_t          cell_size;
    int32_t          origin_x;
    int32_t          origin_y;
    uint32_t         count;
    uint32_t         placed;
    uint32_t         overflow;
    JetGridHashPhase phase;
} JetGridHash;

/**
 * Function: jet_make_grid_hash
 * Summary: Returns an empty, inert grid handle. You must still call jet_grid_hash_init on it before use; this only gives you a safe value to store.
 * Parameters:
 *   - None.
 * Returns: A zeroed JetGridHash, returned by value (no array bound, no cells).
 * Include: jet_grid_hash.h
 * Notes: An inert grid is safe to use — every build verb refuses and every query answers empty — so a cart can hold one before _init() runs.
 * Example:
 *   JetGridHash grid = jet_make_grid_hash();
 */
static inline JetGridHash jet_make_grid_hash(void) {
    JetGridHash grid;
    grid.start          = NULL;
    grid.start_capacity = 0u;
    grid.item_capacity  = 0u;
    grid.cells_x        = 0u;
    grid.cells_y        = 0u;
    grid.cell_count     = 0u;
    grid.cell_size      = 0;
    grid.origin_x       = 0;
    grid.origin_y       = 0;
    grid.count          = 0u;
    grid.placed         = 0u;
    grid.overflow       = 0u;
    grid.phase          = JET_GRID_HASH_INERT;
    return grid;
}

/**
 * Function: jet_grid_hash_init
 * Summary: Binds the grid to the cell-index array you declared and to the length of your own item array. Call it once before the first build.
 * Parameters:
 *   - grid: The grid to initialise.
 *   - start: Your cell-index array — declare it with JET_GRID_HASH_START_ENTRIES for the largest grid you will build.
 *   - start_capacity: How many uint32_t entries that array holds.
 *   - item_capacity: How many items your own array holds; no slot returned by jet_grid_hash_place is ever outside it.
 * Returns: true when the grid is ready to build; false for a NULL array or a zero capacity, which leaves the grid inert.
 * Include: jet_grid_hash.h
 * Notes: THIS RESERVES NOTHING — unlike jet_pool_init or jet_spatial_hash_init it only remembers the memory you supplied, so it is free to call again (after a memset, or on a grid inside a struct you reset). The arrays must outlive the grid: static or WRAM storage, never a local.
 * Example:
 *   jet_grid_hash_init(&grid, start, JET_GRID_HASH_START_ENTRIES(64, 64), BODY_MAX);
 */
bool jet_grid_hash_init(JetGridHash* grid, uint32_t* start, uint32_t start_capacity, uint32_t item_capacity);

/**
 * Function: jet_grid_hash_begin
 * Summary: Starts a build over a grid whose corner you name. Clears every cell counter and fixes the lattice for this build.
 * Parameters:
 *   - grid: The grid to build.
 *   - cells_x: Grid width in cells (must be > 0).
 *   - cells_y: Grid height in cells (must be > 0).
 *   - cell_size: Width and height of one cell in world units (must be > 0); choose it near your largest query diameter so a query touches only a 3x3 block.
 *   - origin_x: World X of cell (0, 0)'s low corner.
 *   - origin_y: World Y of cell (0, 0)'s low corner.
 * Returns: true when the build may proceed; false when the grid is inert or the shape needs more entries than your cell-index array holds, which leaves the grid inert.
 * Include: jet_grid_hash.h
 * Notes: O(cells) — one clear per build, so a grid that covers ground your objects never occupy costs you every tick. Use jet_grid_hash_begin_window when the interesting region follows a character. The shape may change between builds (a coarse grid for steering, a fine one for contact) as long as each fits the array init was given.
 * Example:
 *   jet_grid_hash_begin(&grid, MAP_CELLS_X, MAP_CELLS_Y, CELL_TILES, 0, 0);
 */
bool jet_grid_hash_begin(JetGridHash* grid, uint32_t cells_x, uint32_t cells_y, int32_t cell_size,
                         int32_t origin_x, int32_t origin_y);

/**
 * Function: jet_grid_hash_begin_window
 * Summary: Starts a build over a grid CENTRED on a point — the moving window a following camera or an active character wants. Same as jet_grid_hash_begin with the corner worked out for you.
 * Parameters:
 *   - grid: The grid to build.
 *   - cells_x: Window width in cells (must be > 0).
 *   - cells_y: Window height in cells (must be > 0).
 *   - cell_size: Width and height of one cell in world units (must be > 0). The window spans cells_x * cell_size by cells_y * cell_size.
 *   - centre_x: World X the window is centred on.
 *   - centre_y: World Y the window is centred on.
 * Returns: true when the build may proceed; false on the same terms as jet_grid_hash_begin.
 * Include: jet_grid_hash.h
 * Notes: The corner is snapped DOWN to the cell lattice, so the lattice belongs to the world and not to the centre: an object changes cell only when it crosses a lattice line, never merely because the window slid under it. Objects outside the window are simply not bucketed — which is exact, not lossy, whenever your rules already bound how far from the centre anything can matter.
 * Example:
 *   jet_grid_hash_begin_window(&grid, WINDOW_CELLS, WINDOW_CELLS, CELL_TILES, hero_tile_x, hero_tile_y);
 */
bool jet_grid_hash_begin_window(JetGridHash* grid, uint32_t cells_x, uint32_t cells_y, int32_t cell_size,
                                int32_t centre_x, int32_t centre_y);

/**
 * Function: jet_grid_hash_prefix_sum
 * Summary: Closes the tally pass and turns the per-cell counts into the offsets the place pass consumes. Call it once, between tallying and placing.
 * Parameters:
 *   - grid: The grid being built.
 * Returns: The number of objects tallied — exactly how many jet_grid_hash_place calls must follow. Returns 0 when the grid is not tallying, or when more objects were tallied than your item array can hold (the build is then refused whole and jet_grid_hash_overflow states the excess).
 * Include: jet_grid_hash.h
 * Notes: O(cells). A refused build is honestly EMPTY, not partly right: every cell reads zero candidates and every place() answers JET_GRID_HASH_NONE. Size your item array for the most objects you can ever tally and this cannot happen.
 * Example:
 *   uint32_t total = jet_grid_hash_prefix_sum(&grid);
 */
uint32_t jet_grid_hash_prefix_sum(JetGridHash* grid);

/**
 * Function: jet_grid_hash_finish
 * Summary: Closes the build and makes the grid queryable, confirming that every tallied object was placed.
 * Parameters:
 *   - grid: The grid being built.
 * Returns: true when the build is complete and queryable; false when the grid was not placing or the two passes disagreed, which leaves it inert and empty.
 * Include: jet_grid_hash.h
 * Notes: The two passes must visit the SAME objects — that is what makes the offsets meaningful — so this is the check that turns a mismatched pair from silent corruption into an honest, empty grid. Deriving both passes from one predicate over one list is the way to guarantee it.
 * Example:
 *   if (!jet_grid_hash_finish(&grid)) LOG_WARN("the broadphase build was incomplete");
 */
bool jet_grid_hash_finish(JetGridHash* grid);
// --- the lattice, the two build passes, and the query ----------------------
// These five verbs are called once per object per pass, or once per cell per
// query, in the loops a dense crowd spends its tick in, and each is a handful
// of integer operations — so they are `static inline` here rather than out of
// line in the .c, exactly as jet_spatial_hash's accessors are. Only the five
// cold verbs above, the ones that validate a shape, are real calls.

// <nocodex>
// One axis of the lattice: floor((world - origin) / cell_size), toward negative
// infinity on both sides so the lattice is continuous across the origin.
// Computed in 64-bit because the difference of two extreme int32 coordinates
// does not fit an int32, then saturated: exact for every in-range coordinate,
// and a wildly out-of-range one stays wildly out of range, which is all the
// bounds test below asks of it. `cell_size` is > 0 at every call site.
static inline int32_t jet_grid_hash_axis_(int32_t world, int32_t origin, int32_t cell_size) {
    const int64_t offset = (int64_t)world - (int64_t)origin;
    const int64_t size   = (int64_t)cell_size;
    const int64_t cell   = offset >= 0 ? offset / size : -((-offset + size - 1) / size);
    if (cell < (int64_t)INT32_MIN) return INT32_MIN;
    if (cell > (int64_t)INT32_MAX) return INT32_MAX;
    return (int32_t)cell;
}
// </nocodex>

/**
 * Function: jet_grid_hash_cell_x
 * Summary: Returns the cell column a world X falls in, in the grid's own coordinates.
 * Parameters:
 *   - grid: The grid to ask.
 *   - world_x: A world X coordinate.
 * Returns: The column index, which may be negative or beyond the grid when the coordinate is outside it (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: DELIBERATELY UNCLAMPED, so a caller sweeping a small block of cells gets a range that clips naturally against jet_grid_hash_cell_range's own bounds test rather than being folded onto an edge cell — which would answer a question about somewhere else. Floor division, so the lattice is continuous across the origin: one unit west of cell 0 is cell -1, never cell 0 again.
 * Example:
 *   int32_t cell_x = jet_grid_hash_cell_x(&grid, body_x);
 */
static inline int32_t jet_grid_hash_cell_x(const JetGridHash* grid, int32_t world_x) {
    return grid && grid->cell_size > 0 ? jet_grid_hash_axis_(world_x, grid->origin_x, grid->cell_size) : 0;
}

/**
 * Function: jet_grid_hash_cell_y
 * Summary: Returns the cell row a world Y falls in, in the grid's own coordinates.
 * Parameters:
 *   - grid: The grid to ask.
 *   - world_y: A world Y coordinate.
 * Returns: The row index, which may be negative or beyond the grid when the coordinate is outside it (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: Unclamped and floor-divided, exactly like jet_grid_hash_cell_x; see its note for why that matters to a block sweep.
 * Example:
 *   int32_t cell_y = jet_grid_hash_cell_y(&grid, body_y);
 */
static inline int32_t jet_grid_hash_cell_y(const JetGridHash* grid, int32_t world_y) {
    return grid && grid->cell_size > 0 ? jet_grid_hash_axis_(world_y, grid->origin_y, grid->cell_size) : 0;
}

// <nocodex>
// The flat index of a cell, or JET_GRID_HASH_NONE when it lies outside the grid.
// The single bounds test the three verbs below share, so "inside the grid" is
// one statement rather than three that could drift. jet_grid_hash_begin holds
// both dimensions to INT32_MAX, so the casts are exact, and every caller has
// already established that `grid` is real.
static inline uint32_t jet_grid_hash_flat_(const JetGridHash* grid, int32_t cell_x, int32_t cell_y) {
    if (cell_x < 0 || cell_y < 0 || cell_x >= (int32_t)grid->cells_x || cell_y >= (int32_t)grid->cells_y) {
        return JET_GRID_HASH_NONE;
    }
    return (uint32_t)cell_y * grid->cells_x + (uint32_t)cell_x;
}

// The flat index of the cell a world position falls in, or JET_GRID_HASH_NONE
// outside the grid. Both passes over the objects ask exactly this, of exactly
// the same coordinates, which is what keeps them in step.
static inline uint32_t jet_grid_hash_flat_at_(const JetGridHash* grid, int32_t world_x, int32_t world_y) {
    return jet_grid_hash_flat_(grid, jet_grid_hash_cell_x(grid, world_x), jet_grid_hash_cell_y(grid, world_y));
}
// </nocodex>

/**
 * Function: jet_grid_hash_tally
 * Summary: Counts one object into its cell — the first of the build's two passes over your objects.
 * Parameters:
 *   - grid: The grid being built.
 *   - world_x: The object's world X.
 *   - world_y: The object's world Y.
 * Returns: true when the object was counted; false when it lies outside the grid or the grid is not tallying.
 * Include: jet_grid_hash.h
 * Notes: An object outside the grid is simply not bucketed, and the later place pass refuses it identically — both passes ask the same question of the same coordinates, which is what keeps them in step. Nothing is stored yet: the tally pass only counts.
 * Example:
 *   for (int i = 0; i < body_count; i++) jet_grid_hash_tally(&grid, body[i].x, body[i].y);
 */
static inline bool jet_grid_hash_tally(JetGridHash* grid, int32_t world_x, int32_t world_y) {
    if (!grid || grid->phase != JET_GRID_HASH_TALLYING) return false;
    const uint32_t cell = jet_grid_hash_flat_at_(grid, world_x, world_y);
    if (cell == JET_GRID_HASH_NONE) return false;
    grid->start[cell]++;
    grid->count++;
    return true;
}

/**
 * Function: jet_grid_hash_place
 * Summary: Returns the slot of YOUR item array this object belongs in — the second of the build's two passes. Write your item there.
 * Parameters:
 *   - grid: The grid being built.
 *   - world_x: The object's world X, the same value it was tallied with.
 *   - world_y: The object's world Y, the same value it was tallied with.
 * Returns: A slot below your item capacity, or JET_GRID_HASH_NONE when the object lies outside the grid, the grid is not placing, or more objects have been placed than were tallied.
 * Include: jet_grid_hash.h
 * Notes: Each cell fills from its end backwards, so walking your objects in the REVERSE of the tally order leaves every cell in your original order (a stable sort); walking forward reverses each cell. Either is a correct broadphase — pick one and keep it, because it is the order your queries will see.
 * Example:
 *   for (int i = body_count - 1; i >= 0; i--) {
 *       uint32_t at = jet_grid_hash_place(&grid, body[i].x, body[i].y);
 *       if (at != JET_GRID_HASH_NONE) bucketed[at] = body[i];
 *   }
 */
static inline uint32_t jet_grid_hash_place(JetGridHash* grid, int32_t world_x, int32_t world_y) {
    if (!grid || grid->phase != JET_GRID_HASH_PLACING) return JET_GRID_HASH_NONE;
    const uint32_t cell = jet_grid_hash_flat_at_(grid, world_x, world_y);
    if (cell == JET_GRID_HASH_NONE) return JET_GRID_HASH_NONE;
    // Two guards against a place pass that does not mirror its tally pass — the
    // one misuse the phases cannot catch. The first bounds the pass as a whole;
    // the second stops a cell handed more objects than it was counted for from
    // decrementing its offset below the start of the array. Neither can fire
    // when both passes run one predicate over one list, which is how a build
    // should be written.
    if (grid->placed >= grid->count || grid->start[cell] == 0u) return JET_GRID_HASH_NONE;
    grid->placed++;
    return --grid->start[cell];
}

/**
 * Function: jet_grid_hash_cell_range
 * Summary: Reports which run of your item array holds one cell's objects — the query. Everything in the cell is a candidate; test the exact distance yourself.
 * Parameters:
 *   - grid: The finished grid to query.
 *   - cell_x: Cell column, typically from jet_grid_hash_cell_x.
 *   - cell_y: Cell row, typically from jet_grid_hash_cell_y.
 *   - first: Receives the index of the cell's first item (0 when there is none).
 *   - count: Receives how many items the cell holds (0 when empty).
 * Returns: true when the cell is inside the finished grid; false when it is outside, or the grid is not built — so a block sweep clips itself against the edges.
 * Include: jet_grid_hash.h
 * Notes: CONSERVATIVE — a cell holds every object bucketed in it, so the run is a SUPERSET of the true neighbours: it may include objects just outside your radius, and never omits one inside it as long as a cell is at least the query diameter. Do the exact test yourself. The run stays valid until the next build.
 * Example:
 *   uint32_t first, count;
 *   if (jet_grid_hash_cell_range(&grid, cell_x, cell_y, &first, &count)) {
 *       for (uint32_t k = 0; k < count; k++) test_against(&bucketed[first + k]);
 *   }
 */
static inline bool jet_grid_hash_cell_range(const JetGridHash* grid, int32_t cell_x, int32_t cell_y,
                                            uint32_t* first, uint32_t* count) {
    if (!first || !count) return false;
    *first = 0u;
    *count = 0u;
    if (!grid || grid->phase != JET_GRID_HASH_BUILT) return false;
    const uint32_t cell = jet_grid_hash_flat_(grid, cell_x, cell_y);
    if (cell == JET_GRID_HASH_NONE) return false;
    *first = grid->start[cell];
    *count = grid->start[cell + 1u] - grid->start[cell];
    return true;
}

/**
 * Function: jet_grid_hash_count
 * Summary: Returns how many objects this build holds.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: Objects tallied into the current build — during the place pass, how many must still be placed in total; after finish, how many the grid holds. 0 for an inert grid.
 * Include: jet_grid_hash.h
 * Notes: Objects outside the grid are never counted, so this is the population of the grid and not of your world. Iterating 0 to count over your item array walks every bucketed object exactly once, in cell order.
 * Example:
 *   for (uint32_t i = 0; i < jet_grid_hash_count(&grid); i++) reset(bucketed[i].slot);
 */
static inline uint32_t jet_grid_hash_count(const JetGridHash* grid) {
    return grid ? grid->count : 0u;
}

/**
 * Function: jet_grid_hash_overflow
 * Summary: Returns how many objects the last build could not hold, over your item capacity.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: The excess over item_capacity that caused a build to be refused; 0 whenever the build was accepted.
 * Include: jet_grid_hash.h
 * Notes: Non-zero means the whole build was refused and every query answers empty — raise the item array (and the capacity passed to init) by at least this much. Read it after jet_grid_hash_prefix_sum if you want to log an undersized grid.
 * Example:
 *   if (jet_grid_hash_overflow(&grid) > 0u) LOG_WARN("broadphase item array undersized");
 */
static inline uint32_t jet_grid_hash_overflow(const JetGridHash* grid) {
    return grid ? grid->overflow : 0u;
}

/**
 * Function: jet_grid_hash_phase
 * Summary: Returns which step of a build the grid is in.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: The current JetGridHashPhase; JET_GRID_HASH_INERT for a NULL or unusable grid.
 * Include: jet_grid_hash.h
 * Notes: Mostly for tests and diagnostics — the build verbs already refuse out of sequence on their own. JET_GRID_HASH_BUILT is the only phase a query can answer from.
 * Example:
 *   assert(jet_grid_hash_phase(&grid) == JET_GRID_HASH_BUILT);
 */
static inline JetGridHashPhase jet_grid_hash_phase(const JetGridHash* grid) {
    return grid ? grid->phase : JET_GRID_HASH_INERT;
}

/**
 * Function: jet_grid_hash_cells_x
 * Summary: Returns the current build's grid width in cells.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: Number of cell columns (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: Fixed by jet_grid_hash_begin for the duration of a build; a later build may choose a different shape.
 * Example:
 *   for (int32_t x = 0; x < (int32_t)jet_grid_hash_cells_x(&grid); x++) sweep_column(x);
 */
static inline uint32_t jet_grid_hash_cells_x(const JetGridHash* grid) {
    return grid ? grid->cells_x : 0u;
}

/**
 * Function: jet_grid_hash_cells_y
 * Summary: Returns the current build's grid height in cells.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: Number of cell rows (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: Fixed by jet_grid_hash_begin for the duration of a build; a later build may choose a different shape.
 * Example:
 *   for (int32_t y = 0; y < (int32_t)jet_grid_hash_cells_y(&grid); y++) sweep_row(y);
 */
static inline uint32_t jet_grid_hash_cells_y(const JetGridHash* grid) {
    return grid ? grid->cells_y : 0u;
}

/**
 * Function: jet_grid_hash_cell_size
 * Summary: Returns the current build's cell size, in world units.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: The width and height of one cell (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: The value passed to jet_grid_hash_begin, exactly — this module never adjusts a shape behind your back; it refuses one it cannot build.
 * Example:
 *   int32_t span = (int32_t)jet_grid_hash_cells_x(&grid) * jet_grid_hash_cell_size(&grid);
 */
static inline int32_t jet_grid_hash_cell_size(const JetGridHash* grid) {
    return grid ? grid->cell_size : 0;
}

/**
 * Function: jet_grid_hash_origin_x
 * Summary: Returns the world X of cell (0, 0)'s low corner.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: The grid's origin on the X axis (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: For a windowed build this is the snapped corner, not the centre you asked for — which is exactly the number a test needs to state where the window actually landed.
 * Example:
 *   bool inside = tile_x >= jet_grid_hash_origin_x(&grid);
 */
static inline int32_t jet_grid_hash_origin_x(const JetGridHash* grid) {
    return grid ? grid->origin_x : 0;
}

/**
 * Function: jet_grid_hash_origin_y
 * Summary: Returns the world Y of cell (0, 0)'s low corner.
 * Parameters:
 *   - grid: The grid to ask.
 * Returns: The grid's origin on the Y axis (0 for an inert grid).
 * Include: jet_grid_hash.h
 * Notes: The snapped corner for a windowed build, exactly as jet_grid_hash_origin_x reports it for the other axis.
 * Example:
 *   bool inside = tile_y >= jet_grid_hash_origin_y(&grid);
 */
static inline int32_t jet_grid_hash_origin_y(const JetGridHash* grid) {
    return grid ? grid->origin_y : 0;
}

#endif // JET_GRID_HASH_H
