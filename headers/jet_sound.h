// runtime/jet_sound.h
#ifndef JET_SOUND_H
#define JET_SOUND_H

// ============================================================================
// Jet64 sound library: recorded sounds, loaded when they are needed (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// The cart resolver compiles jet_sound.c only when a cart's #include tree
// reaches this header, so a cart that never asks for it carries none of it —
// not a byte of code, not a byte of its tables, not a cycle of any tick. It
// stands on jet_pcm.c (the channels it plays on) and on firmware's tick count
// (tick_internal.h), and adds nothing to either.
//
// THE LAWS
//
//   1. One arena, reserved once in _init, never grown: WRAM is a bump allocator
//      that gives nothing back, so everything after jet_sound_open is placement
//      inside one block. It is the whole catalogue or the cart's ceiling,
//      whichever is smaller: a catalogue that fits costs exactly a preload of
//      it and never evicts; one past the ceiling costs the ceiling however far
//      it grows.
//   2. Nothing is given up while a channel may still read it. The mixer reads a
//      playing sound straight out of WRAM, so a sound armed on a channel is
//      held for as long as that channel's descriptor says it is being read —
//      which follows rate rides and loops exactly, because it is the hardware's
//      own answer. A one-shot the host has played to its end is let go at once;
//      one cut mid-play or stopped is held JET_SOUND_GRACE_TICKS more, from the
//      moment the module sees it (a stop's declick, a new arm the host has not
//      latched yet).
//   3. What goes is what was least recently needed. A sound is one unbroken
//      block, so making room means choosing a WINDOW of the arena: of every
//      window wide enough that covers nothing held, the one whose freshest
//      sound is the stalest, free space counting as never used. Ties go to the
//      window that gives up fewer bytes, then the lower address, which makes
//      the free-space case address-ordered first fit — the policy the
//      allocator literature finds fragments least (Wilson, Johnstone, Neely and
//      Boles, "Dynamic Storage Allocation: A Survey and Critical Review", 1995).
//   4. The arena starts full: jet_sound_open loads the catalogue in the order it
//      was added, into free space only.
//
// Determinism: placement reads no clock of the host's. Its time is firmware's
// tick count and its recency a count of its own uses. Law 2 reads the PCM
// descriptors, whose read cursor the host's mixer advances in real time, so on
// a live host which sounds are resident — and which channel a sound lands on —
// may differ between two runs of one scenario; what a cart can observe through
// the simulation does not. A headless run mixes nothing, so it is exact.
// A cart's own arena was this module's ancestor: these laws, with a computed
// hold in place of law 2.
// </nocodex>
//
// Play as many recordings as your game has, without loading them all. A game
// with a few sounds can load each one with jet_pcm_load_wav and keep it for
// ever. A game with hundreds — every footstep, spell, creature and line of
// dialogue — would fill its working memory that way. The sound library keeps
// only the sounds your game has needed lately: it loads a sound the first time
// you play it, and when its room is full it lets go of the sound that has gone
// longest unplayed. You never load or free anything yourself; you add every
// sound once, then play them by name.
//
// It works in three steps, all of them in _init but the last.
//
//   1. Add every sound with jet_sound_add (a .wav) or jet_sound_add_raw (a
//      headerless .pcm). Adding reads only the file's header, so adding a
//      thousand sounds costs a thousand header reads and no sound memory.
//   2. Open the library with jet_sound_open, saying how much working memory it
//      may use. If every sound fits it loads them all now; if not it loads as
//      many as fit and loads the rest as they are played.
//   3. Play a sound with jet_sound_play, anywhere: it finds the sound (loading
//      it if it has to), finds a free PCM channel, and plays it there.
//
// A sound that has to be loaded is loaded on the spot, in the same tick, so it
// is never late and never skipped; a long one may cost that tick some time.
// Call jet_sound_preload as a scene begins for the sounds it is about to need.
//
// A sound that is playing is never let go, so your working memory must have
// room for every channel's sound at once and one more. jet_sound_open warns in
// the log when it may not.
//
// The library plays on the PCM channels of jet_pcm.h, and leaves alone any
// channel your own code is playing on, so music from jet_ogg.h and the sound
// library share the channels safely. A channel that pauses or stops, though,
// looks free, so keep your music channel out with jet_sound_use_channels.
//
// This header is opt-in, and it brings jet_pcm.h with it: include it yourself,
// after jet64.h, and only when you want it.
//
//   #include "jet64.h"
//   #include "jet_sound.h"   // <-- explicit opt-in
//
// Complete sound-board example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_sound.h"
//
//   enum { FULL_VOLUME = 255, CENTRE_PAN = 128, SOUND_MEMORY = 2 * 1024 * 1024 };
//   static JetSound jump, coin, roar;
//
//   void _init(void) {
//       jump = jet_sound_add("sfx/jump.wav");
//       coin = jet_sound_add("sfx/coin.wav");
//       roar = jet_sound_add("sfx/roar.wav");
//       jet_sound_open(SOUND_MEMORY);
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) jet_sound_play(jump, FULL_VOLUME, CENTRE_PAN);
//       if (btnp(BTN_B)) jet_sound_play(coin, FULL_VOLUME, CENTRE_PAN);
//       if (btnp(BTN_X)) jet_sound_play(roar, FULL_VOLUME, CENTRE_PAN);
//   }
//
//   void _draw(void) {
//       JetSoundStats stats;
//       char line[48];
//       jet_sound_stats(&stats);
//       snprintf(line, sizeof line, "%u OF %u SOUNDS LOADED", (unsigned)stats.resident,
//                (unsigned)stats.sounds);
//       jet_cls(0);
//       jet_print("A JUMP  B COIN  X ROAR", 16, 16, 15);
//       jet_print(line, 16, 32, 15);
//   }
//
//   void _end(void) {
//       jet_pcm_stop_all();
//   }

