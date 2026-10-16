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
// oscilloscope does, and follow the beat of the music — hear each new sound
// start, know the tempo, and see the next beat coming — so your pictures
// can move to the sound. This header is not part of jet64.h; include it
// yourself, as shown above.
//
// <nocodex>
// jet_scope reads the AUDIO SCOPE hardware block (JET_SCOPE_OFFSET): a
// host-written ring of the post-mix master bus — everything the speakers get,
// tapped at the Mixing Station analyser's point. On top of the raw ring it
// offers a music-visualization toolkit of its own design: rate-invariant
// envelope smoothing, windowed RMS/peak statistics, an oscilloscope sync that
// holds a waveform still by matching its shape, and a pulse detector — onsets
// from the rise in five bands' loudness, the tempo from their
// autocorrelation, and the beat's phase from a resonator tuned to it — so a
// picture can land ON the beat rather than after it. docs/audio/scope_pulse.md
// is the design record and its measurement.
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
 * Constant: JET_SCOPE_PULSE_BANDS
 * Summary: How many bands of pitch JetScopePulse listens to separately: five, from the bass up to the hiss.
 * Include: jet_scope.h
 * Notes: Band 0 is the bass and the kick drum, below about 120 Hz; band 1 the low notes, to about 500 Hz; band 2 the middle, to about 2 kHz; band 3 the upper middle; band 4 the air, the hiss of hi-hats and cymbals. JetScopePulse.band_flux holds one value for each, lowest first, so a picture can answer the kick drum and the hi-hat differently.
 * Example:
 *   float kick = pulse.band_flux[0], hats = pulse.band_flux[JET_SCOPE_PULSE_BANDS - 1];
 */
#define JET_SCOPE_PULSE_BANDS 5

/**
 * Constant: JET_SCOPE_PULSE_HISTORY
 * Summary: How many of its analysis frames JetScopePulse remembers for its tempo search: a little over two seconds.
 * Include: jet_scope.h
 * Notes: The detector listens in frames of 1/120 of a second, whatever the sound device's rate, and keeps the last 256 frames of onset strength. That is long enough to compare each frame with the one two beats before it at the slowest tempo it reports, 60 beats a minute. It sizes arrays inside JetScopePulse; you never need it to use the detector.
 * Example:
 *   float seconds = (float)JET_SCOPE_PULSE_HISTORY / 120.0f; // about 2.1
 */
#define JET_SCOPE_PULSE_HISTORY 256

/**
 * Constant: JET_SCOPE_PULSE_SILENCE_DB_DEFAULT
 * Summary: The default loudness, 60 dB below full scale, under which JetScopePulse treats the sound as silence.
 * Include: jet_scope.h
 * Notes: jet_scope_pulse_init puts this value in JetScopePulse.silence_db. A frame quieter than this starts no onset, and half a second of them ends a steady beat. Raise it to ignore a noisy background; lower it to follow a very quiet mix. Everything else the detector measures is relative, so the music's volume makes no other difference.
 * Example:
 *   pulse.silence_db = JET_SCOPE_PULSE_SILENCE_DB_DEFAULT + 12.0f; // ignore a hissy line
 */
#define JET_SCOPE_PULSE_SILENCE_DB_DEFAULT (-60.0f)

/**
 * Constant: JET_SCOPE_SYNC_POINTS
 * Summary: How many points of the drawn waveform JetScopeSync remembers as its shape.
 * Include: jet_scope.h
 * Notes: The sync samples the stretch you draw at this many evenly spaced points and keeps a running average of that shape, which is what it matches the next tick's wave against. It sizes the memory inside JetScopeSync; you never need it to use the sync.
 * Example:
 *   uint32_t spacing = span / JET_SCOPE_SYNC_POINTS; // samples between remembered points
 */
#define JET_SCOPE_SYNC_POINTS 32

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
 * Notes: Use the result as r in x = x*r + target*(1-r). Running n ticks of a rate r keeps r to the power n of the old value, so a rate tuned at ref_hz ticks a second keeps the same share each second once it is raised to ref_hz / actual_hz. That is why a look designed at 30 frames a second still looks right at 120.
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

// --- Oscilloscope sync ---

