// runtime/jet_net.h
#ifndef JET_NET_H
#define JET_NET_H

// ============================================================================
// Jet64 networking — TCP, UDP and LAN discovery (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_pcm.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_net.c only when a
// cart's #include tree reaches this header. A cart that never includes it
// contains no networking code at all — not "disabled", *absent*, in the
// workstation guest and in every export, auditable by reading the includes.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_net.h"     // <-- explicit opt-in
//
// Use this module for non-blocking TCP, UDP and LAN discovery on the
// workstation and native player. Raw sockets are unavailable on the web player.
//
// <nocodex>
// The machines Jet64 honours were social machines: the C64 and Amiga scenes ran
// on BBSes as much as on games. This module is the phone line — raw TCP for
// dialling a board, UDP for LAN play, and a standard discovery beacon so two
// copies of a cart can find each other on a home network with no server at all.
// See docs/networking/design.md.
// </nocodex>
//
// ----------------------------------------------------------------------------
// THE FOUR RULES
// ----------------------------------------------------------------------------
// 1. NOTHING BLOCKS. The guest advances at a fixed 120 Hz, so every call here
//    returns immediately. Work is submitted, progress is *observed* from
//    _update by polling (jet_tcp_state), and bytes surface at tick boundaries.
// <nocodex>
//    Hostname resolution runs on a runtime-owned worker thread the cart never
//    sees, exactly as the host audio thread already exists.
// </nocodex>
//
// 2. CAPABILITIES ARE HONEST, NEVER SILENT. Browsers have no raw sockets, so
//    the whole surface below is absent on the web player. It does not pretend
//    otherwise: every entry point answers JET_NET_E_UNSUPPORTED there, and
//    jet_net_capability() lets a cart ask *before* it tries, so it can adapt
//    (grey out "dial", offer a WebSocket bridge — see jet_ws.h) instead of
//    failing mysteriously. Ask first.
// <nocodex>
//    The answer is a compile-time constant on
//    the runtime that has to honour it, so the check is free.
// </nocodex>
//
// 3. HANDLES HAVE LIFETIMES. A zero-initialised JetTcp, JetUdp or
//    JetTcpListener is safely invalid. After closing one, stop using it; a stale
//    handle reports JET_NET_E_ARG instead of referring to a newer connection.
//
// <nocodex>
// 3. HANDLES CANNOT DANGLE. JetTcp / JetUdp / JetTcpListener are opaque
//    generation-stamped ids over fixed pools (the jet_pool idiom), so a handle
//    used after close fails with JET_NET_E_ARG and can never alias a recycled
//    slot. A zero-initialised handle (`static JetTcp s_sock;`) is safely
//    invalid, and there is no malloc anywhere in this module.
// </nocodex>
//
// 4. THE HOST IS ASKED FIRST. Before the cart dials a host (jet_tcp_connect) or
//    sends to an address that has not sent to it first (jet_udp_send_to), the
//    host is asked whether this cart may reach it: the allowlist the cart was
//    exported with, and in the Workstation the one-time network prompt. So a dial
//    can end with JET_NET_E_DENIED, and the first one may take a few ticks
//    longer. Listening, accepting, answering whoever wrote to you, and the LAN
//    beacon are never asked about: they reach no one who did not reach you, or
//    no one beyond the local network. While the host is being asked, a dial
//    stays JET_TCP_CONNECTING and a first datagram is DROPPED (the send returns
//    0), and there is no timeout: in the Workstation the wait lasts as long as
//    the person takes to answer the prompt. A cart that wants a deadline counts
//    its own ticks and closes the socket, which withdraws the question.
//
// <nocodex>
// 4. WHY THE GUEST ASKS. These sockets are the guest's own — the host never
//    stands between them and the wire — so the host cannot refuse a packet, only
//    an intention. The runtime therefore states each new destination in a NET CMD
//    dial descriptor and waits for the answer (jet_core.h, design §14), which
//    passes through the very consent, allowlist and first-contact log that HTTP,
//    WebSocket and WebRTC pass through: one rule for every road out.
//
//    WHAT THAT RULE BINDS. Every cart that reaches the network through this API —
//    which is every cart the include-tree audit and the INFO screen's ACCESS line
//    describe. It cannot bind a cart written to defeat it: the ask, the answer and
//    the header that says whether to ask live in shared memory the guest can
//    write, and the guest is native code in its own process, so a cart that
//    scribbles on the transport ABI directly (JET_BASE + JET_NET_CMD_OFFSET) is
//    outside any check made inside that process. §6.1's include-tree audit and
//    the disclosure built on it remain the defence there; an OS-level network
//    sandbox around the guest process is the enforcement that would close it, and
//    is future work.
// </nocodex>
//
// Use jet_http.h for HTTP(S), or jet_ws.h for a live connection that must also
// work in a browser. Both support TLS through https:// or wss:// URLs.
//
// <nocodex>
// ----------------------------------------------------------------------------
// WHAT LIVES ELSEWHERE
// ----------------------------------------------------------------------------
// TLS is deliberately absent: Jet64 will not vendor a C TLS stack into the
// sandbox when every host is Rust and one rustls dependency away from an
// audited one. So `https://` and `wss://` are serviced host-side through
// jet_http.h and jet_ws.h — which are also the only transports that reach the
// browser. Raw sockets are the desktop-native privilege, exactly as on real
// hardware, where the WiFi modem speaks TCP but the browser port of your game
// cannot.
// </nocodex>
//
// Complete UDP loopback example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_net.h"
//
//   enum { LOOPBACK_PORT = 41000 };
//   static JetUdp socket;
//   static int packets_received;
//   static const char* network_status = "STARTING";
//
//   void _init(void) {
//       if (!jet_net_capability(JET_NET_CAP_UDP)) {
//           network_status = "UDP UNSUPPORTED";
//           return;
//       }
//       socket = jet_udp_open(LOOPBACK_PORT);
//       network_status = jet_udp_valid(socket) ? "PRESS A TO LOOP BACK" : "PORT OPEN FAILED";
//   }
//
//   void _update(float dt) {
//       static const char ping[] = "ping";
//       const JetNetEndpoint loopback = { "127.0.0.1", LOOPBACK_PORT };
//       char packet[32];
//       (void)dt;
//       if (!jet_udp_valid(socket)) return;
//       if (btnp(BTN_A)) jet_udp_send_to(socket, &loopback, ping, (int)(sizeof ping - 1));
//       while (jet_udp_recv_from(socket, packet, sizeof packet, NULL) > 0) packets_received++;
//   }
//
//   void _draw(void) {
//       char line[48];
//       snprintf(line, sizeof line, "LOOPBACK PACKETS %d", packets_received);
//       jet_cls(0);
//       jet_print(network_status, 16, 16, 15);
//       jet_print(line, 16, 36, 15);
//   }
//
//   void _end(void) {
//       jet_udp_close(socket);
//       jet_net_shutdown();
//   }

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool / memcpy (sandbox-safe)

