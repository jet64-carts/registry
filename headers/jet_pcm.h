// runtime/jet_pcm.h
#ifndef JET_PCM_H
#define JET_PCM_H

// ============================================================================
// Jet64 PCM channels: samples and streamed sound, Amiga Paula style (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_audio_ext.h it is deliberately kept out of jet64.h, so it costs
// nothing unless a cart asks for it: the cart resolver compiles jet_pcm.c only
// when a cart's #include tree reaches this header. Carts that never use PCM pay
// zero compile time and zero binary size.
// </nocodex>
//
// Play recorded sound, and sound you make yourself one number at a time. A
// sample is a list of numbers, each one the height of the sound wave at one
// instant. Each number is called a frame; play the frames back fast enough and
// you hear the sound. This header gives you JET_PCM_CHANNEL_COUNT (8) channels
// for samples, in the style of Paula, the Amiga's sound chip. They are
// separate from the synth channels that play sound effects and music, and they
// are mixed into the same output. Each channel can play a sample once, play it
// in a loop, or stream sound you keep generating through a ring buffer, a
// block of memory that you write into and the host reads out of. Streaming is
// how a cart makes live sound: a chiptune core, a voice, an Amiga-style
// sampled instrument. A channel plays mono sound, which its pan places between
// the speakers, or stereo sound, which already has a left and a right of its
// own; the sample's JetPcmFormat says which. For whole pieces of recorded
// music, jet_ogg.h plays compressed .ogg files through these channels.
//
// The rest of Jet64's sound lives in jet64.h. This header is opt-in: include
// it yourself, after jet64.h, and only when you want it.
//
//   #include "jet64.h"
//   #include "jet_pcm.h"     // <-- explicit opt-in
//
// ----------------------------------------------------------------------------
// THE ONE HARD RULE: SAMPLE DATA MUST LIVE IN WRAM
// ----------------------------------------------------------------------------
// The host, the program that runs your cartridge, is what plays the sound, and
// it can see only one part of your cartridge's memory: the shared working
// memory, WRAM. It cannot see a static array baked into your code, a string,
// or anything on the stack. So every sample and every ring buffer you hand to
// a jet_pcm call must be in WRAM. jet_pcm_load and jet_pcm_load_wav take care
// of this for you: each reserves WRAM, fills it from the bundled file, and
// gives you back a JetPcmSample ready to play. Call jet_memory_reserve
// yourself only for sample data you generate by hand and for streaming ring
// buffers.
//
// A pointer that is not in WRAM is refused: the runtime logs a warning and the
// channel stays silent.
//
// <nocodex>
// In the workstation the cartridge runs as a separate process; the host audio
// thread can read only the shared motherboard memory, never the cart's own
// .rodata or stack. So every sample/ring buffer you hand to jet_pcm MUST be
// reserved in WRAM with jet_memory_reserve() (then filled or generated in
// place) — a `static const int8_t kick[]` baked into the binary is INVISIBLE to
// the mixer and will be silent. The helpers convert your WRAM pointer to a
// motherboard-relative offset, assert it lies in WRAM (emitting a clear log
// warning otherwise), and the host bounds-checks every access.
// </nocodex>
//
// The numbers work as they do in the rest of Jet64's sound calls: volume is 0
// to 255, pan is 0 to 255 with 128 in the centre, a channel is 0 to
// JET_PCM_CHANNEL_COUNT-1, and a channel number outside that range does
// nothing and does no harm.
//
// Complete sampled-SFX example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_pcm.h"
//
//   enum { SHOT_CHANNEL = 0, FULL_VOLUME = 255, CENTRE_PAN = 128 };
//   static JetPcmSample shot;
//
//   void _init(void) {
//       shot = jet_pcm_load_wav("sfx/shot.wav");
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A) && jet_pcm_sample_valid(&shot)) {
//           jet_pcm_play_sample(SHOT_CHANNEL, &shot, FULL_VOLUME, CENTRE_PAN);
//       }
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print("PRESS A TO PLAY SHOT.WAV", 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_pcm_stop_all();
//   }

#include "jet_core.h" // IWYU pragma: keep JetPcmChannel, JET_BASE, JET_PCM_* constants, bool/stdint

// A host-side test harness has no machine behind JET_BASE, so it stands each block in with a
// pointer object of its own (at a stub, or NULL for a host that plays nothing) and builds with
// JET_AUDIO_BLOCKS_STUBBED defined; the blocks are then those objects, declared here as the
// runtime declared them before the blocks left firmware. A cartridge never defines it.
#ifdef JET_AUDIO_BLOCKS_STUBBED
// <nocodex>
extern volatile uint8_t* PCM;
// </nocodex>
#else

/**
 * Constant: PCM
 * Summary: The PCM block: its header, then the eight JetPcmChannel descriptors.
 * Include: jet_pcm.h
 * Notes: The jet_pcm_* calls read and write it for you; it is this module's alone.
 * Example:
 *   uint32_t rate = jet_pcm_device_rate(); // reads PCM for you
 */
#define PCM ((volatile uint8_t*)(JET_BASE + JET_PCM_OFFSET))
#endif // JET_AUDIO_BLOCKS_STUBBED

/**
 * Enum: JetPcmFormat
 * Summary: How each frame of a sample is stored in memory, and whether it is mono or stereo.
 * Include: jet_pcm.h
 * Notes: A sample is a run of frames, one for each instant of sound, and the format says how big each number in a frame is, where silence sits, and how many numbers a frame holds. JET_PCM_S8 is a signed 8-bit number, from -128 to 127, with silence at 0. JET_PCM_S16 is a signed 16-bit number stored little-endian, low byte first, with silence at 0. JET_PCM_U8 is an unsigned 8-bit number, from 0 to 255, with silence at 128, which is what the WAV standard requires for 8-bit sound. JET_PCM_F32 is a 32-bit float, with silence at 0.0 and the loudest ordinary sound at -1.0 and 1.0; a little over is fine, and the host keeps a broken value, such as NaN, from ever reaching the speakers.
 *
 *        Those four are mono: one number a frame, which the channel's pan places between the speakers. The two stereo formats hold two numbers a frame, the left one first and then the right one: JET_PCM_S16_STEREO is two JET_PCM_S16 numbers and JET_PCM_F32_STEREO is two JET_PCM_F32 numbers. A stereo sound already knows where everything sits, so its pan works as a balance: at 128 both sides play just as they were recorded, and turning towards one end fades the other side out. Everything that counts frames counts whole frames, so a stereo frame of two numbers is still one frame. The format must match the bytes exactly, or what you hear is noise.
 * Example:
 *   JetPcmSample kick = jet_pcm_load("sfx/kick.pcm", JET_PCM_S8, 11025);
 */
typedef enum {
    JET_PCM_S8 = 0,         // signed 8-bit (two's complement), the Amiga's own format; silence is 0
    JET_PCM_S16 = 1,        // signed 16-bit, little-endian; silence is 0
    JET_PCM_U8 = 2,         // unsigned 8-bit, the WAV PCM8 and Audacity "8-bit" format; silence is 128
    JET_PCM_F32 = 3,        // 32-bit float, full scale -1.0 to 1.0; silence is 0.0
    JET_PCM_S16_STEREO = 4, // two JET_PCM_S16 numbers a frame, left then right
    JET_PCM_F32_STEREO = 5, // two JET_PCM_F32 numbers a frame, left then right
} JetPcmFormat;

/**
 * Enum: JetPcmInterp
 * Summary: How a channel fills in the gaps when a sample's rate is not the same as the sound device's rate.
 * Include: jet_pcm.h
 * Notes: A sample made at 11025 frames a second has to be stretched to the device's rate, such as 48000 a second, and the in-between values have to come from somewhere. JET_PCM_NEAREST repeats the nearest frame, and switches to the next one at the nearest device frame, which is a rough, crunchy sound. JET_PCM_LINEAR draws a straight line between the two nearest frames; it sounds smoother, it is the default, and it is normally the right choice for speech, music and modern recorded effects. JET_PCM_HOLD keeps each frame for exactly as long as the sample's own rate says, splitting a changeover that lands in the middle of a device frame across it instead of rounding it to one side: that is what an old sound chip's sample-and-hold actually put on the wire, so it is the faithful choice for a hardware port, and above the device's rate it is the only one of the three that does not alias. It is the same cost as JET_PCM_NEAREST. The choice belongs to the channel, not to one sound: every channel starts each run on JET_PCM_LINEAR, and once you choose another, every sound played on that channel uses it until you choose again.
 * Example:
 *   jet_pcm_set_interp(0, JET_PCM_HOLD); // crunchy retro playback, faithfully
 */
typedef enum {
    JET_PCM_LINEAR = 0,  // blend the two nearest frames (a 2-tap linear filter); smoother, and the default — zero, the value a channel powers on holding
    JET_PCM_NEAREST = 1, // repeat the nearest frame, changing over on the device's grid; rough
    JET_PCM_HOLD = 2,    // hold each frame for its own exact duration, as a sample-and-hold chip did
} JetPcmInterp;

// --- One-shot and looped sample playback ---

/**
 * Function: jet_pcm_play
 * Summary: Plays a sample once on a channel. The sample data MUST be in WRAM.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - data: The sample's frames, in WRAM, the cartridge's working memory, reserved with jet_memory_reserve.
 *   - frames: How long the sample is, in frames, one number of sound each.
 *   - fmt: How each frame is stored, as one of the JetPcmFormat values.
 *   - rate_hz: How many frames a second the sample was recorded or made at. The host stretches it to the device's rate for you.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo format it is the balance instead; see JetPcmFormat.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: A channel number outside the range does nothing and does no harm. data MUST be in WRAM, see THE ONE HARD RULE at the top of this header, or the channel stays silent.
 * Example:
 *   int8_t* s = jet_memory_reserve(256);
 *   jet_pcm_play(0, s, 256, JET_PCM_S8, 8287, 200, 128);
 */
void jet_pcm_play(uint8_t channel, const void* data, uint32_t frames, JetPcmFormat fmt,
                   uint32_t rate_hz, uint8_t vol, uint8_t pan);

/**
 * Function: jet_pcm_play_loop
 * Summary: Plays a sample on a channel, then repeats one part of it for ever.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - data: The sample's frames, in WRAM, the cartridge's working memory.
 *   - frames: How long the whole sample is, in frames, one number of sound each.
 *   - fmt: How each frame is stored, as one of the JetPcmFormat values.
 *   - rate_hz: How many frames a second the sample was recorded or made at.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo format it is the balance instead; see JetPcmFormat.
 *   - loop_start: The first frame of the part that repeats.
 *   - loop_len: How many frames repeat. 0 means no loop, and the call behaves exactly like jet_pcm_play.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: The channel plays the whole sample once, from frame 0 up to frames, then plays the part from loop_start for loop_len frames, over and over, until you stop it. A channel number outside the range does nothing and does no harm. data MUST be in WRAM.
 * Example:
 *   int8_t* saw = jet_memory_reserve(256);
 *   for (int i = 0; i < 256; ++i) saw[i] = (int8_t)(i - 128);
 *   jet_pcm_play_loop(0, saw, 256, JET_PCM_S8, 8287, 200, 128, 0, 256);
 */
void jet_pcm_play_loop(uint8_t channel, const void* data, uint32_t frames, JetPcmFormat fmt,
                        uint32_t rate_hz, uint8_t vol, uint8_t pan, uint32_t loop_start,
                        uint32_t loop_len);

// --- Sampled SFX: load and play a bundled sample file ---
//
// The easy way to play a recording in your cart: put a sample file in the
// cartridge's data folder and play it. Either loader reads the file straight
// into WRAM, where the host's sound thread can see it (the one hard rule), and
// gives you back a JetPcmSample that you keep for the whole run and play as
// often as you like. Pick the loader by what you have: jet_pcm_load for a raw
// .pcm file with no header, where you say the format and rate yourself, or
// jet_pcm_load_wav for a .wav, whose header says them for you.

/**
 * Struct: JetPcmSample
 * Summary: A sample that has been loaded into WRAM and is ready for the PCM channels to play, as often as you like.
 * Fields:
 *   - data: Where the frames are in WRAM, or NULL if the load failed.
 *   - frames: How many frames the sample has, one number of sound each.
 *   - fmt: How each frame is stored; see JetPcmFormat.
 *   - rate_hz: How many frames a second the recording plays at.
 * Include: jet_pcm.h
 * Notes: Load it once in _init and keep the struct for the whole run of the cart. WRAM, the cartridge's working memory, is handed out from the front and never given back, so there is no way to free a sample. A failed load leaves data as NULL, and that is safe to pass to jet_pcm_sample_valid or jet_pcm_play_sample. From a WAV file the loader fills in every field; for a raw file you give it fmt and rate_hz.
 * Example:
 *   static JetPcmSample explosion;
 *   void _init(void) { explosion = jet_pcm_load_wav("sfx/explosion.wav"); }
 *   void play_explosion(void) { jet_pcm_play_sample(0, &explosion, 255, 128); }
 */
typedef struct {
    void* data;          // WRAM pointer to the sample frames (NULL on failure)
    uint32_t frames;     // length in frames
    JetPcmFormat fmt;   // sample format
    uint32_t rate_hz;    // native sample rate
} JetPcmSample;

/**
 * Function: jet_pcm_load
 * Summary: Loads a raw sample file, one with no header, from the cartridge bundle straight into WRAM, ready to play.
 * Parameters:
 *   - rel_path: The file's path from inside the cartridge's data/ folder, such as "sfx/jump.pcm" for data/sfx/jump.pcm. Leave data/ off: the runtime adds it, exactly as jet_asset_open and jet_asset_read_to_wram do.
 *   - fmt: How each frame in the file is stored: JET_PCM_S8 (signed, the Amiga's own format), JET_PCM_U8 (unsigned, WAV 8-bit) JET_PCM_S16, or any other JetPcmFormat, including a stereo one for a file of left-then-right pairs.
 *   - rate_hz: How many frames a second the recording plays at, such as 11025 for a Paula-style 8-bit mono sample.
 * Returns: A JetPcmSample. Its data is NULL if the load failed, because the file is missing or WRAM is full.
 * Include: jet_pcm.h
 * Notes: CALL IT ONLY IN _init, because it reserves WRAM, which is handed out from the front and never given back. Load once, play many times. The bytes must match fmt exactly; a 16-bit file read as 8-bit is pure noise.
 *
 *        To make a raw file from a WAV, export it without its header first, for example with sox. sox in.wav -c 1 -b 16 -e signed-integer out.pcm makes a file to load as JET_PCM_S16; sox in.wav -c 1 -b 8 -e signed-integer out.pcm makes one for JET_PCM_S8; and sox in.wav -c 1 -b 8 -e unsigned-integer out.pcm makes one for JET_PCM_U8.
 * Example:
 *   JetPcmSample jump = jet_pcm_load("sfx/jump.pcm", JET_PCM_S8, 11025);
 *   if (jet_pcm_sample_valid(&jump)) jet_pcm_play_sample(2, &jump, 230, 128);
 */
JetPcmSample jet_pcm_load(const char* rel_path, JetPcmFormat fmt, uint32_t rate_hz);

/**
 * Function: jet_pcm_load_wav
 * Summary: Loads a .wav file from the cartridge bundle into WRAM, reading its header so the format and rate are found for you.
 * Parameters:
 *   - rel_path: The file's path from inside the cartridge's data/ folder, such as "sfx/jump.wav" for data/sfx/jump.wav. Leave data/ off: the runtime adds it, exactly as jet_asset_open and jet_asset_read_to_wram do.
 * Returns: A JetPcmSample with fmt, rate_hz and frames filled in from the WAV header. Its data is NULL if the load failed: the file is missing, WRAM is full, the file is not a RIFF WAVE file, or its encoding is one the loader does not read. Each failure logs its reason.
 * Include: jet_pcm.h
 * Notes: CALL IT ONLY IN _init, because it reserves WRAM, which is handed out from the front and never given back. Load once, play many times. This is the sibling of jet_pcm_load for when you know nothing about the file: you pass no format or rate, because the WAV header's "fmt " chunk carries them.
 *
 *        It reads plain uncompressed whole-number PCM only: 8-bit, which it reads as JET_PCM_U8 because that is what the WAV standard requires for 8-bit, or 16-bit, read as JET_PCM_S16; mono or stereo. A stereo file is mixed down to mono in place, so the two channels play added together on one PCM channel. Anything else, such as floating-point samples, ADPCM or any other compressed form, 24-bit or 32-bit samples, or more than two channels, is refused with a log warning rather than played as noise. Export it again as 16-bit PCM first, for example with sox in.wav -c 1 -b 16 -e signed-integer out.wav.
 * Example:
 *   JetPcmSample shot = jet_pcm_load_wav("sfx/shot.wav");
 *   if (jet_pcm_sample_valid(&shot)) jet_pcm_play_sample(2, &shot, 235, 128);
 */
JetPcmSample jet_pcm_load_wav(const char* rel_path);

/**
 * Function: jet_pcm_sample_valid
 * Summary: Tells you whether a sample loaded and can be played.
 * Parameters:
 *   - sample: The JetPcmSample that jet_pcm_load or jet_pcm_load_wav gave you.
 * Returns: true if the sample is in WRAM and has at least one frame; otherwise false.
 * Include: jet_pcm.h
 * Notes: Safe with NULL, and with a JetPcmSample that is all zeros or whose load failed. Check once after loading, so your cart can say that a file is missing or unreadable instead of staying silent when you try to play it.
 * Example:
 *   if (jet_pcm_sample_valid(&jump)) jet_pcm_play_sample(2, &jump, 230, 128);
 */
bool jet_pcm_sample_valid(const JetPcmSample* sample);

/**
 * Function: jet_pcm_play_sample
 * Summary: Plays a loaded sample once on a channel, at the format and rate it was loaded with.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - sample: A sample from jet_pcm_load or jet_pcm_load_wav.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo sample it is the balance instead; see JetPcmFormat.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: If the sample is not valid, because its load failed, nothing happens. It is a shortcut for jet_pcm_play with the sample's own fields filled in.
 * Example:
 *   jet_pcm_play_sample(2, &explosion, 235, 128);
 */
void jet_pcm_play_sample(uint8_t channel, const JetPcmSample* sample, uint8_t vol, uint8_t pan);

/**
 * Function: jet_pcm_play_sample_loop
 * Summary: Plays a loaded sample on a channel, then repeats one part of it for ever.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - sample: A sample from jet_pcm_load or jet_pcm_load_wav.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo sample it is the balance instead; see JetPcmFormat.
 *   - loop_start: The first frame of the part that repeats.
 *   - loop_len: How many frames repeat. 0 means no loop, and the sample plays once.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: The channel plays the whole sample once, then the part from loop_start for loop_len frames, over and over, until you stop it. If the sample is not valid, nothing happens. It is a shortcut for jet_pcm_play_loop with the sample's own fields filled in.
 * Example:
 *   jet_pcm_play_sample_loop(3, &engine, 200, 128, 0, engine.frames);
 */
void jet_pcm_play_sample_loop(uint8_t channel, const JetPcmSample* sample, uint8_t vol,
                               uint8_t pan, uint32_t loop_start, uint32_t loop_len);

// --- Streaming (double-buffered DMA, the chiptune-core / SID path) ---

/**
 * Function: jet_pcm_stream_open
 * Summary: Ties a ring buffer of yours, in WRAM, to a channel, so you can keep feeding it sound for as long as you like.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - ring: The ring buffer, in WRAM, with room for ring_frames frames. A ring buffer is a block of memory that you write into and the host reads out of, each wrapping back to the start when it reaches the end.
 *   - ring_frames: How many frames the ring holds.
 *   - fmt: How each frame is stored, as one of the JetPcmFormat values. A stereo format makes a stereo stream: write two numbers, left then right, for every frame.
 *   - rate_hz: How many frames a second you will generate. Use jet_pcm_device_rate for a 1:1 pass-through, with no stretching at all.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo format it is the balance instead; see JetPcmFormat.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: Open the stream once, then feed the ring every frame with jet_pcm_stream_write, or write into it yourself and call jet_pcm_stream_submit. Keep a buffer or two of sound queued ahead, checking with jet_pcm_stream_queued, so the channel never runs dry.
 * Example:
 *   int16_t* ring = jet_memory_reserve(4096 * sizeof(int16_t));
 *   jet_pcm_stream_open(0, ring, 4096, JET_PCM_S16, jet_pcm_device_rate(), 255, 128);
 */
void jet_pcm_stream_open(uint8_t channel, void* ring, uint32_t ring_frames, JetPcmFormat fmt,
                          uint32_t rate_hz, uint8_t vol, uint8_t pan);

/**
 * Function: jet_pcm_stream_submit
 * Summary: Tells the channel that you have written frames into the ring yourself, so it may play them. Use it when you index the ring by hand.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - frames: How many frames you have just written.
 * Returns: How many frames are now queued and waiting to be played: the write position minus the read position. Use it to decide whether to write more.
 * Include: jet_pcm.h
 * Notes: This is the path with no copying. Generate your sound straight into the ring at write_cursor % ring_frames, wrapping round the end yourself, then submit the count; the call moves the write position on. If you already have the frames in a scratch buffer, jet_pcm_stream_write copies them in and handles the wrap for you.
 * Example:
 *   // ... you filled n frames into the ring in place ...
 *   uint32_t queued = jet_pcm_stream_submit(0, n);
 */
uint32_t jet_pcm_stream_submit(uint8_t channel, uint32_t frames);

/**
 * Function: jet_pcm_stream_write
 * Summary: Copies frames from your buffer into the ring, wrapping round the end for you, and tells the channel to play them, all in one call.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - src: The frames to copy, from any memory your cart can read; a plain array in your code is fine.
 *   - frames: How many frames to copy. If the ring has less free room than that, only what fits is copied, so frames not yet played are never written over.
 * Returns: How many frames are queued and waiting to be played after the write.
 * Include: jet_pcm.h
 * Notes: This is the safe way to feed a stream. The number that comes back, or jet_pcm_stream_space, lets you keep a buffer or two queued so the channel neither runs dry nor writes over sound it has not played yet. A write bigger than the free room copies only what fits.
 * Example:
 *   int16_t block[256];
 *   generate(block, 256);
 *   jet_pcm_stream_write(0, block, 256); // cut to the free room automatically
 */
uint32_t jet_pcm_stream_write(uint8_t channel, const void* src, uint32_t frames);

/**
 * Function: jet_pcm_stream_queued
 * Summary: How many frames are queued in a channel's ring, still waiting to be played.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 * Returns: The number of frames written but not yet played: the write position minus the host's read position.
 * Include: jet_pcm.h
 * Notes: This is your gauge for keeping the stream fed. While it sits below the depth you want, write more, so playback never runs dry.
 * Example:
 *   while (jet_pcm_stream_queued(0) < target_frames) {
 *       generate(block, chunk);
 *       jet_pcm_stream_write(0, block, chunk);
 *   }
 */
uint32_t jet_pcm_stream_queued(uint8_t channel);

/**
 * Function: jet_pcm_stream_space
 * Summary: How many frames of free room a channel's ring has right now.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 * Returns: The number of frames you may write before you would write over sound that has not been played yet: the ring's capacity minus what is queued.
 * Include: jet_pcm.h
 * Notes: The other half of jet_pcm_stream_queued; the two always add up to the ring's capacity. Size a chunk of generated sound to this and you can write it in one go.
 * Example:
 *   uint32_t room = jet_pcm_stream_space(0);
 *   if (room > 0) jet_pcm_stream_write(0, block, room);
 */
uint32_t jet_pcm_stream_space(uint8_t channel);

// --- Live parameter rides (single-field, no retrigger) ---

/**
 * Function: jet_pcm_set_volume
 * Summary: Changes a channel's volume while it plays, without starting the sound again.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - vol: The new volume, from 0 to 255.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: It takes effect at once, whether the channel is playing a sample once, looping, or streaming. A channel number outside the range does nothing and does no harm.
 * Example:
 *   jet_pcm_set_volume(0, 128); // half volume
 */
void jet_pcm_set_volume(uint8_t channel, uint8_t vol);

/**
 * Function: jet_pcm_set_pan
 * Summary: Moves a channel between the speakers while it plays, without starting the sound again.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - pan: The new pan, from 0 to 255: 0 is hard left, 128 is the centre, and 255 is hard right. On a stereo sound the same register is the balance: 128 plays both sides as recorded, and each end fades the other side out.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: It takes effect at once and does not restart the sound. A channel number outside the range does nothing and does no harm.
 * Example:
 *   jet_pcm_set_pan(0, 0); // swing the channel hard left
 */
void jet_pcm_set_pan(uint8_t channel, uint8_t pan);

/**
 * Function: jet_pcm_set_rate
 * Summary: Changes the rate a channel plays its sound at, while it plays. This is a live pitch bend.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - rate_hz: The new rate, in frames a second. The host stretches the sound from this rate to the device's rate.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: A higher rate plays faster and higher in pitch; a lower rate plays slower and lower.
 * Example:
 *   jet_pcm_set_rate(0, 16000); // pitch the sample up
 */
void jet_pcm_set_rate(uint8_t channel, uint32_t rate_hz);

/**
 * Function: jet_pcm_set_interp
 * Summary: Chooses how a channel fills in the gaps between frames, for the sound it is playing and every sound played on it after.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 *   - interp: JET_PCM_NEAREST, which repeats the nearest frame and sounds rough; JET_PCM_LINEAR, which blends the two nearest frames, sounds smoother, and is the default; or JET_PCM_HOLD, which holds each frame for its own exact duration the way a sample-and-hold chip did.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: The choice belongs to the channel, so a channel that always wants the same one needs this once, in _init: playing, looping, streaming and stopping all leave it as it is, and it lasts until you choose again. Every channel starts each run on JET_PCM_LINEAR. It takes effect at once and does not restart the sound. A channel number outside the range does nothing and does no harm; a value that is not one of the three is read as JET_PCM_NEAREST.
 * Example:
 *   jet_pcm_set_interp(0, JET_PCM_HOLD); // the rough edges an old chip really made
 */
void jet_pcm_set_interp(uint8_t channel, JetPcmInterp interp);

// --- Common ---

/**
 * Function: jet_pcm_stop
 * Summary: Stops a channel with a short fade so it does not click, then leaves it idle.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: The host fades the channel to silence over about 5 ms before it goes idle, so even a sudden stop never clicks.
 * Example:
 *   jet_pcm_stop(0); // fade channel 0 to silence
 */
void jet_pcm_stop(uint8_t channel);

/**
 * Function: jet_pcm_stop_all
 * Summary: Stops every PCM channel, each with its short fade so nothing clicks.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_pcm.h
 * Notes: The one call that silences everything. Handy in _end, or when a scene changes.
 * Example:
 *   void _end(void) { jet_pcm_stop_all(); }
 */
void jet_pcm_stop_all(void);

/**
 * Function: jet_pcm_active
 * Summary: Tells you whether a channel is making sound right now.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1.
 * Returns: true while the channel is playing a sample, looping, or streaming; false once a one-shot sample has finished, after you have asked it to stop, or when it is idle.
 * Include: jet_pcm.h
 * Notes: A channel number outside the range gives false. A channel you have asked to stop counts as finished at once, even while the host is still fading it out over its last 5 ms.
 * Example:
 *   if (!jet_pcm_active(2)) jet_pcm_play_sample(2, &shot, 235, 128);
 */
bool jet_pcm_active(uint8_t channel);

/**
 * Function: jet_pcm_device_rate
 * Summary: The rate the host's sound device plays at, in frames a second (Hz), as the host has written it into the PCM block's header.
 * Parameters:
 *   - None.
 * Returns: The device rate, or 0 before the sound engine has reported it. A cart that generates its own sound and wants it played exactly as made should generate at this rate, so the host has nothing to stretch. Treat 0 as "pick a sensible rate yourself, such as 48000, and let the host stretch it".
 * Include: jet_pcm.h
 * Notes: Ask for this before you size a stream you generate live. The rate differs from one host to another, so do not write 44100 or 48000 into your code when exact 1:1 playback matters.
 * Example:
 *   uint32_t rate = jet_pcm_device_rate();
 *   if (rate == 0) rate = 48000;
 */
uint32_t jet_pcm_device_rate(void);

#endif // JET_PCM_H
