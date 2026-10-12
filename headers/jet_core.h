// runtime/jet_core.h
#ifndef JET_CORE_H
#define JET_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // IWYU pragma: keep (offsetof for ABI layout asserts)
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // IWYU pragma: keep
#include <math.h> // IWYU pragma: keep

// The bottom layer of the machine, in one header: the fixed-width number types
// (uint8_t, uint32_t and friends), where every block of the machine's memory
// sits, the live screen size, the raw input bytes, the records that describe a
// sound, and the Copper, a background colour for each row of the screen. You
// do not need to include this header yourself. Every cartridge gets it through
// jet64.h, and jet64.h is the header to include. Then use the everyday calls it
// brings in, such as jet_spr, jet_print and btn, rather than writing to the
// hardware blocks or calling the atomic helpers declared here. Those blocks are
// shared with the program that runs your cartridge, and each has its own rules
// about who may write which byte, and when. The everyday calls keep those rules
// for you.
//
// The parts of this header you can use straight away are the BTN_ names with
// btn, btnp and btnr; the JET_PLATFORM_ names; and the Copper calls at the
// bottom, which paint a true-colour background wherever the screen shows
// palette colour 0.
//
// Complete Copper-background example (copy into main.c):
//
//   #include "jet64.h"
//
//   void _init(void) {
//       jet_copper_enable(true);
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       const uint32_t midnight_blue = jet_rgb(8, 16, 48);
//       jet_copper_fill(midnight_blue);
//       jet_cls(0); // colour 0 lets the Copper background show through
//       jet_print("COPPER SKY", 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_copper_enable(false);
//   }

// TinyCC has the __atomic_* builtins used below, but unlike GCC and Clang it
// does not predefine the memory-order constants; it declares them in the
// standard <stdatomic.h>. Including that header on that one toolchain is the
// portable way to name __ATOMIC_SEQ_CST, and it is the runtime's entire
// allowance for TinyCC (docs/tcc_accelerator.md section 10).
#if defined(__TINYC__)
#include <stdatomic.h> // IWYU pragma: keep (TinyCC declares __ATOMIC_SEQ_CST here)
#endif