// ============================================================================
// CAPABILITIES
// ============================================================================

/**
 * Enum: JetNetCapability
 * Summary: One bit per networking capability, so a cart can ask what this runtime can actually do before it tries.
 * Include: jet_net.h
 * Notes: The guest-direct capabilities (TCP, TCP_LISTEN, UDP, HTTP_SERVE) resolve at compile time — they are present on the workstation and the native player, and absent on the web player, which has no raw sockets and cannot listen. The host-mediated ones (HTTP, WS, RTC) are performed by the host, so the host publishes them: they are read from the NET CMD block it stamps before the guest's first instruction, which is why an old player running a newer cart reports what it can really do rather than a guess. Test them with jet_net_capability().
 * Example:
 *   if (jet_net_capability(JET_NET_CAP_WS | JET_NET_CAP_HTTP)) online_menu_enable();
 */
typedef enum {
    JET_NET_CAP_TCP        = 1u << 0, // jet_tcp_connect and friends
    JET_NET_CAP_TCP_LISTEN = 1u << 1, // jet_tcp_listen / jet_tcp_accept
    JET_NET_CAP_UDP        = 1u << 2, // jet_udp_* including broadcast
    JET_NET_CAP_HTTP       = 1u << 3, // jet_http_* client (jet_http.h)
    JET_NET_CAP_HTTP_SERVE = 1u << 4, // jet_http_serve_* (jet_http_serve.h)
    JET_NET_CAP_WS         = 1u << 5, // jet_ws_* client (jet_ws.h)
    JET_NET_CAP_RTC        = 1u << 6, // jet_rtc_* peer-to-peer data channels (jet_rtc.h)
} JetNetCapability;

// The guest-direct capability set this build was compiled with. Raw sockets do
// not exist in a browser, so a JET_WEB build honestly offers none of them; the
// whole implementation below compiles to JET_NET_E_UNSUPPORTED stubs there.
//
// HTTP_SERVE sits here rather than with the host-mediated bits because the cart's
// HTTP server is guest-direct: it is jet_tcp_listen plus a parser (jet_http.c), so
// its presence is a fact about the GUEST, and no host can claim it on the guest's
// behalf. A browser cannot listen, which is why it leaves with the sockets.
#ifdef JET_WEB
#define JET_NET_CAPS_GUEST_DIRECT 0u
#else
#define JET_NET_CAPS_GUEST_DIRECT                                                    \
    ((uint32_t)JET_NET_CAP_TCP | (uint32_t)JET_NET_CAP_TCP_LISTEN                    \
     | (uint32_t)JET_NET_CAP_UDP | (uint32_t)JET_NET_CAP_HTTP_SERVE)
#endif

// ============================================================================
// RESULTS
// ============================================================================

/**
 * Enum: JetNetResult
 * Summary: The single result vocabulary shared by every networking module — negative is an error, 0 is success, 1 is "not yet".
 * Include: jet_net.h
 * Notes: The sign convention is deliberate: `if (r < 0)` reads as "did it fail?" and can never accidentally swallow the pending case. Byte-moving calls (jet_tcp_send / jet_tcp_recv / jet_udp_*) return a non-negative BYTE COUNT on success instead of JET_NET_OK, with 0 meaning "would block / nothing pending" — so `if (n < 0)` is still the error test there.
 * Example:
 *   int sent = jet_tcp_send(sock, bytes, byte_count);
 *   if (sent < 0) connection_failed((JetNetResult)sent);
 */
typedef enum {
    JET_NET_OK            = 0,  // success
    JET_NET_PENDING       = 1,  // still in flight — poll again next tick
    JET_NET_E_UNSUPPORTED = -1, // capability absent on this runtime (see jet_net_capability)
    JET_NET_E_DENIED      = -2, // the allowlist, or the player's own consent, refused this host
    JET_NET_E_RESOLVE     = -3, // DNS lookup failed
    JET_NET_E_CONNECT     = -4, // refused / unreachable / TLS or upgrade failed
    JET_NET_E_CLOSED      = -5, // peer closed the connection
    JET_NET_E_TIMEOUT     = -6, // deadline exceeded
    JET_NET_E_LIMIT       = -7, // handle pool, slot table, or size cap exhausted
    JET_NET_E_ARG         = -8, // bad argument, or a stale/forged handle
} JetNetResult;

// ============================================================================
// LIMITS — every bound named once
// ============================================================================

/**
 * Constant: JET_NET_MAX_TCP
 * Summary: Maximum simultaneous TCP connection handles owned by one cart.
 * Include: jet_net.h
 * Notes: Outbound connections, accepted inbound connections, and HTTP-server clients share this fixed pool. A ninth claim returns an invalid handle or waits in a listener backlog rather than allocating memory.
 * Example:
 *   JetTcp peers[JET_NET_MAX_TCP] = {0};
 */
#define JET_NET_MAX_TCP 8

/**
 * Constant: JET_NET_MAX_UDP
 * Summary: Maximum simultaneous UDP sockets owned by one cart.
 * Include: jet_net.h
 * Notes: jet_udp_open returns an invalid JetUdp when all slots are occupied. One UDP socket can communicate with many peers, so most carts need only one gameplay socket and one discovery socket.
 * Example:
 *   JetUdp sockets[JET_NET_MAX_UDP] = {0};
 */
#define JET_NET_MAX_UDP 4

/**
 * Constant: JET_NET_MAX_LISTENERS
 * Summary: Maximum simultaneous TCP listening sockets owned by one cart.
 * Include: jet_net.h
 * Notes: Listening is unavailable on the web player. jet_tcp_listen returns an invalid handle when the fixed listener pool is full.
 * Example:
 *   JetTcpListener listeners[JET_NET_MAX_LISTENERS] = {0};
 */
#define JET_NET_MAX_LISTENERS 2

