// runtime/jet_ws.h
#ifndef JET_WS_H
#define JET_WS_H

// ============================================================================
// Jet64 WebSocket — the universal transport (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_net.h and jet_http.h it is deliberately kept out of jet64.h, so it
// costs nothing unless a cart asks for it: the cart resolver compiles jet_ws.c
// only when a cart's #include tree reaches this header, and the exporter keys the
// player's WebSocket stack off the very same signal. A cart that never includes
// it contains no WebSocket code at all — and its exported player never compiles
// `tungstenite` either.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_ws.h"      // <-- explicit opt-in
//
// A WebSocket is a non-blocking, bidirectional, whole-message connection that
// works in the workstation, native player and web player. Use a wss:// relay
// for browser-compatible multiplayer, chat, lobbies or bridged services.
//
// <nocodex>
// WHY THIS MODULE MATTERS MOST. It is the only *stream* that reaches the browser.
// Raw TCP is a desktop-native privilege (jet_net.h) and HTTP is request/response
// (jet_http.h); a WebSocket is a long-lived, bidirectional, message-at-a-time
// link that works identically on the workstation, on a downloaded native build,
// and on an itch.io page. That makes it the recommended transport for internet
// multiplayer, for a chat or lobby relay, and for a BBS reached through a
// telnet→WebSocket bridge. See docs/networking/design.md §5.4 and §9.3.
// </nocodex>
//
// ----------------------------------------------------------------------------
// THE FOUR RULES
// ----------------------------------------------------------------------------
// 1. NOTHING BLOCKS. jet_ws_connect returns on the tick it was called; the
//    handshake happens over the following ones. Poll jet_ws_state from _update
//    and start sending when it reads JET_TCP_OPEN. The 120 Hz heartbeat is never
//    held up by a network that may take seconds.
//
// 2. MESSAGES, NEVER FRAGMENTS. One jet_ws_send is one message on the wire; one
//    jet_ws_recv is one whole message or nothing. That is the browser's own
//    contract and the right level for a game protocol — no framing to reinvent,
//    no half-messages to reassemble. Ping/pong keepalive is answered host-side,
//    invisibly.
//
// 3. THE ARENA IS THE SOCKET'S. Reserve it in WRAM with jet_memory_reserve,
//    then leave it untouched until jet_ws_close reports JET_NET_OK. A static
//    array is not visible to the host and cannot be used as the arena.
//
// <nocodex>
// 3. THE ARENA IS THE SOCKET'S. You hand jet_ws_connect a block of WRAM and the
//    runtime splits it into the transmit and receive rings that carry your
//    messages (see "SIZING THE ARENA" below). It belongs to the socket until
//    jet_ws_close reports JET_NET_OK — the host writes received bytes into it
//    directly, which is the jet_pcm rule generalised, and is exactly why it must
//    be WRAM rather than a `static uint8_t buf[N]` the host cannot see.
// </nocodex>
//
// 4. HANDLES HAVE LIFETIMES. A zero-initialised JetWs is safely invalid. After
//    closing one, stop using it; a stale handle reports JET_NET_E_ARG instead of
//    referring to a newer connection.
//
// <nocodex>
// 4. HANDLES CANNOT DANGLE. JetWs is a generation-stamped id over a fixed pool
//    (the jet_pool idiom), so a handle used after close fails with JET_NET_E_ARG
//    and can never alias a recycled slot. A zero-initialised handle
//    (`static JetWs s_link;`) is safely invalid, and there is no malloc anywhere.
// </nocodex>
//
// <nocodex>
// ----------------------------------------------------------------------------
// WHY THE HOST DOES THE WORK
// ----------------------------------------------------------------------------
// `wss://` means TLS, and Jet64 will not vendor a C TLS stack into the sandbox
// when every host is Rust and one audited dependency away. So a socket is
// *described* here and *driven* by the host: the workstation and the native
// player run one shared Rust client, and the web player hands the same
// description to the browser's own WebSocket. One TLS stack, owned by the host,
// everywhere. The cart's own data path is a pair of lock-free rings in its
// arena — the same single-writer discipline the keyboard and oscilloscope taps
// already use — so the bytes cross with no copy the design does not need and no
// lock at all. See docs/networking/design.md §3 and §7.
// </nocodex>
//
// ----------------------------------------------------------------------------
// TWO BROWSER TRUTHS, STATED PLAINLY
// ----------------------------------------------------------------------------
// * MIXED CONTENT. An itch.io page is `https://`, so only `wss://` works there;
//   a plain `ws://` URL is blocked outright by the browser. Ship `wss://`.
// * NO SERVER SIDE. A browser cannot listen, so there is no jet_ws_serve and
//   never will be. A cart that wants to *host* something reaches for
//   jet_http_serve_* (native only) or a relay both players connect out to.
//
// ----------------------------------------------------------------------------
// SIZING THE ARENA
// ----------------------------------------------------------------------------
// The runtime splits the arena evenly between outgoing and incoming messages.
// Use a power-of-two size so none is wasted: 16 KiB provides 8 KiB in each
// direction, while 64 KiB provides 32 KiB. The largest message is slightly
// smaller than one half; call jet_ws_message_max after connecting for the exact
// limit, and size the arena for the largest message your protocol will carry.
//
// <nocodex>
// The runtime halves your arena into a transmit ring and a receive ring, each
// rounded DOWN to a power of two — that is what lets a ring position be a mask
// instead of a division and makes the byte counters wrap harmlessly. So pass a
// power-of-two arena and nothing is wasted: 16 KiB gives two 8 KiB rings, 64 KiB
// gives two 32 KiB. The largest message a socket can carry in either direction is
// one ring minus the frame header (jet_ws_message_max reports it), so size the
// arena for the biggest message YOUR protocol sends — a message larger than the
// receive ring can never be delivered, and the runtime says so by failing the
// link rather than by dropping it in silence.
// </nocodex>
//
// Complete WebSocket messaging example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_ws.h"
//
//   enum { ARENA_BYTES = 16 * 1024, MESSAGE_CAPACITY = 64 };
//   static JetWs link;
//   static uint8_t* arena;
//   static char last_message[MESSAGE_CAPACITY] = "CONNECTING...";
//   static char link_status[MESSAGE_CAPACITY] = "STARTING";
//   static bool greeting_sent;
//
//   void _init(void) {
//       arena = (uint8_t*)jet_memory_reserve(ARENA_BYTES);
//       if (arena == NULL) {
//           snprintf(link_status, sizeof link_status, "NO WRAM");
//           return;
//       }
//       // Replace this placeholder with the wss:// URL of your own echo/relay service.
//       link = jet_ws_connect("wss://echo.example.com/", arena, ARENA_BYTES);
//       if (!jet_ws_valid(link)) snprintf(link_status, sizeof link_status, "SOCKET NOT CREATED");
//   }
//
//   void _update(float dt) {
//       static const char greeting[] = "hello";
//       (void)dt;
//       if (!jet_ws_valid(link)) return;
//       const int state = jet_ws_state(link);
//       if (state == JET_TCP_CONNECTING) {
//           snprintf(link_status, sizeof link_status, "CONNECTING...");
//           return;
//       }
//       if (state == JET_TCP_FAILED || state == JET_TCP_CLOSED) {
//           snprintf(link_status, sizeof link_status, "%s (%d)",
//                    state == JET_TCP_FAILED ? "FAILED" : "CLOSED", jet_ws_error(link));
//           return;
//       }
//       if (state != JET_TCP_OPEN) {
//           snprintf(link_status, sizeof link_status, "INVALID STATE (%d)", state);
//           return;
//       }
//       snprintf(link_status, sizeof link_status, "CONNECTED");
//       if (!greeting_sent && jet_ws_send(link, greeting, sizeof greeting - 1, JET_WS_TEXT) == JET_NET_OK) {
//           greeting_sent = true;
//       }
//       const int received = jet_ws_recv(link, last_message, MESSAGE_CAPACITY - 1, NULL);
//       if (received > 0) last_message[received] = '\0';
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(link_status, 16, 16, 15);
//       jet_print(last_message, 16, 36, 15);
//   }
//
//   void _end(void) {
//       jet_ws_shutdown();
//   }

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memcpy (sandbox-safe)
#include "jet_net.h"  // the shared JetNetResult vocabulary, JetTcpState, handle discipline

