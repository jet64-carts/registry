// runtime/jet_input_binds.h
#ifndef JET_INPUT_BINDS_H
#define JET_INPUT_BINDS_H

#include "jet_core.h" // IWYU pragma: keep

// Give your game's controls names, such as "JUMP" and "FIRE", and let the
// player decide which key, mouse button, pad button or stick direction does
// each one. In _init you register each action once with jet_bind_register,
// giving it the input it starts with and a spare. In _update you ask about the
// action by name, with jet_bind_pressed or jet_bind_down, and it does not
// matter which device the player used. Their choices are saved in the
// cartridge's own folder (its AppData sandbox), so they are still there next
// time. When you ask about an action every tick, look its id up once with
// jet_bind_get_id and use the jet_bind_id_* calls, which skip the search by
// name.
//
// LETTING THE PLAYER CHANGE THEM
//
// Jet64 ships a rebinding screen, jet_bind_open_ui, and it is a view drawn on
// the developer terminal — so it is declared in terminal/jet_terminal.h beside
// the two calls that make it run, not here. A game that offers it includes that
// header and drives the panel from its own hooks:
//
//   #include "terminal/jet_terminal.h"
//   void _update(float dt) { if (jet_terminal_update(dt)) return; ... }
//   void _draw(void)       { ... jet_terminal_draw(); }
//
// (carts/jetquest is the worked example.) Bindings themselves cost a cartridge
// nothing extra: they are firmware, and btn()/btnp() read them. The screen is
// the part that arrives with the terminal.
//
// TWO PLAYERS, AND CONTROLLERS THAT DISAGREE
//
// A binding lives in one of two places, and knowing which is the whole of it.
//
//   * Keys and mouse buttons belong to a PLAYER. Player one and player two sit
//     at the same keyboard, so they need different keys, and jet_bind_register
//     gives player one's while jet_bind_register_player gives anyone else's.
//   * Pad buttons and stick directions belong to a CONTROLLER. Every pad has
//     its own set, found by jet_pad_id, so the player can correct one awkward
//     controller — the old pad whose stick reports right when it is pushed left
//     — without disturbing the good one plugged in beside it. The layout a pad
//     starts with is the one jet_bind_register_pad gives, and each controller
//     keeps its own changes to it.
//
// Which player a controller drives is the SLOT it is plugged into, never the
// kind of pad it is: jet_bind_set_pad_player says so, and the shipped answer is
// that pad slot 0 drives player one, slot 1 drives player two, and so on. That
// is what lets two identical controllers share one set of corrections and still
// steer two different ships.
//
// jet_bind_down asks "did ANY player, on ANY device, do this?", which is what a
// title screen wants. jet_bind_player_down asks about one player alone, which
// is what a two-player game wants once the game has started.
//
// Complete rebindable-action example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_input_binds.h"
//
//   static int jump_action;
//   static bool airborne;
//
//   void _init(void) {
//       jet_bind_register("JUMP", jet_bind_key(JET_KEY_SPACE), jet_bind_none());
//       jet_bind_register_pad("JUMP", jet_bind_pad(JET_PAD_BTN_A), jet_bind_none());
//       jump_action = jet_bind_get_id("JUMP");
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (jet_bind_id_pressed(jump_action)) airborne = true;
//       if (jet_bind_id_released(jump_action)) airborne = false;
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(airborne ? "JUMP!" : "PRESS SPACE OR PAD A", 16, 16, 15);
//   }
//
//   void _end(void) {}

/**
 * Constant: JET_BIND_MAX_ACTIONS
 * Summary: How many named actions one cartridge can register.
 * Include: jet_input_binds.h
 * Notes: A registration past this many is ignored rather than overwriting anything.
 * Example:
 *   // 64 named controls is far more than most games need.
 */
#define JET_BIND_MAX_ACTIONS 64

/**
 * Constant: JET_BIND_PLAYERS
 * Summary: How many players can have their own keys, mouse buttons and pad slot. Player numbers run from 0, so player one is 0.
 * Include: jet_input_binds.h
 * Notes: It matches the four pad slots jet_gamepad.h offers, so every controller the machine can see has a player to drive.
 * Example:
 *   for (int player = 0; player < JET_BIND_PLAYERS; player++) { draw_seat(player); }
 */
#define JET_BIND_PLAYERS 4

