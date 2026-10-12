// runtime/jet_intellisense.h
#ifndef JET_INTELLISENSE_H
#define JET_INTELLISENSE_H

// ==============================================================================
// JET64 STANDARD C LIBRARY — SAFE, CART-AVAILABLE BUILDING BLOCKS
// ==============================================================================
// This file is NEVER compiled or evaluated by the C compiler during cartridge
// builds. It exists exclusively to feed the Workstation IDE's IntelliSense
// and Codex database with the small standard-C surface already made available
// through jet64.h. Do not include this file in a cart. Prefer bounded operations
// such as snprintf(), validate sizes before copying raw memory, and remember
// that string functions require a terminating '\0'. Jet64 carts cannot use
// malloc/free or host filesystem functions; use jet_memory_reserve() and jet_io.
//
// Complete bounded-formatting example (copy into main.c; do not include this header):
//
//   #include "jet64.h"
//
//   static uint32_t score;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) score += 100;
//   }
//
//   void _draw(void) {
//       char line[32];
//       snprintf(line, sizeof line, "SCORE %u", score);
//       jet_cls(0);
//       jet_print(line, 16, 16, 15);
//   }
//
//   void _end(void) {}
// ==============================================================================

#if 0

/**
 * Function: memcpy
 * Summary: Copies a specific number of bytes from a source memory block to a destination.
 * Parameters:
 *   - dest: Pointer to the destination array where the content is to be copied.
 *   - src: Pointer to the source of data to be copied.
 *   - n: Number of bytes to copy.
 * Returns: A pointer to the destination buffer.
 * Include: <string.h>
 * Notes: The memory areas must not overlap. Use memmove() if overlapping is possible.
 * Example:
 *   uint8_t buffer[64];
 *   memcpy(buffer, source_data, 64);
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * Function: memset
 * Summary: Fills a block of memory with a specific byte value.
 * Parameters:
 *   - s: Pointer to the block of memory to fill.
 *   - c: Value to be set. The value is passed as an int, but the function fills the block of memory using the unsigned char conversion of this value.
 *   - n: Number of bytes to be set to the value.
 * Returns: A pointer to the memory area s.
 * Include: <string.h>
 * Notes: The standard way to zero arrays, structs, or custom buffers. c is converted to one unsigned byte and repeated, so memset(values, 1, sizeof(values)) does not set each multi-byte integer to numeric 1.
 * Example:
 *   memset(player_state, 0, sizeof(PlayerState));
 */
void *memset(void *s, int c, size_t n);

/**
 * Function: memmove
 * Summary: Safely copies a block of memory, handling overlapping regions correctly.
 * Parameters:
 *   - dest: Pointer to the destination array where the content is to be copied.
 *   - src: Pointer to the source of data to be copied.
 *   - n: Number of bytes to copy.
 * Returns: A pointer to the destination buffer.
 * Include: <string.h>
 * Notes: Slightly slower than memcpy(), but guarantees safety if src and dest overlap.
 * Example:
 *   memmove(&buffer[4], &buffer[0], 12);
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * Function: memcmp
 * Summary: Compares the first n bytes of two memory blocks.
 * Parameters:
 *   - s1: Pointer to block of memory.
 *   - s2: Pointer to block of memory.
 *   - n: Number of bytes to compare.
 * Returns: 0 if the first n bytes match; a negative value if the first differing byte in s1 is lower, or a positive value if it is higher.
 * Include: <string.h>
 * Example:
 *   if (memcmp(state_a, state_b, sizeof(State)) == 0) { ... }
 */
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * Function: strlen
 * Summary: Calculates the length of a given string.
 * Parameters:
 *   - s: The C-string to measure.
 * Returns: The length of the string, excluding the terminating null byte ('\0').
 * Include: <string.h>
 * Example:
 *   size_t len = strlen("Jet64");
 */
size_t strlen(const char *s);

/**
 * Function: strcpy
 * Summary: Copies a C-string into an array, including the terminating null character.
 * Parameters:
 *   - dest: Pointer to the destination array.
 *   - src: C-string to be copied.
 * Returns: A pointer to the destination string.
 * Include: <string.h>
 * Notes: dest must have at least strlen(src) + 1 bytes and the regions must not overlap. This function has no capacity argument; prefer snprintf() when copying into a fixed buffer.
 * Example:
 *   strcpy(player_name, "Hero");
 */
char *strcpy(char *dest, const char *src);

