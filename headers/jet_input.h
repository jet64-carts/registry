// runtime/jet_input.h
#ifndef JET_INPUT_H
#define JET_INPUT_H

#include <stdint.h>
#include <stdbool.h>

// The mouse and the keyboard. Ask jet_mouse_down or jet_key_down whether a
// button or key is held right now; that is the question for steady movement
// while something is held. Ask jet_mouse_pressed or jet_key_pressed whether
// it went down this tick; that is the question for a one-off action such as
// a jump. Ask jet_mouse_released or jet_key_released whether it came up this
// tick; that is the question for the end of a hold, such as letting a charged
// shot fly. Those three want to be told which key or button to look at; the
// keyboard answers the same three about no key in particular, through
// jet_any_key_down, jet_any_key_pressed and jet_any_key_released. A "press any
// key" prompt wants the middle one: it waits for a fresh press, where the level
// would be answered on the spot by whatever the player happened to be holding
// when the prompt appeared. Jet64 takes a fresh snapshot of every input
// before each _update, so your cartridge never polls a device or sets one up
// itself. For named actions the player can rebind, mixing keyboard, mouse and
// gamepad, include jet_input_binds.h and use the jet_bind_* calls.
//
// A key's code is its ASCII code wherever ASCII has one: 'A' and JET_KEY_A
// are the same number. The named keys carry on past ASCII: the arrows, the
// modifiers (Shift, Control and Alt), the navigation cluster (Delete, Insert,
// Home, End, Page Up and Page Down), the numeric keypad, the function row,
// and the two GUI keys (Command on a Mac, the Windows key, Super on Linux).
// The keypad is a block of its own, JET_KEY_KP_0 to JET_KEY_KP_9, then
// JET_KEY_KP_DECIMAL, JET_KEY_KP_DIVIDE, JET_KEY_KP_MULTIPLY,
// JET_KEY_KP_SUBTRACT, JET_KEY_KP_ADD, JET_KEY_KP_ENTER, JET_KEY_KP_EQUAL and
// JET_KEY_NUMLOCK, so a cartridge can give the keypad its own controls, its
// Enter included, separate from the top row. The function row is JET_KEY_F1
// to JET_KEY_F12. jet_key_name spells any key for a control legend.
//
// A shortcut such as Save is Ctrl+S on Windows and Linux and Command+S on a
// Mac. Ask jet_key_command_down whether the player is holding the command key
// of their own keyboard, and the same line of C reads right on all three;
// jet_keyboard_kind says which keyboard it is, for a legend that spells it.
//
// Complete mouse-input example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { CURSOR_RADIUS = 8 };
//   static bool painting;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       (void)dt;
//       painting = jet_mouse_down(MOUSE_BTN_LEFT);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_circfill(jet_mouse_x(), jet_mouse_y(), CURSOR_RADIUS, painting ? 10 : 15);
//       jet_print("HOLD LEFT MOUSE", 16, 16, 15);
//   }
//
//   void _end(void) {}

// --- MOUSE BUTTONS ---
// The numbers that name the mouse buttons. Give one to jet_mouse_down,
// jet_mouse_pressed or jet_mouse_released, or to jet_bind_mouse in
// jet_input_binds.h. jet_mouse_button_name spells any of them for a control
// legend.

/**
 * Constant: MOUSE_BTN_LEFT
 * Summary: The left mouse button, the main click, and the one button every pointer has.
 * Include: jet_input.h
 * Notes: This is the usual button for select, fire and place. jet_mouse_pressed gives you one action for each click; jet_mouse_down stays true for as long as the button is held, which is what painting or dragging wants.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_BTN_LEFT)) place_tile(jet_mouse_x(), jet_mouse_y());
 */
#define MOUSE_BTN_LEFT   0

/**
 * Constant: MOUSE_BTN_RIGHT
 * Summary: The right mouse button, the second click.
 * Include: jet_input.h
 * Notes: This is the usual button for cancel, for a context menu, or for a second way to fire. A cartridge that draws its own cursor usually treats it and MOUSE_BTN_LEFT as the two halves of one control.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_BTN_RIGHT)) open_context_menu();
 */
#define MOUSE_BTN_RIGHT  1

/**
 * Constant: MOUSE_BTN_MIDDLE
 * Summary: The middle mouse button, which is the wheel pushed down as a button.
 * Include: jet_input.h
 * Notes: Pushing the wheel is not the same as turning it. Turning is MOUSE_WHEEL_UP and MOUSE_WHEEL_DOWN. A middle click is comfortable for panning, or for switching free-look on and off, and every three-button mouse has one.
 * Example:
 *   if (jet_mouse_down(MOUSE_BTN_MIDDLE)) pan_camera(jet_mouse_dx(), jet_mouse_dy());
 */
#define MOUSE_BTN_MIDDLE 2

/**
 * Constant: MOUSE_BTN_X1
 * Summary: The first side button, the thumb button usually marked "back".
 * Include: jet_input.h
 * Notes: Treat it as an extra, never as the only way to do something. The Workstation reports it exactly, but the exported player's window library may mix it up with the other side button or drop it altogether, so always give the same action a keyboard or gamepad binding as well.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_BTN_X1)) step_back_one_page();
 */
#define MOUSE_BTN_X1     3 // First side button ("back" thumb button)

/**
 * Constant: MOUSE_BTN_X2
 * Summary: The second side button, the thumb button usually marked "forward".
 * Include: jet_input.h
 * Notes: The twin of MOUSE_BTN_X1, with the same warning: treat it as an extra a player may not have, never as the only way to reach an action.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_BTN_X2)) step_forward_one_page();
 */
#define MOUSE_BTN_X2     4 // Second side button ("forward" thumb button)

// The Workstation delivers X1 and X2 exactly. The exported player is limited
// by the window library it is built on (miniquad): depending on the operating
// system, the two side buttons arrive as one and the same, or not at all. So
// treat X1 and X2 as extras, and always give their actions another binding
// as well.

// The wheel as two pretend buttons. You can bind them and read them through
// jet_mouse_down, jet_mouse_pressed and jet_mouse_released exactly like real
// buttons. They are momentary: down only on the ticks the wheel actually
// turned that way. Their numbers sit outside the range real buttons use
// (0 to 7), so a real button added later can never collide with them.

/**
 * Constant: MOUSE_WHEEL_UP
 * Summary: The wheel turned up, one notch away from the player, read as a button.
 * Include: jet_input.h
 * Notes: It is momentary: it reads as down only on the ticks the wheel actually turned that way, so jet_mouse_down and jet_mouse_pressed agree on it. Use it for steps, such as changing weapon. For something smooth, such as a zoom, read the raw amount with jet_mouse_wheel instead.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_WHEEL_UP)) select_previous_weapon();
 */
#define MOUSE_WHEEL_UP   8

/**
 * Constant: MOUSE_WHEEL_DOWN
 * Summary: The wheel turned down, one notch towards the player, read as a button.
 * Include: jet_input.h
 * Notes: The twin of MOUSE_WHEEL_UP, and momentary in exactly the same way. Both numbers sit outside the range real buttons use, so a real button added later can never collide with a wheel binding.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_WHEEL_DOWN)) select_next_weapon();
 */
#define MOUSE_WHEEL_DOWN 9

/**
 * Function: jet_trap_mouse
 * Summary: Traps the mouse inside the game display, or lets it go again. A trapped mouse reports raw movement and is not stopped by the edges of the desktop.
 * Parameters:
 *   - enable: true to ask the host to trap the mouse and report raw movement; false to give back the normal desktop cursor.
 * Returns: Nothing.
 * Include: jet_input.h
 * Notes: Use it for a first-person camera, a free-look editor, or any control that steers by how far the mouse moved rather than by where it is. The host (the program that runs your cartridge) applies the trap once the game display has focus. In fullscreen the window already covers the whole display, so a request to trap is recorded as false, and jet_mouse_trapped then returns false.
 * Example:
 *   void _init(void) {
 *       jet_trap_mouse(true);
 *   }
 */
void jet_trap_mouse(bool enable);

/**
 * Function: jet_mouse_trapped
 * Summary: Tells you whether the mouse is trapped inside the game display.
 * Parameters:
 *   - None.
 * Returns: true if the mouse is trapped; otherwise false.
 * Include: jet_input.h
 * Notes: It follows what your cartridge asked for with jet_trap_mouse.
 * Example:
 *   if (jet_mouse_trapped()) { process_camera_look(); }
 */
