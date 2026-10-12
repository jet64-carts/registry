// runtime/jet_lockstep.h
#ifndef JET_LOCKSTEP_H
#define JET_LOCKSTEP_H

// ============================================================================
// Jet64 Lockstep — the deterministic session engine (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_ws.h and jet_grid_hash.h it is deliberately kept out of jet64.h, so
// it costs nothing unless a cart asks for it: the cart resolver compiles
// jet_lockstep.c only when a cart's #include tree reaches this header. A cart
// that never includes it contains no session code at all.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_lockstep.h"      // <-- explicit opt-in
//
// Jet64 is already a lockstep machine: every host runs a cart at the same
// 120 Hz tick, and a seeded game keeps its state in memory it owns. So two
// machines that start from the same state and apply the same inputs on the
// same frame stay identical, and only the INPUTS need to cross the wire. This
// module is everything around that: it delays your own input so a partner can
// hold it in time, re-sends what was lost, acknowledges what arrived, waits
// honestly when an input is late, keeps two clocks in step, notices the moment
// two games part, and — if you ask for it — guesses a partner's input and
// rewinds when the guess was wrong.
//
// It sends nothing itself. It makes small packets, and your cart carries them
// by whatever transport it chose (a jet_rtc connection's FAST lane, a jet_ws
// relay, jet_udp on a LAN, or a plain array in a test), handing arrivals back
// with jet_lockstep_receive.
//
// <nocodex>
// THE SHAPE IS GGPO'S, which hundreds of shipped games have proven: the cart
// describes its input and supplies callbacks — step, save, load, digest,
// confirmed — and the engine does the rest. It is pure guest C on purpose: no
// host service, no Rust, no clock and no allocation, so it is a pure function
// of the calls and packets it is given, identical on all three runtimes, and
// provable headlessly (tests/jet_lockstep). The full design, and the reason for
// every rule below, is docs/networking/lockstep.md.
// </nocodex>
//
// ----------------------------------------------------------------------------
// THE SIX RULES
// ----------------------------------------------------------------------------
// 1. ONE SLOT, THREE CALLS. Once per frame your game would step, call
//    jet_lockstep_add_local_input with this frame's hand, then
//    jet_lockstep_advance, then jet_lockstep_poll_packet until it returns 0,
//    sending each packet it gives you. Hand every packet that arrives to
//    jet_lockstep_receive, whenever it arrives. That is the whole loop.
//
// 2. STEP IS THE ONLY PLACE THE GAME CHANGES. Your step callback receives
//    every player's input for one frame and must advance the game from that
//    and nothing else. Reading the keyboard, a clock or an unseeded random
//    number into the game state anywhere else parts the two games.
//
// 3. EVERY MACHINE STARTS EQUAL. Same seed, same starting state, and the same
//    configuration apart from local_player and input_delay, which are each
//    machine's own. A session cannot be joined late.
//
// 4. WITH PREDICTION, A FRAME MAY RUN TWICE. When prediction is above 0 the
//    engine guesses a late input, and when the guess proves wrong it loads the
//    state it saved and steps the frames again with replaying == true. Silence
//    one-shot effects (sounds, saves) while replaying; never skip the state
//    change itself.
//
// 5. TURNS ARE GAME LOGIC. Change who is active with jet_lockstep_set_active
//    from inside step, for a frame after the one being stepped. Every machine
//    then makes the same call on the same frame, because every machine runs
//    the same game.
//
// 6. HANDLES HAVE LIFETIMES. A zero-initialised JetLockstep is safely invalid,
//    and a closed one stays stale: every call refuses it with
//    JET_LOCKSTEP_E_ARG rather than reaching whatever session is open now.
//
// <nocodex>
// WHY A SLOT IS NOT A FRAME. A slot is one call of jet_lockstep_advance, made
// whether or not a frame can be stepped. Slots are the engine's only clock:
// they pass while it waits, so a round trip and a silence can be measured in
// them without reading any clock, and a cart that stops calling has stopped
// time — which is the truth on a hidden web page too.
// </nocodex>
//
// ----------------------------------------------------------------------------
// HOW THE CALLS ANSWER
// ----------------------------------------------------------------------------
// Every call but jet_lockstep_advance answers with one convention: a positive
// number when it did what was asked, 0 when there was nothing to do (which is
// never an error), and a negative JetLockstepResult when it refused.
// jet_lockstep_advance answers with a JetLockstepResult of its own: treat
// JET_LOCKSTEP_WAITING and JET_LOCKSTEP_SYNCING exactly alike — nothing was
// stepped this slot; call again next slot.
//
// ----------------------------------------------------------------------------
// THREE SHAPES OF SESSION
// ----------------------------------------------------------------------------
// * AT ONCE — every player active. Give each machine an input_delay of about
//   half the round trip in frames, and the game never waits on a good link.
// * ALTERNATING — one player active at a time, handed over by
//   jet_lockstep_set_active. The active machine waits for nobody, so it can
//   run with input_delay 0 at any distance; at each handover the game pauses
//   for about one round trip, on the frame your game chose. With prediction
//   the pause is shorter by about that many frames: the old owner shows the
//   new turn's first frames on guesses, and corrects them as they arrive.
// * SPECTATORS — players never made active. They run the game and send
//   packets, but nobody waits for them: one that falls a whole history behind
//   is left behind rather than allowed to hold the players back, and to it the
//   session answers JET_LOCKSTEP_E_LEFT_BEHIND.
//
// ----------------------------------------------------------------------------
// SIZING THE ARENA
// ----------------------------------------------------------------------------
// Everything a session keeps lives in memory you lend it — any memory: a
// static array is fine. Fill in the configuration, ask
// jet_lockstep_arena_bytes for the size, and lend at least that much. With
// prediction 0 a two-player session needs a few kilobytes; with prediction the
// arena also holds prediction + 1 copies of your saved state, so size
// snapshot_bytes to your largest save.
//
// Complete two-seat example (copy into main.c): two seats on one machine,
// joined by a pretend wire that takes WIRE_SLOTS to deliver a packet. Seat 0
// steers with the arrow keys and seat 1 with W A S D; each seat runs its own
// copy of the game, and the two copies never differ by a pixel.
//
//   #include "jet64.h"
//   #include "jet_lockstep.h"
//
//   enum {
//       SEATS = 2,
//       WIRE_SLOTS = 4,         // the wire's one-way delay, in slots
//       INPUT_DELAY = 5,        // half the round trip: each leg lands a slot after it is due
//       WIRE_CAPACITY = 32,     // packets in flight at once
//       ARENA_WORDS = 1024,     // 8 KiB per seat: more than the session asks
//       DIGEST_FRAMES = 60,     // compare the two games twice a second
//       SESSION_ID = 64,        // any number every seat agrees on
//       PACKET_BYTES = JET_LOCKSTEP_PACKET_BYTES(1),
//       FIELD = 140,            // the playfield, in pixels, on every machine
//       BOX = 8,
//       PAD_LEFT = 1, PAD_RIGHT = 2, PAD_UP = 4, PAD_DOWN = 8,
//       PAPER = 0, INK = 7, CALM = 11, ALARM = 8, FIRST_SEAT_COLOUR = 9,
//       TEXT_X = 2,             // the lines of text beneath the playfields
//       STATS_Y = FIELD + 4,
//       VERDICT_Y = FIELD + 16,
//       LINE_CHARS = 48
//   };
//
//   typedef struct { int32_t x[SEATS], y[SEATS]; uint32_t moves; } Game;
//   typedef struct { uint32_t due, len; uint8_t from, to, bytes[PACKET_BYTES]; } Packet;
//
//   // Each seat's own copy of the world, and its session.
//   static Game game[SEATS];
//   static JetLockstep seat[SEATS];
//   static uint64_t arena[SEATS][ARENA_WORDS];
//   static Packet wire[WIRE_CAPACITY];
//   static uint32_t wire_used, slot;
//   static bool parted;
//
//   static int32_t clamp_to_field(int32_t v) {
//       return v < 0 ? 0 : (v > FIELD - BOX ? FIELD - BOX : v);
//   }
//
//   static void game_step(void* user, const void* inputs, uint32_t frame, bool replaying) {
//       // The whole game: one byte of input per seat moves that seat's box.
//       Game* g = (Game*)user;
//       const uint8_t* pad = (const uint8_t*)inputs;
//       (void)frame;
//       (void)replaying;
//       for (int p = 0; p < SEATS; p++) {
//           const int32_t dx = ((pad[p] & PAD_RIGHT) ? 1 : 0) - ((pad[p] & PAD_LEFT) ? 1 : 0);
//           const int32_t dy = ((pad[p] & PAD_DOWN) ? 1 : 0) - ((pad[p] & PAD_UP) ? 1 : 0);
//           g->x[p] = clamp_to_field(g->x[p] + dx);
//           g->y[p] = clamp_to_field(g->y[p] + dy);
//           g->moves += (uint32_t)(dx != 0 || dy != 0);
//       }
//   }
//
//   static uint64_t game_digest(void* user) {
//       const Game* g = (const Game*)user;
//       uint64_t d = g->moves;
//       for (int p = 0; p < SEATS; p++) {
//           d = d * FIELD * FIELD + (uint64_t)(g->y[p] * FIELD + g->x[p]);
//       }
//       return d;
//   }
//
//   static uint8_t read_pad(int s) {
//       const int left = s == 0 ? JET_KEY_LEFT : JET_KEY_A, right = s == 0 ? JET_KEY_RIGHT : JET_KEY_D;
//       const int up = s == 0 ? JET_KEY_UP : JET_KEY_W, down = s == 0 ? JET_KEY_DOWN : JET_KEY_S;
//       return (uint8_t)((jet_key_down(left) ? PAD_LEFT : 0) | (jet_key_down(right) ? PAD_RIGHT : 0)
//                        | (jet_key_down(up) ? PAD_UP : 0) | (jet_key_down(down) ? PAD_DOWN : 0));
//   }
//
//   static void wire_send(int s) {
//       // The pretend transport: every packet a seat makes waits WIRE_SLOTS on the wire.
//       uint8_t to;
//       while (wire_used < WIRE_CAPACITY) {
//           Packet* p = &wire[wire_used];
//           const int n = jet_lockstep_poll_packet(seat[s], p->bytes, sizeof p->bytes, &to);
//           if (n <= 0) break;
//           p->due = slot + WIRE_SLOTS;
//           p->len = (uint32_t)n;
//           p->from = (uint8_t)s;
//           p->to = to;
//           wire_used++;
//       }
//   }
//
//   static void wire_deliver(void) {
//       uint32_t kept = 0;
//       for (uint32_t i = 0; i < wire_used; i++) {
//           if (wire[i].due <= slot) {
//               jet_lockstep_receive(seat[wire[i].to], wire[i].from, wire[i].bytes, wire[i].len);
//           } else {
//               wire[kept++] = wire[i];
//           }
//       }
//       wire_used = kept;
//   }
//
//   void _init(void) {
//       for (int s = 0; s < SEATS; s++) {
//           const JetLockstepConfig cfg = {
//               .players = SEATS, .local_player = (uint8_t)s, .input_bytes = 1,
//               .input_delay = INPUT_DELAY, .digest_interval = DIGEST_FRAMES,
//               .session_id = SESSION_ID,
//               .callbacks = { .step = game_step, .digest = game_digest },
//               .user = &game[s], .arena = arena[s], .arena_bytes = sizeof arena[s],
//           };
//           for (int p = 0; p < SEATS; p++) {
//               game[s].x[p] = (FIELD / (SEATS + 1)) * (p + 1);
//               game[s].y[p] = FIELD / 2;
//           }
//           seat[s] = jet_lockstep_open(&cfg);
//       }
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       slot++;
//       for (int s = 0; s < SEATS; s++) {
//           const uint8_t pad = read_pad(s);
//           jet_lockstep_add_local_input(seat[s], &pad);
//           if (jet_lockstep_advance(seat[s]) == JET_LOCKSTEP_E_DESYNC) parted = true;
//           wire_send(s);
//       }
//       wire_deliver();
//   }
//
//   void _draw(void) {
//       char line[LINE_CHARS];
//       JetLockstepStats st;
//       jet_cls(PAPER);
//       for (int s = 0; s < SEATS; s++) {
//           const int left = s * (FIELD + BOX);
//           jet_rect(left, 0, FIELD, FIELD, INK);
//           for (int p = 0; p < SEATS; p++) {
//               jet_rectfill(left + game[s].x[p], game[s].y[p], BOX, BOX, (uint8_t)(FIRST_SEAT_COLOUR + p));
//           }
//           if (jet_lockstep_stats(seat[s], &st) > 0) {
//               snprintf(line, sizeof line, "FRAME %u RTT %u", (unsigned)st.frame,
//                        (unsigned)st.rtt_slots[SEATS - 1 - s]);
//               jet_print(line, left + TEXT_X, STATS_Y, INK);
//           }
//       }
//       jet_print(parted ? "THE GAMES PARTED" : "IN STEP", TEXT_X, VERDICT_Y, parted ? ALARM : CALM);
//   }
//
//   void _end(void) {
//       for (int s = 0; s < SEATS; s++) jet_lockstep_close(seat[s]);
//   }

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memcpy (sandbox-safe)

