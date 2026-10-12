// runtime/jet_gui_stateful.h
#ifndef JET_GUI_STATEFUL_H
#define JET_GUI_STATEFUL_H

#include "jet_gui.h" // IWYU pragma: keep

/*
 * Buttons, checkboxes and sliders for a game that keeps to the usual split:
 * input handled in _update, drawing in _draw. They are the two-call partners
 * of the one-call widgets in jet_gui.h. Each interactive widget comes in two
 * halves. The update half (jet_gui_stateful_button_update, and so on) reads
 * the mouse and works out whether the widget is hovered, pressed or clicked;
 * call it from _update. The draw half (jet_gui_stateful_button_draw, and so
 * on) draws the widget from what the update half found and changes nothing;
 * call it from _draw. The draw half changes nothing because _draw only shows
 * the state that _update left behind, and it may run more or fewer times than
 * _update does. Both halves take the same position and size, and both share a
 * JetGuiState that you own. The state holds two widget ids: the widget that
 * holds the mouse right now, and the widget under the pointer. Because each
 * screen has its own state, instead of every widget sharing one global, this
 * style suits the real interface of a game.
 *
 * Each frame, call jet_gui_stateful_begin_update once, then each widget's
 * update half, all from _update. Then call each widget's draw half from
 * _draw. Panels and labels only draw, and take no state. When you want one
 * combined call for a quick debugging overlay, use jet_gui.h instead.
 *
 * Complete split update/draw example (copy into main.c):
 *
 *   #include "jet64.h"
 *   #include "jet_gui_stateful.h"
 *
 *   static JetGuiState ui;
 *   static float volume = 1.0f;
 *
 *   void _init(void) { ui = jet_gui_stateful_create(); }
 *
 *   void _update(float dt) {
 *       (void)dt;
 *       jet_gui_stateful_begin_update(&ui);
 *       volume = jet_gui_stateful_slider_update(&ui, 20, 40, 120, 10,
 *                                                volume, 0.0f, 1.0f);
 *   }
 *
 *   void _draw(void) {
 *       jet_cls(0);
 *       jet_gui_stateful_panel_draw(8, 8, 152, 64, "Audio");
 *       jet_gui_stateful_slider_draw(&ui, 20, 40, 120, 10,
 *                                     volume, 0.0f, 1.0f);
 *   }
 *
 *   void _end(void) {}
 */

/**
 * Constant: JET_GUI_STATEFUL_NO_CONTROL
 * Summary: The id a JetGuiState holds when no widget has the mouse, or none is under the pointer.
 * Include: jet_gui_stateful.h
 * Notes: jet_gui_stateful_create and jet_gui_stateful_begin_update store it for you. You need it yourself only when you write a control of your own that joins the same active and hovered id scheme.
 * Example:
 *   if (ui.active_id == JET_GUI_STATEFUL_NO_CONTROL) begin_custom_drag();
 */
#define JET_GUI_STATEFUL_NO_CONTROL 0u
#define JET_GUI_STATEFUL_ID_SHIFT_X 16u
#define JET_GUI_STATEFUL_ID_SHIFT_W 8u
#define JET_GUI_STATEFUL_ID_SHIFT_H 24u
/**
 * Constant: JET_GUI_STATEFUL_BUTTON_SALT
 * Summary: The number mixed into a button's id, so that a button and a different kind of widget with the same position and size get different ids.
 * Include: jet_gui_stateful.h
 * Notes: Give it to jet_gui_stateful_make_id when drawing code of your own needs the same id that jet_gui_stateful_button_update uses. Each kind of widget has its own salt, which is what keeps two controls with the same bounds from colliding.
 * Example:
 *   uint32_t id = jet_gui_stateful_make_id(x, y, w, h, JET_GUI_STATEFUL_BUTTON_SALT);
 */
#define JET_GUI_STATEFUL_BUTTON_SALT 0x4242u

/**
 * Constant: JET_GUI_STATEFUL_CHECKBOX_SALT
 * Summary: The number mixed into a checkbox's id, so that a checkbox and a different kind of widget with the same position and size get different ids.
 * Include: jet_gui_stateful.h
 * Notes: Give it to jet_gui_stateful_make_id when drawing code of your own needs the same id that jet_gui_stateful_checkbox_update uses.
 * Example:
 *   uint32_t id = jet_gui_stateful_make_id(x, y, w, h, JET_GUI_STATEFUL_CHECKBOX_SALT);
 */
#define JET_GUI_STATEFUL_CHECKBOX_SALT 0x1010u

/**
 * Constant: JET_GUI_STATEFUL_SLIDER_SALT
 * Summary: The number mixed into a slider's id, so that a slider and a different kind of widget with the same position and size get different ids.
 * Include: jet_gui_stateful.h
 * Notes: Give it to jet_gui_stateful_make_id when drawing code of your own needs the same id that jet_gui_stateful_slider_update uses.
 * Example:
 *   uint32_t id = jet_gui_stateful_make_id(x, y, w, h, JET_GUI_STATEFUL_SLIDER_SALT);
 */
