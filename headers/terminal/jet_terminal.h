// runtime/terminal/jet_terminal.h
#ifndef JET_TERMINAL_H
#define JET_TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include "jet_text.h"

// A command console for your own game. It is a panel that drops down over the
// picture while the game runs, with a prompt where you type a command and press
// Enter. Use it while you are making the game, to test things and to write your
// own cheats: spawn ten enemies, jump to level 3, show a hidden number. Press
// the backquote key (the one marked ` and ~) to open it, and again to close it.
// Nothing is drawn while it is closed.
//
// The order of calls matters. Call jet_terminal_init once in _init. Call
// jet_terminal_update(dt) first thing in _update; when it returns true the
// terminal is open and has taken the keyboard, so return before your game reads
// any keys. That is what pauses the game. Call jet_terminal_draw last in _draw,
// so the panel lands on top of everything else. Then add a command with
// jet_terminal_register_command: a word to type, a line of help, and the
// function to run. For a tool that wants whole lines of text, or its own
// picture inside the panel, see jet_terminal_set_input_handler and
// jet_terminal_set_view.
//
// A game whose whole screen is the terminal — a text adventure, a story, a
// shell — pins it with jet_terminal_set_pinned. A pinned terminal is the page
// rather than a panel over one: the backquote key no longer closes it, the key
// guide under the prompt stops offering to, and the game calls
// jet_terminal_update every tick and carries on, because the page is the game.
//
// One such view ships with Jet64: jet_bind_open_ui, the screen where a player
// changes which key or pad button does which action. It is declared at the foot
// of this header because it is drawn ON the panel, so it runs only while the
// cartridge is driving jet_terminal_update and jet_terminal_draw. The bindings
// themselves are firmware and cost nothing (jet_input_binds.h); the screen is
// the part that arrives with the terminal. jet_bind_set_ui_note puts a few
// lines of your own at the foot of that screen, such as your game's credits.
//
// The terminal copies the command's name and its help text, so those strings
// may be temporary. It does not copy your function. A registered function must
// stay alive for as long as it is registered.
//
// Complete custom-command example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_terminal.h"
//
//   static void command_hello(int argc, char** argv) {
//       (void)argc;
//       (void)argv;
//       jet_terminal_print("Hello from your cart!");
//   }
//
//   void _init(void) {
//       jet_terminal_init(NULL);
//       jet_terminal_register_command("hello", "Print a greeting.", command_hello);
//   }
//
//   void _update(float dt) {
//       if (jet_terminal_update(dt)) return;
//       // your game's own update goes here
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print("PRESS ` AND TYPE HELLO", 16, 16, 15);
//       jet_terminal_draw();
//   }
//
//   void _end(void) {}

/**
 * Callback: JetTerminalCallback
 * Summary: A function you write to run one of your commands. The terminal calls it when someone types the command's name and presses Enter.
 * Parameters:
 *   - argc: How many words were typed, counting the command name itself.
 *   - argv: The words, as an array of argc C strings. argv[0] is the command name, so the first argument is argv[1].
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: The typed line is split into words at the spaces. argc is the count and argv holds the words, the command itself first.
 *
 *        argv and its strings are borrowed: they belong to the terminal and are gone the moment your function returns. Copy any value you want to keep. Check argc before you read an argument, or you will read past the end of the array.
 * Example:
 *   static void cmd_spawn(int argc, char** argv) {
 *       if (argc < 2) { jet_terminal_print_error("usage: spawn COUNT"); return; }
 *       spawn_enemies(atoi(argv[1]));
 *   }
 */
typedef void (*JetTerminalCallback)(int argc, char** argv);

/**
 * Callback: JetTerminalInputHandler
 * Summary: A function you write that is given every typed line first, whole and unsplit, before the terminal looks for a command in it.
 * Parameters:
 *   - input: The line exactly as it was typed, as a C string. It is borrowed: it belongs to the terminal and is gone when your function returns.
 * Returns: true if your function dealt with the line, so the terminal does nothing more with it; false to let the registered commands have it.
 * Include: jet_terminal.h
 * Notes: Install one with jet_terminal_set_input_handler, and pass NULL there to remove it. This is for a game that wants whole sentences rather than commands, such as a text adventure.
 * Example:
 *   static bool story_input(const char* input) {
 *       return story_accept_command(input);
 *   }
 */