// ============================================================================
// LIMITS — every bound named once
// ============================================================================

/**
 * Constant: JET_LOCKSTEP_MAX_PLAYERS
 * Summary: The most players one session may seat, spectators included.
 * Include: jet_lockstep.h
 * Notes: Every machine in a session is one player. A spectator is simply a player who is never made active. The per-player arrays in JetLockstepStats are this long; entries at or beyond your players count read 0.
 * Example:
 *   uint16_t worst_rtt = 0;
 *   for (int p = 0; p < JET_LOCKSTEP_MAX_PLAYERS; p++) if (st.rtt_slots[p] > worst_rtt) worst_rtt = st.rtt_slots[p];
 */
#define JET_LOCKSTEP_MAX_PLAYERS 4

/**
 * Constant: JET_LOCKSTEP_MAX_SESSIONS
 * Summary: The most sessions one cart may hold open at once: one per seat.
 * Include: jet_lockstep.h
 * Notes: A networked game needs one. The rest let a cart seat every player of a full session on one machine, each in its own session joined by a pretend wire — as the example cart does with two — which is the way to try a four-player game before there is a network. jet_lockstep_open returns an invalid handle while every session is in use; close one to free its place.
 * Example:
 *   JetLockstep seats[JET_LOCKSTEP_MAX_SESSIONS] = {0};
 */
