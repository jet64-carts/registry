// runtime/jet_http_serve.h
#ifndef JET_HTTP_SERVE_H
#define JET_HTTP_SERVE_H

// ============================================================================
// Jet64 HTTP server, a cartridge that answers the phone (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_net.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_http_serve.c only
// when a cart's #include tree reaches this header.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_http_serve.h"    // <-- explicit opt-in
//
// Run a small web server inside a native build of your cartridge, so that
// browsers and other machines on your own local network, your LAN, can send
// requests to your game and get answers back.
//
// <nocodex>
// ----------------------------------------------------------------------------
// WHY THIS IS ITS OWN MODULE, AND NOT HALF OF jet_http.h
// ----------------------------------------------------------------------------
// The two directions of HTTP have nothing in common but the letters. The CLIENT
// (jet_http.h) is host-mediated because `https://` means TLS, and reaching it
// makes the exported player compile `ureq` and `rustls` — one and a half
// megabytes of them. The SERVER is guest-direct: jet_tcp_listen plus a strict,
// allocation-free parser, entirely inside the sandbox, with no host layer and no
// TLS at all. The include tree is Jet64's billing meter (design §12), so the two
// are separately metered: a cart that only SERVES compiles no TLS stack, and a
// cart that only FETCHES compiles no socket code. Neither pays for the other.
// </nocodex>
//
// ----------------------------------------------------------------------------
// WHAT IT IS FOR
// ----------------------------------------------------------------------------
// A second screen: a phone's browser opens a page that the running game itself
// serves, with a level select, a soft keyboard or a map on it. A household
// high-score board that any machine on the LAN can POST a score to. A live page
// showing what the game is doing right now. A door game that other players dial
// into. See docs/networking/design.md sections 5.5 and 10 for the whole shape
// of one.
//
// ----------------------------------------------------------------------------
// NATIVE ONLY, AND IT SAYS SO
// ----------------------------------------------------------------------------
// Native means the workstation and the native player, not the web player. A
// browser cannot listen for connections coming in, so a web build cannot run a
// server. Ask jet_net_capability(JET_NET_CAP_HTTP_SERVE) before you start: it
// answers false on the web player, where jet_http_serve gives you an invalid
// handle. When browser players must be able to reach you, use jet_ws.h and a
// relay instead.
//
// <nocodex>
// A browser cannot listen — there is no API at any privilege level that lets a
// page accept an inbound connection, and there never will be. So on the web
// player this whole surface is honestly ABSENT rather than emulated:
// jet_net_capability(JET_NET_CAP_HTTP_SERVE) reports false, jet_http_serve
// returns an invalid handle, and a cart that asked first can say so plainly
// instead of failing mysteriously later. A cart that wants to be reachable from
// a browser reaches for jet_ws.h and a relay both sides connect out to.
// </nocodex>
//
// ----------------------------------------------------------------------------
// PLAINTEXT, ON YOUR OWN LAN, AND NOTHING MORE
// ----------------------------------------------------------------------------
// Nothing this server sends is encrypted (there is no TLS), and nobody has to
// log in (there is no authentication). Use it only on a network you trust,
// never open it to the internet, and treat every request as something a
// stranger typed. Fixed limits on requests and connections, and two watchdogs
// that hang up on a peer that stalls, keep abuse within bounds.
//
// <nocodex>
// There is no TLS here and no authentication of any kind. Plaintext on a home
// network is period-authentic and honest — it is what the machines Jet64 honours
// would have done — but do not put this on the open internet, and do not accept
// anything through it you would mind a neighbour sending. Bounds are visible
// everywhere: JET_HTTP_SERVE_MAX_CONNECTIONS clients, JET_HTTP_SERVE_REQUEST_MAX
// bodies, and two watchdogs that hang up on a peer which stalls.
// </nocodex>
//
// Complete LAN hello-server example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_http_serve.h"
//
//   enum { SERVER_PORT = 8080 };
//   static JetHttpServer server;
//
//   void _init(void) {
//       server = jet_http_serve(SERVER_PORT);
//   }
//
//   void _update(float dt) {
//       static const char body[] = "Hello from Jet64!\n";
//       JetHttpServerRequest request;
//       (void)dt;
//       while (jet_http_serve_poll(server, &request)) {
//           jet_http_serve_respond(
//               server, request.id, JET_HTTP_STATUS_OK, JET_HTTP_CONTENT_TEXT,
//               body, (uint32_t)(sizeof body - 1)
//           );
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(jet_http_serve_valid(server) ? "OPEN PORT 8080" : "SERVER UNAVAILABLE", 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_http_serve_shutdown();
//   }

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool (sandbox-safe)
#include "jet_net.h"  // the listening socket underneath, and the shared result vocabulary

// ============================================================================
// LIMITS (every bound named once)
// ============================================================================

/**
 * Constant: JET_HTTP_MAX_SERVERS
 * Summary: The most HTTP servers one cartridge can run at once.
 * Include: jet_http_serve.h
 * Notes: One server is how it is meant to be used: send different paths through the one server rather than opening several ports. While the first server is running, a second jet_http_serve call gives you an invalid handle.
 * Example:
 *   JetHttpServer servers[JET_HTTP_MAX_SERVERS] = {0};
 */
#define JET_HTTP_MAX_SERVERS 1

/**
 * Constant: JET_HTTP_SERVE_MAX_CONNECTIONS
 * Summary: The most clients the server can be talking to at once.
 * Include: jet_http_serve.h
 * Notes: These clients share the pool of JET_NET_MAX_TCP connections with the cartridge's own outgoing and accepted raw TCP connections. Extra clients wait in the listener's queue, its backlog, until a slot comes free.
 * Example:
 *   uint32_t client_capacity = JET_HTTP_SERVE_MAX_CONNECTIONS;
 */
#define JET_HTTP_SERVE_MAX_CONNECTIONS JET_NET_MAX_TCP

/**
 * Constant: JET_HTTP_SERVE_REQUEST_MAX
 * Summary: The largest request body the server accepts, in bytes.
 * Include: jet_http_serve.h
 * Notes: A request that announces a larger body is answered with status 413 and never reaches your code. The server owns the memory a request is received into; JetHttpServerRequest.body is a borrowed view into it, a pointer you may read but do not own.
 * Example:
 *   if (request.body_len > JET_HTTP_SERVE_REQUEST_MAX) return;
 */
#define JET_HTTP_SERVE_REQUEST_MAX 65536u

/**
 * Constant: JET_HTTP_SERVE_RESPONSE_MAX
 * Summary: The largest response body the server will send, in bytes.
 * Include: jet_http_serve.h
 * Notes: jet_http_serve_respond refuses a larger body rather than cutting it short. The request and response limits are the same because each connection slot re-uses one fixed buffer for both.
 * Example:
 *   uint8_t response[JET_HTTP_SERVE_RESPONSE_MAX];
 */
#define JET_HTTP_SERVE_RESPONSE_MAX JET_HTTP_SERVE_REQUEST_MAX

/**
 * Constant: JET_HTTP_SERVE_HEAD_MAX
 * Summary: The largest request line and header block the server accepts, in bytes.
 * Include: jet_http_serve.h
 * Notes: Anything over this is answered with status 431. It is roomy enough for a browser, while stopping a peer from making the cartridge use more and more memory.
 * Example:
 *   uint32_t maximum_request_head = JET_HTTP_SERVE_HEAD_MAX;
 */
#define JET_HTTP_SERVE_HEAD_MAX 8192u

/**
 * Constant: JET_HTTP_SERVE_TARGET_MAX
 * Summary: The largest method, path and query the server accepts, in bytes, all three together.
 * Include: jet_http_serve.h
 * Notes: A longer target is answered with status 414. JetHttpServerRequest.method, path and query are borrowed views carved out of one buffer of this size.
 * Example:
 *   uint32_t maximum_target = JET_HTTP_SERVE_TARGET_MAX;
 */
#define JET_HTTP_SERVE_TARGET_MAX 2048u

/**
 * Constant: JET_HTTP_SERVE_HEAD_OUT_MAX
 * Summary: The room set aside for each connection's response status line and headers, in bytes.
 * Include: jet_http_serve.h
 * Notes: It is public because it explains part of JET_HTTP_SERVE_WRAM_BYTES. A content type of your own must fit in here beside the status, length and connection headers the server writes, or jet_http_serve_respond refuses it.
 * Example:
 *   uint32_t response_header_budget = JET_HTTP_SERVE_HEAD_OUT_MAX;
 */
#define JET_HTTP_SERVE_HEAD_OUT_MAX 256u

/**
 * Constant: JET_HTTP_SERVE_WRAM_BYTES
 * Summary: How much WRAM the server takes, for good, the first time a cartridge starts one.
 * Include: jet_http_serve.h
 * Notes: WRAM is the cartridge's working memory. Its allocator only ever hands memory out and never takes it back (a bump allocator), so closing the server does not free this; it only lets a later server re-use it. Count this exact number in your cartridge's WRAM budget at startup.
 * Example:
 *   LOG_INFO("HTTP server reserves %u WRAM bytes", JET_HTTP_SERVE_WRAM_BYTES);
 */
#define JET_HTTP_SERVE_WRAM_BYTES                                                         \
    ((uint32_t)JET_HTTP_SERVE_MAX_CONNECTIONS                                             \
     * (JET_HTTP_SERVE_REQUEST_MAX + JET_HTTP_SERVE_TARGET_MAX + JET_HTTP_SERVE_HEAD_OUT_MAX))

// How long a connection may make no progress at all, with no byte read, no byte
// written and no answer from the cartridge, before the server hangs up. This is
// what stops a peer that opens a socket and then goes silent from holding a
// slot.
//
// The two watchdog deadlines are the module's only build-time overrides, and
// they exist for one reason: tests/jet_http_serve/harness.c must PROVE that a
// timeout fires, and waiting out the shipped deadlines would make that suite a
// minute of sleeping. A cartridge cannot reach the override, because
// jet_http_serve.c is a separate translation unit (a C file compiled on its own)
// that the guest compiler builds with no cart-supplied defines, so these numbers
// are what every cartridge gets.
#ifndef JET_HTTP_SERVE_IDLE_MS
/**
 * Constant: JET_HTTP_SERVE_IDLE_MS
 * Summary: How many milliseconds a connection may make no progress before the server closes it.
 * Include: jet_http_serve.h
 * Notes: It protects the fixed pool of connections from a peer that connects and then goes silent. The number is built into the runtime; a cartridge cannot change it.
 * Example:
 *   uint32_t idle_deadline_ms = JET_HTTP_SERVE_IDLE_MS;
 */
#define JET_HTTP_SERVE_IDLE_MS 10000u
#endif

// How long a whole request may take to arrive, from the moment the connection
// is accepted to its last body byte. The idle deadline above cannot catch a
// "slowloris", a peer that dribbles one byte every few seconds precisely so it
// looks alive; this one can.
#ifndef JET_HTTP_SERVE_REQUEST_MS
/**
 * Constant: JET_HTTP_SERVE_REQUEST_MS
 * Summary: The most milliseconds a whole request may take to arrive.
 * Include: jet_http_serve.h
 * Notes: It works alongside the idle deadline by stopping a slowloris, a peer that sends a byte now and then without ever finishing its request. The number is built into the runtime; a cartridge cannot change it.
 * Example:
 *   uint32_t request_deadline_ms = JET_HTTP_SERVE_REQUEST_MS;
 */
#define JET_HTTP_SERVE_REQUEST_MS 30000u
#endif

/**
 * Constant: JET_HTTP_SERVE_LINGER_MS
 * Summary: The longest the server spends reading and discarding the rest of a request it has refused, in milliseconds, before closing the connection cleanly.
 * Include: jet_http_serve.h
 * Notes: Without this, upload bytes left unread would turn a useful 4xx answer into a connection reset. The wait ends the moment there is nothing left to discard, so ordinary responses do not wait this long.
 * Example:
 *   uint32_t refusal_linger_ms = JET_HTTP_SERVE_LINGER_MS;
 */
#define JET_HTTP_SERVE_LINGER_MS 1000u

/**
 * Constant: JET_HTTP_CONTENT_JSON
 * Summary: The standard content type for JSON, application/json, ready to pass to jet_http_serve_respond.
 * Include: jet_http_serve.h
 * Notes: Pass it when the response body is JSON. Any other valid content-type string is also accepted.
 * Example:
 *   jet_http_serve_respond(server, request.id, JET_HTTP_STATUS_OK, JET_HTTP_CONTENT_JSON, json, json_len);
 */
#define JET_HTTP_CONTENT_JSON "application/json"

/**
 * Constant: JET_HTTP_CONTENT_TEXT
 * Summary: The content type for plain UTF-8 text, ready to pass to jet_http_serve_respond.
 * Include: jet_http_serve.h
 * Notes: It names the UTF-8 character set as well, so browsers decode ordinary cartridge text the same way every time.
 * Example:
 *   jet_http_serve_respond(server, request.id, JET_HTTP_STATUS_OK, JET_HTTP_CONTENT_TEXT, message, message_len);
 */
#define JET_HTTP_CONTENT_TEXT "text/plain; charset=utf-8"

/**
 * Constant: JET_HTTP_CONTENT_HTML
 * Summary: The content type for a UTF-8 HTML page, ready to pass to jet_http_serve_respond.
 * Include: jet_http_serve.h
 * Notes: Use it for companion pages meant to show in a browser. The module neither writes nor cleans up HTML; the body you give it is sent exactly as it is.
 * Example:
 *   jet_http_serve_respond(server, request.id, JET_HTTP_STATUS_OK, JET_HTTP_CONTENT_HTML, page, page_len);
 */
#define JET_HTTP_CONTENT_HTML "text/html; charset=utf-8"

// ============================================================================
// THE SURFACE
// ============================================================================
//
// THE SHAPE OF IT. Start serving in _init, drain the requests in _update, and
// answer every one of them:
//
//   void _init(void)       { s_srv = jet_http_serve(SCORES_PORT); }
//   void _update(float dt) {
//       JetHttpServerRequest req;
//       while (jet_http_serve_poll(s_srv, &req)) route(&req);   // route() responds
//   }
//   void _end(void)        { jet_http_serve_close(s_srv); }
//
// WHAT IT DOES NOT DO, ON PURPOSE. One request per connection (every response
// carries `Connection: close`), so there is no keep-alive and no pipelining to
// think about. No chunked transfer encoding on the way in; a request that asks
// for it is answered 501. No TLS, no compression, no serving of files from disk,
// and no extra response headers in v1. Each of those could be added later
// without breaking anything, and none is needed by the LAN services this exists
// to make possible.

/**
 * Struct: JetHttpServer
 * Summary: A handle to one listening HTTP server. A handle is a small value that stands for a thing the runtime owns.
 * Fields:
 *   - id: The slot number and its generation stamp, packed into one integer. Keep it, pass it back, do not look inside; test it with jet_http_serve_valid.
 * Include: jet_http_serve.h
 * Notes: Copy it freely; it is one integer. Once the server is closed, its handle is stale for good. A handle that is all zeros (`static JetHttpServer s_srv;`) is safely invalid, so you can poll one before you have ever started a server.
 * Example:
 *   JetHttpServer server = jet_http_serve(8080);
 */
typedef struct {
    int32_t id;
} JetHttpServer;

/**
 * Enum: JetHttpStatus
 * Summary: Names for the HTTP status codes a cartridge actually answers with, so no route has to spell out a bare number.
 * Include: jet_http_serve.h
 * Notes: jet_http_serve_respond takes a plain int, so any code you need is allowed; these are the ones worth naming. The server answers 400, 408, 413, 414, 431, 501 and 505 by itself, for requests that are malformed, stalled or too big, with the code's reason phrase as the body; those requests never reach your code.
 * Example:
 *   jet_http_serve_respond(server, request.id, JET_HTTP_STATUS_OK, JET_HTTP_CONTENT_JSON, json, json_len);
 */
typedef enum {
    JET_HTTP_STATUS_OK                    = 200, // it worked, and here is the answer
    JET_HTTP_STATUS_CREATED               = 201, // it worked, and something new now exists
    JET_HTTP_STATUS_NO_CONTENT            = 204, // it worked, and there is nothing to send back
    JET_HTTP_STATUS_BAD_REQUEST           = 400, // the request made no sense
    JET_HTTP_STATUS_FORBIDDEN             = 403, // understood, but not allowed
    JET_HTTP_STATUS_NOT_FOUND             = 404, // there is nothing at that path
    JET_HTTP_STATUS_METHOD_NOT_ALLOWED    = 405, // that verb is not accepted at that path
    JET_HTTP_STATUS_CONFLICT              = 409, // it clashes with the current state of things
    JET_HTTP_STATUS_PAYLOAD_TOO_LARGE     = 413, // the body is bigger than you accept
    JET_HTTP_STATUS_URI_TOO_LONG          = 414, // the path and query are too long
    JET_HTTP_STATUS_UNPROCESSABLE         = 422, // well formed, but the contents cannot be acted on
    JET_HTTP_STATUS_HEADERS_TOO_LARGE     = 431, // the headers are too big
    JET_HTTP_STATUS_INTERNAL_SERVER_ERROR = 500, // something went wrong on your side
    JET_HTTP_STATUS_NOT_IMPLEMENTED       = 501, // you do not support what was asked for
} JetHttpStatus;

/**
 * Struct: JetHttpServerRequest
 * Summary: One request, fully parsed, handed to you as borrowed views into the server's own memory: pointers you may read but do not own.
 * Fields:
 *   - id: The token you pass to jet_http_serve_respond. Never 0 for a request you were given.
 *   - method: The verb, in capitals and ending in a NUL: "GET", "POST", "PUT", ... exactly as it was sent.
 *   - path: The path with its percent-escapes DECODED (a %20 turned back into a space, and so on), ending in a NUL and always starting with '/', for example "/scores".
 *   - query: The query string, still encoded and without its '?', ending in a NUL; "" when there was none.
 *   - body: The request body, or NULL when there was none. It does NOT end in a NUL, because it may be binary.
 *   - body_len: The body's length in bytes; 0 when there was none.
 * Include: jet_http_serve.h
 * Notes: EVERY VIEW IS BORROWED. They point into the server's buffers and stay valid only until your next call to jet_http_serve_poll or jet_http_serve_respond, so copy anything you mean to keep.
 *
 *        THE PATH IS DECODED, NOT MADE SAFE. It is whatever a stranger on your LAN typed, so never hand it to jet_fopen or build a filename from it; route by comparing it with strcmp against paths you chose. `query` is left encoded because splitting it into key=value pairs is your business, and every cartridge wants it done differently.
 * Example:
 *   JetHttpServerRequest req;
 *   while (jet_http_serve_poll(s_srv, &req)) {
 *       if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/scores") == 0) send_scores(req.id);
 *       else jet_http_serve_respond(s_srv, req.id, JET_HTTP_STATUS_NOT_FOUND, JET_HTTP_CONTENT_TEXT, NULL, 0);
 *   }
 */
typedef struct {
    uint32_t    id;
    const char* method;
    const char* path;
    const char* query;
    const void* body;
    uint32_t    body_len;
} JetHttpServerRequest;

/**
 * Function: jet_http_serve_valid
 * Summary: Tells you whether a jet_http_serve call gave you a real, listening server.
 * Parameters:
 *   - s: The handle to test.
 * Returns: true when the port was claimed and is listening; false when claiming it failed, a server is already running, WRAM ran out, or this runtime has no JET_NET_CAP_HTTP_SERVE.
 * Include: jet_http_serve.h
 * Notes: It reports whether jet_http_serve gave you a real handle. After the server is closed, the stale handle still looks like a handle, but every server call safely refuses it.
 * Example:
 *   s_srv = jet_http_serve(SCORES_PORT);
 *   if (!jet_http_serve_valid(s_srv)) ui_show("PORT IN USE");
 */
static inline bool jet_http_serve_valid(JetHttpServer s) {
    return jet_net_handle_valid(s.id);
}

/**
 * Function: jet_http_serve
 * Summary: Claims a port on every network interface this machine has and starts answering HTTP requests on it.
 * Parameters:
 *   - port: The TCP port to listen on, as an ordinary number (host byte order). It must not be zero, and it must be above 1024, because the sandbox has no special privileges.
 * Returns: A server handle, or an invalid handle (see jet_http_serve_valid) when the port is already taken, a server is already running, WRAM could not be reserved, or this runtime has no JET_NET_CAP_HTTP_SERVE, which is every web build.
 * Include: jet_http_serve.h
 * Notes: CALL IT FROM _init(). The first server reserves JET_HTTP_SERVE_WRAM_BYTES of WRAM, the cartridge's working memory, for its connection buffers, and the WRAM allocator never takes memory back, so reserving during play would grow the cartridge's footprint every time. Later servers re-use the same memory.
 *
 *        It listens on 0.0.0.0, which means every address this machine has, so the cartridge can be reached from the whole LAN: a phone's browser pointed at http://<this machine>:<port>/ reaches it, which is the whole trick behind a second-screen companion page.
 *
 *        Nothing is encrypted and nobody has to log in. Put nothing behind it that a neighbour may not have.
 * Example:
 *   static JetHttpServer s_srv;
 *   void _init(void) {
 *       s_srv = jet_http_serve(SCORES_PORT);
 *       if (!jet_http_serve_valid(s_srv)) ui_show("NO SERVICE");
 *   }
 */
JetHttpServer jet_http_serve(uint16_t port);

/**
 * Function: jet_http_serve_poll
 * Summary: Pushes every connection along and hands you at most one complete request.
 * Parameters:
 *   - s: The server to pump.
 *   - out: Filled in with the request when one is ready, and left alone otherwise. Must not be NULL.
 * Returns: true when `out` now describes a request you must answer; false when nothing is ready this tick.
 * Include: jet_http_serve.h
 * Notes: CALL IT EVERY TICK, IN A LOOP, UNTIL IT RETURNS FALSE. It is the pump as well as the question: accepting connections, reading bytes, sending queued responses and hanging up on stalled peers all happen inside it, so a server that is not polled does nothing at all. Each call hands over at most one request, so your routing code stays a straight line.
 *
 *        ANSWER EVERY REQUEST YOU ARE GIVEN, and promptly. The connection is held open until you do, and a request left unanswered for JET_HTTP_SERVE_IDLE_MS is hung up on, at which point the views in `out` go stale with it. Requests that are malformed, too big or stalled are answered by the server itself and never show up here.
 * Example:
 *   void _update(float dt) {
 *       JetHttpServerRequest req;
 *       while (jet_http_serve_poll(s_srv, &req)) api_route(&req);
 *   }
 */
bool jet_http_serve_poll(JetHttpServer s, JetHttpServerRequest* out);

/**
 * Function: jet_http_serve_respond
 * Summary: Answers one request and closes its connection.
 * Parameters:
 *   - s: The server the request came from.
 *   - request_id: The `id` of the request you are answering.
 *   - http_status: The status code to send, either a JetHttpStatus or any other code you need.
 *   - content_type: The value for the Content-Type header, for example JET_HTTP_CONTENT_JSON. NULL means JET_HTTP_CONTENT_TEXT.
 *   - body: The response body. May be NULL when len is 0.
 *   - len: The body's length in bytes, at most JET_HTTP_SERVE_RESPONSE_MAX.
 * Returns: JET_NET_OK once the response is queued; JET_NET_E_ARG for a stale request id, a server handle that is not this request's, or a malformed argument; JET_NET_E_LIMIT when the body or the content type is too big to send.
 * Include: jet_http_serve.h
 * Notes: The body is COPIED, so it may live anywhere and you may re-use it the moment this returns: a `static char s_json[N]` is exactly right. Unlike the client half in jet_http.h there is no WRAM rule here, because no host is involved. The response is queued and sent over the following ticks (usually this one), and then the connection closes: every response carries `Connection: close`, so one request is one connection and there is no keep-alive to think about. The Content-Length header is worked out for you; do not send headers of your own.
 *
 *        Answer a HEAD exactly as you would the matching GET. The server sends the headers your response would have had, declares the length it would have carried, and holds back the body, which is what a HEAD is for.
 *
 *        Calling it twice for the same request answers once and reports JET_NET_E_ARG the second time, because the request's token dies with its connection.
 * Example:
 *   const int len = scores_to_json(s_json, sizeof s_json);
 *   jet_http_serve_respond(s_srv, req.id, JET_HTTP_STATUS_OK, JET_HTTP_CONTENT_JSON, s_json, (uint32_t)len);
 */
int jet_http_serve_respond(JetHttpServer s, uint32_t request_id, int http_status,
                           const char* content_type, const void* body, uint32_t len);

/**
 * Function: jet_http_serve_close
 * Summary: Stops listening and hangs up on every connection the server holds.
 * Parameters:
 *   - s: The server to close. A stale or invalid handle is ignored.
 * Returns: Nothing.
 * Include: jet_http_serve.h
 * Notes: Safe to call again. It takes the whole service down with it: unlike jet_tcp_listener_close, the connections belong to the server, so a response still being sent is cut short. The handle is stale for good afterwards. The WRAM the server reserved is NOT given back (the allocator never takes memory back), but it is re-used by the next jet_http_serve, so a cartridge may stop and restart its service as often as it likes without growing.
 * Example:
 *   void _end(void) { jet_http_serve_close(s_srv); }
 */
void jet_http_serve_close(JetHttpServer s);

/**
 * Function: jet_http_serve_shutdown
 * Summary: Stops every server this module is running and hangs up on every connection they hold.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_http_serve.h
 * Notes: Optional. The runtime arranges for it to run by itself when the program exits, so a cartridge that just ends never leaves a port claimed (docs/networking/design.md section 6.4, nothing survives the cartridge). Call it yourself from _end() when you want to tidy up at a moment you choose. Safe to call again, and safe before anything was ever served. Every server handle taken before the call is stale afterwards; the WRAM they reserved is kept and re-used by the next jet_http_serve.
 * Example:
 *   void _end(void) { jet_http_serve_shutdown(); }
 */
void jet_http_serve_shutdown(void);

#endif // JET_HTTP_SERVE_H
