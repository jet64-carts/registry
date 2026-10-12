// runtime/jet_text.h
#ifndef JET_TEXT_H
#define JET_TEXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Put words on the screen. There are two kinds of typeface to print with. The
// four Font ROM banks hold typefaces baked in the Font Editor; their letters
// are shapes with no colour of their own, so you print them in any palette
// colour, at a size from 8 to 128 pixels. A sprite font is a colour typeface
// whose letters live as cells in the sprite sheet, drawn in the Sprite Editor
// or baked in the Sprite Font Lab; register one with jet_sprite_font and it
// prints in the colours the artist chose.
//
// jet_print is the simple one-line call. For a size, spacing, a dithered fade,
// a drop shadow, or an effect on each character, fill in a TextFormat and call
// jet_print_ext. jet_text_width measures a string so you can centre it, as the
// example does.
//
// Complete styled-title example (copy into main.c):
//
//   #include "jet64.h"
//
//   static TextFormat title;
//
//   void _init(void) {
//       title = jet_make_text_format(0, 15, 32, 1, 0, 1.0f, 2, 1);
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       const char* message = "READY";
//       const int x = (JET_WIDTH - jet_text_width(message, &title)) / 2;
//       jet_cls(0);
//       jet_print_ext(message, x, JET_HEIGHT / 3, &title);
//   }
//
//   void _end(void) {}

// How many Font ROM banks there are. TextFormat.font picks one with a number
// from 0 to JET_FONT_BANK_COUNT-1; a value at or above the count currently
// draws nothing. The number lives here, rather than as a bare 4 inside
// jet_text.c, so that your code and any future font code agree on one value.
#define JET_FONT_BANK_COUNT 4

/**
 * Struct: TextFormat
 * Summary: How a piece of text is drawn: which typeface, what colour, how big, how spaced, how faded, whether it has a shadow, and any effect applied to each character on its own. Give one to jet_print_ext.
 * Fields:
 *   - font: Which typeface. 0 to 3 picks a Font ROM bank; JET_FONT_SPRITE(0) to JET_FONT_SPRITE(3), which is 4 to 7, picks a registered sprite font.
 *   - color: The colour of the letters, as a palette index: a number from 0 to 255 that picks one of the palette's colours. A sprite font keeps the colours the artist chose and ignores this.
 *   - size: How tall the letters should be, in pixels, from 8 to 128. A sprite font ignores this and uses the height of its own cells.
 *   - spacing_x: Extra space after each character, in pixels.
 *   - spacing_y: Extra space between lines, in pixels.
 *   - alpha: How solid the text is, drawn as a dither: a pattern of drawn and skipped pixels that reads as a fade. 0 or less is invisible; 1 or more is solid.
 *   - shadow_offset: How far the drop shadow sits from the letters, in pixels. 0 means no shadow.
 *   - shadow_color: The colour of the shadow, as a palette index.
 *   - char_colors: A colour for each character, one palette index per character, or NULL for none. A sprite font ignores it.
 *   - char_y_offsets: How far to move each character up or down, in pixels, one entry per character, or NULL for none.
 *   - char_x_spacings: A change to how far the pen moves on after each character, in pixels, one entry per character, or NULL for none.
 *   - palette_map: A table of 256 palette indexes that recolours a sprite font: for each colour the letters carry, the colour to draw instead. NULL keeps the artist's colours. A Font ROM bank ignores it.
 * Include: jet64.h
 * Notes: Make one with jet_make_text_format, so that no field is left unset. Any per-character array you give must hold one entry for every byte of the string, counting each newline as a byte, and must stay in memory until jet_print_ext has returned. A palette_map you give must stay in memory in the same way.
 *
 *        palette_map is to a sprite font what color is to a Font ROM bank. A Font ROM letter has no colour of its own, so its colour is simply your choice. A sprite font's letters were coloured by the artist, so recolouring them means saying, for each colour they carry, which colour to draw instead. jet_palette_ink_map builds that table for you from the two colours the face was baked between.
 * Example:
 *   TextFormat title = jet_make_text_format(0, 15, 32, 1, 2, 1.0f, 2, 0);
 *   jet_print_ext("READY", 40, 24, &title);
 */