// ============================================================================
// LIMITS — every bound named once
// ============================================================================

/**
 * Constant: JET_WS_MAX_SOCKETS
 * Summary: Maximum simultaneous WebSocket handles owned by one cart.
 * Include: jet_ws.h
 * Notes: Each live or closing socket occupies one fixed NET command slot. jet_ws_connect returns an invalid handle while all slots are occupied; finish closing a socket before reusing its slot or arena.
 * Example:
 *   JetWs links[JET_WS_MAX_SOCKETS] = {0};
 */
#define JET_WS_MAX_SOCKETS JET_NET_WS_SLOTS

/**
 * Constant: JET_WS_URL_MAX
 * Summary: Maximum WebSocket URL buffer size, including its terminating NUL.
 * Include: jet_ws.h
 * Notes: The URL is copied into a fixed command staging slot. jet_ws_connect rejects a longer URL with an invalid handle rather than truncating it.
 * Example:
 *   char relay_url[JET_WS_URL_MAX] = "wss://relay.example.com/room";
 */
#define JET_WS_URL_MAX JET_NET_CMD_WS_STAGE_URL_MAX

/**
 * Constant: JET_WS_RING_MIN
 * Summary: Smallest transmit or receive ring created for a WebSocket, in bytes.
 * Include: jet_ws.h
 * Notes: Each socket arena is split into two rings of at least this size. The maximum message payload is smaller by JET_WS_FRAME_OVERHEAD.
 * Example:
 *   uint32_t minimum_message_storage = JET_WS_RING_MIN;
 */