/**
 * Constant: JET_NET_LISTEN_BACKLOG
 * Summary: Maximum pending inbound TCP connections queued by one listener.
 * Include: jet_net.h
 * Notes: The operating system may clamp this hint further. Accept connections regularly from _update so the backlog does not fill while live connection slots are available.
 * Example:
 *   uint32_t pending_capacity = JET_NET_LISTEN_BACKLOG;
 */
#define JET_NET_LISTEN_BACKLOG 8

/**
 * Constant: JET_NET_IP_STR_MAX
 * Summary: Character capacity of JetNetEndpoint.ip, including its terminating NUL.
 * Include: jet_net.h
 * Notes: Large enough for a printable IPv6 literal even though the current raw-socket implementation uses IPv4. Use it for any companion buffer that copies or formats an endpoint address.
 * Example:
 *   char peer_address[JET_NET_IP_STR_MAX];
 */
#define JET_NET_IP_STR_MAX 46

/**
 * Constant: JET_NET_HOST_MAX
 * Summary: Maximum accepted hostname buffer size, including its terminating NUL.
 * Include: jet_net.h
 * Notes: jet_tcp_connect rejects a longer hostname with an invalid handle. The bound covers the DNS maximum and keeps resolver staging fixed-size.
 * Example:
 *   char hostname[JET_NET_HOST_MAX] = "bbs.example.org";
 */
#define JET_NET_HOST_MAX 256

// ============================================================================
// HANDLES
// ============================================================================
//
// A handle is one signed 32-bit id: the low 16 bits are the pool slot, the next
// 15 are the generation that was live when the handle was taken, and bit 31 is
// always clear so every valid handle is positive. Generation 0 is reserved for
// "no handle", which is what makes a zero-initialised handle safely invalid and
// JET_NET_HANDLE_NONE simply 0.

#define JET_NET_HANDLE_INDEX_BITS 16u
#define JET_NET_HANDLE_INDEX_MASK 0xFFFFu

// Highest generation a slot reaches before wrapping back to 1. Fifteen bits
// keeps bit 31 clear; a slot would have to be recycled 32767 times before an
// ancient handle could alias, and each recycle invalidates every prior handle.
#define JET_NET_HANDLE_GEN_MAX 0x7FFFu

/**
 * Constant: JET_NET_HANDLE_NONE
 * Summary: Canonical invalid ID stored by an unassigned networking handle.
 * Include: jet_net.h
 * Notes: Numerically zero, so static or explicitly zero-initialised JetTcp, JetUdp, JetTcpListener, JetHttp, JetHttpServer, and JetWs values are safely invalid. Prefer each module's typed valid helper rather than reading id directly.
 * Example:
 *   JetTcp connection = { JET_NET_HANDLE_NONE };
 */
#define JET_NET_HANDLE_NONE ((int32_t)0)

/**
 * Struct: JetTcp
 * Summary: An opaque handle to one TCP connection, carrying a generation-stamped pool id.
 * Fields:
 *   - id: The packed slot + generation id. Treat it as opaque; test it with jet_tcp_valid.
 * Include: jet_net.h
 * Notes: Copy it freely — it is one integer. A handle to a closed connection is permanently stale: every call reports JET_NET_E_ARG rather than touching whatever now occupies the slot.
 * Example:
 *   JetTcp connection = jet_tcp_connect("bbs.example.org", 23);
 */
typedef struct {
    int32_t id;
} JetTcp;

/**
 * Struct: JetTcpListener
 * Summary: An opaque handle to one listening TCP socket, carrying a generation-stamped pool id.
 * Fields:
 *   - id: The packed slot + generation id. Treat it as opaque; test it with jet_tcp_listener_valid.
 * Include: jet_net.h
 * Notes: Listening is a desktop-native privilege — browsers cannot accept connections, so this is absent when jet_net_capability(JET_NET_CAP_TCP_LISTEN) is false.
 * Example:
 *   JetTcpListener listener = jet_tcp_listen(5000);
 */
typedef struct {
    int32_t id;
} JetTcpListener;

/**
 * Struct: JetUdp
 * Summary: An opaque handle to one UDP socket, carrying a generation-stamped pool id.
 * Fields:
 *   - id: The packed slot + generation id. Treat it as opaque; test it with jet_udp_valid.
 * Include: jet_net.h
 * Notes: One socket carries every datagram to and from every peer — UDP is connectionless, so the peer travels in the JetNetEndpoint of each call rather than in the handle.
 * Example:
 *   JetUdp discovery = jet_udp_open(5000);
 */
typedef struct {
    int32_t id;
} JetUdp;

/**
 * Struct: JetNetEndpoint
 * Summary: A peer address in string form — the IP a datagram came from or is going to, plus its port.
 * Fields:
 *   - ip: NUL-terminated printable address, an IPv4 dotted quad today (buffer sized for IPv6 literals).
 *   - port: Host-order port number.
 * Include: jet_net.h
 * Notes: Strings rather than packed integers on purpose: an endpoint is something a cart shows in a lobby list, saves in a phonebook, and compares — and jet_udp_send_to takes the same shape a jet_udp_recv_from filled in, so replying to a peer is a straight copy.
 * Example:
 *   JetNetEndpoint peer = { "192.168.1.42", 5000 };
 */
typedef struct {
    char     ip[JET_NET_IP_STR_MAX];
    uint16_t port;
} JetNetEndpoint;

/**
 * Function: jet_net_handle_valid
 * Summary: Reports whether a raw handle id refers to a slot at all — the shared predicate the per-type helpers wrap.
 * Parameters:
 *   - id: The `id` field of a JetTcp, JetTcpListener or JetUdp.
 * Returns: true when the id carries a non-zero generation; false for JET_NET_HANDLE_NONE and for any zero-initialised handle.
 * Include: jet_net.h
 * Notes: This is a pure unpack of the id's generation field — it does NOT prove the slot is still open (a closed connection's handle is well-formed but stale). Prefer jet_tcp_valid / jet_udp_valid / jet_tcp_listener_valid at call sites; they read better and cost the same.
 * Example:
 *   if (!jet_net_handle_valid(sock.id)) return; // the pool was exhausted
 */
static inline bool jet_net_handle_valid(int32_t id) {
    return id > (int32_t)JET_NET_HANDLE_INDEX_MASK;
}

