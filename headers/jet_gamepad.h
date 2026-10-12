// runtime/jet_gamepad.h
#ifndef JET_GAMEPAD_H
#define JET_GAMEPAD_H

#include "jet_core.h" // IWYU pragma: keep

// Read up to four gamepads: their buttons, sticks and triggers. Each pad has a
// slot number from 0 to 3, and slot 0 is the first controller that connected.
// Use jet_pad_pressed for something that should happen once per push, such as
// a jump; jet_pad_down for something that lasts as long as the button is held;
// and jet_pad_stick for movement, which hands you a stick as an (x, y) pair
// with the deadzone already taken care of. The deadzone is the small zone
// around a stick's centre that counts as still, so a stick at rest does not
// creep. The "any" calls, such as jet_pad_any_pressed, answer for every
// connected pad at once, which is what a menu wants when any player may press
// a button. Jet64 takes a snapshot of every pad before each _update; your
// cartridge never sets up or reads the controller hardware itself.
//
// A slot number only says what order things were plugged in, so a pad also
// tells you WHICH controller it is: jet_pad_id gives a number that stands for
// the controller itself, the same one every time it is connected, and
// jet_pad_name gives its short name to show on screen. That is what lets a
// game remember one player's fix for one awkward controller without touching
// the settings of the good one beside it — and jet_input_binds.h does exactly
// that for you if you would rather not do it by hand.
//
// Complete analogue-movement example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { PLAYER_RADIUS = 10 };
//   static Vector2 player = { 160.0f, 90.0f };
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       const float speed = 180.0f;
//       Vector2 stick;
//       jet_pad_stick(0, JET_PAD_STICK_LEFT, &stick.x, &stick.y);
//       player = jet_vector2_add(player, jet_vector2_scale(stick, speed * dt));
//       player.x = jet_clampf(player.x, 0.0f, (float)(JET_WIDTH - 1));
//       player.y = jet_clampf(player.y, 0.0f, (float)(JET_HEIGHT - 1));
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_circfill((int)player.x, (int)player.y, PLAYER_RADIUS, 12);
//       jet_print(jet_pad_connected(0) ? "PAD 1 CONNECTED" : "CONNECT A GAMEPAD", 16, 16, 15);
//   }
//
//   void _end(void) {}

// The button numbers. The names follow the Xbox layout, but the four face
// buttons are picked by POSITION, not by the letter printed on them: a
// PlayStation pad's Cross is its bottom face button, so it reports as
// JET_PAD_BTN_A, and the same code works on every pad. For each button the
// hardware keeps two things, whether it is held right now and a count of
// presses, so a tap too quick to last a whole tick is still seen.

/**
 * Constant: JET_PAD_BTN_UP
 * Summary: The button number for Up on the D-pad, the direction pad.
 * Include: jet_gamepad.h
 * Notes: Give it to jet_pad_down to ask whether Up is held, jet_pad_pressed to ask whether it was pushed this tick, or jet_pad_released to ask whether it was let go. It reads the D-pad only; for the left stick use jet_pad_stick.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_UP)) menu_index--;
 */
#define JET_PAD_BTN_UP      0   // D-pad

/**
 * Constant: JET_PAD_BTN_DOWN
 * Summary: The button number for Down on the D-pad.
 * Include: jet_gamepad.h
 * Notes: A menu usually wants jet_pad_pressed, so one push moves one row. jet_pad_down answers true on every tick the button is held, and there are 120 ticks a second, so a menu driven by it would race through its rows.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_DOWN)) menu_index++;
 */
#define JET_PAD_BTN_DOWN    1   // D-pad

/**
 * Constant: JET_PAD_BTN_LEFT
 * Summary: The button number for Left on the D-pad.
 * Include: jet_gamepad.h
 * Notes: This reads the D-pad on one chosen pad, and only the D-pad. The older btn call is different: it reads a combined player-one button set, which both the D-pad and a left stick pushed past JET_PAD_DPAD_THRESHOLD feed.
 * Example:
 *   if (jet_pad_down(0, JET_PAD_BTN_LEFT)) player_x -= speed * dt;
 */
#define JET_PAD_BTN_LEFT    2   // D-pad

/**
 * Constant: JET_PAD_BTN_RIGHT
 * Summary: The button number for Right on the D-pad.
 * Include: jet_gamepad.h
 * Notes: A D-pad direction is either on or off, so movement from it is exact and repeats the same way every time, which is why games on a grid prefer it. For free movement in any direction and at any speed, read the stick with jet_pad_stick.
 * Example:
 *   if (jet_pad_down(0, JET_PAD_BTN_RIGHT)) player_x += speed * dt;
 */