#define JET_LOCKSTEP_MAX_SESSIONS JET_LOCKSTEP_MAX_PLAYERS

/**
 * Constant: JET_LOCKSTEP_INPUT_MAX
 * Summary: The most bytes of input one player may contribute to one frame.
 * Include: jet_lockstep.h
 * Notes: Keep your input small and packed — a byte of buttons, a byte or two per stick — because every frame of it crosses the wire, up to JET_LOCKSTEP_PACKET_INPUTS times in each packet. Thirty-two bytes is a generous ceiling, not a target.
 * Example:
 *   typedef struct { uint8_t buttons; int8_t stick_x, stick_y; } PadInput;   // 3 bytes
 *   _Static_assert(sizeof(PadInput) <= JET_LOCKSTEP_INPUT_MAX, "input too large");
 */
#define JET_LOCKSTEP_INPUT_MAX 32

/**
 * Constant: JET_LOCKSTEP_DELAY_MAX
 * Summary: The largest input delay, in frames, a session accepts.
 * Include: jet_lockstep.h
 * Notes: Fifteen frames is 125 ms at 120 Hz — more delay than any player will tolerate, so a larger number would only ever be a mistake. jet_lockstep_open and jet_lockstep_set_input_delay refuse anything above it.
 * Example:
 *   uint8_t delay = rtt_frames / 2 > JET_LOCKSTEP_DELAY_MAX ? JET_LOCKSTEP_DELAY_MAX : (uint8_t)(rtt_frames / 2);
 */
#define JET_LOCKSTEP_DELAY_MAX 15

/**
 * Constant: JET_LOCKSTEP_PREDICTION_MAX
 * Summary: The most frames a session may run ahead of the inputs it holds, guessing the rest.
 * Include: jet_lockstep.h
 * Notes: Each frame of prediction costs one more saved copy of your game state in the arena, and a wrong guess costs a replay of up to that many frames in one slot. Rollback games usually choose 6 to 8.
 * Example:
 *   cfg.prediction = 8;   // rollback, at most 8 frames in speculation
 */
#define JET_LOCKSTEP_PREDICTION_MAX 15

/**
 * Constant: JET_LOCKSTEP_HISTORY
 * Summary: How many frames of each player's input a session remembers.
 * Include: jet_lockstep.h
 * Notes: Your own inputs stay in this history until every peer has acknowledged them. A peer nobody waits for — a spectator, a benched player — that falls this many frames behind is left behind, and the inputs it lacks are let go. One the game still needs holds them: jet_lockstep_add_local_input then answers JET_LOCKSTEP_E_FULL rather than overwrite an input that peer may still need, and the frames between are filled in once it catches up. A power of two.
 * Example:
 *   JetLockstepStats st;
 *   if (jet_lockstep_add_local_input(s, &pad) == JET_LOCKSTEP_E_FULL && jet_lockstep_stats(s, &st) > 0
 *       && st.silence_slots[peer] > GIVE_UP_SLOTS) jet_lockstep_close(s);   // that player has gone
 */
#define JET_LOCKSTEP_HISTORY 256

/**
 * Constant: JET_LOCKSTEP_PACKET_INPUTS
 * Summary: The most frames of input one packet carries.
 * Include: jet_lockstep.h
 * Notes: Each packet carries up to this many of the inputs its peer has not acknowledged, oldest first; when there are more, as on a long link, the packets that follow sweep on through the rest and then begin again from the oldest. So a lost packet costs at most one sweep, a burst of loss at most one round trip, and a link of any length delivers a frame of input for every slot.
 * Example:
 *   uint32_t largest_input_payload = JET_LOCKSTEP_PACKET_INPUTS * sizeof(PadInput);
 */
#define JET_LOCKSTEP_PACKET_INPUTS 32

/**
 * Constant: JET_LOCKSTEP_PACKET_HEADER
 * Summary: The bytes every packet spends before the inputs it carries.
 * Include: jet_lockstep.h
 * Notes: Magic, version, sender, session id, checksum, frame, acknowledgement, the first input frame, the input count, flags, and the two 32-bit round-trip stamps — the layout is in docs/networking/lockstep.md §5.5, little-endian throughout, so it is the same on every runtime.
 * Example:
 *   uint32_t empty_packet = JET_LOCKSTEP_PACKET_HEADER;   // a packet with no inputs and no digest
 */
#define JET_LOCKSTEP_PACKET_HEADER 34

/**
 * Constant: JET_LOCKSTEP_PACKET_DIGEST
 * Summary: The bytes a packet spends when it carries a digest: the frame, then the value.
 * Include: jet_lockstep.h
 * Notes: A packet carries at most one digest — this machine's newest confirmed one, or once the games have parted the frame that parted them — so the cost is the same however often you ask for digests.
 * Example:
 *   uint32_t with_digest = JET_LOCKSTEP_PACKET_HEADER + JET_LOCKSTEP_PACKET_DIGEST;
 */
#define JET_LOCKSTEP_PACKET_DIGEST 12

