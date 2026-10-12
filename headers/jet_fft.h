// runtime/jet_fft.h
#ifndef JET_FFT_H
#define JET_FFT_H

// ============================================================================
// Jet64 FFT — spectrum analysis for carts (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_scope.h it is deliberately kept out of jet64.h, so it costs
// nothing unless a cart asks for it: the cart resolver compiles jet_fft.c
// only when a cart's #include tree reaches this header.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_fft.h"     // <-- explicit opt-in
//
// Turn a window of audio samples into spectrum magnitudes or display-ready
// frequency bands. Keep one JetFft workspace and its sample/output arrays
// persistent: initialise once, apply a window, transform, then measure or bin.
// The transform size must be a power of two from JET_FFT_MIN_N (256) through
// JET_FFT_MAX_N (4096). An invalid size leaves the workspace disabled (n = 0),
// and later transform calls safely do nothing.
//
// <nocodex>
// A textbook iterative radix-2 real-input FFT with precomputed twiddle and
// bit-reversal tables, plus the standard music-visualization garnish: a Hann
// window, magnitude extraction, log-frequency band binning (the Winamp /
// Mixing Station analyser shape), dB conversion, and per-band attack/release
// smoothing so bars fall gracefully.
//
// Zero allocation: the caller owns the JetFft workspace (WRAM via
// jet_memory_reserve, or cart BSS) and every output buffer. A 2048-point
// forward transform costs on the order of tens of microseconds — analysis
// every tick at 120 Hz is comfortably in budget.
// </nocodex>
//
// Pairs naturally with jet_scope.h: read the master bus, window it,
// transform it, bin it, draw it.
//
// Complete spectrum example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_scope.h"
//   #include "jet_fft.h"
//
//   enum { FFT_SIZE = 256, BIN_COUNT = FFT_SIZE / 2, BAR_COUNT = 32 };
//   static JetFft fft;
//   static float samples[FFT_SIZE], real[FFT_SIZE], imag[FFT_SIZE], magnitude[BIN_COUNT];
//
//   void _init(void) {
//       jet_fft_init(&fft, FFT_SIZE);
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (jet_scope_read_mono(samples, FFT_SIZE) == FFT_SIZE) {
//           jet_fft_window_hann(samples, FFT_SIZE);
//           jet_fft_forward(&fft, samples, real, imag);
//           jet_fft_magnitudes(real, imag, magnitude, BIN_COUNT);
//       }
//   }
//
//   void _draw(void) {
//       const int bar_width = JET_WIDTH / BAR_COUNT;
//       jet_cls(0);
//       for (int i = 0; i < BAR_COUNT; i++) {
//           const int height = (int)jet_clampf(magnitude[i] * 0.08f, 0.0f, (float)JET_HEIGHT);
//           jet_rectfill(i * bar_width, JET_HEIGHT - height, bar_width - 1, height, 12);
//       }
//   }
//
//   void _end(void) {}

#include "jet_core.h" // IWYU pragma: keep (stdint/bool, math)

/**
 * Constant: JET_FFT_MAX_N
 * Summary: Largest supported FFT transform size and the fixed table capacity inside JetFft.
 * Include: jet_fft.h
 * Notes: The transform size passed to jet_fft_init must be a power of two no larger than this value. Larger transforms leave the workspace disabled with n equal to zero.
 * Example:
 *   jet_fft_init(&fft, JET_FFT_MAX_N);
 */
#define JET_FFT_MAX_N 4096

/**
 * Constant: JET_FFT_MIN_N
 * Summary: Smallest supported FFT transform size.
 * Include: jet_fft.h
 * Notes: The transform size must be a power of two from this value through JET_FFT_MAX_N. Smaller windows are rejected because they provide little useful spectral detail for a music visualiser.
 * Example:
 *   jet_fft_init(&fft, JET_FFT_MIN_N);
 */
#define JET_FFT_MIN_N 256

/**
 * Constant: JET_FFT_DB_FLOOR
 * Summary: Finite decibel value returned by jet_fft_to_db for silence and extremely small magnitudes.
 * Include: jet_fft.h
 * Notes: Using a floor avoids negative infinity and gives displays a stable lower bound. Map values from this floor to 0 dB when drawing a conventional analyser.
 * Example:
 *   float height01 = (db - JET_FFT_DB_FLOOR) / -JET_FFT_DB_FLOOR;
 */
#define JET_FFT_DB_FLOOR (-96.0f)

// Amplitude-quantity decibel scale: dB = 20·log10(mag), i.e. 20 dB per decade
// of magnitude. FFT magnitudes are amplitudes, not powers — power quantities
// would use 10. This is physics, not a tuning knob: rescale your display
// range instead (see jet_fft_to_db's normalisation note).
#define JET_FFT_DB_PER_DECADE (20.0f)

/**
 * Struct: JetFft
 * Summary: Caller-owned FFT workspace containing the lookup tables for one fixed transform size.
 * Fields:
 *   - n: Active transform size, a power of two from 256 to 4096; 0 means initialisation failed.
 *   - cos_tab: Internal cosine twiddle factors filled by jet_fft_init.
 *   - sin_tab: Internal sine twiddle factors filled by jet_fft_init.
 *   - rev_tab: Internal bit-reversal permutation filled by jet_fft_init.
 * Include: jet_fft.h
 * Notes: Keep one persistent instance and initialise it once from _init. The struct is about 41 KiB regardless of n, so declare it static/global rather than as a local stack variable. Treat the tables as read-only; reuse them for every transform of the same size.
 * Example:
 *   static JetFft fft;
 *   void _init(void) { jet_fft_init(&fft, 2048); }
 */
typedef struct {
    uint32_t n;                            // transform size (power of two)
    float cos_tab[JET_FFT_MAX_N / 2];     // cos(2*pi*k/n) twiddles
    float sin_tab[JET_FFT_MAX_N / 2];     // sin(2*pi*k/n) twiddles
    uint16_t rev_tab[JET_FFT_MAX_N];      // bit-reversal permutation
} JetFft;

/**
 * Function: jet_fft_init
 * Summary: Prepares an FFT workspace for transforms of size n.
 * Parameters:
 *   - f: The workspace to fill (caller-owned; WRAM or cart BSS).
 *   - n: Transform size — a power of two in [JET_FFT_MIN_N, JET_FFT_MAX_N]
 *        (256..4096). Out-of-range or non-power-of-two sizes leave the
 *        workspace disabled (n = 0) and every transform a no-op.
 * Returns: Nothing.
 * Include: jet_fft.h
 * Notes: USE ONLY IN _init() spirit — it fills ~41 KiB of tables; re-init only
 *        to change size. 2048 is the sweet spot for a music analyser at
 *        44.1/48 kHz (≈43 ms window, ≈23 Hz per bin).
 * Example:
 *   static JetFft fft;
 *   jet_fft_init(&fft, 2048);
 */
void jet_fft_init(JetFft* f, uint32_t n);

/**
 * Function: jet_fft_window_hann
 * Summary: Applies a Hann window to a sample buffer in place.
 * Parameters:
 *   - x: The samples to window.
 *   - n: Sample count.
 * Returns: Nothing.
 * Include: jet_fft.h
 * Notes: Windowing before the transform suppresses the spectral leakage of a
 *        non-periodic capture window — without it, one loud tone smears
 *        across the whole spectrum. Hann's coherent gain is 0.5: a full-scale
 *        sine reads as n/4 magnitude instead of n/2.
 * Example:
 *   jet_fft_window_hann(window, 2048);
 */
void jet_fft_window_hann(float* x, uint32_t n);

/**
 * Function: jet_fft_forward
 * Summary: Forward FFT of n real samples into n complex bins.
 * Parameters:
 *   - f: An initialised workspace (jet_fft_init).
 *   - in: n real input samples (e.g. from jet_scope_read_mono, windowed).
 *   - re: Output, n floats — real parts. May NOT alias `in`.
 *   - im: Output, n floats — imaginary parts.
 * Returns: Nothing (a no-op if the workspace failed to initialise).
 * Include: jet_fft.h
 * Notes: Only bins [0, n/2) are meaningful for real input (the upper half
 *        mirrors); feed them to jet_fft_magnitudes. Bin k is centred at
 *        k * sample_rate / n Hz.
 * Example:
 *   jet_fft_forward(&fft, window, re, im);
 *   jet_fft_magnitudes(re, im, mag, 1024);
 */
void jet_fft_forward(const JetFft* f, const float* in, float* re, float* im);

/**
 * Function: jet_fft_magnitudes
 * Summary: Converts complex bins to magnitudes: mag[k] = sqrt(re^2 + im^2).
 * Parameters:
 *   - re: Real parts (from jet_fft_forward).
 *   - im: Imaginary parts.
 *   - mag: Output, half_n floats.
 *   - half_n: Bin count to convert — n/2 for a real-input transform.
 * Returns: Nothing.
 * Include: jet_fft.h
 * Notes: For real input, request half_n = fft.n / 2. Magnitudes are not normalised; divide by the desired reference (for example n / 4 after a Hann window) before converting to dB.
 * Example:
 *   jet_fft_magnitudes(re, im, mag, 1024);
 */
void jet_fft_magnitudes(const float* re, const float* im, float* mag, uint32_t half_n);

/**
 * Function: jet_fft_bands_log
 * Summary: Folds linear FFT bins into log-spaced frequency bands — the
 *          classic analyser-bar shape (equal screen width per octave).
 * Parameters:
 *   - mag: half_n linear-bin magnitudes (jet_fft_magnitudes).
 *   - half_n: Linear bin count (n/2).
 *   - sample_rate: The capture rate in Hz (jet_scope_sample_rate()) — bin
 *                  frequencies depend on it, never assume 48000.
 *   - f_lo: Lowest band edge in Hz (e.g. 40).
 *   - f_hi: Highest band edge in Hz (e.g. 16000; clamped to Nyquist).
 *   - bands: Output, band_count floats — mean magnitude per band.
 * - band_count: Number of bands (e.g. 64).
 * Returns: Nothing.
 * Include: jet_fft.h
 * Notes: Every linear bin inside [f_lo, f_hi) lands in exactly one band; a
 *        band too narrow to own any bin (possible at low frequencies with
 *        many bands) is filled by sampling the spectrum at its centre
 *        frequency, so no bar ever reads falsely dead.
 * Example:
 *   float bars[64];
 *   jet_fft_bands_log(mag, 1024, jet_scope_sample_rate(), 40.0f, 16000.0f, bars, 64);
 */
void jet_fft_bands_log(const float* mag, uint32_t half_n, uint32_t sample_rate, float f_lo,
                        float f_hi, float* bands, uint32_t band_count);

/**
 * Function: jet_fft_to_db
 * Summary: Converts a linear magnitude to decibels: 20*log10(mag).
 * Parameters:
 *   - mag: Linear magnitude (>= 0).
 * Returns: dB value, floored at JET_FFT_DB_FLOOR (-96) — silence never
 *          returns -inf.
 * Include: jet_fft.h
 * Notes: Normalise magnitudes to your own reference before converting if you
 *        want 0 dB to mean full scale (a Hann-windowed full-scale sine peaks
 *        at n/4).
 * Example:
 *   float db = jet_fft_to_db(bars[i] / (2048.0f / 4.0f));
 */
float jet_fft_to_db(float mag);

/**
 * Function: jet_fft_bands_smooth
 * Summary: Per-band attack/release smoothing — bars leap up and fall
 *          gracefully instead of flickering.
 * Parameters:
 *   - smoothed: The persistent smoothed bands, updated in place.
 *   - fresh: This tick's fresh band values (jet_fft_bands_log).
 *   - band_count: Number of bands.
 *   - attack: Smoothing rate applied when a band rises (0 = jump instantly,
 *             closer to 1 = slower). Retime with jet_scope_env_rate.
 *   - release: Smoothing rate applied when a band falls (typically slower
 *              than attack, e.g. 0.85 at 30 Hz).
 * Returns: Nothing.
 * Include: jet_fft.h
 * Notes: Each band computes s = s*rate + fresh*(1-rate), choosing attack or
 *        release by direction. Pre-retime both rates to your tick rate —
 *        rates are per *tick*, not per second.
 * Example:
 *   float rel = jet_scope_env_rate(0.85f, 30.0f, JET_GUEST_TICK_HZ);
 *   jet_fft_bands_smooth(bars_smoothed, bars, 64, 0.0f, rel);
 */
void jet_fft_bands_smooth(float* smoothed, const float* fresh, uint32_t band_count, float attack,
                           float release);

#endif // JET_FFT_H