#define JET_PAD_BTN_RIGHT   3   // D-pad

/**
 * Constant: JET_PAD_BTN_A
 * Summary: The button number for the bottom face button, which is A on an Xbox pad and Cross on a PlayStation pad.
 * Include: jet_gamepad.h
 * Notes: The button is picked by its position, not the letter printed on it, so a pad with a different label still reports its bottom face button here. Players expect it to confirm, jump or fire. A menu should accept it from any pad, through jet_pad_any_pressed. For an action that should happen once per push, use jet_pad_pressed rather than jet_pad_down.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_A)) jump();
 */
#define JET_PAD_BTN_A       4   // South face button

/**
 * Constant: JET_PAD_BTN_B
 * Summary: The button number for the right face button, which is B on an Xbox pad and Circle on a PlayStation pad.
 * Include: jet_gamepad.h
 * Notes: Players expect it to cancel, go back, or fire a second weapon. Using JET_PAD_BTN_A to confirm and this button to cancel is what players expect on every kind of pad.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_B)) close_menu();
 */
#define JET_PAD_BTN_B       5   // East face button

/**
 * Constant: JET_PAD_BTN_X
 * Summary: The button number for the left face button, which is X on an Xbox pad and Square on a PlayStation pad.
 * Include: jet_gamepad.h
 * Notes: Free for your game's third action: reload, a special move, a swap. Players have no fixed expectation of it, unlike the confirm and cancel buttons, so you can give it any job without surprising anyone.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_X)) reload_weapon();
 */
#define JET_PAD_BTN_X       6   // West face button

/**
 * Constant: JET_PAD_BTN_Y
 * Summary: The button number for the top face button, which is Y on an Xbox pad and Triangle on a PlayStation pad.
 * Include: jet_gamepad.h
 * Notes: The fourth face button, which games usually use to switch a mode or open the inventory. jet_pad_button_name calls it "Pad Y", which is handy for a controls screen.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_Y)) toggle_inventory();
 */
#define JET_PAD_BTN_Y       7   // North face button

/**
 * Constant: JET_PAD_BTN_LB
 * Summary: The button number for the left bumper, the button on the pad's left shoulder. It is simply on or off, unlike the trigger behind it.
 * Include: jet_gamepad.h
 * Notes: The trigger behind it is not a button but an axis, a value that can be part way, read with jet_pad_axis(pad, JET_PAD_AXIS_LT). Because a bumper is simply on or off, it suits turning a page or cycling through weapons.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_LB)) select_previous_weapon();
 */
#define JET_PAD_BTN_LB      8   // Left bumper

/**
 * Constant: JET_PAD_BTN_RB
 * Summary: The button number for the right bumper, the button on the pad's right shoulder. It is simply on or off, unlike the trigger behind it.
 * Include: jet_gamepad.h
 * Notes: The trigger behind it is not a button but an axis, a value that can be part way, read with jet_pad_axis(pad, JET_PAD_AXIS_RT). Use jet_pad_down when holding it should do something, such as aim, and jet_pad_pressed when each push should step once, such as cycling through weapons.
 * Example:
 *   if (jet_pad_down(0, JET_PAD_BTN_RB)) aim_mode();
 */
#define JET_PAD_BTN_RB      9   // Right bumper

/**
 * Constant: JET_PAD_BTN_L3
 * Summary: The button number for the left stick's click, made by pressing the stick straight down.
 * Include: jet_gamepad.h
 * Notes: A player pushing the stick hard can click it by accident, so give it an action that is easy to undo, such as crouch or sprint, and never anything that cannot be taken back.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_L3)) toggle_sprint();
 */
#define JET_PAD_BTN_L3      10  // Left stick click

/**
 * Constant: JET_PAD_BTN_R3
 * Summary: The button number for the right stick's click, made by pressing the stick straight down.
 * Include: jet_gamepad.h
 * Notes: Games usually use it to recentre the camera or to switch a zoom on and off. A player can click it by accident while pushing the stick hard, the same warning as for JET_PAD_BTN_L3.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_R3)) recentre_camera();
 */
#define JET_PAD_BTN_R3      11  // Right stick click

