// runtime/jet_scope.h
#ifndef JET_SCOPE_H
#define JET_SCOPE_H

// ============================================================================
// Jet64 audio scope: listen to the machine's own sound (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_pcm.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_scope.c only when
// a cart's #include tree reaches this header.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_scope.h"   // <-- explicit opt-in
//
// Listen to the sound the machine is sending to the speakers. The scope is a
// tap on the wire to the speakers: it hears everything at once, the music and
// the sound effects mixed together, exactly as the player hears it. With it
// you can copy out the newest stretch of the waveform to draw, measure how
// loud the sound is, hold a waveform still on the screen the way an
// oscilloscope does, and pick out the beat of the music, so your pictures
// can move to the sound. This header is not part of jet64.h; include it
// yourself, as shown above.
//
// <nocodex>
// jet_scope reads the AUDIO SCOPE hardware block (JET_SCOPE_OFFSET): a
// host-written ring of the post-mix master bus — everything the speakers get,
// tapped at the Mixing Station analyser's point. On top of the raw ring it
// offers the classic music-visualization toolkit: rate-invariant envelope
// smoothing, windowed RMS/peak statistics, a phase-stabilising oscilloscope
// trigger, and a beat-detection engine derived from the Geiss screensaver
// (© 1998-2022 Ryan Geiss, BSD-3-Clause; see carts/jetamp — algorithms
// transcribed, code written fresh).
// </nocodex>
//
// The scope is a real-time tap, not a deterministic input: what it hears is
// never quite the same twice, so treat what you read from it the way you
// treat a joystick or the mouse, as something that changes from one run to
// the next, never as a fixed part of the game. The sound comes as frames. A
// frame is one left sample and one right sample side by side, each a float,
// at the sound device's own rate, which jet_scope_sample_rate tells you.
// <nocodex>
// The host advances
// a monotonic write head every audio buffer, in all three runtimes —
// workstation, native player, and web player alike.
// </nocodex>
//
// jet_fft.h goes with this header: read the sound here, then use jet_fft.h
// to find out how much of each pitch is in it.
//
// Complete oscilloscope example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_scope.h"
//
//   enum { SAMPLE_COUNT = 256, WAVE_HEIGHT = 80 };
//   static float waveform[SAMPLE_COUNT];
//   static uint32_t samples_read;
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       (void)dt;
//       samples_read = jet_scope_read_mono(waveform, SAMPLE_COUNT);
//   }
//
//   void _draw(void) {
//       const int centre_y = JET_HEIGHT / 2;
//       jet_cls(0);
//       for (uint32_t i = 1; i < samples_read; i++) {
//           const int x0 = (int)((i - 1) * (uint32_t)JET_WIDTH / SAMPLE_COUNT);
//           const int x1 = (int)(i * (uint32_t)JET_WIDTH / SAMPLE_COUNT);
//           const int y0 = centre_y - (int)(waveform[i - 1] * WAVE_HEIGHT);
//           const int y1 = centre_y - (int)(waveform[i] * WAVE_HEIGHT);
//           jet_line(x0, y0, x1, y1, 12);
//       }
//   }
//
//   void _end(void) {}

#include "jet_core.h" // IWYU pragma: keep (JET_BASE, JetScopeHeader, JET_SCOPE_* constants)

// A host-side test harness has no machine behind JET_BASE, so it stands each block in with a
// pointer object of its own (at a stub, or NULL for a host that plays nothing) and builds with
// JET_AUDIO_BLOCKS_STUBBED defined; the blocks are then those objects, declared here as the
// runtime declared them before the blocks left firmware. A cartridge never defines it.
#ifdef JET_AUDIO_BLOCKS_STUBBED
// <nocodex>
extern volatile uint8_t* SCOPE;
// </nocodex>
#else

/**
 * Constant: SCOPE
 * Summary: The scope block: its JetScopeHeader, then the ring of the master bus's last samples.
 * Include: jet_scope.h
 * Notes: The host writes it; the jet_scope_* calls read it for you.
 * Example:
 *   bool live = jet_scope_available(); // reads SCOPE for you
 */
#define SCOPE ((volatile uint8_t*)(JET_BASE + JET_SCOPE_OFFSET))
#endif // JET_AUDIO_BLOCKS_STUBBED

