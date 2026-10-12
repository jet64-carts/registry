// runtime/jet_http.h
#ifndef JET_HTTP_H
#define JET_HTTP_H

// ============================================================================
// Jet64 HTTP(S), the web client (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_net.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_http.c only when a
// cart's #include tree reaches this header, and the exporter keys the player's
// TLS stack off the very same signal. A cart that never includes it contains no
// HTTP code at all — and its exported player never compiles rustls either.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_http.h"    // <-- explicit opt-in
//
// Fetch things from web addresses: a high-score table, a daily seed, a news
// banner. You give it a URL and a buffer, and over the next few ticks the reply
// lands in the buffer. The host, the program that runs your cartridge, does the
// actual fetching. That is why this works everywhere: a browser has a perfectly
// good web client of its own, so a cartridge that fetches through these calls
// behaves the same on the workstation, in a downloaded native build, and on an
// itch.io page. Raw sockets (jet_net.h) are for native builds only; HTTP is
// not.
//
// <nocodex>
// ----------------------------------------------------------------------------
// WHY THE HOST DOES THE WORK
// ----------------------------------------------------------------------------
// `https://` means TLS, and Jet64 will not vendor a C TLS stack into the sandbox
// when every host is Rust and one audited `rustls` dependency away. So a request
// is *described* here and *performed* by the host: the workstation and the
// native player run one shared Rust client, and the web player hands the same
// description to the browser's own fetch. One TLS stack, owned by the host,
// everywhere — and the cart never gains a capability it could not have
// implemented safely. See docs/networking/design.md §3 and §7.
// </nocodex>
//
// ----------------------------------------------------------------------------
// THE THREE RULES
// ----------------------------------------------------------------------------
// 1. NOTHING BLOCKS. jet_http_request comes back on the same tick you call it,
// and the transfer happens over the ticks that follow. You poll for the result,
// which means you ask jet_http_status each tick from _update whether it is done
// and never wait; read the body when it reports JET_NET_OK. Your game's 120
// ticks a second are never held up by a network that may take seconds.
//
// 2. HOST-WRITTEN BUFFERS LIVE IN WRAM. The buffer the reply lands in, and the
// request body if you send one, must be memory the host can see, and that means
// the cartridge's working memory, WRAM, which you get from jet_memory_reserve.
// A `static uint8_t buf[N]` lives inside the cartridge's own program, where the
// host cannot reach it, so passing one is refused outright, with a line in the
// log, rather than quietly returning nothing. This is the rule jet_pcm.h has,
// made general: anything the host must read or write lives in the shared block.
// The URL and the extra headers are the one deliberate exception. They are
// copied into the machine's own staging area on the way out, so a string
// literal is fine for those.
//
// 3. A STATUS IS NOT A FAILURE. A 404 is a "no" that was delivered
// successfully: the status reads JET_NET_OK and jet_http_response_code() reads
// 404, exactly as a browser's fetch does. Only a transfer that never produced a
// reply at all comes back negative: the address could not be looked up (DNS),
// the connection could not be made, the secure TLS handshake failed, or the
// time ran out. Test for the code you expect, not for a kind of failure you
// cannot tell apart from another.
//
// To accept HTTP requests from other machines on your own local network, a LAN,
// use the server in jet_http_serve.h. That one runs in native builds only.
//
// <nocodex>
// ----------------------------------------------------------------------------
// AND THE OTHER DIRECTION: A SERVER IN A CART
// ----------------------------------------------------------------------------
// A cart can also ANSWER the phone — a companion UI a browser opens, a LAN
// high-score board — and that lives next door in jet_http_serve.h. It is a
// separate module on purpose: the server is guest-direct and needs no TLS, so
// metering it separately means a cart that only serves never compiles `rustls`,
// and a cart that only fetches never compiles socket code. Neither pays for the
// other. See design §5.5 and §10.
//
// ----------------------------------------------------------------------------
// WHAT IS DELIBERATELY ABSENT IN v1
// ----------------------------------------------------------------------------
// No response-header enumeration and no streaming download: the cart-facing need
// is bodies, and response_cap bounds memory honestly rather than pretending a
// cart has unlimited room. Both are compatible later additions.
// </nocodex>
//
// Complete HTTPS GET example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_http.h"
//
//   enum { RESPONSE_CAPACITY = 4096 };
//   static JetHttp request;
//   static uint8_t* response;
//   static bool request_active;
//   static const char* result_text = "STARTING";
//   static int result_code;
//
//   void _init(void) {
//       response = (uint8_t*)jet_memory_reserve(RESPONSE_CAPACITY + 1);
//       if (response != NULL) {
//           request = jet_http_get("https://example.com/", response, RESPONSE_CAPACITY);
//           request_active = jet_http_valid(request);
//           result_text = request_active ? "FETCHING" : "REQUEST NOT STARTED";
//       } else {
//           result_text = "NO WRAM";
//       }
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (!request_active) return;
//       const int status = jet_http_status(request);
//       if (status == JET_NET_PENDING) return;
//       if (status == JET_NET_OK) {
//           result_code = jet_http_response_code(request);
//           if (result_code == 200) {
//               response[jet_http_response_len(request)] = '\0';
//               result_text = "HTTP SUCCESS";
//           } else {
//               result_text = "HTTP ERROR RESPONSE";
//           }
//       } else {
//           result_code = status;
//           result_text = "TRANSPORT FAILED";
//       }
//       jet_http_release(request);
//       request_active = false;
//   }
//
//   void _draw(void) {
//       char line[64];
//       if (result_code == 0) snprintf(line, sizeof line, "%s", result_text);
//       else snprintf(line, sizeof line, "%s (%d)", result_text, result_code);
//       jet_cls(0);
//       jet_print(line, 16, 16, 15);
//   }
//
//   void _end(void) {
//       if (request_active) jet_http_release(request);
//       jet_http_shutdown();
//   }