#define JET_GUI_STATEFUL_SLIDER_SALT 0x1337u
#define JET_GUI_STATEFUL_MIN_PERCENT 0.0f
#define JET_GUI_STATEFUL_MAX_PERCENT 1.0f
#define JET_GUI_STATEFUL_DEFAULT_TEXT_SIZE 8
#define JET_GUI_STATEFUL_TEXT_BASELINE_OFFSET 4
#define JET_GUI_STATEFUL_PANEL_HEADER_HEIGHT 16
#define JET_GUI_STATEFUL_PANEL_TITLE_Y_OFFSET 4

/**
 * Struct: JetGuiState
 * Summary: The state you own that the update and draw halves of a screen's widgets share: which widget holds the mouse, and which is under the pointer.
 * Fields:
 *   - active_id: The id of the control that holds the current press or drag, or JET_GUI_STATEFUL_NO_CONTROL when none does.
 *   - hovered_id: The id of the last control found under the pointer in this update pass, or JET_GUI_STATEFUL_NO_CONTROL when none was.
 * Include: jet_gui_stateful.h
 * Notes: Keep one state for each screen or panel that works the mouse on its own, and keep it alive from frame to frame. Make it once with jet_gui_stateful_create, call jet_gui_stateful_begin_update once at the start of each _update, then give each widget's update call and draw call the same position and size. Do not make a fresh state every frame, or a press is forgotten before its release arrives.
 * Example:
 *   static JetGuiState settings_ui;
 *   void _init(void) { settings_ui = jet_gui_stateful_create(); }
 */
typedef struct {
    uint32_t active_id;
    uint32_t hovered_id;
} JetGuiState;

/**
 * Function: jet_gui_stateful_create
 * Summary: Makes a fresh JetGuiState in which no widget holds the mouse and none is under the pointer.
 * Parameters:
 *   - None.
 * Returns: A JetGuiState with no active or hovered control.
 * Include: jet_gui_stateful.h
 * Notes: Keep the value for as long as its screen or panel lasts. Do not make a new one every frame unless you mean to forget every press and hover.
 * Example:
 *   static JetGuiState ui;
 *   ui = jet_gui_stateful_create();
 */
JetGuiState jet_gui_stateful_create(void);

/**
 * Function: jet_gui_stateful_begin_update
 * Summary: Starts a frame's update pass by forgetting which widget was under the pointer.
 * Parameters:
 *   - state: The state to prepare. NULL is ignored.
 * Returns: Nothing.
 * Include: jet_gui_stateful.h
 * Notes: Call it once near the start of _update, before any widget's update call. Only the hover is forgotten; a press or drag in progress is kept.
 * Example:
 *   jet_gui_stateful_begin_update(&ui);
 */
void jet_gui_stateful_begin_update(JetGuiState* state);

/**
 * Function: jet_gui_stateful_clampf
 * Summary: Holds a float between a low value and a high value, both included.
 * Parameters:
 *   - value: The value to hold in range.
 *   - min: The lowest value allowed.
 *   - max: The highest value allowed.
 * Returns: value if it is already in range, min if it was below, or max if it was above.
 * Include: jet_gui_stateful.h
 * Notes: A helper the sliders use. Give it a min no greater than max.
 * Example:
 *   percent = jet_gui_stateful_clampf(percent, 0.0f, 1.0f);
 */
float jet_gui_stateful_clampf(float value, float min, float max);

/**
 * Function: jet_gui_stateful_make_id
 * Summary: Builds a widget's id from its position, its size and a salt for its kind. The same inputs always give the same id.
 * Parameters:
 *   - x: The widget's left edge, in pixels.
 *   - y: The widget's top edge, in pixels.
 *   - w: The widget's width in pixels.
 *   - h: The widget's height in pixels.
 *   - salt: A number for the widget's kind, such as JET_GUI_STATEFUL_BUTTON_SALT, so that two kinds of control with the same bounds get different ids.
 * Returns: A 32-bit id, fit for the active_id and hovered_id fields of a JetGuiState.
 * Include: jet_gui_stateful.h
 * Notes: The id comes from the layout, so a widget that moves while it holds the mouse becomes a different widget, and is no longer the one that holds the press.
 * Example:
 *   uint32_t id = jet_gui_stateful_make_id(x, y, w, h, JET_GUI_STATEFUL_BUTTON_SALT);
 */
uint32_t jet_gui_stateful_make_id(int x, int y, int w, int h, uint32_t salt);