// The runtime's cross-process atomics, the audio and PCM register writers (torn-write brackets)
// and the IPC bridges' command bytes (ipc_internal.h), route through these helpers rather than raw
// `__atomic_*` builtins, so the sequentially-consistent contract lives in exactly one place. The
// reader halves are in `jet64_core::audio` (Rust) and the host IPC pumps; both agree on that
// contract, and `__atomic_thread_fence` here emits the hardware fences weakly-ordered CPUs such as
// Apple Silicon require. Defining them in this foundational header, reached first by every runtime
// translation unit via `jet64.h`, makes routing every runtime atomic through them a single,
// enforceable rule.
// Internal: Runtime implementation only; carts use the subsystem APIs built on these primitives.
static inline void jet_atomic_store_u8(volatile uint8_t* cell, uint8_t value) {
    __atomic_store_n(cell, value, __ATOMIC_SEQ_CST);
}
static inline uint8_t jet_atomic_load_u8(const volatile uint8_t* cell) {
    return __atomic_load_n(cell, __ATOMIC_SEQ_CST);
}
// The 32-bit twins, for the command channels whose ownership token is a word
// rather than a byte (the NET CMD slot `state`). Same contract, same one place.
// Internal: Runtime implementation only; carts use the subsystem APIs built on these primitives.
static inline void jet_atomic_store_u32(volatile uint32_t* cell, uint32_t value) {
    __atomic_store_n(cell, value, __ATOMIC_SEQ_CST);
}
static inline uint32_t jet_atomic_load_u32(const volatile uint32_t* cell) {
    return __atomic_load_n(cell, __ATOMIC_SEQ_CST);
}
static inline void jet_atomic_fence(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

// Your _end hook is quietly renamed _end_cart, because some linkers (MinGW, for one) reserve the name _end for themselves.
#define _end _end_cart
// Your _init hook is quietly renamed _init_cart for the same reason: every ELF linker (Linux,
// Android) takes a symbol named _init as the loader's DT_INIT, which would run it before the
// runtime has connected the cartridge to the machine.
#define _init _init_cart

#ifndef JET_DEFAULT_WIDTH
#define JET_DEFAULT_WIDTH 1280
#endif

#ifndef JET_DEFAULT_HEIGHT
#define JET_DEFAULT_HEIGHT 720
#endif

extern uint32_t JET_SCREEN_WIDTH;
extern uint32_t JET_SCREEN_HEIGHT;

// Cast to signed int on purpose, so that every bounds check in the runtime compares signed numbers and stays safe.
#ifdef JET_WIDTH
#undef JET_WIDTH
#endif
#define JET_WIDTH ((int)JET_SCREEN_WIDTH)

#ifdef JET_HEIGHT
#undef JET_HEIGHT
#endif
#define JET_HEIGHT ((int)JET_SCREEN_HEIGHT)

// The size in bytes of one screen buffer at the live resolution. The screen is
// 8 bits per pixel, so that is one byte per pixel. Both expand to the current
// screen size, and JET_SCREEN_WIDTH and JET_SCREEN_HEIGHT can change while the
// game runs, so these are NOT compile-time constants. Do not use them to size a
// fixed array.
#define JET_VRAM_SIZE (JET_WIDTH * JET_HEIGHT)
#define JET_VRAM_SIZE_U32 (JET_VRAM_SIZE / 4)

// Where each block of the machine's memory sits, as an offset from JET_BASE.
// These addresses are the hardware layout shared with jet64_core::hardware_map
// and the exported player; docs/architecture.md section 2 lays out the whole
// map and every value. Never renumber an offset. The host and the asset files
// on disk both depend on these exact addresses.
#define JET_SHM_SIZE       (112 * 1024 * 1024)  // Total virtual motherboard (112 MiB)
#define JET_ANIM_OFFSET    0x00010000  // Sprite animation defs (128 KiB region)
#define JET_ANIM_FRAMES_OFFSET 0x00030000  // JetAnimFrame records for TABLE animations (512 KiB region)
#define JET_SPRITES_OFFSET 0x00100000  // Sprite color banks, 8bpp (8 MiB)
#define JET_MASKS_OFFSET   0x00900000  // Sprite transparency masks, 1bpp (1 MiB)
#define JET_SFX_OFFSET     0x00A00000  // SFX patch/macro/wavetable/curve banks
#define JET_BGM_CMD_OFFSET   0x00A80000  // BGM command IPC bridge (256 B)
#define JET_BGM_STATE_OFFSET 0x00A80100  // Live 32-channel BGM state (128 B)
#define JET_BGM_TRACKER_OFFSET 0x00A80180  // Tracker pattern window (32 KiB)
#define JET_BGM_TRANSPORT_OFFSET 0x00A841C0 // The cart's song playing (64 B, in the tracker region's tail)
#define JET_PAL_CMD_OFFSET     0x00A88180  // Palette-load IPC bridge (256 B)
#define JET_PALETTE_OFFSET     0x00A88280  // Live 256-entry RGBA LUT (1 KiB)
#define JET_MAP_CMD_OFFSET     0x00A88680  // Map-injection IPC bridge (256 B)
#define JET_ASSET_CMD_OFFSET   0x00A88780  // Generic-asset IPC bridge (256 B)
#define JET_HOST_CMD_OFFSET    0x00A88880  // Host-command IPC bridge (256 B)
#define JET_IPC_CMD_BLOCK_BYTES 256        // Every 256-byte IPC command bridge (BGM/PAL/MAP/ASSET/HOST)
#define JET_NET_CMD_OFFSET     (JET_HOST_CMD_OFFSET + JET_IPC_CMD_BLOCK_BYTES) // 0x00A88980, host-mediated networking (32 KiB)
#define JET_IO_OFFSET      0x00B00000  // Legacy P1 bitmask + legacy mouse mirror (16 B)
#define JET_AUDIO_OFFSET   0x00B00010  // 16 live synth registers, 64 B each (1 KiB)
#define JET_SYS_OFFSET     0x00B00410  // System-state registers (128 B)
#define JET_KBD_OFFSET     0x00B00490  // Key states + typed-char ring (512 B)
#define JET_FONT_OFFSET    0x00C00000  // Font ROM rasterization cache (4 MiB)
#define JET_VRAM_OFFSET    0x01000000  // Triple-buffered video memory (32 MiB)
#define JET_WRAM_OFFSET    0x03000000  // User working RAM (64 MiB)

// The alignment every motherboard base address has, whatever backs it: a shared
// segment is page-aligned, and every host allocates a heap motherboard to it.
// The runtime stores through typed pointers into the map (SYS's uint32_t
// registers, the float master volumes, a cart's own doubles and pointers in
// WRAM), and each offset is aligned only relative to the base. Mirrors
// jet64_core::hardware_map::JET_SHM_ALIGN_BYTES, which explains the value.
#define JET_SHM_ALIGN_BYTES 16
_Static_assert((JET_SHM_ALIGN_BYTES & (JET_SHM_ALIGN_BYTES - 1)) == 0,
               "the motherboard alignment must be a power of two");
_Static_assert(JET_SHM_ALIGN_BYTES % _Alignof(uint64_t) == 0,
               "an aligned motherboard base must serve a uint64_t store");
_Static_assert(JET_SHM_ALIGN_BYTES % _Alignof(double) == 0,
               "an aligned motherboard base must serve a double store");
_Static_assert(JET_SHM_ALIGN_BYTES % _Alignof(void*) == 0,
               "an aligned motherboard base must serve a pointer store");
_Static_assert(JET_WRAM_OFFSET % JET_SHM_ALIGN_BYTES == 0,
               "WRAM must start on the motherboard alignment, so its reservations stay aligned");

// The shape of the video memory region: 32 MiB shared by up to three screen
// buffers. Triple-buffered 4K UHD needs 3 x 7.91 MiB = 23.7 MiB, which leaves
// headroom.
#define JET_VRAM_REGION_BYTES (JET_WRAM_OFFSET - JET_VRAM_OFFSET)
#define JET_VRAM_BUFFER_COUNT 3

// The Copper block: one background colour for each row of the screen (see
// docs/copper.md). Mirrors jet64_core::hardware_map. It lives in the reserved
// gap before the Font ROM.
#define JET_COPPER_OFFSET       0x00B007C0  // Right after the AUDIO MASTER register
#define JET_COPPER_HEADER_BYTES 64          // Enable byte + reserved (keeps the table 64-aligned)
#define JET_COPPER_ENABLE_OFFSET 0          // u8 within the block: 0 = off, nonzero = active
#define JET_COPPER_MAX_LINES    4096        // Covers 4K UHD in either orientation
#define JET_COPPER_ENTRY_BYTES  4           // u32 LE 0x00RRGGBB (24-bit truecolour) per scanline
#define JET_COPPER_TABLE_OFFSET JET_COPPER_HEADER_BYTES  // Table start *within* the block (after the header)

// How a Copper colour word is laid out. This is the ONE place the channel
// layout is named, so no packer, generator or unpacker below ever carries a
// bare shift or mask of its own.
#define JET_COPPER_CHANNEL_BITS 8    // Bits per colour channel
#define JET_COPPER_CHANNEL_MAX  ((1 << JET_COPPER_CHANNEL_BITS) - 1) // 255
#define JET_COPPER_B_SHIFT      0
#define JET_COPPER_G_SHIFT      (JET_COPPER_B_SHIFT + JET_COPPER_CHANNEL_BITS) // 8
#define JET_COPPER_R_SHIFT      (JET_COPPER_G_SHIFT + JET_COPPER_CHANNEL_BITS) // 16
// The 24 colour bits. The top byte of every entry is RESERVED and is always
// written as zero; it is where a future per-line force/overlay flag will land
// (docs/copper.md section 16).
#define JET_COPPER_RGB_MASK     0x00FFFFFFu

// ===== PCM channel control block (Amiga Paula-style; see docs/audio/design.md) =====
// A small fixed table of channel DESCRIPTORS at the start of the reserved gap
// after the Copper block. The sound bytes themselves live in WRAM, the
// cartridge's working memory; a descriptor points at them by their offset from
// JET_BASE, the start of the machine's memory, and the host checks every such
// offset against the bounds before it touches a byte. Mirrors
// jet64_core::hardware_map and audio::pcm. The calls a cartridge uses are in
// the opt-in header runtime/jet_pcm.h.
#define JET_PCM_OFFSET            0x00B04800  // Right after the Copper block
#define JET_PCM_HEADER_BYTES      64          // device_rate + count + reserved (keeps the table 64-aligned)
#define JET_PCM_DEVICE_RATE_OFFSET   0        // u32 LE within the block, host-written
#define JET_PCM_CHANNEL_COUNT_OFFSET 4        // u8 within the block, informational mirror
#define JET_PCM_CHANNEL_COUNT     8           // dedicated PCM channels (Paula's 4, doubled)
#define JET_PCM_CHANNEL_BYTES     64          // one descriptor stride (cursors stay 8-aligned)
#define JET_PCM_TABLE_OFFSET      JET_PCM_HEADER_BYTES  // descriptor table start *within* the block
#define JET_PCM_BLOCK_BYTES       (JET_PCM_HEADER_BYTES + JET_PCM_CHANNEL_COUNT * JET_PCM_CHANNEL_BYTES)

// The values a descriptor's mode byte can take (mirror PCM_MODE_* in hardware_map.rs).
#define JET_PCM_MODE_IDLE    0
#define JET_PCM_MODE_ONESHOT 1
#define JET_PCM_MODE_LOOP    2
#define JET_PCM_MODE_STREAM  3

// The bits of a descriptor's flags byte.
#define JET_PCM_FLAG_STOP_REQUESTED 0x01u

/**
 * Struct: JetPcmChannel
 * Summary: The 64-byte record that controls one of the machine's eight PCM channels, the channels that play recorded sound. It sits in memory shared with the host, and an ordinary cartridge drives it through the calls in jet_pcm.h rather than by hand.
 * Fields:
 *   - seq: The lock byte. While it is odd the cartridge is in the middle of rewriting the record; while it is even the record is complete and safe to read.
 *   - trigger: A count that goes up by one each time new playback starts on this channel. The host watches it for a change.
 *   - mode: What the channel is doing: JET_PCM_MODE_IDLE, JET_PCM_MODE_ONESHOT, JET_PCM_MODE_LOOP or JET_PCM_MODE_STREAM.
 *   - format: How each frame is stored, and whether it is mono or stereo, as one of the JetPcmFormat values in jet_pcm.h.
 *   - volume: How loud the channel is, from 0 (silent) to 255 (full). You may change it while the sound plays.
 *   - pan: Where the sound sits between the speakers, from 0 (all left) through 128 (centre) to 255 (all right). For a stereo format it is the balance instead: 128 plays both sides as recorded. You may change it while the sound plays.
 *   - interp: How in-between samples are made when the sound is resampled, as one of the JetPcmInterp values in jet_pcm.h. It belongs to the channel rather than to one sound: starting a sound leaves it as it was, and 0, the value every run starts with, is JET_PCM_LINEAR. You may change it while the sound plays.
 *   - flags: Control bits, made of the JET_PCM_FLAG_ names added together.
 *   - rate_hz: How fast the sound was recorded, in frames per second. A frame is one sample for every channel. You may change it while the sound plays.
 *   - data_offset: Where the sound's bytes are, as an offset from JET_BASE, the start of the machine's memory. The bytes live in WRAM, the cartridge's working memory.
 *   - data_frames: For a one-shot or a loop, the length of the sound in frames. For a stream, the size of the ring buffer in frames.
 *   - loop_start: The frame the loop starts from.
 *   - loop_len: How many frames the loop covers. 0 means the sound does not loop.
 *   - _pad: Padding that keeps the two cursors on an 8-byte boundary. Do not change it.
 *   - write_cursor: How many stream frames the cartridge has handed over in total. It only ever goes up. The cartridge owns it.
 *   - read_cursor: How many stream frames the host has played in total. It only ever goes up. The host owns it.
 *   - reserved: Spare bytes. Keep them zero.
 * Include: jet_pcm.h
 * Notes: The record IS the hardware: reading or writing a field touches the shared memory straight away, and each field has a strict rule about who may write it. Do not copy the record, and do not write it in one bulk store yourself. Start a sound with jet_pcm_play, jet_pcm_play_loop or jet_pcm_stream_open, which set the seq byte odd, write the fields, and set it even again, in the right order; change a field the notes above say may change while the sound plays with its jet_pcm_set_ helper, which is one plain store the host reads whole.
 * Example:
 *   volatile JetPcmChannel* channel =
 *       (volatile JetPcmChannel*)(PCM + JET_PCM_TABLE_OFFSET);
 *   uint8_t current_volume = channel->volume;
 */
typedef struct {
    uint8_t  seq;          // +0  lock byte: odd = rewrite in progress
    uint8_t  trigger;      // +1  goes up when new playback starts; host watches for change
    uint8_t  mode;         // +2  JET_PCM_MODE_*
    uint8_t  format;       // +3  JetPcmFormat
    uint8_t  volume;       // +4  0..255 (may change while playing)
    uint8_t  pan;          // +5  0..255, 128 = centre (may change while playing)
    uint8_t  interp;       // +6  JetPcmInterp, the channel's own: kept across plays (may change while playing)
    uint8_t  flags;        // +7  JET_PCM_FLAG_* (bit0 = stop requested)
    uint32_t rate_hz;      // +8  source sample rate (may change while playing)
    uint32_t data_offset;  // +12 where the sound bytes are in WRAM, as an offset from JET_BASE
    uint32_t data_frames;  // +16 one-shot/loop: length; stream: ring size (frames)
    uint32_t loop_start;   // +20 frames
    uint32_t loop_len;     // +24 frames (0 = no loop)
    uint32_t _pad;         // +28 keeps the cursors 8-byte aligned
    uint64_t write_cursor; // +32 CART-owned: total frames handed over (only goes up)
    uint64_t read_cursor;  // +40 HOST-owned: total frames played (only goes up)
    uint8_t  reserved[16]; // +48 to 64
} JetPcmChannel;

// ===== AUDIO SCOPE block (a tap on the mixed master bus; see docs/jetamp/design.md section 2) =====
// A ring buffer, written by the HOST, holding the final mixed sound: the
// machine's oscilloscope tap. On every audio buffer, in all three runtimes,
// the mixer publishes stereo frames here as pairs of f32 numbers (left, right),
// taken at the same point the Mixing Station's analyser listens to, after the
// EQ and before the final clamp. Only the host writes here, and the head only
// ever goes up (the same model as the KEYBOARD block): the host owns every
// byte, and a reader copies the newest N frames backwards from write_head, then
// checks the head again to find out whether the host overwrote any of them
// mid-copy. Mirrors jet64_core::hardware_map. The calls a cartridge uses to
// read it are in the opt-in header runtime/jet_scope.h.
#define JET_SCOPE_OFFSET       (JET_PCM_OFFSET + JET_PCM_BLOCK_BYTES) // 0x00B04A40, right after the PCM block
#define JET_SCOPE_HEADER_BYTES 64    // keeps the ring 64-aligned (frames land 8-aligned)
#define JET_SCOPE_MAGIC        0x50435346u // 'F','S','C','P' little-endian, host-stamped
#define JET_SCOPE_VERSION      1
#define JET_SCOPE_CHANNELS     2     // interleaved stereo (L,R); fixed for version 1
#define JET_SCOPE_FRAMES       8192  // power of two: ~171 ms @ 48 kHz; readers mask, never divide
#define JET_SCOPE_FRAME_MASK   (JET_SCOPE_FRAMES - 1)
#define JET_SCOPE_RING_OFFSET  JET_SCOPE_HEADER_BYTES // ring start *within* the block
#define JET_SCOPE_BLOCK_BYTES  (JET_SCOPE_HEADER_BYTES + JET_SCOPE_FRAMES * JET_SCOPE_CHANNELS * 4)

// The bits of the header's flags word. LATENCY and ROW_ANCHOR are the E1 and
// E2 host extensions (docs/pulsar/design.md section 10, D6): fields added by
// carving them out of the reserved tail, each behind a flag, so that version
// stays 1 and every reader already shipped keeps working.
#define JET_SCOPE_FLAG_TAP_ACTIVE 0x01u
#define JET_SCOPE_FLAG_LATENCY    0x02u // output_latency_frames is being published
#define JET_SCOPE_FLAG_ROW_ANCHOR 0x04u // the bgm_row_* anchor is being published

/**
 * Struct: JetScopeHeader
 * Summary: The read-only header at the front of the audio scope, the ring buffer where the host publishes the final mixed sound. The calls in jet_scope.h read it for you.
 * Fields:
 *   - magic: Equal to JET_SCOPE_MAGIC when the host running your cartridge has the scope tap. On an older host it is not.
 *   - version: The layout version of this block, currently JET_SCOPE_VERSION.
 *   - channels: How many channels are interleaved in each frame, currently 2 for stereo.
 *   - sample_rate: The rate of the host's audio device, in frames per second.
 *   - flags: Status bits, made of the JET_SCOPE_FLAG_ names added together.
 *   - write_head: How many stereo frames the host has published in total. It only ever goes up.
 *   - output_latency_frames: The host's honest estimate of the delay from the mixer to the sound output (the DAC), in scope frames; 0 means it does not know. Only meaningful when JET_SCOPE_FLAG_LATENCY is set in flags.
 *   - bgm_row_head: The value write_head had at the instant the music engine last committed a pattern row. Only meaningful when JET_SCOPE_FLAG_ROW_ANCHOR is set in flags.
 *   - bgm_row_pattern: The position in the song order of that committed row.
 *   - bgm_row_line: The row's place within its pattern.
 *   - reserved: Spare bytes. The host owns them.
 * Include: jet_scope.h
 * Notes: The host owns every byte. Never write to this record, and never keep a copy of it or hold on to the pointer; use jet_scope_available and jet_scope_read, which check the header and copy a stable window of frames safely.
 *
 *        The row anchor names its own row, and the host publishes the rows first and the head last. So read it in the order head, rows, head, and try again if the head changed in between. jet_scope_row_anchor does this for you.
 * Example:
 *   const volatile JetScopeHeader* header =
 *       (const volatile JetScopeHeader*)SCOPE;
 *   uint32_t device_rate = header->sample_rate;
 */
typedef struct {
    uint32_t magic;        // +0  JET_SCOPE_MAGIC; absent on a host too old for the tap
    uint16_t version;      // +4  JET_SCOPE_VERSION
    uint16_t channels;     // +6  JET_SCOPE_CHANNELS
    uint32_t sample_rate;  // +8  device rate in Hz (44100, 48000, ...), stamped before the first tick
    uint32_t flags;        // +12 JET_SCOPE_FLAG_*
    uint32_t write_head;   // +16 total frame count, only goes up, wraps at 2^32; frame n
                           //     lives at ring[n & JET_SCOPE_FRAME_MASK]
    // E1 (flag LATENCY): the sink's own buffer depth plus the device/context
    // report where the backend offers one, in scope frames; 0 = unknown.
    uint32_t output_latency_frames; // +20
    // E2 (flag ROW_ANCHOR): ties chart time to ring time, sample-exactly. The
    // host stores the row identity first and the head last, once per row
    // commit (fewer than ten times a second), never in the per-frame mix loop.
    uint32_t bgm_row_head;    // +24 write_head at the last BGM row commit
    uint8_t  bgm_row_pattern; // +28 song-order position of that row
    uint8_t  bgm_row_line;    // +29 row within its pattern
    uint8_t  reserved[34];    // +30 to 64
} JetScopeHeader;

// ===== AUDIO EQ block (the cartridge's control of the master EQ; docs/jetamp/design.md section 5.6) =====
// The cartridge's side of the host's 10-band master graphic EQ, the tone
// control that boosts or cuts ten bands of the sound (peaking biquad filters
// at the ISO octave centres, 31 Hz to 16 kHz). The CARTRIDGE owns the block:
// it writes with plain aligned stores, and the host's mixer reads them once
// per audio buffer, the same pattern as the AUDIO MASTER volume register, only
// bigger. All zeros (mode INHERIT) leaves the EQ the project authored in
// charge, so a cartridge that never touches the block hears no change. Every
// host zeroes the block each time it starts a cartridge. Mirrors
// jet64_core::hardware_map AUDIO_EQ_*; the calls a cartridge uses are
// jet_host_eq_* in jet_audio.h.
#define JET_AUDIO_EQ_OFFSET       (JET_SCOPE_OFFSET + JET_SCOPE_BLOCK_BYTES) // 0x00B14A80, right after the SCOPE block
#define JET_AUDIO_EQ_BLOCK_BYTES  64   // mode + 10 bands, reserved tail
#define JET_AUDIO_EQ_BAND_COUNT   10
#define JET_AUDIO_EQ_BAND_DB_MAX  12.0f // the host clamps each band's gain to plus or minus this when it reads

/**
 * Constant: JET_AUDIO_EQ_INHERIT
 * Summary: Leaves the master EQ curve the project authored in charge, so the cartridge changes nothing.
 * Include: jet64.h
 * Notes: Give it to jet_host_eq_set_mode. It is the state every cartridge boots in, since the block starts as all zeros, and it is the right choice whenever your cartridge has no reason to override the project's mix.
 * Example:
 *   jet_host_eq_set_mode(JET_AUDIO_EQ_INHERIT);
 */
#define JET_AUDIO_EQ_INHERIT 0u
/**
 * Constant: JET_AUDIO_EQ_CART
 * Summary: Uses the ten band gains your cartridge sets with jet_host_eq_set_band as the master EQ.
 * Include: jet64.h
 * Notes: Give it to jet_host_eq_set_mode before you set any band with jet_host_eq_set_band.
 * Example:
 *   jet_host_eq_set_mode(JET_AUDIO_EQ_CART);
 */
#define JET_AUDIO_EQ_CART    1u
/**
 * Constant: JET_AUDIO_EQ_BYPASS
 * Summary: Switches the master graphic EQ off, so neither the project's band gains nor the cartridge's apply.
 * Include: jet64.h
 * Notes: Give it to jet_host_eq_set_mode when you want the master path deliberately flat. Use JET_AUDIO_EQ_INHERIT to hand control back to the project.
 * Example:
 *   jet_host_eq_set_mode(JET_AUDIO_EQ_BYPASS);
 */
#define JET_AUDIO_EQ_BYPASS  2u

typedef struct {
    uint32_t mode;                                 // +0  JET_AUDIO_EQ_*
    float    bands_db[JET_AUDIO_EQ_BAND_COUNT];   // +4  gains in dB, plus or minus JET_AUDIO_EQ_BAND_DB_MAX
    uint8_t  reserved[20];                         // +44 to 64
} JetAudioEqBlock;
_Static_assert(sizeof(JetAudioEqBlock) == JET_AUDIO_EQ_BLOCK_BYTES,
               "AUDIO EQ block ABI (Rust mirror contract)");

// ===== PUBLISH block (the cartridge's publishing record; docs/publish_record.md) =====
// The cartridge's own public identity, as the Workstation's Project Publishing tab authors
// it in data/publish.ron: its name, publisher, version, copyright line and description, and
// whether Enable Publishing is on. The HOST writes the block, before the cartridge's first
// instruction on every runtime and again, live, when the Workstation's Publishing tab saves,
// so a cartridge learns its own version without reading or parsing a file.
//
// The host is the only writer, under a generation seqlock: it makes the generation odd,
// writes every other byte, then makes the generation even again. A reader that finds the
// same even generation before and after its copy has copied one whole record. A generation
// of 0 with no magic is a block no host has stamped. Every text field is NUL-terminated and
// NUL-padded UTF-8, cut by the host at a character boundary, so its last byte is always NUL.
// Mirrors jet64_core::hardware_map PUBLISH_*; the call a cartridge uses is
// jet_publish_record() in jet_publish.h.
#define JET_PUBLISH_OFFSET       (JET_AUDIO_EQ_OFFSET + JET_AUDIO_EQ_BLOCK_BYTES) // 0x00B14AC0, right after the AUDIO EQ block
#define JET_PUBLISH_BLOCK_BYTES  2048
#define JET_PUBLISH_HEADER_BYTES 64
#define JET_PUBLISH_MAGIC        0x3436504Au // 'J','P','6','4' little-endian, host-stamped
#define JET_PUBLISH_LAYOUT_VERSION 1         // the block's layout, not the cartridge's version

// The header, host-written (offsets within the block).
#define JET_PUBLISH_MAGIC_OFFSET          0  // u32, JET_PUBLISH_MAGIC once a record is published
#define JET_PUBLISH_LAYOUT_VERSION_OFFSET 4  // u16, JET_PUBLISH_LAYOUT_VERSION
#define JET_PUBLISH_GENERATION_OFFSET     8  // u32 seqlock word: odd while the host writes
#define JET_PUBLISH_FLAGS_OFFSET          12 // u32, JET_PUBLISH_FLAG_*
#define JET_PUBLISH_MAJOR_OFFSET          16 // u32, the version's three numbers, parsed by the host
#define JET_PUBLISH_MINOR_OFFSET          20 // u32
#define JET_PUBLISH_PATCH_OFFSET          24 // u32

// The bits of the header's flags word.
#define JET_PUBLISH_FLAG_PUBLISHED 0x01u // the Publishing tab's Enable Publishing switch

// The text fields, each its width in bytes with the NUL included, laid end to end after
// the header. Each offset is derived from the field before it, never written twice.
#define JET_PUBLISH_APP_NAME_BYTES    128
#define JET_PUBLISH_PUBLISHER_BYTES   128
#define JET_PUBLISH_VERSION_BYTES     32   // the version as authored, "1.0.3"
#define JET_PUBLISH_COPYRIGHT_BYTES   256
#define JET_PUBLISH_DESCRIPTION_BYTES 1024
#define JET_PUBLISH_APP_NAME_OFFSET    JET_PUBLISH_HEADER_BYTES
#define JET_PUBLISH_PUBLISHER_OFFSET   (JET_PUBLISH_APP_NAME_OFFSET + JET_PUBLISH_APP_NAME_BYTES)
#define JET_PUBLISH_VERSION_OFFSET     (JET_PUBLISH_PUBLISHER_OFFSET + JET_PUBLISH_PUBLISHER_BYTES)
#define JET_PUBLISH_COPYRIGHT_OFFSET   (JET_PUBLISH_VERSION_OFFSET + JET_PUBLISH_VERSION_BYTES)
#define JET_PUBLISH_DESCRIPTION_OFFSET (JET_PUBLISH_COPYRIGHT_OFFSET + JET_PUBLISH_COPYRIGHT_BYTES)
#define JET_PUBLISH_FIELDS_END         (JET_PUBLISH_DESCRIPTION_OFFSET + JET_PUBLISH_DESCRIPTION_BYTES)

_Static_assert(JET_PUBLISH_OFFSET == 0x00B14AC0,
               "PUBLISH block moved — jet64_core::hardware_map::JET_PUBLISH_OFFSET must match");
_Static_assert((JET_PUBLISH_OFFSET + JET_PUBLISH_GENERATION_OFFSET) % _Alignof(uint32_t) == 0,
               "the PUBLISH generation word must be aligned for the atomic loads that read it");
_Static_assert(JET_PUBLISH_PATCH_OFFSET + sizeof(uint32_t) <= JET_PUBLISH_HEADER_BYTES,
               "the PUBLISH header overflows into its text fields");
_Static_assert(JET_PUBLISH_FIELDS_END <= JET_PUBLISH_BLOCK_BYTES,
               "the PUBLISH text fields overflow the block");
_Static_assert(JET_PUBLISH_OFFSET + JET_PUBLISH_BLOCK_BYTES <= JET_FONT_OFFSET,
               "PUBLISH block overflows into the Font ROM");

// ===== SHADER block (the cartridge's screen shader: its numbers and switches; docs/cart_shader.md section 6.1) =====
// The cartridge's half of its own screen shader (data/shader/screen.glsl): thirty-two numbers
// the shader reads as jet_param, and one bit per effect the cartridge has switched off; and the
// host's answer beside them, whether the stage is on screen and which effects this host
// compiled in. jet_shader.h writes and reads it with plain stores, so a cartridge that never
// includes that header never touches it. The switches are OFF bits: the zeroed block of a cold
// boot runs everything the creator ticked. A Warm Boot keeps the block; a cold boot zeroes it.
// Mirrors jet64_core::hardware_map SHADER_*; the calls a cartridge uses are in jet_shader.h.
#define JET_SHADER_OFFSET (JET_PUBLISH_OFFSET + JET_PUBLISH_BLOCK_BYTES) // 0x00B152C0, right after the PUBLISH block
#define JET_SHADER_BLOCK_BYTES 144
#define JET_SHADER_BLOCK_ALIGN 16 // the numbers are read as vec4s, so the block starts on one

// The header (offsets within the block). The host reads all of it beside every frame.
#define JET_SHADER_EFFECTS_OFF_OFFSET 0 // u8, cartridge-written: one bit per JetShaderEffect switched off
#define JET_SHADER_STATUS_OFFSET 1      // u8, host-written: JetShaderStatus
#define JET_SHADER_AVAILABLE_OFFSET 2   // u8, host-written: one bit per JetShaderEffect compiled in
#define JET_SHADER_HEADER_BYTES 16      // the rest of the header is reserved, zero

// The numbers, float little-endian: number i is jet_param[i / 4], component i % 4.
#define JET_SHADER_PARAMS_OFFSET JET_SHADER_HEADER_BYTES
#define JET_SHADER_PARAM_COUNT 32

_Static_assert(JET_SHADER_OFFSET == 0x00B152C0,
               "SHADER block moved — jet64_core::hardware_map::JET_SHADER_OFFSET must match");
_Static_assert(JET_SHADER_OFFSET % JET_SHADER_BLOCK_ALIGN == 0,
               "the SHADER block must start on a vec4");
_Static_assert(JET_SHADER_AVAILABLE_OFFSET < JET_SHADER_HEADER_BYTES,
               "the SHADER header's fields overflow into its numbers");
_Static_assert(JET_SHADER_PARAMS_OFFSET + JET_SHADER_PARAM_COUNT * sizeof(float) == JET_SHADER_BLOCK_BYTES,
               "the SHADER numbers must fill the block exactly");
_Static_assert(JET_SHADER_OFFSET + JET_SHADER_BLOCK_BYTES <= JET_FONT_OFFSET,
               "SHADER block overflows into the Font ROM");

// ===== SHADER COLORMAP block (the light table the screen shader reads; docs/cart_shader.md section 6.5) =====
// The table jet_triblit_lit shades through, rows[level * 256 + index], copied in by
// jet_shader_colormap with the generation advanced, so a host uploads it only when it moves.
#define JET_SHADER_COLORMAP_OFFSET (JET_SHADER_OFFSET + JET_SHADER_BLOCK_BYTES) // 0x00B15350
#define JET_SHADER_COLORMAP_GENERATION_OFFSET 0 // u32, cartridge-written: advanced by each call
#define JET_SHADER_COLORMAP_LEVELS_OFFSET 4     // u32, cartridge-written: how many rows
#define JET_SHADER_COLORMAP_HEADER_BYTES 16     // the rest of the header is reserved, zero
#define JET_SHADER_COLORMAP_MAX_LEVELS 32       // Doom's own count of light levels
#define JET_SHADER_COLORMAP_ROW_BYTES 256       // one entry per palette index
#define JET_SHADER_COLORMAP_BYTES 8208          // the header and thirty-two rows of 256

_Static_assert(JET_SHADER_COLORMAP_OFFSET == 0x00B15350,
               "SHADER COLORMAP block moved — jet64_core::hardware_map must match");
_Static_assert(JET_SHADER_COLORMAP_HEADER_BYTES + JET_SHADER_COLORMAP_MAX_LEVELS * JET_SHADER_COLORMAP_ROW_BYTES == JET_SHADER_COLORMAP_BYTES,
               "SHADER COLORMAP block: the header and the rows are the block");
_Static_assert(JET_SHADER_COLORMAP_OFFSET + JET_SHADER_COLORMAP_BYTES <= JET_FONT_OFFSET,
               "SHADER COLORMAP block overflows into the Font ROM");

// ===== SHADER FRAMES block (the SHADER block each frame was drawn with; docs/cart_shader.md section 6.7) =====
// One copy of the SHADER block per VRAM buffer, stamped by the guest's tick loop (jet64.c) as it
// marks that buffer ready. A cartridge writes its numbers whenever it likes, so a host whose guest
// runs in another process, and may already be inside the next tick when it takes a frame, presents
// each frame with its own buffer's copy. Cartridges never touch it. Mirrors jet64_core::hardware_map.
#define JET_SHADER_FRAMES_OFFSET (JET_SHADER_COLORMAP_OFFSET + JET_SHADER_COLORMAP_BYTES) // 0x00B17360
#define JET_SHADER_FRAME_SLOTS JET_VRAM_BUFFER_COUNT          // one slot per VRAM buffer
#define JET_SHADER_FRAME_SLOT_BYTES JET_SHADER_BLOCK_BYTES    // each a whole SHADER block
#define JET_SHADER_FRAMES_BYTES (JET_SHADER_FRAME_SLOTS * JET_SHADER_FRAME_SLOT_BYTES)

_Static_assert(JET_SHADER_FRAMES_OFFSET == 0x00B17360,
               "SHADER FRAMES block moved — jet64_core::hardware_map must match");
_Static_assert(JET_SHADER_FRAMES_OFFSET % JET_SHADER_BLOCK_ALIGN == 0 &&
                   JET_SHADER_FRAME_SLOT_BYTES % JET_SHADER_BLOCK_ALIGN == 0,
               "every SHADER FRAMES slot must start on a vec4, as the block it copies does");
_Static_assert(JET_SHADER_FRAMES_OFFSET + JET_SHADER_FRAMES_BYTES <= JET_FONT_OFFSET,
               "SHADER FRAMES block overflows into the Font ROM");

// ===== SHADER COLORMAP FRAMES block (the light table each frame was drawn with; docs/cart_shader.md section 6.7) =====
// One copy of the SHADER COLORMAP block per VRAM buffer, stamped by the guest's tick loop
// (jet64.c) only when the slot's generation is not the block's: a table almost never moves, so
// this costs one comparison a tick. A cold boot clears it with the block. Cartridges never touch
// it. Mirrors jet64_core::hardware_map.
#define JET_SHADER_COLORMAP_FRAMES_OFFSET (JET_SHADER_FRAMES_OFFSET + JET_SHADER_FRAMES_BYTES) // 0x00B17510
#define JET_SHADER_COLORMAP_FRAME_SLOTS JET_VRAM_BUFFER_COUNT            // one slot per VRAM buffer
#define JET_SHADER_COLORMAP_FRAME_SLOT_BYTES JET_SHADER_COLORMAP_BYTES   // each a whole COLORMAP block
#define JET_SHADER_COLORMAP_FRAMES_BYTES (JET_SHADER_COLORMAP_FRAME_SLOTS * JET_SHADER_COLORMAP_FRAME_SLOT_BYTES)

_Static_assert(JET_SHADER_COLORMAP_FRAMES_OFFSET == 0x00B17510,
               "SHADER COLORMAP FRAMES block moved — jet64_core::hardware_map must match");
_Static_assert(JET_SHADER_COLORMAP_FRAMES_OFFSET % JET_SHADER_BLOCK_ALIGN == 0 &&
                   JET_SHADER_COLORMAP_FRAME_SLOT_BYTES % JET_SHADER_BLOCK_ALIGN == 0,
               "every SHADER COLORMAP FRAMES slot must start on a vec4, as the block it copies does");

// ===== COPPER FRAMES block (the Copper each frame was drawn with; docs/copper.md section 7.1) =====
// One copy of the Copper block per VRAM buffer, stamped by the guest's tick loop (jet64.c): the
// header always, and while the enable byte is set the rows the screen has, so a host whose guest
// may already be inside the next tick composites each frame with the Copper it was drawn against.
// Rows past the screen's height are never stamped. Cartridges never touch it. Mirrors
// jet64_core::hardware_map.
#define JET_COPPER_FRAMES_OFFSET (JET_SHADER_COLORMAP_FRAMES_OFFSET + JET_SHADER_COLORMAP_FRAMES_BYTES) // 0x00B1D540
#define JET_COPPER_FRAME_SLOTS JET_VRAM_BUFFER_COUNT // one slot per VRAM buffer
#define JET_COPPER_FRAME_SLOT_BYTES (JET_COPPER_HEADER_BYTES + JET_COPPER_MAX_LINES * JET_COPPER_ENTRY_BYTES) // a whole Copper block
#define JET_COPPER_FRAMES_BYTES (JET_COPPER_FRAME_SLOTS * JET_COPPER_FRAME_SLOT_BYTES)

_Static_assert(JET_COPPER_FRAMES_OFFSET == 0x00B1D540,
               "COPPER FRAMES block moved — jet64_core::hardware_map must match");
_Static_assert(JET_COPPER_FRAMES_OFFSET % JET_COPPER_HEADER_BYTES == 0 &&
                   JET_COPPER_FRAME_SLOT_BYTES % JET_COPPER_HEADER_BYTES == 0,
               "every COPPER FRAMES slot keeps its table 64-byte aligned, as the block it copies does");
_Static_assert(JET_COPPER_FRAMES_OFFSET + JET_COPPER_FRAMES_BYTES <= JET_FONT_OFFSET,
               "COPPER FRAMES block overflows into the Font ROM");

// ===== NET CMD block (host-mediated networking; docs/networking/design.md section 7.1) =====
// The mailbox through which the guest asks the HOST to perform the networking that
// needs TLS. Raw TCP/UDP are guest-direct in jet_net.c precisely because they need
// none; HTTP(S) and WebSocket are serviced host-side so the one audited TLS stack
// in the Rust host serves all three runtimes. Mirrors jet64_core::hardware_map; the
// cart-facing APIs are the opt-in headers jet_http.h and jet_ws.h.
//
// A CONTROL block, not a data buffer: request bodies and response buffers live in
// WRAM and travel as motherboard-relative offsets the host re-validates on every
// access, the PCM descriptor discipline. The URL and extra request headers are the
// deliberate exception: they are STAGED inside the block, because a cart's URL is
// nearly always a string literal in guest .rodata the host cannot see, and because
// staging by slot index means there is no offset for a cart to get wrong or forge.
//
// Ownership per slot is the BGM CMD mailbox: `state` names the side that may write
// every other field. Guest fills the descriptor, then release-stores SUBMITTED; host
// claims ACTIVE, services, writes results, then release-stores DONE/ERROR; guest reads,
// then stores FREE. The one field the non-owner writes is `abort` (guest-owned, raised
// only), which is why it is its own word and not a state value.
#define JET_NET_CMD_BLOCK_BYTES  (32 * 1024)
#define JET_NET_CMD_HEADER_BYTES 64          // magic + version + geometry + capabilities
#define JET_NET_CMD_MAGIC        0x34364E4Au // 'J','N','6','4' little-endian, host-stamped
#define JET_NET_CMD_VERSION      1

#define JET_NET_HTTP_SLOTS       8           // concurrent host-mediated HTTP requests
#define JET_NET_HTTP_SLOT_BYTES  128
#define JET_NET_WS_SLOTS         8           // concurrent host-mediated WebSockets
#define JET_NET_WS_SLOT_BYTES    64
#define JET_NET_CMD_HTTP_TABLE_OFFSET JET_NET_CMD_HEADER_BYTES
#define JET_NET_CMD_WS_TABLE_OFFSET   (JET_NET_CMD_HTTP_TABLE_OFFSET + JET_NET_HTTP_SLOTS * JET_NET_HTTP_SLOT_BYTES)

// Per-slot request staging: the URL, then the extra request headers.
#define JET_NET_CMD_STAGE_OFFSET       2048
#define JET_NET_CMD_STAGE_URL_MAX      1024
#define JET_NET_CMD_STAGE_HEADERS_MAX  1024
#define JET_NET_CMD_STAGE_SLOT_BYTES   (JET_NET_CMD_STAGE_URL_MAX + JET_NET_CMD_STAGE_HEADERS_MAX)

// The WebSocket URL staging table, in the reserve the HTTP stages left behind.
// A socket has no header block, so one URL region per slot is all it needs.
#define JET_NET_CMD_WS_STAGE_OFFSET \
    (JET_NET_CMD_STAGE_OFFSET + JET_NET_HTTP_SLOTS * JET_NET_CMD_STAGE_SLOT_BYTES)
#define JET_NET_CMD_WS_STAGE_URL_MAX 1024

// Slot ownership states. FREE must stay 0: it is the meaning of the spawn-time zero.
#define JET_NET_SLOT_FREE      0u  // nobody owns it; the guest may claim it
#define JET_NET_SLOT_SUBMITTED 1u  // guest filled it; the host may claim it
#define JET_NET_SLOT_ACTIVE    2u  // host is servicing it; only the host may write
#define JET_NET_SLOT_DONE      3u  // host published a result; the guest may read
#define JET_NET_SLOT_ERROR     4u  // host published a failure; `error` says why

// Header `capabilities` bits, the host-mediated half of JetNetCapability
// (jet_net.h). The guest-direct half never appears here: sockets are a
// compile-time fact about the guest, not something a host can grant.
#define JET_NET_CMD_CAP_HTTP       (1u << 3)
#define JET_NET_CMD_CAP_HTTP_SERVE (1u << 4)
#define JET_NET_CMD_CAP_WS         (1u << 5)
#define JET_NET_CMD_CAP_RTC        (1u << 6)

// ----- The dial gate (design §14): the guest's own sockets ask first -----
// jet_net's TCP and UDP are guest-direct — the cart's own sockets, not the host's —
// so the host cannot stand between them and the wire. What it can do is answer
// first. Before jet_tcp_connect dials a host, and before jet_udp_send_to sends to an
// address that has not sent to the socket, jet_net.c stages the destination in one
// of these descriptors and waits for the host's answer: the same consent, allowlist
// and first-contact log the host-mediated transports pass through. A header whose
// `dial_slot_count` is 0 is a host with no gate, and the sockets dial as before.
//
// Mailbox discipline as for HTTP: FREE → the guest stages the host and the port,
// then release-stores SUBMITTED → the host decides, writes `answer`, release-stores
// DONE (or leaves the slot SUBMITTED while its consent prompt is up) → the guest
// reads the answer and stores FREE. `abort` is the guest's one word in a slot it
// does not own: raised, the host still answers, and the guest reclaims the slot.
#define JET_NET_DIAL_SLOTS             8
#define JET_NET_DIAL_SLOT_BYTES        32
#define JET_NET_CMD_DIAL_TABLE_OFFSET  (JET_NET_CMD_WS_TABLE_OFFSET + JET_NET_WS_SLOTS * JET_NET_WS_SLOT_BYTES)
#define JET_NET_CMD_DIAL_STAGE_OFFSET \
    (JET_NET_CMD_WS_STAGE_OFFSET + JET_NET_WS_SLOTS * JET_NET_CMD_WS_STAGE_URL_MAX)
#define JET_NET_CMD_DIAL_STAGE_HOST_MAX 256 // matches JET_NET_HOST_MAX (jet_net.h)
#define JET_NET_DIAL_TCP 1u // a jet_tcp_connect to a named host
#define JET_NET_DIAL_UDP 2u // a jet_udp_send_to an address that has not sent first

// One 32-byte dial descriptor.
// Internal: Transport ABI; carts use jet_tcp_connect and jet_udp_send_to.
typedef struct {
    uint32_t state;      // +0  JET_NET_SLOT_*
    uint32_t kind;       // +4  JET_NET_DIAL_*
    uint32_t host_len;   // +8  bytes staged in this slot's host stage (no NUL)
    uint32_t port;       // +12 the port named
    int32_t  answer;     // +16 HOST-written: JET_NET_OK to dial, JET_NET_E_DENIED not to
    uint32_t abort;      // +20 GUEST-written: 1 = the cart no longer needs the answer
    uint8_t  reserved[8]; // +24 to 32
} JetNetDialSlot;

// ----- WebRTC data channels (jet_rtc.h, docs/networking/rtc.md) -----
// A peer connection is host-mediated like a WebSocket: the guest DESCRIBES it here
// and the host drives it — a Rust WebRTC stack on native, the browser's own
// RTCPeerConnection on the web — while whole messages cross two rings in the cart's
// arena with the WebSocket's framing (net_ring_internal.h), the frame's kind byte
// naming the lane. The arena also holds the two connection descriptions, the local
// one written by the host and the remote one by the guest, so the cart can carry
// them over whatever signalling channel it likes. The ICE servers are staged in the
// block, as a URL is for a socket.
//
// `state` is the descriptor's mailbox token, exactly as for a WebSocket: the guest
// submits, the host claims it ACTIVE for the connection's whole life and frees it
// once the guest has asked to close and the connection has let go. `tx_head`,
// `rx_tail`, `remote_len` and `close` are the guest's; `link`, `local_len`, `error`,
// `path`, `rtt_ms`, `tx_tail` and `rx_head` are the host's — each with one writer for
// the slot's life, as each ring's producer owns its head and its consumer its tail.
#define JET_NET_RTC_SLOTS             4
#define JET_NET_RTC_SLOT_BYTES        128
#define JET_NET_CMD_RTC_TABLE_OFFSET \
    (JET_NET_CMD_DIAL_STAGE_OFFSET + JET_NET_DIAL_SLOTS * JET_NET_CMD_DIAL_STAGE_HOST_MAX)
#define JET_NET_CMD_RTC_STAGE_OFFSET \
    (JET_NET_CMD_RTC_TABLE_OFFSET + JET_NET_RTC_SLOTS * JET_NET_RTC_SLOT_BYTES)
#define JET_NET_CMD_RTC_STAGE_BYTES   896 // the ICE servers, as JET_RTC_ICE_* records
// The arena's geometry: two description areas of this size, then two rings of at
// least this size each, both powers of two.
#define JET_NET_RTC_DESCRIPTION_MAX   4096
#define JET_NET_RTC_RING_MIN          4096
// The two roles a connection may take, the two paths it may be allowed.
#define JET_NET_RTC_ROLE_OFFER        1u
#define JET_NET_RTC_ROLE_ANSWER       2u
#define JET_NET_RTC_POLICY_ANY        1u // direct when it can be, relayed when it must
#define JET_NET_RTC_POLICY_RELAY      2u // always through a TURN relay: no address shown
// `path`, what the host reports of the route the connection found.
#define JET_NET_RTC_PATH_UNKNOWN      0u
#define JET_NET_RTC_PATH_DIRECT       1u
#define JET_NET_RTC_PATH_RELAYED      2u
// The frame kinds the rings carry: the lane each message travels.
#define JET_NET_RTC_LANE_FAST         1u // unordered, never retransmitted
#define JET_NET_RTC_LANE_SURE         2u // ordered and reliable
// One staged ICE server record: three NUL-terminated strings — the URL, the
// username, the credential — back to back; the stage holds `ice_count` of them.

// One 128-byte WebRTC descriptor.
// Internal: Transport ABI; carts use JetRtc and the jet_rtc_* functions.
typedef struct {
    uint32_t state;           // +0   JET_NET_SLOT_*
    uint32_t link;            // +4   HOST-written: JET_NET_WS_LINK_* (the same four states)
    uint32_t role;            // +8   JET_NET_RTC_ROLE_*
    uint32_t policy;          // +12  JET_NET_RTC_POLICY_*
    uint32_t ice_count;       // +16  ICE server records staged
    uint32_t ice_bytes;       // +20  bytes of the stage they occupy
    uint32_t local_offset;    // +24  motherboard-relative WRAM offset of the local description area
    uint32_t remote_offset;   // +28  … of the remote description area
    uint32_t description_cap; // +32  bytes each area holds
    uint32_t local_len;       // +36  HOST-written: the local description's length, once ready
    uint32_t remote_len;      // +40  GUEST-written: the remote description's length, once given
    uint32_t tx_offset;       // +44  motherboard-relative WRAM offset of the TX ring
    uint32_t tx_bytes;        // +48  TX ring capacity, a power of two
    uint32_t rx_offset;       // +52  … of the RX ring
    uint32_t rx_bytes;        // +56  RX ring capacity, a power of two
    uint32_t tx_head;         // +60  GUEST-written: bytes produced into TX
    uint32_t tx_tail;         // +64  HOST-written:  bytes consumed from TX
    uint32_t rx_head;         // +68  HOST-written:  bytes produced into RX
    uint32_t rx_tail;         // +72  GUEST-written: bytes consumed from RX
    int32_t  error;           // +76  HOST-written: the JetNetResult that ended it
    uint32_t path;            // +80  HOST-written: JET_NET_RTC_PATH_*
    uint32_t rtt_ms;          // +84  HOST-written: the transport's round trip, 0 = not yet known
    uint32_t close;           // +88  GUEST-written: 1 = hang up and hand the slot back
    uint8_t  reserved[36];    // +92 to 128
} JetNetRtcSlot;

// The 64-byte NET CMD header. Host-owned in its entirety; the guest only reads.
// Internal: Transport ABI; carts use jet_net.h, jet_http.h, and jet_ws.h.
typedef struct {
    uint32_t magic;           // +0  JET_NET_CMD_MAGIC; absent on a host without the service
    uint16_t version;         // +4  JET_NET_CMD_VERSION
    uint16_t http_slot_count; // +6  HTTP descriptors this host actually services
    uint16_t ws_slot_count;   // +8  WebSocket descriptors this host actually services
    uint16_t dial_slot_count; // +10 dial asks this host answers; 0 = jet_net's sockets are ungated
    uint32_t capabilities;    // +12 JET_NET_CMD_CAP_*, what this runtime can really do
    uint16_t rtc_slot_count;  // +16 WebRTC descriptors this host actually services
    uint16_t reserved1;       // +18
    uint8_t  reserved[44];    // +20 to 64
} JetNetCmdHeader;

// One 128-byte HTTP request descriptor. Single-writer-per-state: the side named by
// `state` owns every field except `abort`, which the guest raises to abandon a
// request it no longer wants (the host then reaps the slot instead of publishing).
// Internal: Transport ABI; carts use JetHttpParams and the jet_http_* functions.
typedef struct {
    uint32_t state;       // +0  JET_NET_SLOT_*
    uint32_t method;      // +4  JetHttpMethod
    uint32_t url_len;     // +8  bytes staged in this slot's URL stage (no NUL)
    uint32_t headers_len; // +12 bytes staged in this slot's header stage (no NUL)
    uint32_t body_offset; // +16 motherboard-relative WRAM offset of the request body
    uint32_t body_len;    // +20 0 = no request body
    uint32_t resp_offset; // +24 motherboard-relative WRAM offset of the response buffer
    uint32_t resp_cap;    // +28 capacity of that buffer in bytes
    uint32_t timeout_ms;  // +32 0 = JET_HTTP_DEFAULT_TIMEOUT_MS
    uint32_t resp_len;    // +36 HOST-written: bytes landed in the response buffer
    int32_t  http_code;   // +40 HOST-written: 200, 404, ...
    int32_t  error;       // +44 HOST-written: the JetNetResult that ended it
    uint32_t flags;       // +48 HOST-written: JET_NET_HTTP_FLAG_*
    uint32_t abort;       // +52 GUEST-written: 1 = the request was abandoned
    uint8_t  reserved[72];// +56 to 128
} JetNetHttpSlot;

// `flags` bit: the body outgrew resp_cap; the first resp_cap bytes were kept.
#define JET_NET_HTTP_FLAG_TRUNCATED 0x01u

// One 64-byte WebSocket descriptor. TWO ownership rules live in it, not one.
// `state` is the mailbox token for the DESCRIPTOR, that is, url_len, the ring geometry,
// `link`, `error`, `close_code`. The four ring counters are different: each has
// ONE permanent writer for the whole life of the socket, whatever `state` says,
// because a stream has no handoff to make. The guest produces into TX and consumes
// from RX; the host consumes from TX and produces into RX. That is the KEYBOARD /
// AUDIO SCOPE discipline, so the data path is lock-free and reads identically
// in-process (player, web) and across the workstation's process boundary.
//
// The counters are MONOTONIC byte totals, not indices: `head - tail` is the bytes
// in flight (0..capacity, never ambiguous), and a position is `counter & (cap-1)`.
// That is why both capacities must be powers of two: it makes the counters' wrap
// at 2^32 land exactly on a ring boundary, so there is nothing to special-case.
// Internal: Transport ABI; carts use JetWs and the jet_ws_* functions.
typedef struct {
    uint32_t state;      // +0  JET_NET_SLOT_*
    uint32_t link;       // +4  HOST-written: JET_NET_WS_LINK_*
    uint32_t url_len;    // +8  bytes staged in this slot's URL stage (no NUL)
    uint32_t tx_offset;  // +12 motherboard-relative WRAM offset of the TX ring
    uint32_t tx_bytes;   // +16 TX ring capacity, a power of two
    uint32_t rx_offset;  // +20 motherboard-relative WRAM offset of the RX ring
    uint32_t rx_bytes;   // +24 RX ring capacity, a power of two
    uint32_t tx_head;    // +28 GUEST-written: bytes produced into TX
    uint32_t tx_tail;    // +32 HOST-written:  bytes consumed from TX
    uint32_t rx_head;    // +36 HOST-written:  bytes produced into RX
    uint32_t rx_tail;    // +40 GUEST-written: bytes consumed from RX
    int32_t  error;      // +44 HOST-written: the JetNetResult that ended it
    uint16_t close_code; // +48 HOST-written: the RFC 6455 close code
    uint16_t reserved0;  // +50 keeps `close` 4-byte aligned
    uint32_t close;      // +52 GUEST-written: 1 = hang up and hand the slot back
    uint8_t  reserved[8]; // +56 to 64
} JetNetWsSlot;

// `link` values, the life of the connection, deliberately the same four the TCP
// surface uses (JetTcpState), so a session layer drives either through one switch.
#define JET_NET_WS_LINK_CONNECTING 0u
#define JET_NET_WS_LINK_OPEN       1u
#define JET_NET_WS_LINK_CLOSED     2u
#define JET_NET_WS_LINK_FAILED     3u

// Ring frame header: u32 payload_len, u8 type, three reserved bytes. Eight bytes
// rather than five so every frame starts 4-byte aligned in a power-of-two ring,
// which is what keeps a u32 length from ever straddling the wrap point.
#define JET_NET_WS_FRAME_HEADER_BYTES 8u
#define JET_NET_WS_FRAME_TYPE_OFFSET  4u

// ===== LAUNCH block (how the player was started; jet_launch.h) =====
// The values the player was launched with, stamped by the host once, before the
// guest's first instruction, and never written again: the web page's query
// parameters (`?join=7Q4K`), the native player's `key=value` arguments, or the
// workstation's own (project.ron `launch`, or an agent-door scenario's). A 16-byte
// header, then `count` entries packed end to end, each `key NUL value NUL`. The
// host decodes, bounds and orders the entries; the guest only reads. A block no
// host stamped is all zero, and its missing magic is the "launched with nothing"
// answer. Internal: Transport ABI; carts use jet_launch.h.
#define JET_LAUNCH_OFFSET        (JET_NET_CMD_OFFSET + JET_NET_CMD_BLOCK_BYTES) // 0x00A90980
#define JET_LAUNCH_BLOCK_BYTES   2048
#define JET_LAUNCH_HEADER_BYTES  16
#define JET_LAUNCH_MAGIC         0x34364C4Au // 'J','L','6','4' little-endian, host-stamped
#define JET_LAUNCH_VERSION       1
#define JET_LAUNCH_MAGIC_OFFSET   0          // u32, JET_LAUNCH_MAGIC once stamped
#define JET_LAUNCH_VERSION_OFFSET 4          // u16, JET_LAUNCH_VERSION
#define JET_LAUNCH_COUNT_OFFSET   6          // u16, entries in the table
#define JET_LAUNCH_BYTES_OFFSET   8          // u32, bytes the table occupies after the header
#define JET_LAUNCH_TABLE_OFFSET  JET_LAUNCH_HEADER_BYTES
#define JET_LAUNCH_TABLE_BYTES   (JET_LAUNCH_BLOCK_BYTES - JET_LAUNCH_HEADER_BYTES)

/**
 * Struct: JetBgmChannelState
 * Summary: A small read-only snapshot of what one channel of the playing music is doing right now. It is meant for music visualisers.
 * Fields:
 *   - sample: The number of the sample or patch the channel is playing, counting from 1. 0 means none.
 *   - volume: How loud the channel is right now, from 0 to 255. The tracker works on a scale of 0 to 64; the host takes the peak of each mix buffer on that scale and stretches it to 0 to 255.
 *   - period: The note's pitch as a ProTracker period, the tracker's own unit for pitch. 0 means no note is sounding.
 * Include: jet64.h
 * Notes: BGM_STATE is live hardware that the host owns. Read the fields, but never write them, and never keep a pointer to them across a project reload.
 * Example:
 *   uint8_t lead_volume = BGM_STATE[0].volume;
 */
typedef struct {
    uint8_t sample;
    uint8_t volume;
    uint16_t period; // the pitch, as a ProTracker period
} JetBgmChannelState;

/**
 * Struct: JetBgmTrackerChannel
 * Summary: One cell of the music's pattern, for one channel on one row: the note, the sample number and the effect. The host publishes a window of these cells while music plays.
 * Fields:
 *   - period: The note's pitch as a ProTracker period. 0 means this cell starts no new note.
 *   - sample: The number of the sample or patch, counting from 1. 0 means the channel keeps the one it had.
 *   - effect_cmd: The ProTracker effect command. In the file it is a nibble, four bits; here it is widened to a whole byte.
 *   - effect_arg: The effect's argument byte.
 *   - reserved: Spare bytes. Ignore them.
 * Include: jet64.h
 * Notes: Read-only live hardware, meant for drawing visualisers. The window of cells can move on between frames, so use a cell in the same _update or _draw you read it in. Do not store its address.
 * Example:
 *   JetBgmTrackerChannel cell = BGM_TRACKER->lines[0][0];
 */
typedef struct {
    uint16_t period;
    uint8_t sample;
    uint8_t effect_cmd;
    uint8_t effect_arg;
    uint8_t reserved[3];
} JetBgmTrackerChannel;

/**
 * Struct: JetBgmTrackerPattern
 * Summary: A read-only window onto the playing music's pattern, 64 rows by 32 channels, published by the background-music player.
 * Fields:
 *   - pattern_pos: Where the song is in its list of patterns, the song order.
 *   - line_pos: The row the playhead is on, counted within this window. Use it to index lines[].
 *   - num_channels: How many channels the music uses, up to 32.
 *   - num_lines: How many rows of this window hold real cells, up to 64.
 *   - line_base: The row of the whole pattern that this window starts at. Always 0 for a MOD file.
 *   - reserved: A spare byte. Ignore it.
 *   - lines: The cells, row by row, indexed as lines[row][channel].
 * Include: jet64.h
 * Notes: BGM_TRACKER points into shared hardware that the host writes. Treat every field as read-only, and keep your loops within num_lines and num_channels.
 *
 *        An XM pattern can be up to 256 rows tall, so this window is a 64-row slice that slides down it. line_pos is the playhead's row WITHIN THE WINDOW; the playhead's row within the whole pattern is jet_bgm_row(). Follow the playhead with jet_bgm_row, never with line_pos alone, because line_pos drops back to 0 each time the window slides to the next slice.
 * Example:
 *   JetBgmTrackerChannel cell = BGM_TRACKER->lines[BGM_TRACKER->line_pos][0];
 */
typedef struct {
    uint8_t pattern_pos;
    uint8_t line_pos;
    uint8_t num_channels;
    uint8_t num_lines;
    uint8_t line_base;
    uint8_t reserved;
    JetBgmTrackerChannel lines[64][32]; // 16 KB: the sliding window of cells
} JetBgmTrackerPattern;

// The BGM TRANSPORT block: which of the cart's plays the host is playing, how
// many times it has come round, its module's own title, and the newest play the
// host refused. It sits in the tracker region's spare tail, 64-aligned after the
// pattern window. The host writes every byte of it; a cart reads it through
// jet_bgm_song, jet_bgm_traversals, jet_bgm_title and jet_bgm_refused
// (jet_audio.h). Mirrors
// jet64_core::hardware_map and audio::types::JetBgmTransport.
#define JET_BGM_TRANSPORT_BYTES 64

/**
 * Constant: JET_BGM_NO_SONG
 * Summary: The ticket that names no song.
 * Include: jet64.h
 * Notes: jet_bgm_song answers with it while none of your cartridge's songs is playing: before the first one has started, after jet_stop_mod, and while the workstation's Tune Grid auditions a song of its own. jet_play_mod never issues it to a song it sends, so a ticket you hold can never be mistaken for it.
 * Example:
 *   bool music_playing = jet_bgm_song() != JET_BGM_NO_SONG;
 */
#define JET_BGM_NO_SONG 0u

/**
 * Constant: JET_BGM_TITLE_BYTES
 * Summary: The size of the buffer that holds any song's title, its closing NUL byte included.
 * Include: jet64.h
 * Notes: The host publishes a title of up to 31 characters, room for the longest title a tracker format carries: Scream Tracker 3's 28. Size a buffer for jet_bgm_title with it and no title is ever cut.
 * Example:
 *   char title[JET_BGM_TITLE_BYTES];
 */
#define JET_BGM_TITLE_BYTES 32

/**
 * Struct: JetBgmTransport
 * Summary: A read-only record of what the host is playing for your cartridge: which of your plays it is, how many times it has come round, and the title its module gives itself.
 * Fields:
 *   - ticket: The ticket jet_play_mod issued the song that is playing, or JET_BGM_NO_SONG while none of your songs is.
 *   - traversals: How many times that song has come round since it started. It comes round when it plays off the end of its order list, and when a jump in it lands back on a row it has already played the same way.
 *   - title: The title the song's module gives itself, as printable ASCII, ending in a NUL byte.
 *   - refused: The ticket of the newest play the host refused, because its file could not be found, read or understood as music; JET_BGM_NO_SONG while it has refused none.
 *   - reserved: Spare bytes. The host owns them.
 * Include: jet64.h
 * Notes: BGM_TRANSPORT points into shared hardware that the host writes. Read it through jet_bgm_song, jet_bgm_traversals, jet_bgm_title and jet_bgm_refused rather than field by field. When a new song starts, the host takes the old ticket down before it changes the other fields and puts the new ticket up last, and no ticket ever names two songs; those calls check the ticket on both sides of what they read, so they can never pair one song's ticket with another song's count or title. refused needs no such care: it is a ticket on its own, written whole, and describes no other field.
 * Example:
 *   uint32_t song = jet_bgm_song();
 */
typedef struct {
    uint32_t ticket;                  // +0  the playing song's ticket; JET_BGM_NO_SONG when none
    uint32_t traversals;              // +4  times it has come round since it started
    char     title[JET_BGM_TITLE_BYTES]; // +8 its module's own title, printable ASCII, NUL-terminated
    uint32_t refused;                 // +40 the newest play the host refused; JET_BGM_NO_SONG until one is
    uint8_t  reserved[JET_BGM_TRANSPORT_BYTES - 3 * sizeof(uint32_t) - JET_BGM_TITLE_BYTES]; // +44 to 64
} JetBgmTransport;

/**
 * Struct: JetFmOperator
 * Summary: One of the four operators in an FM instrument. An operator is a small oscillator with its own volume envelope, and FM synthesis makes a sound by letting operators bend one another's pitch.
 * Fields:
 *   - attack: The attack stage of the volume envelope, when the sound rises from silence, from 0 to 255.
 *   - decay: The decay stage, when the sound falls from its peak to the sustain level, from 0 to 255.
 *   - sustain: The sustain level, held for as long as the note is held, from 0 to 255.
 *   - release: The release stage, when the sound dies away after the note is let go, from 0 to 255.
 *   - level: How loud the operator's output is, from 0 to 255.
 *   - ratio: The operator's frequency as a multiple of the note's frequency.
 *   - detune: A small extra pitch offset, up or down; it is signed.
 *   - feedback: How much of the operator's own output is fed back into its input.
 *   - wave: Which waveform the operator uses.
 *   - phase: Where in its wave the operator starts.
 *   - env_curve: The number of the envelope curve authored for it.
 *   - flags: Option bits kept for the FM engine.
 * Include: jet64.h
 * Notes: Most cartridges set these values in Instrument Studio and never touch them in code. If you build an instrument in code, start from jet_make_fm_operator, so that every field you do not set is zero and not whatever happened to be in memory.
 * Example:
 *   JetFmOperator carrier = jet_make_fm_operator(10, 20, 180, 30, 255, 1);
 */
typedef struct {
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    uint8_t level;
    uint8_t ratio;
    int8_t detune;
    uint8_t feedback;
    uint8_t wave;
    uint8_t phase;
    uint8_t env_curve;
    uint8_t flags;
} JetFmOperator;

/**
 * Function: jet_make_fm_operator
 * Summary: Makes a JetFmOperator from the six fields you set most often, with every other field zero.
 * Parameters:
 *   - attack: The attack stage of the volume envelope, from 0 to 255.
 *   - decay: The decay stage, from 0 to 255.
 *   - sustain: The sustain level, from 0 to 255.
 *   - release: The release stage, from 0 to 255.
 *   - level: How loud the operator's output is, from 0 to 255.
 *   - ratio: The operator's frequency as a multiple of the note's frequency.
 * Returns: The finished JetFmOperator, handed back as a value you can assign.
 * Include: jet64.h
 * Notes: detune, feedback, wave, phase, env_curve and flags are all set to zero. The value owns no memory, so you can copy it freely; you normally assign it into JetInstrumentPatch.patch_data.fm_ops.
 * Example:
 *   JetFmOperator op = jet_make_fm_operator(10, 20, 50, 10, 255, 2);
 */
static inline JetFmOperator jet_make_fm_operator(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release, uint8_t level, uint8_t ratio) {
    JetFmOperator op;
    op.attack = attack;
    op.decay = decay;
    op.sustain = sustain;
    op.release = release;
    op.level = level;
    op.ratio = ratio;
    op.detune = 0;
    op.feedback = 0;
    op.wave = 0;
    op.phase = 0;
    op.env_curve = 0;
    op.flags = 0;
    return op;
}

/**
 * Struct: JetDrumParams
 * Summary: The settings for one drum sound that the machine makes from scratch, with no recording. It lives inside a JetPatchData. A drum here is built from three parts, a tonal body, a click at the start and a burst of noise, plus a few extras.
 * Fields:
 *   - model: Which kind of drum: kick, snare, hi-hat, clap, tom and so on.
 *   - body_wave: The waveform of the tonal body, the pitched part of the drum.
 *   - body_level: How loud the body is, from 0 to 255.
 *   - body_decay: The body's decay, how its volume dies away, from 0 to 255.
 *   - start_freq: The body's frequency at the start of the hit.
 *   - end_freq: The body's frequency at the end of the hit.
 *   - pitch_decay: The decay of the pitch envelope, the slide from the starting frequency to the ending one, from 0 to 255.
 *   - pitch_curve: The shape of that pitch envelope.
 *   - click_level: How loud the click at the very start is, from 0 to 255.
 *   - click_decay: The click's decay, how it dies away, from 0 to 255.
 *   - click_tone: The tone of the click.
 *   - click_phase: Where in its wave the click starts.
 *   - noise_level: How loud the noise part is, from 0 to 255.
 *   - noise_decay: The noise's decay, how it dies away, from 0 to 255.
 *   - noise_color: The colour of the noise, its balance between bright and dark.
 *   - noise_filter: The cutoff of the filter on the noise.
 *   - noise_resonance: How much the noise filter rings at its cutoff.
 *   - drive: The amount of drive, that is, saturation.
 *   - punch: How much punch the transient, the very start of the hit, gets.
 *   - tone: The overall tone.
 *   - modal_mix: How much of the modal part, the ringing resonances, is mixed in.
 *   - modal_ratio_a: The frequency of the first modal ring, as a ratio.
 *   - modal_ratio_b: The frequency of the second modal ring, as a ratio.
 *   - metallic: How much of the metallic part is mixed in.
 *   - _pad: Spare bytes that keep the record at 48 bytes. Keep them zero.
 * Include: jet64.h
 * Notes: Make drum sounds in Instrument Studio whenever you can. If you build one in code, zero the JetInstrumentPatch it lives in first, set that patch's engine_type, then fill in these fields. The record owns no memory outside itself.
 * Example:
 *   JetInstrumentPatch kick = jet_make_instrument_patch();
 *   kick.patch_data.drum.model = 0;
 */
typedef struct {
    uint8_t model;
    uint8_t body_wave;
    uint8_t body_level;
    uint8_t body_decay;
    uint16_t start_freq;
    uint16_t end_freq;
    uint8_t pitch_decay;
    uint8_t pitch_curve;
    uint8_t click_level;
    uint8_t click_decay;
    uint8_t click_tone;
    uint8_t click_phase;
    uint8_t noise_level;
    uint8_t noise_decay;
    uint8_t noise_color;
    uint8_t noise_filter;
    uint8_t noise_resonance;
    uint8_t drive;
    uint8_t punch;
    uint8_t tone;
    uint8_t modal_mix;
    uint8_t modal_ratio_a;
    uint8_t modal_ratio_b;
    uint8_t metallic;
    uint8_t _pad[22];
} JetDrumParams;

/**
 * Constant: JET_PATCH_SOURCE_NONE
 * Summary: A voice register's patch_source when it plays no patch: the synth sounds the register's own fields and nothing else.
 * Include: jet64.h
 * Notes: jet_sfx writes it, and it is the state every register starts in, so a register you fill in field by field plays exactly what you wrote, through the engine its waveform names, whatever patch sits in the bank at its patch_idx. Mirrors jet64_core::audio::types::PATCH_SOURCE_NONE.
 * Example:
 *   JetAudioRegister reg = jet_make_audio_register(); // reg.patch_source is JET_PATCH_SOURCE_NONE
 */
#define JET_PATCH_SOURCE_NONE 0u
/**
 * Constant: JET_PATCH_SOURCE_BANK
 * Summary: A voice register's patch_source when it was loaded from the bank patch its patch_idx names, which the synth then plays beside it.
 * Include: jet64.h
 * Notes: jet_sfx_play and jet_note_on and their siblings write it, so the voice keeps the patch's engine, operators, curves and stop fade. Any value but this one plays no patch. Mirrors jet64_core::audio::types::PATCH_SOURCE_BANK.
 * Example:
 *   reg.patch_idx = 12;
 *   reg.patch_source = JET_PATCH_SOURCE_BANK; // play bank patch 12 beside this register
 */
#define JET_PATCH_SOURCE_BANK 1u

/**
 * Struct: JetAudioRegister
 * Summary: Live 64-byte synthesiser voice register; the high-level jet_sfx_* and jet_note_on APIs populate it from authored patches.
 * Fields:
 *   - trigger: Generation byte incremented to start a new sound.
 *   - waveform: Live waveform or lifecycle command.
 *   - volume: Voice level from 0 to 255.
 *   - pan: Stereo pan from 0 (left) through 128 (centre) to 255 (right).
 *   - pitch_base: Initial pitch-period control.
 *   - pitch_limit: Pitch-ramp limit.
 *   - pitch_ramp: Signed primary pitch sweep.
 *   - pitch_dramp: Signed change applied to pitch_ramp.
 *   - duty: Pulse-width control.
 *   - duty_ramp: Signed pulse-width sweep.
 *   - env_attack: Attack-envelope control.
 *   - env_sustain: Sustain-envelope control.
 *   - env_decay: Decay-envelope control.
 *   - env_punch: Envelope punch amount.
 *   - lpf_freq: Low-pass filter cutoff.
 *   - lpf_ramp: Signed low-pass cutoff sweep.
 *   - lpf_resonance: Low-pass resonance.
 *   - hpf_freq: High-pass filter cutoff.
 *   - hpf_ramp: Signed high-pass cutoff sweep.
 *   - vib_strength: Vibrato depth.
 *   - vib_speed: Vibrato speed.
 *   - vib_delay: Vibrato onset delay.
 *   - arp_speed: Arpeggio step speed.
 *   - arp_mod: Signed arpeggio pitch modifier.
 *   - pha_offset: Signed phaser offset.
 *   - pha_ramp: Signed phaser sweep.
 *   - repeat_speed: Retrigger/repeat speed.
 *   - patch_idx: The bank patch this voice was loaded from, when patch_source is JET_PATCH_SOURCE_BANK.
 *   - strike_pitch_mult: How far the strike that loaded this register moved its patch's pitch, as a multiplier. jet_note_on and jet_sfx_play write it; 0, on a register no strike has written, plays the patch as it was made.
 *   - patch_source: Whether patch_idx names a bank patch the synth plays beside this register (JET_PATCH_SOURCE_BANK, which jet_sfx_play and jet_note_on write) or the register plays its own fields alone (JET_PATCH_SOURCE_NONE, which jet_sfx writes and every register starts as).
 *   - reserved2: Reserved payload bytes; keep zero.
 *   - seq: Seqlock commit byte used by high-level bulk writers; odd means a rewrite is in progress.
 * Include: jet64.h
 * Notes: Prefer jet_sfx_play, jet_note_on and their stop/release helpers. AUDIO aliases live cross-thread hardware; direct bulk writes must obey the seq protocol or the audio thread may observe a torn register.
 * Example:
 *   const volatile JetAudioRegister* voices = (const volatile JetAudioRegister*)AUDIO;
 *   uint8_t current_volume = voices[0].volume;
 */
typedef struct {
    uint8_t trigger;
    uint8_t waveform;
    uint8_t volume;
    uint8_t pan;

    uint16_t pitch_base;
    uint16_t pitch_limit;
    int16_t pitch_ramp;
    int16_t pitch_dramp;

    uint8_t duty;
    int8_t duty_ramp;

    uint8_t env_attack;
    uint8_t env_sustain;
    uint8_t env_decay;
    uint8_t env_punch;

    uint8_t lpf_freq;
    int8_t lpf_ramp;
    uint8_t lpf_resonance;

    uint8_t hpf_freq;
    int8_t hpf_ramp;

    uint8_t vib_strength;
    uint8_t vib_speed;
    uint8_t vib_delay;

    uint8_t arp_speed;
    int8_t arp_mod;

    int8_t pha_offset;
    int8_t pha_ramp;

    uint8_t repeat_speed;

    uint8_t patch_idx;

    // How far the strike that loaded this register transposed its patch, as a
    // pitch multiplier (jet_note_on and jet_sfx_play* write it). The pitch
    // fields above already sound the transposition; when the strike lands, the
    // synth carries it to the one part of a patch no register field can hold,
    // a drum's sweep (JetDrumParams.start_freq and end_freq). 0, a register no
    // strike has written, plays the patch as made, as does any value that is
    // not a positive number.
    float strike_pitch_mult;

    // Whether patch_idx names the bank patch this register was loaded from
    // (JET_PATCH_SOURCE_BANK, which every jet_sfx_play* and jet_note_on* strike
    // writes) or no patch at all (JET_PATCH_SOURCE_NONE, jet_sfx's). The zero
    // state is no patch, and so is any value but BANK, so a register written
    // field by field sounds what its fields say, through the engine its waveform
    // names, and never the operators, curves or stop fade of whatever patch the
    // bank holds at patch_idx.
    uint8_t patch_source;
    uint8_t reserved2[26];

    // Bulk-write commit sequence byte (seqlock). The jet_sfx_play*/jet_note_on*
    // helpers mark it odd before rewriting the register payload and even again
    // once every field has landed, so the synth thread can detect — and skip for
    // one buffer — a snapshot torn by an in-flight bulk write. Carts writing
    // individual register fields by hand never need to touch it.
    uint8_t seq;
} JetAudioRegister;

/**
 * Function: jet_make_audio_register
 * Summary: Initializes a JetAudioRegister structure safely to zeroed defaults.
 * Parameters:
 *   - None.
 * Returns: A zeroed JetAudioRegister struct passed by value.
 * Include: jet64.h
 * Notes: Prefer jet_sfx_play or jet_note_on for ordinary cart audio. Direct register editing is an advanced hardware path; preserve the sequence field protocol when bulk-writing live registers.
 * Example:
 *   JetAudioRegister reg = jet_make_audio_register();
 */
static inline JetAudioRegister jet_make_audio_register(void) {
    JetAudioRegister reg;
    memset(&reg, 0, sizeof(JetAudioRegister));
    return reg;
}

/**
 * Struct: JetPatchData
 * Summary: The 48-byte engine-specific payload within a JetInstrumentPatch, viewed as four FM operators, one drum block or raw bytes.
 * Fields:
 *   - fm_ops: Four JetFmOperator records used by the FM engine.
 *   - drum: JetDrumParams used by the procedural drum engine.
 *   - raw: Uninterpreted 48-byte view used for serialisation and future engines.
 * Include: jet64.h
 * Notes: This is a union: all three fields share the same bytes, so only access the view selected by the enclosing patch's engine_type. It owns no external memory.
 * Example:
 *   JetPatchData data = { 0 };
 *   data.fm_ops[0] = jet_make_fm_operator(10, 20, 180, 30, 255, 1);
 */
typedef union {
    JetFmOperator fm_ops[4];
    JetDrumParams drum;
    uint8_t raw[48];
} JetPatchData;

/**
 * Struct: JetInstrumentPatch
 * Summary: Complete 128-byte authored instrument record used by jet_note_on and the SFX/tracker engines.
 * Fields:
 *   - engine_type: Synthesis engine selector: legacy, wavetable, FM or drum.
 *   - flags: Engine-wide patch option bits.
 *   - amp_curve_idx: Authored amplitude-curve index.
 *   - pitch_curve_idx: Authored pitch-curve index.
 *   - macro_idx: Authored macro-sequence index.
 *   - wavetable_idx: Authored wavetable index.
 *   - fm_algorithm: Four-operator FM routing algorithm.
 *   - fm_feedback: Global FM feedback amount.
 *   - patch_data: Engine-specific FM/drum/raw payload.
 *   - legacy_payload: Legacy synthesiser register payload retained for compatible playback.
 *   - stop_fade_ms: Click-free stop fade in milliseconds; 0 selects the engine default.
 *   - reserved: Reserved footer bytes; keep zero.
 * Include: jet64.h
 * Notes: Instrument Studio is the normal authoring path; carts play its hardware-bank records by patch index. For procedural construction, begin with jet_make_instrument_patch so every reserved and legacy byte is zero.
 * Example:
 *   JetInstrumentPatch patch = jet_make_instrument_patch();
 *   patch.stop_fade_ms = 10;
 */
typedef struct {
    uint8_t engine_type;
    uint8_t flags;
    uint8_t amp_curve_idx;
    uint8_t pitch_curve_idx;
    uint8_t macro_idx;
    uint8_t wavetable_idx;
    uint8_t fm_algorithm;
    uint8_t fm_feedback;

    JetPatchData patch_data;

    JetAudioRegister legacy_payload; // Firmly offsets to exactly 56 bytes maintaining backwards compatibility

    // Footer (8 bytes). stop_fade_ms is the patch's anti-click stop-fade length
    // in milliseconds: 0 selects the engine default (~5 ms, perceptually
    // instant); larger values stretch jet_sfx_stop()'s fade into a deliberate
    // cinematic ramp. Honored by every stop source (cartridge stops, tracker
    // note cuts, editor preview stops).
    uint8_t stop_fade_ms;
    uint8_t reserved[7];
} JetInstrumentPatch;

/**
 * Function: jet_make_instrument_patch
 * Summary: Initializes a JetInstrumentPatch structure safely to zeroed defaults.
 * Parameters:
 *   - None.
 * Returns: A zeroed JetInstrumentPatch struct passed by value.
 * Include: jet64.h
 * Notes: Most carts author patches in the Instrument Studio and play them by index. This builder is for advanced procedural patch construction and owns no external memory.
 * Example:
 *   JetInstrumentPatch patch = jet_make_instrument_patch();
 */
static inline JetInstrumentPatch jet_make_instrument_patch(void) {
    JetInstrumentPatch patch;
    memset(&patch, 0, sizeof(JetInstrumentPatch));
    return patch;
}

// Compile-time checks guarding the ABI layout shared with the Rust mirror.
_Static_assert(sizeof(JetBgmChannelState) == 4, "JetBgmChannelState size mismatch");
_Static_assert(sizeof(JetBgmTrackerChannel) == 8, "JetBgmTrackerChannel size mismatch");
_Static_assert(offsetof(JetBgmTrackerPattern, line_base) == 4,
               "JetBgmTrackerPattern line_base offset (Rust mirror contract)");
_Static_assert(offsetof(JetBgmTrackerPattern, lines) == 6,
               "JetBgmTrackerPattern lines offset (Rust mirror contract)");
_Static_assert(sizeof(JetBgmTrackerPattern) == offsetof(JetBgmTrackerPattern, lines) +
                                                   sizeof(((JetBgmTrackerPattern*)0)->lines),
               "JetBgmTrackerPattern carries tail padding the Rust mirror does not");
_Static_assert(JET_BGM_TRACKER_OFFSET + sizeof(JetBgmTrackerPattern) <= JET_BGM_TRANSPORT_OFFSET,
               "Tracker pattern window overflows into the BGM TRANSPORT block");
_Static_assert(offsetof(JetBgmTransport, traversals) == 4,
               "JetBgmTransport traversals offset (Rust mirror contract)");
_Static_assert(offsetof(JetBgmTransport, title) == 8,
               "JetBgmTransport title offset (Rust mirror contract)");
_Static_assert(offsetof(JetBgmTransport, refused) == 40,
               "JetBgmTransport refused offset (Rust mirror contract)");
_Static_assert(sizeof(JetBgmTransport) == JET_BGM_TRANSPORT_BYTES,
               "JetBgmTransport must fill its block exactly (Rust mirror contract)");
_Static_assert(JET_BGM_TRANSPORT_OFFSET % JET_BGM_TRANSPORT_BYTES == 0,
               "the BGM TRANSPORT block must be aligned to its own size");
_Static_assert(JET_BGM_TRANSPORT_OFFSET + JET_BGM_TRANSPORT_BYTES <= JET_PAL_CMD_OFFSET,
               "BGM TRANSPORT block overflows into the PAL CMD bridge");
_Static_assert(sizeof(JetFmOperator) == 12, "JetFmOperator size mismatch");
_Static_assert(sizeof(JetDrumParams) == 48, "JetDrumParams size mismatch");
_Static_assert(sizeof(JetPatchData) == 48, "JetPatchData size mismatch");
_Static_assert(sizeof(JetAudioRegister) == 64, "JetAudioRegister size mismatch");
_Static_assert(offsetof(JetAudioRegister, seq) == 63, "JetAudioRegister seq byte must stay the final byte (Rust mirror contract)");
_Static_assert(offsetof(JetAudioRegister, strike_pitch_mult) == 32, "JetAudioRegister strike_pitch_mult offset (Rust mirror contract)");
_Static_assert(offsetof(JetAudioRegister, patch_source) == 36, "JetAudioRegister patch_source offset (Rust mirror contract)");
_Static_assert(sizeof(JetInstrumentPatch) == 128, "JetInstrumentPatch size mismatch");
_Static_assert(offsetof(JetInstrumentPatch, stop_fade_ms) == 120, "JetInstrumentPatch stop_fade_ms footer offset (Rust mirror contract)");
_Static_assert(sizeof(JetPcmChannel) == JET_PCM_CHANNEL_BYTES, "JetPcmChannel size mismatch (Rust mirror contract)");
_Static_assert(offsetof(JetPcmChannel, write_cursor) == 32, "JetPcmChannel write_cursor offset (Rust mirror contract)");
_Static_assert(offsetof(JetPcmChannel, read_cursor) == 40, "JetPcmChannel read_cursor offset (Rust mirror contract)");
_Static_assert(JET_PCM_OFFSET + JET_PCM_BLOCK_BYTES <= JET_FONT_OFFSET,
               "PCM control block overflows into the Font ROM");
_Static_assert(sizeof(JetScopeHeader) == JET_SCOPE_HEADER_BYTES,
               "JetScopeHeader size mismatch (Rust mirror contract)");
_Static_assert(offsetof(JetScopeHeader, sample_rate) == 8,
               "JetScopeHeader sample_rate offset (Rust mirror contract)");
_Static_assert(offsetof(JetScopeHeader, write_head) == 16,
               "JetScopeHeader write_head offset (Rust mirror contract)");
_Static_assert(offsetof(JetScopeHeader, output_latency_frames) == 20,
               "JetScopeHeader output_latency_frames offset (Rust mirror contract, E1)");
_Static_assert(offsetof(JetScopeHeader, bgm_row_head) == 24,
               "JetScopeHeader bgm_row_head offset (Rust mirror contract, E2)");
_Static_assert(offsetof(JetScopeHeader, bgm_row_pattern) == 28,
               "JetScopeHeader bgm_row_pattern offset (Rust mirror contract, E2)");
_Static_assert(offsetof(JetScopeHeader, bgm_row_line) == 29,
               "JetScopeHeader bgm_row_line offset (Rust mirror contract, E2)");
_Static_assert(JET_SCOPE_OFFSET == 0x00B04A40,
               "SCOPE block moved — jet64_core::hardware_map::JET_SCOPE_OFFSET must match");
_Static_assert(JET_SCOPE_OFFSET + JET_SCOPE_BLOCK_BYTES <= JET_FONT_OFFSET,
               "AUDIO SCOPE block overflows into the Font ROM");

// NET CMD: every field offset is an ABI the Rust host mirrors byte-for-byte
// (jet64_core::hardware_map NET_CMD_* / NET_HTTP_*), so each one is pinned here
// rather than merely commented. A drift in either language is a build failure.
_Static_assert(sizeof(JetNetCmdHeader) == JET_NET_CMD_HEADER_BYTES,
               "JetNetCmdHeader size mismatch (Rust mirror contract)");
_Static_assert(offsetof(JetNetCmdHeader, version) == 4, "NET CMD version offset");
_Static_assert(offsetof(JetNetCmdHeader, http_slot_count) == 6, "NET CMD http_slot_count offset");
_Static_assert(offsetof(JetNetCmdHeader, ws_slot_count) == 8, "NET CMD ws_slot_count offset");
_Static_assert(offsetof(JetNetCmdHeader, capabilities) == 12, "NET CMD capabilities offset");
_Static_assert(sizeof(JetNetHttpSlot) == JET_NET_HTTP_SLOT_BYTES,
               "JetNetHttpSlot size mismatch (Rust mirror contract)");
_Static_assert(offsetof(JetNetHttpSlot, method) == 4, "NET CMD HTTP method offset");
_Static_assert(offsetof(JetNetHttpSlot, url_len) == 8, "NET CMD HTTP url_len offset");
_Static_assert(offsetof(JetNetHttpSlot, headers_len) == 12, "NET CMD HTTP headers_len offset");
_Static_assert(offsetof(JetNetHttpSlot, body_offset) == 16, "NET CMD HTTP body_offset offset");
_Static_assert(offsetof(JetNetHttpSlot, body_len) == 20, "NET CMD HTTP body_len offset");
_Static_assert(offsetof(JetNetHttpSlot, resp_offset) == 24, "NET CMD HTTP resp_offset offset");
_Static_assert(offsetof(JetNetHttpSlot, resp_cap) == 28, "NET CMD HTTP resp_cap offset");
_Static_assert(offsetof(JetNetHttpSlot, timeout_ms) == 32, "NET CMD HTTP timeout_ms offset");
_Static_assert(offsetof(JetNetHttpSlot, resp_len) == 36, "NET CMD HTTP resp_len offset");
_Static_assert(offsetof(JetNetHttpSlot, http_code) == 40, "NET CMD HTTP http_code offset");
_Static_assert(offsetof(JetNetHttpSlot, error) == 44, "NET CMD HTTP error offset");
_Static_assert(offsetof(JetNetHttpSlot, flags) == 48, "NET CMD HTTP flags offset");
_Static_assert(offsetof(JetNetHttpSlot, abort) == 52, "NET CMD HTTP abort offset");
_Static_assert(sizeof(JetNetWsSlot) == JET_NET_WS_SLOT_BYTES,
               "JetNetWsSlot size mismatch (Rust mirror contract)");
_Static_assert(offsetof(JetNetWsSlot, link) == 4, "NET CMD WS link offset");
_Static_assert(offsetof(JetNetWsSlot, url_len) == 8, "NET CMD WS url_len offset");
_Static_assert(offsetof(JetNetWsSlot, tx_offset) == 12, "NET CMD WS tx_offset offset");
_Static_assert(offsetof(JetNetWsSlot, tx_bytes) == 16, "NET CMD WS tx_bytes offset");
_Static_assert(offsetof(JetNetWsSlot, rx_offset) == 20, "NET CMD WS rx_offset offset");
_Static_assert(offsetof(JetNetWsSlot, rx_bytes) == 24, "NET CMD WS rx_bytes offset");
_Static_assert(offsetof(JetNetWsSlot, tx_head) == 28, "NET CMD WS tx_head offset");
_Static_assert(offsetof(JetNetWsSlot, tx_tail) == 32, "NET CMD WS tx_tail offset");
_Static_assert(offsetof(JetNetWsSlot, rx_head) == 36, "NET CMD WS rx_head offset");
_Static_assert(offsetof(JetNetWsSlot, rx_tail) == 40, "NET CMD WS rx_tail offset");
_Static_assert(offsetof(JetNetWsSlot, error) == 44, "NET CMD WS error offset");
_Static_assert(offsetof(JetNetWsSlot, close_code) == 48, "NET CMD WS close_code offset");
_Static_assert(offsetof(JetNetWsSlot, close) == 52, "NET CMD WS close offset");
_Static_assert(JET_NET_WS_FRAME_HEADER_BYTES % 4u == 0u,
               "a ring frame header must keep every frame 4-byte aligned");
_Static_assert(JET_NET_CMD_WS_TABLE_OFFSET + JET_NET_WS_SLOTS * JET_NET_WS_SLOT_BYTES
                   <= JET_NET_CMD_STAGE_OFFSET,
               "NET CMD descriptor tables overlap the request staging table");
_Static_assert(JET_NET_CMD_STAGE_OFFSET + JET_NET_HTTP_SLOTS * JET_NET_CMD_STAGE_SLOT_BYTES
                   <= JET_NET_CMD_WS_STAGE_OFFSET,
               "NET CMD HTTP request staging overlaps the WebSocket URL staging");
_Static_assert(JET_NET_CMD_WS_STAGE_OFFSET + JET_NET_WS_SLOTS * JET_NET_CMD_WS_STAGE_URL_MAX
                   <= JET_NET_CMD_BLOCK_BYTES,
               "NET CMD request staging overflows the block");
_Static_assert(offsetof(JetNetCmdHeader, dial_slot_count) == 10, "NET CMD dial_slot_count offset");
_Static_assert(offsetof(JetNetCmdHeader, rtc_slot_count) == 16, "NET CMD rtc_slot_count offset");
_Static_assert(sizeof(JetNetDialSlot) == JET_NET_DIAL_SLOT_BYTES, "NET CMD dial slot size");
_Static_assert(offsetof(JetNetDialSlot, state) == 0, "NET CMD dial state offset");
_Static_assert(offsetof(JetNetDialSlot, kind) == 4, "NET CMD dial kind offset");
_Static_assert(offsetof(JetNetDialSlot, host_len) == 8, "NET CMD dial host_len offset");
_Static_assert(offsetof(JetNetDialSlot, port) == 12, "NET CMD dial port offset");
_Static_assert(offsetof(JetNetDialSlot, answer) == 16, "NET CMD dial answer offset");
_Static_assert(offsetof(JetNetDialSlot, abort) == 20, "NET CMD dial abort offset");
_Static_assert(offsetof(JetNetDialSlot, reserved) == 24, "NET CMD dial reserved offset");
_Static_assert(JET_NET_CMD_DIAL_TABLE_OFFSET + JET_NET_DIAL_SLOTS * JET_NET_DIAL_SLOT_BYTES
                   <= JET_NET_CMD_STAGE_OFFSET,
               "NET CMD dial table overlaps the request staging table");
_Static_assert(sizeof(JetNetRtcSlot) == JET_NET_RTC_SLOT_BYTES, "NET CMD RTC slot size");
_Static_assert(offsetof(JetNetRtcSlot, state) == 0, "NET CMD RTC state offset");
_Static_assert(offsetof(JetNetRtcSlot, link) == 4, "NET CMD RTC link offset");
_Static_assert(offsetof(JetNetRtcSlot, role) == 8, "NET CMD RTC role offset");
_Static_assert(offsetof(JetNetRtcSlot, policy) == 12, "NET CMD RTC policy offset");
_Static_assert(offsetof(JetNetRtcSlot, ice_count) == 16, "NET CMD RTC ice_count offset");
_Static_assert(offsetof(JetNetRtcSlot, ice_bytes) == 20, "NET CMD RTC ice_bytes offset");
_Static_assert(offsetof(JetNetRtcSlot, local_offset) == 24, "NET CMD RTC local_offset offset");
_Static_assert(offsetof(JetNetRtcSlot, remote_offset) == 28, "NET CMD RTC remote_offset offset");
_Static_assert(offsetof(JetNetRtcSlot, description_cap) == 32,
               "NET CMD RTC description_cap offset");
_Static_assert(offsetof(JetNetRtcSlot, local_len) == 36, "NET CMD RTC local_len offset");
_Static_assert(offsetof(JetNetRtcSlot, remote_len) == 40, "NET CMD RTC remote_len offset");
_Static_assert(offsetof(JetNetRtcSlot, tx_offset) == 44, "NET CMD RTC tx_offset offset");
_Static_assert(offsetof(JetNetRtcSlot, tx_bytes) == 48, "NET CMD RTC tx_bytes offset");
_Static_assert(offsetof(JetNetRtcSlot, rx_offset) == 52, "NET CMD RTC rx_offset offset");
_Static_assert(offsetof(JetNetRtcSlot, rx_bytes) == 56, "NET CMD RTC rx_bytes offset");
_Static_assert(offsetof(JetNetRtcSlot, tx_head) == 60, "NET CMD RTC tx_head offset");
_Static_assert(offsetof(JetNetRtcSlot, tx_tail) == 64, "NET CMD RTC tx_tail offset");
_Static_assert(offsetof(JetNetRtcSlot, rx_head) == 68, "NET CMD RTC rx_head offset");
_Static_assert(offsetof(JetNetRtcSlot, rx_tail) == 72, "NET CMD RTC rx_tail offset");
_Static_assert(offsetof(JetNetRtcSlot, error) == 76, "NET CMD RTC error offset");
_Static_assert(offsetof(JetNetRtcSlot, path) == 80, "NET CMD RTC path offset");
_Static_assert(offsetof(JetNetRtcSlot, rtt_ms) == 84, "NET CMD RTC rtt_ms offset");
_Static_assert(offsetof(JetNetRtcSlot, close) == 88, "NET CMD RTC close offset");
_Static_assert(offsetof(JetNetRtcSlot, reserved) == 92, "NET CMD RTC reserved offset");
_Static_assert(JET_NET_CMD_RTC_STAGE_OFFSET + JET_NET_RTC_SLOTS * JET_NET_CMD_RTC_STAGE_BYTES
                   <= JET_NET_CMD_BLOCK_BYTES,
               "NET CMD RTC staging overflows the block");
_Static_assert(JET_NET_CMD_OFFSET == 0x00A88980,
               "NET CMD block moved — jet64_core::hardware_map::JET_NET_CMD_OFFSET must match");
_Static_assert(JET_NET_CMD_OFFSET + JET_NET_CMD_BLOCK_BYTES <= JET_IO_OFFSET,
               "NET CMD block overflows into the IO register file");
_Static_assert(JET_LAUNCH_OFFSET == 0x00A90980,
               "LAUNCH block moved — jet64_core::hardware_map::JET_LAUNCH_OFFSET must match");
_Static_assert(JET_LAUNCH_OFFSET + JET_LAUNCH_BLOCK_BYTES <= JET_IO_OFFSET,
               "LAUNCH block overflows into the IO register file");
_Static_assert(JET_LAUNCH_BYTES_OFFSET + 4 <= JET_LAUNCH_HEADER_BYTES,
               "the LAUNCH header's fields overflow the header");
_Static_assert(JET_LAUNCH_OFFSET % 4 == 0, "the LAUNCH header's words must be 4-byte aligned");

#define IPC_TICK_BYTE       0x01
#define IPC_QUIT_BYTE       0x02
#define IPC_DONE_BYTE       0xFF

// Host Command Definitions
#define JET_HOST_CMD_SET_SFX_VOLUME 1
#define JET_HOST_CMD_SET_BGM_VOLUME 2
#define JET_HOST_CMD_SET_FULLSCREEN 3
// jet_host_set_resolution: Fixed n× for the cart's session on every host, or 0 to hand back to
// the player's mode (docs/project_export/presentation_repair.md R13).
#define JET_HOST_CMD_SET_RESOLUTION 4
#define JET_HOST_CMD_EXIT 5
#define JET_HOST_CMD_SET_VSYNC 6
#define JET_HOST_CMD_SCREENSHOT 7
#define JET_HOST_CMD_SET_VIDEO_BACKEND 8
// Opens a subfolder of the cart's AppData sandbox in the OS file manager. The
// guest stages the subfolder name (a string arg); the host validates it as a
// safe relative subpath and joins it onto the host-derived AppData root, so the
// cart can only ever reveal a folder inside its own sandbox. Native-only; the
// web player ACKs and ignores it (see jet_host_reveal_dir).
#define JET_HOST_CMD_REVEAL_DIR 9
// Asks the host to let the player import files into a subfolder of the cart's
// AppData sandbox (docs/file_import.md). The guest stages "subdir|exts" (a
// string arg); the host validates the subfolder as a safe relative subpath.
// A native host opens the OS multi-file picker; web raises a drop-zone/browse
// overlay. Completion is signalled per-file via SYS[SYS_IMPORT_GENERATION]
// (see jet_host_import_files / jet_host_import_generation).
#define JET_HOST_CMD_IMPORT_FILES 10
// Enables or disables display-sleep inhibition for this cartridge. Host-owned
// and idempotent; teardown releases an enabled assertion automatically.
#define JET_HOST_CMD_KEEP_AWAKE 11
// Moves UTF-8 text between WRAM and the system clipboard (jet_clipboard.h,
// docs/jet_text/design.md §2.2): SET puts the cart's bytes on it (copy, cut),
// GET writes its text into the cart's buffer (paste). Neither carries a string,
// so their span is laid over the string argument (JET_HOST_CMD_CLIPBOARD_*_ARG
// below). A host honours each only just after the player's own copy or paste
// chord, and only for a span inside WRAM.
#define JET_HOST_CMD_CLIPBOARD_SET 12
#define JET_HOST_CMD_CLIPBOARD_GET 13
// jet_host_step_scale: the player's own Ctrl/Cmd+Shift+S, pressed through the cart — one step of
// the picture-scale ring, remembered as the player's choice where each host already keeps it
// (docs/project_export/presentation_repair.md R14). It carries no argument.
#define JET_HOST_CMD_STEP_SCALE 14
// Mouse visibility is NOT a host command: it is driven via SYS[SYS_MOUSE_VISIBLE].

#define JET_HOST_CMD_FLOAT_ARG 4
#define JET_HOST_CMD_INT_ARG 8
// NUL-terminated string arg (e.g. jet_host_reveal_dir's subfolder), staged
// after the scalar args and running to the block end. The block is 256 bytes;
// the last byte is reserved for the terminator.
#define JET_HOST_CMD_STR_ARG 12
#define JET_HOST_CMD_BLOCK_BYTES 256
#define JET_HOST_CMD_STR_MAX (JET_HOST_CMD_BLOCK_BYTES - JET_HOST_CMD_STR_ARG - 1)

// The clipboard's span, four little-endian u32s laid over the string argument
// (mirrors jet64_core::ipc HOST_CMD_CLIPBOARD_*_OFFSET). WRAM is the text's
// offset from WRAM's first byte, and LEN its length (SET) or the buffer's
// capacity (GET). STATUS is the JetClipboardStatus the host answers: the guest
// stages JET_CLIPBOARD_REFUSED there first, because a player built without a
// clipboard acknowledges a command it does not know and leaves the block as it
// was staged. NEEDED is GET's answered length: the bytes written, or the bytes
// a buffer too small would have needed.
#define JET_HOST_CMD_CLIPBOARD_WRAM_ARG 12
#define JET_HOST_CMD_CLIPBOARD_LEN_ARG 16
#define JET_HOST_CMD_CLIPBOARD_STATUS_ARG 20
#define JET_HOST_CMD_CLIPBOARD_NEEDED_ARG 24
_Static_assert(JET_HOST_CMD_CLIPBOARD_WRAM_ARG == JET_HOST_CMD_STR_ARG,
               "the clipboard's span is laid over the string argument");
_Static_assert(JET_HOST_CMD_CLIPBOARD_NEEDED_ARG + sizeof(uint32_t) <= JET_HOST_CMD_BLOCK_BYTES,
               "the clipboard's span lies inside the HOST command block");

// BGM Command Definitions (mirrors jet64_core::ipc BGM_CMD_*)
#define JET_BGM_CMD_PLAY_MOD 1
#define JET_BGM_CMD_STOP 2
// Pause/resume freeze the tracker playhead in place through the host's
// transport freeze gate (declick-clean, resumes from the exact frame).
#define JET_BGM_CMD_PAUSE 3
#define JET_BGM_CMD_RESUME 4
// Seek jumps the song cursor to a (order, row) staged at
// JET_BGM_CMD_SEEK_ORDER_OFFSET and JET_BGM_CMD_SEEK_ROW_OFFSET — the E3
// transport verb (docs/pulsar/design.md §10.3). The host clamps both to the
// playing song and hard-cuts sounding voices through the existing declick
// path; out-of-range values land on the nearest valid row.
#define JET_BGM_CMD_SEEK 5

// The command ids of the palette, map and asset bridges (mirrors jet64_core::ipc
// PAL_CMD_LOAD, MAP_CMD_INJECT and ASSET_CMD_INJECT). Each bridge keeps its own
// registry, so the three share a number without sharing a meaning.
#define JET_PAL_CMD_LOAD 1
#define JET_MAP_CMD_INJECT 1
#define JET_ASSET_CMD_INJECT 1

// The value byte 0 of every one-byte command bridge holds while no command is
// pending, and the one a host's acknowledgement leaves there. Mirrors
// jet64_core::ipc::HOST_CMD_NONE, which every bridge shares.
#define JET_HOST_CMD_NONE 0

// Where the BGM, PAL, MAP and ASSET bridges stage their arguments (mirrors
// jet64_core::ipc IPC_ARG_OFFSET, IPC_FILENAME_OFFSET and IPC_COMMAND_BYTES).
// Every bridge's arguments start at JET_IPC_ARG_OFFSET, straight after the
// command byte. PAL stages its palette name there. BGM, MAP and ASSET stage a
// little-endian u32 there instead — BGM a play's ticket, MAP and ASSET the
// WRAM offset out and the loaded length back — so a path they stage starts
// after it, at JET_IPC_FILENAME_OFFSET. A staged string always ends by
// JET_IPC_COMMAND_BYTES, the block's last byte, which holds its terminator.
#define JET_IPC_ARG_OFFSET 1
#define JET_IPC_FILENAME_OFFSET 5
#define JET_IPC_COMMAND_BYTES 255
_Static_assert(JET_IPC_ARG_OFFSET + sizeof(uint32_t) <= JET_IPC_FILENAME_OFFSET,
               "a bridge's u32 argument must end before the path staged after it");
_Static_assert(JET_IPC_COMMAND_BYTES == JET_IPC_CMD_BLOCK_BYTES - 1,
               "a staged string's terminator must be the IPC command block's last byte");

// The BGM bridge's own arguments (mirrors jet64_core::ipc BGM_TICKET_OFFSET
// and BGM_SEEK_*_OFFSET). PLAY_MOD stages its ticket at the first argument
// byte and the module's name at JET_IPC_FILENAME_OFFSET. No other BGM command
// writes the ticket's bytes, and no boot clears a bridge's arguments, so
// between plays they hold the last ticket issued: they are jet_play_mod's
// counter. SEEK stages its (order, row) after them, so a seek never disturbs it.
#define JET_BGM_CMD_TICKET_OFFSET 1
#define JET_BGM_CMD_SEEK_ORDER_OFFSET 5
#define JET_BGM_CMD_SEEK_ROW_OFFSET 6
_Static_assert(JET_BGM_CMD_TICKET_OFFSET == JET_IPC_ARG_OFFSET,
               "PLAY_MOD stages its ticket where every bridge's u32 argument goes");
_Static_assert(JET_BGM_CMD_SEEK_ORDER_OFFSET >= JET_BGM_CMD_TICKET_OFFSET + sizeof(uint32_t),
               "a seek must never overwrite jet_play_mod's ticket counter");

#define JET_VIDEO_BACKEND_CPU 0
#define JET_VIDEO_BACKEND_NEON 1

// Canonical guest simulation rate. The runtime advances the cartridge at this
// fixed timestep on every host — the workstation ping-pong loop and both the
// native and web exported players alike — mirroring the authoritative
// jet64_core::hardware_map::GUEST_TICK_HZ. Time-based runtime logic derives its
// per-tick seconds from JET_GUEST_TICK_SECONDS so the rate lives in one place
// and can never drift into "120 in three forms".
#define JET_GUEST_TICK_HZ      120.0f
#define JET_GUEST_TICK_SECONDS (1.0f / JET_GUEST_TICK_HZ)

// SYS Register Safe-Access Memory Mapping Indices
#define SYS_READY_BUFFER       0
#define SYS_HOST_LOCKED_BUFFER 1
#define SYS_FPS                2
#define SYS_UNCAPPED_FPS       3
#define SYS_VSYNC              4
#define SYS_BUFFERING_MODE     5
#define SYS_ORIENTATION        6
#define SYS_MOUSE_TRAP         7
#define SYS_PAL_DIRTY          8
#define SYS_VIDEO_BACKEND_STATUS 9
#define SYS_SCREEN_WIDTH       10
#define SYS_SCREEN_HEIGHT      11
#define SYS_WARM_BOOT          12
#define SYS_FULLSCREEN         13
#define SYS_MOUSE_VISIBLE      14
#define SYS_PLATFORM           15
#define SYS_IS_WORKSTATION     16
#define SYS_WRAM_USAGE_BYTES   17
// Host-written monotonic counter, bumped once per file that lands in the
// AppData sandbox through the import surface (picker or drag-and-drop). Carts
// poll it cheaply each frame and re-enumerate folders only on change — the
// import surfaces (a web overlay in particular) complete asynchronously, so
// the verb's ACK alone does not mean files have arrived.
#define SYS_IMPORT_GENERATION  18
// Guest-written cumulative microsecond counters, one per cart hook: how long
// _update() and _draw() have taken since this guest booted. Only the workstation
// guest's ping-pong loop writes them, and only the workstation host reads them —
// as wrapping deltas over its own telemetry window, which is why they are allowed
// to be cumulative and to wrap. They are telemetry, never instrumentation: three
// clock reads a tick, nothing attributed, nothing hashed and nothing journalled
// (docs/profiling/dashboard_and_report.md §5). In an exported player they exist
// in the map, hold zero, and are read by nothing.
#define SYS_UPDATE_MICROS      19
#define SYS_DRAW_MICROS        20
// Host-written once at boot: the kind of keyboard the person has, one of the
// JET_KEYBOARD_* values in jet_input.h. The Workstation and the native player
// state the platform they were built for, the web player what the browser says
// it runs on, and the agent door whatever its scenario names (a PC unless one
// names another). jet_key_command_down() and jet_key_name() read it; the guest's
// own boot leaves it alone (docs/jet_text/design.md §2.1).
#define SYS_KEYBOARD_KIND      21
// A rest (jet_host_rest, docs/idle_energy.md §5): the cart's promise that its
// next ticks would change nothing but the time, and the host's answer.
// SYS_REST_TICKS is the guest's: the loop that runs ticks clears it as each tick
// begins, jet_host_rest writes the ticks it promises during _update, and the
// host reads it after the frame's draw — so a rest holds only for the tick it
// was asked on, however many ticks a host granted at once. SYS_RESTED_TICKS is
// the host's: the ticks it skipped, resting, written before the tick it wakes to
// run and set back to 0 before the next. A host that never rests never writes
// it, so it reads 0 — the honest answer from a host that ignored the rest.
#define SYS_REST_TICKS         22
#define SYS_RESTED_TICKS       23
// Host-written: 1 while the person cannot see the player — its window minimised,
// its browser tab or page hidden, the Workstation's own window minimised — and 0
// while they can. jet_visibility_hidden() reads it (jet_visibility.h). A host
// that cannot tell leaves it 0, the honest answer of a picture it believes is on
// screen; the agent door writes what its scenario says.
#define SYS_HIDDEN             24

// The SYS block is JET_SYS_BYTES wide and addressed as uint32_t, so this is the
// number of slots a SYS_* index may name — the C mirror of
// jet64_core::hardware_map::SYS_REGISTER_COUNT. The highest index declared above
// must lie inside it, exactly as the Rust side asserts of its own last register.
#define JET_SYS_REGISTER_COUNT ((JET_KBD_OFFSET - JET_SYS_OFFSET) / (int)sizeof(uint32_t))
_Static_assert(SYS_HIDDEN < JET_SYS_REGISTER_COUNT,
               "the last named SYS register must lie inside the SYS block");

/**
 * Constant: JET_PLATFORM_UNKNOWN
 * Summary: Indicates that the running host platform could not be identified.
 * Include: jet64.h
 * Notes: May be returned by jet_host_get_platform. Keep a portable fallback path rather than assuming a desktop operating system.
 * Example:
 *   bool platform_known = jet_host_get_platform() != JET_PLATFORM_UNKNOWN;
 */
#define JET_PLATFORM_UNKNOWN  0
/**
 * Constant: JET_PLATFORM_WINDOWS
 * Summary: Identifies a native Windows build.
 * Include: jet64.h
 * Notes: Compare with jet_host_get_platform only when behaviour genuinely differs by host; gameplay should normally remain platform-independent.
 * Example:
 *   bool on_windows = jet_host_get_platform() == JET_PLATFORM_WINDOWS;
 */
#define JET_PLATFORM_WINDOWS  1
/**
 * Constant: JET_PLATFORM_MACOS
 * Summary: Identifies a native macOS build.
 * Include: jet64.h
 * Notes: Compare with jet_host_get_platform only when behaviour genuinely differs by host.
 * Example:
 *   bool on_macos = jet_host_get_platform() == JET_PLATFORM_MACOS;
 */
#define JET_PLATFORM_MACOS    2
/**
 * Constant: JET_PLATFORM_LINUX
 * Summary: Identifies a native Linux build.
 * Include: jet64.h
 * Notes: Compare with jet_host_get_platform only when behaviour genuinely differs by host.
 * Example:
 *   bool on_linux = jet_host_get_platform() == JET_PLATFORM_LINUX;
 */
#define JET_PLATFORM_LINUX    3
/**
 * Constant: JET_PLATFORM_WEB
 * Summary: Identifies the browser-based Web player.
 * Include: jet64.h
 * Notes: Equivalent to jet_host_is_web. Use it for genuine browser differences such as unavailable native-only networking or keyboard affordances.
 * Example:
 *   bool in_browser = jet_host_get_platform() == JET_PLATFORM_WEB;
 */
#define JET_PLATFORM_WEB      4

// ===== Input Hardware Protocol (mirrors jet64_core::hardware_map) =====
// Every IO/KBD input byte has exactly ONE writer (the host); the guest only reads.
// IO register byte layout (16-byte block at JET_IO_OFFSET):
// NOTE: the IO mouse bytes are a LEGACY MIRROR (Left/Right/Middle only); the
// full first-class mouse device lives in the MOUSE block below.
#define JET_IO_GAMEPAD_P1           0
#define JET_IO_MOUSE_BUTTONS        1   // bit per button: held level
#define JET_IO_MOUSE_X              4   // f32 LE
#define JET_IO_MOUSE_Y              8   // f32 LE
#define JET_IO_MOUSE_PRESS_COUNTERS 12  // one press-counter byte per button
#define JET_IO_MOUSE_LEGACY_BUTTON_COUNT 3

// KBD[0..256): one byte per Jet key code.
// bit 0 = held level; bits 1-7 = press counter incremented by the host on every
// press edge. Edge queries are derived from counter deltas, so a tap shorter
// than one guest tick can never be lost.
#define JET_KBD_KEY_STATE_BYTES     256
#define JET_KBD_KEY_LEVEL_MASK      0x01
#define JET_KBD_KEY_COUNTER_SHIFT   1
// Press counters occupy 7 bits and wrap; only inequality is meaningful.
#define JET_KBD_KEY_COUNTER_MASK    0x7F

// The held level and the shifted press counter must tile the key byte exactly:
// that is the whole encoding, so a drift in either constant is a build failure
// rather than a lost edge. jet64_core::hardware_map asserts the same invariant
// over its KBD_KEY_* mirror.
_Static_assert((JET_KBD_KEY_LEVEL_MASK & (JET_KBD_KEY_COUNTER_MASK << JET_KBD_KEY_COUNTER_SHIFT)) == 0,
               "KBD key byte: press counter overlaps the held-level bit");
_Static_assert((JET_KBD_KEY_LEVEL_MASK | (JET_KBD_KEY_COUNTER_MASK << JET_KBD_KEY_COUNTER_SHIFT)) == 0xFF,
               "KBD key byte: level bit + press counter must tile all 8 bits (Rust mirror contract)");

// KBD[256..512): typed-character ring. KBD[256] is the host-owned tail index;
// the guest keeps a private head and drains [head, tail).
#define JET_KBD_TEXT_TAIL_INDEX     256
#define JET_KBD_TEXT_RING_INDEX     257
#define JET_KBD_TEXT_RING_CAPACITY  255

// Gamepad hardware block: 4 pads × 64 bytes immediately after KBD, host-owned.
// Each button byte reuses the keyboard encoding (bit 0 = held level, bits 1-7 =
// press counter) so the guest derives edges from counter deltas — sub-tick taps
// are never lost. Axes are f32 LE: sticks -1..1 (screen-convention Y: up = -1),
// triggers 0..1. Cart-facing button/axis indices live in jet_gamepad.h.
#define JET_GPAD_OFFSET        0x00B00690
#define JET_GPAD_MAX_PADS      4
#define JET_GPAD_BLOCK_STRIDE  64
#define JET_GPAD_CONNECTED     0   // u8: 1 = pad present
#define JET_GPAD_BUTTONS       2   // 16 button bytes (level | counter)
#define JET_GPAD_BUTTON_COUNT  16
#define JET_GPAD_AXES          20  // 6 × f32 LE
#define JET_GPAD_AXIS_COUNT    6
// The identity tail, in the twenty bytes the buttons and axes leave spare at
// the end of every pad slot: a u32 LE hash of the controller's own description
// (0 = no pad), then a short NUL-terminated display name. One place decides
// both for all three runtimes (jet64_core::gamepad), so a cart can key a saved
// control profile on the CONTROLLER rather than on the slot it happened to
// land in. Cart-facing readers are jet_pad_id and jet_pad_name.
#define JET_GPAD_ID            44  // u32 LE, 0 = no controller in this slot
#define JET_GPAD_NAME          48  // NUL-terminated, NUL-padded display name
#define JET_GPAD_NAME_BYTES    16

_Static_assert(JET_GPAD_AXES + JET_GPAD_AXIS_COUNT * 4 <= JET_GPAD_ID,
               "GPAD identity must start after the axes it shares a slot with");
_Static_assert(JET_GPAD_NAME + JET_GPAD_NAME_BYTES <= JET_GPAD_BLOCK_STRIDE,
               "GPAD identity must fit inside one pad's 64-byte slot");

// Mouse hardware block: first-class mouse device immediately after GAMEPADS,
// host-owned. Button bytes reuse the keyboard encoding (bit 0 = held level,
// bits 1-7 = press counter). Wheel accumulators are f32 LE running totals in
// notches (vertical: positive = up / away from the user; horizontal: positive
// = right) — the guest derives per-tick deltas, mirroring the counter scheme.
// Cart-facing button indices and wheel queries live in jet_input.h.
#define JET_MOUSE_OFFSET       0x00B00790
#define JET_MOUSE_BUTTONS      0   // 8 button bytes (level | counter)
#define JET_MOUSE_MAX_BUTTONS  8
#define JET_MOUSE_BUTTON_COUNT 5   // Left, Right, Middle, X1, X2
#define JET_MOUSE_X            8   // f32 LE, game-display pixels
#define JET_MOUSE_Y            12  // f32 LE, game-display pixels
#define JET_MOUSE_WHEEL_Y      16  // f32 LE scroll accumulator (notches)
#define JET_MOUSE_WHEEL_X      20  // f32 LE scroll accumulator (notches)

// ===== Audio master-volume register (guest-written, host-read) =====
// Lock-free single-writer register: the cart's BGM/SFX master gains as f32 LE
// (0.0..1.0). The audio-extension fade engine and jet_host_set_volume_* store
// here with a plain aligned f32 write — NO HOST_CMD trigger-wait-ACK handshake
// — so riding the master through a multi-second fade is free and never
// busy-spins the guest core. The host samples both floats once per frame into
// its volume atomics and clamps to 0.0..1.0. (Mirrors jet64_core::hardware_map.)
#define JET_AUDIO_MASTER_OFFSET 0x00B007B0
#define JET_AUDIO_MASTER_BGM 0   // f32 LE
#define JET_AUDIO_MASTER_SFX 4   // f32 LE
// Which Amiga the cart's .mod music was made on (jet_mod_set_machine, jet_audio.h),
// u8, in the block's reserved headroom. The host reads it once, when it starts a
// song — a reconstruction is chosen per play, not sampled per frame. Zero means
// no machine, so a cart that never names one changes nothing.
#define JET_AUDIO_MASTER_MOD_MACHINE 8 // u8, JET_MOD_MACHINE_* (jet_audio.h)
#define JET_AUDIO_MASTER_BLOCK_BYTES 16
#define JET_AUDIO_MASTER_DEFAULT 1.0f // unity gain boot/reset default
// The .mod machine the register is reset to at boot: none (jet_audio.h's JET_MOD_MACHINE_NONE,
// which jet_audio.h asserts equal). Firmware resets the register by these values, never by the
// audio module's names, because firmware never includes an opt-in module.
#define JET_AUDIO_MASTER_MOD_MACHINE_RESET 0u

// The master-volume block must stay inside the reserved gap before the Font ROM.
_Static_assert(JET_AUDIO_MASTER_OFFSET + JET_AUDIO_MASTER_BLOCK_BYTES <= JET_FONT_OFFSET,
               "Audio master-volume block overflows into the Font ROM");

// Hardware pointers (populated by jet64.c). The audio blocks' are not here: they belong to the
// opt-in audio modules that use them (jet_audio.h, jet_pcm.h, jet_scope.h), each an expression
// over JET_BASE, so firmware names none of them.
extern uint8_t* JET_BASE;
extern uint8_t* ANIM_BANK_PTR;
extern uint8_t* ANIM_FRAMES_PTR; // JetAnimFrame table a TABLE animation indexes into (see jet_sprite.h)
extern uint8_t* VRAM;
extern uint8_t* SPRITES; // 8bpp sprite color data (1024x1024 banks; see jet_sprite.h)
extern uint8_t* MASKS;   // 1bpp sprite transparency masks
extern volatile uint8_t* PAL_CMD; // IPC bridge for dynamic palette loading
extern uint8_t* PALETTE; // Live hardware color LUT (256 × RGBA)
extern volatile uint8_t* MAP_CMD; // IPC bridge for dynamic map loading into WRAM
extern volatile uint8_t* ASSET_CMD; // IPC bridge for generic asset loading into WRAM
extern volatile uint8_t* HOST_CMD;  // IPC bridge for host system commands
extern volatile uint8_t* IO;     // Legacy P1 bitmask + legacy mouse mirror
extern volatile uint32_t* SYS;  // System-state registers (indexed by SYS_* below)
extern volatile uint8_t* KBD;   // Keyboard key states + typed-char ring
extern volatile uint8_t* GPAD;  // Gamepad block: 4 pads × 64 bytes (see jet_gamepad.h)
extern volatile uint8_t* MOUSE; // Mouse block: buttons, position, wheel (see jet_input.h)
extern uint32_t* COPPER; // Per-scanline 24-bit background-colour table, 0x00RRGGBB (JET_COPPER_TABLE_OFFSET); guest-written, host-read
extern uint8_t* ROM;     // Font ROM cache (starts at JET_FONT_OFFSET)
extern uint8_t* WRAM;    // User working RAM (64 MiB)

// Adds one tick's cart-hook cost to SYS_UPDATE_MICROS / SYS_DRAW_MICROS from the
// three stopwatch readings the workstation guest's loop takes around the hooks.
//
// Unsigned subtraction is modular, so a span whose two readings straddle the
// stopwatch's own 32-bit wrap still yields the true elapsed microseconds; the
// cumulative registers then wrap in the same arithmetic, and the host — which
// only ever takes deltas — reads across that wrap correctly too. There is no
// saturation and no branch here on purpose: the whole cost of the telemetry must
// stay in the noise of an 8.333 ms tick.
//
// Internal: Runtime implementation only. The loop in jet64.c is the sole caller;
// the headless gate (tests/runtime/jet_hook_telemetry.c) drives this very
// arithmetic so the published counters and the tested ones cannot differ.
static inline void _jet_publish_hook_micros(uint32_t update_started,
                                            uint32_t draw_started,
                                            uint32_t draw_ended) {
    SYS[SYS_UPDATE_MICROS] += draw_started - update_started;
    SYS[SYS_DRAW_MICROS] += draw_ended - draw_started;
}

// Legacy P1 button selectors for btn(), btnp() and btnr(). These read the
// composed virtual pad: keyboard binds plus every connected physical gamepad.
/**
 * Constant: BTN_UP
 * Summary: Selects the virtual player-one Up direction.
 * Include: jet64.h
 * Notes: Pass to btn for held state, btnp for a press this tick, or btnr for a release this tick.
 * Example:
 *   if (btn(BTN_UP)) player_y--;
 */
#define BTN_UP     0
/**
 * Constant: BTN_DOWN
 * Summary: Selects the virtual player-one Down direction.
 * Include: jet64.h
 * Notes: Pass to btn, btnp or btnr; the state includes configured keyboard binds and connected physical pads.
 * Example:
 *   if (btn(BTN_DOWN)) player_y++;
 */
#define BTN_DOWN   1
/**
 * Constant: BTN_LEFT
 * Summary: Selects the virtual player-one Left direction.
 * Include: jet64.h
 * Notes: Pass to btn, btnp or btnr; the state includes configured keyboard binds and connected physical pads.
 * Example:
 *   if (btn(BTN_LEFT)) player_x--;
 */
#define BTN_LEFT   2
/**
 * Constant: BTN_RIGHT
 * Summary: Selects the virtual player-one Right direction.
 * Include: jet64.h
 * Notes: Pass to btn, btnp or btnr; the state includes configured keyboard binds and connected physical pads.
 * Example:
 *   if (btn(BTN_RIGHT)) player_x++;
 */
#define BTN_RIGHT  3
/**
 * Constant: BTN_A
 * Summary: Selects the virtual player-one primary action button.
 * Include: jet64.h
 * Notes: btnp(BTN_A) is usually preferable for one-shot actions such as jump, confirm or fire-on-press.
 * Example:
 *   bool jump_pressed = btnp(BTN_A);
 */
#define BTN_A      4
/**
 * Constant: BTN_B
 * Summary: Selects the virtual player-one secondary action button.
 * Include: jet64.h
 * Notes: btnp(BTN_B) is usually preferable for one-shot actions such as cancel or alternate fire.
 * Example:
 *   bool cancel_pressed = btnp(BTN_B);
 */
#define BTN_B      5
/**
 * Constant: BTN_START
 * Summary: Selects the virtual player-one Start/menu button.
 * Include: jet64.h
 * Notes: Use btnp for a single menu toggle rather than repeating while the button is held.
 * Example:
 *   bool pause_pressed = btnp(BTN_START);
 */
#define BTN_START  6
/**
 * Constant: BTN_SELECT
 * Summary: Selects the virtual player-one Select/back button.
 * Include: jet64.h
 * Notes: Use btnp for one-shot selection changes and btn for held behaviour.
 * Example:
 *   bool select_pressed = btnp(BTN_SELECT);
 */
#define BTN_SELECT 7
#define JET_P1_BUTTON_COUNT 8

// Guest-owned composed P1 state, refreshed once per tick by _jet_input_hw_update:
// host-written IO[0] level byte ∨ terminal-remappable binds ∨ every connected
// physical gamepad (d-pad, left stick, A/B/Start/Select). Reading the guest-side
// snapshot instead of IO[0] makes btn() immune to host rewrites mid-tick.
extern uint8_t _jet_p1_level;
extern uint8_t _jet_p1_pressed;
extern uint8_t _jet_p1_released;

/**
 * Function: btn
 * Summary: Reports whether a virtual player-one button is currently held.
 * Parameters:
 *   - b: BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_START or BTN_SELECT.
 * Returns: true while the composed keyboard/gamepad button is held; otherwise false.
 * Include: jet64.h
 * Notes: Use btnp for a one-tick press edge and btnr for a one-tick release edge. The argument is evaluated once.
 * Example:
 *   if (btn(BTN_LEFT)) player_x--;
 */
#define btn(b)  ((_jet_p1_level    & (1u << (b))) != 0)
/**
 * Function: btnp
 * Summary: Reports whether a virtual player-one button was pressed during the current tick.
 * Parameters:
 *   - b: BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_START or BTN_SELECT.
 * Returns: true for one update tick per captured press; otherwise false.
 * Include: jet64.h
 * Notes: Short taps are retained by the input counters, so a press between guest ticks is not lost. Use for one-shot actions.
 * Example:
 *   bool jump_pressed = btnp(BTN_A);
 */
#define btnp(b) ((_jet_p1_pressed  & (1u << (b))) != 0)
/**
 * Function: btnr
 * Summary: Reports whether a virtual player-one button was released during the current tick.
 * Parameters:
 *   - b: BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_START or BTN_SELECT.
 * Returns: true for the update tick in which the composed button became released; otherwise false.
 * Include: jet64.h
 * Notes: Use when an action should occur on release rather than while held or on the initial press.
 * Example:
 *   bool charge_released = btnr(BTN_B);
 */
#define btnr(b) ((_jet_p1_released & (1u << (b))) != 0)

// ===== Copper per-scanline background colour (see docs/copper.md) =====
// The host paints COPPER[row] wherever a framebuffer pixel is palette index 0 and
// the Copper is enabled — the faithful Amiga COLOR00 model. The cart owns the
// table: write it with plain stores (typically rebuilt every _draw, like a real
// Copper list), toggle it live with jet_copper_enable(). Both gates must be true
// for Copper to show: this enable byte AND the project's Copper capability (set in
// the Shader Lab). With the capability off these stores are harmless no-ops on the
// resolve. Index 0 in art becomes "see-through to Copper", so reserve a non-zero
// palette slot for an opaque black when Copper is on (§13).
//
// A Copper colour is 24-bit TRUECOLOUR — the one place the machine steps outside
// its 256-entry palette, and deliberately so. The Copper is the only surface whose
// colour is a function of the ROW ALONE, which makes it the only place a palette
// cannot help: a sky spread down 500 scanlines needs 500 distinct colours, and
// there is no dither that hides a 16-scanline step. Everything the cart DRAWS is
// still an 8-bit palette index; only this table is truecolour (docs/copper.md §15.1).

/**
 * Function: jet_rgb
 * Summary: Packs red, green and blue channels into the 24-bit truecolour word used by the Copper APIs.
 * Parameters:
 *   - r: Red channel from 0 to 255.
 *   - g: Green channel from 0 to 255.
 *   - b: Blue channel from 0 to 255.
 * Returns: A packed 0x00RRGGBB Copper colour.
 * Include: jet64.h
 * Notes: Copper colours are truecolour and are not palette indices. The packed value is passed to jet_copper_set, jet_copper_fill or the generators in jet_copper.h.
 * Example:
 *   uint32_t sunset = jet_rgb(255, 120, 40);
 */
static inline uint32_t jet_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << JET_COPPER_R_SHIFT) | ((uint32_t)g << JET_COPPER_G_SHIFT) |
           ((uint32_t)b << JET_COPPER_B_SHIFT);
}