bool jet_mouse_trapped(void);

/**
 * Function: jet_set_mouse_visible
 * Summary: Shows or hides the operating system's mouse cursor over the game display.
 * Parameters:
 *   - visible: true to show the cursor; false to hide it.
 * Returns: Nothing.
 * Include: jet_input.h
 * Notes: The host does the showing and hiding. It reads your request from the SYS registry, so this works only where the host supports it.
 * Example:
 *   jet_set_mouse_visible(false);
 */
void jet_set_mouse_visible(bool visible);

/**
 * Function: jet_mouse_visible
 * Summary: Tells you whether the operating system's mouse cursor is showing.
 * Parameters:
 *   - None.
 * Returns: true if the cursor is visible.
 * Include: jet_input.h
 * Notes: It follows what your cartridge asked for with jet_set_mouse_visible. While jet_mouse_auto_hide is on, that setting changes by itself, and this call reports it as it is now.
 * Example:
 *   if (!jet_mouse_visible()) { draw_custom_cursor(); }
 */
bool jet_mouse_visible(void);

/**
 * Function: jet_mouse_auto_hide
 * Summary: Hides the mouse cursor by itself once the pointer has been still for a while, and brings it back the moment the player moves it. This works only in the exported player; in the Workstation it does nothing.
 * Parameters:
 *   - enable: true to switch auto-hiding on; false to switch it off and show the cursor again straight away.
 *   - hide_delay_seconds: How long the pointer must stay still, in seconds, before the cursor is hidden. A value of 0 or less hides it as soon as the pointer comes to rest.
 *   - deadzone_pixels: How far the pointer may drift from its resting place, in screen pixels, without the cursor coming back. This is so a small bump or a jittery sensor does not bring the cursor back by mistake. A real move past that distance, or any click or wheel notch, brings it back at once. A value of 0 or less turns the zone off, so any movement at all brings it back.
 * Returns: Nothing.
 * Include: jet_input.h
 * Notes: The runtime counts the still time once per tick, so the delay is exact whatever the screen's frame rate. Auto-hide drives the same setting as jet_set_mouse_visible, and jet_mouse_visible reports what that setting is right now.
 *
 *        It is safe to call every frame to change the delay or the zone: calling it again while it is already on only updates the two numbers and never resets the running timer. While auto-hide is on, it owns the cursor's visibility. Switch it off before you call jet_set_mouse_visible yourself, or the two will fight over the same setting.
 *
 *        It suits a gamepad game, a twin-stick shooter, or a cinematic cartridge, where the pointer should fade away during play and come back the instant the player reaches for the mouse.
 * Example:
 *   void _init(void) {
 *       // Hide the cursor after 2 seconds of stillness; ignore bumps under 6 pixels.
 *       jet_mouse_auto_hide(true, 2.0f, 6.0f);
 *   }
 */
void jet_mouse_auto_hide(bool enable, float hide_delay_seconds, float deadzone_pixels);

/**
 * Function: _jet_input_hw_update
 * Summary: Internal runtime hook that snapshots host-written mouse, keyboard, and binding state before cartridge code runs.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Jet64 calls this exactly once per simulation tick immediately before _update(). Cartridge code must never call it; doing so rotates the edge snapshots twice and can consume typed characters before gameplay reads them.
 * Example:
 *   // No cartridge call is required; read input directly inside _update().
 *   if (jet_key_pressed(JET_KEY_SPACE)) { jump(); }
 */
void _jet_input_hw_update(void);

/**
 * Function: _jet_input_hw_draw_begin
 * Summary: Internal runtime hook that republishes every edge query as the whole frame's span, so the single _draw() following a frame's ticks sees the input that reached _update() during them.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Jet64 calls this exactly once per presented frame, immediately before _draw(). A host frame carries one or more ticks but captures input once, so an edge lands on the frame's first tick and would otherwise have decayed to a level by the time _draw() ran; this publishes the union of every tick edge (and the sum of every wheel/pointer delta) since the previous draw, then consumes it. The per-tick view _update() reads is untouched. Cartridge code must never call it; a second publish reports an empty span and swallows the frame's input. See docs/architecture.md section 3.D.
 * Example:
 *   // No cartridge call is required; immediate-mode UI reads input in _draw().
 *   if (jet_mouse_pressed(MOUSE_BTN_LEFT) && hovering) { activate(); }
 */
void _jet_input_hw_draw_begin(void);

/**
 * Function: jet_mouse_x
 * Summary: Where the mouse is across the screen, in pixels.
 * Parameters:
 *   - None.
 * Returns: The mouse's X position inside the game display, in screen pixels, as the host last reported it.
 * Include: jet_input.h
 * Notes: This is a screen position, not a world position: the camera's offset is not taken off it. To turn it into a world position, add _jet_cam_x yourself.
 * Example:
 *   float mx = jet_mouse_x();
 */
float jet_mouse_x(void);

/**
 * Function: jet_mouse_y
 * Summary: Where the mouse is down the screen, in pixels.
 * Parameters:
 *   - None.
 * Returns: The mouse's Y position inside the game display, in screen pixels, as the host last reported it.
 * Include: jet_input.h
 * Notes: This is a screen position, not a world position: the camera's offset is not taken off it. To turn it into a world position, add _jet_cam_y yourself.
 * Example:
 *   float my = jet_mouse_y();
 */
float jet_mouse_y(void);

/**
 * Function: jet_mouse_dx
 * Summary: How far the mouse moved across since the last input snapshot, in pixels.
 * Parameters:
 *   - None.
 * Returns: The change in X, worked out as the current mouse X minus the previous one.
 * Include: jet_input.h
 * Notes: When the mouse is trapped with jet_trap_mouse, this is the value to turn a camera with, or to drive any other control that uses how far the mouse moved.
 * Example:
 *   camera_yaw += jet_mouse_dx() * 0.01f;
 */
float jet_mouse_dx(void);

/**
 * Function: jet_mouse_dy
 * Summary: How far the mouse moved down since the last input snapshot, in pixels.
 * Parameters:
 *   - None.
 * Returns: The change in Y, worked out as the current mouse Y minus the previous one.
 * Include: jet_input.h
 * Notes: When the mouse is trapped with jet_trap_mouse, this is the value to tilt a camera with, or to drive any other control that uses how far the mouse moved.
 * Example:
 *   camera_pitch += jet_mouse_dy() * 0.01f;
 */
float jet_mouse_dy(void);

/**
 * Function: jet_mouse_btn
 * Summary: An older way to ask whether a mouse button is held. New code should use jet_mouse_down.
 * Parameters:
 *   - button: Which button: one of the MOUSE_BTN_* constants, or MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN for the wheel.
 * Returns: A number other than zero while the button is held; otherwise zero.
 * Include: jet_input.h
 * Notes: It does the same as jet_mouse_down but gives back an int, for older cartridge code that expects one. In new code use jet_mouse_down, jet_mouse_pressed and jet_mouse_released.
 * Example:
 *   if (jet_mouse_btn(MOUSE_BTN_LEFT)) {
 *       drag_selected_item();
 *   }
 */
int jet_mouse_btn(int button);

/**
 * Function: jet_mouse_down
 * Summary: Tells you whether a mouse button is held down right now.
 * Parameters:
 *   - button: Which button: one of the MOUSE_BTN_* constants, or MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN for the wheel.
 * Returns: true while the button is down in the latest input snapshot; otherwise false.
 * Include: jet_input.h
 * Notes: It stays true for every tick the button is held, so it suits steady movement and dragging. For one action per click use jet_mouse_pressed. The runtime takes a fresh snapshot of the mouse before every _update, so there is nothing for you to poll.
 * Example:
 *   if (jet_mouse_down(MOUSE_BTN_RIGHT)) {
 *       pan_camera(jet_mouse_dx(), jet_mouse_dy());
 *   }
 */
bool jet_mouse_down(int button);

/**
 * Function: jet_mouse_pressed
 * Summary: Tells you whether a mouse button went down just now.
 * Parameters:
 *   - button: Which button: one of the MOUSE_BTN_* constants, or MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN for the wheel.
 * Returns: true once, on the tick the button goes from up to down, and false on every other tick. Inside _draw it is true if that happened on any tick since the last frame was drawn.
 * Include: jet_input.h
 * Notes: It compares the latest snapshot with the one before, so it is true once per click however long the button is held. The runtime takes a fresh snapshot before every _update.
 * Example:
 *   if (jet_mouse_pressed(MOUSE_BTN_LEFT)) {
 *       spawn_click_effect(jet_mouse_x(), jet_mouse_y());
 *   }
 */
