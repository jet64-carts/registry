// runtime/jet_pathfind.h
//
// Finds a route across a grid of tiles, moving up, down, left or right. It uses
// A* (say "A star"): the search grows out from the start tile, always
// extending the most promising tile first, until it reaches the goal. Include
// "jet_pathfind.h" yourself; jet64.h does not include it for you. Your cart
// gives it the grid's width and height, and a quick callback that answers true
// for a tile the actor may step onto and false for a wall.
//
// Keep one JetPathFinder in a static or global variable and set it up once in
// _init(). Its max_nodes must be at least width * height for the largest grid
// it will ever search; check the bool result. The workspace holds only the
// search's own working data, never your map. The map and the callback's data
// stay yours.
//
// A search examines tiles one at a time. Each tile it examines is one
// expansion, and on a large grid the work can be sliced across frames by giving
// each _update() a budget of expansions, so no single frame stalls. Choose the
// search style by how much work it is:
//   * jet_pathfind_next_step or jet_pathfind_route is simplest and completes
//     the whole search immediately. Use it for ordinary small tile maps.
//   * On a large grid, begin a frame-sliced search, call search_step with a
//     bounded expansion budget in later _update() calls, then read its route.
//     Keep the grid, callback, and callback data valid and unchanged until that
//     search finishes. One workspace can run only one search at a time.
//
// Routes are best-effort. If the goal is a wall, or cannot be reached, the
// route leads towards the closest tile the search could reach instead of
// failing outright. jet_pathfind_next_step returns false only when there is no
// move to make, because the actor is already at the goal or is completely
// boxed in.
//
// Complete next-step example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_pathfind.h"
//
//   enum { MAP_WIDTH = 10, MAP_HEIGHT = 8, TILE_SIZE = 24 };
//   static uint8_t walls[MAP_WIDTH * MAP_HEIGHT] = {
//       0,0,0,0,0,0,0,0,0,0,
//       0,0,0,1,1,1,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,0,0,0,0,
//       0,0,0,0,0,0,0,0,0,0
//   };
//   static JetPathFinder pathfinder;
//   static JetPathGrid grid;
//   static JetPathPoint actor = { 1, 1 };
//   static bool pathfinder_ready;
//
//   static bool walkable(int x, int y, void* user) {
//       const uint8_t* cells = (const uint8_t*)user;
//       return cells[y * MAP_WIDTH + x] == 0;
//   }
//
//   void _init(void) {
//       grid = (JetPathGrid){
//           MAP_WIDTH, MAP_HEIGHT, false, walkable, walls
//       };
//       pathfinder_ready = jet_pathfind_init(
//           &pathfinder, MAP_WIDTH * MAP_HEIGHT
//       );
//   }
//
//   void _update(float dt) {
//       const JetPathPoint goal = { MAP_WIDTH - 2, MAP_HEIGHT - 2 };
//       JetPathPoint next;
//       (void)dt;
//       if (pathfinder_ready && btnp(BTN_A) &&
//           jet_pathfind_next_step(
//               &pathfinder, &grid, actor, goal, &next, NULL
//           )) {
//           actor = next;
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       for (int y = 0; y < MAP_HEIGHT; y++) {
//           for (int x = 0; x < MAP_WIDTH; x++) {
//               if (walls[y * MAP_WIDTH + x] != 0) {
//                   jet_rectfill(x * TILE_SIZE, y * TILE_SIZE,
//                                TILE_SIZE, TILE_SIZE, 5);
//               }
//           }
//       }
//       jet_rectfill(actor.x * TILE_SIZE, actor.y * TILE_SIZE,
//                    TILE_SIZE, TILE_SIZE, 12);
//   }
//
//   void _end(void) {
//       // The pathfinder's WRAM lasts for the cart's whole run; nothing is released.
//   }
//
// <nocodex>
//
// A small, fast, GENERAL A* over a uniform 4-connected grid — an OPT-IN runtime
// module. It is deliberately NOT pulled in by jet64.h: a cart that needs to
// route something includes "jet_pathfind.h" itself, and only then does the
// compiler's dependency resolver pull jet_pathfind.c into the build. Carts that
// never path pay zero compile cost. This header IS the sandbox boundary for the
// pathfinder; a cart uses only what it exposes.
//
// Nothing in here knows what a dragon, a ghost, a wall, or a pellet is — the
// grid is described entirely through a passability callback, so the same code
// routes anything on any tile map.
//
// Design notes (the performant-A* choices that matter):
//   * Binary min-heap open set                 → O(log n) push/pop.
//   * Generation-stamped node arrays            → O(1) per-search reset, no
//                                                 memset of the arena each call.
//   * Lazy deletion (re-push on relax)          → no decrease-key bookkeeping.
//   * Manhattan heuristic (wrap-aware)          → admissible & consistent on a
//                                                 unit-cost 4-grid, so the first
//                                                 route found is optimal.
//   * Best-effort result                        → if the goal is unreachable or
//                                                 itself blocked, the search
//                                                 returns the route toward the
//                                                 closest tile it could reach, so
//                                                 AI steering ALWAYS gets a usable
//                                                 lead and never stalls.
//   * Caller-owned WRAM workspace               → the per-node arrays live in a
//                                                 single bump-allocator reservation
//                                                 carved at init, NOT in macro-sized
//                                                 struct fields. The workspace size
//                                                 is therefore a runtime value, so
//                                                 there is no compile-time macro that
//                                                 could give two translation units a
//                                                 different PathFinder layout (the ODR
//                                                 hazard that pinned the cart-local
//                                                 ancestor of this module to a single
//                                                 fixed arena). Reserving from the 64
//                                                 MiB workspace also makes the cost
//                                                 visible in WRAM telemetry.
//   * Resumable / frame-sliced search           → a vast board can be searched a
//                                                 bounded number of node-expansions
//                                                 per frame and resumed next frame,
//                                                 so a long hunt never spikes a single
//                                                 frame. See the search_* API below.
//
// BEGINNER RECIPE:
//   1. Describe the board with JetPathGrid and a passability callback.
//   2. Reserve one JetPathFinder in _init() for the largest board you will use.
//   3. Call jet_pathfind_next_step() when an actor needs its next tile, or use
//      search_begin/step/route when the board is large enough to slice over frames.
//
//   static bool walkable(int x, int y, void* user) {
//       const uint8_t* walls = (const uint8_t*)user;
//       return walls[y * MAP_WIDTH + x] == 0;
//   }
//   static JetPathFinder s_pathfinder;
//   static JetPathGrid s_grid = { MAP_WIDTH, MAP_HEIGHT, false, walkable, s_walls };
//   void _init(void) {
//       jet_pathfind_init(&s_pathfinder, MAP_WIDTH * MAP_HEIGHT);
//   }
//   // Later: jet_pathfind_next_step(&s_pathfinder, &s_grid, here, goal, &next, NULL);
// Complete next-step example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_pathfind.h"
//
//   enum { MAP_WIDTH = 10, MAP_HEIGHT = 8, TILE_SIZE = 24 };
//   static uint8_t walls[MAP_WIDTH * MAP_HEIGHT] = {
//       0,0,0,0,0,0,0,0,0,0,
//       0,0,0,1,1,1,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,1,0,0,0,
//       0,0,0,0,0,0,0,0,0,0,
//       0,0,0,0,0,0,0,0,0,0
//   };
//   static JetPathFinder pathfinder;
//   static JetPathGrid grid;
//   static JetPathPoint actor = { 1, 1 };
//
//   static bool walkable(int x, int y, void* user) {
//       const uint8_t* cells = (const uint8_t*)user;
//       return cells[y * MAP_WIDTH + x] == 0;
//   }
//
//   void _init(void) {
//       grid = (JetPathGrid){ MAP_WIDTH, MAP_HEIGHT, false, walkable, walls };
//       jet_pathfind_init(&pathfinder, MAP_WIDTH * MAP_HEIGHT);
//   }
//
//   void _update(float dt) {
//       const JetPathPoint goal = { MAP_WIDTH - 2, MAP_HEIGHT - 2 };
//       JetPathPoint next;
//       (void)dt;
//       if (btnp(BTN_A) && jet_pathfind_next_step(&pathfinder, &grid, actor, goal, &next, NULL)) {
//           actor = next;
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_rectfill(actor.x * TILE_SIZE, actor.y * TILE_SIZE, TILE_SIZE, TILE_SIZE, 12);
//       jet_print("PRESS A FOR NEXT STEP", 16, MAP_HEIGHT * TILE_SIZE + 8, 15);
//   }
//
//   void _end(void) {}
// </nocodex>

#ifndef JET_PATHFIND_H
#define JET_PATHFIND_H

#include "jet_core.h"   // IWYU pragma: keep fixed-width ints / bool / memset (sandbox-safe)
#include "jet_memory.h" // IWYU pragma: keep jet_memory_reserve, the WRAM workspace allocator

/**
 * Struct: JetPathPoint
 * Summary: One tile position on a pathfinding grid, as whole numbers.
 * Fields:
 *   - x: The column, counted in tiles from 0 to grid.width - 1.
 *   - y: The row, counted in tiles from 0 to grid.height - 1.
 * Include: jet_pathfind.h
 * Notes: These name tiles, not pixels. A route you get back includes both its start and its end, so when the route has at least two points, route[1] is the first step to take.
 * Example:
 *   JetPathPoint start = { 3, 7 };
 *   JetPathPoint goal = { 18, 4 };
 */
typedef struct { int x, y; } JetPathPoint;

/**
 * Callback: JetPathPassableFn
 * Summary: Your function that says whether the actor may stand on one tile.
 * Parameters:
 *   - x: The tile's column. It is always inside the grid.
 *   - y: The tile's row. It is always inside the grid.
 *   - user: The pointer you stored in JetPathGrid.user, passed through unchanged.
 * Returns: true when the tile may be entered; false when it is blocked.
 * Include: jet_pathfind.h
 * Notes: The pathfinder calls this many times in one search, so keep it quick, and do no drawing and reserve no memory inside it. Pass your map, and any rules that differ from one actor to another, through user. Both the callback and user must stay valid until a frame-sliced search has finished. The start tile is taken to be standable and is never asked about.
 * Example:
 *   static bool walkable(int x, int y, void* user) {
 *       const uint8_t* walls = (const uint8_t*)user;
 *       return walls[y * MAP_WIDTH + x] == 0;
 *   }
 */
typedef bool (*JetPathPassableFn)(int x, int y, void* user);

/**
 * Struct: JetPathGrid
 * Summary: Describes the grid of tiles, and names the callback that decides which tiles can be walked on.
 * Fields:
 *   - width: The grid's width in tiles. It must be more than 0.
 *   - height: The grid's height in tiles. It must be more than 0.
 *   - wrap_x: true to join the left and right edges, so an actor can walk off one side and appear on the other, like a Pac-Man tunnel; false for solid edges on the left and right.
 *   - passable: The callback that says whether a tile may be entered. It must not be NULL.
 *   - user: A pointer of yours, passed to passable unchanged. It may be NULL when the callback needs nothing.
 * Include: jet_pathfind.h
 * Notes: The top and bottom edges never join. The struct itself is copied when a search begins, but passable and user must stay valid for the whole of a frame-sliced search. width * height must fit within the capacity you gave jet_pathfind_init.
 * Example:
 *   JetPathGrid grid = { MAP_WIDTH, MAP_HEIGHT, true, walkable, walls };
 */
typedef struct {
    int                width;
    int                height;
    bool               wrap_x;
    JetPathPassableFn passable;
    void*              user;
} JetPathGrid;

/**
 * Enum: JetPathStatus
 * Summary: Says whether a frame-sliced search needs more work, has reached the goal, or could only find a route to the closest tile it reached.
 * Include: jet_pathfind.h
 * Notes: JET_PATH_PENDING means call jet_pathfind_search_step again in a later update. JET_PATH_FOUND means the route reaches the goal. JET_PATH_UNREACHABLE still lets you call jet_pathfind_search_route: it gives a best-effort route towards the closest tile the search reached.
 * Example:
 *   JetPathStatus status = jet_pathfind_search_step(&pathfinder, 320);
 *   if (status != JET_PATH_PENDING) route_len = jet_pathfind_search_route(&pathfinder, route, ROUTE_CAP);
 */
typedef enum {
    JET_PATH_PENDING = 0,   // the budget for this call ran out before the search
                             // finished; call jet_pathfind_search_step() again.
    JET_PATH_FOUND,         // a shortest route to the goal is ready.
    JET_PATH_UNREACHABLE,   // the search ran out of tiles to try without reaching
                             // the goal; a best-effort route towards the closest
                             // tile it reached is ready, and is still a good lead.
} JetPathStatus;

/**
 * Struct: JetPathDebug
 * Summary: An optional record, kept by you, of the tiles one search examined, for finding problems and for drawing the search on the screen.
 * Fields:
 *   - visited: An array of yours that receives the examined tiles in the order they were examined. It may be NULL to count them without storing where they were.
 *   - cap: How many JetPathPoint entries visited has room for. Use 0 when visited is NULL.
 *   - count: How many tiles have been examined so far. Only the first cap are stored, but the count keeps going, so it always says how much work was done.
 *   - found: true only when the search reached the goal itself; false for a best-effort route.
 *   - cost: The number of steps from the start to the last tile of the route that was given back.
 * Include: jet_pathfind.h
 * Notes: Pass NULL instead of a JetPathDebug and nothing is recorded, at no cost. The pathfinder clears these fields when a search begins and adds to them across the steps of a frame-sliced search; do not change them while that search is running.
 * Example:
 *   static JetPathPoint visited[512];
 *   JetPathDebug debug = { visited, 512, 0, false, 0 };
 */
typedef struct {
    JetPathPoint* visited;   // caller-owned buffer of `cap` points (may be NULL)
    int            cap;
    int            count;     // tiles expanded this search (clamped to cap on store)
    bool           found;     // true when an exact route to the goal existed
    int            cost;      // step-cost of the returned route's end tile
} JetPathDebug;

/**
 * Struct: JetPathFinder
 * Summary: The working memory for A* searches, reserved in WRAM and used again and again; it runs one search at a time.
 * Fields:
 *   - node_cap: The greatest number of tiles a search may cover, fixed by jet_pathfind_init.
 *   - heap_cap: The room in the list of tiles waiting to be examined; the module manages it.
 *   - stamp: One generation stamp per tile, in WRAM; the module's own working state.
 *   - g: The best cost found so far to each tile, in WRAM; the module's own working state.
 *   - came: The tile each tile was reached from, in WRAM; the module's own working state.
 *   - closed: A flag per tile, set once the search has finished with that tile, in WRAM; the module's own working state.
 *   - heap: The tiles waiting to be examined, in WRAM; the module's own working state.
 *   - heap_f: The priority of each waiting tile, in WRAM; the module's own working state.
 *   - heap_len: How many tiles are waiting right now.
 *   - generation: The search's generation number. Bumping it clears the workspace in one step, however big it is.
 *   - grid: A copy of the JetPathGrid the current search runs over.
 *   - goal_node: The goal tile as one number: its row times the width, plus its column.
 *   - gx: The goal's x, kept for the distance guess.
 *   - gy: The goal's y, kept for the distance guess.
 *   - start_node: The start tile as one number, in the same way as goal_node.
 *   - best_node: The tile reached so far that is closest to the goal.
 *   - best_h: The guessed distance from best_node to the goal.
 *   - found: true once the goal itself has been reached.
 *   - active: true while a frame-sliced search is still pending.
 *   - dbg: The JetPathDebug the current search is recording into, or NULL.
 * Include: jet_pathfind.h
 * Notes: Call jet_pathfind_init once from _init; its WRAM lasts for the cart's whole run and cannot be given back. One workspace can serve any number of searches one after another, but never two at once: beginning a new search abandons any search still running in that workspace. Treat every field as read-only; they are the module's own working state.
 * Example:
 *   static JetPathFinder pathfinder;
 *   void _init(void) {
 *       if (!jet_pathfind_init(&pathfinder, MAP_WIDTH * MAP_HEIGHT)) LOG_ERROR("Pathfinder needs more WRAM");
 *   }
 */
typedef struct {
    // --- caller-owned arena (one WRAM reservation, carved into sub-arrays) -----
    uint32_t  node_cap;   // capacity in nodes (>= a search grid's width*height)
    uint32_t  heap_cap;   // open-set capacity (node_cap * 4; see .c for why)
    uint32_t* stamp;      // [node_cap] last generation that touched a node
    uint16_t* g;          // [node_cap] best cost-so-far to the node
    int32_t*  came;       // [node_cap] predecessor node index (-1 = start)
    uint8_t*  closed;     // [node_cap] finalised flag (stamp-gated)
    int32_t*  heap;       // [heap_cap] binary min-heap of open node indices
    uint16_t* heap_f;     // [heap_cap] priority cached parallel to heap[]
    int       heap_len;
    uint32_t  generation; // bumped once per search

    // --- live search state, valid between begin() and resolution --------------
    JetPathGrid   grid;        // copy of the grid this search runs over
    int            goal_node;   // flattened goal index
    int            gx, gy;      // goal tile (cached for the heuristic)
    int            start_node;  // flattened start index
    int            best_node;   // closest-to-goal node reached (best-effort end)
    int            best_h;      // its heuristic distance to the goal
    bool           found;       // an exact route to the goal was reached
    bool           active;      // a search is mid-flight (resumable)
    JetPathDebug* dbg;         // capture sink for the live search (may be NULL)
} JetPathFinder;

/**
 * Function: jet_pathfind_storage_bytes
 * Summary: Tells you how much WRAM a workspace of a given capacity will reserve.
 * Parameters:
 *   - max_nodes: The capacity you plan to ask for, in tiles. It must be at least the width * height of the largest grid you will search.
 * Returns: The bytes jet_pathfind_init() will take for that capacity, or 0 when max_nodes is 0 or the workspace would be bigger than the 64 MiB of WRAM.
 * Include: jet_pathfind.h
 * Notes: Use it to plan your budget against the 64 MiB of WRAM before you commit. The answer matches the real reservation exactly.
 * Example:
 *   uint32_t cost = jet_pathfind_storage_bytes(map.width * map.height);
 */
uint32_t jet_pathfind_storage_bytes(uint32_t max_nodes);

/**
 * Function: jet_pathfind_init
 * Summary: Reserves a search workspace from WRAM and makes it ready to search.
 * Parameters:
 *   - pf: The workspace to set up.
 *   - max_nodes: The capacity in tiles. It must be at least the width * height of the largest grid the cart will search.
 * Returns: true when the workspace is ready; false when max_nodes is 0 or WRAM has run out, in which case the workspace is left zeroed and inert and not one byte of WRAM has been taken.
 * Include: jet_pathfind.h
 * Notes: Call it only from _init(). It reserves from WRAM's bump allocator, which hands memory out but never takes it back, just as jet_map_init() does. A refused reservation takes nothing, and the console says how much was asked for and how much was left.
 * Example:
 *   JetPathFinder pf;
 *   jet_pathfind_init(&pf, map.width * map.height);
 */
bool jet_pathfind_init(JetPathFinder* pf, uint32_t max_nodes);

/**
 * Function: jet_pathfind_route
 * Summary: Finds a shortest route from start towards goal in one go, running the whole search now.
 * Parameters:
 *   - pf: A workspace set up by jet_pathfind_init.
 *   - grid: The grid to search. It is copied, so you keep yours.
 *   - start: The start tile. It is taken to be standable; the passable callback is never asked about it.
 *   - goal: The goal tile.
 *   - out: Your array to receive the route, in walking order, INCLUDING both the start and the end.
 *   - out_cap: How many points out has room for. When the real route is longer, you get the part that begins at the start, which is still a good lead.
 *   - dbg: A JetPathDebug to record into, or NULL.
 * Returns: The number of points written, at least 1 whenever the search could run, or 0 when out_cap is 0 or less or the grid is larger than the workspace.
 * Include: jet_pathfind.h
 * Notes: Routes are best-effort: a goal that is a wall or cannot be reached gives the route towards the closest tile the search reached, so an actor always has somewhere to go. This is the simple call for most games; for a very large grid use the frame-sliced jet_pathfind_search_begin, jet_pathfind_search_step and jet_pathfind_search_route instead.
 * Example:
 *   JetPathPoint route[256];
 *   int n = jet_pathfind_route(&pf, &grid, start, goal, route, 256, NULL);
 */
int jet_pathfind_route(JetPathFinder* pf, const JetPathGrid* grid,
                        JetPathPoint start, JetPathPoint goal,
                        JetPathPoint* out, int out_cap, JetPathDebug* dbg);

/**
 * Function: jet_pathfind_next_step
 * Summary: Gives you just the next tile to step onto on the way to a goal.
 * Parameters:
 *   - pf: A workspace set up by jet_pathfind_init.
 *   - grid: The grid to search. It is copied, so you keep yours.
 *   - start: The tile the actor is on now.
 *   - goal: The goal tile.
 *   - next: Receives the first tile to step onto along the route towards goal.
 *   - dbg: A JetPathDebug to record into, or NULL.
 * Returns: true when a move was written; false when there is no move to make, because the actor is already at the goal or is completely boxed in.
 * Include: jet_pathfind.h
 * Notes: It runs a whole search now. The point it writes is route[1], never the tile the actor is on. If a large grid could make that too slow for one update, use the frame-sliced calls instead.
 * Example:
 *   JetPathPoint step;
 *   if (jet_pathfind_next_step(&pf, &grid, here, target, &step, NULL)) actor_move_to(step);
 */
bool jet_pathfind_next_step(JetPathFinder* pf, const JetPathGrid* grid,
                             JetPathPoint start, JetPathPoint goal,
                             JetPathPoint* next, JetPathDebug* dbg);

/**
 * Function: jet_pathfind_search_begin
 * Summary: Starts a frame-sliced search towards a goal, for very large grids, that you then move on a little at a time.
 * Parameters:
 *   - pf: A workspace set up by jet_pathfind_init.
 *   - grid: The grid to search. It is copied, so you keep yours.
 *   - start: The start tile.
 *   - goal: The goal tile. The search's distance guess is fixed on this goal for as long as the search lasts.
 *   - dbg: A JetPathDebug to record into, or NULL. It is cleared here, then added to across the steps.
 * Returns: JET_PATH_PENDING normally, or JET_PATH_UNREACHABLE straight away when the grid does not fit the workspace, in which case no search runs.
 * Include: jet_pathfind.h
 * Notes: One whole search can examine thousands of tiles in a single frame. These three calls spread that work over frames: begin here, then move on with jet_pathfind_search_step() each frame, and read the current best route with jet_pathfind_search_route whenever you like. For all the frames the search lasts, the grid's passable callback and its user pointer must stay valid and describe a grid that does not change. When the goal moves (a target that runs away, for example), finish or abandon this search and begin a new one, rather than changing the goal while the search is running.
 * Example:
 *   jet_pathfind_search_begin(&pf, &grid, here, target, &dbg);
 */
JetPathStatus jet_pathfind_search_begin(JetPathFinder* pf, const JetPathGrid* grid,
                                          JetPathPoint start, JetPathPoint goal,
                                          JetPathDebug* dbg);

/**
 * Function: jet_pathfind_search_step
 * Summary: Moves the running frame-sliced search on by a limited number of expansions, one expansion being one tile examined.
 * Parameters:
 *   - pf: A workspace with a search begun by jet_pathfind_search_begin().
 *   - max_expansions: How many tiles this call may examine. Pass 0 for no limit, which runs the search to the end now.
 * Returns: Where the search stands: JET_PATH_PENDING (the budget ran out; call again), JET_PATH_FOUND, or JET_PATH_UNREACHABLE.
 * Include: jet_pathfind.h
 * Notes: Safe to call after the search has finished; it gives the final status without doing any work. While the search is still pending, jet_pathfind_search_route gives a best-effort lead towards the goal, so an actor can start moving at once and correct its course as the search finishes.
 * Example:
 *   JetPathStatus st = jet_pathfind_search_step(&pf, 320); // up to 320 tiles this frame
 */
JetPathStatus jet_pathfind_search_step(JetPathFinder* pf, int max_expansions);

/**
 * Function: jet_pathfind_search_route
 * Summary: Writes the frame-sliced search's current best route into your array.
 * Parameters:
 *   - pf: A workspace with a search begun, whether still pending or finished.
 *   - out: Your array to receive the route, in walking order, INCLUDING both the start and the end.
 *   - out_cap: How many points out has room for. When the route is longer, you get the part that begins at the start.
 * Returns: The number of points written, at least 1 for any search that has begun, or 0 if no search has been begun or out_cap is 0 or less.
 * Include: jet_pathfind.h
 * Notes: Once the status is JET_PATH_FOUND this is the shortest route to the goal; before that, it is the best-effort lead towards it.
 * Example:
 *   JetPathPoint route[512];
 *   int n = jet_pathfind_search_route(&pf, route, 512);
 */
int jet_pathfind_search_route(const JetPathFinder* pf, JetPathPoint* out, int out_cap);

/**
 * Function: jet_pathfind_search_active
 * Summary: Tells you whether a frame-sliced search is still running.
 * Parameters:
 *   - pf: The workspace to ask.
 * Returns: true while a search has begun and not yet finished; false otherwise.
 * Include: jet_pathfind.h
 * Notes: false covers both a finished search and a workspace that never began one. Keep the JetPathStatus that search_begin or search_step gave you when you need to tell those two apart.
 * Example:
 *   if (!jet_pathfind_search_active(&pf)) jet_pathfind_search_begin(&pf, &grid, here, target, &dbg);
 */
bool jet_pathfind_search_active(const JetPathFinder* pf);

#endif // JET_PATHFIND_H