#define JET_WS_RING_MIN (8 * 1024)

/**
 * Constant: JET_WS_ARENA_MIN
 * Summary: Smallest WRAM arena accepted by jet_ws_connect.
 * Include: jet_ws.h
 * Notes: Provides one JET_WS_RING_MIN transmit ring and one receive ring. Reserve a larger power-of-two arena when the protocol sends larger messages or needs more queued backpressure capacity.
 * Example:
 *   uint8_t* arena = (uint8_t*)jet_memory_reserve(JET_WS_ARENA_MIN);
 */
#define JET_WS_ARENA_MIN (2 * JET_WS_RING_MIN)

/**
 * Constant: JET_WS_FRAME_OVERHEAD
 * Summary: Ring bytes consumed by one WebSocket message's length, type, and alignment metadata.
 * Include: jet_ws.h
 * Notes: Subtract this from a ring size to estimate its largest payload, or call jet_ws_message_max on a connected handle to account for arena rounding exactly.
 * Example:
 *   uint32_t minimum_max_message = JET_WS_RING_MIN - JET_WS_FRAME_OVERHEAD;
 */
#define JET_WS_FRAME_OVERHEAD JET_NET_WS_FRAME_HEADER_BYTES

// ============================================================================
// HANDLES AND MESSAGE TYPES
// ============================================================================

/**
 * Struct: JetWs
 * Summary: An opaque handle to one WebSocket connection, carrying a generation-stamped pool id.
 * Fields:
 *   - id: The packed slot + generation id. Treat it as opaque; test it with jet_ws_valid.
 * Include: jet_ws.h
 * Notes: Copy it freely — it is one integer. A handle whose jet_ws_close has completed is permanently stale: every call reports JET_NET_E_ARG rather than touching whatever now occupies the slot. A zero-initialised handle (`static JetWs s_link;`) is safely invalid, so a cart can poll one before it has ever dialled.
 * Example:
 *   JetWs link = jet_ws_connect("wss://relay.example.com/room", arena, ARENA_BYTES);
 */
typedef struct {
    int32_t id;
} JetWs;

/**
 * Enum: JetWsMsgType
 * Summary: Whether a message is UTF-8 text or opaque binary.
 * Include: jet_ws.h
 * Notes: This is a real distinction on the wire, not a hint: browsers hand a TEXT frame to JavaScript as a string and a BINARY frame as bytes, and a relay written for one will often reject the other. Neither value is 0, so a zeroed JetWsMsgType is not silently a valid kind. Jet64 never validates or transcodes your payload — a TEXT message you send must actually be UTF-8, or a strict peer will close the connection on you.
 * Example:
 *   jet_ws_send(link, "ready", 5, JET_WS_TEXT);
 */