/**
 * Constant: JET_PAD_BTN_START
 * Summary: The button number for the Start button, called Menu on some pads.
 * Include: jet_gamepad.h
 * Notes: Read it with jet_pad_any_pressed, so whichever player reaches for it can pause. A game with four pads should not care which one paused it.
 * Example:
 *   if (jet_pad_any_pressed(JET_PAD_BTN_START)) toggle_pause();
 */
#define JET_PAD_BTN_START   12  // Menu

/**
 * Constant: JET_PAD_BTN_SELECT
 * Summary: The button number for the Select button, called Back or View on some pads.
 * Include: jet_gamepad.h
 * Notes: Games usually give it a second screen, such as a map, a scoreboard, or the list of controls. Pausing belongs to JET_PAD_BTN_START.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_SELECT)) show_map();
 */
#define JET_PAD_BTN_SELECT  13  // Back / View

/**
 * Constant: JET_PAD_BTN_GUIDE
 * Summary: The button number for the Guide button in the middle of the pad, the one with the Xbox or PlayStation logo, or marked Mode.
 * Include: jet_gamepad.h
 * Notes: The host (the program that runs your cartridge) or the controller's driver often keeps this button for itself, so your cartridge may never see it pressed. Treat it as a bonus shortcut, never as the only way to reach a screen.
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_GUIDE)) show_debug_overlay();
 */
#define JET_PAD_BTN_GUIDE   14  // Xbox / Mode

// The axis numbers, for jet_pad_axis. A stick axis reads from -1.0 to 1.0.
// Its up-and-down axis follows the screen, where y counts downwards: up is
// -1.0 and down is 1.0, the same way round as the mouse. A trigger reads from
// 0.0 (released) to 1.0 (pulled all the way).

/**
 * Constant: JET_PAD_AXIS_LX
 * Summary: The axis number for the left stick's side-to-side movement. Give it to jet_pad_axis.
 * Include: jet_gamepad.h
 * Notes: jet_pad_axis gives it back as -1.0 (all the way left) to 1.0 (all the way right), with no deadzone, so a stick at rest can read a little off zero. For movement, prefer jet_pad_stick, which treats anything within JET_PAD_STICK_DEADZONE of the centre as still and stretches the rest back out to a full 0 to 1.
 * Example:
 *   float lean = jet_pad_axis(0, JET_PAD_AXIS_LX);
 */
#define JET_PAD_AXIS_LX     0

/**
 * Constant: JET_PAD_AXIS_LY
 * Summary: The axis number for the left stick's up-and-down movement. Give it to jet_pad_axis.
 * Include: jet_gamepad.h
 * Notes: Up is -1.0 and down is 1.0, the same way round as the screen and the mouse, so you can add the value straight onto a y position without flipping its sign.
 * Example:
 *   float climb = jet_pad_axis(0, JET_PAD_AXIS_LY);
 */
#define JET_PAD_AXIS_LY     1

/**
 * Constant: JET_PAD_AXIS_RX
 * Summary: The axis number for the right stick's side-to-side movement. Give it to jet_pad_axis.
 * Include: jet_gamepad.h
 * Notes: The right stick usually aims or turns the camera. jet_pad_axis gives it back raw, from -1.0 to 1.0 with no deadzone; jet_pad_stick with JET_PAD_STICK_RIGHT gives the same stick with the deadzone applied.
 * Example:
 *   float turn = jet_pad_axis(0, JET_PAD_AXIS_RX);
 */
#define JET_PAD_AXIS_RX     2

/**
 * Constant: JET_PAD_AXIS_RY
 * Summary: The axis number for the right stick's up-and-down movement. Give it to jet_pad_axis.
 * Include: jet_gamepad.h
 * Notes: Up is -1.0 and down is 1.0, as for JET_PAD_AXIS_LY. If your game offers an "invert look" option, flip the sign of this value; do not swap to another axis.
 * Example:
 *   float pitch = jet_pad_axis(0, JET_PAD_AXIS_RY);
 */
#define JET_PAD_AXIS_RY     3

/**
 * Constant: JET_PAD_AXIS_LT
 * Summary: The axis number for the left trigger. Give it to jet_pad_axis.
 * Include: jet_gamepad.h
 * Notes: A trigger reads from 0.0 (released) to 1.0 (pulled all the way). It only goes one way, unlike a stick. The bumper above it is a separate on-or-off button, JET_PAD_BTN_LB.
 * Example:
 *   float brake = jet_pad_axis(0, JET_PAD_AXIS_LT);
 */
#define JET_PAD_AXIS_LT     4