typedef bool (*JetTerminalInputHandler)(const char* input);

/**
 * Callback: JetTerminalViewUpdate
 * Summary: A function you write to update a view of your own inside the terminal panel. The terminal calls it while the view is showing.
 * Parameters:
 *   - dt: How much time this tick covers, in seconds.
 * Returns: true to keep your view showing; false to hand the panel back to the normal prompt.
 * Include: jet_terminal.h
 * Notes: Install it together with a JetTerminalViewDraw through jet_terminal_set_view. While a view is installed the terminal's own prompt is switched off, so your view reads the keys itself.
 * Example:
 *   static bool inspector_update(float dt) {
 *       (void)dt;
 *       return !jet_key_pressed(JET_KEY_ESCAPE);
 *   }
 */
typedef bool (*JetTerminalViewUpdate)(float dt);

/**
 * Callback: JetTerminalViewDraw
 * Summary: A function you write to draw a view of your own inside the terminal panel. The terminal calls it with the rectangle the view may draw in.
 * Parameters:
 *   - x: The left edge of the rectangle, in screen pixels.
 *   - y: The top edge of the rectangle, in screen pixels.
 *   - w: The width of the rectangle, in pixels.
 *   - h: The height of the rectangle, in pixels.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Keep your drawing inside the rectangle; the terminal draws its frame around it. Install it together with a JetTerminalViewUpdate through jet_terminal_set_view.
 * Example:
 *   static void inspector_draw(int x, int y, int w, int h) {
 *       jet_rect(x, y, w, h, 7);
 *   }
 */
typedef void (*JetTerminalViewDraw)(int x, int y, int w, int h);

/**
 * Struct: TerminalFormat
 * Summary: How the terminal panel looks: whether it starts open, how tall it is, its colours, and the text style of each kind of line.
 * Fields:
 *   - open_on_init: true to have the terminal already open when the game starts.
 *   - height_pct: How much of the screen's height the panel takes, as a fraction from 0.10f (a tenth of the screen) to 1.0f (the whole screen). A value outside that range is held at the nearest end. The panel is never drawn shorter than its title bar, one line of output, the prompt and the key guide need, so a small fraction on a small screen still gives a working console.
 *   - force_uppercase: true to show every line, and what you type, in capitals. The text itself is not changed, only how it is drawn.
 *   - bg_color: The colour of the panel's background, as a palette index: a number from 0 to 255 that picks one of the palette's colours.
 *   - border_color: The colour of the outer border and the separator lines.
 *   - accent_color: The colour of the header bar, the cursor, and the prompt while it is active.
 *   - muted_color: The colour of the lesser separators and the quiet labels.
 *   - error_color: The colour of an error line: one written with jet_terminal_print_error, and a built-in command's report that it failed. Choose one that reads on bg_color.
 *   - success_color: The colour of a built-in command's report that it worked, such as a file written or a setting changed. Choose one that reads on bg_color.
 *   - text_fmt: The text style of an ordinary line of output.
 *   - title_fmt: The text style of the title in the header bar.
 *   - section_fmt: The text style of a section heading, as the built-in help and diagnostics use.
 *   - command_fmt: The text style of a command as it is echoed back, and of what you type.
 *   - hint_fmt: The text style of a hint: a shortcut, an argument, or a quiet piece of guidance.
 * Include: jet_terminal.h
 * Notes: You may fill in only the fields you care about. An initialiser that sets just height_pct, bg_color and text_fmt still works, and any colour or style field left at zero is given a readable default by jet_terminal_init, error_color and success_color included. Error and success lines take the rest of their look (the face, size and spacing) from command_fmt, the style of what you type.
 *
 *        A game with a palette of its own should name every colour, the error and success ones included: the defaults pick colours from Jet64's starting palette, and on a different palette they may be hard to read.
 * Example:
 *   TextFormat body = { .font = 3, .color = 7, .size = 12, .spacing_x = 1, .spacing_y = 2, .alpha = 1.0f };
 *   TextFormat title = { .font = 3, .color = 15, .size = 16, .spacing_x = 1, .spacing_y = 2, .alpha = 1.0f, .shadow_offset = 1, .shadow_color = 0 };
 *   TerminalFormat term = { .open_on_init = false, .height_pct = 0.72f, .force_uppercase = true, .bg_color = 1, .border_color = 8, .accent_color = 18, .error_color = 8, .success_color = 11, .text_fmt = body, .title_fmt = title };
 *   jet_terminal_init(&term);
 */