/**
 * Constant: JET_LOCKSTEP_PACKET_BYTES
 * Summary: The largest packet a session with the given input size can make — the buffer size jet_lockstep_poll_packet requires.
 * Parameters:
 *   - input_bytes: The session's input_bytes, 1 to JET_LOCKSTEP_INPUT_MAX.
 * Include: jet_lockstep.h
 * Notes: A constant expression, so it can size a static array or an enum. jet_lockstep_poll_packet refuses a smaller buffer with JET_LOCKSTEP_E_ARG on every call, never only on the calls that happen to produce a large packet, so a buffer that is too small fails the first time you try it.
 * Example:
 *   static uint8_t packet[JET_LOCKSTEP_PACKET_BYTES(sizeof(PadInput))];
 */
#define JET_LOCKSTEP_PACKET_BYTES(input_bytes) \
    (JET_LOCKSTEP_PACKET_HEADER + JET_LOCKSTEP_PACKET_INPUTS * (input_bytes) + JET_LOCKSTEP_PACKET_DIGEST)

/**
 * Constant: JET_LOCKSTEP_PACKET_MAX
 * Summary: The largest packet any session can make: room for any input size.
 * Include: jet_lockstep.h
 * Notes: 1070 bytes, which fits a single UDP datagram and a single WebRTC message on any path. Use JET_LOCKSTEP_PACKET_BYTES for a tighter buffer when your input is small.
 * Example:
 *   uint8_t packet[JET_LOCKSTEP_PACKET_MAX];
 */
#define JET_LOCKSTEP_PACKET_MAX JET_LOCKSTEP_PACKET_BYTES(JET_LOCKSTEP_INPUT_MAX)

/**
 * Constant: JET_LOCKSTEP_SYNC_PERIOD
 * Summary: Time sync skips at most one slot in every this many.
 * Include: jet_lockstep.h
 * Notes: When this machine runs more than a frame ahead of its slowest active peer, jet_lockstep_advance answers JET_LOCKSTEP_SYNCING once in every this many slots instead of stepping, so the faster machine eases back by an even trickle rather than a visible hitch. Only a slot that could have stepped is ever skipped, and only peers the game is waiting for set the pace.
 * Example:
 *   uint32_t most_skips_per_second = 120 / JET_LOCKSTEP_SYNC_PERIOD;
 */
#define JET_LOCKSTEP_SYNC_PERIOD 8

/**
 * Constant: JET_LOCKSTEP_DIGEST_SLOTS
 * Summary: How many digests a session holds for comparison, its own and each peer's.
 * Include: jet_lockstep.h
 * Notes: Two machines compare a digest once both have confirmed that frame. If they drift more than this many digests apart, the oldest pending comparison is skipped — never misjudged — and the next one is made instead. At the smallest interval a session accepts, JET_LOCKSTEP_DIGEST_INTERVAL_MIN, that is the whole history; with a digest every 60 frames it is 1920 frames. Whichever machine finds a parting tells the others, so all of them report it.
 * Example:
 *   uint32_t widest_gap_checked = JET_LOCKSTEP_DIGEST_SLOTS * cfg.digest_interval;
 */
#define JET_LOCKSTEP_DIGEST_SLOTS 32

/**
 * Constant: JET_LOCKSTEP_DIGEST_INTERVAL_MIN
 * Summary: The smallest digest_interval a session accepts, other than 0 for none.
 * Include: jet_lockstep.h
 * Notes: JET_LOCKSTEP_HISTORY / JET_LOCKSTEP_DIGEST_SLOTS — 8 frames, at which the digests a session holds span the whole history, as far apart as two machines in one session drift. A smaller interval could hold too short a span to compare anything at all, so jet_lockstep_open refuses it rather than check nothing in silence.
 * Example:
 *   cfg.digest_interval = JET_LOCKSTEP_DIGEST_INTERVAL_MIN;   // the tightest check there is: 15 a second
 */
#define JET_LOCKSTEP_DIGEST_INTERVAL_MIN (JET_LOCKSTEP_HISTORY / JET_LOCKSTEP_DIGEST_SLOTS)

/**
 * Constant: JET_LOCKSTEP_SCHEDULE_MAX
 * Summary: How many activity changes may be in flight at once.
 * Include: jet_lockstep.h
 * Notes: A change is in flight from the step that makes it until the frame being stepped lies JET_LOCKSTEP_PREDICTION_MAX + 1 frames past its from; a change that alters nothing is never stored at all. That count depends on the game's frames alone, so every machine stepping a frame counts the same, and jet_lockstep_set_active answers JET_LOCKSTEP_E_FULL past this on every machine alike — an answer game logic may act on. A turn-based game uses two or three.
 * Example:
 *   if (jet_lockstep_set_active(s, next, true, frame + 1) == JET_LOCKSTEP_E_FULL) g->owner_keeps_turn = true;
 */
#define JET_LOCKSTEP_SCHEDULE_MAX 64

/**
 * Constant: JET_LOCKSTEP_SNAPSHOT_ALIGN
 * Summary: The alignment, in bytes, of every buffer the session hands your save and load callbacks.
 * Include: jet_lockstep.h
 * Notes: Whatever the alignment of the arena you lend, every snapshot buffer starts on this boundary, so save may cast it to your own state struct and copy fields directly.
 * Example:
 *   static uint32_t save_game(void* user, void* dst, uint32_t cap) { *(Game*)dst = *(const Game*)user; return sizeof(Game); }
 */
#define JET_LOCKSTEP_SNAPSHOT_ALIGN 16

/**
 * Constant: JET_LOCKSTEP_FRAME_NONE
 * Summary: The frame number that means "no frame": JetLockstepStats.desync_frame while the games agree.
 * Include: jet_lockstep.h
 * Notes: The largest uint32_t, which no session reaches: at 120 frames a second it is more than a year away.
 * Example:
 *   if (st.desync_frame != JET_LOCKSTEP_FRAME_NONE) show_desync(st.desync_frame);
 */
#define JET_LOCKSTEP_FRAME_NONE UINT32_MAX

// ============================================================================
// HANDLES, CALLBACKS AND THE CONFIGURATION
// ============================================================================

/**
 * Struct: JetLockstep
 * Summary: An opaque handle to one session, carrying a generation-stamped pool id.
 * Fields:
 *   - id: The packed slot and generation. Treat it as opaque; test it with jet_lockstep_valid.
 * Include: jet_lockstep.h
 * Notes: Copy it freely — it is one integer. A zero-initialised handle (`static JetLockstep s_session;`) is safely invalid, and a handle whose session was closed stays stale for good: every call refuses it rather than touching a newer session in the same place.
 * Example:
 *   static JetLockstep s_session;   // invalid until jet_lockstep_open succeeds
 */
typedef struct {
    int32_t id;
} JetLockstep;