typedef enum {
    JET_WS_TEXT   = 1,
    JET_WS_BINARY = 2,
} JetWsMsgType;

/**
 * Function: jet_ws_valid
 * Summary: Reports whether jet_ws_connect handed back a real socket slot.
 * Parameters:
 *   - w: The handle to test.
 * Returns: true when a slot was claimed; false when the pool was full, an argument was refused, or JET_NET_CAP_WS is absent on this runtime.
 * Include: jet_ws.h
 * Notes: A false here means the socket never left the machine, which is worth surfacing differently from a link that failed to connect: a full pool is worth retrying next tick, a rejected URL is not.
 * Example:
 *   s_link = jet_ws_connect(RELAY_URL, s_arena, WS_ARENA_BYTES);
 *   if (!jet_ws_valid(s_link)) status_show("NO FREE SOCKETS");
 */
static inline bool jet_ws_valid(JetWs w) {
    return jet_net_handle_valid(w.id);
}

// ============================================================================
// THE CLIENT
// ============================================================================

/**
 * Function: jet_ws_connect
 * Summary: Starts a WebSocket connection and returns immediately — the handshake happens over the following ticks.
 * Parameters:
 *   - url: Target URL, ws:// or wss://, NUL-terminated and shorter than JET_WS_URL_MAX.
 *   - arena: Scratch memory the socket turns into its two message rings. MUST live in WRAM (jet_memory_reserve).
 *   - arena_bytes: Size of that arena, at least JET_WS_ARENA_MIN.
 * Returns: A handle whose state begins at JET_TCP_CONNECTING, or an invalid handle (see jet_ws_valid) when the pool is full, an argument is malformed, the arena is not in WRAM or overlaps a socket that has not finished with it, or JET_NET_CAP_WS is absent.
 * Include: jet_ws.h
 * Notes: NOTHING BLOCKS HERE. Poll jet_ws_state from _update and send once it reads JET_TCP_OPEN. THE ARENA BELONGS TO THE SOCKET until jet_ws_close reports JET_NET_OK — the host writes arrived messages straight into it, so reusing it before then corrupts whatever you put there. It is halved into a transmit and a receive ring, each rounded down to a power of two, so a power-of-two arena wastes nothing; jet_ws_message_max reports the largest message the result can carry. On an https page only wss:// is permitted — a plain ws:// URL is blocked by the browser, not by us. There is no built-in connect deadline: if a peer never answers, your cart decides when to give up and calls jet_ws_close.
 * Example:
 *   static JetWs s_link;
 *   static uint8_t* s_arena;
 *   void _init(void) { s_arena = jet_memory_reserve(WS_ARENA_BYTES); }
 *   void dial(void)  { s_link = jet_ws_connect("wss://relay.example.com/room/42", s_arena, WS_ARENA_BYTES); }
 */
JetWs jet_ws_connect(const char* url, void* arena, uint32_t arena_bytes);

/**
 * Function: jet_ws_state
 * Summary: Reports where a connection is in its life.
 * Parameters:
 *   - w: The socket to poll.
 * Returns: A JetTcpState value — JET_TCP_CONNECTING, JET_TCP_OPEN, JET_TCP_CLOSED or JET_TCP_FAILED — or JET_NET_E_ARG for a stale, forged, or never-assigned handle.
 * Include: jet_ws.h
 * Notes: The same four states a TCP connection uses, deliberately, so a cart's session layer can drive either transport through one switch — the seam that lets a terminal dial a board over raw TCP on a native host and over a WebSocket bridge in the browser with no second code path. JET_TCP_CLOSED is an orderly close by either side and is not an error; JET_TCP_FAILED means the link never worked or broke, and jet_ws_error says why. Messages already received stay readable after the link ends: drain with jet_ws_recv until it reports JET_NET_E_CLOSED.
 * Example:
 *   switch (jet_ws_state(s_link)) {
 *       case JET_TCP_CONNECTING: status_show("HANDSHAKING..."); break;
 *       case JET_TCP_OPEN:       lobby_pump(s_link);            break;
 *       default:                 status_show("DISCONNECTED");   break;
 *   }
 */