/**
 * Function: jet_tcp_valid
 * Summary: Reports whether a jet_tcp_connect / jet_tcp_accept call handed back a real connection slot.
 * Parameters:
 *   - t: The handle to test.
 * Returns: true when a slot was claimed; false when the pool was exhausted, the capability is absent, or the handle was never assigned.
 * Include: jet_net.h
 * Notes: This validates the handle's packed shape, not whether the connection is still open. Poll jet_tcp_state for its current lifecycle state.
 * Example:
 *   JetTcp sock = jet_tcp_connect("bbs.example.org", 23);
 *   if (!jet_tcp_valid(sock)) { show_error("no free connection slots"); }
 */
static inline bool jet_tcp_valid(JetTcp t) {
    return jet_net_handle_valid(t.id);
}

/**
 * Function: jet_tcp_listener_valid
 * Summary: Reports whether a jet_tcp_listen call handed back a real listener slot.
 * Parameters:
 *   - l: The handle to test.
 * Returns: true when the port was bound and is listening; false when the bind failed, the pool was exhausted, or the capability is absent.
 * Include: jet_net.h
 * Notes: A listener that has since been closed may still have a well-formed stale handle; API calls reject it with JET_NET_E_ARG.
 * Example:
 *   JetTcpListener srv = jet_tcp_listen(SCORES_PORT);
 *   if (!jet_tcp_listener_valid(srv)) { show_error("port already in use"); }
 */
static inline bool jet_tcp_listener_valid(JetTcpListener l) {
    return jet_net_handle_valid(l.id);
}

/**
 * Function: jet_udp_valid
 * Summary: Reports whether a jet_udp_open call handed back a real socket slot.
 * Parameters:
 *   - u: The handle to test.
 * Returns: true when the socket was opened and bound; false when the bind failed, the pool was exhausted, or the capability is absent.
 * Include: jet_net.h
 * Notes: This validates the handle's packed shape, not whether it has since been closed; later calls reject a stale handle with JET_NET_E_ARG.
 * Example:
 *   JetUdp disco = jet_udp_open(LOBBY_PORT);
 *   if (!jet_udp_valid(disco)) { lobby_disable(); }
 */
static inline bool jet_udp_valid(JetUdp u) {
    return jet_net_handle_valid(u.id);
}

// ============================================================================
// MODULE-WIDE QUERIES
// ============================================================================

/**
 * Function: jet_net_available
 * Summary: Reports whether this runtime offers any networking at all.
 * Parameters:
 *   - None.
 * Returns: true when at least one JET_NET_CAP_* capability is present; false on a runtime with no network surface whatsoever.
 * Include: jet_net.h
 * Notes: The coarse question, for a cart whose whole feature is networked (a BBS terminal, a lobby browser) and which wants one early branch to a "desktop only" screen. When you need to know *which* transport you have, ask jet_net_capability instead.
 * Example:
 *   if (!jet_net_available()) { screen_show(SCREEN_DESKTOP_ONLY); return; }
 */
bool jet_net_available(void);

/**
 * Function: jet_net_capability
 * Summary: Tests whether every capability in a JET_NET_CAP_* mask is present on this runtime.
 * Parameters:
 *   - cap: One JET_NET_CAP_* bit, or several OR-ed together.
 * Returns: true only when ALL requested bits are supported; false if any is missing (and false for an empty mask).
 * Include: jet_net.h
 * Notes: This is the anti-surprise API — the honest capability matrix of docs/networking/design.md §3, readable at runtime. Guest-direct bits (TCP, TCP_LISTEN, UDP) are compile-time constants, so branching on them is free; the host-mediated bits (HTTP, HTTP_SERVE, WS) cost one read of the machine's NET CMD header, where the host published what it truly services. ASK BEFORE YOU DIAL: a web build has no raw sockets, and a cart that checks can offer a graceful alternative instead of a mystery failure.
 * Example:
 *   if (jet_net_capability(JET_NET_CAP_TCP)) phonebook_enable_dial();
 *   else                                     phonebook_mark_desktop_only();
 */
bool jet_net_capability(uint32_t cap);

/**
 * Function: jet_net_shutdown
 * Summary: Closes every open socket this module owns and stops its resolver worker.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_net.h
 * Notes: Optional — the runtime registers this to run automatically at process exit, so a cart that simply ends never leaks a socket or a thread. Call it explicitly from _end() when you want the teardown to happen at a moment you choose (an orderly BBS logoff, say), or between levels to reset the pools. Idempotent: calling it twice, or before any socket was ever opened, does nothing. Every handle taken before the call is stale afterwards.
 * Example:
 *   void _end(void) { jet_net_shutdown(); }
 */
void jet_net_shutdown(void);

// ============================================================================
// TCP — CLIENT
// ============================================================================

/**
 * Enum: JetTcpState
 * Summary: Where a TCP connection is in its life: still dialling, ready, finished, or broken.
 * Include: jet_net.h
 * Notes: Poll it from _update with jet_tcp_state. JET_TCP_CLOSED is an orderly shutdown by either side and is not an error; JET_TCP_FAILED means the attempt died, and jet_tcp_error() says why. The same four states describe a WebSocket in jet_ws.h, so a cart's session layer can drive either transport through one switch.
 * Example:
 *   if (jet_tcp_state(connection) == JET_TCP_OPEN) session_pump(connection);
 */
typedef enum {
    JET_TCP_CONNECTING = 0, // resolving and/or handshaking — poll again next tick
    JET_TCP_OPEN       = 1, // ready to send and receive
    JET_TCP_CLOSED     = 2, // orderly shutdown, by either side
    JET_TCP_FAILED     = 3, // see jet_tcp_error()
} JetTcpState;

/**
 * Function: jet_tcp_connect
 * Summary: Starts connecting to a host and port, and returns immediately — the dial happens over the following ticks.
 * Parameters:
 *   - host: Hostname or IPv4 dotted quad, NUL-terminated and shorter than JET_NET_HOST_MAX.
 *   - port: TCP port in host order (23 for telnet, 80 for HTTP, ...).
 * Returns: A handle whose state begins at JET_TCP_CONNECTING, or an invalid handle (see jet_tcp_valid) when the pool is full, the argument is malformed, or JET_NET_CAP_TCP is absent.
 * Include: jet_net.h
 * Notes: NOTHING BLOCKS HERE. Name resolution runs on a runtime-owned worker and the TCP handshake completes in the background; the cart polls jet_tcp_state from _update and starts sending once it reads JET_TCP_OPEN. A dotted quad skips DNS entirely. THE HOST IS ASKED FIRST: before dialling, the runtime asks the host whether this cart may reach that host at all — the project's allowlist in an exported player, and in the Workstation the one-time network prompt as well — so a dial can end JET_TCP_FAILED with jet_tcp_error() reporting JET_NET_E_DENIED, and the first one in a Workstation session stays JET_TCP_CONNECTING for as long as the person takes to answer — there is no timeout, so a cart that wants one counts its own ticks and closes the connection, which withdraws the question. Sockets are non-blocking with TCP_NODELAY set — a terminal wants keystrokes on the wire now, not Nagle-batched. Close every connection you open (jet_tcp_close) even after a failure; the slot is not reclaimed until you do.
 * Example:
 *   static JetTcp s_sock;
 *   void dial(void) { s_sock = jet_tcp_connect("particlesbbs.dyndns.org", 6400); }
 *   void _update(float dt) {
 *       if (jet_tcp_state(s_sock) == JET_TCP_OPEN) session_pump(s_sock);
 *   }
 */