#include "jet_core.h" // IWYU pragma: keep fixed-width ints / bool (sandbox-safe)
#include "jet_net.h"  // the shared JetNetResult vocabulary and handle discipline

// ============================================================================
// LIMITS (every bound named once)
// ============================================================================

/**
 * Constant: JET_HTTP_MAX_REQUESTS
 * Summary: The most HTTP requests one cartridge can have going at once.
 * Include: jet_http.h
 * Notes: Each request holds one of a fixed set of network command slots until you call jet_http_release on it. If every slot is taken, starting another request gives you back an invalid JetHttp handle.
 * Example:
 *   JetHttp requests[JET_HTTP_MAX_REQUESTS] = {0};
 */
#define JET_HTTP_MAX_REQUESTS JET_NET_HTTP_SLOTS

/**
 * Constant: JET_HTTP_URL_MAX
 * Summary: The most bytes a URL may take up, counting the terminating NUL (the zero byte that ends a C string).
 * Include: jet_http.h
 * Notes: The URL is copied into a staging slot of this fixed size on the way out. jet_http_request refuses a longer URL and gives you an invalid handle; it never cuts the URL short.
 * Example:
 *   char url[JET_HTTP_URL_MAX];
 */
#define JET_HTTP_URL_MAX JET_NET_CMD_STAGE_URL_MAX

/**
 * Constant: JET_HTTP_HEADERS_MAX
 * Summary: The most bytes the extra request headers may take up, counting the terminating NUL.
 * Include: jet_http.h
 * Notes: Join your headers with CRLF or LF (a line ending) and keep their whole text below this size once copied. The data you want to send belongs in JetHttpParams.body, not in oversized headers.
 * Example:
 *   char headers[JET_HTTP_HEADERS_MAX] = "Content-Type: application/json";
 */
#define JET_HTTP_HEADERS_MAX JET_NET_CMD_STAGE_HEADERS_MAX

/**
 * Constant: JET_HTTP_DEFAULT_TIMEOUT_MS
 * Summary: The deadline a transfer gets, in milliseconds, when JetHttpParams.timeout_ms is zero.
 * Include: jet_http.h
 * Notes: Fifteen seconds is long enough for a slow connection, and short enough that a server which has stopped responding eventually reports JET_NET_E_TIMEOUT. Set timeout_ms yourself when your game needs a shorter deadline.
 * Example:
 *   params.timeout_ms = JET_HTTP_DEFAULT_TIMEOUT_MS;
 */
#define JET_HTTP_DEFAULT_TIMEOUT_MS 15000u

/**
 * Constant: JET_HTTP_MAX_TIMEOUT_MS
 * Summary: The longest deadline the host will allow a transfer, in milliseconds.
 * Include: jet_http.h
 * Notes: A larger JetHttpParams.timeout_ms is cut down to this, so one request cannot hold a shared service slot for ever.
 * Example:
 *   if (timeout_ms > JET_HTTP_MAX_TIMEOUT_MS) timeout_ms = JET_HTTP_MAX_TIMEOUT_MS;
 */