bool jet_mouse_pressed(int button);

/**
 * Function: jet_mouse_wheel
 * Summary: How far the scroll wheel turned since the last input snapshot.
 * Parameters:
 *   - None.
 * Returns: The amount, in notches, for this tick. Positive means the wheel rolled up, away from the player; negative means down. 0.0 when the wheel did not move. One click of a real wheel is about 1.0; a smooth trackpad gives fractions.
 * Include: jet_input.h
 * Notes: For steps, such as changing weapon or moving through a menu, jet_mouse_pressed(MOUSE_WHEEL_UP) and jet_mouse_pressed(MOUSE_WHEEL_DOWN) are easier. Use this amount for something smooth, such as a zoom or a volume. The runtime takes a fresh snapshot before every _update.
 * Example:
 *   camera_zoom += jet_mouse_wheel() * 0.1f;
 */
float jet_mouse_wheel(void);

/**
 * Function: jet_mouse_wheel_x
 * Summary: How far the wheel scrolled sideways since the last input snapshot.
 * Parameters:
 *   - None.
 * Returns: The amount, in notches, for this tick. Positive means scrolled right; negative means left. 0.0 when nothing moved.
 * Include: jet_input.h
 * Notes: Sideways scrolling comes from a wheel that tilts, or from a trackpad. The runtime takes a fresh snapshot before every _update.
 * Example:
 *   timeline_pan += jet_mouse_wheel_x() * 4.0f;
 */
float jet_mouse_wheel_x(void);

/**
 * Function: jet_mouse_button_name
 * Summary: Gives you a short name for a mouse button, for a control legend or a rebinding screen.
 * Parameters:
 *   - button: One of the MOUSE_BTN_* constants, or MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN.
 * Returns: A fixed string such as "Mouse L", "Mouse 4" or "Wheel Up". A number that is not a button gives "Mouse ?".
 * Include: jet_input.h
 * Notes: The mouse's twin of jet_pad_button_name. The input binding screen uses it.
 * Example:
 *   jet_print(jet_mouse_button_name(MOUSE_BTN_X1), 10, 10, 15);
 */
const char* jet_mouse_button_name(int button);

/**
 * Function: jet_mouse_released
 * Summary: Tells you whether a mouse button came up just now.
 * Parameters:
 *   - button: Which button: one of the MOUSE_BTN_* constants, or MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN for the wheel.
 * Returns: true once, on the tick the button goes from down to up, and false on every other tick. Inside _draw it is true if that happened on any tick since the last frame was drawn.
 * Include: jet_input.h
 * Notes: It compares the latest snapshot with the one before, so it is true once for each release. The runtime takes a fresh snapshot before every _update.
 * Example:
 *   if (jet_mouse_released(MOUSE_BTN_LEFT)) {
 *       finish_drag();
 *   }
 */
bool jet_mouse_released(int button);

// ==========================================
// THE KEYBOARD (read from the 512-byte KBD map)
// ==========================================

/**
 * Function: jet_key_down
 * Summary: Tells you whether a key is held down right now.
 * Parameters:
 *   - key: Which key: one of the JET_KEY_* constants, or the ASCII code of the character on the key, such as 'A'.
 * Returns: true while the key is down in the latest keyboard snapshot; otherwise false.
 * Include: jet_input.h
 * Notes: It stays true for every tick the key is held, so it suits steady movement. For one action per press use jet_key_pressed. A code that is not a real key gives false and nothing goes wrong. The runtime takes a fresh snapshot of the keyboard before every _update, so there is nothing for you to poll.
 * Example:
 *   if (jet_key_down(JET_KEY_LEFT)) {
 *       player_x -= speed * dt;
 *   }
 */
bool jet_key_down(int key);

/**
 * Function: jet_key_pressed
 * Summary: Tells you whether a key went down just now.
 * Parameters:
 *   - key: Which key: one of the JET_KEY_* constants, or the ASCII code of the character on the key, such as 'A'.
 * Returns: true once, on the tick the key goes from up to down, and false on every other tick. Inside _draw it is true if that happened on any tick since the last frame was drawn.
 * Include: jet_input.h
 * Notes: It compares the latest snapshot with the one before, so it is true once per press however long the key is held. A code that is not a real key gives false and nothing goes wrong. The runtime takes a fresh snapshot before every _update.
 * Example:
 *   if (jet_key_pressed(JET_KEY_ENTER)) {
 *       accept_menu_choice();
 *   }
 */
bool jet_key_pressed(int key);

/**
 * Function: jet_key_released
 * Summary: Tells you whether a key came up just now.
 * Parameters:
 *   - key: Which key: one of the JET_KEY_* constants, or the ASCII code of the character on the key, such as 'A'.
 * Returns: true once, on the tick the key goes from down to up, and false on every other tick. Inside _draw it is true if that happened on any tick since the last frame was drawn.
 * Include: jet_input.h
 * Notes: It compares the latest snapshot with the one before, so it is true once for each release. A code that is not a real key gives false and nothing goes wrong. The runtime takes a fresh snapshot before every _update.
 * Example:
 *   if (jet_key_released(JET_KEY_SPACE)) {
 *       charge_shot_release();
 *   }
 */
bool jet_key_released(int key);

/**
 * Function: jet_any_key_down
 * Summary: Tells you whether the player is holding any key at all.
 * Parameters:
 *   - None.
 * Returns: true while at least one key is down in the latest keyboard snapshot; otherwise false.
 * Include: jet_input.h
 * Notes: The keyboard's level asked whole. It reads the same snapshot jet_key_down reads, so the two can never disagree, and it reads all of it at once rather than asking about 256 keys one at a time. It stays true for as long as anything is held and falls only when the last key comes up, which suits "hold anything to keep the attract mode awake". A prompt that waits for the player to press something wants jet_any_key_pressed instead: a level is answered the instant the prompt appears, by a key the player was already holding. Every key counts, the modifiers included: a player resting a finger on Shift is holding a key. The two GUI keys are the exception: Command and the Windows key belong to the desktop (Command+Tab, the Start menu), so a player leaving the window must not answer the prompt they leave behind, and JET_KEY_LGUI and JET_KEY_RGUI never count here. To exclude some, ask jet_key_down about the ones you mean instead.
 * Example:
 *   if (jet_any_key_down()) {
 *       attract_idle_timer = 0;
 *   }
 */
bool jet_any_key_down(void);

/**
 * Function: jet_any_key_pressed
 * Summary: Tells you whether the player just pressed any key at all.
 * Parameters:
 *   - None.
 * Returns: true on every tick at least one key goes from up to down, and false on every other tick. Inside _draw it is true if that happened to any key since the last frame was drawn.
 * Include: jet_input.h
 * Notes: This is the "press any key to continue" question, and it wants an edge rather than a level: a prompt that asks jet_any_key_down is dismissed the moment it appears if the player is still holding the key that got them there, while this one waits for a fresh press. It reads the same edges jet_key_pressed reads, so the two can never disagree, and it reads all of them at once rather than asking about 256 keys one at a time. Every key counts, the modifiers included, except the two GUI keys: Command+Tab away from a title screen must not dismiss it.
 * Example:
 *   if (nag_visible && jet_any_key_pressed()) {
 *       nag_visible = false;
 *   }
 */
bool jet_any_key_pressed(void);

/**
 * Function: jet_any_key_released
 * Summary: Tells you whether the player just let go of any key at all.
 * Parameters:
 *   - None.
 * Returns: true on every tick at least one key goes from down to up, and false on every other tick. Inside _draw it is true if that happened to any key since the last frame was drawn.
 * Include: jet_input.h
 * Notes: The third of the trio, beside jet_any_key_down and jet_any_key_pressed, reading the same edges jet_key_released reads. Use it where letting go is itself the act, such as a charge that flies when the player stops holding whatever they were holding. It answers for EACH key let go, so it can be true while other keys are still down; ask jet_any_key_down when what you mean is "the keyboard is empty now". Every key counts, the modifiers included, except the two GUI keys, which the desktop keeps for itself.
 * Example:
 *   if (charging && jet_any_key_released()) {
 *       fire_charged_shot();
 *   }
 */
bool jet_any_key_released(void);