/**
 * Function: jet_gui_stateful_hover_rect
 * Summary: Tells you whether the mouse is inside a rectangle, with the camera taken into account.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 * Returns: true when the mouse is inside the rectangle; otherwise false.
 * Include: jet_gui_stateful.h
 * Notes: Every stateful control tests hovering with this, so hovering works exactly as it does in jet_gui.h.
 * Example:
 *   bool hovered = jet_gui_stateful_hover_rect(20, 20, 80, 16);
 */
bool jet_gui_stateful_hover_rect(int x, int y, int w, int h);

/**
 * Function: jet_gui_stateful_is_active
 * Summary: Tells you whether a widget id is the one holding the mouse right now.
 * Parameters:
 *   - state: The state to look in.
 *   - id: The widget id to test.
 * Returns: true when state is not NULL and id matches the active control; otherwise false.
 * Include: jet_gui_stateful.h
 * Notes: Handy in drawing code of your own that wants to colour a pressed control the way the built-in widgets do.
 * Example:
 *   if (jet_gui_stateful_is_active(&ui, id)) { draw_pressed_state(); }
 */
bool jet_gui_stateful_is_active(const JetGuiState* state, uint32_t id);

/**
 * Function: jet_gui_stateful_is_hovered
 * Summary: Tells you whether a widget id is the last one found under the pointer in this update pass.
 * Parameters:
 *   - state: The state to look in.
 *   - id: The widget id to test.
 * Returns: true when state is not NULL and id matches the hovered control; otherwise false.
 * Include: jet_gui_stateful.h
 * Notes: Ask it after the widgets' update calls have run for the frame.
 * Example:
 *   bool hot = jet_gui_stateful_is_hovered(&ui, id);
 */
bool jet_gui_stateful_is_hovered(const JetGuiState* state, uint32_t id);

/**
 * Function: jet_gui_stateful_control_color
 * Summary: Picks the fill colour a control should be drawn in, depending on whether it is pressed, hovered or idle.
 * Parameters:
 *   - state: The state holding the active and hovered ids.
 *   - id: The widget id to look up.
 * Returns: From the current GUI style, active_color if the control holds the mouse, hover_color if it is under the pointer, or base_color otherwise.
 * Include: jet_gui_stateful.h
 * Notes: Use it in a control of your own so it looks the same as the built-in stateful widgets.
 * Example:
 *   uint8_t bg = jet_gui_stateful_control_color(&ui, id);
 */
uint8_t jet_gui_stateful_control_color(const JetGuiState* state, uint32_t id);

/**
 * Function: jet_gui_stateful_panel_draw
 * Summary: Draws a box with a border, and a title bar across the top if you give it a title, to go with the stateful widgets.
 * Parameters:
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - title: The title, as a null-terminated string, drawn in the bar across the top. NULL draws no bar.
 * Returns: Nothing.
 * Include: jet_gui_stateful.h
 * Notes: It only draws, so call it from _draw. It takes no JetGuiState because a panel never takes the mouse.
 * Example:
 *   jet_gui_stateful_panel_draw(4, 4, 180, 100, "Settings");
 */
void jet_gui_stateful_panel_draw(int x, int y, int w, int h, const char* title);

/**
 * Function: jet_gui_stateful_label_draw
 * Summary: Draws a line of text in the style's text colour, to go with the stateful widgets.
 * Parameters:
 *   - x: Where the text starts, across, in world pixels.
 *   - y: Where the text starts, down, in world pixels.
 *   - w: Reserved for layout; not used yet.
 *   - h: Reserved for layout; not used yet.
 *   - text: The text to draw, as a null-terminated string. NULL draws nothing.
 * Returns: Nothing.
 * Include: jet_gui_stateful.h
 * Notes: It only draws, so call it from _draw.
 * Example:
 *   jet_gui_stateful_label_draw(8, 24, 120, 8, "Audio");
 */
void jet_gui_stateful_label_draw(int x, int y, int w, int h, const char* text);

/**
 * Function: jet_gui_stateful_button_update
 * Summary: The update half of a button. It reads the mouse and tells you whether the button was clicked, a press on the button followed by a release on it.
 * Parameters:
 *   - state: The state you keep for this screen. NULL makes the call return false.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 * Returns: true only on the update in which the left mouse button is released over this button while the button holds the press; otherwise false.
 * Include: jet_gui_stateful.h
 * Notes: Call it from _update, then call jet_gui_stateful_button_draw from _draw with the same position and size, because the id that joins the two halves is built from those numbers.
 * Example:
 *   if (jet_gui_stateful_button_update(&ui, 8, 8, 80, 18)) { save_game(); }
 */
bool jet_gui_stateful_button_update(JetGuiState* state, int x, int y, int w, int h);

