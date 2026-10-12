// runtime/jet_gui.h
#ifndef JET_GUI_H
#define JET_GUI_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Buttons, checkboxes, sliders, labels and panels that your cartridge draws
 * for itself, in the immediate-mode style. Immediate mode means there is no
 * widget object to make and keep: one call both draws the button and answers
 * whether it was clicked, and you make that call every frame. Each widget
 * reads the mouse, works out whether it is hovered, pressed or dragged, draws
 * itself, and hands back its result (a click, the new slider value, and so
 * on). Because the call draws, make it from _draw, once per frame, with the
 * same arguments every frame. The widget that holds the mouse from press to
 * release is remembered in one shared variable, _jet_gui_active_id, so a
 * widget has to be called every frame for a press on it to be followed
 * through to its release.
 *
 * Reading the mouse from _draw is safe on every host (the program that runs
 * your cartridge). A frame of the host may carry several ticks of your game
 * but draws only once, so inside _draw every press-or-release question answers
 * for the whole stretch since the previous draw, and no click is lost to a
 * display slower than the tick rate. See docs/architecture.md section 3.D.
 *
 * This style suits debugging overlays, menus and HUDs. If you keep to the
 * usual split, with input handled in _update and drawing in _draw, use the
 * stateful widgets in jet_gui_stateful.h instead. Each of those is two calls,
 * an _update half and a _draw half, around a JetGuiState that you own, so
 * several widgets do not have to share one global active id.
 *
 * Complete settings-overlay example (copy into main.c):
 *
 *   #include "jet64.h"
 *   #include "jet_gui.h"
 *
 *   static float music_volume = 1.0f;
 *   static bool music_enabled = true;
 *
 *   void _init(void) {}
 *   void _update(float dt) { (void)dt; }
 *
 *   void _draw(void) {
 *       jet_cls(0);
 *       jet_gui_panel(8, 8, 180, 96, "Settings");
 *       jet_gui_checkbox(20, 36, 12, 12, "Music", &music_enabled);
 *       music_volume = jet_gui_slider(20, 64, 140, 10, music_volume, 0.0f, 1.0f);
 *   }
 *
 *   void _end(void) {}
 */

extern uint32_t _jet_gui_active_id;

/**
 * Struct: JetGuiStyle
 * Summary: The colours, font and spacing that every GUI widget draws with, in this module and in jet_gui_stateful.h.
 * Fields:
 *   - font: Which font to draw labels in, exactly as TextFormat.font takes it: a Font ROM bank from 0 to 3, or JET_FONT_SPRITE(0..3) for a sprite font you have registered.
 *   - text_size: The height of the text in pixels. Use a size the chosen font supports.
 *   - base_color: The fill colour of a control nobody is touching, as a palette index: a number from 0 to 255 that picks one of the palette's colours.
 *   - border_color: The colour of outlines, as a palette index.
 *   - text_color: The colour of labels, as a palette index.
 *   - hover_color: The fill colour of a control the mouse is over, as a palette index.
 *   - active_color: The fill colour of a control being pressed or dragged, as a palette index.
 *   - padding: The spacing inside a control, in pixels.
 * Include: jet_gui.h
 * Notes: Make a complete value with jet_make_gui_style, then put it in place with jet_gui_set_style. Every colour is a palette index, not an RGB value. There is one style for both GUI modules, so set it once in _init, or when you enter a screen with its own look.
 *
 *        The font field goes straight to TextFormat.font, so a set of widgets can use a cartridge's own sprite typeface. A sprite font ignores text_color and text_size, as it does in every other text call, and labels still sit centred because the widgets measure their text with jet_text_width.
 * Example:
 *   JetGuiStyle style = jet_make_gui_style(0, 8, 1, 6, 15, 12, 10, 4);
 *   jet_gui_set_style(style);
 */