typedef struct {
    bool open_on_init;
    float height_pct;
    bool force_uppercase;
    uint8_t bg_color;
    uint8_t border_color;
    uint8_t accent_color;
    uint8_t muted_color;
    uint8_t error_color;
    uint8_t success_color;
    TextFormat text_fmt;
    TextFormat title_fmt;
    TextFormat section_fmt;
    TextFormat command_fmt;
    TextFormat hint_fmt;
} TerminalFormat;

// The terminal's live format, made global so a tool of your own can draw in the
// same colours and text styles as the terminal.
extern TerminalFormat term_fmt;

/**
 * Function: jet_make_terminal_format
 * Summary: Builds a TerminalFormat, the description of how the terminal panel looks, from twelve of its fields in one call. The error and success colours are left at zero, which jet_terminal_init turns into its readable defaults.
 * Parameters:
 *   - open_on_init: true to have the terminal already open when the game starts.
 *   - height_pct: How much of the screen's height the panel takes, as a fraction from 0.10f to 1.0f.
 *   - force_uppercase: true to draw every line, and what you type, in capitals.
 *   - bg_color: The colour of the panel's background, as a palette index from 0 to 255.
 *   - border_color: The colour of the border.
 *   - accent_color: The colour of the header bar, the cursor and the active prompt.
 *   - muted_color: The colour of the quieter separators and labels.
 *   - text_fmt: The text style of an ordinary line of output.
 *   - title_fmt: The text style of the title in the header bar.
 *   - section_fmt: The text style of a section heading in the output.
 *   - command_fmt: The text style of what you type.
 *   - hint_fmt: The text style of help and prompt text.
 * Returns: A filled-in TerminalFormat, handed back by value.
 * Include: jet_terminal.h
 * Notes: To choose the error and success colours as well, set error_color and success_color on the format it returns, before you pass it to jet_terminal_init.
 * Example:
 *   TerminalFormat tf = jet_make_terminal_format(false, 0.5f, true, 1, 8, 18, 5, body, title, sec, cmd, hint);
 *   tf.error_color = 8;
 */
static inline TerminalFormat jet_make_terminal_format(
    bool open_on_init,
    float height_pct,
    bool force_uppercase,
    uint8_t bg_color,
    uint8_t border_color,
    uint8_t accent_color,
    uint8_t muted_color,
    TextFormat text_fmt,
    TextFormat title_fmt,
    TextFormat section_fmt,
    TextFormat command_fmt,
    TextFormat hint_fmt
) {
    TerminalFormat fmt;
    fmt.open_on_init = open_on_init;
    fmt.height_pct = height_pct;
    fmt.force_uppercase = force_uppercase;
    fmt.bg_color = bg_color;
    fmt.border_color = border_color;
    fmt.accent_color = accent_color;
    fmt.muted_color = muted_color;
    fmt.error_color = 0;   // zero asks jet_terminal_init for its readable default
    fmt.success_color = 0;
    fmt.text_fmt = text_fmt;
    fmt.title_fmt = title_fmt;
    fmt.section_fmt = section_fmt;
    fmt.command_fmt = command_fmt;
    fmt.hint_fmt = hint_fmt;
    return fmt;
}