typedef struct {
    uint8_t font;      // Which typeface. 0..JET_FONT_BANK_COUNT-1 (0 to 3) picks a
                       // Font ROM bank; JET_FONT_SPRITE(0..3), which is 4 to 7, picks
                       // a registered sprite font (see JetSpriteFontDef below).
    uint8_t color;     // The colour of every letter pixel, as a palette index (a sprite font ignores it).
    uint8_t size;      // How tall the letters should be, in pixels. The engine
                       // uses the largest baked size that does not go over this
                       // (the baked sizes are 8, 10, 12, 14, 16, 20, 24, 32, 48,
                       // 64, 80, 96 and 128); a request below 8 uses 8.
    int spacing_x;     // Extra pixels between characters.
    int spacing_y;     // Extra pixels between lines.
    float alpha;       // How solid the text is, 0.0 to 1.0, drawn with a 4x4
                       // Bayer dither: a fixed pattern of drawn and skipped
                       // pixels that reads as a fade. Nothing is clamped here;
                       // jet_print_ext() treats alpha <= 0 as invisible and any
                       // value >= 1 as solid.

    int shadow_offset;    // How far the drop shadow sits from the letters, in pixels (0 = no shadow).
    uint8_t shadow_color; // The colour of the drop shadow, as a palette index.

    // Optional arrays with one entry for each character. Pass NULL to skip one;
    // an array you do give must hold one entry per character in the string,
    // counting newline characters (each newline takes one slot).
    const uint8_t* char_colors;    // A palette index for each character.
    const int* char_y_offsets;     // An up-or-down shift for each character, in pixels.
    const int* char_x_spacings;    // Extra horizontal advance for each character, in pixels.

    // Optional recolour table for a sprite font: 256 entries, read as
    // palette_map[c] for every letter pixel whose colour is c. NULL keeps the
    // colours the artist baked. A Font ROM bank ignores it: its letter colour
    // is already your own choice (color, or char_colors), so there is nothing
    // to swap. The drop shadow is never recoloured either, because
    // shadow_color is your choice too. So one face plus one table gives a
    // whole palette of voices, at the cost of one table lookup per drawn pixel
    // and no sprite cells at all (see JetSpriteFontDef). Build the table with
    // jet_palette_ink_map (jet_palette.h) rather than by hand. Painting one
    // flat colour over every visible pixel is what the drop shadow already
    // does, and it would throw away the soft anti-aliased edge and the outline
    // ring the bake paid for.
    const uint8_t* palette_map;
} TextFormat;

/**
 * Function: jet_make_text_format
 * Summary: Builds a TextFormat from the everyday settings, with every per-character array and the palette_map set to NULL.
 * Parameters:
 *   - font: Which typeface: a Font ROM bank (0 to 3) or a sprite font (JET_FONT_SPRITE(0) to JET_FONT_SPRITE(3), which is 4 to 7).
 *   - color: The colour of the letters, as a palette index from 0 to 255. A sprite font ignores it and keeps its own colours.
 *   - size: How tall the letters should be, in pixels, from 8 to 128.
 *   - spacing_x: Extra space between characters, in pixels.
 *   - spacing_y: Extra space between lines, in pixels.
 *   - alpha: How solid the text is, from 0.0f (invisible) to 1.0f (solid).
 *   - shadow_offset: How far the drop shadow sits from the letters, in pixels. 0 means no shadow.
 *   - shadow_color: The colour of the shadow, as a palette index from 0 to 255.
 * Returns: The filled-in TextFormat, by value.
 * Include: jet_text.h
 * Notes: The three per-character arrays and palette_map are set to NULL. The value owns no memory, so you can copy it freely.
 * Example:
 *   TextFormat fmt = jet_make_text_format(0, 15, 16, 1, 1, 1.0f, 1, 0);
 */