#define JET_HTTP_MAX_TIMEOUT_MS 120000u

// ============================================================================
// HANDLES
// ============================================================================

/**
 * Struct: JetHttp
 * Summary: A handle to one HTTP request, from the moment you start it until you release it. A handle is a small value that stands for a thing the runtime owns.
 * Fields:
 *   - id: The slot number and its generation stamp, packed into one integer. Keep it, pass it back, do not look inside; test it with jet_http_valid.
 * Include: jet_http.h
 * Notes: Copy it freely; it is one integer. Once released, a handle is stale for good: every call answers JET_NET_E_ARG instead of reading whatever request now sits in that slot. A handle that is all zeros (`static JetHttp s_req;`) is safely invalid, so you can poll one before you have made any request.
 * Example:
 *   JetHttp request = jet_http_get("https://example.com/news", response, RESPONSE_CAP);
 */
typedef struct {
    int32_t id;
} JetHttp;

/**
 * Enum: JetHttpMethod
 * Summary: The HTTP verb a request uses: GET to fetch something, POST to send something, and so on.
 * Include: jet_http.h
 * Notes: A fixed set of names rather than a string, so a typo is a compile error instead of a puzzling 400 from someone else's server, and so the host checks one integer instead of parsing your text into a request line. JET_HTTP_GET is 0, so a JetHttpParams filled with zeros is a GET without saying so.
 * Example:
 *   params.method = JET_HTTP_POST;
 */
typedef enum {
    JET_HTTP_GET    = 0, // fetch something; the usual verb, and the one a zeroed JetHttpParams gets
    JET_HTTP_POST   = 1, // send something, with the data in JetHttpParams.body
    JET_HTTP_PUT    = 2, // send something that replaces what is there
    JET_HTTP_PATCH  = 3, // send a change to part of what is there
    JET_HTTP_DELETE = 4, // ask the server to remove something
    JET_HTTP_HEAD   = 5, // ask for the headers only; no body is sent or received
} JetHttpMethod;

/**
 * Struct: JetHttpParams
 * Summary: Everything about one request, for the cases jet_http_get cannot express: a verb, extra headers, a body, a deadline of your own.
 * Fields:
 *   - url: The address to fetch, starting http:// or https://, ending in a NUL and shorter than JET_HTTP_URL_MAX.
 *   - method: Which verb to use. Leave it zero and you get JET_HTTP_GET.
 *   - headers: Extra request headers, one "Name: value" per line with CRLF or LF between them; NULL for none.
 *   - body: The data to send with the request, or NULL for none. When body_len is not zero it MUST live in WRAM. GET and HEAD ignore it.
 *   - body_len: How many bytes of body to send; 0 for none.
 *   - response: The buffer the reply's body lands in. It MUST live in WRAM.
 *   - response_cap: How many bytes that buffer holds. A longer reply is cut to fit, and jet_http_response_truncated says so.
 *   - timeout_ms: How long the transfer may take, in milliseconds. 0 means JET_HTTP_DEFAULT_TIMEOUT_MS, and the host cuts anything above JET_HTTP_MAX_TIMEOUT_MS down to that.
 * Include: jet_http.h
 * Notes: Meant to be filled in from a struct of zeros: `JetHttpParams p = {0}` plus a url and a response buffer is a GET with the default deadline. The url and headers are COPIED on the way out, so a string literal or a buffer on the stack is fine for them. The body and the response are read and written in place by the host, which is why those two must live in WRAM, the cartridge's working memory from jet_memory_reserve.
 * Example:
 *   JetHttpParams params = {0};
 *   params.url = "https://api.example.com/scores";
 *   params.method = JET_HTTP_POST;
 *   params.headers = "Content-Type: application/json";
 *   params.body = s_json;            // in WRAM
 *   params.body_len = json_len;
 *   params.response = s_reply;       // in WRAM
 *   params.response_cap = REPLY_CAP;
 *   s_post = jet_http_request(&params);
 */
typedef struct {
    const char*   url;
    JetHttpMethod method;
    const char*   headers;
    const void*   body;
    uint32_t      body_len;
    void*         response;
    uint32_t      response_cap;
    uint32_t      timeout_ms;
} JetHttpParams;