/**
 * Struct: JetScopeSync
 * Summary: The memory jet_scope_sync keeps between calls: the shape of the waveform it showed last, so the next picture can be lined up with it.
 * Fields:
 *   - state: The sync's own working memory: the remembered shape and how loud the wave was. Leave it alone.
 * Include: jet_scope.h
 * Notes: Zero it once (a static one starts zeroed), then pass the same one to jet_scope_sync every tick. Keep one for each waveform you draw: two scopes sharing one would pull each other's picture about.
 * Example:
 *   static JetScopeSync sync; // zero-initialised: ready to use
 *   uint32_t at = jet_scope_sync(&sync, wave, got, 640);
 */
typedef struct {
    struct {
        float shape[JET_SCOPE_SYNC_POINTS]; // the remembered picture, scaled to unit size
        uint32_t spacing;                   // samples between the remembered points
        float level;                        // the smoothed wave's RMS on the last call
        bool primed;                        // a shape has been remembered
    } state; // the sync's own working memory; leave it alone
} JetScopeSync;

/**
 * Function: jet_scope_sync
 * Summary: Finds where in a waveform to start drawing so that the picture holds still from one tick to the next, the job an oscilloscope's trigger does.
 * Parameters:
 *   - s: The sync's memory, a JetScopeSync you zeroed once and pass in every call. It is updated each time.
 *   - wave: The waveform, oldest sample first, for example from jet_scope_read_mono.
 *   - n: How many samples wave holds.
 *   - span: How many samples you will draw from the start point. The start is chosen so that all of them lie inside wave.
 * Returns: The offset into wave to start drawing from, between 0 and n - span. 0 when n is not larger than span.
 * Include: jet_scope.h
 * Notes: It works in two steps. First it finds the candidates: the places where a smoothed copy of the wave rises through zero, with a little hysteresis so that noise cannot make one. The smoothing keeps the strong low notes, so the candidates fall at one point of the bass line's cycle. Then it scores each candidate by how closely the picture drawn from there matches the shape it remembers from earlier ticks, and takes the best, the newest one when two are as good. A steady tone therefore stands still to within a sample, and a mix of notes keeps the same overall picture instead of hopping between cycles. When no crossing falls inside the window, a bass note slower than the window for example, it searches the window for the best match to the remembered shape directly.
 *
 *        Give it a window longer than you draw: the extra, n - span, is the room it searches, and one period of the lowest note you want held still is plenty. There is no tolerance to tune, because the hysteresis follows the wave's own loudness.
 * Example:
 *   static JetScopeSync sync;
 *   float wave[1280];
 *   uint32_t got = jet_scope_read_mono(wave, 1280);
 *   uint32_t at = jet_scope_sync(&sync, wave, got, 640);
 *   draw_wave(wave + at, 640);
 */
uint32_t jet_scope_sync(JetScopeSync* s, const float* wave, uint32_t n, uint32_t span);

// --- The pulse: onsets, tempo and the beat ---

// <nocodex>
// The detector reads the sound in groups of this many samples, one decimated
// sample each, and keeps an unfinished group in its state until the rest of
// it arrives — which is what makes its answers independent of how the sound
// is split between calls.
#define JET_SCOPE_PULSE_GROUP 8
// </nocodex>