static inline TextFormat jet_make_text_format(
    uint8_t font,
    uint8_t color,
    int size,
    int spacing_x,
    int spacing_y,
    float alpha,
    int shadow_offset,
    uint8_t shadow_color
) {
    TextFormat fmt;
    fmt.font = font;
    fmt.color = color;
    fmt.size = (uint8_t)size; // A height of 8 to 128 fits in the byte-sized field.
    fmt.spacing_x = spacing_x;
    fmt.spacing_y = spacing_y;
    fmt.alpha = alpha;
    fmt.shadow_offset = shadow_offset;
    fmt.shadow_color = shadow_color;
    fmt.char_colors = NULL;
    fmt.char_y_offsets = NULL;
    fmt.char_x_spacings = NULL;
    fmt.palette_map = NULL;
    return fmt;
}

/**
 * Function: jet_text_set_color
 * Summary: Changes a TextFormat's colour and hands the same format back, so you can set it inside a call.
 * Parameters:
 *   - fmt: The format to change, or NULL to do nothing.
 *   - color: The new colour of the letters, as a palette index from 0 to 255.
 * Returns: The same fmt you passed in, NULL included.
 * Include: jet_text.h
 * Notes: It allocates nothing. It is for tucking a setting into the jet_print_ext call itself, as the example does.
 * Example:
 *   TextFormat fmt = { .font = 0, .color = 7, .size = 16, .spacing_x = 1, .spacing_y = 1, .alpha = 1.0f };
 *   jet_print_ext("ready", 8, 8, jet_text_set_color(&fmt, 12));
 */
static inline TextFormat* jet_text_set_color(TextFormat* fmt, uint8_t color) {
    if (fmt) fmt->color = color;
    return fmt;
}

/**
 * Function: jet_text_set_alpha
 * Summary: Changes how solid a TextFormat's text is and hands the same format back, so you can set it inside a call.
 * Parameters:
 *   - fmt: The format to change, or NULL to do nothing.
 *   - alpha: How solid the text should be, from 0.0f to 1.0f, drawn with a 4x4 Bayer dither: a fixed pattern of drawn and skipped pixels that reads as a fade.
 * Returns: The same fmt you passed in, NULL included.
 * Include: jet_text.h
 * Notes: A value outside 0.0f to 1.0f is stored as it is, not clamped. Keep your values inside that range so the text looks the way you expect.
 * Example:
 *   jet_text_set_alpha(&fmt, 0.5f);
 *   jet_print_ext("ghost", x, y, &fmt);
 */
static inline TextFormat* jet_text_set_alpha(TextFormat* fmt, float alpha) {
    if (fmt) fmt->alpha = alpha;
    return fmt;
}

/**
 * Function: jet_text_set_shadow
 * Summary: Sets a TextFormat's drop shadow and hands the same format back, so you can set it inside a call.
 * Parameters:
 *   - fmt: The format to change, or NULL to do nothing.
 *   - offset: How far the shadow sits from the letters, in pixels. 0 turns the shadow off.
 *   - color: The colour of the shadow, as a palette index from 0 to 255.
 * Returns: The same fmt you passed in, NULL included.
 * Include: jet_text.h
 * Notes: jet_print_ext draws the shadow first and the letters on top of it. A shadow keeps score text and labels readable over a busy background.
 * Example:
 *   jet_print_ext("SCORE", 4, 4, jet_text_set_shadow(&fmt, 1, 0));
 */
static inline TextFormat* jet_text_set_shadow(TextFormat* fmt, int offset, uint8_t color) {
    if (fmt) {
        fmt->shadow_offset = offset;
        fmt->shadow_color = color;
    }
    return fmt;
}

/**
 * Function: _init_text_engine
 * Summary: Initializes the runtime text subsystem state.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_text.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Internal engine/bootstrap hook. Cartridge code normally uses jet_print(), jet_print_ext(), jet_text_width(), and jet_text_char_width() directly instead of calling this.
 * Example:
 *   _init_text_engine();
 */