JetTcp jet_tcp_connect(const char* host, uint16_t port);

/**
 * Function: jet_tcp_state
 * Summary: Advances a connection's progress and reports where it has got to.
 * Parameters:
 *   - t: The connection to poll.
 * Returns: A JetTcpState value, or JET_NET_E_ARG for a stale, forged, or never-assigned handle.
 * Include: jet_net.h
 * Notes: This is the pump as well as the query — calling it is what lets a pending resolve or handshake finish, so call it once per tick per connection while it is CONNECTING. jet_tcp_send and jet_tcp_recv pump the same machinery, so a cart that is already reading does not need a separate poll. The negative error value can never collide with a state, so `int s = jet_tcp_state(t); if (s < 0) ...` is safe.
 * Example:
 *   switch (jet_tcp_state(s_sock)) {
 *       case JET_TCP_CONNECTING: status_show("Connecting..."); break;
 *       case JET_TCP_OPEN:       session_pump(s_sock);         break;
 *       case JET_TCP_CLOSED:     status_show("NO CARRIER");    break;
 *       default:                 status_show("Failed");        break;
 *   }
 */
int jet_tcp_state(JetTcp t);

/**
 * Function: jet_tcp_error
 * Summary: Explains why a connection reached JET_TCP_FAILED.
 * Parameters:
 *   - t: The connection to ask.
 * Returns: The JetNetResult that ended it — typically JET_NET_E_RESOLVE, JET_NET_E_CONNECT, JET_NET_E_TIMEOUT or JET_NET_E_DENIED — or JET_NET_OK while the connection is still connecting or open, or JET_NET_E_ARG for a stale handle.
 * Include: jet_net.h
 * Notes: Worth surfacing verbatim: "host not found" and "connection refused" send a user to very different remedies, and a phonebook entry that has moved is indistinguishable from a board that is merely down unless the cart says which happened. A connection that reached JET_TCP_CLOSED reports JET_NET_E_CLOSED here, so the reason is available for every ending, not only for failures.
 * Example:
 *   if (jet_tcp_state(s_sock) == JET_TCP_FAILED) {
 *       status_show(jet_tcp_error(s_sock) == JET_NET_E_RESOLVE ? "HOST NOT FOUND" : "NO ANSWER");
 *   }
 */
int jet_tcp_error(JetTcp t);

/**
 * Function: jet_tcp_send
 * Summary: Hands bytes to the connection's send buffer, accepting as many as the socket will take right now.
 * Parameters:
 *   - t: An open connection.
 *   - buf: The bytes to send.
 *   - len: How many bytes to offer (must be > 0).
 * Returns: The number of bytes accepted, which MAY BE FEWER than len; 0 when the socket would block (including while it is still connecting); or a negative JetNetResult.
 * Include: jet_net.h
 * Notes: PARTIAL SENDS ARE THE CALLER'S CONTRACT, exactly as with POSIX — the runtime adds no hidden buffering on the send path, because a cart that needs a queue knows its own memory budget better than the runtime does. Advance your own cursor by the return value and offer the rest next tick. A cart sending small bursts (keystrokes, input packets) will effectively never see a short send.
 * Example:
 *   int n = jet_tcp_send(s_sock, out + sent, pending - sent);
 *   if (n > 0) sent += n;
 *   else if (n < 0) session_drop();
 */
int jet_tcp_send(JetTcp t, const void* buf, int len);

/**
 * Function: jet_tcp_recv
 * Summary: Copies whatever bytes have arrived into your buffer, without waiting for more.
 * Parameters:
 *   - t: An open connection.
 *   - buf: Destination for the bytes.
 *   - cap: Capacity of buf in bytes (must be > 0).
 * Returns: The number of bytes read (1..cap); 0 when nothing has arrived yet; JET_NET_E_CLOSED once the peer has closed and every buffered byte has been delivered; or another negative JetNetResult.
 * Include: jet_net.h
 * Notes: TCP is a stream, not a message queue: a single logical line may arrive across several ticks and several lines may arrive in one call, so feed the bytes to a parser that keeps its own state rather than assuming boundaries. Drain under a per-tick byte budget — a loop that reads until 0 will happily let a flooding peer starve the frame.
 * Example:
 *   uint8_t chunk[1024];
 *   int budget = RECV_BUDGET_PER_TICK;
 *   for (int n; budget > 0 && (n = jet_tcp_recv(s_sock, chunk, sizeof chunk)) > 0; budget -= n) {
 *       telnet_consume(chunk, n);
 *   }
 */
int jet_tcp_recv(JetTcp t, void* buf, int cap);

/**
 * Function: jet_tcp_close
 * Summary: Closes a connection and frees its pool slot.
 * Parameters:
 *   - t: The connection to close; a stale or invalid handle is ignored.
 * Returns: Nothing.
 * Include: jet_net.h
 * Notes: Idempotent and always safe, so it is the right thing to call on every exit path — including after a JET_TCP_FAILED dial, whose slot is otherwise held until you do. The handle goes permanently stale: the slot's generation is bumped, so a copy of the old handle can never address whatever connects next.
 * Example:
 *   void _end(void) { jet_tcp_close(s_sock); }
 */
void jet_tcp_close(JetTcp t);

// ============================================================================
// TCP — SERVER
// ============================================================================