/**
 * Function: jet_terminal_init
 * Summary: Sets the terminal up, ready to be opened. Call it once in _init.
 * Parameters:
 *   - fmt: How the panel should look, or NULL for the runtime's own readable defaults.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Call it before you register any command or alias, because setting up throws away every registration made before it. Once set up, the terminal opens and closes with the grave key, the one marked ` and ~, with or without Shift. It takes the keyboard for itself while it is open, and makes jet_terminal_update return true so you can pause the game safely. Setting up also unpins the terminal, so a game whose page it is calls jet_terminal_set_pinned afterwards.
 * Example:
 *   TerminalFormat term = { .height_pct = 0.70f };
 *   jet_terminal_init(&term);
 */
void jet_terminal_init(const TerminalFormat* fmt);

/**
 * Function: jet_terminal_open
 * Summary: Opens the terminal from your code, without waiting for the key.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: The prompt is cleared, so the terminal opens with an empty line to type into.
 * Example:
 *   jet_terminal_open();
 */
void jet_terminal_open(void);

/**
 * Function: jet_terminal_close
 * Summary: Closes the terminal from your code, without waiting for the key.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Closing also removes any view of your own installed with jet_terminal_set_view. It closes a pinned terminal too, and because the backquote key cannot open a pinned terminal, only your own code can then bring it back.
 * Example:
 *   jet_terminal_close();
 */
void jet_terminal_close(void);

/**
 * Function: jet_terminal_is_open
 * Summary: Tells you whether the terminal panel is showing.
 * Parameters:
 *   - None.
 * Returns: true while the panel is up, whether the player opened it with the backtick key or your own code did; otherwise false.
 * Include: jet_terminal.h
 * Notes: While it is up the terminal reads the keyboard, so a game asks this before acting on a key of its own. It is also how code that opened the panel can put it back exactly as it found it.
 * Example:
 *   if (!jet_terminal_is_open() && jet_key_pressed(JET_KEY_P)) { pause_game(); }
 */
bool jet_terminal_is_open(void);

/**
 * Function: jet_terminal_set_pinned
 * Summary: Makes the terminal your game's whole page rather than a panel that drops down over it: the backquote key no longer closes it, and the key guide under the prompt stops offering to.
 * Parameters:
 *   - pinned: true to pin the terminal as the page; false to make it the drop-down again, which the backquote key opens and closes.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Pinning opens the terminal if it is not already open. From then on it opens and closes only when your own code says so, with jet_terminal_open and jet_terminal_close; the backquote key does neither. Unpinning leaves the panel as it is, open or closed, and gives the key back to the player.
 *
 *        On a pinned page the backquote key is an ordinary key. Typed at the prompt, ` and ~ are text like any other character, where the drop-down leaves them out. A view of your own installed with jet_terminal_set_view survives the key, and its update function sees the press like any other key, so it can ignore it or treat it as "back". The rebinding screen (jet_bind_open_ui) treats it as "back" and returns to the page.
 *
 *        jet_terminal_init unpins the terminal, so pin it after jet_terminal_init, beside jet_terminal_set_height. The pin holds until you change it, and it works the same in the Workstation, the native player and the web player.
 * Example:
 *   jet_terminal_init(NULL);
 *   jet_terminal_set_height(1.0f);
 *   jet_terminal_set_pinned(true);
 */
void jet_terminal_set_pinned(bool pinned);

/**
 * Function: jet_terminal_set_height
 * Summary: Changes how tall the terminal panel is, as a fraction of the screen's height.
 * Parameters:
 *   - height_pct: The fraction, from 0.10f (a tenth of the screen) to 1.0f (the whole screen). A value outside that range is held at the nearest end.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: You may call it at any time while the game runs, to make the panel taller or shorter. However small the fraction, the panel keeps room for its title bar, one line of output, the prompt and the key guide.
 * Example:
 *   jet_terminal_set_height(1.0f);
 */
void jet_terminal_set_height(float height_pct);

