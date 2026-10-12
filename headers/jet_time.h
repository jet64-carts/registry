// runtime/jet_time.h
#ifndef JET_TIME_H
#define JET_TIME_H

#include <stdbool.h>
#include <stdint.h>

// The host's clocks: the machine's own date and time, and a stopwatch. This
// header is not part of jet64.h; include it yourself when you want a clock.
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_pcm.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_time.c only when
// a cart's #include tree reaches this header.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_time.h"   // you must include this one yourself
//
// Two clocks, two jobs.
//
// THE CALENDAR reads the real date and time on the machine the cart is
// running on. jet_time_local gives you the local time broken into parts
// (year, month, day, weekday, hour, minute and second) for a clock on the
// screen, a day-and-night mood, or an event on a calendar date.
// jet_time_epoch gives you the same moment as one number, the seconds since
// the start of 1970, for stamping a save file or measuring how long it has
// been since the player last visited.
//
// THE STOPWATCH (jet_time_micros, jet_time_millis and jet_time_seconds)
// starts at zero the first time it is read and only ever counts up, to the
// microsecond. Use it to measure how long something really took: a stretch
// of code, a load, a message across the network and back, or the time on a
// speedrun clock. Unlike the calendar it never jumps: daylight saving, a
// change of time zone, and the player changing the machine's clock cannot
// touch it.
//
// Both clocks are inputs, never metronomes. They read differently on every
// machine, so a game that moved by them would run differently on every
// machine. Move your gameplay and animation from _update and its dt, which
// ticks 120 times a second and is the same on every host, and treat these
// calls like a gamepad: read them, show them, react to them.
//
// <nocodex>
// All three runtimes serve the same verbs: the workstation guest and the
// native player read the host C library directly (clock_gettime on POSIX,
// QueryPerformanceCounter on Windows for the stopwatch), while the web player
// fills JetDateTime through the jet_web_local_time host shim and the stopwatch
// through jet_web_perf_micros over performance.now() — the browser owns the
// timezone database and the high-resolution clock, and the closed web libc
// contract deliberately omits localtime/strftime (docs/web_export.md §5.3,
// the jet_log.c precedent).
// </nocodex>
//
// Complete HUD wall-clock example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_time.h"
//
//   void _init(void) {}
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       JetDateTime now;
//       jet_cls(0);
//       if (jet_time_local(&now)) {
//           char clock[6];
//           snprintf(clock, sizeof clock, "%02d:%02d", now.hour, now.minute);
//           jet_print(clock, 16, 16, 15);
//       }
//   }
//
//   void _end(void) {}

/**
 * Struct: JetDateTime
 * Summary: One reading of the machine's local date and time, broken into parts: the date, the weekday and the time of day.
 * Include: jet_time.h
 * Notes: jet_time_local fills one in. The numbers are the ones a calendar uses: the year is the whole year (2026, never 126), the month counts from 1 (1 is January), and the weekday counts from Sunday as 0. Every field is a plain int32_t, so you can compare readings, write them to a save file, or give them straight to snprintf.
 * Example:
 *   JetDateTime now;
 *   if (jet_time_local(&now) && now.hour >= 18) start_night_ambience();
 */
typedef struct JetDateTime {
    int32_t year;    // The whole year, for example 2026.
    int32_t month;   // The month, 1 to 12 (1 is January).
    int32_t day;     // The day of the month, 1 to 31.
    int32_t weekday; // The day of the week, 0 to 6 (0 is Sunday).
    int32_t hour;    // The hour, 0 to 23.
    int32_t minute;  // The minute, 0 to 59.
    int32_t second;  // The second, 0 to 59 (on some hosts a leap second reads 60).
} JetDateTime;

/**
 * Function: jet_time_local
 * Summary: Reads the machine's clock as the local date and time, broken into parts.
 * Parameters:
 *   - out: The JetDateTime to fill in. Must not be NULL.
 * Returns: true when out holds a good reading. false when out is NULL, or when the host cannot turn its clock into a date and time; out is then filled with zeros, never left holding old bytes.
 * Include: jet_time.h
 * Notes: Local time follows the player's time zone, daylight saving included, so it can jump forwards or backwards between one read and the next. Show it, but never move gameplay or animation by it; use dt in _update for that. A read takes microseconds, so reading once every _draw is fine even at 120 frames a second. For a clock on screen, read it each frame and print out->hour and out->minute; the text only changes when the minute does.
 * Example:
 *   JetDateTime now;
 *   char clock[6];
 *   if (jet_time_local(&now)) {
 *       snprintf(clock, sizeof clock, "%02d:%02d", now.hour, now.minute);
 *       jet_print(clock, 16, 16, 15);
 *   }
 */