/**
 * Callback: JetLockstepStepFn
 * Summary: Your function that advances the game by exactly one frame.
 * Parameters:
 *   - user: The pointer you put in JetLockstepConfig.user, passed through unchanged.
 *   - inputs: players × input_bytes bytes, player-major: player p's input for this frame begins at p * input_bytes. An inactive player's slice is all zero. Valid only during this call.
 *   - frame: The frame being stepped. Frames arrive in order, 0, 1, 2…, except that after a load the frames since the saved one come again.
 *   - replaying: true when this frame is being stepped again after a wrong guess. Suppress sounds and anything else that must happen once; change the state exactly as you otherwise would.
 * Include: jet_lockstep.h
 * Notes: The one place the game's state may change. It may call jet_lockstep_set_active (for a frame after this one) and jet_lockstep_stats; any other call on the same session from in here is refused with JET_LOCKSTEP_E_ARG, and jet_lockstep_close waits until the advance returns.
 * Example:
 *   static void game_step(void* user, const void* inputs, uint32_t frame, bool replaying) {
 *       Game* g = (Game*)user;
 *       const PadInput* pads = (const PadInput*)inputs;
 *       for (int p = 0; p < PLAYERS; p++) ship_steer(&g->ships[p], pads[p]);
 *       if (world_tick(g) == HIT && !replaying) jet_sfx_play(SFX_BANG);
 *       (void)frame;
 *   }
 */
typedef void (*JetLockstepStepFn)(void* user, const void* inputs, uint32_t frame, bool replaying);

/**
 * Callback: JetLockstepSaveFn
 * Summary: Your function that copies the whole game state into a buffer the session keeps, so a wrong guess can be undone.
 * Parameters:
 *   - user: The pointer you put in JetLockstepConfig.user.
 *   - dst: Where to write, JetLockstepConfig.snapshot_bytes long and aligned to JET_LOCKSTEP_SNAPSHOT_ALIGN.
 *   - cap: The buffer's size: your snapshot_bytes.
 * Returns: The bytes written, at most cap. jet_lockstep_load hands the same count back.
 * Include: jet_lockstep.h
 * Notes: Required when prediction is above 0, and never called otherwise. It is called only before a frame the session is stepping on a guess, so a game on a good link saves rarely. Everything step reads must be saved — a value left out survives a rollback and parts the games.
 * Example:
 *   static uint32_t game_save(void* user, void* dst, uint32_t cap) {
 *       memcpy(dst, user, sizeof(Game));
 *       (void)cap;
 *       return sizeof(Game);
 *   }
 */
typedef uint32_t (*JetLockstepSaveFn)(void* user, void* dst, uint32_t cap);

/**
 * Callback: JetLockstepLoadFn
 * Summary: Your function that puts back a state your save callback wrote.
 * Parameters:
 *   - user: The pointer you put in JetLockstepConfig.user.
 *   - src: A buffer save filled earlier, aligned to JET_LOCKSTEP_SNAPSHOT_ALIGN.
 *   - len: The byte count save returned for it.
 * Include: jet_lockstep.h
 * Notes: Required when prediction is above 0. Called at the start of jet_lockstep_advance when a guess proved wrong, immediately before the frames since that save are stepped again with replaying == true.
 * Example:
 *   static void game_load(void* user, const void* src, uint32_t len) {
 *       memcpy(user, src, len);
 *   }
 */
typedef void (*JetLockstepLoadFn)(void* user, const void* src, uint32_t len);

/**
 * Callback: JetLockstepDigestFn
 * Summary: Your function that boils the whole game state down to one number, so two machines can prove they still agree.
 * Parameters:
 *   - user: The pointer you put in JetLockstepConfig.user.
 * Returns: A 64-bit fingerprint of the state; equal states must give equal values.
 * Include: jet_lockstep.h
 * Notes: Optional — leave it NULL and the session never checks. Called right after stepping every frame that is a multiple of digest_interval. Hash the fields one by one rather than the raw bytes of a struct, because padding bytes are not part of the state and may differ.
 * Example:
 *   static uint64_t game_digest(void* user) {
 *       const Game* g = (const Game*)user;
 *       return ((uint64_t)(uint32_t)g->score << 32) ^ (uint32_t)(g->ship_x * 31 + g->ship_y);
 *   }
 */
typedef uint64_t (*JetLockstepDigestFn)(void* user);

/**
 * Callback: JetLockstepConfirmedFn
 * Summary: Your function that hears each frame once it can never change again.
 * Parameters:
 *   - user: The pointer you put in JetLockstepConfig.user.
 *   - inputs: The inputs that frame was finally stepped with, in the same layout step receives. Valid only during this call.
 *   - frame: The confirmed frame.
 * Include: jet_lockstep.h
 * Notes: Optional. Called exactly once per frame, in frame order. Write these inputs to a journal and the seed plus the journal replays the session exactly; do anything here that must happen once per real frame. From here it may call jet_lockstep_stats and jet_lockstep_valid, and a jet_lockstep_close waits until the advance returns; every other call on this session is refused.
 * Example:
 *   static void game_confirmed(void* user, const void* inputs, uint32_t frame) {
 *       journal_append(frame, inputs, PLAYERS * sizeof(PadInput));
 *       (void)user;
 *   }
 */
typedef void (*JetLockstepConfirmedFn)(void* user, const void* inputs, uint32_t frame);

/**
 * Struct: JetLockstepCallbacks
 * Summary: The five functions a session calls into your game.
 * Fields:
 *   - step: Advances the game by one frame. Required.
 *   - save: Copies the game state out. Required when prediction is above 0; NULL otherwise.
 *   - load: Copies a saved state back. Required when prediction is above 0; NULL otherwise.
 *   - digest: Fingerprints the state for the desync check. Optional: NULL never checks.
 *   - confirmed: Hears each frame once it is final. Optional.
 * Include: jet_lockstep.h
 * Notes: Every callback receives JetLockstepConfig.user first, so one set of functions can serve several sessions, as two seats on one machine do.
 * Example:
 *   const JetLockstepCallbacks callbacks = { .step = game_step, .digest = game_digest };
 */
typedef struct {
    JetLockstepStepFn      step;
    JetLockstepSaveFn      save;
    JetLockstepLoadFn      load;
    JetLockstepDigestFn    digest;
    JetLockstepConfirmedFn confirmed;
} JetLockstepCallbacks;