typedef struct {
    uint8_t font;          // Font selector: ROM bank 0-3, or JET_FONT_SPRITE(0..3)
    uint8_t text_size;     // Rendered text height/size
    uint8_t base_color;    // Background
    uint8_t border_color;  // Outline
    uint8_t text_color;    // Text
    uint8_t hover_color;   // Hovering interaction
    uint8_t active_color;  // Pressed interaction
    uint8_t padding;
} JetGuiStyle;

/**
 * Function: jet_make_gui_style
 * Summary: Builds a JetGuiStyle in one call, ready to give to jet_gui_set_style.
 * Parameters:
 *   - font: Which font to draw labels in: a Font ROM bank from 0 to 3, or JET_FONT_SPRITE(0..3) for a sprite font you have registered.
 *   - text_size: The height of the text in pixels.
 *   - base_color: The fill colour of a control nobody is touching, as a palette index: a number from 0 to 255 that picks one of the palette's colours.
 *   - border_color: The colour of outlines, as a palette index.
 *   - text_color: The colour of labels, as a palette index.
 *   - hover_color: The fill colour of a control the mouse is over, as a palette index.
 *   - active_color: The fill colour of a control being pressed, as a palette index.
 *   - padding: The spacing inside a control, in pixels.
 * Returns: A filled-in JetGuiStyle, handed back by value.
 * Include: jet_gui.h
 * Notes: The colour arguments are palette indices from 0 to 255, not RGB values. The value holds its own copy of every field and owns nothing else, so you may keep it, or change its fields, before you pass it to jet_gui_set_style.
 * Example:
 *   JetGuiStyle style = jet_make_gui_style(0, 8, 0, 5, 7, 12, 8, 4);
 */
static inline JetGuiStyle jet_make_gui_style(uint8_t font, uint8_t text_size, uint8_t base_color, uint8_t border_color, uint8_t text_color, uint8_t hover_color, uint8_t active_color, uint8_t padding) {
    JetGuiStyle style;
    style.font = font;
    style.text_size = text_size;
    style.base_color = base_color;
    style.border_color = border_color;
    style.text_color = text_color;
    style.hover_color = hover_color;
    style.active_color = active_color;
    style.padding = padding;
    return style;
}

extern JetGuiStyle jet_gui_style;

/**
 * Function: jet_gui_set_style
 * Summary: Makes a JetGuiStyle the one every GUI widget draws with from now on.
 * Parameters:
 *   - style: The complete style to copy into the shared style variable.
 * Returns: Nothing.
 * Include: jet_gui.h
 * Notes: The style is kept in one variable, jet_gui_style, which this header declares. Call this from your cartridge before you draw the widgets that should use the new colours.
 * Example:
 *   JetGuiStyle style = jet_gui_style;
 *   style.font = 1;
 *   style.text_size = 12;
 *   jet_gui_set_style(style);
 */
void jet_gui_set_style(JetGuiStyle style);

/**
 * Function: _jet_gui_hover
 * Summary: Tests whether the mouse is inside a GUI rectangle in camera-aware coordinates.
 * Parameters:
 *   - x: Rectangle left edge in world/screen pixels.
 *   - y: Rectangle top edge in world/screen pixels.
 *   - w: Rectangle width in pixels.
 *   - h: Rectangle height in pixels.
 * Returns: true when the current mouse position is inside the rectangle; otherwise false.
 * Include: jet_gui.h
 * Internal: Implementation helper; omitted from the creator-facing Codex.
 * Notes: Internal helper for the immediate-mode GUI widgets. It adds the active camera offset so UI can live in the same coordinate space as rendered controls.
 * Example:
 *   bool hovered = _jet_gui_hover(10, 10, 80, 16);
 */
bool _jet_gui_hover(int x, int y, int w, int h);

