// runtime/jet_ascii.h
#ifndef JET_ASCII_H
#define JET_ASCII_H

#include <stdint.h>

// Turns text from outside the machine into text the machine can show. A paste,
// a file a player imports, a document saved by another program: all of them
// arrive as UTF-8, where a curly quote, an em dash or an accented letter is
// two or three bytes the fonts have no picture for. jet_ascii_fold rewrites
// such text into plain ASCII in place, by one fixed table:
//
//   printable ASCII, newline and tab      stay as they are
//   \r\n and a lone \r                    become \n
//   em dash, en dash, the other dashes,
//   the two Unicode hyphens and the
//   minus sign                            become -
//   the curly single quotes, prime and
//   the single guillemets (‹ ›)           become '
//   the curly double quotes, double prime
//   and the guillemets (« »)              become "
//   the ellipsis                          becomes ...
//   non-breaking and other wide spaces    become a space
//   the byte order mark (also the zero-
//   width no-break space) and the soft
//   hyphen                                are dropped, uncounted: they were
//                                         invisible in the text they came from
//   the bullet and the middle dot         become * and .
//   Latin letters with marks (é, ñ, ü)    become the bare letter, and
//                                         ß, æ and œ become ss, ae and oe
//   anything else, and broken UTF-8       becomes ?, and is counted
//
// The text never grows, so it is rewritten where it lies: every character
// past ASCII is at least two bytes, and folds to at most as many. The count
// of question marks it had to write comes back too, so a cartridge can say
// "2 characters had no ASCII form" rather than lose them silently.
//
// Include "jet_ascii.h" yourself; jet64.h does not include it for you. It has
// no host side at all: it is the cartridge's own code, the same on every
// runtime.
//
// Complete paste-and-show example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_ascii.h"
//
//   enum { TEXT_CAPACITY = 64 };
//   static char text[TEXT_CAPACITY + 1];
//   static uint32_t replaced;
//
//   void _init(void) {
//       // "Café — “ok”" as UTF-8, the way another program would hand it over.
//       const char utf8[] = "Caf\xC3\xA9 \xE2\x80\x94 \xE2\x80\x9Cok\xE2\x80\x9D";
//       uint32_t len = (uint32_t)(sizeof utf8 - 1);
//       for (uint32_t i = 0; i < len; i++) text[i] = utf8[i];
//       len = jet_ascii_fold(text, len, &replaced);
//       text[len] = '\0'; // now "Cafe - \"ok\""
//   }
//
//   void _update(float dt) {
//       (void)dt;
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(text, 8, 8, 15);
//   }
//
//   void _end(void) {}

/**
 * Function: jet_ascii_fold
 * Summary: Rewrites UTF-8 text into plain ASCII in place, and answers how long it is now.
 * Parameters:
 *   - text: The bytes to fold. They are rewritten where they lie; nothing outside the first len bytes is read or written.
 *   - len: How many bytes of text there are. It does not need a terminator, and a zero byte inside it is folded like any other control character.
 *   - replaced: Where to store how many characters had no ASCII form and became ?, or NULL when you do not need the count.
 * Returns: The folded text's new length, which is never more than len.
 * Include: jet_ascii.h
 * Notes: The table is in the header's opening comment: dashes and hyphens become -, curly quotes and guillemets become ' and ", the ellipsis becomes ..., wide spaces become a space, Latin letters lose their marks, \r\n becomes \n, and the invisible byte order mark and soft hyphen are dropped without being counted. Anything else past ASCII, a control character other than newline and tab, and bytes that are not valid UTF-8 each become one ?, and each is counted in *replaced. The answer is never longer than the text it was given, so the bytes after the new length are left over from the old text: write a terminator there yourself if you need one. Folding plain ASCII changes nothing and costs about what reading it does, so it is cheap to run over a whole file. Folding text twice gives the same text as folding it once.
 * Example:
 *   uint32_t lost = 0;
 *   len = jet_ascii_fold(paste, len, &lost);
 *   if (lost > 0) show_note("some characters had no ASCII form");
 */
uint32_t jet_ascii_fold(char* text, uint32_t len, uint32_t* replaced);

#endif // JET_ASCII_H