/**
 * Function: jet_http_valid
 * Summary: Tells you whether a request call actually gave you a request.
 * Parameters:
 *   - h: The handle to test.
 * Returns: true when the request was accepted and is under way; false when every slot was taken, an argument was refused, or this runtime has no JET_NET_CAP_HTTP.
 * Include: jet_http.h
 * Notes: A false here means the request never left the machine, which is a different thing from a transfer that failed, and worth showing differently. Trying again next tick makes sense, because a slot may have come free; trying a JET_NET_E_CONNECT again straight away does not.
 * Example:
 *   s_req = jet_http_get(SCORES_URL, s_body, SCORES_BODY_CAP);
 *   if (!jet_http_valid(s_req)) status_show("BUSY");
 */
static inline bool jet_http_valid(JetHttp h) {
    return jet_net_handle_valid(h.id);
}

// ============================================================================
// THE CLIENT
// ============================================================================

/**
 * Function: jet_http_request
 * Summary: Starts a fully described request and comes straight back; the transfer happens over the ticks that follow.
 * Parameters:
 *   - params: The description of the request. It is copied when the request starts, so it may live on the stack.
 * Returns: A handle whose status starts at JET_NET_PENDING, or an invalid handle (see jet_http_valid) when every slot is taken, an argument is malformed, a buffer the host must reach is not in WRAM, or this runtime has no JET_NET_CAP_HTTP.
 * Include: jet_http.h
 * Notes: NOTHING BLOCKS HERE: the call returns straight away and never waits. Ask jet_http_status from _update each tick, and read the body once it reports JET_NET_OK. Release every handle you take with jet_http_release, including after a failure: the slot is not given back until you do, and there are only JET_HTTP_MAX_REQUESTS of them. The response buffer belongs to the request until you release it, so do not hand the same buffer to a second request while the first is still going.
 * Example:
 *   JetHttpParams params = {0};
 *   params.url = HIGHSCORE_URL;
 *   params.method = JET_HTTP_POST;
 *   params.body = s_payload;  params.body_len = payload_len;   // WRAM
 *   params.response = s_reply; params.response_cap = REPLY_CAP; // WRAM
 *   s_submit = jet_http_request(&params);
 */
JetHttp jet_http_request(const JetHttpParams* params);

/**
 * Function: jet_http_get
 * Summary: Fetches a web address into a buffer. This is the one-line call that covers most needs.
 * Parameters:
 *   - url: The address to fetch, starting http:// or https://.
 *   - response: The buffer the reply's body lands in. It MUST live in WRAM, the cartridge's working memory from jet_memory_reserve.
 *   - response_cap: How many bytes that buffer holds.
 * Returns: A handle whose status starts at JET_NET_PENDING, or an invalid handle for the same reasons as jet_http_request.
 * Include: jet_http.h
 * Notes: Exactly jet_http_request with a JetHttpParams of zeros: a GET, no extra headers, no body, the default deadline. Use the full form when you need a different verb, a header, or a body.
 * Example:
 *   static JetHttp s_req;
 *   static uint8_t* s_body;
 *   void _init(void)  { s_body = jet_memory_reserve(SCORES_BODY_CAP); }
 *   void fetch(void)  { s_req = jet_http_get(SCORES_URL, s_body, SCORES_BODY_CAP); }
 *   void _update(float dt) {
 *       if (jet_http_status(s_req) == JET_NET_OK) {
 *           scores_parse(s_body, jet_http_response_len(s_req));
 *           jet_http_release(s_req);
 *       }
 *   }
 */
JetHttp jet_http_get(const char* url, void* response, uint32_t response_cap);

/**
 * Function: jet_http_status
 * Summary: Tells you how far a request has got.
 * Parameters:
 *   - h: The request to ask about.
 * Returns: JET_NET_PENDING while the transfer is still going; JET_NET_OK once a reply has landed; a negative JetNetResult when the transfer failed (JET_NET_E_RESOLVE, JET_NET_E_CONNECT, JET_NET_E_TIMEOUT, JET_NET_E_DENIED, ...); or JET_NET_E_ARG for a handle that is stale, made up, or was never given out.
 * Include: jet_http.h
 * Notes: THE HTTP STATUS CODE IS NOT HERE. JET_NET_OK means the exchange finished, whatever the server thought of it; ask jet_http_response_code whether it was a 200 or a 404. A handle that is all zeros reports JET_NET_E_ARG, so you can poll one before your first request without a special case.
 * Example:
 *   switch (jet_http_status(s_req)) {
 *       case JET_NET_PENDING: break;
 *       case JET_NET_OK:      scores_parse(s_body, jet_http_response_len(s_req)); jet_http_release(s_req); break;
 *       default:              status_show("OFFLINE"); jet_http_release(s_req);   break;
 *   }
 */