/**
 * Constant: JET_BIND_SLOTS
 * Summary: How many inputs one action holds at once, on each device: a primary and a spare.
 * Include: jet_input_binds.h
 * Notes: The two are equals — the action answers true when either one is used. The spare is what lets Up be both the arrow key and W, or both the d-pad and the stick.
 * Example:
 *   JetBindInput spare = jet_bind_get(0, jump_action, JET_BIND_SLOTS - 1);
 */
#define JET_BIND_SLOTS 2

/**
 * Constant: JET_BIND_MAX_PAD_PROFILES
 * Summary: How many different controllers the game remembers bindings for at once.
 * Include: jet_input_binds.h
 * Notes: A controller keeps its bindings while it is unplugged, so this is the number of DIFFERENT pads a player has ever used, not the number plugged in. Past this many, a further controller still plays perfectly well on the shipped layout; it just cannot be given one of its own.
 * Example:
 *   // Eight remembered controllers is generous for a four-slot machine.
 */
#define JET_BIND_MAX_PAD_PROFILES 8

/**
 * Enum: JetBindType
 * Summary: Says which kind of device a JetBindInput refers to: a key, a mouse button, a pad button, a stick pushed one way, or nothing.
 * Include: jet_input_binds.h
 * Notes: You rarely write one of these yourself. Build a JetBindInput with jet_bind_key, jet_bind_mouse, jet_bind_pad, jet_bind_pad_axis or jet_bind_none, which fill in every field for you.
 * Example:
 *   JetBindInput primary = jet_bind_key(JET_KEY_SPACE);
 *   JetBindInput secondary = jet_bind_pad(JET_PAD_BTN_A);
 */
typedef enum {
    JET_BIND_TYPE_NONE = 0, // Nothing at all. Use it for an action's spare slot when there is no second input.
    JET_BIND_TYPE_KEY,      // A keyboard key. The code is a JET_KEY_* constant or an ASCII code.
    JET_BIND_TYPE_MOUSE,    // A mouse button or wheel direction. The code is a MOUSE_BTN_* or MOUSE_WHEEL_* constant.
    JET_BIND_TYPE_PAD,      // A gamepad button. The code is a JET_PAD_BTN_* constant.
    JET_BIND_TYPE_PAD_AXIS  // A gamepad stick or trigger pushed one way. The code is a JET_PAD_AXIS_* constant and direction says which way.
} JetBindType;

/**
 * Struct: JetBindInput
 * Summary: One input, as a device, a code, and — for a stick — which way it is pushed. It is what one of an action's slots is bound to.
 * Fields:
 *   - type: Which kind of device the code belongs to. It decides how code and direction are read.
 *   - code: The key, button, wheel direction or axis: a JET_KEY_ constant or ASCII code, a MOUSE_BTN_ or MOUSE_WHEEL_ constant, a JET_PAD_BTN_ constant, or a JET_PAD_AXIS_ constant, depending on type.
 *   - direction: For JET_BIND_TYPE_PAD_AXIS only, which way the stick must be pushed: negative for left or up, positive for right or down. Zero for every other type.
 * Include: jet_input_binds.h
 * Notes: It is a small description, passed by value, not a live reading of the input. Make one with a jet_bind_* helper and hand it to jet_bind_register, which keeps its own copy, so you need not keep yours.
 * Example:
 *   JetBindInput fire = jet_bind_mouse(MOUSE_BTN_LEFT);
 *   jet_bind_register("FIRE", fire, jet_bind_none());
 */
typedef struct {
    JetBindType type;
    int code;
    int direction;
} JetBindInput;

// The helpers that make a JetBindInput for jet_bind_register. Each one only
// fills in the struct: nothing is allocated and nothing is remembered.

/**
 * Function: jet_bind_key
 * Summary: Makes a JetBindInput for a keyboard key.
 * Parameters:
 *   - code: Which key, as a JET_KEY_* constant or an ASCII code.
 * Returns: A JetBindInput that names that key.
 * Include: jet_input_binds.h
 * Example:
 *   jet_bind_register("JUMP", jet_bind_key(JET_KEY_SPACE), jet_bind_none());
 */
static inline JetBindInput jet_bind_key(int code) {
    JetBindInput i;
    i.type = JET_BIND_TYPE_KEY;
    i.code = code;
    i.direction = 0;
    return i;
}