/**
 * Function: jet_read_char
 * Summary: Takes the next typed character out of the cartridge's text-input queue.
 * Parameters:
 *   - None.
 * Returns: The next character the player typed, or 0 when nothing is waiting. A character is a printable ASCII code from 32 to 126, or the code of the editing key that typed it: JET_KEY_ENTER (10), JET_KEY_BACKSPACE (8) or JET_KEY_TAB (9).
 * Include: jet_input.h
 * Notes: Use it for a text field, a command prompt, or typing a name in a game. It gives you the character the host composed for the player's keyboard layout, not which physical key was pressed, and a held key repeats its character as the host repeats it. A character with no ASCII code, such as an accented letter, is not delivered at all, and neither is a key that types nothing, such as Escape or an arrow: read those with jet_key_pressed. Every runtime delivers the same codes for the same typing. To ask whether a particular key is down, use jet_key_down instead.
 * Example:
 *   int c;
 *   while ((c = jet_read_char()) != 0) {
 *       text_buffer_append((char)c);
 *   }
 */
int jet_read_char(void);

/**
 * Function: jet_key_name
 * Summary: Gives you a short name for a key, for a control legend or a rebinding screen.
 * Parameters:
 *   - key: Which key: one of the JET_KEY_* constants, or the ASCII code of the character on the key.
 * Returns: A fixed string such as "Space", "PgUp", "KP 8" or "F10", or the character itself for a printable ASCII key. A code the keyboard cannot produce gives "Unk".
 * Include: jet_input.h
 * Notes: The keyboard's twin of jet_mouse_button_name and jet_pad_button_name, and the one place a key is spelt: the built-in rebinding overlay in jet_input_binds.h prints exactly what this gives. A printable key's, a keypad digit's or a function key's name comes back in a small shared buffer, so copy it or draw it before you call jet_key_name again. The GUI keys are spelt as the player's own keyboard labels them: "Cmd" on a Mac, "Super" on Linux and "Win" everywhere else (see jet_keyboard_kind).
 * Example:
 *   jet_print(jet_key_name(JET_KEY_KP_8), 10, 10, 15);
 */
const char* jet_key_name(int key);

// --- KEYBOARD KINDS ---
// The kind of keyboard the player has, as jet_keyboard_kind answers it. The
// host states it once when the cartridge boots: the Workstation and the native
// player from the computer they run on, the web player from the computer the
// browser says it runs on. A Mac gives commands with Command; the others give
// them with Control.

/**
 * Constant: JET_KEYBOARD_PC
 * Summary: A Windows keyboard, or any keyboard the host could not name: Ctrl gives commands.
 * Include: jet_input.h
 * Notes: The power-on answer, and the one the agent door's runs stand in unless a scenario names another, so a recorded run reads the same on every machine. jet_key_name spells the GUI keys "Win" on it.
 * Example:
 *   if (jet_keyboard_kind() == JET_KEYBOARD_PC) jet_print("Ctrl+S saves", 8, 8, 15);
 */
#define JET_KEYBOARD_PC 0

/**
 * Constant: JET_KEYBOARD_MAC
 * Summary: A Mac keyboard (an iPad's too): Command gives commands.
 * Include: jet_input.h
 * Notes: On this keyboard jet_key_command_down reads JET_KEY_LGUI and JET_KEY_RGUI, the Command keys, and jet_key_name spells them "Cmd". Control is still an ordinary key a cartridge may read.
 * Example:
 *   const char* save_hint = jet_keyboard_kind() == JET_KEYBOARD_MAC ? "Cmd+S" : "Ctrl+S";
 */
#define JET_KEYBOARD_MAC 1

/**
 * Constant: JET_KEYBOARD_LINUX
 * Summary: A Linux keyboard: Ctrl gives commands, and the GUI key is called Super.
 * Include: jet_input.h
 * Notes: It gives commands exactly as JET_KEYBOARD_PC does; it differs only in jet_key_name spelling the GUI keys "Super".
 * Example:
 *   bool ctrl_commands = jet_keyboard_kind() != JET_KEYBOARD_MAC;
 */
#define JET_KEYBOARD_LINUX 2

/**
 * Function: jet_keyboard_kind
 * Summary: Tells you what kind of keyboard the player has: a PC's, a Mac's or a Linux machine's.
 * Parameters:
 *   - None.
 * Returns: JET_KEYBOARD_PC, JET_KEYBOARD_MAC or JET_KEYBOARD_LINUX.
 * Include: jet_input.h
 * Notes: The host states it once when the cartridge boots and it never changes while the cartridge runs, so read it whenever you like. Most cartridges never need it: jet_key_command_down already reads the right key, and jet_key_name already spells it. Ask it when a legend or a help screen has to say the whole shortcut in words of its own.
 * Example:
 *   bool mac = jet_keyboard_kind() == JET_KEYBOARD_MAC;
 *   jet_print(mac ? "Cmd+S saves" : "Ctrl+S saves", 8, 8, 15);
 */
int jet_keyboard_kind(void);

/**
 * Function: jet_key_command_down
 * Summary: Tells you whether the player is holding their keyboard's command key: Command on a Mac, Ctrl everywhere else.
 * Parameters:
 *   - None.
 * Returns: true while either Command key (JET_KEY_LGUI or JET_KEY_RGUI) is down on a Mac keyboard, or either Control key (JET_KEY_LCTRL or JET_KEY_RCTRL) on any other; otherwise false.
 * Include: jet_input.h
 * Notes: Put every shortcut on it and the same line of C is Ctrl+S on Windows and Linux and Command+S on a Mac, which is where each player's fingers already go. It reads the same snapshot jet_key_down reads. Pair it with jet_key_pressed for the letter, so the action happens once per press. The desktop keeps some Command and Ctrl chords for itself (Command+Q, Command+Tab, Command+H on a Mac), so leave those alone.
 * Example:
 *   if (jet_key_command_down() && jet_key_pressed(JET_KEY_S)) {
 *       save_document();
 *   }
 */
bool jet_key_command_down(void);

// --- KEYBOARD CONSTANTS ---
// A key's code is its ASCII code wherever it has one: the letters, the digits
// and the common punctuation keys, so 'A' and JET_KEY_A are the same number.
// The named keys carry on past ASCII: the arrows, the modifiers, the
// navigation cluster, and the numeric keypad.

/**
 * Constant: JET_KEY_BACKSPACE
 * Summary: The Backspace key. Code 8, which is ASCII backspace.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "Bksp". Read this constant when Backspace is a game control, such as erasing a stroke. For a text field use jet_read_char instead: it delivers the same code 8 through the typed-character queue, and the host's own key repeat does the work for you.
 * Example:
 *   if (jet_key_pressed(JET_KEY_BACKSPACE)) erase_last_stroke();
 */
#define JET_KEY_BACKSPACE  8

/**
 * Constant: JET_KEY_TAB
 * Summary: The Tab key. Code 9, which is ASCII horizontal tab.
 * Include: jet_input.h
 * Notes: Do not make Tab a control in your cartridge. The runtime passes it on like any other key, but the Workstation's shell claims Tab for moving focus between its panels: one press hands the keyboard to the shell, and the cartridge stops hearing EVERY key until the player clicks the game picture again. An exported player has no shell and no such trap, so a Tab binding seems fine everywhere except the very machine the cartridge is made on. If Tab really is the player's key, give the same action a second key the Workstation can hear, and name both wherever the cartridge teaches its controls. A text field still receives a typed Tab as code 9 through jet_read_char.
 * Example:
 *   jet_print(jet_key_name(JET_KEY_TAB), 10, 10, 15); // a legend, not a binding
 */
#define JET_KEY_TAB        9

/**
 * Constant: JET_KEY_ENTER
 * Summary: The main Enter (or Return) key. Code 10, which is ASCII newline, not 13.
 * Include: jet_input.h
 * Notes: The keypad's Enter is a separate key on every runtime, JET_KEY_KP_ENTER, so a menu that should accept either must read both. jet_read_char also delivers code 10 when the player types a newline.
 * Example:
 *   if (jet_key_pressed(JET_KEY_ENTER) || jet_key_pressed(JET_KEY_KP_ENTER)) accept_menu_choice();
 */
#define JET_KEY_ENTER      10

/**
 * Constant: JET_KEY_ESCAPE
 * Summary: The Escape key. Code 27, which is ASCII escape.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "Esc". This is the usual key for back, cancel and pause, and the first one players reach for when they want out of a screen, so a cartridge that opens a menu should let Escape close it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_ESCAPE)) paused = !paused;
 */
#define JET_KEY_ESCAPE     27