/**
 * Function: strncpy
 * Summary: Copies exactly n bytes worth of a C string and padding into an array, with important termination caveats.
 * Parameters:
 *   - dest: Pointer to the destination array.
 *   - src: C-string to be copied.
 *   - n: Maximum number of characters to be copied.
 * Returns: A pointer to the destination string.
 * Include: <string.h>
 * Notes: This is not automatically safe: if strlen(src) is at least n, dest is not null-terminated; if src is shorter, the remainder is padded with zero bytes. Ensure dest has n bytes and set dest[n - 1] = '\0' when n is greater than zero. The regions must not overlap.
 * Example:
 *   strncpy(name, input, 15);
 *   name[15] = '\0';
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * Function: strcmp
 * Summary: Compares two C-strings character by character.
 * Parameters:
 *   - s1: The first string to be compared.
 *   - s2: The second string to be compared.
 * Returns: 0 if strings are exactly equal, <0 if s1 is less than s2, >0 if s1 is greater.
 * Include: <string.h>
 * Example:
 *   if (strcmp(cmd, "jump") == 0) { player_jump(); }
 */
int strcmp(const char *s1, const char *s2);

/**
 * Function: strncmp
 * Summary: Compares up to n characters of two C-strings.
 * Parameters:
 *   - s1: The first string to be compared.
 *   - s2: The second string to be compared.
 *   - n: Maximum number of characters to compare.
 * Returns: 0 if strings match up to n characters.
 * Include: <string.h>
 * Example:
 *   if (strncmp(prefix, "enemy_", 6) == 0) { ... }
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * Function: snprintf
 * Summary: Formats and stores a series of characters and values in a bounded string buffer.
 * Parameters:
 *   - str: Pointer to a buffer where the resulting C-string is stored.
 *   - size: Maximum number of bytes to be used in the buffer (including the null terminator).
 *   - format: C string that contains the text to be written, including format tags (e.g., %d, %s, %f).
 *   - ...: Additional arguments matching the format tags.
 * Returns: The number of characters that would have been written, excluding the null terminator; a negative value on encoding/format failure. A return value greater than or equal to size means truncation.
 * Include: <stdio.h>
 * Notes: Prefer this over sprintf(). When size is greater than zero, the destination is null-terminated even when truncated. Format arguments must exactly match their conversion specifiers; a mismatch is undefined behaviour.
 * Example:
 *   char msg[32];
 *   snprintf(msg, sizeof(msg), "Score: %d", score);
 */
int snprintf(char *str, size_t size, const char *format, ...);

/**
 * Function: sprintf
 * Summary: Formats and stores a series of characters and values in a string buffer.
 * Parameters:
 *   - str: Pointer to a buffer where the resulting C-string is stored.
 *   - format: C string that contains the text to be written, including format tags.
 *   - ...: Additional arguments matching the format tags.
 * Returns: The number of characters written.
 * Include: <stdio.h>
 * Notes: This function cannot know the destination capacity and can overflow it. Use snprintf() instead in new cart code. Format arguments must exactly match their conversion specifiers.
 * Example:
 *   sprintf(msg, "HP: %d", hp);
 */
int sprintf(char *str, const char *format, ...);

/**
 * Function: printf
 * Summary: Prints formatted output to the standard output stream.
 * Parameters:
 *   - format: C string that contains the text to be written, including format tags.
 *   - ...: Additional arguments matching the format tags.
 * Returns: The number of characters written.
 * Include: <stdio.h>
 * Notes: In Jet64, stdout is captured and routed into the Workstation IDE terminal for live debugging. This is diagnostic output, not on-screen cart text; use jet_print()/print() to draw text in the game display.
 * Example:
 *   printf("Spawned entity at %f, %f\n", x, y);
 */
int printf(const char *format, ...);

/**
 * Function: sinf
 * Summary: Calculates the sine of an angle (given in radians).
 * Parameters:
 *   - x: Floating point value representing an angle expressed in radians.
 * Returns: Sine of x, in the range [-1.0, 1.0].
 * Include: <math.h>
 * Notes: Useful for oscillators, circular motion, and wave generation.
 * Example:
 *   float y_offset = sinf(time * 2.0f) * 10.0f;
 */
float sinf(float x);

/**
 * Function: cosf
 * Summary: Calculates the cosine of an angle (given in radians).
 * Parameters:
 *   - x: Floating point value representing an angle expressed in radians.
 * Returns: Cosine of x, in the range [-1.0, 1.0].
 * Include: <math.h>
 * Example:
 *   float x_offset = cosf(time * 2.0f) * 10.0f;
 */
float cosf(float x);

/**
 * Function: atan2f
 * Summary: Calculates the arc tangent of y/x using the signs of both to determine the quadrant.
 * Parameters:
 *   - y: Y-coordinate (opposite side).
 *   - x: X-coordinate (adjacent side).
 * Returns: The principal value of the arc tangent of y/x, expressed in radians.
 * Include: <math.h>
 * Notes: Crucial for calculating rotation angles towards a target or aiming weapons.
 * Example:
 *   float aim_angle = atan2f(target_y - player_y, target_x - player_x);
 */
