// runtime/jet64.h
#ifndef JET64_H
#define JET64_H

#include "jet_core.h" // IWYU pragma: keep

// The one header every cartridge includes. It brings in the everyday firmware:
// drawing, sprites and animation, text, tile maps, collision, the keyboard,
// mouse and gamepads, files, the palette, display settings, maths, and your
// cartridge's own publishing record. It also declares the four
// hooks at the bottom of this file. Those are the four functions your game
// writes and the runtime calls: _init once at the start, _update 120 times a
// second, _draw once per frame, and _end at the finish. Start with _init,
// _update and _draw.
//
// A few headers are opt-in modules, such as jet_audio.h, jet_pool.h and
// jet_pcm.h. Include one of those yourself only when you want what it does: a
// cart that makes a sound includes jet_audio.h, and a cart that makes none ships
// a player that never opens an audio device. Everything else is already here.
//
// Complete first-cart example (copy into main.c):
//
//   #include "jet64.h"
//
//   enum { PLAYER_SIZE = 16, PLAYER_COLOUR = 12 };
//   static float player_x = 80.0f;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       const float speed = 160.0f;
//       if (btn(BTN_LEFT))  player_x -= speed * dt;
//       if (btn(BTN_RIGHT)) player_x += speed * dt;
//       player_x = jet_clampf(player_x, 0.0f, (float)(JET_WIDTH - PLAYER_SIZE));
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_rectfill((int)player_x, JET_HEIGHT / 2, PLAYER_SIZE, PLAYER_SIZE, PLAYER_COLOUR);
//       jet_print("MOVE WITH LEFT / RIGHT", 16, 16, 15);
//   }
//
//   void _end(void) {}

// ====================== C STANDARD LIBRARY & SUBSYSTEMS ======================

// Memory: reserve working memory for big things
#ifndef JET_MEMORY_H
#include "jet_memory.h" // IWYU pragma: keep
#endif

// Graphics: pixels, shapes, sprites and textured triangles
#ifndef JET_GRAPHICS_H
#include "jet_graphics.h" // IWYU pragma: keep
#endif

// Tile maps: layered 32-bit maps with scrolling and collision
#ifndef JET_MAP_H
#include "jet_map.h" // IWYU pragma: keep
#endif

// Sprites: draw Sprite Editor cells and animation frames
#ifndef JET_SPRITE_H
#include "jet_sprite.h" // IWYU pragma: keep
#endif

// Animation: play frame sequences, with events and stateless lookup
#ifndef JET_ANIM_H
#include "jet_anim.h" // IWYU pragma: keep
#endif

// Collision: pixel-perfect and shape tests
#ifndef JET_COLLISION_H
#include "jet_collision.h" // IWYU pragma: keep
#endif

// Text: print with the font banks or a colour sprite font
#ifndef JET_TEXT_H
#include "jet_text.h" // IWYU pragma: keep
#endif

// Utilities: random numbers, running statistics, the FPS counter
#ifndef JET_UTILS_H
#include "jet_utils.h" // IWYU pragma: keep
#endif

// Display: resolution, orientation and buffering
#ifndef JET_DISPLAY_H
#include "jet_display.h" // IWYU pragma: keep
#endif

// Input: the mouse and the keyboard
#ifndef JET_INPUT_H
#include "jet_input.h" // IWYU pragma: keep
#endif

// Input bindings: named actions the player can rebind
#ifndef JET_INPUT_BINDS_H
#include "jet_input_binds.h" // IWYU pragma: keep
#endif

// Gamepads: up to four controllers with buttons, sticks and triggers
#ifndef JET_GAMEPAD_H
#include "jet_gamepad.h" // IWYU pragma: keep
#endif

// Files: saved games in the cartridge's own folder, and bundled assets
#ifndef JET_IO_H
#include "jet_io.h" // IWYU pragma: keep
#endif