/**
 * Struct: JetScopePulse
 * Summary: The pulse detector: it listens to the sound tick by tick and tells you when a new sound starts, how fast the beat is, where you are inside the beat, and whether the music has a steady beat at all.
 * Fields:
 *   - onset: true on a tick in which a new sound started: a drum hit, a plucked note, a chord struck. It reports what has happened, within about 10 ms of it.
 *   - accent: How hard that onset hit next to the recent ones: 1 is a typical hit, 2 twice as strong. 0 on a tick with no onset.
 *   - flux: The newest frame's onset strength: how far the loudness of the five bands rose, added up, in decibels. 0 in a steady sound, a few decibels for a soft note, tens for a drum.
 *   - band_flux: The same rise for each band on its own, lowest first (see JET_SCOPE_PULSE_BANDS): a kick drum shows in band 0, a hi-hat in band 4.
 *   - beat: true on the tick the detector expects a beat, foreseen from the beats before it rather than heard. Only while steady is true.
 *   - phase: How far the current beat has got, from 0 on the beat to nearly 1 just before the next. It moves smoothly, so a picture can swing with it.
 *   - bpm: The tempo the detector has settled on, in beats a minute, between 60 and 200; 0 until it has heard enough.
 *   - confidence: How sure it is of that tempo, from 0 to 1: how closely the onsets repeat at that beat. Noise reads under 0.1; music with a clear beat reads 0.3 to 0.9.
 *   - steady: true while the music has a steady beat. It switches on when confidence rises above 0.25, and off when confidence falls below 0.15, when no onset has come for 2.5 seconds, or after half a second of silence.
 *   - silence_db: A setting: the loudness, in decibels below full scale, under which the sound counts as silence. jet_scope_pulse_init sets JET_SCOPE_PULSE_SILENCE_DB_DEFAULT.
 *   - state: The detector's own working memory: its band filters, its record of onset strength and that record's autocorrelation, and the resonator that keeps the beat's phase. Leave it alone.
 * Include: jet_scope.h
 * Notes: Set it up once with jet_scope_pulse_init, then call jet_scope_pulse_tick once in every _update. Each call hears exactly the frames the host has written since the call before, so nothing is missed or heard twice. The detector is deterministic: the same samples give the same answers, bit for bit, however they are split between calls.
 *
 *        Use onset to react to what you hear, beat and phase to move with the music's pulse, and steady to tell music with a beat from a held chord, speech or noise. The next beat is (1 - phase) * 60 / bpm seconds away, so a picture can start moving before it lands. The scope runs ahead of the speakers by jet_scope_output_latency_frames, so a flash that must land when the beat is heard can wait that long.
 * Example:
 *   static JetScopePulse pulse;
 *   void _init(void) { jet_scope_pulse_init(&pulse); }
 *   void _update(float dt) {
 *       jet_scope_pulse_tick(&pulse);
 *       if (pulse.beat) kick_background_pulse();
 *       float swing = pulse.steady ? sinf(pulse.phase * JET_TAU) : 0.0f;
 *       tilt_camera(swing);
 *   }
 */
typedef struct {
    bool onset;                             // a new sound started in the frames this call heard
    float accent;                           // that onset over the recent ones (1 = typical)
    float flux;                             // the newest frame's summed rise in band loudness, dB
    float band_flux[JET_SCOPE_PULSE_BANDS]; // that rise band by band, lowest first
    bool beat;                              // the foreseen beat fell in the frames this call heard
    float phase;                            // 0 on the beat, rising to 1 at the next
    float bpm;                              // the tempo, beats a minute (0 = none yet)
    float confidence;                       // how strongly the onsets repeat at that tempo, 0..1
    bool steady;                            // the music has a steady beat (hysteretic)
    float silence_db;                       // setting: frames quieter than this are silence

    struct {
        uint32_t sample_rate;                      // the rate the filters are tuned for (0 = none)
        uint32_t head;                             // the scope frame the next tick reads from
        bool listening;                            // head is valid
        float pending[JET_SCOPE_PULSE_GROUP];      // an unfinished group of samples
        uint32_t pending_count;                    // how many samples it holds
        uint32_t frame_groups;                     // groups in the frame being gathered
        uint32_t frame_fill;                       // groups gathered so far
        uint32_t frame_carry;                      // spreads uneven frames evenly (sample units)
        float split_low, split_mid, split_high;    // the band splits' one-pole coefficients
        float low_a, low_b, mid, high;             // the band splits' filter states
        float prev, prev2;                         // the last two pair sums (the air band's)
        float energy[JET_SCOPE_PULSE_BANDS];       // each band's energy so far this frame
        float energy_mean;                         // the group means' energy so far this frame
        float level_db[JET_SCOPE_PULSE_BANDS];     // each band's loudness last frame
        float peak_db[JET_SCOPE_PULSE_BANDS];      // each band's decaying recent peak
        float step_db[JET_SCOPE_PULSE_BANDS];      // each band's typical frame-to-frame movement
        float flux_mean, flux_var;                 // the summed onset threshold's statistics
        float band_flux_mean[JET_SCOPE_PULSE_BANDS]; // each band's own onset threshold's
        float band_flux_var[JET_SCOPE_PULSE_BANDS];  // statistics (the upper bands use theirs)
        float onset_mean;                          // a typical onset's flux (accent's yardstick)
        uint32_t since_onset;                      // frames since the last onset
        float tempo_mean;                          // the tempo strength's running mean
        float tempo_prev, tempo_prev2;             // the smoothing taps on the tempo strength
        float history[JET_SCOPE_PULSE_HISTORY];    // the smoothed tempo strength, a ring
        uint32_t history_pos;                      // the newest entry in history
        float acf[JET_SCOPE_PULSE_HISTORY];        // its leaky autocorrelation, by lag in frames
        float prior[JET_SCOPE_PULSE_HISTORY / 2];  // the tempo prior, by lag in frames
        float period;                              // frames per beat (0 = none yet)
        uint32_t search_wait;                      // frames until the next full tempo search
        float res_re, res_im;                      // the beat resonator
        float rot_re, rot_im;                      // its turn per frame (with its decay)
        float lead_re, lead_im;                    // its turn by the analysis lag
        float rot_period;                          // the period that turn was built for
        uint32_t since_beat;                       // frames since the last foreseen beat
        uint32_t silent_frames;                    // consecutive silent frames
        uint32_t heard_frames;                     // frames of sound since the last long pause
        float silence_power;                       // silence_db as a mean square
        float silence_for_db;                      // the silence_db that power was worked out for
    } state; // the detector's own working memory; leave it alone
} JetScopePulse;

