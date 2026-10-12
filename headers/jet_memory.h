// runtime/jet_memory.h
#ifndef JET_MEMORY_H
#define JET_MEMORY_H

#include <stdint.h>

// Reserve big blocks of the cartridge's working memory, WRAM. There is 64 MiB
// of it. The allocator is the simplest kind there is, a bump allocator: each
// block you ask for is handed out from the next free spot, one after another,
// and nothing is ever given back. Memory you reserve here is yours for the
// cartridge's whole life. That is fine for a game, because a game knows at
// the start what it needs: reserve your arrays and your state once in _init,
// check that the pointer is not NULL, and keep it for the whole run. The
// count goes back to zero each time the cartridge starts. Never reserve in
// _update or _draw; a reservation every frame would use up all of WRAM in
// the end. The usage calls are cheap, so a debug overlay may read them every
// frame.
//
// Complete persistent-WRAM example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { POINT_COUNT = 1024 };
//   static Vector2* points;
//
//   void _init(void) {
//       points = (Vector2*)jet_memory_reserve(sizeof(*points) * POINT_COUNT);
//       if (points != NULL) memset(points, 0, sizeof(*points) * POINT_COUNT);
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       char usage[48];
//       snprintf(usage, sizeof usage, "WRAM %.2f MiB", jet_memory_usage_mb());
//       jet_cls(0);
//       jet_print(points != NULL ? usage : "WRAM RESERVATION FAILED", 16, 16, 15);
//   }
//
//   void _end(void) {}

// How much WRAM there is. This must stay equal to the host's own figure.
#define JET_WRAM_CAPACITY_BYTES (64 * 1024 * 1024)

/**
 * Function: _init_memory_engine
 * Summary: Initializes the WRAM bump allocator, explicitly resetting it to 0.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_memory.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Called by the runtime before _init(); cartridge code must not call it. It resets allocator bookkeeping, not the bytes already held in WRAM, so Warm Boot data remains available when the cart deliberately reuses the same allocation order.
 */
void _init_memory_engine(void);

/**
 * Function: jet_memory_reserve
 * Summary: Reserves one unbroken block of the cartridge's working memory, WRAM, and gives you a pointer to it.
 * Parameters:
 *   - size: How many bytes you want.
 * Returns: A pointer to the block, or NULL if there is not enough WRAM left for it (or if it is bigger than the whole 64 MiB).
 * Include: jet_memory.h
 * Notes: Call it only from _init. The block is yours for the rest of the run; there is no way to give it back, and nothing is ever freed. Each size is rounded up to a multiple of 8 bytes. Because blocks are handed out one after another, the same order of calls in _init gives the same pointers every time, which is what lets a Warm Boot keep your data where it was.
 * Example:
 *   void* buffer = jet_memory_reserve(2048);
 */
void* jet_memory_reserve(uint32_t size);

/**
 * Function: jet_memory_usage_bytes
 * Summary: Tells you how many bytes of WRAM have been reserved so far.
 * Parameters:
 *   - None.
 * Returns: The total reserved, in bytes.
 * Include: jet_memory.h
 * Notes: It reads the same host register the workstation reads to draw its WRAM meter, so the two always agree.
 * Example:
 *   uint32_t used = jet_memory_usage_bytes();
 */
uint32_t jet_memory_usage_bytes(void);

/**
 * Function: jet_memory_usage_pct
 * Summary: Tells you what share of WRAM has been reserved, as a fraction.
 * Parameters:
 *   - None.
 * Returns: A number from 0.0f (nothing reserved) to 1.0f (all of it reserved).
 * Include: jet_memory.h
 * Notes: Despite the name, the answer is a fraction, not a percentage; 1.0f means WRAM is completely full. Multiply by 100 for a percentage.
 * Example:
 *   float ratio = jet_memory_usage_pct();
 */
float jet_memory_usage_pct(void);

/**
 * Function: jet_memory_usage_mb
 * Summary: Tells you how much WRAM has been reserved, in MiB (one MiB is 1024 x 1024 bytes).
 * Parameters:
 *   - None.
 * Returns: The total reserved, in MiB.
 * Include: jet_memory.h
 * Example:
 *   float mb = jet_memory_usage_mb();
 */
float jet_memory_usage_mb(void);

#endif // JET_MEMORY_H
