// runtime/jet_log.h
#ifndef JET_LOG_H
#define JET_LOG_H

#include <stdbool.h>
#include <stdarg.h>

// Write notes about what your game is doing to a log file, so you can read
// them after a run. The file goes in the cartridge's own AppData folder, the
// private folder every cartridge gets for its saves and settings, and it is
// written as Markdown, a plain-text format that reads well in any text editor.
// Every message has a severity, from TRACE (the finest detail) up to FATAL
// (the worst thing that can happen), and you choose the lowest level that
// gets written, so a finished game can keep only its warnings and errors.
//
// To start, build a JetLogOptions with jet_make_log_options, call jet_log_init
// once in _init, then use LOG_INFO, LOG_WARN and LOG_ERROR wherever you like,
// from any thread of the cartridge. If you ask for it, each line is also
// echoed to stdout, which shows up in the Workstation's terminal. A log is
// for finding out what happened afterwards; it does not deal with a problem
// for you, so your game code still has to handle its own errors.
//
// Complete diagnostic-log example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_log.h"
//
//   void _init(void) {
//       JetLogOptions options = jet_make_log_options(
//           "My Cart", "log/session.md", true, true, JET_LOG_INFO, true
//       );
//       jet_log_init(&options);
//       LOG_INFO("cart started");
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) LOG_INFO("player pressed A");
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print("PRESS A, THEN OPEN APPDATA/LOG", 16, 16, 15);
//   }
//
//   void _end(void) {
//       LOG_INFO("cart ended");
//   }

/**
 * Enum: JetLogLevel
 * Summary: How serious a log message is, from TRACE at the bottom to FATAL at the top. The logger uses it to decide which messages to write and how to label each line.
 * Include: jet_log.h
 * Notes: jet_log_init sets the filter: from then on a message is written only when its level is at or above JetLogOptions.min_level. TRACE is the chattiest level and FATAL the most severe. Logging a FATAL message writes the words and nothing more; it does not stop the cartridge.
 * Example:
 *   JetLogOptions opts = jet_make_log_options("My Game", "log/run.md", true, false, JET_LOG_INFO, true);
 *   jet_log_init(&opts);
 */
typedef enum {
    JET_LOG_TRACE = 0, // The finest detail of what the code is doing. Normally switched off.
    JET_LOG_DEBUG,     // Inside information for you, the developer, while you work on the game.
    JET_LOG_INFO,      // Something normal but worth a note, such as a level loading.
    JET_LOG_WARN,      // Something unexpected that the game got past on its own.
    JET_LOG_ERROR,     // Something failed, but the game can carry on.
    JET_LOG_FATAL      // Something the game cannot get past. Logging it does not stop the game.
} JetLogLevel;

/**
 * Struct: JetLogOptions
 * Summary: The settings for the logger: what to call the log, where to write it, what to put on each line, and the lowest level worth writing.
 * Fields:
 *   - project_name: The title written at the top of the Markdown log file.
 *   - log_filename: Where to write the log, as a path inside the cartridge's own AppData folder.
 *   - include_datetime: true to put a readable date and time in the file's header, and a time on every entry.
 *   - include_system_specs: true to write the host's platform, the screen resolution and the video backend at the top of the log.
 *   - min_level: The lowest level a message may have and still be written.
 *   - echo_to_stdout: true to print every line to the terminal as well: the Workstation's terminal, or the system one an exported player runs in.
 * Include: jet_log.h
 * Notes: The logger keeps its own copy of this struct, but not of the two strings: it reads project_name and log_filename through your pointers for as long as it runs. So use string literals, or storage that stays alive for every later jet_log call. Fill the struct with jet_make_log_options, then pass it to jet_log_init.
 * Example:
 *   JetLogOptions opts = jet_make_log_options("My Game", "log/run.md", true, true, JET_LOG_INFO, true);
 *   jet_log_init(&opts);
 */
typedef struct {
    const char* project_name;
    const char* log_filename;
    bool include_datetime;
    bool include_system_specs;
    JetLogLevel min_level;
    bool echo_to_stdout;
} JetLogOptions;

/**
 * Function: jet_make_log_options
 * Summary: Fills in a JetLogOptions from its six settings and hands it back, ready for jet_log_init.
 * Parameters:
 *   - project_name: The title written at the top of the Markdown log file.
 *   - log_filename: Where to write the log, as a path inside the cartridge's own AppData folder, for example "log/game.md".
 *   - include_datetime: true to put the current local time on every entry.
 *   - include_system_specs: true to write the screen resolution, the video backend and the platform in the log's header.
 *   - min_level: The lowest level a message may have and still be written. JET_LOG_INFO, for example, drops TRACE and DEBUG messages.
 *   - echo_to_stdout: true to print every line to stdout as well, which the Workstation shows in its console.
 * Returns: The filled-in JetLogOptions, as a value you can keep or pass straight on.
 * Include: jet_log.h
 * Example:
 *   JetLogOptions opts = jet_make_log_options("My Game", "log/run.md", true, true, JET_LOG_WARN, true);
 */
