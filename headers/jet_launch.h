// runtime/jet_launch.h
#ifndef JET_LAUNCH_H
#define JET_LAUNCH_H

#include "jet_core.h" // IWYU pragma: keep — fixed-width ints, bool, the LAUNCH block geometry

// What the player was started with: the values on the end of the link that
// opened it. A web page opened as
//
//     https://play.example.com/?join=7Q4K&name=ACE
//
// was launched with two values, join = 7Q4K and name = ACE. A desktop player
// started as `mygame join=7Q4K name=ACE` (or `--join=7Q4K`) was launched with the
// same two. jet_launch_value looks one up by its key, so a link someone pasted
// into a chat can open the game straight into a room, a level or a replay.
//
// Include "jet_launch.h" yourself; jet64.h does not include it for you.
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware. The
// cart resolver compiles jet_launch.c only when a cart's #include tree reaches
// this header, and the exporter keys the players' launch reading off the very
// same signal, so a cart that never asks carries none of it. The host decodes the
// link once, before the cart's first instruction, into the LAUNCH block
// (jet_core.h); this module only reads that table, so a lookup costs a scan of at
// most JET_LAUNCH_TABLE_BYTES and never a round trip to the host.
// </nocodex>
//
// Three rules come with it.
//
//   1. ANYONE CAN WRITE A LINK. A launch value is typed by whoever made the link,
//      which may be a stranger, so treat it as you would a player's typing:
//      check its length, check its characters, and refuse what you do not
//      expect. Never use one as a file name or a host to connect to without
//      checking it against a list of your own.
//   2. VALUES ARRIVE DECODED. `?name=ACE%20PILOT` and `?name=ACE+PILOT` both
//      arrive as "ACE PILOT", exactly as the browser itself would read them. A key
//      given twice is the first one given; a key given with no value (`?debug`)
//      is present and empty.
//   3. THE WORKSTATION HAS ITS OWN. Set `launch: "join=7Q4K"` in the project's
//      project.ron to try a link's path while you build, or give the agent door's
//      run.play a `launch` string. Exported players read the real link.
//
// The table holds at most JET_LAUNCH_MAX_BYTES of keys and values together;
// a link longer than that keeps the values that fit, first to last.
//
// Complete join-by-link example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_launch.h"
//
//   enum { ROOM_CODE_CAPACITY = 8 };
//   static char room[ROOM_CODE_CAPACITY];
//   static bool joining;
//
//   // A room code is four letters or digits. Anything else is somebody's typo,
//   // or somebody's mischief, and is ignored.
//   static bool room_code_ok(const char* code, int len) {
//       if (len != 4) return false;
//       for (int i = 0; i < len; i++) {
//           const char c = code[i];
//           if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
//       }
//       return true;
//   }
//
//   void _init(void) {
//       const int len = jet_launch_value("join", room, sizeof room);
//       joining = len > 0 && room_code_ok(room, len);
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       char line[48];
//       jet_cls(0);
//       if (joining) {
//           snprintf(line, sizeof line, "JOINING ROOM %s", room);
//       } else {
//           snprintf(line, sizeof line, "%u LAUNCH VALUES", (unsigned)jet_launch_count());
//       }
//       jet_print(line, 8, 8, 15);
//   }
//
//   void _end(void) {}

/**
 * Constant: JET_LAUNCH_MAX_BYTES
 * Summary: The most bytes of launch keys and values together a player keeps, each key and each value counting one more for its terminator.
 * Include: jet_launch.h
 * Notes: About two thousand characters: room for any link a person would paste. A launch past it keeps the values that fit, in the order they were given, and drops the rest whole — never half a value. No single value can be longer, so a buffer of this size holds any of them.
 * Example:
 *   char value[JET_LAUNCH_MAX_BYTES];
 */
#define JET_LAUNCH_MAX_BYTES JET_LAUNCH_TABLE_BYTES

/**
 * Enum: JetLaunchResult
 * Summary: The two answers jet_launch_value and jet_launch_key give instead of a length.
 * Include: jet_launch.h
 * Notes: Both are negative, so `if (len >= 0)` reads as "found and copied". JET_LAUNCH_ABSENT is no error: most players are launched with nothing at all.
 * Example:
 *   if (jet_launch_value("join", code, sizeof code) == JET_LAUNCH_ABSENT) show_menu();
 */
typedef enum {
    JET_LAUNCH_ABSENT    = -1, // no value has that key (or no entry has that index)
    JET_LAUNCH_TOO_SMALL = -2, // the value is longer than the buffer; nothing was copied
} JetLaunchResult;

/**
 * Function: jet_launch_value
 * Summary: Copies the launch value with the given key into a buffer, and answers its length.
 * Parameters:
 *   - key: The key to look up, such as "join". Keys are matched exactly, capitals included.
 *   - out: Where to copy the value, as a NUL-terminated string. May be anywhere.
 *   - cap: The size of out in bytes, its terminator included.
 * Returns: The value's length (0 for a key given with no value), JET_LAUNCH_ABSENT when the player was not launched with that key, or JET_LAUNCH_TOO_SMALL when the value and its terminator do not fit in cap bytes — in which case out is set to the empty string.
 * Include: jet_launch.h
 * Notes: The value is exactly what the person who made the link typed, decoded: check it before you trust it. It never changes while the cart runs, so read it once, in _init.
 * Example:
 *   char code[8];
 *   if (jet_launch_value("join", code, sizeof code) > 0) lobby_join(code);
 */
int jet_launch_value(const char* key, char* out, uint32_t cap);

/**
 * Function: jet_launch_has
 * Summary: Reports whether the player was launched with the given key, with or without a value.
 * Parameters:
 *   - key: The key to look for, matched exactly.
 * Returns: true when the key was given; false when it was not.
 * Include: jet_launch.h
 * Notes: The test for a switch such as `?attract`, which carries no value: jet_launch_value answers 0 for it, which a buffer cannot tell from an empty value.
 * Example:
 *   s_attract_mode = jet_launch_has("attract");
 */
bool jet_launch_has(const char* key);

/**
 * Function: jet_launch_count
 * Summary: How many values the player was launched with.
 * Parameters:
 *   - None.
 * Returns: The number of entries, 0 when it was launched with nothing.
 * Include: jet_launch.h
 * Notes: With jet_launch_key, lets a cart list everything it was given — useful for a debug screen, and for a game that takes free-form options.
 * Example:
 *   for (uint32_t i = 0; i < jet_launch_count(); i++) { ... }
 */
uint32_t jet_launch_count(void);

/**
 * Function: jet_launch_key
 * Summary: Copies the key of one launch entry, by its index, into a buffer.
 * Parameters:
 *   - index: Which entry, from 0 up to jet_launch_count() - 1, in the order they were given.
 *   - out: Where to copy the key, as a NUL-terminated string.
 *   - cap: The size of out in bytes, its terminator included.
 * Returns: The key's length, JET_LAUNCH_ABSENT for an index past the last entry, or JET_LAUNCH_TOO_SMALL when it does not fit (out is then the empty string).
 * Include: jet_launch.h
 * Notes: Pass the key to jet_launch_value for its value. Keys appear once each: a key given twice keeps its first value only.
 * Example:
 *   char key[32], value[64];
 *   if (jet_launch_key(0, key, sizeof key) >= 0) jet_launch_value(key, value, sizeof value);
 */
int jet_launch_key(uint32_t index, char* out, uint32_t cap);

#endif // JET_LAUNCH_H