/**
 * Function: jet_bind_mouse
 * Summary: Makes a JetBindInput for a mouse button or a wheel direction.
 * Parameters:
 *   - code: Which button, as a MOUSE_BTN_* constant, or MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN for a turn of the wheel.
 * Returns: A JetBindInput that names that mouse input.
 * Include: jet_input_binds.h
 * Example:
 *   jet_bind_register("FIRE", jet_bind_mouse(MOUSE_BTN_LEFT), jet_bind_none());
 */
static inline JetBindInput jet_bind_mouse(int code) {
    JetBindInput i;
    i.type = JET_BIND_TYPE_MOUSE;
    i.code = code;
    i.direction = 0;
    return i;
}

/**
 * Function: jet_bind_pad
 * Summary: Makes a JetBindInput for a gamepad button.
 * Parameters:
 *   - code: Which button, as a JET_PAD_BTN_* constant.
 * Returns: A JetBindInput that names that gamepad button.
 * Include: jet_input_binds.h
 * Notes: A pad binding belongs to a controller rather than to a player, so this is the button on WHICHEVER pad is asked about — and each controller keeps its own answer once the player has changed it.
 * Example:
 *   jet_bind_register_pad("JUMP", jet_bind_pad(JET_PAD_BTN_A), jet_bind_none());
 */
static inline JetBindInput jet_bind_pad(int code) {
    JetBindInput i;
    i.type = JET_BIND_TYPE_PAD;
    i.code = code;
    i.direction = 0;
    return i;
}

/**
 * Function: jet_bind_pad_axis
 * Summary: Makes a JetBindInput for a stick or trigger pushed one way, so a stick can do the work of a d-pad button.
 * Parameters:
 *   - axis: Which stick or trigger, as a JET_PAD_AXIS_* constant.
 *   - direction: Which way it must be pushed: a negative number for left or up, a positive number for right or down. A trigger only travels one way, so give it a positive number.
 * Returns: A JetBindInput that names that stick direction.
 * Include: jet_input_binds.h
 * Notes: It counts as held once the stick is pushed past JET_PAD_DPAD_THRESHOLD, the same point at which jet64's own d-pad reading turns on, so a stick and a d-pad bound to the same action agree about when they are pressed. This is the binding a player reaches for when a controller reports a stick backwards: bind Left to the direction that actually moves left on THAT pad, and every other controller is untouched.
 * Example:
 *   jet_bind_register_pad("LEFT", jet_bind_pad(JET_PAD_BTN_LEFT), jet_bind_pad_axis(JET_PAD_AXIS_LX, -1));
 */
static inline JetBindInput jet_bind_pad_axis(int axis, int direction) {
    JetBindInput i;
    i.type = JET_BIND_TYPE_PAD_AXIS;
    i.code = axis;
    i.direction = (direction < 0) ? -1 : 1;
    return i;
}

/**
 * Function: jet_bind_none
 * Summary: Makes an empty JetBindInput, for an action that has no second input.
 * Parameters:
 *   - None.
 * Returns: A JetBindInput that never matches anything the player does.
 * Include: jet_input_binds.h
 * Example:
 *   jet_bind_register("START", jet_bind_key(JET_KEY_ENTER), jet_bind_none());
 */
static inline JetBindInput jet_bind_none(void) {
    JetBindInput i;
    i.type = JET_BIND_TYPE_NONE;
    i.code = 0;
    i.direction = 0;
    return i;
}

/**
 * Function: jet_bind_init
 * Summary: Loads the player's saved bindings from disk and registers the eight built-in actions: UP, DOWN, LEFT, RIGHT, A, B, START and SELECT.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: It runs once; calling it again does nothing. You normally never call it yourself: the first jet_bind_register, jet_bind_register_player or jet_bind_register_pad calls it for you, as do the first tick and the rebinding screen (jet_bind_open_ui, in terminal/jet_terminal.h). So the eight built-in actions are always there to be named, even by the very first binding call a cartridge makes. Any binding the player has saved, in the file jet_binds.dat in the cartridge's own file area (its VFS), wins over the default an action was registered with. Those eight actions are the ones btn() and btnp() read, so rebinding them rebinds the simple controls too — on the keyboard and on each controller alike.
 * Example:
 *   jet_bind_init();
 */
void jet_bind_init(void);