int jet_ws_state(JetWs w);

/**
 * Function: jet_ws_error
 * Summary: Explains why a connection reached JET_TCP_FAILED or JET_TCP_CLOSED.
 * Parameters:
 *   - w: The socket to ask.
 * Returns: The JetNetResult that ended it — typically JET_NET_E_RESOLVE, JET_NET_E_CONNECT, JET_NET_E_DENIED, JET_NET_E_CLOSED or JET_NET_E_LIMIT — or JET_NET_OK while it is still connecting or open, or JET_NET_E_ARG for a stale handle.
 * Include: jet_ws.h
 * Notes: Worth surfacing verbatim: "host not found" and "the handshake was refused" send a user to very different remedies, and on the web a browser reports every network-level failure identically on purpose (so a page cannot probe the network with it), which surfaces here as JET_NET_E_CONNECT rather than a guess dressed up as a diagnosis. JET_NET_E_LIMIT means a peer sent a message larger than your receive ring could ever hold — the cure is a bigger arena, not a retry.
 * Example:
 *   if (jet_ws_state(s_link) == JET_TCP_FAILED) {
 *       status_show(jet_ws_error(s_link) == JET_NET_E_RESOLVE ? "HOST NOT FOUND" : "NO ANSWER");
 *   }
 */
int jet_ws_error(JetWs w);

/**
 * Function: jet_ws_close_code
 * Summary: The RFC 6455 close code the connection ended with.
 * Parameters:
 *   - w: The socket to ask.
 * Returns: The close code (1000 normal, 1001 going away, 1008 policy violation, 1011 server error, ...), or 0 while the link is still live, when it never completed a close handshake, or for a stale handle.
 * Include: jet_ws.h
 * Notes: The peer's own last word, and often the only one you get: a relay that refuses your room, your protocol version, or your rate limit will usually say so here rather than in a message. Meaningful once jet_ws_state reports JET_TCP_CLOSED; a link that died without a handshake (a dropped network, a killed server) reports 0, which is itself the distinction between "they hung up" and "the wire went dead".
 * Example:
 *   if (jet_ws_state(s_link) == JET_TCP_CLOSED && jet_ws_close_code(s_link) == 1008) {
 *       status_show("THE RELAY REFUSED THIS ROOM");
 *   }
 */
int jet_ws_close_code(JetWs w);

/**
 * Function: jet_ws_message_max
 * Summary: The largest single message this socket can carry, in either direction.
 * Parameters:
 *   - w: The socket to ask.
 * Returns: The payload byte count one ring can hold, or 0 for a stale handle.
 * Include: jet_ws.h
 * Notes: Derived from the arena you supplied — one ring (half the arena, rounded down to a power of two) minus JET_WS_FRAME_OVERHEAD — so it answers the question the rounding makes non-obvious. Sending more than this is a programming error and is refused with JET_NET_E_ARG, not JET_NET_E_LIMIT, precisely so a cart can tell "too big to ever fit" from "no room right now". A peer that sends more than this fails the link with JET_NET_E_LIMIT, because delivering half a message would be worse than saying so.
 * Example:
 *   if (state_blob_len > jet_ws_message_max(s_link)) LOG_WARN("arena too small for a full snapshot");
 */
uint32_t jet_ws_message_max(JetWs w);

/**
 * Function: jet_ws_send
 * Summary: Queues one whole message for the peer.
 * Parameters:
 *   - w: An open socket.
 *   - msg: The payload bytes. May be anywhere — the cart copies them into its own ring, so this does NOT have to be WRAM.
 *   - len: Payload length in bytes; 0 sends an empty message, which is legal.
 *   - type: JET_WS_TEXT for UTF-8, JET_WS_BINARY for anything else.
 * Returns: JET_NET_OK when the message was queued; JET_NET_PENDING while the link is still connecting; JET_NET_E_LIMIT when the transmit ring has no room for it RIGHT NOW (retry next tick); JET_NET_E_ARG for a stale handle, an unknown type, or a message larger than jet_ws_message_max; JET_NET_E_CLOSED once the link has ended or jet_ws_close has been called; JET_NET_E_UNSUPPORTED on a runtime without the capability.
 * Include: jet_ws.h
 * Notes: WHOLE MESSAGES ONLY — there are no partial sends to loop over, which is the point: either the message is queued in full or nothing happened, so a retry is a plain retry and never a resend of half a packet. THE TWO REFUSALS MEAN DIFFERENT THINGS: JET_NET_E_LIMIT is backpressure (the peer or the network is behind — try again next tick), while JET_NET_E_ARG means this message will never fit any tick and the arena needs to be bigger. Queued is not sent: the host drains the ring over the following ticks.
 * Example:
 *   const int r = jet_ws_send(s_link, &s_input, sizeof s_input, JET_WS_BINARY);
 *   if (r == JET_NET_E_LIMIT) s_resend_next_tick = true;   // backpressure, not a bug
 */