/**
 * Function: jet_tcp_listen
 * Summary: Binds a port on every interface and starts accepting connections.
 * Parameters:
 *   - port: TCP port to bind in host order; must be non-zero.
 * Returns: A listener handle, or an invalid handle (see jet_tcp_listener_valid) when the port is taken, the pool is full, or JET_NET_CAP_TCP_LISTEN is absent.
 * Include: jet_net.h
 * Notes: Binds 0.0.0.0 with a backlog of JET_NET_LISTEN_BACKLOG, so the socket is reachable from the whole LAN — this is how a cart becomes a service other machines can dial (a household high-score board, a door game, the second-screen companion UI of jet_http.h's server). Browsers cannot listen, so this is honestly absent on the web player. Ports below 1024 need privileges the sandbox does not have; pick a high one.
 * Example:
 *   static JetTcpListener s_srv;
 *   void _init(void) { s_srv = jet_tcp_listen(SCORES_PORT); }
 */
JetTcpListener jet_tcp_listen(uint16_t port);

/**
 * Function: jet_tcp_accept
 * Summary: Takes one connection off a listener's queue, if anybody has dialled in.
 * Parameters:
 *   - l: The listener to check.
 * Returns: A handle to a connection already in JET_TCP_OPEN, or an invalid handle when nobody is waiting (the common case), the connection pool is full, or the listener handle is stale.
 * Include: jet_net.h
 * Notes: Non-blocking, so call it each tick and simply move on when it comes back invalid — "nobody is waiting" and "the pool is full" are both reported the same way because both mean "no new connection this tick". Accepted connections arrive non-blocking with TCP_NODELAY set, identical to a jet_tcp_connect socket, and each one occupies a JET_NET_MAX_TCP slot until you close it.
 * Example:
 *   for (JetTcp c = jet_tcp_accept(s_srv); jet_tcp_valid(c); c = jet_tcp_accept(s_srv)) {
 *       clients_add(c);
 *   }
 */
JetTcp jet_tcp_accept(JetTcpListener l);

/**
 * Function: jet_tcp_listener_close
 * Summary: Stops listening and frees the listener's pool slot.
 * Parameters:
 *   - l: The listener to close; a stale or invalid handle is ignored.
 * Returns: Nothing.
 * Include: jet_net.h
 * Notes: Idempotent. Connections already accepted are NOT closed with it — they are independent JetTcp handles with their own lifetimes, so close them yourself (or call jet_net_shutdown to end everything at once).
 * Example:
 *   void _end(void) { jet_tcp_listener_close(s_srv); }
 */
void jet_tcp_listener_close(JetTcpListener l);

// ============================================================================
// UDP
// ============================================================================

/**
 * Function: jet_udp_open
 * Summary: Opens a UDP socket, optionally on a fixed port.
 * Parameters:
 *   - port: Port to bind in host order, or 0 to let the OS pick an ephemeral one.
 * Returns: A socket handle, or an invalid handle (see jet_udp_valid) when the port is taken, the pool is full, or JET_NET_CAP_UDP is absent.
 * Include: jet_net.h
 * Notes: Pass 0 for a client that only ever talks to a peer it already knows; pass a fixed, agreed port for anything peers must find — a discovery beacon or a game server. A fixed port is bound with address reuse enabled, so two copies of the cart on one machine can both discover and play, which is exactly how you test LAN play without a second computer.
 * Example:
 *   static JetUdp s_disco;
 *   void _init(void) { s_disco = jet_udp_open(LOBBY_PORT); }
 */
JetUdp jet_udp_open(uint16_t port);

/**
 * Function: jet_udp_send_to
 * Summary: Sends one datagram to one peer.
 * Parameters:
 *   - u: An open UDP socket.
 *   - to: The destination address and port.
 *   - buf: The datagram payload.
 *   - len: Payload length in bytes (must be > 0).
 * Returns: The number of bytes sent (always the whole datagram); 0 when nothing was sent this tick — the socket would block, or the host is still being asked about this address, and either way the datagram is dropped, not queued; JET_NET_E_DENIED when the host refused the address; JET_NET_E_ARG for a malformed argument, including port 0; or another negative JetNetResult.
 * Include: jet_net.h
 * Notes: A datagram is all-or-nothing — there is no partial send to resume, unlike jet_tcp_send. UDP gives no delivery or ordering guarantee, which is exactly why it suits per-tick game state: put a tick number in your own packet header and drop what arrives late. Keep payloads under about 1200 bytes so nothing fragments on a normal LAN. Broadcasting needs jet_udp_set_broadcast first. THE HOST IS ASKED FIRST, ONCE PER ADDRESS: a send to an address the socket has not heard from returns 0 and DROPS the datagram while the host checks the project's allowlist (and, in the Workstation, the one-time network prompt, for as long as it is up) — send it again next tick, as UDP carts already resend; once answered, every send goes straight out, or is JET_NET_E_DENIED. The answer is kept for the address, whatever the port, and a socket keeps answers for a handful of addresses, so a cart talking to many more peers than that is asked again, a few ticks' delay, about one it was answered for long ago. Replying to an address that sent to this socket is never asked about, because it is an answer rather than a dial — and that, too, is keyed by the ADDRESS, not the port: once any datagram from an address arrives (a beacon jet_net_lan_poll returned included), this socket may send to every port at that address unasked. Neither is 255.255.255.255 or jet_net_lan_announce, which reach only the local network.
 * Example:
 *   JetNetEndpoint peer = { "192.168.1.42", LOBBY_PORT };
 *   jet_udp_send_to(s_sock, &peer, &input_packet, sizeof input_packet);
 */
int jet_udp_send_to(JetUdp u, const JetNetEndpoint* to, const void* buf, int len);

/**
 * Function: jet_udp_recv_from
 * Summary: Takes one whole datagram off the socket, and says who sent it.
 * Parameters:
 *   - u: An open UDP socket.
 *   - buf: Destination for the payload.
 *   - cap: Capacity of buf in bytes (must be > 0).
 *   - from: Filled in with the sender's address and port; pass NULL if you do not care.
 * Returns: The datagram's length in bytes (1..cap); 0 when none is pending; JET_NET_E_LIMIT when the datagram was larger than cap (the first cap bytes are still copied and `from` is filled, but the remainder is gone); or another negative JetNetResult.
 * Include: jet_net.h
 * Notes: One call returns exactly one datagram — never a fragment and never two spliced together, which is the whole reason to prefer UDP for packetised game traffic. JET_NET_E_LIMIT is a design bug in the making, not a transient condition: size your buffer to your protocol's largest packet and treat the error as "my buffer is too small". (The true length of an oversized datagram is not available on every platform, so the module reports the overflow honestly rather than pretending to a number it cannot always know.) Drain under a per-tick budget, as with TCP.
 * Example:
 *   InputPacket pkt; JetNetEndpoint from;
 *   for (int n; (n = jet_udp_recv_from(s_sock, &pkt, sizeof pkt, &from)) > 0; ) {
 *       if (n == (int)sizeof pkt) remote_input_apply(&from, &pkt);
 *   }
 */