/**
 * Constant: JET_PAD_AXIS_RT
 * Summary: The axis number for the right trigger. Give it to jet_pad_axis.
 * Include: jet_gamepad.h
 * Notes: A trigger reads from 0.0 (released) to 1.0 (pulled all the way). If you want the trigger to act like a button, compare it with a level you choose, because an axis has no press or release of its own for jet_pad_pressed to report.
 * Example:
 *   float throttle = jet_pad_axis(0, JET_PAD_AXIS_RT);
 */
#define JET_PAD_AXIS_RT     5

// Which stick jet_pad_stick reads.

/**
 * Constant: JET_PAD_STICK_LEFT
 * Summary: The left stick, as a choice for jet_pad_stick.
 * Include: jet_gamepad.h
 * Notes: The left stick usually moves the character. jet_pad_stick gives it back as an (x, y) pair with the deadzone already applied, the small circle around the stick's centre that counts as still, so you never have to filter out a resting stick's creep yourself.
 * Example:
 *   float mx, my;
 *   jet_pad_stick(0, JET_PAD_STICK_LEFT, &mx, &my);
 */
#define JET_PAD_STICK_LEFT  0

/**
 * Constant: JET_PAD_STICK_RIGHT
 * Summary: The right stick, as a choice for jet_pad_stick.
 * Include: jet_gamepad.h
 * Notes: The right stick usually aims or steers the camera. Inside the deadzone both x and y come back as 0, so a stick that has been let go holds the camera perfectly still.
 * Example:
 *   float ax, ay;
 *   jet_pad_stick(0, JET_PAD_STICK_RIGHT, &ax, &ay);
 */
#define JET_PAD_STICK_RIGHT 1

/**
 * Constant: JET_PAD_STICK_DEADZONE
 * Summary: The size of the deadzone jet_pad_stick applies, the small circle around a stick's centre that counts as still.
 * Include: jet_gamepad.h
 * Notes: It is a circle, not a square. The distance from the centre is what counts, not each axis on its own, so a diagonal push is treated the same as a straight one. Inside the circle both x and y come back as 0. Outside it, how far the stick has travelled past the circle's edge is stretched back out to run from 0 to 1, so movement starts gently from a standstill instead of jumping.
 * Example:
 *   bool centred = fabsf(jet_pad_axis(0, JET_PAD_AXIS_LX)) < JET_PAD_STICK_DEADZONE;
 */
#define JET_PAD_STICK_DEADZONE 0.15f

// How far the left stick must be pushed before it counts as a D-pad press in
// the older player-one button set that btn and btnp read.

/**
 * Constant: JET_PAD_DPAD_THRESHOLD
 * Summary: How far the left stick must be pushed before the older btn call reports it as a D-pad direction.
 * Include: jet_gamepad.h
 * Notes: Jet64 folds every connected pad into one pretend player-one pad, which is what btn, btnp and btnr read. A left stick pushed past this point counts there as a D-pad press. The jet_pad_* calls are not affected: they always report the real control you asked about.
 * Example:
 *   bool would_move_p1 = jet_pad_axis(0, JET_PAD_AXIS_LX) > JET_PAD_DPAD_THRESHOLD;
 */
#define JET_PAD_DPAD_THRESHOLD 0.5f

/**
 * Function: jet_pad_connected
 * Summary: Tells you whether a gamepad is plugged in at a slot.
 * Parameters:
 *   - pad: The slot number, 0 to 3. Slot 0 is the first controller that connected.
 * Returns: true if a controller is in the slot; otherwise false.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_connected(0)) { show_pad_controls(); }
 */
bool jet_pad_connected(int pad);

/**
 * Function: jet_pad_count
 * Summary: Counts the gamepads connected right now.
 * Parameters:
 *   - None.
 * Returns: How many controllers are connected, 0 to 4.
 * Include: jet_gamepad.h
 * Example:
 *   int players = jet_pad_count();
 */
int jet_pad_count(void);

/**
 * Function: jet_pad_down
 * Summary: Tells you whether a gamepad button is held down right now.
 * Parameters:
 *   - pad: The slot number, 0 to 3.
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: true while the button is held. A tap too quick to last a whole tick (one step of the game; there are 120 a second) is stretched to fill one tick, so it is never missed.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_down(0, JET_PAD_BTN_RB)) { aim_mode(); }
 */
bool jet_pad_down(int pad, int button);

/**
 * Function: jet_pad_pressed
 * Summary: Tells you whether a gamepad button was pushed down this tick.
 * Parameters:
 *   - pad: The slot number, 0 to 3.
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: true exactly once for each real press, and false while the button stays held. The hardware counts presses, which is how a quick tap is never missed.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_pressed(0, JET_PAD_BTN_A)) { jump(); }
 */