/**
 * Constant: JET_SCOPE_BEAT_TAPE_N
 * Summary: How many entries JetScopeBeat keeps in its four-second history of the volume.
 * Include: jet_scope.h
 * Notes: The beat detector writes one entry 30 times a second, whatever your tick rate is, so its 120 entries always cover four seconds. Use this constant when you look through, or draw, the past array.
 * Example:
 *   for (uint32_t i = 0; i < JET_SCOPE_BEAT_TAPE_N; i++) draw_history(beat.past[i]);
 */
#define JET_SCOPE_BEAT_TAPE_N 120

/**
 * Constant: JET_SCOPE_BEAT_REF_HZ
 * Summary: The rate, 30 times a second, at which the beat detector's tuning numbers were first worked out.
 * Include: jet_scope.h
 * Notes: jet_scope_beat_tick and jet_scope_env_rate rescale everything from this 30 Hz starting point for you. Tell them your real tick rate; do not slow your calls to the beat detector down to 30 a second to match.
 * Example:
 *   float release = jet_scope_env_rate(0.85f, JET_SCOPE_BEAT_REF_HZ, JET_GUEST_TICK_HZ);
 */
#define JET_SCOPE_BEAT_REF_HZ 30.0f

/**
 * Constant: JET_SCOPE_BEAT_SILENCE_FLOOR
 * Summary: The default volume below which the beat detector treats the sound as silence and reports no beat.
 * Include: jet_scope.h
 * Notes: jet_scope_beat_init puts this value in JetScopeBeat.silence_floor. Raise it to ignore more background noise; lower it when you mean to analyse a very quiet mix. The number comes from the original Geiss code, which ignored an average volume below 10 in its own units. That unit was the 16-bit peak-to-peak value divided by 256, which is 256 times the normalised half-peak-to-peak volume this detector is fed, so 10 of those units is 10/256 here.
 * Example:
 *   beat.silence_floor = JET_SCOPE_BEAT_SILENCE_FLOOR;
 */
#define JET_SCOPE_BEAT_SILENCE_FLOOR (10.0f / 256.0f)

/**
 * Constant: JET_SCOPE_BIG_BEAT_THRESHOLD_DEFAULT
 * Summary: The default for how much louder than the recent past a hit must be to count as a big beat.
 * Include: jet_scope.h
 * Notes: jet_scope_beat_init puts this value in JetScopeBeat.big_beat_threshold. A higher value demands a rarer, harder hit. Code that runs a scene may lower it bit by bit during a quiet passage, so that softer hits still set things off.
 * Example:
 *   beat.big_beat_threshold = JET_SCOPE_BIG_BEAT_THRESHOLD_DEFAULT;
 */
#define JET_SCOPE_BIG_BEAT_THRESHOLD_DEFAULT 1.10f

// --- Availability & raw ring access ---

/**
 * Function: jet_scope_available
 * Summary: Tells you whether the host is feeding the scope at all.
 * Parameters:
 *   - None.
 * Returns: true once the scope's memory block carries the host's marker value (its magic number), its version, and its tap-active flag; false on a host too old to write the tap.
 * Include: jet_scope.h
 * Notes: It tells "this host has no tap" apart from "the tap is there but the sound is silent". The host writes the header before your cartridge's first tick, so the answer is reliable from _init onward.
 * Example:
 *   if (!jet_scope_available()) { draw_static_backdrop(); return; }
 */
bool jet_scope_available(void);

/**
 * Function: jet_scope_sample_rate
 * Summary: How many frames of sound a second the scope carries: the sound device's own sample rate, in Hz.
 * Parameters:
 *   - None.
 * Returns: The rate the host's mixer runs at, for example 44100 or 48000, or 0 while the scope is unavailable.
 * Include: jet_scope.h
 * Notes: It depends on the sound device, so never assume 48000. Every jet_scope measurement takes the rate into account. Pass this to jet_fft_bands_log so that its bins land on the right pitches.
 * Example:
 *   uint32_t rate = jet_scope_sample_rate();
 */
uint32_t jet_scope_sample_rate(void);