#include "jet_pcm.h" // IWYU pragma: keep (JetPcmFormat, the channels it plays on; jet_core.h's bool/stdint)

/**
 * Type: JetSound
 * Summary: One sound in the library: the number jet_sound_add gave you, which you keep and play by.
 * Include: jet_sound.h
 * Notes: It names the sound, not the memory it sits in, so it stays good for the whole run, however many times the sound is loaded and let go. JET_SOUND_NONE (0) is the one "no sound" value: a failed add returns it, a JetSound that is still zero because it was never set is one, and every call given it does nothing.
 * Example:
 *   static JetSound jump;
 *   void _init(void) { jump = jet_sound_add("sfx/jump.wav"); }
 */
typedef uint16_t JetSound;

/**
 * Constant: JET_SOUND_NONE
 * Summary: The one "no sound" value for a JetSound.
 * Include: jet_sound.h
 * Notes: jet_sound_add and jet_sound_add_raw return it when a sound could not be added, and every call that takes a JetSound does nothing when given it.
 * Example:
 *   if (jet_sound_add("sfx/jump.wav") == JET_SOUND_NONE) printf("jump.wav is missing\n");
 */
#define JET_SOUND_NONE ((JetSound)0)

/**
 * Constant: JET_SOUND_MAX
 * Summary: The most sounds the library holds: how many times jet_sound_add can succeed in one run.
 * Include: jet_sound.h
 * Notes: The count of sounds you add, not of sounds loaded at once; how many fit at once is set by the memory you give jet_sound_open.
 * Example:
 *   static JetSound footsteps[JET_SOUND_MAX / 8];
 */
#define JET_SOUND_MAX 1024

/**
 * Constant: JET_SOUND_WHOLE_CATALOGUE
 * Summary: The memory to give jet_sound_open when every sound you added should stay loaded.
 * Include: jet_sound.h
 * Notes: The library then reserves exactly what your sounds need, loads them all in _init and never lets one go. Use it while a game's sounds are few; give jet_sound_open a size of its own once they outgrow what you want to spend.
 * Example:
 *   jet_sound_open(JET_SOUND_WHOLE_CATALOGUE);
 */
#define JET_SOUND_WHOLE_CATALOGUE UINT32_MAX

/**
 * Constant: JET_SOUND_NO_CHANNEL
 * Summary: What jet_sound_play and jet_sound_loop return when the sound was not played.
 * Include: jet_sound.h
 * Notes: Every channel was busy with something that may not be cut, or the sound could not be loaded. Nothing is wrong with your game; a sound was dropped, exactly as an old machine dropped one when its channels were all in use. jet_sound_stats counts each reason.
 * Example:
 *   if (jet_sound_play(roar, 255, 128) == JET_SOUND_NO_CHANNEL) printf("roar dropped\n");
 */
#define JET_SOUND_NO_CHANNEL (-1)