/**
 * Function: jet_bind_register
 * Summary: Registers a named action with the two inputs player one starts with: a primary, the one it usually uses, and a secondary, a spare.
 * Parameters:
 *   - action_name: The action's name, such as "JUMP". It is copied, and cut to 31 characters if it is longer.
 *   - primary: The primary input to start with. Build it with jet_bind_key, jet_bind_mouse, jet_bind_pad or jet_bind_pad_axis.
 *   - secondary: The secondary input to start with, or jet_bind_none() if there is none.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: Call it once for each action, from _init. Each input is filed where it belongs: a key or mouse button becomes one of player one's, and a pad button or stick direction becomes part of the layout every controller starts with — so an action can be given a key and a pad button in one call, exactly as before. If the name is already registered, the call is ignored, so registering twice never overwrites a binding. The eight built-in actions (jet_bind_init) are always registered, so to change one of their defaults use jet_bind_register_player for player 0 and jet_bind_register_pad instead. If the player has saved bindings under this name, theirs are used instead of the defaults you give here. Up to JET_BIND_MAX_ACTIONS actions can be registered; any more are ignored.
 * Example:
 *   jet_bind_register("JUMP", jet_bind_key(JET_KEY_SPACE), jet_bind_pad(JET_PAD_BTN_A));
 */
void jet_bind_register(const char* action_name, JetBindInput primary, JetBindInput secondary);

/**
 * Function: jet_bind_register_player
 * Summary: Gives one player their own two keys or mouse buttons for an action already registered, so two people at one keyboard do not fight over the arrows.
 * Parameters:
 *   - player: Which player, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 *   - action_name: The name you gave jet_bind_register, or a built-in action's, such as "LEFT".
 *   - primary: The primary input this player starts with.
 *   - secondary: The spare, or jet_bind_none() if there is none.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: The action must exist: one you have given to jet_bind_register, or one of the eight built-ins (jet_bind_init), which are there even when this is the first binding call your cartridge makes. An unknown name, or a player out of range, is ignored. Only keys and mouse buttons are a player's own — hand it a pad button and it is ignored, because a controller's buttons belong to the controller and the player is decided by jet_bind_set_pad_player instead. Player 0 is the one jet_bind_register itself fills in, so calling this for player 0 replaces those. If the player has already saved bindings for this action, theirs are kept and this call changes nothing — so when a later build gives an action a meaning a saved binding should not survive, give it a NEW NAME rather than new defaults.
 * Example:
 *   jet_bind_register_player(1, "LEFT", jet_bind_key(JET_KEY_J), jet_bind_none());
 *   jet_bind_register_player(1, "RIGHT", jet_bind_key(JET_KEY_L), jet_bind_none());
 */
void jet_bind_register_player(int player, const char* action_name, JetBindInput primary, JetBindInput secondary);

/**
 * Function: jet_bind_register_pad
 * Summary: Gives an action already registered the two gamepad inputs every controller starts with — usually a button and the matching stick direction.
 * Parameters:
 *   - action_name: The name you gave jet_bind_register, or a built-in action's, such as "LEFT".
 *   - primary: The pad button or stick direction to start with.
 *   - secondary: The spare, or jet_bind_none() if there is none.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: The action must exist: one you have given to jet_bind_register, or one of the eight built-ins (jet_bind_init), which are there even when this is the first binding call your cartridge makes. An unknown name is ignored, and so is anything that is not a pad button or a stick direction. This is the layout a controller the player has never adjusted uses, and the one a fresh controller is given when it is first plugged in. A controller the player HAS adjusted keeps its own bindings and ignores a later change here, so you may improve your shipped layout between builds without undoing anybody's work.
 * Example:
 *   jet_bind_register_pad("LEFT", jet_bind_pad(JET_PAD_BTN_LEFT), jet_bind_pad_axis(JET_PAD_AXIS_LX, -1));
 */
void jet_bind_register_pad(const char* action_name, JetBindInput primary, JetBindInput secondary);

/**
 * Function: jet_bind_get_id
 * Summary: Finds a registered action by name and gives you its id number, for the faster jet_bind_id_* calls.
 * Parameters:
 *   - action_name: The name you gave jet_bind_register.
 * Returns: The action's id, or -1 if no action with that name is registered.
 * Include: jet_input_binds.h
 * Notes: The search compares the name against every registered action in turn, so look the id up once, in _init, and keep it, rather than asking by name every tick.
 * Example:
 *   int act_jump = jet_bind_get_id("JUMP");
 */