/**
 * Constant: JET_KEY_SPACE
 * Summary: The Space bar. Code 32, which is ASCII space, so jet_key_down(' ') and jet_key_down(JET_KEY_SPACE) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the usual key for jump, fire and advance. jet_key_pressed gives one action per push; jet_key_down would fire on every one of the 120 ticks a second for as long as the bar is held. A text field sees the same code 32 through jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_SPACE)) jump();
 */
#define JET_KEY_SPACE      32

// The letters, A to Z, at their ASCII codes 65 to 90.

/**
 * Constant: JET_KEY_A
 * Summary: The A key. Code 65, which is ASCII 'A', so jet_key_down('A') and jet_key_down(JET_KEY_A) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_down(JET_KEY_A)) player_x -= speed * dt;
 */
#define JET_KEY_A          65

/**
 * Constant: JET_KEY_B
 * Summary: The B key. Code 66, which is ASCII 'B', so jet_key_down('B') and jet_key_down(JET_KEY_B) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_B)) toggle_build_menu();
 */
#define JET_KEY_B          66

/**
 * Constant: JET_KEY_C
 * Summary: The C key. Code 67, which is ASCII 'C', so jet_key_down('C') and jet_key_down(JET_KEY_C) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_C)) crouching = !crouching;
 */
#define JET_KEY_C          67

/**
 * Constant: JET_KEY_D
 * Summary: The D key. Code 68, which is ASCII 'D', so jet_key_down('D') and jet_key_down(JET_KEY_D) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_down(JET_KEY_D)) player_x += speed * dt;
 */
#define JET_KEY_D          68

/**
 * Constant: JET_KEY_E
 * Summary: The E key. Code 69, which is ASCII 'E', so jet_key_down('E') and jet_key_down(JET_KEY_E) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_E)) interact_with_facing_tile();
 */
#define JET_KEY_E          69

/**
 * Constant: JET_KEY_F
 * Summary: The F key. Code 70, which is ASCII 'F', so jet_key_down('F') and jet_key_down(JET_KEY_F) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_F)) toggle_torch();
 */
#define JET_KEY_F          70

/**
 * Constant: JET_KEY_G
 * Summary: The G key. Code 71, which is ASCII 'G', so jet_key_down('G') and jet_key_down(JET_KEY_G) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_G)) drop_held_item();
 */
#define JET_KEY_G          71

/**
 * Constant: JET_KEY_H
 * Summary: The H key. Code 72, which is ASCII 'H', so jet_key_down('H') and jet_key_down(JET_KEY_H) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_H)) toggle_help_overlay();
 */
#define JET_KEY_H          72

/**
 * Constant: JET_KEY_I
 * Summary: The I key. Code 73, which is ASCII 'I', so jet_key_down('I') and jet_key_down(JET_KEY_I) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_I)) toggle_inventory();
 */
#define JET_KEY_I          73

/**
 * Constant: JET_KEY_J
 * Summary: The J key. Code 74, which is ASCII 'J', so jet_key_down('J') and jet_key_down(JET_KEY_J) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_J)) toggle_journal();
 */
#define JET_KEY_J          74

/**
 * Constant: JET_KEY_K
 * Summary: The K key. Code 75, which is ASCII 'K', so jet_key_down('K') and jet_key_down(JET_KEY_K) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_K)) toggle_skill_page();
 */
#define JET_KEY_K          75

/**
 * Constant: JET_KEY_L
 * Summary: The L key. Code 76, which is ASCII 'L', so jet_key_down('L') and jet_key_down(JET_KEY_L) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_L)) toggle_map_legend();
 */
#define JET_KEY_L          76

/**
 * Constant: JET_KEY_M
 * Summary: The M key. Code 77, which is ASCII 'M', so jet_key_down('M') and jet_key_down(JET_KEY_M) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_M)) toggle_minimap();
 */
#define JET_KEY_M          77

/**
 * Constant: JET_KEY_N
 * Summary: The N key. Code 78, which is ASCII 'N', so jet_key_down('N') and jet_key_down(JET_KEY_N) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_N)) name_current_waypoint();
 */
#define JET_KEY_N          78

/**
 * Constant: JET_KEY_O
 * Summary: The O key. Code 79, which is ASCII 'O', so jet_key_down('O') and jet_key_down(JET_KEY_O) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_O)) open_facing_door();
 */
#define JET_KEY_O          79

/**
 * Constant: JET_KEY_P
 * Summary: The P key. Code 80, which is ASCII 'P', so jet_key_down('P') and jet_key_down(JET_KEY_P) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_P)) paused = !paused;
 */
#define JET_KEY_P          80

/**
 * Constant: JET_KEY_Q
 * Summary: The Q key. Code 81, which is ASCII 'Q', so jet_key_down('Q') and jet_key_down(JET_KEY_Q) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_down(JET_KEY_Q)) camera_angle -= turn_rate * dt;
 */
#define JET_KEY_Q          81

/**
 * Constant: JET_KEY_R
 * Summary: The R key. Code 82, which is ASCII 'R', so jet_key_down('R') and jet_key_down(JET_KEY_R) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_R)) reload_weapon();
 */
#define JET_KEY_R          82

/**
 * Constant: JET_KEY_S
 * Summary: The S key. Code 83, which is ASCII 'S', so jet_key_down('S') and jet_key_down(JET_KEY_S) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_down(JET_KEY_S)) player_y += speed * dt;
 */
#define JET_KEY_S          83

/**
 * Constant: JET_KEY_T
 * Summary: The T key. Code 84, which is ASCII 'T', so jet_key_down('T') and jet_key_down(JET_KEY_T) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_T)) open_chat_line();
 */
#define JET_KEY_T          84

/**
 * Constant: JET_KEY_U
 * Summary: The U key. Code 85, which is ASCII 'U', so jet_key_down('U') and jet_key_down(JET_KEY_U) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_U)) use_selected_item();
 */
#define JET_KEY_U          85

/**
 * Constant: JET_KEY_V
 * Summary: The V key. Code 86, which is ASCII 'V', so jet_key_down('V') and jet_key_down(JET_KEY_V) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_down(JET_KEY_V)) camera_angle += turn_rate * dt;
 */
#define JET_KEY_V          86

/**
 * Constant: JET_KEY_W
 * Summary: The W key. Code 87, which is ASCII 'W', so jet_key_down('W') and jet_key_down(JET_KEY_W) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_down(JET_KEY_W)) player_y -= speed * dt;
 */
#define JET_KEY_W          87

/**
 * Constant: JET_KEY_X
 * Summary: The X key. Code 88, which is ASCII 'X', so jet_key_down('X') and jet_key_down(JET_KEY_X) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_X)) swap_weapon_set();
 */
#define JET_KEY_X          88

/**
 * Constant: JET_KEY_Y
 * Summary: The Y key. Code 89, which is ASCII 'Y', so jet_key_down('Y') and jet_key_down(JET_KEY_Y) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_Y)) confirm_prompt();
 */
#define JET_KEY_Y          89

/**
 * Constant: JET_KEY_Z
 * Summary: The Z key. Code 90, which is ASCII 'Z', so jet_key_down('Z') and jet_key_down(JET_KEY_Z) mean the same thing.
 * Include: jet_input.h
 * Notes: A letter key is known by the key itself, not by the character the player's keyboard layout prints. Neither Shift nor Caps Lock changes the code, so the read is the same on the Workstation, the native player and the web player. Use this constant when the letter is a control; when the player is typing words, use jet_read_char instead.
 * Example:
 *   if (jet_key_pressed(JET_KEY_Z)) undo_last_move();
 */
#define JET_KEY_Z          90

// The punctuation keys that have a name. Every other punctuation key can be
// read by its own ASCII character, jet_key_down('-'), jet_key_down('/'),
// jet_key_down('['), so only the ones a cartridge usually spells by name get
// a constant.
//
// A digit or punctuation key held with Shift reports its shifted character,
// and only that one. While Shift is down, the ; key reads as ':' (58) and
// JET_KEY_SEMICOLON is NOT down. The character is fixed at the moment of the
// press, so letting go of Shift in the middle of a hold never swaps the code
// your cartridge is reading, and every runtime works it out the same way.
// Letter keys are known by the key itself, so Shift and Caps Lock leave them
// alone, and so do the arrows, the modifiers, the navigation cluster and the
// whole keypad. Typed text is a separate promise: jet_read_char still
// delivers whatever the player's layout actually produces.