/**
 * Constant: JET_SOUND_ALL_CHANNELS
 * Summary: Every PCM channel, as a mask for jet_sound_use_channels: what the library may play on until you say otherwise.
 * Include: jet_sound.h
 * Notes: Bit n stands for channel n. Take a channel out with the usual bit operators, as in the example.
 * Example:
 *   jet_sound_use_channels(JET_SOUND_ALL_CHANNELS & ~(1u << MUSIC_CHANNEL));
 */
#define JET_SOUND_ALL_CHANNELS 0xFFu

/**
 * Constant: JET_SOUND_GRACE_TICKS
 * Summary: How many ticks the library keeps a sound after a channel lets go of it, before its memory may be used again.
 * Include: jet_sound.h
 * Notes: A fifth of a second. The host keeps reading a sound for a moment after its channel is stopped or given something new: through a 5 ms fade, and until its next buffer of sound picks the change up. The grace covers both, many times over, on every host, so no sound is ever written over while it can still be heard.
 * Example:
 *   float grace_seconds = JET_SOUND_GRACE_TICKS / JET_GUEST_TICK_HZ;
 */
#define JET_SOUND_GRACE_TICKS 24

/**
 * Struct: JetSoundStats
 * Summary: What the library holds and what it has done since it opened, from jet_sound_stats.
 * Fields:
 *   - sounds: How many sounds were added.
 *   - resident: How many of them are loaded now.
 *   - catalogue_bytes: How much memory every added sound would take at once.
 *   - arena_bytes: How much memory the library reserved, the smaller of catalogue_bytes and what you gave jet_sound_open.
 *   - resident_bytes: How much of that the loaded sounds take now.
 *   - plays: Sounds played, on any channel.
 *   - hits: Plays and preloads that found their sound already loaded.
 *   - loads: Plays and preloads that had to load their sound from the cartridge.
 *   - bytes_loaded: How many bytes those loads read.
 *   - evictions: Sounds let go to make room.
 *   - no_room: Plays and preloads refused because every place big enough held a sound still playing.
 *   - no_channel: Plays refused because every channel was busy with something that may not be cut.
 *   - unreadable: Loads that failed because the file could not be read whole, as if it had changed since it was added.
 * Include: jet_sound.h
 * Notes: The counts only go up, from the moment the library opens. A game whose hits far outnumber its loads has given the library enough memory; one that loads on most plays would be smoother with more, or with jet_sound_preload as each scene begins.
 * Example:
 *   JetSoundStats stats;
 *   jet_sound_stats(&stats);
 *   printf("%u loads, %u evictions\n", (unsigned)stats.loads, (unsigned)stats.evictions);
 */
typedef struct {
    uint32_t sounds;
    uint32_t resident;
    uint64_t catalogue_bytes;
    uint32_t arena_bytes;
    uint32_t resident_bytes;
    uint32_t plays;
    uint32_t hits;
    uint32_t loads;
    uint64_t bytes_loaded;
    uint32_t evictions;
    uint32_t no_room;
    uint32_t no_channel;
    uint32_t unreadable;
} JetSoundStats;

/**
 * Function: jet_sound_add
 * Summary: Adds a .wav file to the library, reading only its header, and gives you the JetSound to play it by.
 * Parameters:
 *   - rel_path: The file's path from inside the cartridge's data/ folder, such as "sfx/jump.wav" for data/sfx/jump.wav. Leave data/ off: the runtime adds it.
 * Returns: The new sound, or JET_SOUND_NONE if it could not be added. Each failure logs its reason.
 * Include: jet_sound.h
 * Notes: CALL IT ONLY IN _init, before jet_sound_open. It reads the header to learn the sound's length, format and rate, and loads nothing else: the sound itself is read when it is first played, or by jet_sound_open if there is room.
 *
 *        It reads plain uncompressed PCM, 8-bit or 16-bit, mono or stereo — exactly what jet_pcm_load_wav reads. Unlike jet_pcm_load_wav it plays a stereo file in stereo: the pan you give a stereo sound is its balance (see JetPcmFormat), and an 8-bit stereo file is widened to 16 bits as it loads, because the channels play 8-bit sound in mono only. Anything else — compressed or floating-point samples, 24-bit samples, more than two channels — is refused, as is a file whose frames begin more than 4 KiB in (strip its metadata), a path that does not fit the library's name store, and a sound past JET_SOUND_MAX.
 * Example:
 *   static JetSound jump;
 *   void _init(void) { jump = jet_sound_add("sfx/jump.wav"); }
 */
JetSound jet_sound_add(const char* rel_path);