JetLogOptions jet_make_log_options(
    const char* project_name,
    const char* log_filename,
    bool include_datetime,
    bool include_system_specs,
    JetLogLevel min_level,
    bool echo_to_stdout
);

/**
 * Function: jet_log_init
 * Summary: Starts the logger and creates a fresh Markdown log file.
 * Parameters:
 *   - options: A pointer to your JetLogOptions, or NULL to use the runtime's own defaults.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: If a log file already exists at that path, it is wiped and started again. This call also makes the logger's own mutex, so several threads can log at once without their lines running into each other.
 * Example:
 *   JetLogOptions opts = jet_make_log_options("My Game", "log/run.md", true, true, JET_LOG_INFO, true);
 *   jet_log_init(&opts);
 */
void jet_log_init(const JetLogOptions* options);

/**
 * Function: jet_log
 * Summary: Writes one message to the log file, if its level is at or above the level you chose. The message is built printf-style, from a format string and its arguments.
 * Parameters:
 *   - level: How serious the message is: one of JET_LOG_TRACE, JET_LOG_DEBUG, JET_LOG_INFO, JET_LOG_WARN, JET_LOG_ERROR or JET_LOG_FATAL.
 *   - format: The text of the message, in printf's style, with %d, %s and the rest standing for the values that follow.
 *   - ...: The values the format string asks for, in order.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: You will nearly always want LOG_INFO, LOG_WARN and the other capitalised macros instead of calling this yourself. It is safe to call from more than one thread at a time.
 * Example:
 *   jet_log(JET_LOG_INFO, "Player scored %d points", score);
 */
void jet_log(JetLogLevel level, const char* format, ...);

/**
 * Function: LOG_TRACE
 * Summary: Writes a printf-style message to the log at the TRACE level, the finest detail.
 * Parameters:
 *   - ...: The format string, then the values it asks for, as you would give them to printf.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: Nothing is written when min_level is above TRACE.
 * Example:
 *   LOG_TRACE("player x=%d", player_x);
 */
#define LOG_TRACE(...) jet_log(JET_LOG_TRACE, __VA_ARGS__)

/**
 * Function: LOG_DEBUG
 * Summary: Writes a printf-style message to the log at the DEBUG level, inside information for you while you work on the game.
 * Parameters:
 *   - ...: The format string, then the values it asks for, as you would give them to printf.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: Nothing is written when min_level is above DEBUG.
 * Example:
 *   LOG_DEBUG("active enemies=%d", enemy_count);
 */
#define LOG_DEBUG(...) jet_log(JET_LOG_DEBUG, __VA_ARGS__)

/**
 * Function: LOG_INFO
 * Summary: Writes a printf-style message to the log at the INFO level, for something normal but worth a note.
 * Parameters:
 *   - ...: The format string, then the values it asks for, as you would give them to printf.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: Use it for the ordinary milestones you would want to see in the log afterwards, such as a level loaded or a game saved.
 * Example:
 *   LOG_INFO("loaded level %d", level_number);
 */
#define LOG_INFO(...)  jet_log(JET_LOG_INFO,  __VA_ARGS__)

/**
 * Function: LOG_WARN
 * Summary: Writes a printf-style message to the log at the WARN level, for something unexpected that the game got past.
 * Parameters:
 *   - ...: The format string, then the values it asks for, as you would give them to printf.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: Use it when the game recovers on its own, but you would still want to look into what happened.
 * Example:
 *   LOG_WARN("save missing; using defaults");
 */
#define LOG_WARN(...)  jet_log(JET_LOG_WARN,  __VA_ARGS__)

/**
 * Function: LOG_ERROR
 * Summary: Writes a printf-style message to the log at the ERROR level, for something that failed.
 * Parameters:
 *   - ...: The format string, then the values it asks for, as you would give them to printf.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: Writing an error to the log changes nothing else. Your code still has to return early, or recover, on its own.
 * Example:
 *   LOG_ERROR("could not load slot %d", slot);
 */
#define LOG_ERROR(...) jet_log(JET_LOG_ERROR, __VA_ARGS__)

/**
 * Function: LOG_FATAL
 * Summary: Writes a printf-style message to the log at the FATAL level, for something the game cannot get past.
 * Parameters:
 *   - ...: The format string, then the values it asks for, as you would give them to printf.
 * Returns: Nothing.
 * Include: jet_log.h
 * Notes: This does not stop the cartridge. After logging, stop the game, or put it into a safe error state, yourself.
 * Example:
 *   LOG_FATAL("world state is unrecoverable");
 */
#define LOG_FATAL(...) jet_log(JET_LOG_FATAL, __VA_ARGS__)

#endif // JET_LOG_H