/**
 * Function: jet_scope_head
 * Summary: A count of every frame the host has written to the scope since the machine started. It only ever goes up.
 * Parameters:
 *   - None.
 * Returns: The total number of frames written since the machine started. It wraps around to 0 after 2^32 frames. Frame number n sits in ring slot (n & JET_SCOPE_FRAME_MASK).
 * Include: jet_scope.h
 * Notes: The scope keeps its frames in a ring: a fixed row of slots that, once full, starts again from the first slot. The count grows by about sample_rate/120 frames each tick. To count the frames that arrived between two reads, subtract the older count from the newer one in unsigned arithmetic; that gives the right answer even across the wrap. Most cartridges never need this call, because the read and measuring calls do this work for them.
 * Example:
 *   uint32_t fresh = jet_scope_head() - last_head; // wrapping math
 */
uint32_t jet_scope_head(void);

/**
 * Function: jet_scope_output_latency_frames
 * Summary: The host's best estimate of the delay between a sound being mixed and the same sound reaching the speakers, in frames.
 * Parameters:
 *   - None.
 * Returns: The estimated delay, in frames at jet_scope_sample_rate, between a sample being mixed into the scope's ring and that sample reaching the sound output; 0 when the host does not know, or is too old to report it.
 * Include: jet_scope.h
 * Notes: This is the E1 host extension (docs/pulsar/design.md 10.1). The estimate is the depth of the host's own output buffer, plus whatever delay the sound device or the web player's audio context reports, where one is offered. A song's rows are recorded at the moment they are mixed, not the moment they are heard, so the scope runs ahead of the speakers by exactly this much. Code that lines pictures up with sound takes this delay off, so a flash lands when the note is heard rather than when it was mixed. Divide by jet_scope_sample_rate to turn it into seconds.
 * Example:
 *   float lead_s = (float)jet_scope_output_latency_frames() /
 *                  (float)jet_scope_sample_rate();
 */
uint32_t jet_scope_output_latency_frames(void);

/**
 * Function: jet_scope_row_anchor
 * Summary: Tells you exactly which frame the music's newest row started on, and which row it was. A row is one step of a tracker song.
 * Parameters:
 *   - out_head: Filled in with the scope's frame count at the moment the row was mixed: the frame that carries the row's first sample. May be NULL if you do not want it.
 *   - out_pattern: Filled in with the row's position in the song's order list, that is, which pattern the song was on. May be NULL.
 *   - out_line: Filled in with the row's line number inside that pattern. May be NULL.
 * Returns: true when the host publishes this anchor (the E2 extension); false on an older host, in which case the outputs are left untouched.
 * Include: jet_scope.h
 * Notes: This ties the song's time to the scope's time to the exact sample. Subtract out_head from jet_scope_head, in unsigned arithmetic so the wrap does no harm, to find how many frames the mixer has produced since the row began; take off jet_scope_output_latency_frames as well to find when the row reaches the speakers. The anchor carries its own row numbers, so you can never pair an old frame count with a new row; if the host commits a new row while you are reading, the read tries again by itself. Before a song's first row, all three values read zero.
 * Example:
 *   uint32_t at; uint8_t order, row;
 *   if (jet_scope_row_anchor(&at, &order, &row)) {
 *       uint32_t age = jet_scope_head() - at; // frames since the commit
 *   }
 */
bool jet_scope_row_anchor(uint32_t* out_head, uint8_t* out_pattern, uint8_t* out_line);

/**
 * Function: jet_scope_read_mono
 * Summary: Copies the newest n frames of sound into your array as mono, one value per frame, oldest first.
 * Parameters:
 *   - out: Your array, with room for n floats. out[n-1] is the newest sample. Each value is the left and right samples added and halved, (L+R)*0.5, and normally lies between -1 and 1. It can go a little outside that range, because the tap reads the sound before the host clamps it.
 *   - n: How many frames you want, up to JET_SCOPE_FRAMES (8192). A larger number is clamped to that.
 * Returns: The number of frames copied: n, or JET_SCOPE_FRAMES if n was larger. 0 when the scope is unavailable.
 * Include: jet_scope.h
 * Notes: The host writes the ring and your cartridge reads it, with no lock between them. If the host writes right over the part you are copying, the copy is done again, once. In practice that never happens: one tick brings in about 400 new frames, while the ring holds 8192.
 *
 *        The host clears the ring to silence each time a cartridge starts, so for the first 171 ms or so the older slots of your window read as silence. The frame count wraps around after 2^32 frames, about 24 hours at 48 kHz, and the copy stays correct across the wrap with no special handling.
 * Example:
 *   float wave[1280];
 *   uint32_t got = jet_scope_read_mono(wave, 1280);
 */