/**
 * Function: jet_sound_add_raw
 * Summary: Adds a raw sample file, one with no header, to the library, and gives you the JetSound to play it by.
 * Parameters:
 *   - rel_path: The file's path from inside the cartridge's data/ folder, such as "sfx/kick.pcm". Leave data/ off.
 *   - fmt: How each frame in the file is stored, as one of the JetPcmFormat values.
 *   - rate_hz: How many frames a second the recording plays at, such as 11025.
 * Returns: The new sound, or JET_SOUND_NONE if it could not be added: the file is missing or empty, the format is not a JetPcmFormat, or the rate is 0. Each failure logs its reason.
 * Include: jet_sound.h
 * Notes: CALL IT ONLY IN _init, before jet_sound_open. It measures the file and reads none of it. The bytes must match fmt exactly, as for jet_pcm_load; a trailing partial frame is never played.
 * Example:
 *   static JetSound kick;
 *   void _init(void) { kick = jet_sound_add_raw("sfx/kick.pcm", JET_PCM_S8, 11025); }
 */
JetSound jet_sound_add_raw(const char* rel_path, JetPcmFormat fmt, uint32_t rate_hz);

/**
 * Function: jet_sound_open
 * Summary: Opens the library once every sound is added: reserves its memory and loads as many of the sounds as fit.
 * Parameters:
 *   - max_bytes: The most working memory the library may use, in bytes, or JET_SOUND_WHOLE_CATALOGUE to keep every sound loaded.
 * Returns: true if the library is open; false if it was already open or its memory could not be reserved, each logged.
 * Include: jet_sound.h
 * Notes: CALL IT ONCE, IN _init, after every jet_sound_add. It reserves the smaller of max_bytes and what every sound would take at once, so a game whose sounds all fit pays for exactly its sounds, and one whose sounds do not pays max_bytes however many it adds. It then loads the sounds in the order you added them, for as long as they fit, so put the sounds every scene needs first.
 *
 *        A sound being played is never let go, so the memory must hold a sound on every channel at once and one more; when it may not, the log says so and names the size that would. Sounds are not added after the library opens; nothing plays before it does.
 * Example:
 *   jet_sound_open(4 * 1024 * 1024); // 4 MiB of sound at a time
 */
bool jet_sound_open(uint32_t max_bytes);

/**
 * Function: jet_sound_play
 * Summary: Plays a sound once on any free PCM channel, loading it first if it is not loaded.
 * Parameters:
 *   - sound: The sound, from jet_sound_add or jet_sound_add_raw.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo sound it is the balance instead.
 * Returns: The channel the sound plays on, from 0 to JET_PCM_CHANNEL_COUNT-1, or JET_SOUND_NO_CHANNEL if it was not played.
 * Include: jet_sound.h
 * Notes: It takes the lowest free channel of those jet_sound_use_channels allows. With none free it cuts the oldest sound the library is playing once; it never cuts a loop, nor anything your own code is playing. The channel it returns is yours to ride while the sound plays: jet_pcm_set_volume, jet_pcm_set_pan, jet_pcm_set_rate and jet_pcm_stop all work on it, and the library follows each.
 * Example:
 *   if (btnp(BTN_A)) jet_sound_play(jump, 255, 128);
 */
int jet_sound_play(JetSound sound, uint8_t vol, uint8_t pan);

/**
 * Function: jet_sound_loop
 * Summary: Plays a sound on any free PCM channel, over and over until that channel is stopped.
 * Parameters:
 *   - sound: The sound, from jet_sound_add or jet_sound_add_raw.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo sound it is the balance instead.
 * Returns: The channel the loop plays on, or JET_SOUND_NO_CHANNEL if it was not played. Keep it: it is how you stop the loop.
 * Include: jet_sound.h
 * Notes: The whole sound repeats. It picks its channel as jet_sound_play does, and a loop is never cut to make room for another sound: it plays until you call jet_pcm_stop on its channel, or play something else there.
 * Example:
 *   int engine = jet_sound_loop(engine_hum, 160, 128);
 *   // ... later, as the engine dies:
 *   if (engine != JET_SOUND_NO_CHANNEL) jet_pcm_stop((uint8_t)engine);
 */
int jet_sound_loop(JetSound sound, uint8_t vol, uint8_t pan);