/**
 * Function: jet_gui_button
 * Summary: Draws a button and tells you whether it was clicked. A click is a press on the button followed by a release on it.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - text: The label, centred in the button, as a null-terminated string. NULL draws no label.
 * Returns: true only on the frame the left mouse button is released over this button while the button holds the press; otherwise false.
 * Include: jet_gui.h
 * Notes: Call it every frame from _draw. The position is a world position: the camera is taken into account for the mouse test as well as for the drawing, so with the camera at zero it is a plain screen position.
 *
 *        When you press on the button it takes hold of the mouse, remembered in the shared active widget id, and keeps it until you let go. For a finished game's interface, driven from _update, prefer jet_gui_stateful_button_update and jet_gui_stateful_button_draw.
 * Example:
 *   if (jet_gui_button(12, 12, 72, 18, "Start")) { start_game(); }
 */
bool jet_gui_button(int x, int y, int w, int h, const char* text);

/**
 * Function: jet_gui_checkbox
 * Summary: Draws a checkbox and flips a bool of yours each time it is clicked.
 * Parameters:
 *   - x: The left edge of the box, in world pixels.
 *   - y: The top edge of the box, in world pixels.
 *   - w: The width of the box in pixels.
 *   - h: The height of the box in pixels.
 *   - text: The label, drawn to the right of the box, as a null-terminated string. NULL draws no label.
 *   - checked: A pointer to the bool the box shows and flips on a click, or NULL to only report clicks.
 * Returns: true on the frame the checkbox is clicked; otherwise false.
 * Include: jet_gui.h
 * Notes: Call it every frame from _draw. The position is a world position, so the camera is taken into account for the mouse test as well as for the drawing. With NULL for checked the widget still reports clicks but changes nothing of yours.
 * Example:
 *   jet_gui_checkbox(8, 40, 12, 12, "Music", &music_enabled);
 */
bool jet_gui_checkbox(int x, int y, int w, int h, const char* text, bool* checked);

/**
 * Function: jet_gui_slider
 * Summary: Draws a horizontal slider and gives you back its value, which changes while you drag it.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - value: The slider's current value.
 *   - min: The smallest value the slider can give.
 *   - max: The largest value the slider can give.
 * Returns: The value you passed in, or, while the slider is being dragged, the new value, which lies between min and max.
 * Include: jet_gui.h
 * Notes: Call it every frame from _draw, and store what it returns, as the example does. The position is a world position, so the camera is taken into account for the mouse test as well as for the drawing.
 *
 *        Once you press on the slider it keeps the drag for as long as the left mouse button is held down. If max equals min, or the width is zero or less, the slider has no place to put its fill, so the value comes back unchanged.
 * Example:
 *   volume = jet_gui_slider(20, 64, 100, 10, volume, 0.0f, 1.0f);
 */
float jet_gui_slider(int x, int y, int w, int h, float value, float min, float max);

/**
 * Function: jet_gui_label
 * Summary: Draws a line of text in the style's text colour.
 * Parameters:
 *   - x: Where the text starts, across, in world pixels.
 *   - y: Where the text starts, down, in world pixels.
 *   - w: Reserved for layout; not used yet.
 *   - h: Reserved for layout; not used yet.
 *   - text: The text to draw, as a null-terminated string. NULL draws nothing.
 * Returns: Nothing.
 * Include: jet_gui.h
 * Notes: The width and height are there so every widget takes the same shape of arguments; the label ignores them on purpose.
 * Example:
 *   jet_gui_label(8, 8, 120, 8, "Inventory");
 */
void jet_gui_label(int x, int y, int w, int h, const char* text);

/**
 * Function: jet_gui_panel
 * Summary: Draws a box with a border, and a title bar across the top if you give it a title.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - title: The title, as a null-terminated string, drawn in a bar 16 pixels tall across the top of the panel. NULL draws no bar.
 * Returns: Nothing.
 * Include: jet_gui.h
 * Notes: A panel is only a picture to group other widgets in. It never takes the mouse.
 * Example:
 *   jet_gui_panel(4, 4, 160, 96, "Debug");
 */
void jet_gui_panel(int x, int y, int w, int h, const char* title);

#endif // JET_GUI_H