/**
 * Struct: JetLockstepConfig
 * Summary: Everything a session needs to know before it opens.
 * Fields:
 *   - players: Seats in the session, 1 to JET_LOCKSTEP_MAX_PLAYERS, spectators included. The same on every machine.
 *   - local_player: This machine's seat, 0 to players - 1. Different on every machine.
 *   - input_bytes: Bytes of input per player per frame, 1 to JET_LOCKSTEP_INPUT_MAX. The same on every machine.
 *   - input_delay: How many frames late this machine's own input takes effect, 0 to JET_LOCKSTEP_DELAY_MAX. Each machine chooses its own; change it later with jet_lockstep_set_input_delay.
 *   - prediction: How many frames this machine may run ahead on guessed input, 0 to JET_LOCKSTEP_PREDICTION_MAX. 0 is plain lockstep; above 0 is rollback and needs save and load.
 *   - digest_interval: Frames between desync checks: 0 never checks, and otherwise at least JET_LOCKSTEP_DIGEST_INTERVAL_MIN. The same on every machine.
 *   - session_id: Any number every machine agrees on. Packets carrying another are dropped, so a stray packet from an earlier game cannot leak into this one.
 *   - snapshot_bytes: The most your save callback ever writes. Required when prediction is above 0; ignored when it is 0.
 *   - active_mask: Bit p set means player p is active from frame 0. 0 means every player. The same on every machine.
 *   - callbacks: Your step, save, load, digest and confirmed functions.
 *   - user: Passed unchanged as the first argument of every callback — usually your game state.
 *   - arena: The memory the session keeps everything in: any memory, at least jet_lockstep_arena_bytes long, untouched by you until jet_lockstep_close.
 *   - arena_bytes: The arena's size.
 * Include: jet_lockstep.h
 * Notes: Use designated initialisers and leave what you do not need at zero. The session copies what it needs at jet_lockstep_open, so the config itself may be a local variable.
 * Example:
 *   const JetLockstepConfig cfg = {
 *       .players = 2, .local_player = me, .input_bytes = sizeof(PadInput), .input_delay = 3,
 *       .digest_interval = 60, .session_id = room_number,
 *       .callbacks = { .step = game_step, .digest = game_digest },
 *       .user = &s_game, .arena = s_arena, .arena_bytes = sizeof s_arena,
 *   };
 */
typedef struct {
    uint8_t              players;
    uint8_t              local_player;
    uint8_t              input_bytes;
    uint8_t              input_delay;
    uint8_t              prediction;
    uint16_t             digest_interval;
    uint32_t             session_id;
    uint32_t             snapshot_bytes;
    uint32_t             active_mask;
    JetLockstepCallbacks callbacks;
    void*                user;
    void*                arena;
    uint32_t             arena_bytes;
} JetLockstepConfig;

/**
 * Enum: JetLockstepResult
 * Summary: What a slot did, or why a call was refused.
 * Include: jet_lockstep.h
 * Notes: jet_lockstep_advance answers with the first three as well as the errors. WAITING and SYNCING mean the same thing to a cart — nothing was stepped this slot, call again next slot — and are told apart only so a cart can count them. The errors are negative, so `r < 0` tests for any of them. E_DESYNC and E_LEFT_BEHIND are sticky: the session steps nothing more, and the cart ends the game.
 * Example:
 *   switch (jet_lockstep_advance(s)) {
 *       case JET_LOCKSTEP_STEPPED:       break;
 *       case JET_LOCKSTEP_WAITING:
 *       case JET_LOCKSTEP_SYNCING:       s_waits++; break;
 *       case JET_LOCKSTEP_E_DESYNC:      show_desync(); break;
 *       case JET_LOCKSTEP_E_LEFT_BEHIND: show("TOO FAR BEHIND TO FOLLOW"); break;
 *       default:                         break;
 *   }
 */
typedef enum {
    JET_LOCKSTEP_STEPPED       = 1,  // one frame stepped (after any rollback replay)
    JET_LOCKSTEP_WAITING       = 0,  // an input is late: nothing stepped, try next slot
    JET_LOCKSTEP_SYNCING       = 2,  // this machine is ahead: a slot deliberately skipped
    JET_LOCKSTEP_E_ARG         = -1, // stale handle, bad argument, or a call from inside a callback
    JET_LOCKSTEP_E_DESYNC      = -2, // the games parted: sticky, see jet_lockstep_stats
    JET_LOCKSTEP_E_FULL        = -3, // a needed peer has stopped acknowledging, or the schedule is full
    JET_LOCKSTEP_E_LEFT_BEHIND = -4, // an input this machine needs is gone from its maker: sticky
} JetLockstepResult;

/**
 * Struct: JetLockstepStats
 * Summary: Where a session has got to, and how its link is doing.
 * Fields:
 *   - frame: The next frame to step, which is also how many frames have been stepped.
 *   - confirmed_frame: Every frame below this is confirmed and will never change. It never decreases.
 *   - input_frame: The next frame this machine's own input is for.
 *   - desync_frame: The first frame whose digests differed between machines — found here, or told by the peer that found it — or JET_LOCKSTEP_FRAME_NONE.
 *   - rollbacks: How many times a wrong guess was undone.
 *   - rollback_frames: How many frames were stepped again in all, across every rollback.
 *   - stalls: Slots that answered JET_LOCKSTEP_WAITING.
 *   - syncs: Slots that answered JET_LOCKSTEP_SYNCING.
 *   - packets_accepted: Packets jet_lockstep_receive took.
 *   - packets_dropped: Packets jet_lockstep_receive refused as malformed, foreign or impossible.
 *   - digests_checked: Digest comparisons made with peers — each one a proof the games still agreed, or the one that found they did not.
 *   - frames_ahead: How many frames this machine runs ahead of its slowest active peer, smoothed, in whole frames; negative when it runs behind, and 0 while it waits — a machine that is waiting runs ahead of no one.
 *   - rtt_slots: The smoothed round trip to each player, in slots; 0 for this machine's own seat and until the first measurement.
 *   - silence_slots: Slots since each player's last accepted packet; 0 for this machine's own seat.
 * Include: jet_lockstep.h
 * Notes: A snapshot, filled by jet_lockstep_stats. A peer whose silence_slots keeps climbing has gone — the cart decides how long to wait before saying so; a session that answers JET_LOCKSTEP_WAITING all the while is waiting for exactly that peer.
 * Example:
 *   JetLockstepStats st;
 *   if (jet_lockstep_stats(s, &st) > 0 && st.silence_slots[peer] > 5 * 120) show("CONNECTION LOST");
 */
typedef struct {
    uint32_t frame;
    uint32_t confirmed_frame;
    uint32_t input_frame;
    uint32_t desync_frame;
    uint32_t rollbacks;
    uint32_t rollback_frames;
    uint32_t stalls;
    uint32_t syncs;
    uint32_t packets_accepted;
    uint32_t packets_dropped;
    uint32_t digests_checked;
    int32_t  frames_ahead;
    uint16_t rtt_slots[JET_LOCKSTEP_MAX_PLAYERS];
    uint16_t silence_slots[JET_LOCKSTEP_MAX_PLAYERS];
} JetLockstepStats;