float atan2f(float y, float x);

/**
 * Function: tanhf
 * Summary: Calculates the hyperbolic tangent of x.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The hyperbolic tangent of x, in the range [-1.0, 1.0]: close to x near zero, approaching ±1.0 as x grows.
 * Include: <math.h>
 * Notes: The smooth saturator: a soft clip for audio, or an ease that never overshoots its limit. The whole hyperbolic family (sinhf, coshf, tanhf, asinhf, acoshf, atanhf) is available on every runtime; tanh() is the double twin.
 * Example:
 *   float clipped = tanhf(sample * drive);
 */
float tanhf(float x);

/**
 * Function: sqrtf
 * Summary: Computes the principal square root of a floating-point number.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The square root of x; NaN for a negative finite input.
 * Include: <math.h>
 * Notes: Expensive operation. If only comparing distances, compare squared distances instead to save CPU cycles.
 * Example:
 *   float dist = sqrtf((dx * dx) + (dy * dy));
 */
float sqrtf(float x);

/**
 * Function: powf
 * Summary: Returns base raised to the power exponent.
 * Parameters:
 *   - base: Base value.
 *   - exp: Exponent value.
 * Returns: The result of base ^ exp.
 * Include: <math.h>
 * Example:
 *   float damage = powf(base_dmg, multiplier);
 */
float powf(float base, float exp);

/**
 * Function: floorf
 * Summary: Rounds x downward, returning the largest integral value that is not greater than x.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The rounded down value.
 * Include: <math.h>
 * Example:
 *   int grid_x = (int)floorf(world_x / 16.0f);
 */
float floorf(float x);

/**
 * Function: ceilf
 * Summary: Rounds x upward, returning the smallest integral value that is not less than x.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The rounded up value.
 * Include: <math.h>
 * Example:
 *   float bounds_top = ceilf(box_y);
 */
float ceilf(float x);

/**
 * Function: roundf
 * Summary: Rounds x to the nearest integral value, rounding halfway cases away from zero.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The rounded value.
 * Include: <math.h>
 * Example:
 *   float nearest = roundf(10.5f); // Returns 11.0f
 */
float roundf(float x);

/**
 * Function: lroundf
 * Summary: Rounds x to the nearest integer, rounding halfway cases away from zero, and returns it as a long.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The nearest integer. A cart's long is 32-bit on the web player (and on Windows), so a result past ±2^31 is out of range there; C leaves it unspecified, and the web player saturates.
 * Include: <math.h>
 * Notes: One call where (long)roundf(x) is two. lround() is the double twin; reach for llroundf() whenever the result may pass ±2^31.
 * Example:
 *   long column = lroundf(cursor_x / cell_width);
 */
long lroundf(float x);

/**
 * Function: llroundf
 * Summary: Rounds x to the nearest integer, rounding halfway cases away from zero, and returns it as a long long.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The nearest integer, 64-bit on every runtime.
 * Include: <math.h>
 * Notes: The width-portable face of lroundf(): a long long is 64-bit on the workstation, the native players and the web player alike. llround() is the double twin.
 * Example:
 *   long long elapsed_ms = llroundf(elapsed_seconds * 1000.0f);
 */
long long llroundf(float x);

/**
 * Function: lrintf
 * Summary: Rounds x to the nearest integer, rounding halfway cases to the even neighbour, and returns it as a long.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The nearest integer, with 0.5 → 0, 1.5 → 2 and 2.5 → 2. A cart's long is 32-bit on the web player (and on Windows), so a result past ±2^31 is out of range there; C leaves it unspecified, and the web player saturates.
 * Include: <math.h>
 * Notes: Ties to even never drift a long sum of rounded values one way, which lroundf()'s away-from-zero ties do. rintf() and nearbyintf() round the same way and keep a float; llrintf() answers in a long long; lrint() is the double twin. Every C99 <math.h> function, in float and double, is available on every runtime.
 * Example:
 *   int pixel_x = (int)lrintf(anchor_x);
 */
long lrintf(float x);

/**
 * Function: fmodf
 * Summary: Returns the floating-point remainder of x / y.
 * Parameters:
 *   - x: The dividend.
 *   - y: The divisor.
 * Returns: The remainder of dividing x by y; NaN when y is zero.
 * Include: <math.h>
 * Notes: The result has the same sign as x. Useful for wrapping angles or timers.
 * Example:
 *   float wrapped_angle = fmodf(angle, JET_PI * 2.0f);
 */
float fmodf(float x, float y);