int jet_udp_recv_from(JetUdp u, void* buf, int cap, JetNetEndpoint* from);

/**
 * Function: jet_udp_set_broadcast
 * Summary: Permits (or forbids) this socket to send to a broadcast address.
 * Parameters:
 *   - u: An open UDP socket.
 *   - enable: true to allow broadcast sends, false to forbid them again.
 * Returns: JET_NET_OK, or a negative JetNetResult.
 * Include: jet_net.h
 * Notes: Broadcast is off by default because the OS insists a program say out loud that it means to shout at the whole subnet — jet_net_lan_announce needs this enabled first and fails cleanly if you forget. It affects only sending; receiving broadcasts needs nothing beyond binding the agreed port.
 * Example:
 *   jet_udp_set_broadcast(s_disco, true);
 */
int jet_udp_set_broadcast(JetUdp u, bool enable);

/**
 * Function: jet_udp_close
 * Summary: Closes a UDP socket and frees its pool slot.
 * Parameters:
 *   - u: The socket to close; a stale or invalid handle is ignored.
 * Returns: Nothing.
 * Include: jet_net.h
 * Notes: Idempotent. The handle goes permanently stale, so a copy can never address the next socket to occupy the slot.
 * Example:
 *   void _end(void) { jet_udp_close(s_disco); }
 */
void jet_udp_close(JetUdp u);

// ============================================================================
// LAN DISCOVERY
// ============================================================================
//
// Finding a peer on a home network is fiddly in ways that have nothing to do
// with your game — the broadcast address, your own echo coming back, validating
// a frame somebody else's software sent to the same port. Leaving every cart to
// reinvent that would be the definition of a DRY violation across an ecosystem,
// so the two helpers below standardise one small beacon frame and let the cart
// own the only part that is actually its business: the payload.
//
// THE BEACON WIRE FORMAT (all multi-byte fields little-endian, as everywhere in
// Jet64) — published so a companion tool in any language can join in:
//
//   offset  size  field
//   0       4     magic "J64D"
//   4       2     format version (JET_NET_LAN_FRAME_VERSION)
//   6       2     payload length (0..JET_NET_LAN_PAYLOAD_MAX)
//   8       4     game_id — the cart's own identifier, so two Jet64 games
//                 sharing a port never see each other's lobbies
//   12      4     source token — opaque per-socket value used only to drop your
//                 own echo. A receiver ignores a frame carrying its own token;
//                 the reserved value 0 means "no token" and always passes.
//   16      ...   payload (the cart's business: lobby name, player count, a
//                 protocol version of its own, whatever it needs)

#define JET_NET_LAN_MAGIC         "J64D" // 4 bytes, no NUL on the wire
#define JET_NET_LAN_MAGIC_BYTES   4u
#define JET_NET_LAN_FRAME_VERSION 1u
#define JET_NET_LAN_HEADER_BYTES  16u

/**
 * Constant: JET_NET_LAN_PAYLOAD_MAX
 * Summary: Largest application payload carried by one Jet64 LAN discovery announcement.
 * Include: jet_net.h
 * Notes: The bound keeps the whole beacon well below the smallest Ethernet MTU so it is not fragmented on a home network. Use it for lobby metadata, not gameplay snapshots; jet_net_lan_announce rejects a larger payload.
 * Example:
 *   uint8_t beacon[JET_NET_LAN_PAYLOAD_MAX];
 */
#define JET_NET_LAN_PAYLOAD_MAX 400u

// Whole-frame size, for a buffer that must always hold one beacon.
#define JET_NET_LAN_FRAME_MAX (JET_NET_LAN_HEADER_BYTES + JET_NET_LAN_PAYLOAD_MAX)

// How many non-matching datagrams one jet_net_lan_poll call will discard while
// hunting for a beacon. Bounded so a flood of junk on the discovery port costs a
// fixed slice of the tick instead of stalling the frame.
#define JET_NET_LAN_POLL_MAX_DROPS 32

/**
 * Function: jet_net_lan_announce
 * Summary: Broadcasts one "here I am" beacon to every machine on the local subnet.
 * Parameters:
 *   - u: A UDP socket bound to the agreed discovery port, with broadcast enabled.
 *   - game_id: Your cart's own identifier, so only your game's beacons match.
 *   - payload: Whatever you want peers to see — lobby name, player count, protocol version. May be NULL when len is 0.
 *   - len: Payload length, 0..JET_NET_LAN_PAYLOAD_MAX.
 * Returns: JET_NET_OK when the beacon went out; JET_NET_E_LIMIT when the payload is too big; JET_NET_E_ARG for a stale handle or a malformed argument; or another negative JetNetResult (notably when broadcast was never enabled).
 * Include: jet_net.h
 * Notes: Sent to the broadcast address of every live network this machine is on, at the socket's OWN bound port — so every peer must open the same agreed port, and that shared number is the entire discovery protocol. PACE THE CALLS YOURSELF: one beacon every 30 ticks (four a second) makes a lobby feel instant while staying invisible on the network; one per tick is 120 broadcasts a second and merely rude. Enable broadcast with jet_udp_set_broadcast before the first call, or the OS refuses with JET_NET_E_DENIED. ONE MACHINE IS NOT A LAN: several stacks (macOS among them) never deliver a broadcast back to sockets on the sending machine, so two copies of a cart on ONE computer will not find each other by beacon — test discovery across two machines, and give the cart a way to enter an address by hand for everything else.
 * Example:
 *   if (++s_beacon_tick >= LOBBY_BEACON_INTERVAL_TICKS) {
 *       s_beacon_tick = 0;
 *       jet_net_lan_announce(s_disco, MY_GAME_ID, &s_lobby, sizeof s_lobby);
 *   }
 */
int jet_net_lan_announce(JetUdp u, uint32_t game_id, const void* payload, int len);