void _init_text_engine(void);

/**
 * Function: jet_text_char_width
 * Summary: Tells you how wide one character is, in pixels, for a Font ROM bank at a given size. Every character in a bank is the same width.
 * Parameters:
 *   - font: Which Font ROM bank, 0 to 3.
 *   - size: How tall the letters are, in pixels: the same number you put in a TextFormat's size.
 * Returns: The width of one character cell, in pixels, at the size the bank actually has; or 8 if the bank holds no usable font.
 * Include: jet_text.h
 * Notes: The size is rounded down: the largest baked size that does not go over your request is the one used, and a request smaller than the smallest baked size gets the smallest. This is the same distance jet_print_ext moves along for each character, so use it to line text up by hand or to lay out a score panel. A bank number outside 0 to 3 gets the fallback width of 8.
 * Example:
 *   int label_w = jet_text_char_width(fmt.font, fmt.size) * 5;
 */
int jet_text_char_width(uint8_t font, uint8_t size);

/**
 * Function: jet_text_width
 * Summary: Measures how wide a string will be when printed, in pixels, using the same spacing rules as jet_print_ext.
 * Parameters:
 *   - text: The text to measure, a C string ending in a zero byte.
 *   - fmt: The format that says which typeface, size and spacing to measure with. NULL gives 0.
 * Returns: The width of the widest line, in pixels; or 0 if text or fmt is NULL.
 * Include: jet_text.h
 * Notes: A string with newlines in it is measured line by line, and you get the widest line, not the lines added together. The spacing that would follow the last character is left off, just as jet_print_ext leaves it off.
 * Example:
 *   int w = jet_text_width("GAME OVER", &fmt);
 *   jet_print_ext("GAME OVER", (JET_WIDTH - w) / 2, 80, &fmt);
 */
int jet_text_width(const char* text, const TextFormat* fmt);

/**
 * Function: jet_print
 * Summary: Prints text in one colour with the everyday settings. This is the simplest way to put words on the screen.
 * Parameters:
 *   - text: The text to draw, a C string ending in a zero byte.
 *   - x: The left edge, in world pixels, which are screen pixels until you move the camera.
 *   - y: The top edge, in world pixels.
 *   - color: The colour of the letters, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet_text.h
 * Notes: It calls jet_print_ext with Font ROM bank 0, size 12, spacing 1 and full opacity. A NULL text draws nothing. For a shadow, a fade, another size, or an effect on each character, use jet_print_ext.
 * Example:
 *   jet_print("hello", 10, 10, 7);
 */
void jet_print(const char* text, int x, int y, uint8_t color);

/**
 * Function: jet_print_terminal
 * Summary: Prints clear 16-pixel text with the system terminal font, which every cartridge has.
 * Parameters:
 *   - text: The text to draw, a C string ending in a zero byte.
 *   - x: The left edge, in world pixels, which are screen pixels until you move the camera.
 *   - y: The top edge, in world pixels.
 *   - color: The colour of the letters, as a palette index from 0 to 255.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: It uses Font ROM bank 3 at size 16, with 1 pixel between characters, 2 pixels between lines, full opacity, and a shadow 1 pixel away in palette index 0. Bank 3 is filled for every cartridge, so this is the quick way to print a heading, an instruction, or a label on the score panel. It allocates nothing, respects the camera and the clip rectangle, and a NULL text draws nothing. To change any of those settings, use jet_print_ext.
 * Example:
 *   jet_print_terminal("ESC FOR MENU", 16, 16, 15);
 */
void jet_print_terminal(const char* text, int x, int y, uint8_t color);