uint32_t jet_scope_read_mono(float* out, uint32_t n);

/**
 * Function: jet_scope_read_stereo
 * Summary: Copies the newest n frames of sound into two arrays, left and right, oldest first.
 * Parameters:
 *   - out_l: Your array for the left channel, with room for n floats.
 *   - out_r: Your array for the right channel, with room for n floats.
 *   - n: How many frames you want, up to JET_SCOPE_FRAMES (8192). A larger number is clamped to that.
 * Returns: The number of frames copied: n, or JET_SCOPE_FRAMES if n was larger. 0 when the scope is unavailable. See jet_scope_read_mono for what the first moments after start-up read as.
 * Include: jet_scope.h
 * Notes: The stereo partner of jet_scope_read_mono. Use it for a Lissajous scope (left drives x and right drives y), for a pair of stereo VU meters, or to draw each channel's waveform on its own.
 * Example:
 *   float l[512], r[512];
 *   uint32_t got = jet_scope_read_stereo(l, r, 512);
 */
uint32_t jet_scope_read_stereo(float* out_l, float* out_r, uint32_t n);

// --- Rate-invariant smoothing ---

/**
 * Function: jet_scope_env_rate
 * Summary: Converts a smoothing rate that was tuned at one tick rate into the rate that gives the same smoothing at another, so a smoothed value settles at the same speed in real time whatever the tick rate.
 * Parameters:
 *   - rate_at_ref: The smoothing rate that felt right at ref_hz, for example 0.85 tuned at 30 ticks a second. A smoothing rate is the share of its old value that a smoothed number keeps each tick; the rest comes from the new target.
 *   - ref_hz: The tick rate that rate was tuned at, in ticks a second.
 *   - actual_hz: The tick rate you are running at, in ticks a second, for example 120.
 * Returns: The rate to use at actual_hz: rate_at_ref raised to the power (ref_hz / actual_hz).
 * Include: jet_scope.h
 * Notes: Use the result as r in x = x*r + target*(1-r). This is the AdjustRateToFPS law from the Geiss visualiser (main.cpp:548), and it is the reason a look designed at 30 frames a second still looks right at 120.
 * Example:
 *   float r = jet_scope_env_rate(0.85f, 30.0f, 120.0f);
 *   env = env * r + vol * (1.0f - r);
 */
float jet_scope_env_rate(float rate_at_ref, float ref_hz, float actual_hz);

// --- Windowed statistics ---

/**
 * Function: jet_scope_rms
 * Summary: How loud the newest n frames are, as one number: the root mean square, or RMS, which is the everyday measure of steady loudness.
 * Parameters:
 *   - n: How many of the newest frames to measure, up to JET_SCOPE_FRAMES. A larger number is clamped to that.
 * Returns: The RMS of the mono value (L+R)*0.5 across those frames: square every sample, take the average, then the square root. 0 when the scope is unavailable or n is 0.
 * Include: jet_scope.h
 * Notes: This is the steady "how loud is it" meter. It moves more smoothly than jet_scope_peak. It reads the ring in one pass and needs no array of yours.
 * Example:
 *   float loudness = jet_scope_rms(2048);
 */
float jet_scope_rms(uint32_t n);

/**
 * Function: jet_scope_peak
 * Summary: The biggest swing in the newest n frames: the largest sample, ignoring its sign.
 * Parameters:
 *   - n: How many of the newest frames to measure, up to JET_SCOPE_FRAMES. A larger number is clamped to that.
 * Returns: The largest absolute value of the mono value (L+R)*0.5 across those frames. 0 when the scope is unavailable.
 * Include: jet_scope.h
 * Notes: This is the "did something just hit" meter. Show it beside jet_scope_rms and you have the classic pair of peak and average level meters.
 * Example:
 *   if (jet_scope_peak(400) > 0.9f) flash_clip_led();
 */
float jet_scope_peak(uint32_t n);

// --- Oscilloscope trigger ---