/**
 * Function: jet_net_lan_poll
 * Summary: Takes the next beacon from another copy of your game off the socket, discarding anything that is not one.
 * Parameters:
 *   - u: The UDP socket bound to the discovery port.
 *   - game_id: The same identifier you announce with; beacons from other games are dropped.
 *   - from: Filled in with the announcing peer's address and port — the address you then connect to. Pass NULL if you do not need it.
 *   - payload: Destination for the peer's payload.
 *   - cap: Capacity of payload in bytes.
 * Returns: The matching beacon's payload length (0..cap); 0 when nothing is pending; JET_NET_E_LIMIT when the beacon's payload exceeds cap (the beacon is dropped); JET_NET_E_ARG for a stale handle; or another negative JetNetResult.
 * Include: jet_net.h
 * Notes: "Nothing pending" and "a beacon with an empty payload" share the return value 0 — harmless, because the datagram is consumed either way, so a drain loop that stops at 0 simply resumes next tick. Give your beacon a payload (even one version byte) and 0 means exactly "no news". Call it in a loop each tick; each call yields at most one beacon. Your OWN announcements are filtered out, so a lobby list never lists you twice. USE A DEDICATED SOCKET FOR DISCOVERY: this call consumes and discards up to JET_NET_LAN_POLL_MAX_DROPS non-beacon datagrams while hunting, which would eat game traffic sharing the same socket. Peers go quiet without saying so, so age your lobby list by ticks-since-last-beacon rather than expecting a goodbye.
 * Example:
 *   LobbyInfo info; JetNetEndpoint from;
 *   for (int n; (n = jet_net_lan_poll(s_disco, MY_GAME_ID, &from, &info, sizeof info)) >= 0; ) {
 *       if (n == 0) break;                       // nothing more pending
 *       if (n == (int)sizeof info) lobby_seen(&from, &info);
 *   }
 */
int jet_net_lan_poll(JetUdp u, uint32_t game_id, JetNetEndpoint* from, void* payload, int cap);

// ============================================================================
// RUNTIME-INTERNAL — not part of the cart-facing API
// ============================================================================
//
// The host-mediated modules (jet_http.c, jet_ws.c) all speak through the one NET
// CMD block, and all need the same two questions answered before they touch it: is
// the block there, and is it a layout we understand? Answering that in each module
// would be the same six lines three times, so it lives here — beneath the covenant
// line, prefixed like every other runtime internal, and of no use to a cart, which
// asks jet_net_capability() instead.

/// Pack a slot index and its generation into the cart-visible handle id. Every
/// pool in every networking module packs handles the same way — that is what makes
/// jet_net_handle_valid above meaningful for all of them — so the arithmetic lives
/// here once rather than being re-derived per module.
// Internal: Runtime implementation only; omitted from the creator-facing Codex.
static inline int32_t _jet_net_handle_pack(uint32_t index, uint32_t generation) {
    return (int32_t)((generation << JET_NET_HANDLE_INDEX_BITS) | index);
}

/// Split a handle id, rejecting anything that could not have been packed. Proves
/// only that the id is WELL-FORMED; whether the slot it names is still the one the
/// handle was taken from is the owning pool's generation check.
// Internal: Runtime implementation only; omitted from the creator-facing Codex.
static inline bool _jet_net_handle_split(int32_t id, uint32_t* out_index,
                                         uint32_t* out_generation) {
    if (!jet_net_handle_valid(id)) {
        return false;
    }
    *out_index = (uint32_t)id & JET_NET_HANDLE_INDEX_MASK;
    *out_generation = (uint32_t)id >> JET_NET_HANDLE_INDEX_BITS;
    return true;
}

/// The generation a slot takes on its next occupancy. Wraps to 1 rather than 0,
/// because generation 0 is what makes a zero-initialised handle invalid.
// Internal: Runtime implementation only; omitted from the creator-facing Codex.
static inline uint32_t _jet_net_handle_next_generation(uint32_t generation) {
    return generation >= JET_NET_HANDLE_GEN_MAX ? 1u : generation + 1u;
}

/// Milliseconds on a monotonic clock — one that only ever moves forward, and is
/// therefore immune to the wall clock being set, to daylight saving, and to NTP.
/// The absolute value is meaningless; only DIFFERENCES are, which is exactly what
/// a deadline is.
///
/// This is not a cart-facing timekeeper and must never become one: a cart measures
/// time in the machine's own currency — the fixed 120 Hz tick and the `dt` handed
/// to _update — because that is what keeps behaviour identical on every host. What
/// needs a real clock is the runtime's own guest-direct WATCHDOGS, where the whole
/// point is to survive a peer that has stopped calling anything at all
/// (jet_http.c's server hangs up on a half-sent request), and where counting a
/// cart's polls would let a caller's own loop shape decide when a stranger's
/// connection times out.
// Internal: Runtime implementation only; omitted from the creator-facing Codex.
uint64_t _jet_net_monotonic_ms(void);

/// The NET CMD block base, or NULL when this host publishes no such block (an
/// older player, a build without the service) or stamped a version this runtime
/// does not know. A NULL answer is the honest "no host-mediated networking here",
/// and every caller treats it as JET_NET_E_UNSUPPORTED.
// Internal: Runtime implementation only; omitted from the creator-facing Codex.
volatile uint8_t* _jet_net_cmd_block(void);

/// Convert a buffer the HOST must read or write into a motherboard-relative
/// offset, proving `[buffer, buffer+len)` lies inside WRAM. False means it does
/// not — most often because the cart passed a `static uint8_t buf[N]`, which lives
/// in the cart's own binary where the host cannot see it at all.
///
/// Header-inline and log-free on purpose: the arithmetic is identical for every
/// host-mediated module and belongs in one place, while the diagnostic that
/// follows a refusal is the caller's, because "the response buffer" and "the
/// socket arena" send an author to different lines of their cart. The host
/// re-validates the offset before it dereferences — a sandbox-side check is a
/// courtesy to the cart author, never a security guarantee for the host.
// Internal: Runtime implementation only; omitted from the creator-facing Codex.
static inline bool _jet_net_wram_offset(const void* buffer, uint32_t len, uint32_t* out_offset) {
    const uintptr_t base = (uintptr_t)JET_BASE;
    const uintptr_t p = (uintptr_t)buffer;
    const uintptr_t wram_lo = base + JET_WRAM_OFFSET;
    const uintptr_t wram_hi = base + JET_SHM_SIZE;

    if (JET_BASE == NULL || buffer == NULL || len == 0u || p < wram_lo || p > wram_hi - len) {
        return false;
    }
    *out_offset = (uint32_t)(p - base);
    return true;
}

#endif // JET_NET_H