int jet_http_status(JetHttp h);

/**
 * Function: jet_http_response_code
 * Summary: The HTTP status code the server answered with.
 * Parameters:
 *   - h: A request whose status has reached JET_NET_OK.
 * Returns: The code (200, 404, 500, ...); or 0 while the transfer is still going, when it failed before any reply arrived, or when the handle is stale.
 * Include: jet_http.h
 * Notes: It means something only once jet_http_status reports JET_NET_OK. Treat 2xx (200 to 299) as success and everything else as the server's answer to what you asked: a 401 wants a login, a 429 wants you to slow down, and a 500 is the server's bug, not yours.
 * Example:
 *   if (jet_http_status(s_req) == JET_NET_OK && jet_http_response_code(s_req) == HTTP_OK) {
 *       scores_parse(s_body, jet_http_response_len(s_req));
 *   }
 */
int jet_http_response_code(JetHttp h);

/**
 * Function: jet_http_response_len
 * Summary: How many bytes of the reply's body landed in your buffer.
 * Parameters:
 *   - h: A request whose status has reached JET_NET_OK.
 * Returns: The number of bytes written into `response`, never more than `response_cap`; or 0 while the transfer is still going, when it failed, or when the handle is stale.
 * Include: jet_http.h
 * Notes: The buffer is NOT ended with a NUL, because a reply may be binary and adding a terminator would quietly cost you one byte of the room you asked for. Work from the length, or add the NUL yourself, using a buffer one byte bigger than the largest body you expect.
 * Example:
 *   uint32_t n = jet_http_response_len(s_req);
 *   if (n < SCORES_BODY_CAP) s_body[n] = '\0';   // now safe to treat as text
 */
uint32_t jet_http_response_len(JetHttp h);

/**
 * Function: jet_http_response_truncated
 * Summary: Tells you whether the reply's body was bigger than the buffer you gave it.
 * Parameters:
 *   - h: A request whose status has reached JET_NET_OK.
 * Returns: true when the body was longer than response_cap (the first response_cap bytes were kept); false otherwise, and for a stale handle.
 * Include: jet_http.h
 * Notes: A cut-off reply is usually a mistake in the design rather than a passing fault: a JSON document cut in half will not parse, and the honest cure is a bigger buffer, not a retry. Check it wherever the body has a structure; ignore it where you only ever wanted the first few kilobytes.
 * Example:
 *   if (jet_http_response_truncated(s_req)) LOG_WARN("scores reply exceeded %u bytes", SCORES_BODY_CAP);
 */
bool jet_http_response_truncated(JetHttp h);

/**
 * Function: jet_http_release
 * Summary: Gives a request's slot back, abandoning the request if it is still going.
 * Parameters:
 *   - h: The request to release. A stale or invalid handle is ignored.
 * Returns: Nothing.
 * Include: jet_http.h
 * Notes: Safe to call again, and always safe, so call it on every way out of your code, including after a failure, whose slot is held until you do. The handle is stale for good afterwards.
 *
 *        A request abandoned in the middle of its transfer cannot be un-sent. The host stops when the transfer ends or its deadline passes, and then throws the answer away WITHOUT writing your response buffer, so the slot may take a moment to come back. For that reason, treat the response buffer as the request's until you have released it and moved on; do not re-use it for something else straight away.
 * Example:
 *   void _end(void) { jet_http_release(s_req); }
 */
void jet_http_release(JetHttp h);

/**
 * Function: jet_http_shutdown
 * Summary: Abandons every request this module still holds and empties its pool.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_http.h
 * Notes: Optional. The runtime arranges for it to run by itself when the program exits, so a cartridge that just ends never leaves a slot hanging. Call it yourself from _end() when you want to tidy up at a moment you choose, or between levels to reset the pool. Safe to call again, and safe before any request was ever made. Every handle taken before the call is stale afterwards. A cartridge that is also SERVING has its own tidy-up next door, jet_http_serve_shutdown.
 * Example:
 *   void _end(void) { jet_http_shutdown(); }
 */
void jet_http_shutdown(void);

#endif // JET_HTTP_H
