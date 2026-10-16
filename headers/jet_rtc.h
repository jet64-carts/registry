// runtime/jet_rtc.h
#ifndef JET_RTC_H
#define JET_RTC_H

// ============================================================================
// Jet64 WebRTC — a direct line between two players (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_ws.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_rtc.c only when a
// cart's #include tree reaches this header, and the exporter keys the player's
// WebRTC stack off the very same signal. A cart that never includes it contains
// no WebRTC code at all — and its exported player compiles no WebRTC stack.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_rtc.h"     // <-- explicit opt-in
//
// A peer connection joins two copies of a game directly, computer to computer,
// on the workstation, the native player and the web player alike — so a browser
// player and a desktop player can play each other with no server carrying the
// game between them. It carries whole messages on two lanes:
//
//   * JET_RTC_FAST — unordered and never resent. A message arrives at once or
//     not at all: the right lane for per-frame input, where a resent packet would
//     arrive too late to matter and would hold up the ones behind it.
//   * JET_RTC_SURE — ordered and reliable, like a WebSocket: chat, a lobby's
//     words, a level's seed, anything that must arrive whole and in order.
//
// <nocodex>
// WHY THIS MODULE EXISTS (docs/networking/rtc.md §1). A
// browser has exactly one way to talk to another computer directly, WebRTC, and
// exactly one unreliable transport, its data channel; a WebSocket is a relayed
// TCP stream whose one lost packet holds up everything behind it. And WebRTC's
// connection setup, ICE, is NAT traversal done properly — learn your public
// address from a STUN server, try every pairing of addresses at once, relay
// through a TURN server when the routers will not allow a direct path — so a
// player behind a home router, or an internet provider's CGNAT, never forwards a
// port. The browser carries its own WebRTC stack; the native hosts carry a Rust
// one. Jet64 itself contains no NAT-traversal code of its own.
// </nocodex>
//
// ----------------------------------------------------------------------------
// THE FIVE RULES
// ----------------------------------------------------------------------------
// 1. NOTHING BLOCKS. jet_rtc_open returns on the tick it was called; everything
//    else happens over the following ones. Poll jet_rtc_state from _update and
//    send once it reads JET_TCP_OPEN — the same four states a WebSocket and a
//    TCP connection use, so one session layer can drive any of the three.
//
// 2. THE CART CARRIES THE INTRODUCTIONS. Before two peers can connect, each must
//    hand the other a *description* of itself — a few hundred bytes of text
//    naming its addresses and its keys. Jet64 writes yours (read it with
//    jet_rtc_local_description) and takes theirs (jet_rtc_remote_description),
//    but carrying them between the two machines is yours to do: through your own
//    lobby over jet_ws, by jet_http, or pasted by hand. One peer opens as
//    JET_RTC_OFFER and sends its description first; the other opens as
//    JET_RTC_ANSWER, takes it, and sends its own back.
//
// 3. THE SERVERS ARE YOURS. A connection learns its public address from a STUN
//    server and, when no direct path exists, relays through a TURN server. Name
//    them in the config. Public STUN servers are free (stun:stun.cloudflare.com:3478);
//    TURN costs money to run, so its credentials should come from your own lobby,
//    short-lived, for each session — never baked into the cart.
//
// 4. THE ARENA IS THE CONNECTION'S. Reserve it in WRAM with jet_memory_reserve
//    and leave it untouched until jet_rtc_close reports JET_NET_OK. The host
//    writes descriptions and arrived messages straight into it.
//
// 5. PLAYERS SEE EACH OTHER'S ADDRESS. A direct connection shows each computer
//    the other's internet address — that is what direct means. Offer your players
//    JET_RTC_RELAY_ONLY, which always goes through the TURN server and shows only
//    the server's address, at the cost of a little latency and the relay's bill.
//
// <nocodex>
// ----------------------------------------------------------------------------
// WHY THE HOST DOES THE WORK
// ----------------------------------------------------------------------------
// DTLS and SCTP — WebRTC's encryption and its message transport — are a TLS
// stack and a transport protocol Jet64 will not vendor into the sandbox, for the
// reason jet_ws.h gives: one audited stack, owned by the host. So a connection is
// *described* in a NET CMD descriptor (jet_core.h) and driven by the host: a Rust
// WebRTC stack on a worker thread in the workstation and the native player, the
// browser's own RTCPeerConnection in the web player. The data path is the
// WebSocket's two lock-free rings, framed by net_ring_internal.h, with the
// frame's kind byte naming the lane; the two descriptions sit beside the rings in
// the arena. See docs/networking/rtc.md.
// </nocodex>
//
// ----------------------------------------------------------------------------
// SIZING THE ARENA
// ----------------------------------------------------------------------------
// The first 2 × JET_RTC_DESCRIPTION_MAX bytes hold the two descriptions; the rest
// is split evenly between outgoing and incoming messages, each half rounded down
// to a power of two. JET_RTC_ARENA_MIN (16 KiB) gives two 4 KiB rings — plenty
// for game input; jet_rtc_message_max reports the exact limit once open.
//
// Size a bigger arena the same way — the descriptions plus two equal power-of-two
// rings, 2 * JET_RTC_DESCRIPTION_MAX + 2 * ring — rather than as a power of two
// itself: because the descriptions come first, a power-of-two arena wastes up to a
// ring's worth. 32 KiB leaves 24 KiB for rings, which halve to 12 and round down
// to 8, so a quarter of it goes unused; the descriptions' 8 KiB and two 8 KiB
// rings make 24 KiB, which buys the same rings and wastes nothing.
//
// Complete two-peer example — both ends in one cart, introduced in memory, so it
// runs anywhere (copy into main.c; a real game carries the descriptions over its
// lobby instead):
//
//   #include "jet64.h"
//   #include "jet_rtc.h"
//
//   enum { ARENA_BYTES = 16 * 1024, LINE_CAPACITY = 64 };
//   static const JetRtcIceServer STUN[] = { { "stun:stun.cloudflare.com:3478", NULL, NULL } };
//   static JetRtc host, guest;
//   static bool introduced, answered, greeted;
//   static char description[JET_RTC_DESCRIPTION_MAX];
//   static char status_line[LINE_CAPACITY] = "INTRODUCING...";
//
//   void _init(void) {
//       void* host_arena = jet_memory_reserve(ARENA_BYTES);
//       void* guest_arena = jet_memory_reserve(ARENA_BYTES);
//       const JetRtcConfig offer = { JET_RTC_OFFER, JET_RTC_ANY_PATH, STUN, 1 };
//       const JetRtcConfig answer = { JET_RTC_ANSWER, JET_RTC_ANY_PATH, STUN, 1 };
//       host = jet_rtc_open(&offer, host_arena, ARENA_BYTES);
//       guest = jet_rtc_open(&answer, guest_arena, ARENA_BYTES);
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       // The offer travels to the answering side, and its answer comes back.
//       int n;
//       if (!introduced && (n = jet_rtc_local_description(host, description, sizeof description)) > 0) {
//           introduced = jet_rtc_remote_description(guest, description, (uint32_t)n) == JET_NET_OK;
//       }
//       if (introduced && !answered
//           && (n = jet_rtc_local_description(guest, description, sizeof description)) > 0) {
//           answered = jet_rtc_remote_description(host, description, (uint32_t)n) == JET_NET_OK;
//       }
//       if (jet_rtc_state(host) == JET_TCP_OPEN && !greeted) {
//           greeted = jet_rtc_send(host, JET_RTC_SURE, "hello", 5) == JET_NET_OK;
//       }
//       char got[LINE_CAPACITY];
//       if ((n = jet_rtc_recv(guest, got, sizeof got - 1, NULL)) > 0) {
//           got[n] = '\0';
//           snprintf(status_line, sizeof status_line, "GUEST HEARD: %s", got);
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(status_line, 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_rtc_shutdown();
//   }

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memcpy (sandbox-safe)
#include "jet_net.h"  // the shared JetNetResult vocabulary, JetTcpState, handle discipline

// ============================================================================
// LIMITS — every bound named once
// ============================================================================

/**
 * Constant: JET_RTC_MAX_PEERS
 * Summary: Maximum simultaneous peer connections one cart may hold.
 * Include: jet_rtc.h
 * Notes: One per partner: a two-player game needs one, a four-player mesh three on each machine. Each live or closing connection occupies one fixed NET command slot; finish closing one before reusing its slot or its arena.
 * Example:
 *   JetRtc partners[JET_RTC_MAX_PEERS] = {0};
 */
#define JET_RTC_MAX_PEERS JET_NET_RTC_SLOTS

/**
 * Constant: JET_RTC_ICE_SERVERS_MAX
 * Summary: Most STUN and TURN servers one connection may name.
 * Include: jet_rtc.h
 * Notes: One STUN server and one TURN server is the usual pair; a TURN service that offers several transports can take the rest.
 * Example:
 *   JetRtcIceServer servers[JET_RTC_ICE_SERVERS_MAX];
 */
#define JET_RTC_ICE_SERVERS_MAX 4

/**
 * Constant: JET_RTC_DESCRIPTION_MAX
 * Summary: The largest connection description either side writes, in bytes, its terminator included.
 * Include: jet_rtc.h
 * Notes: A description is usually five hundred to fifteen hundred bytes; this leaves room for every address a computer can have. Size the buffer you read your own into, and the message your lobby carries, to this.
 * Example:
 *   static char my_description[JET_RTC_DESCRIPTION_MAX];
 */
#define JET_RTC_DESCRIPTION_MAX JET_NET_RTC_DESCRIPTION_MAX

/**
 * Constant: JET_RTC_RING_MIN
 * Summary: Smallest transmit or receive ring a connection's arena is split into, in bytes.
 * Include: jet_rtc.h
 * Notes: The largest message is smaller by JET_RTC_FRAME_OVERHEAD. A message on the FAST lane is best kept under about a kilobyte, so it travels in one network packet: one that needs several is lost if any of them is.
 * Example:
 *   uint32_t minimum_message_storage = JET_RTC_RING_MIN;
 */
#define JET_RTC_RING_MIN JET_NET_RTC_RING_MIN

/**
 * Constant: JET_RTC_ARENA_MIN
 * Summary: Smallest WRAM arena jet_rtc_open accepts: room for both descriptions and two minimum rings.
 * Include: jet_rtc.h
 * Notes: 16 KiB. A larger power-of-two remainder after the descriptions buys longer rings: more messages queued in each direction before backpressure. Size a larger arena as 2 * JET_RTC_DESCRIPTION_MAX plus two equal power-of-two rings, not as a power of two itself — the descriptions come first, so a power-of-two arena wastes up to a ring's worth (32 KiB gets the same 8 KiB rings as 24 KiB).
 * Example:
 *   uint8_t* arena = (uint8_t*)jet_memory_reserve(JET_RTC_ARENA_MIN);
 */
#define JET_RTC_ARENA_MIN (2 * JET_RTC_DESCRIPTION_MAX + 2 * JET_RTC_RING_MIN)

/**
 * Constant: JET_RTC_FRAME_OVERHEAD
 * Summary: Ring bytes one message's header spends on its length and lane.
 * Include: jet_rtc.h
 * Notes: Subtract it from a ring's size for its largest message, or call jet_rtc_message_max on an open handle for the exact figure. Each payload is also padded to a 4-byte boundary, so a message occupies up to 3 bytes more than this and its length: count that when working out how many messages a ring queues.
 * Example:
 *   uint32_t largest = JET_RTC_RING_MIN - JET_RTC_FRAME_OVERHEAD;
 */
#define JET_RTC_FRAME_OVERHEAD JET_NET_WS_FRAME_HEADER_BYTES

/**
 * Constant: JET_RTC_ICE_TEXT_MAX
 * Summary: Most bytes one STUN or TURN URL, username or credential may be, its terminator included.
 * Include: jet_rtc.h
 * Notes: Short-lived TURN credentials from a service such as Cloudflare's run to about a hundred characters; jet_rtc_open refuses a longer one rather than cutting it short.
 * Example:
 *   char turn_password[JET_RTC_ICE_TEXT_MAX];
 */
#define JET_RTC_ICE_TEXT_MAX 256

// ============================================================================
// HANDLES, ROLES AND LANES
// ============================================================================

/**
 * Struct: JetRtc
 * Summary: An opaque handle to one peer connection, carrying a generation-stamped pool id.
 * Fields:
 *   - id: The packed slot + generation id. Treat it as opaque; test it with jet_rtc_valid.
 * Include: jet_rtc.h
 * Notes: Copy it freely — it is one integer. A handle whose jet_rtc_close has completed is permanently stale: every call reports JET_NET_E_ARG rather than touching whatever now occupies the slot. A zero-initialised handle is safely invalid.
 * Example:
 *   static JetRtc s_partner;
 */
typedef struct {
    int32_t id;
} JetRtc;

/**
 * Enum: JetRtcRole
 * Summary: Which side of the introduction a connection takes.
 * Include: jet_rtc.h
 * Notes: Exactly one peer of a pair offers and the other answers; two offers, or two answers, never connect. A lobby usually decides: whoever hosted the room offers, whoever joined answers. Neither value is 0, so a zeroed config is refused rather than silently one of them.
 * Example:
 *   const JetRtcConfig cfg = { i_am_host ? JET_RTC_OFFER : JET_RTC_ANSWER, JET_RTC_ANY_PATH, servers, 2 };
 */
typedef enum {
    JET_RTC_OFFER  = 1, // writes its description first
    JET_RTC_ANSWER = 2, // takes the offer, then writes its own
} JetRtcRole;

/**
 * Enum: JetRtcPolicy
 * Summary: Which routes a connection may take.
 * Include: jet_rtc.h
 * Notes: JET_RTC_ANY_PATH goes direct whenever the two computers can reach each other, which is fastest, and relays through TURN only when they cannot. JET_RTC_RELAY_ONLY always relays, so neither player learns the other's address — it needs a TURN server in the config, and fails without one. 0 in a config means JET_RTC_ANY_PATH.
 * Example:
 *   cfg.policy = settings.hide_my_address ? JET_RTC_RELAY_ONLY : JET_RTC_ANY_PATH;
 */
typedef enum {
    JET_RTC_ANY_PATH   = 1,
    JET_RTC_RELAY_ONLY = 2,
} JetRtcPolicy;

/**
 * Enum: JetRtcLane
 * Summary: The two ways a message can travel.
 * Include: jet_rtc.h
 * Notes: JET_RTC_FAST is unordered and never resent: right for per-frame input and state, which is stale the moment a fresher copy exists. JET_RTC_SURE is ordered and reliable: right for anything that must arrive whole and in order. Messages on different lanes may overtake one another. Neither value is 0.
 * Example:
 *   jet_rtc_send(s_partner, JET_RTC_FAST, &input, sizeof input);
 */
typedef enum {
    JET_RTC_FAST = 1, // unordered, never retransmitted
    JET_RTC_SURE = 2, // ordered and reliable
} JetRtcLane;

/**
 * Struct: JetRtcIceServer
 * Summary: One STUN or TURN server a connection may use to find its way to the other peer.
 * Fields:
 *   - url: `stun:host:port` or `turn:host:port`, NUL-terminated. The native players reach STUN and TURN over UDP; a browser also understands `turns:` and `?transport=tcp`.
 *   - username: A TURN server's username, or NULL for STUN.
 *   - credential: A TURN server's password, or NULL for STUN.
 * Include: jet_rtc.h
 * Notes: The host names in these URLs are checked against the project's network allowlist like any other host the cart reaches. The other player's address cannot be known in advance, so it is not on any list — which is why the Workstation's network prompt, and an export's summary, say plainly that the cart connects directly to other players.
 * Example:
 *   const JetRtcIceServer servers[] = {
 *       { "stun:stun.cloudflare.com:3478", NULL, NULL },
 *       { "turn:turn.cloudflare.com:3478", s_turn_user, s_turn_password },
 *   };
 */
typedef struct {
    const char* url;
    const char* username;
    const char* credential;
} JetRtcIceServer;

/**
 * Struct: JetRtcConfig
 * Summary: Everything jet_rtc_open needs to know about a connection before it begins.
 * Fields:
 *   - role: JET_RTC_OFFER or JET_RTC_ANSWER.
 *   - policy: JET_RTC_ANY_PATH or JET_RTC_RELAY_ONLY; 0 means JET_RTC_ANY_PATH.
 *   - ice_servers: The STUN and TURN servers to use; may be NULL when ice_server_count is 0.
 *   - ice_server_count: How many, 0 to JET_RTC_ICE_SERVERS_MAX.
 * Include: jet_rtc.h
 * Notes: With no servers at all, two computers can still connect when each can reach the other's own address — on one home network, or one machine — which is all a LAN game needs. The config is copied by jet_rtc_open, so it and its strings may be temporaries.
 * Example:
 *   const JetRtcConfig cfg = { JET_RTC_OFFER, JET_RTC_ANY_PATH, servers, 2 };
 */
typedef struct {
    JetRtcRole             role;
    JetRtcPolicy           policy;
    const JetRtcIceServer* ice_servers;
    uint32_t               ice_server_count;
} JetRtcConfig;

/**
 * Enum: JetRtcPathKind
 * Summary: The route an open connection found.
 * Include: jet_rtc.h
 * Notes: JET_RTC_PATH_UNKNOWN until the connection is open and the host has looked.
 * Example:
 *   if (path.kind == JET_RTC_PATH_RELAYED) jet_print("RELAYED", 8, 8, 10);
 */
typedef enum {
    JET_RTC_PATH_UNKNOWN = 0,
    JET_RTC_PATH_DIRECT  = 1, // computer to computer
    JET_RTC_PATH_RELAYED = 2, // through a TURN server
} JetRtcPathKind;

/**
 * Struct: JetRtcPath
 * Summary: What jet_rtc_path reports of an open connection's route.
 * Fields:
 *   - kind: Direct, relayed, or not yet known.
 *   - rtt_ms: The connection's own measured round trip in milliseconds, 0 until it has one.
 * Include: jet_rtc.h
 * Notes: Worth showing your players before a game begins — "DIRECT 18 ms" or "RELAYED 64 ms" sets their expectations honestly.
 * Example:
 *   JetRtcPath path;
 *   if (jet_rtc_path(s_partner, &path) == JET_NET_OK) show_ping(path.rtt_ms);
 */
typedef struct {
    JetRtcPathKind kind;
    uint32_t       rtt_ms;
} JetRtcPath;

/**
 * Function: jet_rtc_valid
 * Summary: Reports whether jet_rtc_open handed back a real connection slot.
 * Parameters:
 *   - r: The handle to test.
 * Returns: true when a slot was claimed; false when the pool was full, an argument was refused, or JET_NET_CAP_RTC is absent on this runtime.
 * Include: jet_rtc.h
 * Notes: A false here means nothing was started — retry a full pool next tick, but fix a refused argument.
 * Example:
 *   s_partner = jet_rtc_open(&cfg, s_arena, ARENA_BYTES);
 *   if (!jet_rtc_valid(s_partner)) status_show("NO DIRECT CONNECTIONS HERE");
 */
static inline bool jet_rtc_valid(JetRtc r) {
    return jet_net_handle_valid(r.id);
}

// ============================================================================
// THE CONNECTION
// ============================================================================

/**
 * Function: jet_rtc_open
 * Summary: Starts a peer connection and returns immediately; its description is prepared over the following ticks.
 * Parameters:
 *   - cfg: The role, the routes allowed, and the STUN/TURN servers. Copied; may be a temporary.
 *   - arena: Memory for the two descriptions and the two message rings. MUST live in WRAM (jet_memory_reserve).
 *   - arena_bytes: Its size, at least JET_RTC_ARENA_MIN.
 * Returns: A handle whose state begins at JET_TCP_CONNECTING, or an invalid handle (see jet_rtc_valid) when the pool is full, an argument is malformed, the arena is not in WRAM or overlaps a connection that has not finished with it, or JET_NET_CAP_RTC is absent.
 * Include: jet_rtc.h
 * Notes: NOTHING BLOCKS. An offer's description is ready within a second or two, once the computer has learnt its addresses from the STUN and TURN servers; an answer's is ready the same way once the offer has been given to it. There is no built-in deadline for the other peer to answer: your cart decides when to give up, and calls jet_rtc_close. In the Workstation, a cart's first connection waits on the project's one-time network prompt.
 * Example:
 *   static JetRtc s_partner;
 *   static uint8_t* s_arena;
 *   void host_room(void) {
 *       const JetRtcConfig cfg = { JET_RTC_OFFER, JET_RTC_ANY_PATH, s_servers, 2 };
 *       s_partner = jet_rtc_open(&cfg, s_arena, ARENA_BYTES);
 *   }
 */
JetRtc jet_rtc_open(const JetRtcConfig* cfg, void* arena, uint32_t arena_bytes);

/**
 * Function: jet_rtc_state
 * Summary: Reports where a connection is in its life.
 * Parameters:
 *   - r: The connection to poll.
 * Returns: A JetTcpState value — JET_TCP_CONNECTING (preparing, being introduced, or finding its route), JET_TCP_OPEN, JET_TCP_CLOSED or JET_TCP_FAILED — or JET_NET_E_ARG for a stale or never-assigned handle.
 * Include: jet_rtc.h
 * Notes: The same four states a WebSocket and a TCP connection report, so a session layer can drive any of them through one switch. JET_TCP_OPEN means both lanes are ready. JET_TCP_CLOSED is the other peer hanging up, the connection being lost, or your own jet_rtc_close once the host has let go; JET_TCP_FAILED means it never worked, and jet_rtc_error says why. Messages already received stay readable after the end: drain with jet_rtc_recv until it reports JET_NET_E_CLOSED.
 * Example:
 *   if (jet_rtc_state(s_partner) == JET_TCP_OPEN) lockstep_pump(s_partner);
 */
int jet_rtc_state(JetRtc r);

/**
 * Function: jet_rtc_error
 * Summary: Explains why a connection reached JET_TCP_FAILED or JET_TCP_CLOSED.
 * Parameters:
 *   - r: The connection to ask.
 * Returns: The JetNetResult that ended it — JET_NET_E_CONNECT when no route could be found, JET_NET_E_DENIED when a server was not on the project's allowlist or network use was refused, JET_NET_E_RESOLVE when a server's name could not be found, JET_NET_E_ARG when the other peer's description could not be read or the host refused a server's URL, username or credential, JET_NET_E_LIMIT when a SURE message was too big for this cart's receive ring, a message of either lane too big for the other cart's (each side tells the other what it takes), or this side's description too long for its area, JET_NET_E_CLOSED when the other peer hung up — or JET_NET_OK while still connecting or open, or JET_NET_E_ARG for a stale handle.
 * Include: jet_rtc.h
 * Notes: JET_NET_E_CONNECT after a long wait is the commonest failure, and it usually means both players are behind routers that refuse strangers and no TURN server was named. The cure is a TURN server, not a retry.
 * Example:
 *   if (jet_rtc_state(s_partner) == JET_TCP_FAILED && jet_rtc_error(s_partner) == JET_NET_E_CONNECT) {
 *       status_show("NO ROUTE - TRY THE RELAY");
 *   }
 */
int jet_rtc_error(JetRtc r);

/**
 * Function: jet_rtc_local_description
 * Summary: Copies this connection's description — the text the other peer needs to reach it — once it is ready.
 * Parameters:
 *   - r: The connection.
 *   - out: Where to copy it, NUL-terminated.
 *   - cap: The size of out, at least JET_RTC_DESCRIPTION_MAX to be sure of room.
 * Returns: Its length (more than 0) once ready; 0 while it is still being prepared — an answer's is not prepared until the offer has been given to it; JET_NET_E_LIMIT when it does not fit in cap bytes (out is then the empty string); JET_NET_E_CLOSED when it is not ready and the connection has ended or been closed; JET_NET_E_ARG for a stale handle.
 * Include: jet_rtc.h
 * Notes: Send it to the other peer exactly as it is, over whatever you use for the introduction — it is text, so a WebSocket's JET_WS_TEXT message or an HTTP body carries it unchanged. Ask once per tick until it is ready; it never changes once it is. It names this computer's addresses, so it is worth carrying over an encrypted channel (wss://, https://).
 * Example:
 *   int len = jet_rtc_local_description(s_partner, s_description, sizeof s_description);
 *   if (len > 0 && !s_sent) s_sent = jet_ws_send(s_lobby, s_description, (uint32_t)len, JET_WS_TEXT) == JET_NET_OK;
 */
int jet_rtc_local_description(JetRtc r, char* out, uint32_t cap);

/**
 * Function: jet_rtc_remote_description
 * Summary: Hands this connection the other peer's description, so it can find its way to them.
 * Parameters:
 *   - r: The connection.
 *   - text: The description the other peer's jet_rtc_local_description produced, exactly as it arrived.
 *   - len: Its length in bytes, without a terminator; at most JET_RTC_DESCRIPTION_MAX - 1.
 * Returns: JET_NET_OK when it was taken; JET_NET_E_ARG for an empty or over-long description, a second one, or a stale handle; JET_NET_E_CLOSED once the connection has ended or jet_rtc_close has been called.
 * Include: jet_rtc.h
 * Notes: Exactly once per connection. An answering connection takes the offer and then prepares its own description; an offering one takes the answer and then begins to connect. A description that is not one — the wrong text, or a second offer handed to an offer — ends the connection JET_TCP_FAILED with JET_NET_E_ARG.
 * Example:
 *   if (got_offer) jet_rtc_remote_description(s_partner, offer_text, offer_len);
 */
int jet_rtc_remote_description(JetRtc r, const char* text, uint32_t len);

/**
 * Function: jet_rtc_message_max
 * Summary: The largest single message this connection can carry, in either direction.
 * Parameters:
 *   - r: The connection.
 * Returns: The payload byte count one ring can hold, or 0 for a stale handle.
 * Include: jet_rtc.h
 * Notes: Derived from the arena: one ring minus JET_RTC_FRAME_OVERHEAD. Sending more is refused with JET_NET_E_ARG. A FAST message is best kept far below it — under about a kilobyte — so it fits one network packet.
 * Example:
 *   if (snapshot_len > jet_rtc_message_max(s_partner)) LOG_WARN("arena too small for a snapshot");
 */
uint32_t jet_rtc_message_max(JetRtc r);

/**
 * Function: jet_rtc_send
 * Summary: Queues one whole message for the other peer, on one lane.
 * Parameters:
 *   - r: An open connection.
 *   - lane: JET_RTC_FAST or JET_RTC_SURE.
 *   - msg: The payload. May be anywhere — it is copied into the ring.
 *   - len: Its length; 0 sends an empty message.
 * Returns: JET_NET_OK when queued; JET_NET_PENDING while the connection is still connecting; JET_NET_E_LIMIT when there is no room right now (retry next tick); JET_NET_E_ARG for an unknown lane, a stale handle, or a message larger than jet_rtc_message_max; JET_NET_E_CLOSED once the connection has ended or jet_rtc_close has been called; JET_NET_E_UNSUPPORTED when this runtime no longer offers WebRTC at all.
 * Include: jet_rtc.h
 * Notes: Whole messages only, as with jet_ws_send. JET_NET_OK on the FAST lane means queued, not delivered: a FAST message may be lost — on the way, or dropped by either host when the other peer is behind or the message is larger than its receive ring holds — and your protocol should expect it: send the last few frames of input in every packet, as jet_lockstep does. A FAST message never waits behind a SURE one. Keep messages within the smaller of the two peers' jet_rtc_message_max: one larger than the other cart takes ends the connection with JET_NET_E_LIMIT.
 * Example:
 *   jet_rtc_send(s_partner, JET_RTC_FAST, packet, (uint32_t)packet_len);
 */
int jet_rtc_send(JetRtc r, JetRtcLane lane, const void* msg, uint32_t len);

/**
 * Function: jet_rtc_recv
 * Summary: Takes the next whole message the other peer sent, if there is one.
 * Parameters:
 *   - r: The connection.
 *   - buf: Where to copy the payload. May be anywhere.
 *   - cap: The capacity of buf.
 *   - out_lane: Filled in with the lane it travelled; NULL if you do not care.
 * Returns: Its length (0..cap) when one was taken; 0 when nothing is waiting; JET_NET_E_LIMIT when the next message is larger than cap (it is dropped, so the stream moves on); JET_NET_E_CLOSED when nothing is waiting and the connection has ended or been closed; JET_NET_E_ARG for a stale handle.
 * Include: jet_rtc.h
 * Notes: Call it in a loop each tick until it stops returning positive. Both lanes arrive through this one call, in the order they arrived; give every message at least one byte so 0 means "nothing".
 * Example:
 *   uint8_t packet[256];
 *   for (int n; (n = jet_rtc_recv(s_partner, packet, sizeof packet, NULL)) > 0; ) protocol_consume(packet, n);
 */
int jet_rtc_recv(JetRtc r, void* buf, uint32_t cap, JetRtcLane* out_lane);

/**
 * Function: jet_rtc_path
 * Summary: Reports the route an open connection found, and its round trip.
 * Parameters:
 *   - r: The connection.
 *   - out: Filled in with the route and the round trip.
 * Returns: JET_NET_OK, or JET_NET_E_ARG for a stale handle or a NULL out.
 * Include: jet_rtc.h
 * Notes: The round trip is the connection's own measurement, refreshed every second or so while it is open; a lockstep game sizes its input delay from it.
 * Example:
 *   JetRtcPath path;
 *   jet_rtc_path(s_partner, &path);
 */
int jet_rtc_path(JetRtc r, JetRtcPath* out);

/**
 * Function: jet_rtc_close
 * Summary: Hangs up, and reports when the connection and its arena are yours again.
 * Parameters:
 *   - r: The connection; a stale or invalid handle is ignored.
 * Returns: JET_NET_PENDING while the host is still finishing with it; JET_NET_OK once the slot is free, the arena is the cart's again, and the handle has gone stale.
 * Include: jet_rtc.h
 * Notes: Idempotent and always safe. Call it until it returns JET_NET_OK if you want to reuse the arena; a cart simply hanging up can call it once and forget — the slot comes home by itself, collected by the next jet_rtc_open once the host has let go, and the handle goes stale then — and a jet_rtc_open with the same arena is refused until it is genuinely free. Once it has been called, the connection sends nothing more and takes no description.
 * Example:
 *   if (s_leaving && jet_rtc_close(s_partner) == JET_NET_OK) s_leaving = false;
 */
int jet_rtc_close(JetRtc r);

/**
 * Function: jet_rtc_shutdown
 * Summary: Hangs up every connection this module still holds and empties its pool.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_rtc.h
 * Notes: Optional on the workstation and the native player, which run it at process exit; a web player has no exit hook, and there the host owns the module's lifetime. Call it from _end() to hang up at a moment you choose, on every runtime alike. Idempotent, and safe before any connection was opened.
 * Example:
 *   void _end(void) { jet_rtc_shutdown(); }
 */
void jet_rtc_shutdown(void);

#endif // JET_RTC_H