int jet_bind_get_id(const char* action_name);

/**
 * Function: jet_bind_pressed
 * Summary: Tells you whether an action was pressed this tick by any player, on any device.
 * Parameters:
 *   - action_name: The name you gave jet_bind_register.
 * Returns: true on the tick one of its inputs goes from up to down; otherwise false, and always false for a name that is not registered.
 * Include: jet_input_binds.h
 * Notes: It searches for the name on every call. If you ask every tick, get the id once with jet_bind_get_id and use jet_bind_id_pressed instead. For a game that has already asked who is playing, jet_bind_player_pressed answers for one player alone.
 * Example:
 *   if (jet_bind_pressed("JUMP")) { player_velocity_y = -150.0f; }
 */
bool jet_bind_pressed(const char* action_name);

/**
 * Function: jet_bind_down
 * Summary: Tells you whether an action is held right now by any player, on any device.
 * Parameters:
 *   - action_name: The name you gave jet_bind_register.
 * Returns: true while one of its inputs is held; otherwise false, and always false for a name that is not registered.
 * Include: jet_input_binds.h
 * Notes: It searches for the name on every call. If you ask every tick, get the id once with jet_bind_get_id and use jet_bind_id_down instead.
 * Example:
 *   if (jet_bind_down("THRUST")) { emit_particles(); }
 */
bool jet_bind_down(const char* action_name);

/**
 * Function: jet_bind_released
 * Summary: Tells you whether an action was released this tick by any player, on any device.
 * Parameters:
 *   - action_name: The name you gave jet_bind_register.
 * Returns: true on the tick one of its inputs goes from down to up; otherwise false, and always false for a name that is not registered.
 * Include: jet_input_binds.h
 * Notes: It searches for the name on every call. If you ask every tick, get the id once with jet_bind_get_id and use jet_bind_id_released instead.
 * Example:
 *   if (jet_bind_released("CHARGE")) { fire_weapon(); }
 */
bool jet_bind_released(const char* action_name);

// The same three questions, asked by id. They go straight to the action's
// place in the table and skip the search by name, so get the id once with
// jet_bind_get_id and use these every tick.

/**
 * Function: jet_bind_id_pressed
 * Summary: Tells you whether the action with this id was pressed this tick by any player, on any device. It is the faster form of jet_bind_pressed.
 * Parameters:
 *   - id: The action's id, from jet_bind_get_id.
 * Returns: true on the tick one of its inputs goes from up to down; otherwise false, and always false for an id that is out of range, such as the -1 an unknown name gives.
 * Include: jet_input_binds.h
 * Example:
 *   if (jet_bind_id_pressed(act_jump)) { player_velocity_y = -150.0f; }
 */
bool jet_bind_id_pressed(int id);

/**
 * Function: jet_bind_id_down
 * Summary: Tells you whether the action with this id is held right now by any player, on any device. It is the faster form of jet_bind_down.
 * Parameters:
 *   - id: The action's id, from jet_bind_get_id.
 * Returns: true while one of its inputs is held; otherwise false, and always false for an id that is out of range, such as the -1 an unknown name gives.
 * Include: jet_input_binds.h
 * Example:
 *   if (jet_bind_id_down(act_thrust)) { emit_particles(); }
 */
bool jet_bind_id_down(int id);

/**
 * Function: jet_bind_id_released
 * Summary: Tells you whether the action with this id was released this tick by any player, on any device. It is the faster form of jet_bind_released.
 * Parameters:
 *   - id: The action's id, from jet_bind_get_id.
 * Returns: true on the tick one of its inputs goes from down to up; otherwise false, and always false for an id that is out of range, such as the -1 an unknown name gives.
 * Include: jet_input_binds.h
 * Example:
 *   if (jet_bind_id_released(act_charge)) { fire_charge_up_weapon(); }
 */
bool jet_bind_id_released(int id);

// The same three questions again, asked for ONE player: their own keys and
// mouse buttons, plus every controller whose slot is driving them. This is
// what a two-player game asks once both ships are on the board.

