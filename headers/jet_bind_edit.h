// runtime/jet_bind_edit.h
#ifndef JET_BIND_EDIT_H
#define JET_BIND_EDIT_H

#include "jet_input_binds.h" // JetBindInput, and the table these two verbs write

// Change a binding from your own code. Most games never need this: the rebinding
// screen (jet_bind_open_ui, in terminal/jet_terminal.h) lets the player change
// every binding, and saves each change itself. Reach for it when your game draws
// its own controls screen, offers a "reset to defaults", or asks a question of
// its own first — "press the button you want to jump with".
//
// The two verbs are the twins of jet_bind_get and jet_bind_pad_get, and they keep
// the bindings' one rule: a key or a mouse button belongs to a PLAYER, and a pad
// button or a stick direction belongs to a CONTROLLER (jet_input_binds.h). So
// jet_bind_set takes the first kind and jet_bind_pad_set the second, and each
// refuses the other. Either one takes jet_bind_none() to clear a slot.
//
// A change is heard at once — by btn(), by jet_bind_down, and by the rebinding
// screen — but it is not written to disk until you call jet_bind_save. Call it
// once, as the player leaves your screen.
//
// Include "jet_bind_edit.h" yourself; jet64.h does not include it for you.
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware. The
// cart resolver compiles jet_bind_edit.c only when a cart's #include tree
// reaches this header, so a cart that never changes a binding from its own code
// carries none of it. Both verbs stand on the bindings' internal seam
// (_jet_bind_store, _jet_bind_pad_profile), the same one the rebinding screen
// writes through.
// </nocodex>
//
// Complete example (copy into main.c) — the first time a controller is used, its
// player is asked which button jumps:
//
//   #include "jet64.h"
//   #include "jet_bind_edit.h"
//
//   static int jump_action;
//   static bool asking = true;
//
//   void _init(void) {
//       jet_bind_register("JUMP", jet_bind_key(JET_KEY_SPACE), jet_bind_none());
//       jump_action = jet_bind_get_id("JUMP");
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (!asking) return;
//       for (int b = 0; b < JET_GPAD_BUTTON_COUNT; b++) {
//           if (!jet_pad_pressed(0, b)) continue;
//           if (jet_bind_pad_set(0, jump_action, 0, jet_bind_pad(b))) jet_bind_save();
//           asking = false;
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(asking ? "PRESS THE BUTTON TO JUMP WITH" : "JUMP IS SET", 16, 16, 15);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_bind_set
 * Summary: Changes one of a player's key or mouse bindings, for a controls screen of your own.
 * Parameters:
 *   - player: Which player, from 0 to JET_BIND_PLAYERS - 1. Player one is 0.
 *   - id: The action's id, from jet_bind_get_id.
 *   - slot: Which of the action's inputs: 0 for the primary, 1 for the spare.
 *   - input: The new binding — a key or a mouse button — or jet_bind_none() to clear the slot.
 * Returns: true if the binding was stored; false for a player, an id or a slot that is out of range, or for a pad button or a stick direction, which belong to a controller (use jet_bind_pad_set).
 * Include: jet_bind_edit.h
 * Notes: The twin of jet_bind_get, writing the same table the rebinding screen edits, so btn(), jet_bind_down and the screen all hear the change at once. A key given to a player who had none brings that player into jet_bind_down's "did anybody?" straight away. It is not written to disk until you call jet_bind_save.
 * Example:
 *   if (jet_bind_set(0, act_jump, 0, jet_bind_key(JET_KEY_UP))) jet_bind_save();
 */
bool jet_bind_set(int player, int id, int slot, JetBindInput input);

/**
 * Function: jet_bind_pad_set
 * Summary: Changes one of a controller's own gamepad bindings, for a controls screen of your own.
 * Parameters:
 *   - pad: Which pad slot, from 0 to 3. The binding belongs to the controller plugged in there, and goes with it to whichever slot it is plugged into next.
 *   - id: The action's id, from jet_bind_get_id.
 *   - slot: Which of the action's inputs: 0 for the primary, 1 for the spare.
 *   - input: The new binding — a pad button or a stick direction — or jet_bind_none() to clear the slot.
 * Returns: true if the binding was stored; false for a pad slot, an id or a binding slot that is out of range, for a key or a mouse button (use jet_bind_set), or when the pad slot has no controller of its own to remember the binding for.
 * Include: jet_bind_edit.h
 * Notes: The twin of jet_bind_pad_get. A controller is remembered by its model (jet_pad_id), from the first tick it is plugged in, so a call in _init — before any tick — answers false. So do an empty slot, a controller that names no model, and one past JET_BIND_MAX_PAD_PROFILES: each plays on the shipped layout, and changing that is jet_bind_register_pad's job, not the player's. Two identical controllers share one set of bindings, so a change made through either is heard on both. It is not written to disk until you call jet_bind_save.
 * Example:
 *   if (jet_bind_pad_set(0, act_jump, 1, jet_bind_pad_axis(JET_PAD_AXIS_LY, -1))) jet_bind_save();
 */
bool jet_bind_pad_set(int pad, int id, int slot, JetBindInput input);

#endif // JET_BIND_EDIT_H