/**
 * Constant: JET_KEY_GRAVE
 * Summary: The ` backquote key. Code 96, which is ASCII backquote. On most keyboards this is the console key.
 * Include: jet_input.h
 * Notes: This is the key's unshifted character, on every runtime. A shifted press reports JET_KEY_TILDE instead and this code stays up, so a console the player must always be able to open reads both, or reads this one and teaches the bare key. Punctuation Jet64 gives no name to is still readable by its own character, jet_key_down('-') or jet_key_down('/').
 * Example:
 *   if (jet_key_pressed(JET_KEY_GRAVE)) toggle_console();
 */
#define JET_KEY_GRAVE      96

/**
 * Constant: JET_KEY_TILDE
 * Summary: The ~ tilde. Code 126, which is ASCII tilde. It is the shifted character of the backquote key.
 * Include: jet_input.h
 * Notes: This is the code every runtime reports in place of JET_KEY_GRAVE while Shift is held, on the Workstation, the native player and the web player alike. It is one key with two characters, never two keys: exactly one of the pair is down at any moment. Read both when either should open the console.
 * Example:
 *   if (jet_key_pressed(JET_KEY_TILDE) || jet_key_pressed(JET_KEY_GRAVE)) toggle_console();
 */
#define JET_KEY_TILDE      126

/**
 * Constant: JET_KEY_SEMICOLON
 * Summary: The ; semicolon key. Code 59, which is ASCII ';', so jet_key_down(';') and jet_key_down(JET_KEY_SEMICOLON) mean the same thing.
 * Include: jet_input.h
 * Notes: It has a name because a command line usually wants it. A shifted press reports ':' (58) instead and this code is not down, so a cartridge that wants the colon reads jet_key_pressed(':') rather than watching JET_KEY_LSHIFT or JET_KEY_RSHIFT beside the semicolon.
 * Example:
 *   if (jet_key_pressed(JET_KEY_SEMICOLON)) open_command_line();
 */
#define JET_KEY_SEMICOLON  59

// The top row of digits, 0 to 9, at their ASCII codes 48 to 57. The keypad's
// digits are separate keys with their own codes, further down.

/**
 * Constant: JET_KEY_0
 * Summary: The top-row 0 key. Code 48, which is ASCII '0', so jet_key_down('0') and jet_key_down(JET_KEY_0) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_0 is its own key on every runtime. A shifted press reports ')' (41) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed(')') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_0)) select_hotbar_slot(9);
 */
#define JET_KEY_0          48

/**
 * Constant: JET_KEY_1
 * Summary: The top-row 1 key. Code 49, which is ASCII '1', so jet_key_down('1') and jet_key_down(JET_KEY_1) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_1 is its own key on every runtime. A shifted press reports '!' (33) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('!') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_1)) select_hotbar_slot(0);
 */
#define JET_KEY_1          49

/**
 * Constant: JET_KEY_2
 * Summary: The top-row 2 key. Code 50, which is ASCII '2', so jet_key_down('2') and jet_key_down(JET_KEY_2) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_2 is its own key on every runtime. A shifted press reports '@' (64) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('@') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_2)) select_hotbar_slot(1);
 */
#define JET_KEY_2          50

/**
 * Constant: JET_KEY_3
 * Summary: The top-row 3 key. Code 51, which is ASCII '3', so jet_key_down('3') and jet_key_down(JET_KEY_3) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_3 is its own key on every runtime. A shifted press reports '#' (35) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('#') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_3)) select_hotbar_slot(2);
 */
#define JET_KEY_3          51

/**
 * Constant: JET_KEY_4
 * Summary: The top-row 4 key. Code 52, which is ASCII '4', so jet_key_down('4') and jet_key_down(JET_KEY_4) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_4 is its own key on every runtime. A shifted press reports '$' (36) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('$') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_4)) select_hotbar_slot(3);
 */
#define JET_KEY_4          52

/**
 * Constant: JET_KEY_5
 * Summary: The top-row 5 key. Code 53, which is ASCII '5', so jet_key_down('5') and jet_key_down(JET_KEY_5) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_5 is its own key on every runtime. A shifted press reports '%' (37) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('%') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_5)) select_hotbar_slot(4);
 */
#define JET_KEY_5          53

/**
 * Constant: JET_KEY_6
 * Summary: The top-row 6 key. Code 54, which is ASCII '6', so jet_key_down('6') and jet_key_down(JET_KEY_6) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_6 is its own key on every runtime. A shifted press reports '^' (94) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('^') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_6)) select_hotbar_slot(5);
 */
#define JET_KEY_6          54

/**
 * Constant: JET_KEY_7
 * Summary: The top-row 7 key. Code 55, which is ASCII '7', so jet_key_down('7') and jet_key_down(JET_KEY_7) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_7 is its own key on every runtime. A shifted press reports '&' (38) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('&') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_7)) select_hotbar_slot(6);
 */
#define JET_KEY_7          55

/**
 * Constant: JET_KEY_8
 * Summary: The top-row 8 key. Code 56, which is ASCII '8', so jet_key_down('8') and jet_key_down(JET_KEY_8) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_8 is its own key on every runtime. A shifted press reports '*' (42) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('*') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_8)) select_hotbar_slot(7);
 */
#define JET_KEY_8          56

/**
 * Constant: JET_KEY_9
 * Summary: The top-row 9 key. Code 57, which is ASCII '9', so jet_key_down('9') and jet_key_down(JET_KEY_9) mean the same thing.
 * Include: jet_input.h
 * Notes: This is the top row, never the keypad: JET_KEY_KP_9 is its own key on every runtime. A shifted press reports '(' (40) instead and this code is not down, so a hotbar that should still answer when the player fumbles Shift reads jet_key_pressed('(') beside it.
 * Example:
 *   if (jet_key_pressed(JET_KEY_9)) select_hotbar_slot(8);
 */
#define JET_KEY_9          57

// Past ASCII, the named keys. An arrow, a modifier or a navigation key prints
// no character, so none of them ever arrives through jet_read_char.

/**
 * Constant: JET_KEY_UP
 * Summary: The Up arrow key. Code 128, the first code past ASCII.
 * Include: jet_input.h
 * Notes: jet_key_down keeps something moving for as long as the arrow is held; jet_key_pressed steps a menu exactly one row per push. The arrows are the one set of controls every player already knows, so a cartridge does well to accept them alongside whatever else it binds.
 * Example:
 *   if (jet_key_pressed(JET_KEY_UP)) menu_index--;
 */
#define JET_KEY_UP         128

/**
 * Constant: JET_KEY_DOWN
 * Summary: The Down arrow key. Code 129.
 * Include: jet_input.h
 * Notes: The twin of JET_KEY_UP. A held arrow does not repeat by itself: jet_key_down is true on every one of the 120 ticks a second, so a menu that should keep stepping while the key is held must time its own steps.
 * Example:
 *   if (jet_key_pressed(JET_KEY_DOWN)) menu_index++;
 */
#define JET_KEY_DOWN       129

/**
 * Constant: JET_KEY_LEFT
 * Summary: The Left arrow key. Code 130.
 * Include: jet_input.h
 * Notes: With JET_KEY_RIGHT it makes the simplest movement control there is: on or off, exact, and the same on every keyboard, which is why grid games prefer it to an analogue stick.
 * Example:
 *   if (jet_key_down(JET_KEY_LEFT)) player_x -= speed * dt;
 */
#define JET_KEY_LEFT       130

/**
 * Constant: JET_KEY_RIGHT
 * Summary: The Right arrow key. Code 131.
 * Include: jet_input.h
 * Notes: The twin of JET_KEY_LEFT. jet_bind_key in jet_input_binds.h can fold it into a named action, so the player can rebind it without the cartridge knowing.
 * Example:
 *   if (jet_key_down(JET_KEY_RIGHT)) player_x += speed * dt;
 */
#define JET_KEY_RIGHT      131

// The modifiers. Left and right are separate keys, and neither stands in for
// the other, so a cartridge that will accept either must read both.

/**
 * Constant: JET_KEY_LSHIFT
 * Summary: The left Shift key. Code 132.
 * Include: jet_input.h
 * Notes: A modifier is just a key that is down or up, so read it in the same tick as the key it goes with. The right Shift is a different key, JET_KEY_RSHIFT, so a cartridge that accepts either must test both.
 * Example:
 *   if (jet_key_down(JET_KEY_LSHIFT) && jet_key_down(JET_KEY_W)) sprint();
 */