/**
 * Function: jet_gui_stateful_button_draw
 * Summary: The draw half of a button. It draws the button in the colour the update half decided on.
 * Parameters:
 *   - state: The state the update half filled in, read to pick the pressed or hovered colour.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - text: The label, centred in the button, as a null-terminated string. NULL draws no label.
 * Returns: Nothing.
 * Include: jet_gui_stateful.h
 * Notes: Call it from _draw with the same position and size you gave jet_gui_stateful_button_update. It changes nothing in the state.
 * Example:
 *   jet_gui_stateful_button_draw(&ui, 8, 8, 80, 18, "Save");
 */
void jet_gui_stateful_button_draw(const JetGuiState* state, int x, int y, int w, int h, const char* text);

/**
 * Function: jet_gui_stateful_checkbox_update
 * Summary: The update half of a checkbox. It reads the mouse and flips a bool of yours each time the box is clicked.
 * Parameters:
 *   - state: The state you keep for this screen. NULL makes the call return false.
 *   - x: The left edge of the box, in world pixels.
 *   - y: The top edge of the box, in world pixels.
 *   - w: The width of the box in pixels.
 *   - h: The height of the box in pixels.
 *   - checked: A pointer to the bool to flip on a click, or NULL to only report clicks.
 * Returns: true on the update in which the checkbox is clicked; otherwise false.
 * Include: jet_gui_stateful.h
 * Notes: Call it from _update, then call jet_gui_stateful_checkbox_draw from _draw with the same position and size. With NULL for checked the widget still reports clicks but changes nothing of yours.
 * Example:
 *   jet_gui_stateful_checkbox_update(&ui, 8, 32, 12, 12, &fullscreen);
 */
bool jet_gui_stateful_checkbox_update(JetGuiState* state, int x, int y, int w, int h, bool* checked);

/**
 * Function: jet_gui_stateful_checkbox_draw
 * Summary: The draw half of a checkbox. It draws the box, its tick and its label, in the colour the update half decided on.
 * Parameters:
 *   - state: The state the update half filled in, read to pick the pressed or hovered colour.
 *   - x: The left edge of the box, in world pixels.
 *   - y: The top edge of the box, in world pixels.
 *   - w: The width of the box in pixels.
 *   - h: The height of the box in pixels.
 *   - text: The label, drawn to the right of the box, as a null-terminated string. NULL draws no label.
 *   - checked: true to draw the box ticked, false to draw it empty.
 * Returns: Nothing.
 * Include: jet_gui_stateful.h
 * Notes: Call it from _draw with the same position and size you gave jet_gui_stateful_checkbox_update. It changes nothing in the state.
 * Example:
 *   jet_gui_stateful_checkbox_draw(&ui, 8, 32, 12, 12, "Fullscreen", fullscreen);
 */
void jet_gui_stateful_checkbox_draw(const JetGuiState* state, int x, int y, int w, int h, const char* text, bool checked);

/**
 * Function: jet_gui_stateful_slider_update
 * Summary: The update half of a horizontal slider. It reads the mouse and gives you back the slider's value, which changes while you drag it.
 * Parameters:
 *   - state: The state you keep for this screen. NULL makes the call return value unchanged.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - value: The slider's current value.
 *   - min: The smallest value the slider can give.
 *   - max: The largest value the slider can give.
 * Returns: The value you passed in, or, while the slider is being dragged, the new value.
 * Include: jet_gui_stateful.h
 * Notes: Call it from _update, and store what it returns, as the example does. Once you press on the slider it keeps the drag for as long as the left mouse button is held down. If max equals min, or the width is zero or less, the slider has no place to put its fill, so the value comes back unchanged.
 * Example:
 *   music_volume = jet_gui_stateful_slider_update(&ui, 8, 56, 100, 10, music_volume, 0.0f, 1.0f);
 */
float jet_gui_stateful_slider_update(JetGuiState* state, int x, int y, int w, int h, float value, float min, float max);

/**
 * Function: jet_gui_stateful_slider_draw
 * Summary: The draw half of a horizontal slider. It draws the track and the filled part, in the colour the update half decided on.
 * Parameters:
 *   - state: The state the update half filled in, read to pick the pressed or hovered colour.
 *   - x: The left edge, in world pixels.
 *   - y: The top edge, in world pixels.
 *   - w: The width in pixels.
 *   - h: The height in pixels.
 *   - value: The slider's current value, which sets how much of the track is filled.
 *   - min: The smallest value the slider can give.
 *   - max: The largest value the slider can give.
 * Returns: Nothing.
 * Include: jet_gui_stateful.h
 * Notes: Call it from _draw with the same position, size, min and max you gave jet_gui_stateful_slider_update. It changes nothing in the state.
 * Example:
 *   jet_gui_stateful_slider_draw(&ui, 8, 56, 100, 10, music_volume, 0.0f, 1.0f);
 */
void jet_gui_stateful_slider_draw(const JetGuiState* state, int x, int y, int w, int h, float value, float min, float max);

#endif // JET_GUI_STATEFUL_H