/**
 * Struct: JetScopeTrigger
 * Summary: The memory jet_scope_trigger keeps between calls, so that a waveform drawn like an oscilloscope holds still instead of sliding sideways from frame to frame.
 * Fields:
 *   - level: The sample value that was matched last time.
 *   - slope: The slope of the wave at that sample. Only its sign matters: it says whether the wave was rising or falling there.
 * Include: jet_scope.h
 * Notes: Set it to all zeros once, and pass the same one to jet_scope_trigger every tick. The memory carried from one call to the next is what makes a repeating wave stand still; if you make a fresh one each frame, the trigger cannot work.
 * Example:
 *   static JetScopeTrigger trigger;
 *   uint32_t got = jet_scope_read_mono(wave, WAVE_CAP);
 *   uint32_t start = jet_scope_trigger(&trigger, wave, got / 2, 0.01f);
 */
typedef struct {
    float level; // the sample value matched last tick
    float slope; // the slope at that sample (only its sign matters)
} JetScopeTrigger;

/**
 * Function: jet_scope_trigger
 * Summary: Finds where in a waveform to start drawing so that the picture holds still: the first sample that matches last tick's level and direction.
 * Parameters:
 *   - t: The trigger's memory, a JetScopeTrigger you zeroed once and pass in every call. It is updated each time.
 *   - wave: The waveform, for example from jet_scope_read_mono.
 *   - search: How many samples from the start of wave to look through. Keep it to half the window at most, so a full screen's width of wave is left after the start point.
 *   - level_tolerance: How close a sample must be to the remembered level to count as a match, in the same units as the samples. Size it to your sound: at least as much as the wave changes over the two-sample step the search takes, which is roughly amplitude * 4*pi*f/sample_rate for a tone of f Hz (about 0.06 for a half-height 440 Hz wave at 48 kHz). Too small and the search steps over the crossings and the picture jumps between cycles; a generous value is stable.
 * Returns: The offset into wave to start drawing from. 0 when nothing matched; the memory then latches on afresh, so the next tick can recover.
 * Include: jet_scope.h
 * Notes: This is the oscilloscope level trigger from the Geiss visualiser (main.cpp:8033). It looks at every second sample, insists that the wave be heading the same way as last time, and takes the first sample whose level is within the tolerance. Draw the wave from that offset onward and a steady tone stands still on the screen.
 * Example:
 *   static JetScopeTrigger trig; // zero-initialised
 *   uint32_t got = jet_scope_read_mono(wave, 1280);
 *   uint32_t at = jet_scope_trigger(&trig, wave, got / 2, 0.0078f);
 *   draw_wave(wave + at, got - at);
 */
uint32_t jet_scope_trigger(JetScopeTrigger* t, const float* wave, uint32_t search,
                            float level_tolerance);

// --- Volume statistics & beat detection (the Geiss engine) ---

/**
 * Struct: JetScopeBeat
 * Summary: The beat detector's memory. It takes the loudness of the sound each tick and turns it into a beat signal, a flash amount and smoothed loudness levels for your game to use.
 * Fields:
 *   - vol: The volume you gave it on the latest tick, in normalised units, where 1.0 is full scale.
 *   - avg_narrow: A fast-moving smoothed volume, tuned at 30 ticks a second and rescaled to your tick rate.
 *   - avg_mid: A medium-speed smoothed volume.
 *   - avg_wide: A slow-moving smoothed volume.
 *   - past: For the detector's own use: the last four seconds of volume, one entry 30 times a second.
 *   - past_pos: The slot in past where the next entry will go.
 *   - beat_strength: How bumpy the volume history is, scaled to run from about 0 to 200.
 *   - beat_mode: true while a steady beat is going. It switches on when beat_strength rises above 109 and off when it falls below 71, so it does not flicker in between.
 *   - big_beat: true on a tick with a standout hit.
 *   - big_beat_threshold: How much louder than the recent past a hit must be to count as big. The default is 1.10.
 *   - brightness: A flash amount from 0 to 1, worked out from the recent loudness.
 *   - silence_floor: When the average of the history is below this, beat_strength is forced to zero.
 *   - tape_mean: The average of the four-second history, kept up to date. Read it, never write it.
 *   - tape_std: The standard deviation of the history (how far the volume tends to wander from its average), kept up to date. Read it, never write it.
 *   - tape_max: The largest value in the history, kept up to date. Read it, never write it.
 *   - tick_accum: For the detector's own use: the counter that turns your ticks into 30 entries a second.
 * Include: jet_scope.h
 * Notes: Set it up once with jet_scope_beat_init, then call jet_scope_beat_tick once, and only once, in every _update. Read big_beat for a one-tick event, beat_mode for a beat that keeps going, and brightness for a smooth pulse in your picture. Volumes are in normalised sample units; jet_scope_peak over about one tick's worth of frames is a good thing to feed it.
 * Example:
 *   static JetScopeBeat beat;
 *   void _init(void) { jet_scope_beat_init(&beat); }
 *   void _update(float dt) {
 *       uint32_t frames = jet_scope_sample_rate() / JET_GUEST_TICK_HZ;
 *       jet_scope_beat_tick(&beat, jet_scope_peak(frames), JET_GUEST_TICK_HZ);
 *   }
 */