// ============================================================================
// OPENING AND CLOSING
// ============================================================================

/**
 * Function: jet_lockstep_arena_bytes
 * Summary: How much memory a session with this configuration needs to be lent.
 * Parameters:
 *   - cfg: The configuration you mean to open with. Its arena and arena_bytes are not read.
 * Returns: The arena size in bytes, or 0 when the configuration is invalid — a count out of range, a missing step callback, a prediction without save, load and snapshot_bytes, a digest_interval below JET_LOCKSTEP_DIGEST_INTERVAL_MIN, or an active_mask naming a seat that does not exist.
 * Include: jet_lockstep.h
 * Notes: The answer allows for any arena alignment, so a plain `static uint8_t arena[N]` works. It grows with players, input_bytes and — when prediction is above 0 — prediction + 1 copies of snapshot_bytes. A 0 here is also the quickest way to find out which configurations are valid before you open one.
 * Example:
 *   const uint32_t need = jet_lockstep_arena_bytes(&cfg);
 *   if (need == 0 || need > sizeof s_arena) LOG_WARN("lockstep config needs %u bytes", (unsigned)need);
 */
uint32_t jet_lockstep_arena_bytes(const JetLockstepConfig* cfg);

/**
 * Function: jet_lockstep_open
 * Summary: Starts a session at frame 0 in the memory you lend it.
 * Parameters:
 *   - cfg: The configuration. It is copied, so it may be a local.
 * Returns: A live handle, or an invalid one (see jet_lockstep_valid) when the configuration is invalid, the arena is NULL or smaller than jet_lockstep_arena_bytes, the arena overlaps another open session's, or JET_LOCKSTEP_MAX_SESSIONS sessions are already open.
 * Include: jet_lockstep.h
 * Notes: Every machine opens from the same game state, and the session's first input_delay frames carry an all-zero input from this machine, so the game can step them before any hand has been read. THE ARENA BELONGS TO THE SESSION until jet_lockstep_close: the session clears it now and keeps everything in it.
 * Example:
 *   s_session = jet_lockstep_open(&cfg);
 *   if (!jet_lockstep_valid(s_session)) show("COULD NOT START THE SESSION");
 */
JetLockstep jet_lockstep_open(const JetLockstepConfig* cfg);

/**
 * Function: jet_lockstep_valid
 * Summary: Reports whether a handle names a session that is open now.
 * Parameters:
 *   - s: The handle to test.
 * Returns: true while the session is open; false for a zero-initialised handle, a refused open, or a closed session.
 * Include: jet_lockstep.h
 * Notes: Unlike a shape check, this asks the pool, so it turns false the moment the session closes.
 * Example:
 *   if (!jet_lockstep_valid(s_session)) return;   // not in a game
 */
bool jet_lockstep_valid(JetLockstep s);

/**
 * Function: jet_lockstep_close
 * Summary: Ends a session and gives its arena back.
 * Parameters:
 *   - s: The session; a stale or invalid handle is ignored.
 * Returns: Nothing.
 * Include: jet_lockstep.h
 * Notes: Idempotent, so it is safe on every exit path. The handle is stale afterwards. Called from inside one of the session's own callbacks it takes effect when the jet_lockstep_advance that made the call returns, so the session never vanishes beneath its own step. Sending a goodbye to peers is the cart's business; to them this machine simply falls silent.
 * Example:
 *   void _end(void) { jet_lockstep_close(s_session); }
 */
void jet_lockstep_close(JetLockstep s);

// ============================================================================
// THE SLOT
// ============================================================================

/**
 * Function: jet_lockstep_add_local_input
 * Summary: Hands the session this machine's input for the frame input_delay frames ahead.
 * Parameters:
 *   - s: The session.
 *   - input: input_bytes bytes of this machine's input. Copied at once.
 * Returns: 1 when the input was taken; 0 when the session has no room for it — it is waiting, or a lowered input_delay has not yet caught up — so the input would be stale before it was used and is not kept; JET_LOCKSTEP_E_FULL when a peer the game still needs has stopped acknowledging and the history is full; JET_LOCKSTEP_E_ARG for a stale handle, a NULL input, or a call from inside a callback.
 * Include: jet_lockstep.h
 * Notes: Call it exactly once per slot, before jet_lockstep_advance, and ignore a 0 — the session never needs you to know whether it is waiting. A taken input rides the very next packet. Nothing a needed peer lacks is ever overwritten: E_FULL is what happens instead, and the frames it missed are filled in once that peer catches up. It can happen only while this machine is not needed itself. A peer you are waiting for that falls silent shows instead as JET_LOCKSTEP_WAITING from every slot and a climbing silence_slots, and a peer nobody needs is left behind rather than allowed to fill the history.
 * Example:
 *   const PadInput pad = read_pad();
 *   if (jet_lockstep_add_local_input(s_session, &pad) == JET_LOCKSTEP_E_FULL) s_full_slots++;   // a player lags
 */
int jet_lockstep_add_local_input(JetLockstep s, const void* input);

/**
 * Function: jet_lockstep_advance
 * Summary: Runs one slot: undoes any wrong guess, confirms what can be confirmed, checks digests, and steps the next frame if it may.
 * Parameters:
 *   - s: The session.
 * Returns: JET_LOCKSTEP_STEPPED when one frame was stepped; JET_LOCKSTEP_WAITING when an input it needs is late; JET_LOCKSTEP_SYNCING when this machine is ahead and skipped a slot on purpose; JET_LOCKSTEP_E_DESYNC once the games have parted (from then on, every slot); JET_LOCKSTEP_E_LEFT_BEHIND once an input this machine needs is gone from the peer that made it (from then on, every slot); JET_LOCKSTEP_E_ARG for a stale handle or a call from inside a callback.
 * Include: jet_lockstep.h
 * Notes: Steps at most one new frame per slot, but a rollback can replay up to prediction frames first, all inside this call. Every machine runs the slot in the same fixed order — replay, confirm, compare digests, time sync, step — and every advance marks each peer due for a packet, so poll afterwards. Once desynchronised the session steps nothing more but keeps exchanging packets, and tells every peer the frame that parted it, so each of them reports the same parting. Once left behind it steps nothing more and falls silent, so a peer that waits for it sees it vanish.
 * Example:
 *   jet_lockstep_add_local_input(s_session, &pad);
 *   if (jet_lockstep_advance(s_session) == JET_LOCKSTEP_E_DESYNC) show("THE GAMES PARTED");
 */
int jet_lockstep_advance(JetLockstep s);