/**
 * Function: fabsf
 * Summary: Computes the absolute value of a floating-point number.
 * Parameters:
 *   - x: The floating point value.
 * Returns: The absolute value of x.
 * Include: <math.h>
 * Example:
 *   float distance_x = fabsf(target_x - player_x);
 */
float fabsf(float x);

/**
 * Function: abs
 * Summary: Computes the absolute value of an integer.
 * Parameters:
 *   - j: The integer value.
 * Returns: The absolute value of j.
 * Include: <stdlib.h>
 * Example:
 *   int delta = abs(current_hp - max_hp);
 */
int abs(int j);

/**
 * Function: atoi
 * Summary: Converts a C-string representation of an integral number into an int.
 * Parameters:
 *   - nptr: The string containing the representation of a number.
 * Returns: The converted integral number. Returns 0 when no conversion occurs, which is indistinguishable from a valid "0"; out-of-range input is not safely diagnosable through atoi().
 * Include: <stdlib.h>
 * Example:
 *   int level = atoi("42");
 */
int atoi(const char *nptr);

/**
 * Function: atof
 * Summary: Converts a C-string representation of a floating-point number into a double.
 * Parameters:
 *   - nptr: The string containing the representation of a floating-point number.
 * Returns: The converted value. Returns 0.0 when no conversion occurs, which is indistinguishable from a valid "0"; range errors are not exposed through this convenience function.
 * Include: <stdlib.h>
 * Example:
 *   float speed = (float)atof("3.14159");
 */
double atof(const char *nptr);

/**
 * Function: strtol
 * Summary: Converts the leading integer field of a C-string to a long, reporting where the field ended.
 * Parameters:
 *   - nptr: The string containing the representation of a number.
 *   - endptr: Receives a pointer to the first character not consumed; pass NULL to discard it. When no conversion occurs it receives nptr itself, which is how "not a number" is told apart from a valid 0.
 *   - base: The radix, 2..36, or 0 to auto-detect from a 0x (hex) or 0 (octal) prefix.
 * Returns: The converted value, saturated to LONG_MIN or LONG_MAX on overflow. Note that a cart's long is 32-bit on the web player.
 * Include: <stdlib.h>
 * Example:
 *   char *end = NULL;
 *   long count = strtol(line, &end, 10);
 *   if (end == line) { count = 0; } // the line held no number at all
 */
long strtol(const char *nptr, char **endptr, int base);

/**
 * Function: strtoul
 * Summary: Converts the leading integer field of a C-string to an unsigned long, reporting where the field ended.
 * Parameters:
 *   - nptr: The string containing the representation of a number.
 *   - endptr: Receives a pointer to the first character not consumed; pass NULL to discard it. When no conversion occurs it receives nptr itself.
 *   - base: The radix, 2..36, or 0 to auto-detect from a 0x (hex) or 0 (octal) prefix.
 * Returns: The converted value, saturated to ULONG_MAX on overflow. A leading '-' negates modulo the return type, exactly as C specifies.
 * Include: <stdlib.h>
 * Example:
 *   unsigned long rgb = strtoul("0xFF8800", NULL, 0);
 */
unsigned long strtoul(const char *nptr, char **endptr, int base);

/**
 * Function: strtod
 * Summary: Converts the leading decimal field of a C-string to a double, reporting where the field ended.
 * Parameters:
 *   - nptr: The string containing the representation of a floating-point number.
 *   - endptr: Receives a pointer to the first character not consumed; pass NULL to discard it. When no conversion occurs it receives nptr itself, which is how "not a number" is told apart from a valid 0.
 * Returns: The converted value; out-of-range magnitudes saturate to infinity or zero. The recognised field is [+-]digits[.digits][(e|E)[+-]digits] — hex floats, "inf" and "nan" are refused rather than parsed.
 * Include: <stdlib.h>
 * Example:
 *   char *end = NULL;
 *   double gain = strtod(token, &end);
 *   if (end == token) { gain = 1.0; }
 */
double strtod(const char *nptr, char **endptr);

/**
 * Function: strtof
 * Summary: Converts the leading decimal field of a C-string to a float, reporting where the field ended.
 * Parameters:
 *   - nptr: The string containing the representation of a floating-point number.
 *   - endptr: Receives a pointer to the first character not consumed; pass NULL to discard it. When no conversion occurs it receives nptr itself.
 * Returns: The converted value, narrowed to float; out-of-range magnitudes saturate to infinity or zero. Prefer this over atof() whenever a parse failure must be detectable, or when walking a line token by token.
 * Include: <stdlib.h>
 * Example:
 *   char *end = NULL;
 *   float speed = strtof(argument, &end);
 *   const char *next = end; // carry on scanning from here
 */
float strtof(const char *nptr, char **endptr);

#endif // 0
#endif // JET_INTELLISENSE_H