/**
 * Function: jet_print_ext
 * Summary: Prints text with a Font ROM bank or a sprite font, the way a TextFormat describes: size, spacing, fade, shadow, and any effect on each character.
 * Parameters:
 *   - text: The text to draw, a C string ending in a zero byte. A newline character starts a new line.
 *   - x: The left edge, in world pixels, which are screen pixels until you move the camera.
 *   - y: The top edge, in world pixels.
 *   - fmt: The TextFormat that says how to draw it: colour, size, spacing, fade, shadow, and the per-character arrays if you use them.
 * Returns: Nothing.
 * Include: jet_text.h
 * Notes: It respects the camera and the clip rectangle. A NULL text or fmt draws nothing, and so does an alpha of 0 or less. If char_colors, char_y_offsets or char_x_spacings is not NULL, it must hold one entry for every character in the string, counting each newline. If palette_map is not NULL it must hold 256 entries; it recolours only the letters of a sprite font, and leaves the drop shadow and Font ROM text alone.
 * Example:
 *   TextFormat fmt = { .font = 0, .color = 15, .size = 16, .spacing_x = 1, .spacing_y = 2, .alpha = 1.0f,
 *                      .shadow_offset = 1, .shadow_color = 0 };
 *   jet_print_ext("Jet64", 24, 24, &fmt);
 */
void jet_print_ext(const char* text, int x, int y, const TextFormat* fmt);

// ============================ Sprite fonts ==================================
// A sprite font is a colour typeface of your own (the design is written up in
// docs/sprite_fonts/design.md). Its letters are ordinary cells in the sprite
// sheet, and you choose it with a TextFormat.font of JET_FONT_SPRITE(0) to
// JET_FONT_SPRITE(3), which is 4 to 7. Every text call prints one without any
// change on your part. color and size are ignored, because the artist chose
// those; spacing, the alpha dither, the drop shadow, the per-character y
// offsets and x spacings, clipping, the camera, and drawing into a target of
// your own all apply exactly as they do for a Font ROM bank.
//
// TextFormat.palette_map is the one setting that answers the artist back. It
// is a table of 256 entries that every letter pixel is drawn through, so one
// baked face can speak in as many colours as you care to build tables for,
// without a second cell of sprite memory. jet_palette_ink_map (jet_palette.h)
// builds that table from the two colours the bake declared, and that one call
// is all you write to gain a new voice.
//
// Letters are spaced proportionally: how far each one advances is measured
// from its own pixels when the font is registered. An outline ring declared
// with JetSpriteFontDef.outline_w is drawn but never counted in that distance;
// the measurements come from the letter shape inside the ring, so an outlined
// face is still spaced by its letters.

#define JET_FONT_SPRITE_SLOTS  4                       // How many sprite-font slots there are (TextFormat.font 4 to 7).
#define JET_FONT_SPRITE(slot)  ((uint8_t)(JET_FONT_BANK_COUNT + (slot)))

/**
 * Constant: JET_SFONT_DEFAULTS
 * Summary: The plain choice for a sprite font's flags: letters spaced by their own width, the mask you painted deciding what shows, and lowercase left as it is.
 * Include: jet64.h
 * Notes: Give it as JetSpriteFontDef.flags, or to jet_sprite_font, when you want none of the optional behaviours.
 * Example:
 *   jet_sprite_font(0, 64, 8, 8, JET_SFONT_DEFAULTS);
 */
#define JET_SFONT_DEFAULTS   0u
/**
 * Constant: JET_SFONT_MONO
 * Summary: Makes every letter of a sprite font advance by the full cell width, cell_w, instead of by the width of its own ink.
 * Include: jet64.h
 * Notes: Combine it into JetSpriteFontDef.flags with a bitwise OR, the | operator. Good for a terminal, a scoreboard, or anything laid out on a grid.
 * Example:
 *   uint32_t flags = JET_SFONT_MONO | JET_SFONT_FOLD_UPPER;
 */
#define JET_SFONT_MONO       (1u << 0)
/**
 * Constant: JET_SFONT_FOLD_UPPER
 * Summary: Prints lowercase letters with the uppercase cells, for a small sprite font of 64 characters that has no lowercase.
 * Include: jet64.h
 * Notes: Combine it into JetSpriteFontDef.flags with a bitwise OR, the | operator, when the sheet holds only character codes 32 to 95.
 * Example:
 *   jet_sprite_font(0, 64, 8, 8, JET_SFONT_FOLD_UPPER);
 */