bool jet_time_local(JetDateTime* out);

/**
 * Function: jet_time_epoch
 * Summary: Reads the machine's clock as whole seconds since the Unix epoch: the start of 1 January 1970, UTC.
 * Returns: The seconds since then, as a signed 64-bit number, so the count does not run out in 2038 the way a 32-bit one does.
 * Include: jet_time.h
 * Notes: The number does not depend on the time zone: the same moment reads the same in Sydney and in Seattle, which makes it the right stamp for a save file or a high score. Subtract two readings to measure the real seconds between them, for example how long since the player last visited. Within one session, add up dt from _update instead; that is the same on every machine, and the player changing the system clock cannot upset it. If all you want is a different seed each run, call jet_randomise, which already mixes a finer form of this clock with variation from the launch.
 * Example:
 *   int64_t stamp = jet_time_epoch();
 *   jet_fwrite(&stamp, sizeof stamp, 1, save_file);
 */
int64_t jet_time_epoch(void);

/**
 * Function: jet_time_micros
 * Summary: Reads the stopwatch in whole microseconds: how long since it was first read.
 * Returns: The microseconds since the first reading, as an unsigned 64-bit number. The first reading is exactly 0, and the count never runs out in practice: a 64-bit count of microseconds lasts more than half a million years.
 * Include: jet_time.h
 * Notes: The stopwatch never goes backwards. Nothing that makes the calendar clock jump (daylight saving, a change of time zone, the player resetting the clock) can touch it, so subtracting one reading from a later one is the right way to measure how long something really took: a stretch of code, a load, a message across the network and back.
 *
 *        The precision is honest rather than promised. On a native host the clock really does count microseconds, but a browser blurs its clock on purpose, usually to about 100 microseconds, to stop web pages spying on each other, so treat a web reading as fine but not exact.
 *
 *        Measuring is this call's whole job. Keep gameplay and animation on dt in _update, which is the same on every machine; a cart that makes decisions from the stopwatch behaves differently on every machine.
 * Example:
 *   uint64_t started = jet_time_micros();
 *   rebuild_navigation_mesh();
 *   printf("mesh rebuild took %llu us\n", (unsigned long long)(jet_time_micros() - started));
 */
uint64_t jet_time_micros(void);

/**
 * Function: jet_time_millis
 * Summary: Reads the stopwatch in whole milliseconds: how long since it was first read.
 * Returns: The milliseconds since the first reading, as an unsigned 64-bit number. The first reading is exactly 0.
 * Include: jet_time.h
 * Notes: This is the same clock as jet_time_micros, already divided by 1000 for the everyday jobs: a cooldown in real time, a rough benchmark, a session timer on the screen. All three stopwatch calls read one clock with one zero, so a reading from one can be compared with a reading from another once you convert the units. Like the microsecond form it never goes backwards and never jumps, and like it, it is for measuring, never for moving gameplay; dt in _update does that, the same on every machine.
 * Example:
 *   uint64_t now = jet_time_millis();
 *   if (now - last_autosave_ms >= AUTOSAVE_INTERVAL_MS) { autosave(); last_autosave_ms = now; }
 */
uint64_t jet_time_millis(void);

/**
 * Function: jet_time_seconds
 * Summary: Reads the stopwatch in seconds, with a fraction: how long since it was first read.
 * Returns: The seconds since the first reading, as a double. The first reading is exactly 0.0. A double holds the clock's full microsecond precision for thousands of years, so nothing is lost in the conversion.
 * Include: jet_time.h
 * Notes: This is the same clock as jet_time_micros, in the units that maths already uses: give a reading straight to sin for an effect that moves in real time, or print it with snprintf for a speedrun display. It never goes backwards and never jumps. It is an instrument, not a metronome: measure and show with it, but keep gameplay and animation on dt in _update, which is the same on every machine.
 * Example:
 *   char lap[16];
 *   snprintf(lap, sizeof lap, "%.3fs", jet_time_seconds() - lap_started);
 *   jet_print(lap, 16, 16, 15);
 */
double jet_time_seconds(void);

#endif // JET_TIME_H