/**
 * Function: jet_terminal_update
 * Summary: Runs the terminal for one tick: it watches for the open-and-close key, reads what is typed, keeps the history and the scrolling, completes a half-typed command name, and runs a command when Enter is pressed.
 * Parameters:
 *   - dt: How much time this tick covers, in seconds; the dt that _update was given.
 * Returns: true when the terminal is open and has taken the keyboard; false when it is closed.
 * Include: jet_terminal.h
 * Notes: Call it first thing in _update. When it returns true, return from _update straight away, so the keys typed into the terminal do not also drive your game. A game whose whole screen is a pinned terminal (jet_terminal_set_pinned) calls it and carries on instead, because the terminal is always open and the page is the game. The Tab key completes a half-typed command name, and so does Ctrl+Space, which is there because the Workstation keeps Tab for itself and cannot pass it on.
 * Example:
 *   if (jet_terminal_update(dt)) return;
 */
bool jet_terminal_update(float dt);

/**
 * Function: jet_terminal_draw
 * Summary: Draws the terminal panel over the picture, if the terminal is open. When it is closed this draws nothing.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Call it last in _draw, after everything else, so the panel sits on top of your game, its score display and any debugging overlay.
 * Example:
 *   jet_terminal_draw();
 */
void jet_terminal_draw(void);

/**
 * Function: jet_terminal_print
 * Summary: Adds a line of ordinary text to the terminal's output.
 * Parameters:
 *   - text: The text to show, as a C string. A newline character inside it starts a new row.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: This is the call to use for a command's reply and for debugging messages from your game. A line longer than a terminal row is wide is clipped to fit.
 * Example:
 *   jet_terminal_print("Player reached checkpoint 2.");
 */
void jet_terminal_print(const char* text);

/**
 * Function: jet_terminal_print_header
 * Summary: Adds a line to the terminal's output in the section-heading style.
 * Parameters:
 *   - text: The heading, as a C string.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Use it for a line that should stand out: the name of a scene, the title of a block of diagnostics, or a banner for one part of your game.
 * Example:
 *   jet_terminal_print_header("AI DEBUG");
 */
void jet_terminal_print_header(const char* text);

/**
 * Function: jet_terminal_print_hint
 * Summary: Adds a line to the terminal's output in the quiet hint style.
 * Parameters:
 *   - text: The hint, as a C string.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Use it for the lines a reader may skip: a keyboard shortcut, a tip on what to type next, or a small detail beside a diagnostic.
 * Example:
 *   jet_terminal_print_hint("Press ~ to close the terminal.");
 */
void jet_terminal_print_hint(const char* text);

/**
 * Function: jet_terminal_print_error
 * Summary: Adds a line to the terminal's output in the error style.
 * Parameters:
 *   - text: The error message, as a C string.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Use it when a command fails: a file is missing, an argument makes no sense, or a check in your own game does not pass.
 * Example:
 *   jet_terminal_print_error("cat failed: missing file");
 */
void jet_terminal_print_error(const char* text);

/**
 * Function: jet_terminal_register_command
 * Summary: Adds a command of your own to the terminal: a word to type, a line of help, and the function that runs when the word is typed.
 * Parameters:
 *   - name: The word to type at the prompt.
 *   - description: One line of help about the command, shown in the built-in help.
 *   - callback: Your function, of the shape void func(int argc, char** argv). It runs when the first word typed matches name.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: The name and the description are copied, the name to 31 characters and the description to 63, so the strings you pass may be temporary. The function is not copied: it must stay alive until you unregister it.
 *
 *        Registering a name that already exists replaces its function and its help. A NULL name or a NULL function is ignored, and so is a 33rd command; the terminal holds 32.
 * Example:
 *   jet_terminal_register_command("spawn", "Creates an enemy dynamically.", cmd_spawn);
 */
void jet_terminal_register_command(const char* name, const char* description, JetTerminalCallback callback);

/**
 * Function: jet_terminal_unregister_command
 * Summary: Removes a command you registered, so typing its name no longer runs your function.
 * Parameters:
 *   - name: The word you registered the command under.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: A name that was never registered is ignored, and nothing goes wrong.
 * Example:
 *   jet_terminal_unregister_command("spawn");
 */