#define JET_SFONT_FOLD_UPPER (1u << 1)
/**
 * Constant: JET_SFONT_COLOR_KEY
 * Summary: Makes palette index 0 see-through in a sprite font's letters, instead of letting the Sprite Editor mask decide what shows.
 * Include: jet64.h
 * Notes: Combine it into JetSpriteFontDef.flags with a bitwise OR, the | operator, only for a font whose index-0 pixels are meant to be see-through. Leave it out when the artwork uses index 0 as real black that the painted mask keeps visible.
 * Example:
 *   jet_sprite_font(0, 64, 8, 8, JET_SFONT_COLOR_KEY);
 */
#define JET_SFONT_COLOR_KEY  (1u << 2)

/**
 * Struct: JetSpriteFontDef
 * Summary: Describes a sprite font: a colour typeface whose letters sit in a run of cells, one after another, in the sprite sheet.
 * Fields:
 *   - grid_index: The editor cell number of the first letter, which is the space character (ASCII 32).
 *   - cell_w: The width of each letter cell, in pixels: 8 to 128, and a multiple of 8.
 *   - cell_h: The height of each letter cell, in pixels, in the same range. This is also the height of a line.
 *   - flags: Any of JET_SFONT_DEFAULTS, JET_SFONT_MONO, JET_SFONT_FOLD_UPPER and JET_SFONT_COLOR_KEY, combined with a bitwise OR.
 *   - space_width: How far a space advances, in pixels. 0 lets Jet64 work it out.
 *   - outline_w: The width, in pixels, of the outline ring baked around every letter. 0 means there is none.
 * Include: jet64.h
 * Notes: jet_sprite_font_set copies this record and measures every letter's advance straight away; the pixels themselves stay in the sprite sheet. When outline_w is not zero, the advances and jet_text_width measure the letter shape inside the ring, and the ring hangs outside the pen position, as the drop shadow does, so neighbouring rings overlap underneath the letters. For the usual printable-ASCII font, jet_sprite_font sets this up in one line.
 * Example:
 *   JetSpriteFontDef font = { .grid_index = 64, .cell_w = 8, .cell_h = 8, .flags = JET_SFONT_DEFAULTS, .space_width = 4 };
 *   bool ok = jet_sprite_font_set(0, &font);
 */
typedef struct {
    uint32_t grid_index;  // The cell of the space character (ASCII 32), counted in cells;
                          //   cells per sheet row = JET_SPRITE_SHEET_WIDTH / cell_w.
    uint16_t cell_w;      // Cell width in pixels: 8 to 128, a multiple of 8.
    uint16_t cell_h;      // Cell height in pixels: 8 to 128, a multiple of 8. Also the line height.
    uint32_t flags;       // JET_SFONT_* flags, combined with bitwise OR.
    uint8_t  space_width; // How far a space advances, in pixels; 0 = automatic (cell_w/2, or cell_w with MONO).
    uint8_t  outline_w;   // Width of the baked outline ring in pixels; 0 = none. The ring is drawn but
                          //   never counted in the advance: the measurements come from the letter shape inside it.
} JetSpriteFontDef;