/**
 * Function: jet_bind_player_pressed
 * Summary: Tells you whether one player pressed the action with this id this tick, on their own keys or on a controller driving them.
 * Parameters:
 *   - player: Which player, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 *   - id: The action's id, from jet_bind_get_id.
 * Returns: true on the tick one of that player's inputs goes from up to down; otherwise false, and always false for a player or an id that is out of range.
 * Include: jet_input_binds.h
 * Notes: Ask by name once with jet_bind_get_id; there is no by-name form of the per-player questions, because a game that has two players on the board is asking these every tick.
 * Example:
 *   if (jet_bind_player_pressed(1, act_fire)) { fire_from(&ship_two); }
 */
bool jet_bind_player_pressed(int player, int id);

/**
 * Function: jet_bind_player_down
 * Summary: Tells you whether one player is holding the action with this id, on their own keys or on a controller driving them.
 * Parameters:
 *   - player: Which player, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 *   - id: The action's id, from jet_bind_get_id.
 * Returns: true while one of that player's inputs is held; otherwise false, and always false for a player or an id that is out of range.
 * Include: jet_input_binds.h
 * Example:
 *   if (jet_bind_player_down(1, act_left)) { ship_two.x -= speed; }
 */
bool jet_bind_player_down(int player, int id);

/**
 * Function: jet_bind_player_released
 * Summary: Tells you whether one player released the action with this id this tick, on their own keys or on a controller driving them.
 * Parameters:
 *   - player: Which player, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 *   - id: The action's id, from jet_bind_get_id.
 * Returns: true on the tick one of that player's inputs goes from down to up; otherwise false, and always false for a player or an id that is out of range.
 * Include: jet_input_binds.h
 * Example:
 *   if (jet_bind_player_released(1, act_charge)) { fire_charge_up_weapon(&ship_two); }
 */
bool jet_bind_player_released(int player, int id);

/**
 * Function: jet_bind_pad_player
 * Summary: Tells you which player the controller in a pad slot is driving.
 * Parameters:
 *   - pad: Which pad slot, from 0 to 3.
 * Returns: The player number, from 0 to JET_BIND_PLAYERS - 1, or 0 for a slot number that is out of range.
 * Include: jet_input_binds.h
 * Notes: Unless something has changed it, slot 0 drives player one, slot 1 drives player two, and so on — so plugging two controllers in gives you two players with no setting up at all. The player is a property of the SLOT, not of the controller, which is what lets two identical pads share one set of corrected bindings and still steer two different ships.
 * Example:
 *   printf("pad 2 is player %d\n", jet_bind_pad_player(1) + 1);
 */
int jet_bind_pad_player(int pad);

/**
 * Function: jet_bind_set_pad_player
 * Summary: Says which player the controller in a pad slot drives.
 * Parameters:
 *   - pad: Which pad slot, from 0 to 3.
 *   - player: Which player it drives, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: A slot number or a player number out of range is ignored rather than stored. The choice is saved with the bindings, and the rebinding screen offers it too, so most games never call this — set it only if your own front end asks the question first.
 * Example:
 *   jet_bind_set_pad_player(1, 1); // the second controller drives player two
 */
void jet_bind_set_pad_player(int pad, int player);

/**
 * Function: jet_bind_get
 * Summary: Gives you one of a player's key or mouse bindings, to print on your own controls screen.
 * Parameters:
 *   - player: Which player, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 *   - id: The action's id, from jet_bind_get_id.
 *   - slot: Which of the action's inputs: 0 for the primary, 1 for the spare.
 * Returns: The binding, or jet_bind_none() for a player, an id or a slot that is out of range.
 * Include: jet_input_binds.h
 * Notes: Hand what you get to jet_bind_input_name to spell it, so your legend and the rebinding screen can never disagree about what a key is called.
 * Example:
 *   jet_print(jet_bind_input_name(jet_bind_get(0, act_jump, 0)), 10, 10, 15);
 */
JetBindInput jet_bind_get(int player, int id, int slot);

/**
 * Function: jet_bind_pad_get
 * Summary: Gives you one of a controller's own gamepad bindings, to print on your own controls screen.
 * Parameters:
 *   - pad: Which pad slot, from 0 to 3.
 *   - id: The action's id, from jet_bind_get_id.
 *   - slot: Which of the action's inputs: 0 for the primary, 1 for the spare.
 * Returns: The binding that controller uses, or jet_bind_none() for a slot, an id or a binding slot that is out of range.
 * Include: jet_input_binds.h
 * Notes: An empty pad slot answers with the layout a fresh controller would start on, so a legend still has something to print before anybody plugs one in.
 * Example:
 *   jet_print(jet_bind_input_name(jet_bind_pad_get(0, act_jump, 0)), 10, 20, 15);
 */