// Host: ask about, and change, the program running the cartridge
#ifndef JET_HOST_H
#include "jet_host.h" // IWYU pragma: keep
#endif

// Publishing: your cartridge's own name, version and copyright line
#ifndef JET_PUBLISH_H
#include "jet_publish.h" // IWYU pragma: keep
#endif

// Palette: the 256 colours, gradients and colour cycling
#ifndef JET_PALETTE_H
#include "jet_palette.h" // IWYU pragma: keep
#endif

// Maths: vectors, clamping, angles and wrap-around playfields
#ifndef JET_MATH_H
#include "jet_math.h" // IWYU pragma: keep
#endif

/**
 * Function: _init
 * Summary: Runs once when your cartridge starts. Write this function in main.c to set your game up.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: The runtime calls _init after it has connected the cartridge to the machine's memory and started every subsystem, so every jet_ call is ready to use. Load your animations and sounds here, set the sprite grid, seed the random numbers, and give your game's variables their starting values. Never call _init yourself.
 * Example:
 *   void _init(void) {
 *       jet_set_sprite_grid(16, 16);
 *       jet_randomise();
 *   }
 */
extern void _init(void);

/**
 * Function: _update
 * Summary: Runs 120 times a second to move your game forward by one tick. Write this function in main.c.
 * Parameters:
 *   - dt: How much time this tick covers, in seconds. The machine runs at a fixed 120 ticks a second, so dt is one 120th of a second.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Read the controls, move things, run the enemies' thinking, advance animations, and change any game state here. Draw nothing here; drawing belongs in _draw. Because every tick is the same length on every machine, a game that moves things by speed * dt runs the same everywhere.
 * Example:
 *   void _update(float dt) {
 *       if (btn(BTN_RIGHT)) player_x += 120.0f * dt;
 *   }
 */
extern void _update(float dt);

/**
 * Function: _draw
 * Summary: Draws one frame of your game onto the screen. Write this function in main.c.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Use the drawing calls here: cls to clear the screen, spr to draw a sprite, rectfill for a box, print for words, and jet_map_draw_view for a tile map.
 *
 *        _draw only shows the state that _update left behind. It must not move anything. The runtime promises to call _update for 120 ticks of game time every second on every machine, but it makes no such promise about _draw. An exported player runs every tick it owes but draws only once for each frame the screen shows, so on a 60 Hz monitor _draw runs once for every two _update calls, and on a slow frame it skips more. Anything that has to keep time, such as animation timers, physics, particle ages, or the bookkeeping of a dirty-rectangle scheme, belongs in _update, where it is counted exactly once per tick.
 *
 *        You may read the controls inside _draw, which is what an immediate-mode interface does. Every press-or-release question (jet_mouse_pressed and jet_mouse_released, jet_key_pressed and jet_key_released, jet_pad_pressed and jet_pad_released, btnp and btnr) and every movement amount (jet_mouse_wheel, jet_mouse_dx and jet_mouse_dy) answers for the whole stretch since the previous _draw, so a click is never lost to a frame that carried several ticks. Inside _update the same calls answer for that one tick, which is the view a simulation wants. See docs/architecture.md section 3.D.
 * Example:
 *   void _draw(void) {
 *       cls(0);
 *       spr_grid(0, player_x, player_y, false, false);
 *   }
 */
extern void _draw(void);

/**
 * Function: _end
 * Summary: Runs once when your cartridge is about to stop. Write it in main.c only if you have something to tidy up.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Save what must be saved, stop any sound, and let go of anything held outside the cartridge. If you do not write _end, the runtime supplies an empty one where the compiler allows it, so a cartridge with nothing to clean up needs no _end at all.
 * Example:
 *   void _end(void) {
 *       jet_sfx_stop_all(); // a cartridge that makes a sound includes jet_audio.h
 *   }
 */
extern void _end(void); // Called once before the runtime exits

#endif // JET64_H