bool jet_pad_pressed(int pad, int button);

/**
 * Function: jet_pad_released
 * Summary: Tells you whether a gamepad button was let go this tick.
 * Parameters:
 *   - pad: The slot number, 0 to 3.
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: true on the tick the button goes from held to up; otherwise false.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_released(0, JET_PAD_BTN_B)) { fire_charged_shot(); }
 */
bool jet_pad_released(int pad, int button);

/**
 * Function: jet_pad_any_down
 * Summary: Tells you whether a button is held down on any connected gamepad.
 * Parameters:
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: true if at least one connected pad has the button held; otherwise false.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_any_down(JET_PAD_BTN_START)) { open_pause_menu(); }
 */
bool jet_pad_any_down(int button);

/**
 * Function: jet_pad_any_pressed
 * Summary: Tells you whether a button was pushed down on any connected gamepad this tick.
 * Parameters:
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: true if at least one connected pad pressed the button this tick; otherwise false.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_any_pressed(JET_PAD_BTN_A)) { advance_dialogue(); }
 */
bool jet_pad_any_pressed(int button);

/**
 * Function: jet_pad_any_released
 * Summary: Tells you whether a button was let go on any connected gamepad this tick.
 * Parameters:
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: true if at least one connected pad released the button this tick; otherwise false.
 * Include: jet_gamepad.h
 * Example:
 *   if (jet_pad_any_released(JET_PAD_BTN_RB)) { release_charge(); }
 */
bool jet_pad_any_released(int button);

/**
 * Function: jet_pad_axis
 * Summary: Reads one stick axis or trigger as a raw number, with no deadzone applied, so a stick at rest may read a little off zero. For movement, jet_pad_stick is easier.
 * Parameters:
 *   - pad: The slot number, 0 to 3.
 *   - axis: Which axis, as one of the JET_PAD_AXIS_* constants.
 * Returns: For a stick axis, -1.0 to 1.0, with up as -1.0 and down as 1.0, the same way round as the screen. For a trigger, 0.0 (released) to 1.0 (pulled all the way). 0.0 if the pad or axis number is out of range.
 * Include: jet_gamepad.h
 * Example:
 *   float throttle = jet_pad_axis(0, JET_PAD_AXIS_RT);
 */
float jet_pad_axis(int pad, int axis);

/**
 * Function: jet_pad_stick
 * Summary: Reads a stick as an (x, y) pair, with the deadzone already applied: the small circle around the stick's centre that counts as still, so a stick at rest does not creep.
 * Parameters:
 *   - pad: The slot number, 0 to 3.
 *   - stick: JET_PAD_STICK_LEFT or JET_PAD_STICK_RIGHT.
 *   - x: Where to put the side-to-side value, -1.0 (left) to 1.0 (right). Give it a real pointer, not NULL.
 *   - y: Where to put the up-and-down value, -1.0 (up) to 1.0 (down), the same way round as the screen. Give it a real pointer, not NULL.
 * Returns: Nothing. Inside the deadzone, a circle of JET_PAD_STICK_DEADZONE around the centre, both x and y are set to 0. Past it, the distance the stick has travelled is stretched back out to run from 0 to 1, so movement starts gently. If either pointer is NULL, or the pad number is out of range, whichever pointer is not NULL is set to 0 and nothing else happens.
 * Include: jet_gamepad.h
 * Example:
 *   float mx, my;
 *   jet_pad_stick(0, JET_PAD_STICK_LEFT, &mx, &my);
 *   player_x += mx * speed * dt;
 */
void jet_pad_stick(int pad, int stick, float* x, float* y);

/**
 * Function: jet_pad_button_name
 * Summary: Gives you a short name for a button, such as "Pad A", to print on a controls screen.
 * Parameters:
 *   - button: Which button, as one of the JET_PAD_BTN_* constants.
 * Returns: A string such as "Pad A" or "Pad LB", or "Pad ?" if the number is not a button. The string is built into the runtime: keep the pointer as long as you like, and do not change or free it.
 * Include: jet_gamepad.h
 * Example:
 *   jet_print(jet_pad_button_name(JET_PAD_BTN_Y), 10, 10, 15);
 */
const char* jet_pad_button_name(int button);

