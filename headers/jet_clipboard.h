// runtime/jet_clipboard.h
#ifndef JET_CLIPBOARD_H
#define JET_CLIPBOARD_H

#include <stdbool.h>
#include <stdint.h>

// Copy and paste with the computer's own clipboard: the one every other
// program shares. jet_clipboard_set puts text on it (copy, cut) and
// jet_clipboard_get takes the text that is on it (paste). The text is UTF-8,
// exactly the bytes the other program put there; run jet_ascii_fold
// (jet_ascii.h) over a paste to make it text the machine's fonts can draw.
//
// Include "jet_clipboard.h" yourself; jet64.h does not include it for you.
//
// Three rules come with it.
//
//   1. THE TEXT LIVES IN WRAM. The host copies straight to and from your
//      buffer, and it can only see working memory, so reserve the buffer with
//      jet_memory_reserve. A static array lives in the cartridge's own program,
//      where the host cannot reach it, and is refused.
//   2. ONLY THE PLAYER'S OWN KEYS OPEN IT. The clipboard often holds something
//      private (a password, an address), and writing it throws away what the
//      player was about to paste. So the host lets a cartridge read it only
//      within JET_CLIPBOARD_GESTURE_TICKS of the player pressing V with the
//      command key (Command on a Mac, Ctrl elsewhere: jet_key_command_down),
//      and write it only that soon after C or X with the command key. Call
//      them on the tick the chord arrives, from _update. The price: a paste
//      or copy chosen from a menu with the mouse is refused, because no key
//      was pressed. Offer the keys, and say so on the menu.
//   3. TOO BIG IS AN ANSWER. A paste that does not fit your buffer writes
//      nothing and tells you how big it is, so you can say so ("that paste is
//      30 MB; there is room for 12") rather than keep half of it.
//
// The web player has no clipboard: jet_clipboard_available answers false and
// both calls answer JET_CLIPBOARD_UNAVAILABLE.
//
// Complete copy-and-paste example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_ascii.h"
//   #include "jet_clipboard.h"
//
//   enum { NOTE_CAPACITY = 4096 };
//   static char* note; // in WRAM, where the host can reach it
//   static uint32_t note_len;
//
//   void _init(void) {
//       note = jet_memory_reserve(NOTE_CAPACITY + 1);
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (note == NULL || !jet_key_command_down()) return;
//       if (jet_key_pressed(JET_KEY_V)) {
//           uint32_t len = 0;
//           if (jet_clipboard_get(note, NOTE_CAPACITY, &len) == JET_CLIPBOARD_OK) {
//               // Another program's text is UTF-8: fold it to what the fonts draw.
//               note_len = jet_ascii_fold(note, len, NULL);
//           }
//       }
//       if (jet_key_pressed(JET_KEY_C)) {
//           jet_clipboard_set(note, note_len);
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       if (note != NULL) {
//           note[note_len] = '\0';
//           jet_print(note, 8, 8, 15);
//       }
//   }
//
//   void _end(void) {}

/**
 * Constant: JET_CLIPBOARD_GESTURE_TICKS
 * Summary: How many ticks after the player's copy or paste keys the clipboard stays open to the cartridge: 30, a quarter of a second.
 * Include: jet_clipboard.h
 * Notes: The host notes the tick it hands the cartridge V, C or X pressed together with the command key (jet_key_command_down). A jet_clipboard_get is honoured within this many ticks of the V, and a jet_clipboard_set within this many ticks of the C or the X; outside that window both answer JET_CLIPBOARD_REFUSED. Calling them from _update on the tick jet_key_pressed reports the letter is always inside it. A paste chosen from a menu with the mouse has no key behind it, so it is refused: that is the price of keeping the clipboard private.
 * Example:
 *   if (jet_key_command_down() && jet_key_pressed(JET_KEY_V)) paste(); // inside the window
 */
#define JET_CLIPBOARD_GESTURE_TICKS 30

/**
 * Enum: JetClipboardStatus
 * Summary: What became of a copy or a paste.
 * Include: jet_clipboard.h
 * Notes: JET_CLIPBOARD_OK means it happened: the text was taken (a copy) or written into your buffer (a paste, with its length in *len). JET_CLIPBOARD_EMPTY means the clipboard holds no text, perhaps a picture or nothing at all. JET_CLIPBOARD_TOO_SMALL means a paste did not fit your buffer: nothing was written, and *len is the size it needs. JET_CLIPBOARD_REFUSED means the host would not do it, because the player had not just pressed the copy or paste keys (JET_CLIPBOARD_GESTURE_TICKS) or because the buffer is not in WRAM. JET_CLIPBOARD_UNAVAILABLE means this runtime has no clipboard at all, as in the web player.
 * Example:
 *   if (jet_clipboard_get(buf, cap, &len) == JET_CLIPBOARD_TOO_SMALL) show_note("that paste is too big");
 */
typedef enum JetClipboardStatus {
    JET_CLIPBOARD_OK = 0,          // *len bytes written (get) or taken (set)
    JET_CLIPBOARD_EMPTY = 1,       // the clipboard holds no text
    JET_CLIPBOARD_TOO_SMALL = 2,   // nothing written; *len is the size needed
    JET_CLIPBOARD_REFUSED = 3,     // no copy or paste keys just pressed, or a buffer outside WRAM
    JET_CLIPBOARD_UNAVAILABLE = 4, // this runtime has no clipboard (the web player)
} JetClipboardStatus;

/**
 * Function: jet_clipboard_available
 * Summary: Tells you whether this runtime has a clipboard at all.
 * Parameters:
 *   - None.
 * Returns: true on the Workstation and in an exported native player; false in the web player.
 * Include: jet_clipboard.h
 * Notes: Use it to decide whether to offer copy and paste at all, such as greying a menu row out. A runtime with a clipboard can still refuse a single call (JET_CLIPBOARD_REFUSED), so check what each call answers too.
 * Example:
 *   bool offer_paste = jet_clipboard_available();
 */
bool jet_clipboard_available(void);

/**
 * Function: jet_clipboard_set
 * Summary: Puts text on the computer's clipboard, for a copy or a cut.
 * Parameters:
 *   - utf8: The text to copy, as UTF-8. It must be in WRAM (reserve it with jet_memory_reserve). It needs no terminator.
 *   - len: How many bytes of utf8 to copy. Zero copies empty text.
 * Returns: JET_CLIPBOARD_OK when the text is on the clipboard; JET_CLIPBOARD_REFUSED when the player had not just pressed C or X with the command key, or the text is not in WRAM; JET_CLIPBOARD_UNAVAILABLE on the web.
 * Include: jet_clipboard.h
 * Notes: Call it from _update on the tick the player presses C or X with the command key (jet_key_command_down). The host takes the bytes during the call, so the buffer is yours again as soon as it returns. Bytes that are not valid UTF-8 reach the other program as the replacement character, so copy text, not binary data.
 * Example:
 *   if (jet_key_command_down() && jet_key_pressed(JET_KEY_C)) jet_clipboard_set(selection, selection_len);
 */
JetClipboardStatus jet_clipboard_set(const char* utf8, uint32_t len);

/**
 * Function: jet_clipboard_get
 * Summary: Copies the text on the computer's clipboard into your buffer, for a paste.
 * Parameters:
 *   - dst: Where to write the text. It must be in WRAM (reserve it with jet_memory_reserve).
 *   - capacity: How many bytes dst can hold.
 *   - len: Where to store the answer's length: the bytes written for JET_CLIPBOARD_OK, the bytes needed for JET_CLIPBOARD_TOO_SMALL, and 0 for every other answer.
 * Returns: JET_CLIPBOARD_OK with the text written; JET_CLIPBOARD_EMPTY when the clipboard holds no text; JET_CLIPBOARD_TOO_SMALL when it does not fit; JET_CLIPBOARD_REFUSED when the player had not just pressed V with the command key, or dst is not in WRAM; JET_CLIPBOARD_UNAVAILABLE on the web.
 * Include: jet_clipboard.h
 * Notes: Call it from _update on the tick the player presses V with the command key (jet_key_command_down). The text is the other program's UTF-8, with no terminator added, so run jet_ascii_fold over it before you draw it. Offer the largest buffer you can: a paste that does not fit writes nothing at all, and *len says how big it was, so one call answers both "here it is" and "it is this much too big".
 * Example:
 *   uint32_t len = 0;
 *   if (jet_clipboard_get(buf, BUF_CAPACITY, &len) == JET_CLIPBOARD_OK) insert_text(buf, len);
 */
JetClipboardStatus jet_clipboard_get(char* dst, uint32_t capacity, uint32_t* len);

#endif // JET_CLIPBOARD_H