int jet_ws_send(JetWs w, const void* msg, uint32_t len, JetWsMsgType type);

/**
 * Function: jet_ws_recv
 * Summary: Takes the next whole message the peer sent, if there is one.
 * Parameters:
 *   - w: The socket to read.
 *   - buf: Destination for the payload. May be anywhere — the cart copies out of its own ring, so this does NOT have to be WRAM.
 *   - cap: Capacity of buf in bytes.
 *   - out_type: Filled in with the message's type when one is returned. Pass NULL if you do not care.
 * Returns: The message's payload length (0..cap) when one was taken; 0 when nothing is pending; JET_NET_E_LIMIT when the next message is larger than cap (it is DROPPED, so the stream keeps moving); JET_NET_E_CLOSED when nothing is pending and the link has ended; JET_NET_E_ARG for a stale handle.
 * Include: jet_ws.h
 * Notes: Call it in a loop each tick until it stops returning positive — one whole message per call, never a fragment. "Nothing pending" and "an empty message" share the return value 0, exactly as jet_net_lan_poll's beacon does; give your protocol at least one byte and 0 means precisely "no news". A message the peer sent BEFORE hanging up is still delivered after the link closes, so drain until JET_NET_E_CLOSED rather than stopping the moment jet_ws_state changes — the last thing a relay says is often the interesting one.
 * Example:
 *   uint8_t msg[MSG_CAP];
 *   for (int n; (n = jet_ws_recv(s_link, msg, sizeof msg, NULL)) > 0; ) protocol_consume(msg, n);
 */
int jet_ws_recv(JetWs w, void* buf, uint32_t cap, JetWsMsgType* out_type);

/**
 * Function: jet_ws_close
 * Summary: Hangs up, and reports when the socket and its arena are yours again.
 * Parameters:
 *   - w: The socket to close; a stale or invalid handle is ignored.
 * Returns: JET_NET_PENDING while the runtime is still finishing with the socket, or JET_NET_OK once the slot is free, the arena is the cart's again, and the handle has gone stale.
 * Include: jet_ws.h
 * Notes: Idempotent and always safe, so it is the right thing to call on every exit path. The first call starts a clean close handshake; each later one simply asks whether it has finished. CALL IT UNTIL IT RETURNS JET_NET_OK IF YOU WANT THE ARENA BACK — until then the host may still be writing arrived messages into the receive ring, and reusing that memory would corrupt them. A cart that is simply hanging up and moving on can call it once and forget: the slot reclaims itself, and re-dialling with the same arena is refused (invalid handle) until it is genuinely free, so a retry next tick is all a reconnect ever needs.
 * Example:
 *   void _update(float dt) {
 *       if (s_hanging_up && jet_ws_close(s_link) == JET_NET_OK) { s_hanging_up = false; redial(); }
 *   }
 */
int jet_ws_close(JetWs w);

/**
 * Function: jet_ws_shutdown
 * Summary: Hangs up every socket this module still holds and empties its pool.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_ws.h
 * Notes: Optional — the runtime registers it to run automatically at process exit, so a cart that simply ends never strands a socket. Call it explicitly from _end() when you want teardown at a moment you choose, or between levels to reset the pool. Idempotent, and safe before any socket was ever opened. Every handle taken before the call is stale afterwards, and each slot returns to the pool as the host lets go of it over the following ticks.
 * Example:
 *   void _end(void) { jet_ws_shutdown(); }
 */
void jet_ws_shutdown(void);

#endif // JET_WS_H