/**
 * Function: jet_copper_enable
 * Summary: Enables or disables the Copper's per-scanline background colours at runtime.
 * Parameters:
 *   - on: true to enable the Copper list or false to show ordinary palette index 0.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: The project's Copper capability must also be enabled in Shader Lab. When active, framebuffer pixels containing palette index 0 reveal the Copper colour for that scanline.
 * Example:
 *   jet_copper_enable(true);
 */
static inline void jet_copper_enable(bool on) {
    JET_BASE[JET_COPPER_OFFSET + JET_COPPER_ENABLE_OFFSET] = on ? 1u : 0u;
}

/**
 * Function: jet_copper_enabled
 * Summary: Reports whether the cart has enabled the Copper list.
 * Parameters:
 *   - None.
 * Returns: true when the cart-side Copper enable byte is set; otherwise false.
 * Include: jet64.h
 * Notes: This does not report whether the project capability is enabled in Shader Lab; both gates must be on before Copper colours appear.
 * Example:
 *   if (!jet_copper_enabled()) jet_copper_enable(true);
 */
static inline bool jet_copper_enabled(void) {
    return JET_BASE[JET_COPPER_OFFSET + JET_COPPER_ENABLE_OFFSET] != 0u;
}

/**
 * Function: jet_copper_set
 * Summary: Sets the truecolour background shown behind palette-index-0 pixels on one scanline.
 * Parameters:
 *   - line: Zero-based screen row; rows at or above JET_COPPER_MAX_LINES are ignored.
 *   - rgb: Packed 0x00RRGGBB colour, normally created with jet_rgb.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: The reserved top byte is discarded. Call jet_copper_enable(true) and enable the project capability in Shader Lab. Setting rows beyond the live display height is harmless but invisible.
 * Example:
 *   jet_copper_set(20, jet_rgb(20, 40, 100));
 */