JetBindInput jet_bind_pad_get(int pad, int id, int slot);

/**
 * Function: jet_bind_input_name
 * Summary: Spells a binding for a controls screen, such as "Space", "Pad A" or "Stick Left".
 * Parameters:
 *   - input: The binding, from jet_bind_get or jet_bind_pad_get, or one you built yourself.
 * Returns: The input's name, or "---" for an empty binding. The string is built into the runtime: keep the pointer as long as you like, and do not change or free it.
 * Include: jet_input_binds.h
 * Notes: It is the same spelling the rebinding screen uses, and it goes on to jet_key_name, jet_mouse_button_name, jet_pad_button_name and jet_pad_axis_name, so every place a control is named in Jet64 agrees.
 * Example:
 *   jet_print(jet_bind_input_name(jet_bind_get(0, act_fire, 0)), 10, 10, 15);
 */
const char* jet_bind_input_name(JetBindInput input);

/**
 * Function: jet_bind_save
 * Summary: Writes the current bindings to the file jet_binds.dat, in the cartridge's own file area (its VFS).
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: It saves every player's keys, every remembered controller's own bindings, and which player each pad slot drives. The rebinding screen saves after every change, so most games never call this. Call it if your own code changes a binding.
 * Example:
 *   jet_bind_save();
 */
void jet_bind_save(void);

/**
 * Function: jet_bind_load
 * Summary: Reads the player's saved bindings from jet_binds.dat, ready to replace defaults as actions are registered.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Notes: jet_bind_init calls it for you, before it registers the defaults. What it loads takes effect as each action is registered, not straight away, and a controller's own bindings take effect when that controller is next plugged in. A file that is not this cartridge's, or that came from a build with a different shape, is refused whole rather than read in part. You will rarely need it yourself.
 * Example:
 *   jet_bind_load();
 */
void jet_bind_load(void);

// ---- Internal runtime hooks (not cart API) ----

// The rebinding screen's seam. The screen is drawn on the developer terminal
// (runtime/terminal/terminal_binds.c), so it lives on the terminal's side of
// the boundary and the arrow between the two modules points one way: the
// terminal reads the bindings, and the bindings know nothing of the terminal.
// These are the only things it needs of the table that no cartridge needs.

/**
 * Function: _jet_bind_action_count
 * Summary: Internal runtime hook that answers how many actions the cartridge has registered.
 * Parameters:
 *   - None.
 * Returns: The number of registered actions, from 0 to JET_BIND_MAX_ACTIONS.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The rebinding screen walks the actions by index, which nothing in the cart API does — a cartridge names its actions and looks them up with jet_bind_get_id.
 * Example:
 *   // No cartridge call is required; use jet_bind_get_id() instead.
 */
int _jet_bind_action_count(void);

/**
 * Function: _jet_bind_action_name
 * Summary: Internal runtime hook that gives a registered action's name from its id.
 * Parameters:
 *   - id: The action's id, from 0 to _jet_bind_action_count() - 1.
 * Returns: The action's name, or an empty string if the id names no action.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The inverse of jet_bind_get_id, and the reason the rebinding screen can list actions it was never told about. The string belongs to the runtime and stays valid for the life of the cartridge.
 * Example:
 *   // No cartridge call is required; a cartridge already knows its own names.
 */
const char* _jet_bind_action_name(int id);

/**
 * Function: _jet_bind_profile_count
 * Summary: Internal runtime hook that answers how many binding profiles exist: the shipped layout, then one for every controller the player has used.
 * Parameters:
 *   - None.
 * Returns: The number of profiles, always at least 1.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Profile 0 is the layout a controller starts on rather than one of the player's, which is why the rebinding screen offers profile 0's index as "keyboard and mouse" and every index above it as a controller.
 * Example:
 *   // No cartridge call is required; use jet_bind_pad_get() to read a pad's binding.
 */
int _jet_bind_profile_count(void);

/**
 * Function: _jet_bind_profile_name
 * Summary: Internal runtime hook that gives a binding profile's controller name.
 * Parameters:
 *   - profile: The profile's index, from 0 to _jet_bind_profile_count() - 1.
 * Returns: The controller's name, or an empty string if the index names no profile.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The name is the one the host published for the controller when its profile was made, so it is still there when the controller is unplugged — which is exactly when the rebinding screen has to name it.
 * Example:
 *   // No cartridge call is required; use jet_pad_name() for a connected pad.
 */