typedef struct {
    float vol;         // the volume fed in this tick
    float avg_narrow;  // fast smoothing   (rate 0.30 at 30 Hz, rescaled)
    float avg_mid;     // medium smoothing (rate 0.85 at 30 Hz, rescaled)
    float avg_wide;    // slow smoothing   (rate 0.96 at 30 Hz, rescaled)

    // The "volume tape": avg_narrow sampled 30 times a second, a sliding
    // four-second window whose bumpiness is the signature of a beat.
    float past[JET_SCOPE_BEAT_TAPE_N];
    uint32_t past_pos;

    float beat_strength;      // bumpiness of the tape, scaled to about 0..200
    bool beat_mode;           // on above 109, off below 71, unchanged in between
    bool big_beat;            // avg_narrow beat the tape's windowed maximum
    float big_beat_threshold; // the big-beat gate; a scene director may lower it over time
    float brightness;         // (vol - mean) / (0.5 * stddev), clamped to 0..1
    float silence_floor;      // a tape mean below this forces beat_strength to 0

    // The detector's own bookkeeping: the tape's cached statistics (worked out
    // at the tape's 30 Hz pace) and the counter that thins ticks down to that
    // pace. Read freely; never write.
    float tape_mean;
    float tape_std;
    float tape_max;
    float tick_accum;
} JetScopeBeat;

/**
 * Function: jet_scope_beat_init
 * Summary: Puts a JetScopeBeat back to its starting state, with the default settings.
 * Parameters:
 *   - b: The beat detector to set up.
 * Returns: Nothing.
 * Include: jet_scope.h
 * Notes: It sets the Geiss defaults: big_beat_threshold to 1.10, and silence_floor to the normalised silence floor, JET_SCOPE_BEAT_SILENCE_FLOOR. Call it once in _init, or again to start afresh after the song changes.
 * Example:
 *   static JetScopeBeat beat;
 *   jet_scope_beat_init(&beat);
 */
void jet_scope_beat_init(JetScopeBeat* b);

/**
 * Function: jet_scope_beat_tick
 * Summary: Feeds the beat detector this tick's volume and moves it forward one tick.
 * Parameters:
 *   - b: The beat detector.
 *   - vol: This tick's volume, in normalised sample units. The Geiss way is to use half the peak-to-peak of the tick's mono frames, that is, half the distance from the lowest sample to the highest.
 *   - tick_hz: Your tick rate, in ticks a second: JET_GUEST_TICK_HZ for a normal cartridge. Every rate inside the detector is rescaled from the 30 Hz reference for you.
 * Returns: Nothing. The results are in the fields: beat_mode (a steady beat is on), big_beat (this tick is a standout hit), brightness (a flash amount from 0 to 1), beat_strength, and the three avg_ smoothed volumes.
 * Include: jet_scope.h
 * Notes: The arithmetic follows the Geiss code exactly. The smoothed volumes are rescaled with jet_scope_env_rate, and the volume history is thinned to its 30 entries a second, so the thresholds (on above 109 and off below 71 for beat_mode, 1.10 for a big beat) behave the same at any tick rate. Three lines make a game pulse to its music:
 * Example:
 *   float vol = jet_scope_peak((uint32_t)(jet_scope_sample_rate() / 120));
 *   jet_scope_beat_tick(&beat, vol, JET_GUEST_TICK_HZ);
 *   if (beat.big_beat) kick_background_pulse();
 */
void jet_scope_beat_tick(JetScopeBeat* b, float vol, float tick_hz);

#endif // JET_SCOPE_H