/**
 * Function: jet_scope_pulse_init
 * Summary: Puts a JetScopePulse back to its starting state, ready to listen.
 * Parameters:
 *   - p: The detector to set up. NULL is ignored.
 * Returns: Nothing.
 * Include: jet_scope.h
 * Notes: It clears everything the detector has learnt and sets silence_db to JET_SCOPE_PULSE_SILENCE_DB_DEFAULT. Call it once in _init, or again to start afresh, after a change of song for instance; without one, the detector follows a new song's tempo by itself within a few seconds.
 * Example:
 *   static JetScopePulse pulse;
 *   jet_scope_pulse_init(&pulse);
 */
void jet_scope_pulse_init(JetScopePulse* p);

/**
 * Function: jet_scope_pulse_tick
 * Summary: Listens to every frame of sound the host has written since the last call, and brings the detector's answers up to date.
 * Parameters:
 *   - p: The detector, set up with jet_scope_pulse_init.
 * Returns: Nothing. The answers are in the fields: onset, accent, flux, band_flux, beat, phase, bpm, confidence and steady.
 * Include: jet_scope.h
 * Notes: Call it once in every _update. The first call starts listening from the newest frames. If the cartridge stalls for longer than the scope's ring holds, about 171 ms, the oldest of the missed frames are gone and the detector carries on from the ones still there. With no scope, or a host too old to write one, it does nothing and steady stays false.
 * Example:
 *   jet_scope_pulse_tick(&pulse);
 *   if (pulse.onset) spawn_sparks(pulse.accent);
 */
void jet_scope_pulse_tick(JetScopePulse* p);

/**
 * Function: jet_scope_pulse_feed
 * Summary: Gives the detector samples of your own to listen to, instead of the scope's: a sound you are generating, or one read some other way.
 * Parameters:
 *   - p: The detector, set up with jet_scope_pulse_init.
 *   - mono: The samples, oldest first, each normally between -1 and 1.
 *   - n: How many samples there are.
 *   - sample_rate: Their rate in samples a second, for example 48000. 0 ignores the call. A new rate tunes the filters afresh and starts the detector over.
 * Returns: Nothing. The answers are in the fields, exactly as jet_scope_pulse_tick leaves them.
 * Include: jet_scope.h
 * Notes: jet_scope_pulse_tick is this call fed with the scope's newest frames. Feed each sample once, in order: the answers describe the samples of this call, so onset and beat are cleared at its start. Do not mix it with jet_scope_pulse_tick on the same detector.
 * Example:
 *   jet_scope_pulse_feed(&pulse, my_synth_out, 400, 48000);
 */
void jet_scope_pulse_feed(JetScopePulse* p, const float* mono, uint32_t n, uint32_t sample_rate);

#endif // JET_SCOPE_H
