// runtime/jet_camera.h
#ifndef JET_CAMERA_H
#define JET_CAMERA_H

#include "jet_core.h" // IWYU pragma: keep
#include "jet_graphics.h" // IWYU pragma: keep

// Scroll your world without changing every position you draw. Set the camera
// to the world position that should sit at the screen's top-left corner, then
// draw the scene as if nothing had moved; the drawing calls take the camera's
// offset off every position for you. Reset the camera before you draw a score,
// a menu, or anything else that should stay put on the screen.
//
// Complete scrolling-camera example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_camera.h"
//
//   enum { WORLD_WIDTH = 2400, PLAYER_SIZE = 16 };
//   static float player_x = 640.0f;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       const float speed = 240.0f;
//       if (btn(BTN_LEFT))  player_x -= speed * dt;
//       if (btn(BTN_RIGHT)) player_x += speed * dt;
//       player_x = jet_clampf(player_x, 0.0f, (float)(WORLD_WIDTH - PLAYER_SIZE));
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_cam_set((int)player_x - JET_WIDTH / 2, 0);
//       jet_rectfill(0, JET_HEIGHT / 2, WORLD_WIDTH, 8, 5);
//       jet_rectfill((int)player_x, JET_HEIGHT / 2 - PLAYER_SIZE, PLAYER_SIZE, PLAYER_SIZE, 12);
//       jet_cam_reset();
//       jet_print("SCREEN-SPACE HUD", 16, 16, 15);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_cam_set
 * Summary: Moves the camera, so that everything drawn afterwards is shifted by this offset.
 * Parameters:
 *   - x: The world position that will sit at the left edge of the screen, in pixels.
 *   - y: The world position that will sit at the top edge of the screen, in pixels.
 * Returns: Nothing.
 * Include: jet_camera.h
 * Notes: The drawing calls take this offset off every position before they clip and draw, so your game code can draw in world positions and let the camera do the scrolling. Call jet_cam_reset before drawing a score, a menu, or anything else that belongs to the screen rather than the world.
 * Example:
 *   jet_cam_set(player_x - JET_WIDTH / 2, player_y - JET_HEIGHT / 2);
 *   spr_grid(12, enemy_x, enemy_y, false, false);
 */
static inline void jet_cam_set(int x, int y) {
    _jet_cam_x = x;
    _jet_cam_y = y;
}

/**
 * Function: jet_cam_reset
 * Summary: Puts the camera back at zero, so drawing lands on the screen exactly where you say.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_camera.h
 * Notes: Call it before you draw a score, a menu, a debugging overlay, or anything else that should not scroll with the world.
 * Example:
 *   jet_cam_reset();
 *   print("ready", 8, 8, 7);
 */
static inline void jet_cam_reset(void) {
    _jet_cam_x = 0;
    _jet_cam_y = 0;
}

#endif // JET_CAMERA_H