#define JET_KEY_LSHIFT     132

/**
 * Constant: JET_KEY_RSHIFT
 * Summary: The right Shift key. Code 133.
 * Include: jet_input.h
 * Notes: The twin of JET_KEY_LSHIFT, reported separately so a cartridge can tell the two hands apart. Neither Shift changes a letter's code, so JET_KEY_W reads the same with Shift held or not.
 * Example:
 *   bool shift = jet_key_down(JET_KEY_LSHIFT) || jet_key_down(JET_KEY_RSHIFT);
 */
#define JET_KEY_RSHIFT     133

/**
 * Constant: JET_KEY_LCTRL
 * Summary: The left Control key. Code 134.
 * Include: jet_input.h
 * Notes: This is the usual modifier for an action the player should have to mean, such as saving or deleting. The Workstation has Ctrl shortcuts of its own, so a cartridge that must work there should offer a plain-key way to do anything it puts on a Ctrl combination.
 * Example:
 *   if (jet_key_down(JET_KEY_LCTRL) && jet_key_pressed(JET_KEY_S)) save_game();
 */
#define JET_KEY_LCTRL      134

/**
 * Constant: JET_KEY_RCTRL
 * Summary: The right Control key. Code 135.
 * Include: jet_input.h
 * Notes: The twin of JET_KEY_LCTRL. Test both sides for a combination the player should be able to reach comfortably with either hand.
 * Example:
 *   bool ctrl = jet_key_down(JET_KEY_LCTRL) || jet_key_down(JET_KEY_RCTRL);
 */
#define JET_KEY_RCTRL      135

/**
 * Constant: JET_KEY_LALT
 * Summary: The left Alt key, called Option on a Mac. Code 136.
 * Include: jet_input.h
 * Notes: The desktop claims Alt more than any other modifier, for dragging windows and opening menus by letter, so use it for something handy rather than for an action the player cannot do without.
 * Example:
 *   if (jet_key_down(JET_KEY_LALT)) show_item_labels();
 */
#define JET_KEY_LALT       136

/**
 * Constant: JET_KEY_RALT
 * Summary: The right Alt key. Code 137.
 * Include: jet_input.h
 * Notes: On many European keyboard layouts this key is AltGr, and the player uses it to type, so bind the left Alt by preference and let jet_bind_key offer a rebind rather than assuming a spare modifier.
 * Example:
 *   if (jet_key_down(JET_KEY_RALT)) show_item_labels();
 */
#define JET_KEY_RALT       137

// The navigation cluster, which is Delete, Insert, Home, End, Page Up and
// Page Down.

/**
 * Constant: JET_KEY_DELETE
 * Summary: The Delete key. Code 138.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "Del". It is not JET_KEY_BACKSPACE. Delete prints no character and never reaches jet_read_char, so a text field must act on this key itself.
 * Example:
 *   if (jet_key_pressed(JET_KEY_DELETE)) delete_selected_entry();
 */
#define JET_KEY_DELETE     138

/**
 * Constant: JET_KEY_INSERT
 * Summary: The Insert key. Code 139.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "Ins". Many laptops have no Insert key, so treat it as an extra and always leave a second way to reach the same action.
 * Example:
 *   if (jet_key_pressed(JET_KEY_INSERT)) overwrite_mode = !overwrite_mode;
 */
#define JET_KEY_INSERT     139

/**
 * Constant: JET_KEY_HOME
 * Summary: The Home key. Code 140.
 * Include: jet_input.h
 * Notes: This is the usual key for jumping to the start. With JET_KEY_END it gives a long list two instant ends, which a scroll wheel alone cannot.
 * Example:
 *   if (jet_key_pressed(JET_KEY_HOME)) scroll_offset = 0;
 */
#define JET_KEY_HOME       140

/**
 * Constant: JET_KEY_END
 * Summary: The End key. Code 141.
 * Include: jet_input.h
 * Notes: The twin of JET_KEY_HOME, and the key a message log wants when new lines have pushed the view off the bottom.
 * Example:
 *   if (jet_key_pressed(JET_KEY_END)) scroll_offset = last_line;
 */
#define JET_KEY_END        141

/**
 * Constant: JET_KEY_PAGEUP
 * Summary: The Page Up key. Code 142.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "PgUp". A page step is a whole screenful, not one line. Pair it with the wheel buttons MOUSE_WHEEL_UP and MOUSE_WHEEL_DOWN so a player can read a long log either way.
 * Example:
 *   if (jet_key_pressed(JET_KEY_PAGEUP)) scroll_offset -= lines_per_page;
 */
#define JET_KEY_PAGEUP     142

/**
 * Constant: JET_KEY_PAGEDOWN
 * Summary: The Page Down key. Code 143.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "PgDn". The twin of JET_KEY_PAGEUP.
 * Example:
 *   if (jet_key_pressed(JET_KEY_PAGEDOWN)) scroll_offset += lines_per_page;
 */
#define JET_KEY_PAGEDOWN   143

// --- THE NUMERIC KEYPAD ---
// The keypad is separate hardware and gets separate codes: JET_KEY_KP_5 is
// never JET_KEY_5, so a cartridge can give the keypad its own controls (a map
// scroll, a facing, a row of cheats) without taking the top row's. Typed text
// is unaffected. The host puts a keypad digit into the jet_read_char queue
// exactly as it does a top-row digit, and the keypad's Enter still types a
// newline, so a text field keeps working while jet_key_down tells the two
// sets of keys apart.
//
// This is the whole of a standard keypad, and every code here is delivered by
// all three runtimes (the Workstation, the native player and the web player).
// NumLock is the key a Mac keypad prints as Clear.

/**
 * Constant: JET_KEY_KP_0
 * Summary: The keypad's own 0 key. Code 144.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_0, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_0)) pick_up_item();
 */
#define JET_KEY_KP_0        144

/**
 * Constant: JET_KEY_KP_1
 * Summary: The keypad's own 1 key. Code 145.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_1, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_1)) step(-1, +1); // south-west
 */
#define JET_KEY_KP_1        145

/**
 * Constant: JET_KEY_KP_2
 * Summary: The keypad's own 2 key. Code 146.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_2, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_2)) step(0, +1);  // south
 */
#define JET_KEY_KP_2        146

/**
 * Constant: JET_KEY_KP_3
 * Summary: The keypad's own 3 key. Code 147.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_3, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_3)) step(+1, +1); // south-east
 */
#define JET_KEY_KP_3        147

/**
 * Constant: JET_KEY_KP_4
 * Summary: The keypad's own 4 key. Code 148.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_4, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_4)) step(-1, 0);  // west
 */
#define JET_KEY_KP_4        148

/**
 * Constant: JET_KEY_KP_5
 * Summary: The keypad's own 5 key. Code 149.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_5, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_5)) wait_one_turn();
 */
#define JET_KEY_KP_5        149

/**
 * Constant: JET_KEY_KP_6
 * Summary: The keypad's own 6 key. Code 150.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_6, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_6)) step(+1, 0);  // east
 */
#define JET_KEY_KP_6        150

/**
 * Constant: JET_KEY_KP_7
 * Summary: The keypad's own 7 key. Code 151.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_7, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_7)) step(-1, -1); // north-west
 */
#define JET_KEY_KP_7        151

/**
 * Constant: JET_KEY_KP_8
 * Summary: The keypad's own 8 key. Code 152.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_8, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_8)) step(0, -1);  // north
 */
#define JET_KEY_KP_8        152

/**
 * Constant: JET_KEY_KP_9
 * Summary: The keypad's own 9 key. Code 153.
 * Include: jet_input.h
 * Notes: Separate hardware, separate code: this is never JET_KEY_9, so the keypad can carry its own controls while the top row keeps its own. The ten keypad digits run in order with no gaps, so JET_KEY_KP_0 + n is a valid way to pick digit n. Typing is unaffected: a keypad digit still reaches jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_9)) step(+1, -1); // north-east
 */
#define JET_KEY_KP_9        153

/**
 * Constant: JET_KEY_KP_DECIMAL
 * Summary: The keypad's own . key. Code 154.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "KP ." so a legend can never be mistaken for the main row's full stop, which arrives as its own ASCII code 46.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_DECIMAL)) toggle_decimal_readout();
 */
#define JET_KEY_KP_DECIMAL  154 // the pad's own "."

/**
 * Constant: JET_KEY_KP_DIVIDE
 * Summary: The keypad's own / key. Code 155.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "KP /". The main row's slash is ASCII code 47 and a different key altogether.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_DIVIDE)) halve_time_scale();
 */