const char* _jet_bind_profile_name(int profile);

/**
 * Function: _jet_bind_profile_pad
 * Summary: Internal runtime hook that answers which pad slot, if any, a binding profile is currently plugged into.
 * Parameters:
 *   - profile: The profile's index, from 0 to _jet_bind_profile_count() - 1.
 * Returns: The pad slot reading that profile, or -1 when no connected controller does.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The rebinding screen needs it twice over: a capture has to come from THAT controller, and the PLAYER row sets the slot's player rather than the profile's.
 * Example:
 *   // No cartridge call is required; use jet_pad_connected() and jet_pad_id().
 */
int _jet_bind_profile_pad(int profile);

/**
 * Function: _jet_bind_slot
 * Summary: Internal runtime hook that gives a writable pointer at one binding slot, in whichever of the two tables it lives.
 * Parameters:
 *   - profile: 0 or any index outside the live profile table for the player's keyboard and mouse, or a controller's profile index for that controller.
 *   - player: Which player's keys, from 0 to JET_BIND_PLAYERS - 1. Ignored for a controller, whose bindings belong to the controller.
 *   - action: The action's id, from 0 to _jet_bind_action_count() - 1.
 *   - slot: 0 for the primary binding or 1 for the spare.
 * Returns: A pointer at the slot, which is never NULL.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: One accessor, so reading a binding for the rebinding screen and writing the one the player just captured can never disagree about which table they are in. The caller owns the ranges: they are not checked, because the screen derives all four from the counts above.
 * Example:
 *   // No cartridge call is required; use jet_bind_get() or jet_bind_pad_get() to read one.
 */
JetBindInput* _jet_bind_slot(int profile, int player, int action, int slot);

/**
 * Function: _jet_bind_axis_pressed
 * Summary: Internal runtime hook that answers whether a controller's stick was pushed into one direction this tick.
 * Parameters:
 *   - pad: The pad slot, from 0 to JET_GPAD_MAX_PADS - 1.
 *   - axis: The stick axis, from 0 to JET_GPAD_AXIS_COUNT - 1.
 *   - direction: -1 for the negative direction or 1 for the positive one.
 * Returns: true on the tick the stick crossed into that direction.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The edges _jet_bind_hw_update derives, read without consuming them, so the rebinding screen can capture a stick push as a binding. A cartridge asks the same question of an action instead, with jet_bind_id_pressed.
 * Example:
 *   // No cartridge call is required; use jet_bind_id_pressed() on a bound action.
 */
bool _jet_bind_axis_pressed(int pad, int axis, int direction);

/**
 * Function: _jet_bind_materialise_profiles
 * Summary: Internal runtime hook that brings every controller the player has ever adjusted into the live profile table, whether or not it is plugged in.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: It runs once. jet_bind_save and _jet_bind_hw_update already call it, so the rebinding screen's own call only matters for a cartridge that opens the screen before its first tick — without it that screen would offer no controller the player is not holding.
 * Example:
 *   // No cartridge call is required; jet_bind_save() already does this.
 */
void _jet_bind_materialise_profiles(void);

/**
 * Function: _jet_bind_hw_update
 * Summary: Internal runtime hook that matches connected controllers to their remembered bindings and works out which sticks were pushed this tick.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Called by _jet_input_hw_update() after the pads are snapshotted and before btn() is composed. Cartridge code must never call this directly because a second pass would consume the stick press and release edges it publishes.
 * Example:
 *   // No cartridge call is required; use jet_bind_id_pressed() inside _update().
 */
void _jet_bind_hw_update(void);

/**
 * Function: _jet_bind_hw_draw_begin
 * Summary: Internal runtime hook that republishes the stick edge sets as the whole frame's span, for the single _draw() that follows a frame's ticks.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_input_binds.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: The stick half of _jet_input_hw_draw_begin(), which is its only caller, and the twin of _jet_pad_hw_draw_begin(). Cartridge code must never call this directly because publishing consumes the span, and a second publish would report an empty one.
 * Example:
 *   // No cartridge call is required; use jet_bind_id_pressed() inside _draw().
 */
void _jet_bind_hw_draw_begin(void);

#endif // JET_INPUT_BINDS_H
