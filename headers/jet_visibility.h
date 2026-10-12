// runtime/jet_visibility.h
#ifndef JET_VISIBILITY_H
#define JET_VISIBILITY_H

#include <stdbool.h>

// Whether the person can see the game right now. jet_visibility_hidden answers
// true while the player's window is minimised, while its browser tab is in the
// background or its page is hidden, and while the Workstation's own window is
// minimised; false while the picture is on screen.
//
// Include "jet_visibility.h" yourself; jet64.h does not include it for you.
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware. The
// cart resolver compiles jet_visibility.c only when a cart's #include tree reaches
// this header, and the exporter keys the players' window watch and the web
// player's background ticking off the very same signal — so a cart that never
// asks costs nothing, and a web page that never asks still sleeps the moment it
// is hidden (docs/networking/design.md §14, docs/dgalaga/networking_report.md H10).
// </nocodex>
//
// What it is for: a game that stops politely. A single-player game can pause
// itself the moment it is hidden. A game played over the network can do better
// than vanish: it can tell its partner "PLAYER 2 IS AWAY" and pause the session on
// both screens, rather than leave the partner staring at a frozen opponent.
//
// Three rules come with it.
//
//   1. A HIDDEN WEB PAGE STILL TICKS, SLOWLY. A browser stops drawing a page the
//      moment its tab goes into the background. A cart that includes this header
//      is kept ticking there anyway, at the pace the browser allows a hidden page
//      — about once a second, a few ticks at a time — so it can see the change and
//      act on it. Do the little that matters (say you are away, pause) and nothing
//      else: the ticks a hidden page is given are few, and a game simulated through
//      them would crawl.
//   2. HIDDEN IS CERTAIN; SHOWN IS A HOPE. true means the person cannot see the
//      game. false means only that nothing said otherwise: a window wholly covered
//      by another program's is counted as hidden where the host can tell (the
//      Workstation on a Mac can, and so can most browsers) and as shown where it
//      cannot — a desktop player asks only whether it is minimised. Losing the
//      keyboard is not hiding either. Use jet_key_* and the mouse if you want to
//      know whether the player is still at the controls.
//   3. READ IT EVERY TICK. Act on the change when you see it; there is no event to
//      wait for. The host may learn of a minimise a quarter of a second late, or
//      up to a second late while the cart rests (jet_host_rest).
//
// Complete pause-when-hidden example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_visibility.h"
//
//   static int score;
//   static bool paused;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       (void)dt;
//       // Pause the moment the window is minimised or the tab is hidden. Unpausing
//       // is the player's own act, so a game never restarts behind their back.
//       if (jet_visibility_hidden()) paused = true;
//       if (paused && jet_key_pressed(JET_KEY_SPACE)) paused = false;
//       if (!paused) score++;
//   }
//
//   void _draw(void) {
//       char line[32];
//       jet_cls(0);
//       snprintf(line, sizeof line, "SCORE %d", score);
//       jet_print(line, 8, 8, 15);
//       if (paused) jet_print("PAUSED - PRESS SPACE", 8, 24, 10);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_visibility_hidden
 * Summary: Reports whether the person cannot see the game: its window minimised, its browser tab in the background, its page hidden.
 * Parameters:
 *   - None.
 * Returns: true while the game cannot be seen, false while it is on screen.
 * Include: jet_visibility.h
 * Notes: The Workstation answers for its own window, the native player for its window, and the web player for its page. true is certain; false means only that nothing said otherwise — a window wholly covered by another program's reads true where the computer can tell (macOS, most browsers) and false where it cannot. On the web, a cart that includes jet_visibility.h keeps ticking while its page is hidden — about once a second, a few ticks at a time — so that it can act on the answer; keep what it does there small. In the agent door the answer is whatever the scenario says (a `hidden` event), so a cart's away-and-back path can be played headlessly.
 * Example:
 *   if (jet_visibility_hidden() && !s_said_away) { send_away_message(); s_said_away = true; }
 */
bool jet_visibility_hidden(void);

#endif // JET_VISIBILITY_H