#define JET_KEY_KP_DIVIDE   155 // the pad's own "/"

/**
 * Constant: JET_KEY_KP_MULTIPLY
 * Summary: The keypad's own * key. Code 156.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "KP *". The four operator keys sit in a column under the player's fingers, which makes them a natural home for a row of speed or zoom controls.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_MULTIPLY)) double_time_scale();
 */
#define JET_KEY_KP_MULTIPLY 156 // the pad's own "*"

/**
 * Constant: JET_KEY_KP_SUBTRACT
 * Summary: The keypad's own - key. Code 157.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "KP -". The main row's minus is ASCII code 45 and a different key.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_SUBTRACT)) zoom_out();
 */
#define JET_KEY_KP_SUBTRACT 157 // the pad's own "-"

/**
 * Constant: JET_KEY_KP_ADD
 * Summary: The keypad's own + key. Code 158.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "KP +". With JET_KEY_KP_SUBTRACT it makes the zoom control players find soonest without being told.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_ADD)) zoom_in();
 */
#define JET_KEY_KP_ADD      158 // the pad's own "+"

/**
 * Constant: JET_KEY_KP_ENTER
 * Summary: The keypad's own Enter key. Code 159, a different key from JET_KEY_ENTER on every runtime.
 * Include: jet_input.h
 * Notes: The keypad's Enter is its own key on the Workstation, the native player and the web player alike, so a menu that must accept either has to read both. Typing is untouched: it still puts a newline in the queue for jet_read_char.
 * Example:
 *   if (jet_key_pressed(JET_KEY_ENTER) || jet_key_pressed(JET_KEY_KP_ENTER)) accept_menu_choice();
 */
#define JET_KEY_KP_ENTER    159 // distinct from JET_KEY_ENTER, on every runtime

/**
 * Constant: JET_KEY_KP_EQUAL
 * Summary: The keypad's own = key. Code 160. Mac keypads have it.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "KP =". Most PC keypads have no such key, so bind it only as an extra.
 * Example:
 *   if (jet_key_pressed(JET_KEY_KP_EQUAL)) reset_zoom();
 */
#define JET_KEY_KP_EQUAL    160 // the pad's own "=" (Mac keypads)

/**
 * Constant: JET_KEY_NUMLOCK
 * Summary: The NumLock key. Code 161. On a Mac keypad it is the key printed "Clear".
 * Include: jet_input.h
 * Notes: jet_key_name spells it "NumLk". Jet64 reports whether a key is down, not the host's lock modes, so a cartridge can see this key pressed but can never read whether the lock is on.
 * Example:
 *   if (jet_key_pressed(JET_KEY_NUMLOCK)) clear_entry();
 */
#define JET_KEY_NUMLOCK     161 // "Clear" on a Mac keypad

// The ten keypad digits run in order with no gaps, so a cartridge may pick one
// as JET_KEY_KP_0 + n, and the web player's key table works out "Numpad<d>"
// from the digit exactly as it works out "Digit<d>" from ASCII.
_Static_assert(JET_KEY_KP_9 - JET_KEY_KP_0 == 9, "the keypad digits must stay contiguous and in order");

// --- THE FUNCTION ROW ---
// F1 to F12, delivered by all three runtimes. They type nothing, so
// jet_read_char never sees them.
//
// On each machine, something else may want these keys too:
//   * The Workstation's own shortcuts live on this row (F5 builds the cart,
//     and the others open its tools), so a press there reaches the cart AND
//     does the shell's work. A cart that must be driven on the Workstation
//     should give such a control a second key as well.
//   * The web player keeps F1 to F10 from the browser, so F5 in a playing cart
//     is the cart's, not a page reload. F11 and F12 still reach the cart, but
//     the browser keeps its fullscreen and developer tools on them too.
//   * A Mac keyboard sends brightness and media keys from this row unless Fn
//     is held, or the system is set to use the standard function keys.

/**
 * Constant: JET_KEY_F1
 * Summary: The F1 key. Code 162.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F1".
 * Example:
 *   if (jet_key_pressed(JET_KEY_F1)) show_help = !show_help;
 */
#define JET_KEY_F1          162

/**
 * Constant: JET_KEY_F2
 * Summary: The F2 key. Code 163.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F2".
 */
#define JET_KEY_F2          163

/**
 * Constant: JET_KEY_F3
 * Summary: The F3 key. Code 164.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F3".
 */
#define JET_KEY_F3          164

/**
 * Constant: JET_KEY_F4
 * Summary: The F4 key. Code 165.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F4".
 */
#define JET_KEY_F4          165

/**
 * Constant: JET_KEY_F5
 * Summary: The F5 key. Code 166.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F5". On the Workstation F5 also rebuilds the running cart, and in a browser it would reload the page, which the web player prevents while a cart is playing.
 * Example:
 *   if (jet_key_pressed(JET_KEY_F5)) quick_save();
 */
#define JET_KEY_F5          166

/**
 * Constant: JET_KEY_F6
 * Summary: The F6 key. Code 167.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F6".
 */
#define JET_KEY_F6          167

/**
 * Constant: JET_KEY_F7
 * Summary: The F7 key. Code 168.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F7".
 */
#define JET_KEY_F7          168

/**
 * Constant: JET_KEY_F8
 * Summary: The F8 key. Code 169.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F8".
 */
#define JET_KEY_F8          169

/**
 * Constant: JET_KEY_F9
 * Summary: The F9 key. Code 170.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F9".
 */
#define JET_KEY_F9          170

/**
 * Constant: JET_KEY_F10
 * Summary: The F10 key. Code 171.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F10".
 */
#define JET_KEY_F10         171

/**
 * Constant: JET_KEY_F11
 * Summary: The F11 key. Code 172.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F11". macOS keeps F11 for showing the desktop unless the system shortcut is turned off.
 */
#define JET_KEY_F11         172

/**
 * Constant: JET_KEY_F12
 * Summary: The F12 key. Code 173.
 * Include: jet_input.h
 * Notes: The twelve function keys run in order with no gaps, so JET_KEY_F1 + n - 1 is Fn. jet_key_name spells it "F12".
 */
#define JET_KEY_F12         173

// The GUI keys: Command on a Mac, the Windows key on a PC, Super on Linux. One
// code serves all three because they are the same key in the same place, named
// as the USB keyboard standard names it. To give a command, read
// jet_key_command_down rather than these: it is Command on a Mac and Ctrl
// everywhere else.

/**
 * Constant: JET_KEY_LGUI
 * Summary: The left GUI key: Command on a Mac, the Windows key on a PC, Super on Linux. Code 174.
 * Include: jet_input.h
 * Notes: jet_key_name spells it "Cmd", "Win" or "Super" as the player's keyboard labels it. The desktop claims this key more than any other: the Windows key opens the Start menu, and a Mac keeps Command+Q, Command+Tab and Command+H. The "any key" questions (jet_any_key_down and its twins) never count it, so leaving the window with Command+Tab answers no prompt. For a shortcut ask jet_key_command_down, which is this key on a Mac and Ctrl elsewhere.
 * Example:
 *   bool gui = jet_key_down(JET_KEY_LGUI) || jet_key_down(JET_KEY_RGUI);
 */
#define JET_KEY_LGUI        174

/**
 * Constant: JET_KEY_RGUI
 * Summary: The right GUI key: Command on a Mac, the Windows key on a PC, Super on Linux. Code 175.
 * Include: jet_input.h
 * Notes: The twin of JET_KEY_LGUI. Many PC keyboards have no right Windows key, and a laptop may have neither, so never make it the only way to do something.
 * Example:
 *   if (jet_key_pressed(JET_KEY_RGUI)) show_shortcut_help();
 */
#define JET_KEY_RGUI        175

// The function keys run in order with no gaps, so a cartridge may pick one as
// JET_KEY_F1 + n - 1, and jet_key_name and the web player's key table both
// work out "F<n>" from the number.
_Static_assert(JET_KEY_F12 - JET_KEY_F1 == 11, "the function keys must stay contiguous and in order");

// The any-key scanner reads the snapshot eight keys to a word and masks the GUI
// keys out of the one word that holds them both.
_Static_assert(JET_KEY_LGUI / sizeof(uint64_t) == JET_KEY_RGUI / sizeof(uint64_t),
               "the two GUI keys must share one eight-key word");

#endif // JET_INPUT_H
