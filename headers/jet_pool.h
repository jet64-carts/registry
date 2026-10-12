// runtime/jet_pool.h
//
// An object pool is a box with a fixed number of slots, all one size, for
// things that come and go: bullets, enemies, sparks. You take a slot, fill it
// in, and give it back when the thing is finished with. Taking a slot and
// giving one back each take the same small time however many slots there are,
// and a loop over the pool visits only the slots in use. For a few permanent
// objects, a plain struct or array is usually simpler.
//
// Include "jet_pool.h" yourself; jet64.h does not include it for you.
//
// The pool's memory is reserved in the cartridge's working memory, WRAM. A
// slot is named by its index, a number from 0 up to the capacity. For a
// reference you keep from one frame to the next, use a JetPoolHandle instead:
// a small value that stands for one object, and stays honest even after that
// object is gone. Each slot counts how many times it has been reused, and a
// handle remembers the count it was given, so an old handle to a reused slot
// is refused.
//
// Two words the rules below use. A pool is inert when it has no storage: before
// jet_pool_init has run, or after it failed. An inert pool answers every call
// safely with a failure. Dense iteration is the loop that visits only the slots
// in use, with jet_pool_active_count and jet_pool_active_at, as the example
// shows.
//
// The rules that matter:
//   * Keep the JetPool itself in storage that outlives the objects (normally a
//     static or global variable), then call jet_pool_init exactly once in
//     _init(). It reserves WRAM for the cart's lifetime; there is no destroy
//     call and _end() has nothing to release.
//   * Capacity never grows. Check jet_pool_init's bool result, and check every
//     jet_pool_alloc result against JET_POOL_INVALID_INDEX. A full or inert
//     pool never overwrites a live object.
//   * A newly allocated slot keeps its old bytes. Immediately initialise the
//     whole struct. Pointers returned by jet_pool_get are borrowed from the
//     pool and stop being valid when that slot is freed or the pool is reset.
//   * Use an index only while you know that allocation is still alive. For a
//     reference kept across frames, store a JetPoolHandle and call
//     jet_pool_resolve each time; NULL means that object was removed.
//   * Dense iteration has one trap: jet_pool_free fills the removed position
//     with the last live slot. When freeing inside a loop, do not advance k
//     after a free, as the example shows. Alternatively, collect indices and
//     free them after the loop.
//
// Complete pooled-object example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_pool.h"
//
//   enum { DOT_CAPACITY = 128, DOT_RADIUS = 3, DOT_COLOUR = 12 };
//
//   typedef struct {
//       Vector2 position;
//       float life;
//   } Dot;
//
//   static JetPool dots;
//   static bool dots_ready;
//
//   void _init(void) {
//       dots_ready = jet_pool_init(&dots, DOT_CAPACITY, sizeof(Dot));
//   }
//
//   void _update(float dt) {
//       if (!dots_ready) return;
//
//       if (btnp(BTN_A)) {
//           const uint32_t index = jet_pool_alloc(&dots);
//           if (index != JET_POOL_INVALID_INDEX) {
//               Dot* dot = (Dot*)jet_pool_get(&dots, index);
//               *dot = (Dot){
//                   .position = jet_make_vector2((float)jet_mouse_x(),
//                                                (float)jet_mouse_y()),
//                   .life = 1.0f,
//               };
//           }
//       }
//
//       uint32_t k = 0;
//       while (k < jet_pool_active_count(&dots)) {
//           const uint32_t index = jet_pool_active_at(&dots, k);
//           Dot* dot = (Dot*)jet_pool_get(&dots, index);
//           dot->life -= dt;
//           if (dot->life <= 0.0f) {
//               jet_pool_free(&dots, index); // the slot moved into position k is next
//           } else {
//               k++;
//           }
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       for (uint32_t k = 0; k < jet_pool_active_count(&dots); k++) {
//           const uint32_t index = jet_pool_active_at(&dots, k);
//           const Dot* dot = (const Dot*)jet_pool_get(&dots, index);
//           jet_circfill((int)dot->position.x, (int)dot->position.y,
//                        DOT_RADIUS, DOT_COLOUR);
//       }
//   }
//
//   void _end(void) {
//       // The pool's WRAM lasts for the cart's whole run, so there is nothing to free.
//   }
//
// <nocodex>
//
// A generic, fixed-capacity OBJECT POOL — an OPT-IN runtime module. Like
// jet_pathfind.h and jet_redraw.h it is deliberately NOT pulled in by
// jet64.h: a cart that needs pooling includes "jet_pool.h" itself, and only
// then does the compiler's dependency resolver pull jet_pool.c into the build.
// Carts that never pool pay zero compile cost. This header IS the sandbox
// boundary for the pool; a cart uses only what it exposes.
//
// THE PROBLEM IT REPLACES: today every cart hand-rolls its own pool as a fixed
// array plus a `bool active` flag, and spawning does an O(n) linear scan for the
// first free slot (see carts/asteroids/bullets.c, carts/asteroids/particles.c).
// At bullet-hell spawn rates that O(n)-per-spawn becomes O(n^2) per frame, and
// the pattern is copy-pasted across carts. This module is the one well-crafted
// replacement: O(1) alloc, O(1) free, dense O(active_count) iteration.
//
// THE CAPABILITY IT ADDS: a per-slot GENERATION counter plus packed HANDLES.
// A bare slot index is unsafe to hold across frames — the slot can be freed and
// recycled to a different object, and the stale index silently aliases the new
// one. A handle stamps the index with the generation that was live when it was
// taken; jet_pool_resolve() returns NULL once that slot has been recycled. This
// is exactly what homing weapons and mind-control in "Attack of the Sentient
// Plasmas" need: a safe reference to a target blob that may have been destroyed.
//
// Design notes (the choices that matter):
//   * WRAM bump-allocator backing            → reserved ONCE at init, never freed
//                                              (the allocator has no free), so the
//                                              cost is visible in WRAM telemetry
//                                              and there is no malloc anywhere. One
//                                              reservation holds every array, so a
//                                              pool WRAM cannot hold costs nothing.
//   * Element-type-agnostic byte storage     → pool any struct by passing its
//                                              sizeof; the pool stores raw bytes.
//   * Explicit free-list (stack of indices)  → O(1) alloc/free, never a scan;
//                                              freed slots recycle in LIFO order.
//   * Parity-coded generation per slot        → even = free, odd = live. "Is this
//                                              slot active?" is one bit test, and
//                                              every alloc/free bumps it so a
//                                              recycled slot invalidates old
//                                              handles automatically.
//   * Dense active-index set (sparse-set)    → iterate only LIVE slots in
//                                              O(active_count), cache-friendly,
//                                              while slot indices stay STABLE for
//                                              the lifetime of an allocation.
// Complete pooled-object example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_pool.h"
//
//   enum { DOT_CAPACITY = 128, DOT_RADIUS = 3 };
//   typedef struct { Vector2 position; } Dot;
//   static JetPool dots;
//
//   void _init(void) {
//       jet_pool_init(&dots, DOT_CAPACITY, sizeof(Dot));
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) {
//           const uint32_t index = jet_pool_alloc(&dots);
//           Dot* dot = (Dot*)jet_pool_get(&dots, index);
//           if (dot != NULL) dot->position = jet_make_vector2((float)jet_mouse_x(), (float)jet_mouse_y());
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       for (uint32_t k = 0; k < jet_pool_active_count(&dots); k++) {
//           const Dot* dot = (const Dot*)jet_pool_get(&dots, jet_pool_active_at(&dots, k));
//           jet_circfill((int)dot->position.x, (int)dot->position.y, DOT_RADIUS, 12);
//       }
//   }
//
//   void _end(void) {}
// </nocodex>

#ifndef JET_POOL_H
#define JET_POOL_H

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memset (sandbox-safe)

/**
 * Constant: JET_POOL_INVALID_INDEX
 * Summary: The answer a pool gives when it cannot hand you a real slot index.
 * Include: jet_pool.h
 * Notes: Every real slot index is smaller than the pool's capacity, so this value can never name a slot. Always test what jet_pool_alloc gave you against it before calling jet_pool_get. A full pool answers with this value rather than overwriting a live object.
 * Example:
 *   uint32_t index = jet_pool_alloc(&bullets);
 *   if (index == JET_POOL_INVALID_INDEX) return; // pool full: drop this spawn
 */
#define JET_POOL_INVALID_INDEX ((uint32_t)0xFFFFFFFFu)

// How a JetPoolHandle is packed: the slot index sits in the low 32 bits and
// the generation, the slot's reuse count, in the high 32. Each half has 32
// bits of its own, so neither is ever squeezed, and a generation cannot count
// round to zero again in any real run.
#define JET_POOL_HANDLE_INDEX_BITS 32u

// The bits of a handle that hold the slot index.
#define JET_POOL_HANDLE_INDEX_MASK ((JetPoolHandle)0xFFFFFFFFu)

// The lowest bit of a slot's generation says whether the slot is in use: odd
// means live, even means free. A freshly initialised pool sets every
// generation to zero, so every slot starts free.
#define JET_POOL_LIVE_BIT 1u

/**
 * Type: JetPoolHandle
 * Summary: A reference to one pooled object that stays honest after the object is gone. It packs the slot's index together with the generation the slot had when the handle was taken, so a handle to a slot that has since been freed and reused resolves to NULL instead of quietly pointing at the new occupant.
 * Include: jet_pool.h
 * Notes: A slot's generation is the count of how many times it has been reused; a live slot's generation is odd and never zero, so a live handle is never 0. JET_POOL_HANDLE_NULL (0) is the one "no object" handle and always resolves to NULL. Store handles, not bare indices, whenever a reference must survive from one frame to the next.
 * Example:
 *   JetPoolHandle target = jet_pool_make_handle(&enemies, enemy_index);
 *   Enemy* enemy = (Enemy*)jet_pool_resolve(&enemies, target);
 */
typedef uint64_t JetPoolHandle;

/**
 * Constant: JET_POOL_HANDLE_NULL
 * Summary: The one "no object" value for a JetPoolHandle.
 * Include: jet_pool.h
 * Notes: A live handle always carries an odd, non-zero generation, so it can never equal zero. jet_pool_resolve returns NULL for this value, so a handle field that is still zero, because it was never given a real handle, is safe to resolve.
 * Example:
 *   JetPoolHandle target = JET_POOL_HANDLE_NULL;
 */
#define JET_POOL_HANDLE_NULL ((JetPoolHandle)0)

/**
 * Struct: JetPool
 * Summary: A pool of equal-size slots with a fixed capacity, carved from WRAM once at init. It hands out slot indices that stay put until freed, keeps a free-list so taking a slot and giving one back each take the same small time however many there are, stamps each slot with a generation so handles can be checked, and keeps a packed list of the slots in use so a loop can visit only those.
 * Fields:
 *   - data: The slot bytes, capacity times elem_size of them; slot i starts at data + i * elem_size.
 *   - generations: One generation per slot, capacity of them: even means free, odd means live. It goes up by one on every alloc and every free.
 *   - free_stack: The free-list. The indices of the free slots fill positions 0 up to capacity - active_count; alloc takes the top one and free puts one back on top.
 *   - active_slots: The packed list of the slots in use, valid from position 0 up to active_count. Walk this to visit only live slots.
 *   - active_pos: For each slot, where it sits in active_slots, so a free can swap the last live slot into the gap in one step. It only means something for a live slot.
 *   - capacity: The greatest number of slots, fixed at init.
 *   - elem_size: The size of one slot's element in bytes, fixed at init.
 *   - active_count: How many slots are in use right now. The number of free slots is always capacity - active_count.
 * Include: jet_pool.h
 * Notes: After jet_pool_init the pool reserves no more memory; alloc, free, reset and resolve all work on the same fixed buffers. If WRAM ran out at init the pool is left inert, with a capacity of 0, and alloc always returns JET_POOL_INVALID_INDEX. Do not read or write the fields yourself; use the calls below.
 * Example:
 *   static JetPool bullets;
 *   void _init(void) { jet_pool_init(&bullets, 1024, sizeof(Bullet)); }
 */
typedef struct {
    uint8_t*  data;
    uint32_t* generations;
    uint32_t* free_stack;
    uint32_t* active_slots;
    uint32_t* active_pos;
    uint32_t  capacity;
    uint32_t  elem_size;
    uint32_t  active_count;
} JetPool;

/**
 * Function: jet_make_pool
 * Summary: Gives you an empty, zeroed pool to store. You must still call jet_pool_init on it before use; this only makes a safe value to hold until then.
 * Parameters:
 *   - None.
 * Returns: A zeroed JetPool, returned by value: capacity 0, every pointer NULL.
 * Include: jet_pool.h
 * Notes: A pool like this is inert, and safe to ask questions of: alloc returns JET_POOL_INVALID_INDEX, and free, resolve and get do nothing. So a cart can hold one before _init() has run.
 * Example:
 *   JetPool bullets = jet_make_pool();
 */
static inline JetPool jet_make_pool(void) {
    JetPool p;
    p.data         = NULL;
    p.generations  = NULL;
    p.free_stack   = NULL;
    p.active_slots = NULL;
    p.active_pos   = NULL;
    p.capacity     = 0;
    p.elem_size    = 0;
    p.active_count = 0;
    return p;
}

/**
 * Function: jet_pool_storage_bytes
 * Summary: Tells you how much WRAM a pool of a given capacity and element size will reserve.
 * Parameters:
 *   - capacity: The number of slots you plan to ask for.
 *   - elem_size: The size of one slot's element in bytes.
 * Returns: The bytes jet_pool_init will take for that shape, or 0 when the request makes no sense: a capacity or element size of zero, or a total that would not fit the 64 MiB of WRAM.
 * Include: jet_pool.h
 * Notes: Use it to plan your WRAM budget before you commit. The answer matches the real reservation exactly, including the rounding of each array up to a multiple of 8 bytes and the four bookkeeping arrays.
 * Example:
 *   uint32_t cost = jet_pool_storage_bytes(4096, sizeof(Bullet));
 */
uint32_t jet_pool_storage_bytes(uint32_t capacity, uint32_t elem_size);

/**
 * Function: jet_pool_init
 * Summary: Reserves a pool's slots and bookkeeping from WRAM and makes it ready to hand out slots.
 * Parameters:
 *   - pool: The pool to set up.
 *   - capacity: The greatest number of slots the pool will ever hold. It must be more than 0.
 *   - elem_size: The size of one slot's element in bytes. It must be more than 0; pass sizeof(YourStruct).
 * Returns: true when the pool is ready; false when the request makes no sense or WRAM has run out, in which case the pool is left zeroed and inert and not one byte of WRAM has been taken.
 * Include: jet_pool.h
 * Notes: Call it only from _init(). It reserves from WRAM's bump allocator, which hands memory out but never takes it back, so calling it again and again leaks WRAM. Slot storage is lined up on 8-byte boundaries, the allocator's grain; pass elem_size = sizeof(YourStruct) so the spacing between slots keeps your struct's own alignment. When the reservation fails the pool still behaves: alloc returns JET_POOL_INVALID_INDEX, in the same way that jet_redraw_init's calls do nothing after a failure.
 * Example:
 *   JetPool bullets;
 *   jet_pool_init(&bullets, 4096, sizeof(Bullet));
 */
bool jet_pool_init(JetPool* pool, uint32_t capacity, uint32_t elem_size);

/**
 * Function: jet_pool_alloc
 * Summary: Takes a free slot and gives you its index. It takes the same small time however many slots there are.
 * Parameters:
 *   - pool: The pool to take a slot from.
 * Returns: The index of the slot you now own, smaller than the capacity, or JET_POOL_INVALID_INDEX when the pool is full or inert.
 * Include: jet_pool.h
 * Notes: The slot's old bytes are NOT cleared, so fill in the whole element straight after. The index stays yours until you free the slot; if you need a reference that stays safe across a free that might happen, take a handle with jet_pool_make_handle. A full pool never overwrites a live slot; it returns JET_POOL_INVALID_INDEX and you decide what to drop.
 * Example:
 *   uint32_t i = jet_pool_alloc(&bullets);
 *   if (i != JET_POOL_INVALID_INDEX) { Bullet* b = jet_pool_get(&bullets, i); *b = spawn; }
 */
uint32_t jet_pool_alloc(JetPool* pool);

/**
 * Function: jet_pool_free
 * Summary: Gives a slot back to the pool and bumps its generation, so every handle to it goes stale. It takes the same small time however many slots there are.
 * Parameters:
 *   - pool: The pool the slot belongs to.
 *   - index: The slot to give back, as jet_pool_alloc gave it to you.
 * Returns: true when a live slot was freed; false when the index is out of range or the slot was already free, so freeing twice is caught and does no harm.
 * Include: jet_pool.h
 * Notes: After this a later alloc may hand the same index out for a different object, which is exactly why handles exist.
 *
 *        Freeing inside a loop over the live slots needs care: free moves the last live slot into the gap the freed one left. Either read jet_pool_active_count again each time round and do not advance after a free, or collect the indices to free and free them after the loop.
 * Example:
 *   if (bullet->life <= 0.0f) jet_pool_free(&bullets, i);
 */
bool jet_pool_free(JetPool* pool, uint32_t index);

/**
 * Function: jet_pool_reset
 * Summary: Frees every slot at once, so the pool is as it was just after jet_pool_init.
 * Parameters:
 *   - pool: The pool to empty.
 * Returns: Nothing.
 * Include: jet_pool.h
 * Notes: Every live slot's generation goes up, so every handle taken before the reset resolves to NULL afterwards. The slot bytes are left as they were. Use it when a level restarts, instead of freeing slots one at a time.
 * Example:
 *   jet_pool_reset(&bullets); // wipe the field between waves
 */
void jet_pool_reset(JetPool* pool);

/**
 * Function: jet_pool_get
 * Summary: Gives you a pointer to a slot's element by its index. It takes the same small time however many slots there are.
 * Parameters:
 *   - pool: The pool the slot belongs to.
 *   - index: The slot's index.
 * Returns: A pointer to the slot's elem_size bytes, or NULL if the index is out of range or the pool is inert.
 * Include: jet_pool.h
 * Notes: This is the direct way in: it does NOT check whether the slot is in use, so it is the right call straight after an alloc, and inside a loop over the live slots, where you already know the slot is live. To follow a reference kept from an earlier frame, which might be stale, use jet_pool_resolve on a handle instead.
 * Example:
 *   Bullet* b = (Bullet*)jet_pool_get(&bullets, i);
 */
static inline void* jet_pool_get(const JetPool* pool, uint32_t index) {
    if (!pool || !pool->data || index >= pool->capacity) return NULL;
    return pool->data + (size_t)index * pool->elem_size;
}

/**
 * Function: jet_pool_is_active
 * Summary: Tells you whether a slot is in use right now.
 * Parameters:
 *   - pool: The pool to ask.
 *   - index: The slot's index.
 * Returns: true when the slot is in use; false when it is free, out of range, or the pool is inert.
 * Include: jet_pool.h
 * Notes: It only answers the question; it does not give you the element. Call jet_pool_get once it says true, or use jet_pool_resolve when the reference came from an earlier frame.
 * Example:
 *   if (jet_pool_is_active(&bullets, i)) { ... }
 */
static inline bool jet_pool_is_active(const JetPool* pool, uint32_t index) {
    if (!pool || !pool->generations || index >= pool->capacity) return false;
    return (pool->generations[index] & JET_POOL_LIVE_BIT) != 0u;
}

/**
 * Function: jet_pool_active_count
 * Summary: Tells you how many slots are in use right now.
 * Parameters:
 *   - pool: The pool to ask.
 * Returns: The number of live slots, or 0 for an inert pool.
 * Include: jet_pool.h
 * Notes: It takes the same small time however many slots there are. Use it as the end of a loop that walks the live slots with jet_pool_active_at.
 * Example:
 *   uint32_t n = jet_pool_active_count(&bullets);
 */
static inline uint32_t jet_pool_active_count(const JetPool* pool) {
    return pool ? pool->active_count : 0u;
}

/**
 * Function: jet_pool_active_at
 * Summary: Gives you the slot index of the k-th live slot, so a loop can visit only the objects in use, one after another.
 * Parameters:
 *   - pool: The pool to ask.
 *   - k: A position in the list of live slots, from 0 up to jet_pool_active_count. It is NOT a slot index.
 * Returns: The slot index of the k-th live slot, or JET_POOL_INVALID_INDEX if k is out of range.
 * Include: jet_pool.h
 * Notes: The order of the live slots is not promised and changes as slots are freed, because a free moves the last live slot down into the gap; do not count on it staying the same. The usual loop is for (uint32_t k = 0; k < jet_pool_active_count(p); k++) { uint32_t i = jet_pool_active_at(p, k); ... }. If you free inside the loop, read the note on jet_pool_free.
 * Example:
 *   for (uint32_t k = 0; k < jet_pool_active_count(&bullets); k++) {
 *       Bullet* b = (Bullet*)jet_pool_get(&bullets, jet_pool_active_at(&bullets, k));
 *       bullet_draw(b);
 *   }
 */
static inline uint32_t jet_pool_active_at(const JetPool* pool, uint32_t k) {
    if (!pool || !pool->active_slots || k >= pool->active_count) return JET_POOL_INVALID_INDEX;
    return pool->active_slots[k];
}

/**
 * Function: jet_pool_make_handle
 * Summary: Makes a handle to a live slot, stamped with the slot's current generation, so it can be checked later.
 * Parameters:
 *   - pool: The pool the slot belongs to.
 *   - index: The slot to make a handle for. It must be in use right now.
 * Returns: A JetPoolHandle that jet_pool_resolve will accept until the slot is freed, or JET_POOL_HANDLE_NULL if the index is out of range or the slot is not in use.
 * Include: jet_pool.h
 * Notes: Store the handle, not the bare index, whenever a reference must live across frames in which the object might be destroyed: the target of a homing missile, a mind-control link. Resolve it again each frame.
 * Example:
 *   JetPoolHandle target = jet_pool_make_handle(&enemies, enemy_index);
 */
JetPoolHandle jet_pool_make_handle(const JetPool* pool, uint32_t index);

/**
 * Function: jet_pool_resolve
 * Summary: Gives you a pointer to the object a handle stands for, or NULL if the handle is stale.
 * Parameters:
 *   - pool: The pool the handle was taken from.
 *   - handle: A handle from jet_pool_make_handle, or JET_POOL_HANDLE_NULL.
 * Returns: A pointer to the live element, or NULL if the slot has been freed since (and perhaps reused), the handle is null or was not made by jet_pool_make_handle, or the pool is inert.
 * Include: jet_pool.h
 * Notes: This is the safe way to follow a reference kept from an earlier frame. NULL means the thing you were pointing at is gone. Deal with that (let the missile detonate, break the mind-control link) rather than falling back on jet_pool_get with the bare index, which would give you whatever now lives in that slot.
 * Example:
 *   Enemy* tgt = (Enemy*)jet_pool_resolve(&enemies, missile->target);
 *   if (!tgt) { missile_detonate(missile); }
 */
void* jet_pool_resolve(const JetPool* pool, JetPoolHandle handle);

/**
 * Function: jet_pool_handle_index
 * Summary: Reads the slot index out of a handle, without asking the pool.
 * Parameters:
 *   - handle: A handle from jet_pool_make_handle.
 * Returns: The slot index stored in the handle.
 * Include: jet_pool.h
 * Notes: This only unpacks the number; it does NOT check whether the slot is still in use. Use jet_pool_resolve to follow a handle safely, and this only when you need the bare index of a handle you already know is live.
 * Example:
 *   uint32_t i = jet_pool_handle_index(target);
 */
static inline uint32_t jet_pool_handle_index(JetPoolHandle handle) {
    return (uint32_t)(handle & JET_POOL_HANDLE_INDEX_MASK);
}

/**
 * Function: jet_pool_handle_generation
 * Summary: Reads the generation stamp out of a handle, without asking the pool.
 * Parameters:
 *   - handle: A handle from jet_pool_make_handle.
 * Returns: The generation stored in the handle: odd for a handle that was live when it was taken, 0 for the null handle.
 * Include: jet_pool.h
 * Notes: This is for looking at a handle while you debug; it does not prove the handle still resolves. Use jet_pool_resolve for a safe reference.
 * Example:
 *   uint32_t gen = jet_pool_handle_generation(target);
 */
static inline uint32_t jet_pool_handle_generation(JetPoolHandle handle) {
    return (uint32_t)(handle >> JET_POOL_HANDLE_INDEX_BITS);
}

#endif // JET_POOL_H