/**
 * Function: jet_pad_axis_name
 * Summary: Gives you a short name for one way a stick or trigger can be pushed, such as "Stick Left", to print on a controls screen.
 * Parameters:
 *   - axis: Which stick or trigger, as one of the JET_PAD_AXIS_* constants.
 *   - direction: Which way it is pushed: a negative number for left or up, a positive number for right or down. A trigger only ever goes one way, so it is named the same either way.
 * Returns: A string such as "Stick Left" or "Left Trigger", or "Axis ?" if the number is not an axis. The string is built into the runtime: keep the pointer as long as you like, and do not change or free it.
 * Include: jet_gamepad.h
 * Notes: It names a DIRECTION, not the axis, because that is what a player binds a control to: "left on the left stick" is one control and "right on the left stick" is another.
 * Example:
 *   jet_print(jet_pad_axis_name(JET_PAD_AXIS_LX, -1), 10, 10, 15); // "Stick Left"
 */
const char* jet_pad_axis_name(int axis, int direction);

/**
 * Constant: JET_PAD_ID_NONE
 * Summary: The identity number a pad slot with no controller in it reports, and the one number jet_pad_id never gives a real controller.
 * Include: jet_gamepad.h
 * Notes: Test a pad's id against this to ask "is there a controller here that I can remember?" rather than testing the slot number, which is only ever the order things were plugged in.
 * Example:
 *   if (jet_pad_id(0) != JET_PAD_ID_NONE) { load_profile_for(jet_pad_id(0)); }
 */
#define JET_PAD_ID_NONE 0u

/**
 * Function: jet_pad_id
 * Summary: Gives you a number that stands for the CONTROLLER in a pad slot, rather than for the slot, so you can remember settings for it and find them again next time.
 * Parameters:
 *   - pad: Which pad slot, from 0 to 3.
 * Returns: A number that is the same every time that model of controller is plugged in, or JET_PAD_ID_NONE (0) when the slot is empty or the slot number is out of range.
 * Include: jet_gamepad.h
 * Notes: The number comes from what the controller calls itself, so two controllers of the same model share one number. That is usually what you want: a fix a player made for one pad's upside-down stick is exactly the fix the identical pad beside it needs. Which PLAYER a pad drives is still the slot it is in, so two identical pads can still steer two different ships. The number is not a serial number and it is not the same on a different computer, so do not use it for anything but looking settings up on this machine. jet_input_binds.h already does exactly this for you: it keeps one set of gamepad bindings per controller, found by this number.
 * Example:
 *   uint32_t pad_one = jet_pad_id(0);
 *   if (pad_one != JET_PAD_ID_NONE) { printf("pad 1 is device %08x\n", pad_one); }
 */
uint32_t jet_pad_id(int pad);

/**
 * Function: jet_pad_name
 * Summary: Gives you the controller's own short name, such as "Xbox 360 Contro", to show the player which pad they are looking at.
 * Parameters:
 *   - pad: Which pad slot, from 0 to 3.
 * Returns: The controller's name, cut to fifteen characters, or an empty string when the slot is empty or the slot number is out of range. The string belongs to the runtime: read it, but do not change or free it, and copy it if you want to keep it past the next _update.
 * Include: jet_gamepad.h
 * Notes: It is short because the machine keeps only sixteen bytes for it, which is enough to tell one controller from another on a screen. Use jet_pad_id when you need to tell them apart in a save file.
 * Example:
 *   jet_print(jet_pad_name(0), 10, 10, 15);
 */
const char* jet_pad_name(int pad);

// ---- Internal runtime hooks (not cart API) ----

/**
 * Function: _jet_pad_hw_update
 * Summary: Internal runtime hook that snapshots and decodes the host-written gamepad hardware block.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_gamepad.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Called by _jet_input_hw_update() before every _update(). Cartridge code must never call this directly because a second snapshot can corrupt pressed/released edge detection.
 * Example:
 *   // No cartridge call is required; use jet_pad_pressed() inside _update().
 */
void _jet_pad_hw_update(void);

/**
 * Function: _jet_pad_hw_draw_begin
 * Summary: Internal runtime hook that republishes the pad edge sets as the whole frame's span, for the single _draw() that follows a frame's ticks.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_gamepad.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The pad half of _jet_input_hw_draw_begin(), which is its only caller. Cartridge code must never call this directly because publishing consumes the span, and a second publish would report an empty one.
 * Example:
 *   // No cartridge call is required; use jet_pad_pressed() inside _draw().
 */
void _jet_pad_hw_draw_begin(void);

#endif // JET_GAMEPAD_H