void jet_terminal_unregister_command(const char* name);

/**
 * Function: jet_terminal_add_alias
 * Summary: Gives a command a second name, so typing the alias runs that command.
 * Parameters:
 *   - alias_name: The new word to type.
 *   - command_name: The name of the command it stands for.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: Both words are copied, cut to 31 characters if longer, so the strings you pass may be temporary. Adding an alias that already exists points it at the new command instead. A NULL word is ignored, and so is a 33rd alias; the terminal holds 32. Only the first word typed is checked against the aliases, so with an alias "create" for "spawn", typing create runs spawn.
 * Example:
 *   jet_terminal_add_alias("create", "spawn");
 */
void jet_terminal_add_alias(const char* alias_name, const char* command_name);

/**
 * Function: jet_terminal_set_input_handler
 * Summary: Installs a function of yours that is given every typed line first, whole and unsplit, before the terminal looks for a command in it.
 * Parameters:
 *   - handler: The function, of the shape bool func(const char* input). It returns true when it has dealt with the line, and false to let the registered commands have it.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: This is for a game that wants whole sentences rather than commands: a text adventure with its own parser, a MUD, or a port of a Z-Machine story. Pass NULL to remove the handler and go back to commands only.
 * Example:
 *   jet_terminal_set_input_handler(zengine_input);
 */
void jet_terminal_set_input_handler(JetTerminalInputHandler handler);

/**
 * Function: jet_terminal_set_view
 * Summary: Hands the inside of the terminal panel to a tool of your own, which draws there instead of the usual lines of output and the prompt.
 * Parameters:
 *   - update_cb: Your update function. jet_terminal_update calls it while the terminal is open. Have it return false when the tool is finished and the prompt should come back.
 *   - draw_cb: Your draw function. jet_terminal_draw calls it with the rectangle it may draw in.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Notes: While the view is installed the prompt is switched off, so the keys the tool reads cannot also be typed into a command. Install both functions together. When update_cb returns false both are removed. To remove them yourself, call jet_terminal_clear_view.
 * Example:
 *   jet_terminal_set_view(my_tool_update, my_tool_draw);
 */
void jet_terminal_set_view(JetTerminalViewUpdate update_cb, JetTerminalViewDraw draw_cb);

/**
 * Function: jet_terminal_view_bounds
 * Summary: Tells you the rectangle of the screen that a view of your own is drawn into.
 * Parameters:
 *   - x: Where to write the left edge, in screen pixels, or NULL if you do not need it.
 *   - y: Where to write the top edge, in screen pixels, or NULL if you do not need it.
 *   - w: Where to write the width, in pixels, or NULL if you do not need it.
 *   - h: Where to write the height, in pixels, or NULL if you do not need it.
 * Returns: Nothing. The answers are written through the pointers.
 * Include: jet_terminal.h
 * Notes: It is the same rectangle the draw function installed with jet_terminal_set_view is given. Use it in your update function when it needs the panel's size or place, to split a list into pages or to work out what the mouse is over, without doing the panel's sums yourself.
 * Example:
 *   int w, h;
 *   jet_terminal_view_bounds(NULL, NULL, &w, &h);
 */
void jet_terminal_view_bounds(int* x, int* y, int* w, int* h);

/**
 * Function: jet_terminal_clear_view
 * Summary: Removes the view installed with jet_terminal_set_view and gives the panel back to the normal prompt.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_terminal.h
 * Example:
 *   jet_terminal_clear_view();
 */
void jet_terminal_clear_view(void);