/**
 * Function: jet_sprite_font_set
 * Summary: Registers a sprite font in one of the four slots, or replaces the one there, measuring each letter's width from its pixels in the sprite sheet.
 * Parameters:
 *   - slot: Which slot, 0 to JET_FONT_SPRITE_SLOTS-1. Later you choose the font with JET_FONT_SPRITE(slot).
 *   - def: The description of the font: cell size, where the first letter is, flags, space width. NULL is refused.
 * Returns: true when the slot is ready to print with. false, with the slot left as it was, when the slot number or the cell size is bad, when the run of letter cells would go off the sheet, or when outline_w is so wide (2 * outline_w >= cell_w) that no letter could fit inside the ring.
 * Include: jet_text.h
 * Notes: Call it in _init every time, without checking first. A registration belongs to the running cartridge and is wiped each time a cartridge starts, and it costs microseconds.
 *
 *        It looks over each letter cell once and records the first and last column that hold ink, so the width each letter advances comes straight from its pixels. JET_SFONT_MONO skips that trim, and records outline_w as zero, because a letter that advances a full cell needs no ring trimmed off. When outline_w is not zero, each advance is narrowed to the letter shape inside the ring; the ring still draws, hanging over the pen position as the drop shadow does. Which pixels show is decided by the mask plane, unless JET_SFONT_COLOR_KEY makes index 0 see-through instead.
 *
 *        Registering a slot again, even one you have already printed with, measures it again. That is how a cartridge takes up letter cells it has edited while running.
 * Example:
 *   JetSpriteFontDef def = { .grid_index = 512, .cell_w = 16, .cell_h = 16, .flags = JET_SFONT_FOLD_UPPER, .space_width = 0 };
 *   jet_sprite_font_set(0, &def);
 */
bool jet_sprite_font_set(uint8_t slot, const JetSpriteFontDef* def);

/**
 * Function: jet_sprite_font
 * Summary: Registers a sprite font in one line, letting Jet64 work out the width of a space.
 * Parameters:
 *   - slot: Which slot, 0 to JET_FONT_SPRITE_SLOTS-1.
 *   - grid_index: The editor cell number of the space character, counted in cells; there are JET_SPRITE_SHEET_WIDTH / cell_w cells in each row of the sheet.
 *   - cell_w: The width of each letter cell, in pixels: 8 to 128, a multiple of 8.
 *   - cell_h: The height of each letter cell, in pixels: 8 to 128, a multiple of 8. This is also the height of a line.
 *   - flags: JET_SFONT_* flags combined with a bitwise OR, or JET_SFONT_DEFAULTS for none.
 * Returns: true when the font is ready; false for the same reasons jet_sprite_font_set gives.
 * Include: jet_text.h
 * Notes: It is the same as calling jet_sprite_font_set with space_width = 0 (automatic) and outline_w = 0. This is the one line a cartridge adds to _init to switch on a sprite font drawn by hand or baked in the Sprite Font Lab. A face baked with an outline must go through jet_sprite_font_set instead, so the ring is declared.
 * Example:
 *   jet_sprite_font(0, 512, 16, 16, JET_SFONT_FOLD_UPPER);
 */
bool jet_sprite_font(uint8_t slot, uint32_t grid_index, int cell_w, int cell_h, uint32_t flags);

/**
 * Function: jet_sprite_font_clear
 * Summary: Switches a sprite-font slot off. Text printed with it draws nothing until you register a font there again.
 * Parameters:
 *   - slot: Which slot, 0 to JET_FONT_SPRITE_SLOTS-1. A number outside that range is ignored.
 * Returns: Nothing.
 * Include: jet_text.h
 * Notes: A slot that was never registered, or has been cleared, draws nothing and measures nothing, the same as an empty Font ROM bank.
 * Example:
 *   jet_sprite_font_clear(0);
 */
void jet_sprite_font_clear(uint8_t slot);

/**
 * Function: jet_text_line_height
 * Summary: Tells you how tall one line of text is, in pixels, for either kind of font.
 * Parameters:
 *   - fmt: The format that says which font, and for a Font ROM bank which size. NULL gives 0.
 * Returns: The height of one line in pixels, not counting spacing_y; or a small fallback value when no usable font sits behind the font number.
 * Include: jet_text.h
 * Notes: spacing_y is left out, just as jet_text_char_width leaves the spacing out. Add spacing_y yourself when you lay out several lines by hand. For a sprite font the answer is the cell_h you registered; for a Font ROM bank it is the cell height of the size the bank actually has.
 * Example:
 *   int line = jet_text_line_height(&fmt) + fmt.spacing_y;
 */
int jet_text_line_height(const TextFormat* fmt);

// Short names kept for older cartridges.
#define print jet_print
#define print_ext jet_print_ext

#endif // JET_TEXT_H