/**
 * Function: jet_lockstep_poll_packet
 * Summary: Takes the next packet this machine owes a peer, for you to send.
 * Parameters:
 *   - s: The session.
 *   - out: Where to write the packet.
 *   - cap: The size of out: at least JET_LOCKSTEP_PACKET_BYTES(input_bytes).
 *   - out_to: Filled in with the player the packet is for. Pass NULL only with two players, where it is always the other one.
 * Returns: The packet's length, when one was due; 0 when every peer has its packet for this slot, or the session has been left behind; JET_LOCKSTEP_E_ARG for a stale handle, a NULL or too-small buffer, or a call from inside a callback.
 * Include: jet_lockstep.h
 * Notes: Call it in a loop after jet_lockstep_advance until it returns 0: one packet per peer per slot. Send each one unreliably and unordered if your transport allows — a datagram, a WebRTC unordered channel — because the packets themselves re-send whatever was lost, and a reliable transport only adds the delay of its own retries. A packet you never send costs nothing but time: the next carries the same inputs.
 * Example:
 *   uint8_t packet[JET_LOCKSTEP_PACKET_MAX];
 *   uint8_t to;
 *   for (int n; (n = jet_lockstep_poll_packet(s_session, packet, sizeof packet, &to)) > 0; ) {
 *       transport_send(to, packet, (uint32_t)n);
 *   }
 */
int jet_lockstep_poll_packet(JetLockstep s, void* out, uint32_t cap, uint8_t* out_to);

/**
 * Function: jet_lockstep_receive
 * Summary: Hands the session a packet that arrived from a peer.
 * Parameters:
 *   - s: The session.
 *   - from: The player your transport says sent it.
 *   - packet: The bytes, exactly as they arrived.
 *   - len: Their length.
 * Returns: 1 when the packet was taken; 0 when it was dropped as malformed, foreign (another session id), impossible, or not from the player it claims; JET_LOCKSTEP_E_ARG for a stale handle, a NULL packet with a length, or a call from inside a callback.
 * Include: jet_lockstep.h
 * Notes: Call it whenever a packet arrives — before or after advance, as many times a slot as there are packets. Duplicated, reordered and late packets are harmless, because storing an input that is already known is a comparison, not a write; inputs beyond a gap wait for the sender's next sweep, and their packet is still taken. Everything is checked before anything is believed: a packet the wire garbled fails its checksum, a packet no honest peer could send is refused, and neither can crash the session or reach past its arena; each one dropped is counted in packets_dropped. A packet whose inputs disagree with inputs already received is dropped too: an input, once sent, never changes. What no check can stop is a peer in the session that lies — it can stall the game by falling silent, or part it by sending other inputs to other peers — and the digests report such a parting.
 * Example:
 *   void on_transport_packet(uint8_t from, const void* bytes, uint32_t len) {
 *       jet_lockstep_receive(s_session, from, bytes, len);
 *   }
 */
int jet_lockstep_receive(JetLockstep s, uint8_t from, const void* packet, uint32_t len);

// ============================================================================
// TURNS, DELAY AND STATS
// ============================================================================

/**
 * Function: jet_lockstep_set_active
 * Summary: Makes one player active or inactive from a given frame on — how turns pass and spectators are made.
 * Parameters:
 *   - s: The session.
 *   - player: The seat to change, 0 to players - 1.
 *   - active: true when the simulation needs that player's input; false when it reads as zero and nobody waits for it.
 *   - from: The first frame the change applies to. Inside step it must be later than the frame being stepped; outside, the call is allowed only before the first frame has been stepped.
 * Returns: 1 when recorded (or when it already held, which records nothing); JET_LOCKSTEP_E_ARG for a stale handle, a bad seat, a from that is too early, or a call from outside step once the session has begun; JET_LOCKSTEP_E_FULL when JET_LOCKSTEP_SCHEDULE_MAX changes are already in flight — an answer every machine gives alike, so game logic may act on it.
 * Include: jet_lockstep.h
 * Notes: EVERY MACHINE MUST MAKE THE SAME CALL WITH THE SAME from. That is automatic when the call is game logic inside step — the natural place is the frame a turn ends — because every machine runs the same game. A change made inside a step that is later rolled back is rolled back with it, and the replay makes it again if it still happens, so the schedule is part of the game state. For the frames on and after from, a later call overrides an earlier one. An inactive player still sends packets and inputs; only the waiting stops.
 * Example:
 *   if (turn_over(g)) {
 *       jet_lockstep_set_active(s_session, g->owner, false, frame + 1);
 *       g->owner = (uint8_t)(1 - g->owner);
 *       jet_lockstep_set_active(s_session, g->owner, true, frame + 1);
 *   }
 */
int jet_lockstep_set_active(JetLockstep s, uint8_t player, bool active, uint32_t from);

/**
 * Function: jet_lockstep_set_input_delay
 * Summary: Changes how many frames late this machine's own input takes effect.
 * Parameters:
 *   - s: The session.
 *   - frames: The new delay, 0 to JET_LOCKSTEP_DELAY_MAX.
 * Returns: 1 when applied; JET_LOCKSTEP_E_ARG for a stale handle, a delay out of range, or a call from inside a callback.
 * Include: jet_lockstep.h
 * Notes: A machine's own choice, never game logic — peers need not know. Raising it makes the next input repeat the last one across the gap, so no frame is left without an input; lowering it makes jet_lockstep_add_local_input answer 0 for a few slots until the delay is honest again. Either way no input already sent is overwritten. A good delay is about half the round trip: rtt_slots / 2.
 * Example:
 *   if (jet_lockstep_stats(s_session, &st) > 0) jet_lockstep_set_input_delay(s_session, (uint8_t)(st.rtt_slots[peer] / 2));
 */
int jet_lockstep_set_input_delay(JetLockstep s, uint8_t frames);

/**
 * Function: jet_lockstep_stats
 * Summary: Fills in where the session has got to and how its link is doing.
 * Parameters:
 *   - s: The session.
 *   - out: The JetLockstepStats to fill.
 * Returns: 1 when filled; JET_LOCKSTEP_E_ARG for a stale handle or a NULL out.
 * Include: jet_lockstep.h
 * Notes: Cheap enough for every frame, and allowed from inside any callback. desync_frame is the frame to report when the games part: the first digest frame after whatever made them differ, the same on every machine.
 * Example:
 *   JetLockstepStats st;
 *   char line[32];
 *   if (jet_lockstep_stats(s_session, &st) > 0) {
 *       snprintf(line, sizeof line, "PING %u", (unsigned)st.rtt_slots[peer]);
 *       jet_print(line, 4, 4, 7);
 *   }
 */
int jet_lockstep_stats(JetLockstep s, JetLockstepStats* out);

#endif // JET_LOCKSTEP_H