/**
 * Function: jet_sound_play_on
 * Summary: Plays a sound once on the channel you choose, cutting whatever that channel was playing.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1. It need not be one jet_sound_use_channels allows.
 *   - sound: The sound, from jet_sound_add or jet_sound_add_raw.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo sound it is the balance instead.
 * Returns: true if the sound is playing; false for a channel out of range, JET_SOUND_NONE, or a sound that could not be loaded.
 * Include: jet_sound.h
 * Notes: For a sound with a channel of its own — a voice, so that each line cuts off the one before it.
 * Example:
 *   jet_sound_play_on(VOICE_CHANNEL, line_42, 255, 128);
 */
bool jet_sound_play_on(uint8_t channel, JetSound sound, uint8_t vol, uint8_t pan);

/**
 * Function: jet_sound_loop_on
 * Summary: Plays a sound on the channel you choose, over and over until that channel is stopped, cutting whatever it was playing.
 * Parameters:
 *   - channel: Which channel, from 0 to JET_PCM_CHANNEL_COUNT-1. It need not be one jet_sound_use_channels allows.
 *   - sound: The sound, from jet_sound_add or jet_sound_add_raw.
 *   - vol: How loud, from 0 to 255.
 *   - pan: Where it sits between the speakers, from 0 to 255; 128 is the centre. For a stereo sound it is the balance instead.
 * Returns: true if the loop is playing; false for a channel out of range, JET_SOUND_NONE, or a sound that could not be loaded.
 * Include: jet_sound.h
 * Notes: For a bed of sound with a channel of its own — rain, wind, the hum of a ship — so that changing the bed replaces it.
 * Example:
 *   jet_sound_loop_on(AMBIENCE_CHANNEL, rain, 120, 128);
 */
bool jet_sound_loop_on(uint8_t channel, JetSound sound, uint8_t vol, uint8_t pan);

/**
 * Function: jet_sound_preload
 * Summary: Loads a sound now, if it is not loaded, so that playing it later costs nothing.
 * Parameters:
 *   - sound: The sound, from jet_sound_add or jet_sound_add_raw.
 * Returns: true if the sound is loaded; false for JET_SOUND_NONE, before jet_sound_open, or when there is no room for it.
 * Include: jet_sound.h
 * Notes: Loading a sound counts as needing it, so a preloaded sound is the last to be let go. Call it as a scene begins, for the sounds the scene is about to use, and the scene's first plays read nothing from the cartridge.
 * Example:
 *   for (int i = 0; i < BOSS_SOUND_COUNT; i++) jet_sound_preload(boss_sounds[i]);
 */
bool jet_sound_preload(JetSound sound);

/**
 * Function: jet_sound_use_channels
 * Summary: Chooses which PCM channels jet_sound_play and jet_sound_loop may play on.
 * Parameters:
 *   - mask: One bit for each channel the library may use: bit n is channel n. JET_SOUND_ALL_CHANNELS, every channel, is where it starts.
 * Returns: Nothing.
 * Include: jet_sound.h
 * Notes: The library never takes a channel your own code is playing on, but a channel that is paused or stopped looks free. Keep out any channel your game plays on by itself, such as jet_ogg.h's music channel, and that channel is always yours. The choice touches only jet_sound_play and jet_sound_loop: jet_sound_play_on and jet_sound_loop_on play wherever you tell them.
 * Example:
 *   jet_sound_use_channels(JET_SOUND_ALL_CHANNELS & ~(1u << MUSIC_CHANNEL));
 */
void jet_sound_use_channels(uint8_t mask);

/**
 * Function: jet_sound_resident
 * Summary: Tells you whether a sound is loaded right now.
 * Parameters:
 *   - sound: The sound, from jet_sound_add or jet_sound_add_raw.
 * Returns: true if the sound is in memory, so playing it reads nothing from the cartridge; false if it is not, and for JET_SOUND_NONE.
 * Include: jet_sound.h
 * Notes: For a debug overlay or a test. A game never needs to ask: playing a sound loads it whenever it has to.
 * Example:
 *   jet_print(jet_sound_resident(roar) ? "ROAR LOADED" : "ROAR ON DISK", 16, 48, 15);
 */
bool jet_sound_resident(JetSound sound);

/**
 * Function: jet_sound_stats
 * Summary: Fills in what the library holds and what it has done since it opened.
 * Parameters:
 *   - out: The JetSoundStats to fill in.
 * Returns: Nothing.
 * Include: jet_sound.h
 * Notes: Cheap enough for a debug overlay to call every frame. Before jet_sound_open everything but sounds and catalogue_bytes reads 0.
 * Example:
 *   JetSoundStats stats;
 *   jet_sound_stats(&stats);
 */
void jet_sound_stats(JetSoundStats* out);

#endif // JET_SOUND_H