static inline void jet_copper_set(uint32_t line, uint32_t rgb) {
    if (line < (uint32_t)JET_COPPER_MAX_LINES) {
        COPPER[line] = rgb & JET_COPPER_RGB_MASK;
    }
}

/**
 * Function: jet_copper_set_rgb
 * Summary: Sets one scanline's Copper background directly from red, green and blue channel values.
 * Parameters:
 *   - line: Zero-based screen row; out-of-range rows are ignored.
 *   - r: Red channel from 0 to 255.
 *   - g: Green channel from 0 to 255.
 *   - b: Blue channel from 0 to 255.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: Equivalent to jet_copper_set(line, jet_rgb(r, g, b)). The Copper is visible only behind palette-index-0 framebuffer pixels and while both activation gates are enabled.
 * Example:
 *   jet_copper_set_rgb((uint32_t)y, 20, 40, 100);
 */
static inline void jet_copper_set_rgb(uint32_t line, uint8_t r, uint8_t g, uint8_t b) {
    jet_copper_set(line, jet_rgb(r, g, b));
}

/**
 * Function: jet_copper_fill
 * Summary: Fills every active screen row in the Copper list with one truecolour background.
 * Parameters:
 *   - rgb: Packed 0x00RRGGBB colour, normally created with jet_rgb.
 * Returns: Nothing.
 * Include: jet64.h
 * Notes: This fills the live display height, capped at JET_COPPER_MAX_LINES. Enable the Copper and leave framebuffer pixels at palette index 0 where this background should show through.
 * Example:
 *   jet_copper_fill(jet_rgb(8, 12, 32));
 */
static inline void jet_copper_fill(uint32_t rgb) {
    uint32_t height = SYS[SYS_SCREEN_HEIGHT];
    if (height > (uint32_t)JET_COPPER_MAX_LINES) {
        height = (uint32_t)JET_COPPER_MAX_LINES;
    }
    uint32_t color = rgb & JET_COPPER_RGB_MASK;
    for (uint32_t y = 0; y < height; ++y) {
        COPPER[y] = color;
    }
}

#endif // JET_CORE_H