/**
 * Function: jet_bind_open_ui
 * Summary: Opens the rebinding screen, a view on the terminal panel where the player can change which inputs do which actions.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: terminal/jet_terminal.h
 * Notes: The screen is drawn ON the terminal panel, so your cartridge must be driving the panel for it to run at all — jet_terminal_update(dt) in _update and jet_terminal_draw() in _draw, exactly as this header's first example shows. Opening it puts the panel up for you, and takes it back down again on the way out if it was not already showing, so a game that never displays a terminal still gets a screen that appears and leaves cleanly. It also takes the WHOLE panel while it stands, whatever height you have the console set to, because a settings page needs the room; the height you chose comes back the moment the player leaves. The screen lays itself out to whatever panel that gives it, so it reads on a 320-pixel picture as well as on a 1280-pixel one. The bindings it edits are firmware (jet_input_binds.h): this call is the only part that needs the terminal. The screen has two settings at the top and the list of actions below. DEVICE chooses whether you are editing the keyboard and mouse or one particular controller, so a change to one pad never touches another. PLAYER chooses whose keys you are editing, or — with a controller chosen — which player that controller's slot drives. Move with the arrow keys, press ENTER or SPACE to capture a new input, and DELETE or BACKSPACE to clear one. Every change is saved to disk straight away. ESC, or the backtick (grave) key with or without Shift, closes the screen; while it is waiting for an input, ESC cancels that capture instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_B)) { jet_bind_open_ui(); }
 */
void jet_bind_open_ui(void);

/**
 * Constant: JET_BIND_UI_NOTE_BYTES
 * Summary: The most bytes the rebinding screen keeps of a note given to jet_bind_set_ui_note, its terminating NUL included.
 * Include: terminal/jet_terminal.h
 * Notes: A longer note is cut to fit. Size a buffer you build a note in with it, and the whole note always arrives.
 * Example:
 *   char note[JET_BIND_UI_NOTE_BYTES];
 */
#define JET_BIND_UI_NOTE_BYTES 512

/**
 * Function: jet_bind_set_ui_note
 * Summary: Gives the rebinding screen a few lines of your own to show under the list of actions on its last page, such as your game's name, its version and its credits.
 * Parameters:
 *   - text: The note, as a C string. A '\n' starts a new line, and an empty line between two others is drawn as a blank one. NULL or "" removes the note.
 * Returns: Nothing.
 * Include: terminal/jet_terminal.h
 * Notes: The note is copied, up to JET_BIND_UI_NOTE_BYTES - 1 bytes, so the string you pass may be temporary. Call it whenever you like, before the screen opens or while it is showing; the screen draws whichever note it was given last, from its next frame on.
 *
 *        The note is drawn in the terminal's quiet hint style at the foot of the screen's last page, just above the guidance, and each line is wrapped at its spaces to the width of the panel. The screen pages its actions around it, so the note never covers an action: every page before the last is filled in order, and the last keeps room for the note and always holds at least the final action. A note taller than the room left beside that one action is cut after the last line that fits, so keep it to a few short lines on a small screen.
 *
 *        The terminal's text is drawn in a Font ROM face, which has a picture only for the printable ASCII characters, and any other byte is drawn as '?'. Spell a character outside ASCII the way a reader would recognise it, such as "(C)" for ©.
 * Example:
 *   char note[JET_BIND_UI_NOTE_BYTES];
 *   snprintf(note, sizeof(note), "My Game\nVersion: %s", jet_publish_record()->version);
 *   jet_bind_set_ui_note(note);
 */
void jet_bind_set_ui_note(const char* text);

// Short names without the jet_ prefix, kept so older cartridges still build.
#define terminal_init jet_terminal_init
#define terminal_open jet_terminal_open
#define terminal_close jet_terminal_close
#define terminal_set_height jet_terminal_set_height
#define terminal_update jet_terminal_update
#define terminal_draw jet_terminal_draw
#define terminal_print jet_terminal_print
#define terminal_print_header jet_terminal_print_header
#define terminal_print_hint jet_terminal_print_hint
#define terminal_print_error jet_terminal_print_error
#define terminal_register_command jet_terminal_register_command
#define terminal_unregister_command jet_terminal_unregister_command
#define terminal_add_alias jet_terminal_add_alias
#define terminal_set_input_handler jet_terminal_set_input_handler

#endif // JET_TERMINAL_H
