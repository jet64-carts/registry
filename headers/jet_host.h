// runtime/jet_host.h
#ifndef JET_HOST_H
#define JET_HOST_H

#include <stdint.h>
#include <stdbool.h>

// Ask about, and change, the program that is running your cartridge: the
// workstation, the native player, or the web player. That program is the
// host. The jet_host_is_ calls tell you which host you are on, so your cart
// can suit itself to each one. The rest calls are cheap: each writes one shared
// register and returns at once, so you may call them every frame. (The master
// volumes and the equaliser are jet_audio.h's, with the rest of the sound.) The window, screenshot, keep-awake, file-import, reveal and exit
// calls send a command across to the host and wait for its answer, so call
// those only when something changes or the player asks, never every frame.
// You never have to prod the host along yourself; the runtime sees every
// command through.
//
// Complete fullscreen-toggle example (copy into main.c):
//
//   #include "jet64.h"
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_START)) {
//           jet_host_set_fullscreen(!jet_host_is_fullscreen());
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print(jet_host_is_web() ? "WEB PLAYER" : "NATIVE", 16, 16, 15);
//       jet_print("PRESS START FOR FULLSCREEN", 16, 32, 15);
//   }
//
//   void _end(void) {}

#if defined(JET_STATIC_LINK) || defined(JET_GUEST_PROCESS)
/**
 * Function: jet_host_ipc_pump
 * Summary: Internal runtime hook that keeps host commands progressing while Jet64 waits for acknowledgement.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Internal: Called by Jet64 itself; omitted from the creator-facing Codex.
 * Notes: Cartridge code must never call this directly. The runtime invokes it inside synchronous jet_host_* operations; exported players service OS events and workstation guests yield safely.
 * Example:
 *   // Cartridge code calls the requested operation; Jet64 pumps internally.
 *   jet_host_set_fullscreen(true);
 */
extern void jet_host_ipc_pump(void);
#else
/**
 * Function: jet_host_ipc_pump
 * Summary: Internal no-op host-command pump supplied to headless cartridge harnesses.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Internal: Called by test/runtime infrastructure; omitted from the creator-facing Codex.
 * Notes: Cartridge code must never call this directly. Normal Jet64 targets select the active runtime implementation; harnesses whose host stubs acknowledge synchronously need no work here.
 * Example:
 *   // No cartridge call is required.
 */
static inline void jet_host_ipc_pump(void) {}
#endif

// The old name for jet_host_ipc_pump, kept so older carts still build.
#define host_ipc_pump jet_host_ipc_pump

/**
 * Function: jet_host_is_workstation
 * Summary: Tells you whether the cartridge is running inside the workstation, rather than in an exported player.
 * Parameters:
 *   - None.
 * Returns: true inside the workstation; false in an exported player.
 * Include: jet_host.h
 * Example:
 *   if (jet_host_is_workstation()) { draw_debug_ui(); }
 */
bool jet_host_is_workstation(void);

/**
 * Function: jet_host_is_warm_boot
 * Summary: Tells you whether this run began with a Warm Boot, where the cartridge was rebuilt and restarted but WRAM, its working memory, was kept exactly as it was.
 * Parameters:
 *   - None.
 * Returns: true after a Warm Boot; false after a normal start.
 * Include: jet_host.h
 * Example:
 *   if (jet_host_is_warm_boot()) { skip_intro_sequence(); }
 */
bool jet_host_is_warm_boot(void);

/**
 * Function: jet_host_set_fullscreen
 * Summary: Switches between fullscreen and a window.
 * Parameters:
 *   - enable: true for fullscreen, false for a window.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: This sends a command to the host and waits for its answer, so call it when something changes, such as when the player picks it from a menu, not every frame.
 * Example:
 *   jet_host_set_fullscreen(true);
 */
void jet_host_set_fullscreen(bool enable);

/**
 * Function: jet_host_set_resolution
 * Summary: Shows your picture at an exact whole-number size while your cart runs, or hands the choice back.
 * Parameters:
 *   - multiple: How many screen points each of your cart's pixels becomes: 1 for 1 times, 2 for 2 times, and so on. 0 hands back to the player's own choice, or to yours from Engine Settings if they have made none.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: It asks for the Fixed scale mode (Engine Settings, SCALE MODE) at
 *        that multiple, and means the same on every host: the workstation
 *        shows your picture at that size in its display, the web player in its
 *        page, and the native player also resizes its window to fit it,
 *        keeping the window's centre where the player put it. If the screen
 *        cannot hold that many, the biggest multiple that fits is shown. The
 *        request lasts only while your cart runs and is never saved, so it
 *        never replaces the mode you chose in Engine Settings, or the one a
 *        player chose with Ctrl or Cmd+Shift+S. It changes only how big your
 *        picture is shown; the number of pixels your cart draws stays the
 *        same. To change that, use jet_display_set_resolution. This sends a
 *        command to the host and waits for its answer, so call it when
 *        something changes, not every frame.
 * Example:
 *   jet_host_set_resolution(2); // every pixel a 2 by 2 block
 *   jet_host_set_resolution(0); // hand back to the player's choice
 */
void jet_host_set_resolution(int multiple);

/**
 * Function: jet_host_step_scale
 * Summary: Steps the player's picture size on by one, exactly as their Ctrl or Cmd+Shift+S does.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: It is the player's own chord, pressed through your cart, for the
 *        player who would never guess it: give it a key, a button or a line
 *        in your menu. Each step moves along the same ring the chord walks:
 *        Fixed 1 times, 2 times and up to the biggest that fits, then Pixel
 *        Perfect, Aspect Fit and Stretch to Fill, and round again. The host
 *        says where it landed in its own words and remembers it as the
 *        player's choice, where it already keeps the chord's: the native
 *        player beside the cart's saves, the web player in the browser, and
 *        the workstation in Engine Settings, so it is saved with the
 *        cartridge. Nothing is left for your cart to store. A step also
 *        clears any size your cart asked for with jet_host_set_resolution.
 *        Because it is the player's choice, call it only when the player
 *        asks for it, never on your own. This sends a command to the host
 *        and waits for its answer, so call it once for each press, not every
 *        frame.
 * Example:
 *   if (jet_bind_pressed("SCREEN SIZE")) { jet_host_step_scale(); }
 */
void jet_host_step_scale(void);

/**
 * Function: jet_host_exit
 * Summary: Asks the host to close the program.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: This sends a command to the host and waits for its answer.
 * Example:
 *   if (jet_bind_pressed("QUIT")) { jet_host_exit(); }
 */
void jet_host_exit(void);

/**
 * Function: jet_host_set_vsync
 * Summary: Turns vertical sync on or off. With it on, each frame is shown in step with the monitor's refresh, so the picture never tears (shows the top of one frame with the bottom of another).
 * Parameters:
 *   - enable: true to show frames in step with the monitor's refresh; false to let frames run as fast as they can.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: This sends a command to the host and waits for its answer, so call it when something changes, not every frame.
 * Example:
 *   jet_host_set_vsync(true);
 */
void jet_host_set_vsync(bool enable);

/**
 * Function: jet_host_screenshot
 * Summary: Asks the host to save the frame on screen as a PNG file.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: On the workstation and in an exported player alike, the file goes
 *        into the captures folder inside the cartridge's own AppData folder,
 *        next to the clips the workstation's GIF and MP4 buttons record, and
 *        it holds the screen at the screen's own size — never the window's,
 *        so no scaling and no letterbox margin ever reach the picture. In a
 *        web player, where there is no such folder, the browser downloads the
 *        same picture instead.
 *
 *        What is in it is what that host actually shows. The workstation saves
 *        what the cartridge drew: the screen's pixels, coloured through the
 *        palette as it stood at that moment, with nothing laid over them. An
 *        exported player saves the frame as its graphics card produced it, so
 *        a cartridge published with Neon effects gets a picture with those
 *        effects in it — which is the picture its player was looking at.
 *
 *        The frame it saves is the one on screen when you call it. The tick
 *        that is running has not been shown yet, so a call inside _draw saves
 *        what the previous _draw drew. Several calls within one frame save
 *        only one picture.
 *
 *        The call sends a command to the host and waits for its answer. That
 *        answer means the host accepted the request, not that the file is on
 *        disk; the host writes the PNG in the background.
 * Example:
 *   if (jet_key_pressed(JET_KEY_P)) { jet_host_screenshot(); }
 */
void jet_host_screenshot(void);

/**
 * Function: jet_host_set_keep_awake
 * Summary: Asks the host to keep the screen awake while the player is idle, or lets it sleep again.
 * Parameters:
 *   - enable: true to stop the screen going to sleep while the player is idle;
 *             false to go back to the host's normal power rules.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: Call it when something changes, for a mode the player only watches: a
 *        visualiser, a film, a kiosk or a presentation. Calling it twice with
 *        the same value is safe: a second true takes nothing more from the
 *        operating system, and a false when nothing was asked for does
 *        nothing. The host also lets the request go by itself when the cart
 *        ends, crashes, hot-reloads, switches project, or the program closes.
 *
 *        It is a request, not an order. It asks the operating system not to
 *        sleep the screen for idleness; it cannot overrule a closed laptop
 *        lid, a battery rule, an administrator's setting, or anything the
 *        player does on purpose. In a browser it works only on a secure page
 *        that is visible; when the page is hidden and then shown again, the
 *        web player takes the browser's wake lock back for you. The host's
 *        answer means it accepted the request, not that the operating system
 *        had to grant it.
 *
 *        Keeping a screen on uses power, so use this only where a screen that
 *        stays on is part of what the cart is.
 * Example:
 *   jet_host_set_keep_awake(viewing_fullscreen);
 */
void jet_host_set_keep_awake(bool enable);

/**
 * Function: jet_host_keep_awake
 * Summary: Keeps the screen from sleeping while the player is idle. The same as jet_host_set_keep_awake(true).
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: Safe to call again. Call jet_host_set_keep_awake(false) when the
 *        watch-only mode ends. If you forget, the host lets the request go
 *        when the cart shuts down.
 * Example:
 *   jet_host_keep_awake();
 */
void jet_host_keep_awake(void);

/**
 * Function: jet_host_reveal_dir
 * Summary: Opens one folder inside the cartridge's own AppData folder in the desktop's file manager, so the player can see where to put their own files.
 * Parameters:
 *   - subdir: The folder to open, named relative to the cartridge's AppData
 *             folder, for example "music" or "saves". It must be a safe
 *             relative path: not empty, no "..", not absolute, and no drive
 *             colon. The host checks it and joins it onto the AppData folder,
 *             whose path the host works out for itself, so the cart can never
 *             reach outside its own folder. An unsafe name, or NULL, is
 *             ignored.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: This is for the workstation and the native player only, because
 *        finding an AppData folder by hand is awkward for a person. The host
 *        makes the folder if it is missing, then opens it in Finder,
 *        Explorer, or whatever xdg-open chooses. In a browser it quietly does
 *        nothing at all: there is no AppData folder and no file manager
 *        there, so show the button only when !jet_host_is_web(). To pull the
 *        player's files into the cartridge's folder on every platform,
 *        browser included, use jet_host_import_files instead.
 *
 *        The call sends a command to the host and waits for its answer, so
 *        call it on a button press, not every frame.
 * Example:
 *   if (!jet_host_is_web() && ui_button("OPEN FOLDER")) {
 *       jet_host_reveal_dir("music");
 *   }
 */
void jet_host_reveal_dir(const char* subdir);

/**
 * Function: jet_host_import_files
 * Summary: Lets the player bring their own files into a folder inside the cartridge's own AppData folder.
 * Parameters:
 *   - subdir: The folder the files go into, named relative to the cartridge's
 *             AppData folder, for example "disks" or "music". It must be a
 *             safe relative path: not empty, no "..", not absolute, and no
 *             drive colon. The host checks it and joins it onto the AppData
 *             folder, whose path the host works out for itself, so the cart
 *             can never reach outside its own folder. An unsafe name, or
 *             NULL, is ignored.
 *   - extensions: Which file types to accept, as a comma-separated list of
 *                 extensions without the dots, for example "d64" or
 *                 "rom,bin". NULL or "" accepts any file. Upper and lower case
 *                 both match, for each file picked or dropped.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: It works on every platform (see docs/file_import.md). In the
 *        workstation and the native player the host opens the system's file
 *        picker, which allows several files at once, and copies the chosen
 *        files into the folder. In the web player it shows a panel the player
 *        can drop files onto or browse from, and keeps the accepted files
 *        inside the browser: they are still there on the next visit, and they
 *        are never uploaded anywhere.
 *
 *        The host's answer only means the picker or panel was shown. The files
 *        arrive later, and a web panel stays open until the player closes it.
 *        So watch jet_host_import_generation, and when it changes, list the
 *        folder again with jet_ls.
 *
 *        The call sends a command to the host and waits for its answer, so
 *        call it on a button press, not every frame.
 * Example:
 *   if (ui_button("ADD DISKS...")) {
 *       jet_host_import_files("disks", "d64");
 *   }
 */
void jet_host_import_files(const char* subdir, const char* extensions);

/**
 * Function: jet_host_import_generation
 * Summary: Reads the host's import counter, which goes up by one for every file that arrives in the cartridge's AppData folder through an import.
 * Parameters:
 *   - None.
 * Returns: A count that starts at 0 when the cart boots and only ever grows.
 *          Each file that arrives adds exactly one.
 * Include: jet_host.h
 * Notes: It is a plain register read, so it is cheap enough to check every
 *        frame. Keep the last value you saw, and when the count differs, list
 *        the folder again with jet_ls. This is how you know an import has
 *        finished, both for jet_host_import_files and for files the player
 *        drags and drops onto the host.
 * Example:
 *   static uint32_t seen = 0;
 *   if (jet_host_import_generation() != seen) {
 *       seen = jet_host_import_generation();
 *       rescan_disk_folder();
 *   }
 */
uint32_t jet_host_import_generation(void);

/**
 * Constant: JET_REST_UNTIL_INPUT
 * Summary: The rest to ask for when nothing at all is due: rest until input arrives.
 * Include: jet_host.h
 * Notes: Pass it to jet_host_rest. Like every rest it lasts at most one
 *        second, so the host still wakes your cart once a second when nothing
 *        else does; the cart then simply asks again.
 * Example:
 *   if (nothing_due) jet_host_rest(JET_REST_UNTIL_INPUT);
 */
#define JET_REST_UNTIL_INPUT UINT32_MAX

/**
 * Function: jet_host_rest
 * Summary: Promises the host that, until input arrives, your next ticks would change nothing but the time, so it may stop the machine and save the energy a still screen costs.
 * Parameters:
 *   - ticks: How many of the ticks after this one would change nothing if no
 *            input arrived: nothing drawn, no sound you are changing, no state
 *            but your clock. JET_REST_UNTIL_INPUT when nothing at all is due;
 *            0 asks for no rest.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: The host may then run no tick and show no new frame until input
 *        arrives or those ticks have passed. The screen keeps your last
 *        picture and the program sleeps, which saves almost all of what a
 *        still screen costs: showing an unchanged picture at the display's
 *        rate costs far more than your ticks do. When the host wakes it tells
 *        you how many ticks it skipped (jet_host_rested_ticks) and runs the
 *        next tick as normal, with any input that woke it.
 *
 *        A rest holds only for the tick you ask on. Call it from _update on
 *        every tick you still have nothing to do; if you call it more than
 *        once in a tick, the last call counts. It writes one register and
 *        returns at once.
 *
 *        A key, the mouse, the wheel or a touch ends a rest at once on every
 *        host. A controller ends it at once in a desktop player; a browser
 *        raises no event for a controller, so on the web a press is heard when
 *        the rest ends. No rest lasts longer than one second, whatever you
 *        ask, so whatever cannot wake a host, such as a file dropped on a
 *        desktop player's window, still shows within a second. The host does
 *        not rest while a key, a mouse button or a pad button is held down, or
 *        while it still owes your cart ticks.
 *
 *        If your _draw ever returns without drawing because nothing changed,
 *        do not rest until every screen buffer has been given the frame
 *        (JET_VRAM_BUFFER_COUNT + 1 draws of it in a row are always enough): a
 *        host may show any of them on the tick it wakes to run, and a draw you
 *        skip then leaves an older picture on the screen.
 *
 *        It is a promise, not an order. Any host may ignore it and run the
 *        ticks anyway, and then jet_host_rested_ticks reads 0. If the promise
 *        is true, your cart looks the same either way: the same frames, the
 *        same tick count, the same tick for every input. The agent door can
 *        run a scenario both ways and compare the frames, which is how you
 *        prove a cart keeps its promise.
 *
 *        Music and sounds you have already started keep playing through a
 *        rest, because the host mixes them. What stops is anything your cart
 *        changes tick by tick, such as a fade you step every tick or a
 *        sequencer of your own; resting through one of those breaks the
 *        promise. So a game should rest only on a screen that is truly still,
 *        such as a pause menu or a title waiting for a key, and never past the
 *        tick on which anything next moves, blinks or counts down.
 * Example:
 *   #define NOTHING_DUE UINT32_MAX
 *
 *   static uint32_t tick;              // the number of the tick now running
 *   static uint32_t due = NOTHING_DUE; // the next tick anything changes on
 *
 *   void _update(float dt) {
 *       (void)dt;
 *       tick += jet_host_rested_ticks();
 *       handle_keys_and_timers();      // the work, which may move `due`
 *
 *       // Every tick before `due` would change nothing. Written so no
 *       // unsigned subtraction can wrap: when `due` is the very next tick,
 *       // or already here, there is nothing to rest through.
 *       if (due == NOTHING_DUE) {
 *           jet_host_rest(JET_REST_UNTIL_INPUT);
 *       } else if (due > tick && due - tick > 1) {
 *           jet_host_rest(due - tick - 1);
 *       }
 *       tick++;
 *   }
 */
void jet_host_rest(uint32_t ticks);

/**
 * Function: jet_host_rested_ticks
 * Summary: Tells you how many ticks the host skipped, resting, just before this one.
 * Parameters:
 *   - None.
 * Returns: The ticks the host skipped while your cart rested; 0 when it skipped
 *          none, or ignored the rest.
 * Include: jet_host.h
 * Notes: Add it to any clock you keep in ticks, at the start of _update,
 *        before you count the tick that is running, and the clock keeps time
 *        across a rest exactly as if every tick had run (jet_host_rest's
 *        example shows the whole clock).
 *        It reads the same all through the tick the host wakes to run, _draw
 *        included, and 0 again from the next. It is a plain register read, so
 *        it is cheap enough to call every tick.
 * Example:
 *   tick += jet_host_rested_ticks();
 */
uint32_t jet_host_rested_ticks(void);

/**
 * Function: jet_host_disable_neon
 * Summary: Turns off Neon, the path that builds each frame on the graphics card (the GPU), so the CPU builds the frames instead.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_host.h
 * Notes: Call it in _init when you want the exact software picture rather than
 *        the graphics card's. It sends a command to the host, waits for its
 *        answer, and prints a line to the log (stdout). If the CPU path is
 *        already on, it returns at once without asking the host.
 * Example:
 *   void _init(void) {
 *       jet_host_disable_neon();
 *   }
 * NB: CTRL + SHIFT + V already toggles between neon and cpu fallback in all carts.
 */
void jet_host_disable_neon(void);

/**
 * Function: jet_host_get_platform
 * Summary: Tells you which kind of machine the host is running on.
 * Parameters:
 *   - None.
 * Returns: One of the JET_PLATFORM_* constants from jet_core.h:
 *   - JET_PLATFORM_UNKNOWN (0): the platform could not be worked out when the cart was built.
 *   - JET_PLATFORM_WINDOWS (1): a native build for Windows.
 *   - JET_PLATFORM_MACOS   (2): a native build for macOS.
 *   - JET_PLATFORM_LINUX   (3): a native build for Linux.
 *   - JET_PLATFORM_WEB     (4): a wasm32 build running in a web browser. Some
 *                                keys a desktop keyboard has may be missing
 *                                here (an iPad keyboard has no ESC, for
 *                                example), so offer another control on this
 *                                platform.
 * Include: jet_host.h
 * Notes: The answer is fixed when the cart is built, so it never changes
 *        during a run and costs nothing to ask. Use it to fall back gracefully
 *        or to offer controls that suit one platform. For the everyday
 *        question "am I in a browser?", jet_host_is_web reads better.
 * Example:
 *   if (jet_host_get_platform() == JET_PLATFORM_WINDOWS) { show_windows_controls(); }
 */
uint32_t jet_host_get_platform(void);

/**
 * Function: jet_host_is_web
 * Summary: Tells you whether the cartridge is running in a web browser, as a wasm32 build.
 * Parameters:
 *   - None.
 * Returns: true in a browser (JET_PLATFORM_WEB); false in any native build.
 * Include: jet_host.h
 * Notes: A readable way of writing jet_host_get_platform() == JET_PLATFORM_WEB.
 *        A browser lacks some things a desktop has: an iPad keyboard has no
 *        ESC key, and there is no window to close to quit. So use this to
 *        offer a control that works by touch or on any keyboard, for example
 *        an on-screen or remapped "back to menu" button, rather than relying
 *        on ESC alone.
 * Example:
 *   if (jet_host_is_web()) { show_web_menu_button(); }
 */
bool jet_host_is_web(void);

/**
 * Function: jet_host_is_fullscreen
 * Summary: Tells you whether the cartridge is showing fullscreen right now.
 * Parameters:
 *   - None.
 * Returns: true if the host is fullscreen; false if it is in a window.
 * Include: jet_host.h
 * Notes: The answer comes from the host, which updates it whenever fullscreen is switched on or off.
 * Example:
 *   if (jet_host_is_fullscreen()) { render_fullscreen_ui(); }
 */
bool jet_host_is_fullscreen(void);

#endif // JET_HOST_H
