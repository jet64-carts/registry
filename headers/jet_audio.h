// runtime/jet_audio.h
#ifndef JET_AUDIO_H
#define JET_AUDIO_H

#include <stdint.h>

#include "jet_core.h" // the audio blocks' offsets and types, JET_BASE, and JET_BGM_*

// Everything that makes a sound. Three kinds of thing come out of here: a
// sound effect you made in the SFX Editor, an instrument played at a note, and
// tracker music from a file in data/mod/. A patch is a recipe for a sound,
// made in the SFX Editor (the panel is also called the Instrument Studio) and
// saved in the cartridge's bank of patches, numbered 0 to 255. A sound effect
// is a patch played once, at the pitch and volume you saved; an instrument is
// the same kind of patch played at whatever note you ask for. Start with
// jet_sfx_play for a sound effect, jet_note_on and jet_sfx_release_handle for
// a note that holds until you let it go, and jet_play_mod for music.
//
// A sound plays on a channel, one of 16 voices numbered 0 to 15, and a channel
// plays one sound at a time. The calls that pick a channel for you use 0 to 14
// and never touch channel 15, so your cartridge can keep a sound of its own
// there, an engine hum say, that no other sound will ever take over. To stop
// or release one particular sound later, take a handle for it with
// jet_sfx_handle straight after the play call. A handle is a small value that
// stands for that one sound, and it stays harmless even after the channel has
// been given to something else.
//
// Call any of these from your normal _init and _update code. The runtime keeps
// the sound engine running for you.
//
// This is an opt-in module, like jet_pcm.h: jet64.h does not include it, so
// include it yourself in every file that makes a sound. That is how the
// machine knows: a cartridge whose files never include it, directly or
// through jet_audio_ext.h, makes no sound, and its exported player never opens
// an audio device. The master volumes and the equaliser are here too.
//
// Complete authored-SFX example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_audio.h"
//
//   enum { SFX_JUMP = 0 };
//
//   void _init(void) {}
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) jet_sfx_play(SFX_JUMP);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print("PRESS A TO PLAY SFX 0", 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_sfx_stop_all();
//   }

// ====================== THE AUDIO BLOCKS ======================
// The machine's audio hardware, each block named as a pointer into the motherboard at its
// offset in jet_core.h's hardware map. They are this module's and no one else's: a cart that
// makes no sound never includes this header, and its player opens no audio device. Each is an
// expression over JET_BASE, the start of the machine's memory, as jet_net.c reaches its block:
// naming one costs one load and an add, as reading a bound pointer cost one load, and nothing
// has to bind it at boot.

// A host-side test harness has no machine behind JET_BASE, so it stands each block in with a
// pointer object of its own (at a stub, or NULL for a host that plays nothing) and builds with
// JET_AUDIO_BLOCKS_STUBBED defined; the blocks are then those objects, declared here as the
// runtime declared them before the blocks left firmware. A cartridge never defines it.
#ifdef JET_AUDIO_BLOCKS_STUBBED
// <nocodex>
extern uint8_t* SFX;
extern volatile uint8_t* BGM_CMD;
extern volatile JetBgmChannelState* BGM_STATE;
extern volatile JetBgmTrackerPattern* BGM_TRACKER;
extern volatile JetBgmTransport* BGM_TRANSPORT;
extern volatile uint8_t* AUDIO;
extern volatile uint8_t* AUDIO_MASTER;
extern volatile JetAudioEqBlock* AUDIO_EQ;
// </nocodex>
#else

/**
 * Constant: SFX
 * Summary: The SFX block: the cartridge's 256 patches, then its macros, wavetables and curves.
 * Include: jet_audio.h
 * Notes: A patch is JET_PATCH_SIZE bytes; jet_sfx_play and its family read them for you.
 * Example:
 *   const uint8_t* first_patch = SFX;
 */
#define SFX ((uint8_t*)(JET_BASE + JET_SFX_OFFSET))

/**
 * Constant: BGM_CMD
 * Summary: The music's command bridge to the host, which jet_play_mod and its family write.
 * Include: jet_audio.h
 * Notes: Use the jet_play_mod family rather than writing it yourself.
 * Example:
 *   jet_play_mod("theme.flxm"); // writes BGM_CMD for you
 */
#define BGM_CMD ((volatile uint8_t*)(JET_BASE + JET_BGM_CMD_OFFSET))

/**
 * Constant: BGM_STATE
 * Summary: The live state of the music's 32 channels, which the host publishes as it plays.
 * Include: jet_audio.h
 * Notes: Read it to draw a visualiser; it is the host's to write.
 * Example:
 *   uint8_t vol = BGM_STATE[0].volume;
 */
#define BGM_STATE ((volatile JetBgmChannelState*)(JET_BASE + JET_BGM_STATE_OFFSET))

/**
 * Constant: BGM_TRACKER
 * Summary: The playing song's pattern window and position, which the host publishes as it plays.
 * Include: jet_audio.h
 * Notes: Read jet_bgm_row for the row across the window's slices.
 * Example:
 *   int order = BGM_TRACKER->pattern_pos;
 */
#define BGM_TRACKER ((volatile JetBgmTrackerPattern*)(JET_BASE + JET_BGM_TRACKER_OFFSET))

/**
 * Constant: BGM_TRANSPORT
 * Summary: The playing song's ticket, passes and title, and the newest play refused.
 * Include: jet_audio.h
 * Notes: The jet_bgm_* calls below read it for you.
 * Example:
 *   uint32_t song = jet_bgm_song(); // reads BGM_TRANSPORT
 */
#define BGM_TRANSPORT ((volatile JetBgmTransport*)(JET_BASE + JET_BGM_TRANSPORT_OFFSET))

/**
 * Constant: AUDIO
 * Summary: The sixteen live synth registers, one JetAudioRegister of 64 bytes for each channel.
 * Include: jet_audio.h
 * Notes: The jet_sfx_* and jet_note_* calls write them for you.
 * Example:
 *   volatile JetAudioRegister* ch0 = (volatile JetAudioRegister*)AUDIO;
 */
#define AUDIO ((volatile uint8_t*)(JET_BASE + JET_AUDIO_OFFSET))

/**
 * Constant: AUDIO_MASTER
 * Summary: The master-volume register: the music's and the sound effects' gains, and the .mod machine.
 * Include: jet_audio.h
 * Notes: Use jet_host_set_volume_sfx, jet_host_set_volume_bgm and jet_mod_set_machine.
 * Example:
 *   jet_host_set_volume_bgm(0.5f); // writes AUDIO_MASTER for you
 */
#define AUDIO_MASTER ((volatile uint8_t*)(JET_BASE + JET_AUDIO_MASTER_OFFSET))

/**
 * Constant: AUDIO_EQ
 * Summary: The master graphic equaliser's block: its mode and its ten bands' gains.
 * Include: jet_audio.h
 * Notes: Use the jet_host_eq_* calls below.
 * Example:
 *   jet_host_eq_set_mode(JET_AUDIO_EQ_CART); // writes AUDIO_EQ for you
 */
#define AUDIO_EQ ((volatile JetAudioEqBlock*)(JET_BASE + JET_AUDIO_EQ_OFFSET))
#endif // JET_AUDIO_BLOCKS_STUBBED

// The waveforms of the old direct-synth path, for jet_sfx. A new cartridge
// normally makes a patch in the SFX Editor and plays it with jet_sfx_play
// instead.
#define JET_WAVE_NONE   0
#define JET_WAVE_SQUARE 1
#define JET_WAVE_SAW    2
#define JET_WAVE_SINE   3
#define JET_WAVE_NOISE  4
#define JET_WAVE_WAVETABLE 5

// A command, not a waveform, and never one you can author. Written into a
// playing channel's waveform byte, it asks the synth to end the note the
// musical way, through the patch's release or decay phase. Call
// jet_sfx_release instead of writing this value yourself.
#define JET_WAVE_CMD_RELEASE 0xFF

// What jet_sfx_handle gives back for a channel that does not exist. It is safe
// to pass to every jet_sfx_*_handle call; each one does nothing with it.
#define JET_SFX_HANDLE_INVALID (-1)

// The old short names, kept so older cartridges still build.
#define WAVE_NONE   JET_WAVE_NONE
#define WAVE_SQUARE JET_WAVE_SQUARE
#define WAVE_SAW    JET_WAVE_SAW
#define WAVE_SINE   JET_WAVE_SINE
#define WAVE_NOISE  JET_WAVE_NOISE
#define WAVE_WAVETABLE JET_WAVE_WAVETABLE

// ==========================================
// SOUND EFFECTS (SFX)
// ==========================================

/**
 * Function: jet_sfx_play
 * Summary: Plays a sound effect you made in the SFX Editor, on a channel the runtime picks for you.
 * Parameters:
 *   - sfx_id: The patch number in the cartridge's bank, from 0 to 255.
 * Returns: The channel the sound is playing on, from 0 to 14, or -1 if the sound was dropped because this patch had already started as many times this frame as jet_sfx_set_limit allows.
 * Include: jet_audio.h
 * Notes: The sound plays at the pitch and volume saved in the patch, centred between the speakers. The runtime hands out channels 0 to 14 in turn, so a new sound may take over a channel that an older sound is still using. Channel 15 is never picked, so it stays free for a sound you place yourself with jet_sfx_play_on. To stop or release this one sound later, call jet_sfx_handle on the channel number straight away; the bare channel number may be given to another sound before you get to it.
 * Example:
 *   int channel = jet_sfx_play(4);
 *   if (channel >= 0) { } // the sound was sent to a channel
 */
int jet_sfx_play(uint8_t sfx_id);

/**
 * Function: jet_sfx_play_ext
 * Summary: Plays a sound effect from the SFX Editor with your own pitch, pan and volume, on a channel the runtime picks for you.
 * Parameters:
 *   - sfx_id: The patch number in the cartridge's bank, from 0 to 255.
 *   - pitch_mult: How much to change the pitch, as a multiplier. 1.0f keeps the patch's pitch, 2.0f raises it one octave, and 0.5f lowers it one octave. The whole sound moves, as the SFX Editor moves it: a pitch sweep covers the same musical distance in the same time, and the pitch it stops at moves with it. Anything that is not a positive number, zero included, is ignored and the patch's pitch is kept.
 *   - pan: Where the sound sits between the speakers: 0 is hard left, 128 is the centre, and 255 is hard right.
 *   - vol: How loud, from 0 to 255.
 * Returns: The channel the sound is playing on, from 0 to 14, or -1 if the sound was dropped because this patch had already started as many times this frame as jet_sfx_set_limit allows.
 * Include: jet_audio.h
 * Notes: Use this, rather than choosing a channel yourself, for gameplay sounds that may safely be cut short when a newer sound needs the channel.
 * Example:
 *   jet_sfx_play_ext(7, 0.75f, 96, 220);
 */
int jet_sfx_play_ext(uint8_t sfx_id, float pitch_mult, uint8_t pan, uint8_t vol);

/**
 * Function: jet_sfx_play_on
 * Summary: Plays a sound effect from the SFX Editor on a channel you choose.
 * Parameters:
 *   - channel: The channel to play on, from 0 to 15.
 *   - sfx_id: The patch number in the cartridge's bank, from 0 to 255.
 *   - pitch_mult: How much to change the pitch, as a multiplier, taken exactly as jet_sfx_play_ext takes it: 1.0f keeps the patch's pitch, and anything that is not a positive number keeps it too.
 *   - pan: Where the sound sits between the speakers: 0 is hard left, 128 is the centre, and 255 is hard right.
 *   - vol: How loud, from 0 to 255.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: This skips the automatic channel choice and the limit set by jet_sfx_set_limit. Use it for a sound that must always sit on the same channel and must never be cut short by another sound: an engine, background ambience, or a preview in a menu. Channel 15 is the one the automatic choice never uses.
 * Example:
 *   jet_sfx_play_on(15, 2, 1.0f, 128, 180);
 */
void jet_sfx_play_on(uint8_t channel, uint8_t sfx_id, float pitch_mult, uint8_t pan, uint8_t vol);

/**
 * Function: jet_note_on
 * Summary: Plays an instrument patch at a note, as a keyboard would, on a channel the runtime picks for you.
 * Parameters:
 *   - patch_idx: The patch number in the cartridge's bank, from 0 to 255.
 *   - midi_note: The note to play, as a MIDI note number: 60 is middle C, and 69 is the A above it, at 440 Hz.
 *   - velocity: How hard the key is struck, which sets the volume, from 0 to 255.
 * Returns: The channel the note is playing on, from 0 to 14.
 * Include: jet_audio.h
 * Notes: A patch plays the pitch it was made at on note 72, the C above middle C (on note 48 for a drum), and every other note is that same sound moved up or down from there, its pitch sweep and the pitch the sweep stops at included. That is exactly how the SFX Editor's keyboard and the Tune Grid play it, so a cart sounds like the editor. A patch made at C5 (523 Hz), as the SFX Editor's presets are, therefore plays every note at its true pitch: 60 is middle C and 69 the A at 440 Hz. The note sits in the centre between the speakers. A patch that sustains keeps sounding until you release it. For those, call jet_sfx_handle on the returned channel straight away and end the note with jet_sfx_release_handle; then if the channel has since been given to another sound, you cannot release the wrong one.
 * Example:
 *   jet_note_on(12, 60, 220); // Patch 12, middle C, struck hard.
 */
int jet_note_on(uint8_t patch_idx, uint8_t midi_note, uint8_t velocity);

/**
 * Function: jet_note_on_ext
 * Summary: Plays an instrument patch at a note with your own velocity and pan, on a channel the runtime picks for you.
 * Parameters:
 *   - patch_idx: The patch number in the cartridge's bank, from 0 to 255.
 *   - midi_note: The note to play, as a MIDI note number: 60 is middle C, and 69 is the A above it, at 440 Hz.
 *   - velocity: How hard the key is struck, which sets the volume, from 0 to 255.
 *   - pan: Where the note sits between the speakers: 0 is hard left, 128 is the centre, and 255 is hard right.
 * Returns: The channel the note is playing on, from 0 to 14.
 * Include: jet_audio.h
 * Notes: This is the call for playable instruments and piano-style carts. It plays a note the way jet_note_on does: the patch's own pitch on note 72 (on note 48 for a drum), and every other note moved from there. That is jet_sfx_play_ext's pitch multiplier counted in notes, so twelve notes above the patch's own is a multiplier of 2.0f.
 * Example:
 *   jet_note_on_ext(0, 69, 220, 128); // Patch 0, A above middle C, struck hard, in the centre.
 */
int jet_note_on_ext(uint8_t patch_idx, uint8_t midi_note, uint8_t velocity, uint8_t pan);

/**
 * Function: jet_sfx_stop
 * Summary: Stops the sound on one channel, without a click.
 * Parameters:
 *   - channel: The channel to stop, from 0 to 15. Any other number is ignored.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: The synth fades the sound out over a very short ramp before it goes silent, so stopping a sound part way through a wave never pops. The ramp is about 5 ms, which the ear hears as instant; a patch can lengthen it through its stop_fade_ms footer byte for a slow, deliberate fade. It is safe to call every frame, and on a channel that is already silent.
 *
 *        To let a sound end musically, through the shape its patch gives it, use jet_sfx_release instead. To stop a sound on a channel the runtime picked for you, use jet_sfx_stop_handle, which cannot hit a different sound that has since taken the channel.
 * Example:
 *   // A long falling sound started with jet_sfx_play_on. Cut it, without a click, when the player lands.
 *   jet_sfx_stop(CHAN_PLAYER_ACTION);
 */
void jet_sfx_stop(uint8_t channel);

/**
 * Function: jet_sfx_stop_all
 * Summary: Stops the sound on all 16 channels, without a click.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Use it when a scene changes, when you mute the game's sounds, or when you reset the game. Each sound that is playing fades out over the same short ramp as jet_sfx_stop.
 * Example:
 *   jet_sfx_stop_all();
 */
void jet_sfx_stop_all(void);

/**
 * Function: jet_sfx_release
 * Summary: Lets the sound on one channel end musically, the way its patch says a note should die away.
 * Parameters:
 *   - channel: The channel to release, from 0 to 15. Any other number is ignored.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: This is the "key up" that goes with jet_note_on. Every patch has an envelope, the shape of how its sound rises and dies away, and releasing a channel moves the sound into the ending part of that shape: an FM patch enters the release phase of each of its operators, a wavetable patch its decay phase, and an SFXR patch its decay stage. A patch that sustains, one whose env_sustain or FM sustain level is above zero, holds its note for ever until you release it, so for those this call is essential. How long the ending takes comes from the patch. For a fast cut with no click, use jet_sfx_stop instead. Releasing a channel that is idle, or already released, does no harm.
 *
 *        If frames have passed since the play call, use jet_sfx_release_handle instead; by then the bare channel number may have been given to another sound.
 * Example:
 *   int ch = jet_note_on(12, 60, 220); // A pad that sustains, at middle C.
 *   // ... later, when the key is let go:
 *   if (ch >= 0) jet_sfx_release((uint8_t)ch);
 */
void jet_sfx_release(uint8_t channel);

/**
 * Function: jet_sfx_release_all
 * Summary: Lets the sound on all 16 channels end musically, each the way its patch says it should.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: The gentle partner of jet_sfx_stop_all: each sound finishes through its own envelope, the shape its patch gives its ending, instead of fading on the fast no-click ramp. Good for a scene change that should sound composed rather than cut.
 * Example:
 *   jet_sfx_release_all();
 */
void jet_sfx_release_all(void);

// ==========================================
// HANDLES (stop or release one particular sound, safely)
// ==========================================

/**
 * Function: jet_sfx_handle
 * Summary: Gives you a handle for the sound playing on a channel right now, so you can stop or release that one sound later without touching whatever replaced it.
 * Parameters:
 *   - channel: The channel number that jet_sfx_play or jet_note_on gave you, or any channel from 0 to 15.
 * Returns: A handle: a small value that stands for this one sound. Keep it, pass it back, do not look inside. For a channel outside 0 to 15, including the -1 a dropped play returns, you get JET_SFX_HANDLE_INVALID.
 * Include: jet_audio.h
 * Notes: A channel the runtime picks for you is shared. Once the runtime has gone round all 15 of its channels, a channel number you kept from earlier may quietly belong to a different sound. A handle remembers not just the channel but which play on it was yours, so jet_sfx_stop_handle and jet_sfx_release_handle do nothing once the channel has been taken by another sound. Take the handle straight after the play call. A handle could name the wrong sound only after 256 more plays land on the same channel, which is thousands of plays across the shared channels.
 * Example:
 *   int h = jet_sfx_handle(jet_note_on(12, 60, 220)); // A pad that sustains.
 *   // ... many frames later, when the key is let go:
 *   jet_sfx_release_handle(h); // Cannot touch a sound that took the channel since.
 */
int jet_sfx_handle(int channel);

/**
 * Function: jet_sfx_handle_channel
 * Summary: Tells you which channel a handle's sound is still on, as long as the channel has not been given to another sound.
 * Parameters:
 *   - handle: A handle from jet_sfx_handle.
 * Returns: The channel number, 0 to 15, while the handle still names the sound on it; or -1 once the channel has been reused, and for JET_SFX_HANDLE_INVALID.
 * Include: jet_audio.h
 * Notes: Use this when you want to write to the channel's registers yourself, for a pitch bend or a volume ride, on a sound whose channel the runtime picked. A channel number coming back proves only that nothing new has been played on the channel; the sound may still have finished on its own.
 * Example:
 *   int ch = jet_sfx_handle_channel(engine_loop_handle);
 *   if (ch >= 0) { } // the loop still owns the channel
 */
int jet_sfx_handle_channel(int handle);

/**
 * Function: jet_sfx_stop_handle
 * Summary: Stops the one sound a handle names, without a click. If the channel now belongs to another sound, nothing happens.
 * Parameters:
 *   - handle: A handle from jet_sfx_handle.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: The safe partner of jet_sfx_stop. Use it whenever the channel came from jet_sfx_play, jet_sfx_play_ext or jet_note_on and frames may pass before the stop. A handle that is out of date, or JET_SFX_HANDLE_INVALID, is ignored.
 * Example:
 *   int fall = jet_sfx_handle(jet_sfx_play(SFX_FALLING));
 *   // ... on landing:
 *   jet_sfx_stop_handle(fall); // Never cuts a different sound that took the channel.
 */
void jet_sfx_stop_handle(int handle);

/**
 * Function: jet_sfx_release_handle
 * Summary: Lets the one sound a handle names end musically, through the shape its patch gives it. If the channel now belongs to another sound, nothing happens.
 * Parameters:
 *   - handle: A handle from jet_sfx_handle.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: The safe partner of jet_sfx_release, and the right "key up" for a sustaining instrument played through jet_note_on, because the channel the runtime picked may be given to another sound before the key is let go. A handle that is out of date, or JET_SFX_HANDLE_INVALID, is ignored.
 * Example:
 *   int pad = jet_sfx_handle(jet_note_on(12, 60, 220));
 *   // ... when the key is let go:
 *   jet_sfx_release_handle(pad);
 */
void jet_sfx_release_handle(int handle);

/**
 * Function: jet_sfx_set_limit
 * Summary: Sets how many times one patch may start playing in a single frame.
 * Parameters:
 *   - sfx_id: The patch number in the cartridge's bank, from 0 to 255.
 *   - max_per_frame: The most starts allowed in one frame. 255 means as good as no limit.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: This stops a burst of the same sound, such as fifty particles landing or a crowd of enemies dying at once, from turning into a spike of noise. A play past the limit is dropped, and jet_sfx_play returns -1 for it.
 * Example:
 *   jet_sfx_set_limit(9, 3); // At most three explosion sounds per frame.
 */
void jet_sfx_set_limit(uint8_t sfx_id, uint8_t max_per_frame);

// ==========================================
// BACKGROUND MUSIC (tracker files)
// ==========================================

/**
 * Function: jet_play_mod
 * Summary: Starts playing a music file. Despite the name, it plays the whole tracker family: ProTracker .mod, Jet64 .flxm, FastTracker 2 .xm, and Scream Tracker 3 .s3m.
 * Parameters:
 *   - filename: The name of a .mod, .flxm, .xm or .s3m file in data/mod/. Or a name beginning "user:", such as "user:mysong.xm", which looks in the cart's own music/ folder in AppData, so a player can drop a file in and hear it. The "user:" names play on every host, from the cart's own music/ folder in AppData, which on the web is kept in the browser.
 * Returns: The play's ticket: a number, never JET_BGM_NO_SONG, that names this one request. Keep it to ask jet_bgm_song whether the song has started, jet_bgm_refused whether the host refused it, jet_bgm_traversals how many times it has come round, and jet_bgm_title what it calls itself. A NULL filename sends nothing and returns JET_BGM_NO_SONG.
 * Include: jet_audio.h
 * Notes: The call sends a request to the host, the program that runs your cartridge, which owns the music player, and returns at once without waiting for it. The host starts the song some audio buffers later; until then the music, and BGM_TRACKER, are still the song before, which is what the ticket tells apart. A play whose file cannot be found, read or understood as music is refused instead, and never starts: jet_bgm_refused tells you so. A play the host has not yet served when you send another is replaced by it: the host starts only the newer song, and the older ticket never plays. The file name is copied into the request and cut to at most 250 characters, which is all the request can carry. The host looks at the file to tell which kind it is and plays each kind natively: a standard Amiga .mod; a Jet64 Tune Grid .flxm, which is a .mod carrying Jet64's own song data inside it; a FastTracker 2 .xm, with up to 32 channels, 16-bit samples and instrument envelopes; and a Scream Tracker 3 .s3m, with up to 32 channels, each sample's own C-4 tuning, ST3's own effect commands, and OPL2 AdLib instruments played register for register beside the sampled ones. A "user:" path is checked on the host so it cannot reach outside the folder, exactly as a jet_fopen path is, and a cart can list that folder with jet_ls("music").
 * Example:
 *   uint32_t theme = jet_play_mod("theme.flxm");
 */
uint32_t jet_play_mod(const char* filename);

/**
 * Function: jet_bgm_song
 * Summary: Which of your plays the background music is playing now, as the ticket jet_play_mod gave it.
 * Parameters:
 *   - None.
 * Returns: The playing song's ticket, or JET_BGM_NO_SONG while none of your songs is playing.
 * Include: jet_audio.h
 * Notes: The host starts a song some audio buffers after jet_play_mod asks for it. Comparing this with the ticket your play returned is how you know the song has started: once they match, BGM_TRACKER shows your song's pattern too, because the host publishes the window before the ticket. It answers JET_BGM_NO_SONG before any song has started, after jet_stop_mod, and while the workstation's Tune Grid auditions a song of its own. A play whose file cannot be found, read or understood as music never becomes the song playing: whatever played before it plays on, under its own ticket, and jet_bgm_refused tells that play from one not yet started. A paused song is still the song playing. The agent door (jet64 --agent) plays no music, so there it always answers JET_BGM_NO_SONG.
 * Example:
 *   bool theme_started = jet_bgm_song() == theme; // theme = jet_play_mod("theme.xm")
 */
static inline uint32_t jet_bgm_song(void) {
    return jet_atomic_load_u32(&BGM_TRANSPORT->ticket);
}

/**
 * Function: jet_bgm_refused
 * Summary: Whether the host has refused one of your plays, so that it will never start: its file could not be found, read or understood as music.
 * Parameters:
 *   - song: The ticket jet_play_mod returned for the play.
 * Returns: true once the host has refused the play; false while it may yet start, once it has started, and for JET_BGM_NO_SONG.
 * Include: jet_audio.h
 * Notes: Until the host has served a play, jet_bgm_song still names the song before it, and a play the host refuses leaves it that way for good, so a cartridge waiting for its song to start could not otherwise tell "not yet" from "never". The host keeps only its newest refusal, so ask about the play you are waiting for, which is your newest: once a later play is refused, an earlier one reads false again. A play replaced before the host served it (see jet_play_mod) is neither started nor refused. The agent door (jet64 --agent) plays no music, but it refuses the plays every other host would, so a cartridge's answer to a missing file can be proved there.
 * Example:
 *   if (jet_bgm_refused(theme)) theme = jet_play_mod("fallback.mod");
 */
static inline bool jet_bgm_refused(uint32_t song) {
    return song != JET_BGM_NO_SONG && jet_atomic_load_u32(&BGM_TRANSPORT->refused) == song;
}

/**
 * Function: jet_bgm_traversals
 * Summary: How many times one of your songs has come round since it started: 0 during its first time through, 1 once it has played through once, and so on.
 * Parameters:
 *   - song: The ticket jet_play_mod returned for the song.
 * Returns: How many times the song has come round, or 0 while it is not the song playing: before it has started, once it has stopped, or after another has replaced it.
 * Include: jet_audio.h
 * Notes: The background music loops forever, and this count is how a cartridge hears each loop. A song comes round when it plays off the end of its order list, and when a jump in it (Bxx or Dxx) lands back on a row it has already played the same way, since from there it can only repeat. The host proves each one exactly, as OpenMPT measures a song's length, so a pattern loop (E6x) or a jump forward never counts, and a seek (jet_seek_mod) starts the next pass from wherever it lands. The count stands still while the music is paused. Keep the last count you acted on and act when this one goes past it: that is a playlist moving to its next track, or a jingle ending after it has played once.
 * Example:
 *   if (jet_bgm_traversals(jingle) > 0) jingle_finished();
 */
static inline uint32_t jet_bgm_traversals(uint32_t song) {
    if (song == JET_BGM_NO_SONG || jet_bgm_song() != song) return 0;
    const uint32_t traversals = jet_atomic_load_u32(&BGM_TRANSPORT->traversals);
    // The host takes a ticket down before the fields it names change hands, and
    // a ticket names one song for its whole life, so the same ticket found again
    // proves the count was this song's.
    return jet_bgm_song() == song ? traversals : 0;
}

/**
 * Function: jet_bgm_title
 * Summary: Copies the title one of your songs gives itself inside its module file, for a "now playing" line.
 * Parameters:
 *   - song: The ticket jet_play_mod returned for the song, or jet_bgm_song() for whatever is playing.
 *   - out: Where to copy the title. What is copied always ends in a NUL byte.
 *   - out_size: The size of out in bytes. JET_BGM_TITLE_BYTES holds any title whole; a smaller buffer gets the title cut to fit.
 * Returns: true when the song is the one playing, with its title in out (empty when the module has none); false otherwise, with out left empty. Nothing is copied, and false returned, when out is NULL or out_size is 0.
 * Include: jet_audio.h
 * Notes: This is the title stored inside the file (a .mod, .xm or .s3m header, or the title a Tune Grid .flxm was saved with), not the file's name, which you already know because you asked for it. The host gives it as printable ASCII, with every other character shown as '?', so any text call can print it as it is. Plenty of modules carry the tracker's name, the composer's, or nothing at all in this field.
 * Example:
 *   char title[JET_BGM_TITLE_BYTES];
 *   if (jet_bgm_title(jet_bgm_song(), title, sizeof title)) jet_print(title, 8, 8, 7);
 */
static inline bool jet_bgm_title(uint32_t song, char* out, size_t out_size) {
    if (out == NULL || out_size == 0) return false;
    out[0] = '\0';
    if (song == JET_BGM_NO_SONG || jet_bgm_song() != song) return false;

    const size_t limit = (out_size < JET_BGM_TITLE_BYTES ? out_size : JET_BGM_TITLE_BYTES) - 1;
    size_t length = 0;
    while (length < limit) {
        const char character = BGM_TRANSPORT->title[length];
        if (character == '\0') break;
        out[length++] = character;
    }
    out[length] = '\0';

    // Every byte copied is ordered before the ticket is read again, and a ticket
    // names one song for its whole life: the same ticket found proves the title
    // was this song's, whole.
    jet_atomic_fence();
    if (jet_bgm_song() == song) return true;
    out[0] = '\0';
    return false;
}

/**
 * Function: jet_stop_mod
 * Summary: Stops the background music at once.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Sound effects on the 16 channels keep playing; this touches the music only. It stops only music the cartridge started: in the workstation, a song the Tune Grid is auditioning is never silenced by it. The cartridge's music also stops by itself when you rebuild the cart with Warm Boot off, because a cold boot resets the music just as it resets WRAM, the cartridge's working memory; with Warm Boot on, the music keeps playing across a rebuild.
 * Example:
 *   jet_stop_mod();
 */
void jet_stop_mod(void);

/**
 * Function: jet_pause_mod
 * Summary: Freezes the background music where it is, so jet_resume_mod can carry on from the exact frame it paused on.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: The host holds the song's playhead still and stops mixing the song in, through a gate built to avoid clicks, so pausing never clicks, and any synth notes still sounding fade out musically instead of droning on. It touches the cartridge's music only: in the workstation, a song the Tune Grid is auditioning is never frozen by it. Pausing music that is already paused, or stopped, does nothing and does no harm. While the music is paused the AUDIO SCOPE tap keeps running and fills with silence, so a visualiser takes its idle path on its own.
 * Example:
 *   jet_pause_mod();   // the menu opened
 */
void jet_pause_mod(void);

/**
 * Function: jet_resume_mod
 * Summary: Carries the background music on from the exact frame jet_pause_mod froze it on.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: The partner of jet_pause_mod. Voices start again on their own as the song reaches its next notes. Resuming music that is not paused, or not playing, does nothing and does no harm.
 * Example:
 *   jet_resume_mod(); // the menu closed
 */
void jet_resume_mod(void);

/**
 * Function: jet_seek_mod
 * Summary: Jumps the playing background music to a position in the song. This is the call behind a seek bar or a chapter skip.
 * Parameters:
 *   - order: The position in the song's order list, counting from 0. A value past the last position lands on the last one.
 *   - row: The row inside that position's pattern, counting from 0. A value past the pattern's last row lands on the last one.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: A tracker song is a list of patterns played in order, and each pattern is a grid of rows. This call does what a song's own position-jump command (Bxx or Dxx) does, but when you say so: the song cursor moves to (order, row), any voice still sounding is cut through the host's no-click path, and playback goes on from there at the next row. Effects that were running before the jump, such as slides, queued jumps and pattern loops, are cleared; each channel keeps its instrument and volume, exactly as it does across a pattern jump in the song. It touches the cartridge's music only, like jet_pause_mod: a Tune Grid audition is never moved by it, and seeking when nothing is playing does nothing and does no harm. You can read the live position back from BGM_TRACKER->pattern_pos and line_pos.
 * Example:
 *   jet_seek_mod(4, 0); // jump to the top of order 4
 */
void jet_seek_mod(uint8_t order, uint8_t row);

/**
 * Constant: JET_MOD_MACHINE_NONE
 * Summary: Give it to jet_mod_set_machine to play .mod files the plain way, which is how a cartridge starts.
 * Include: jet_audio.h
 * Notes: The starting value, so a cartridge that never calls jet_mod_set_machine sounds exactly as it always has.
 * Example:
 *   jet_mod_set_machine(JET_MOD_MACHINE_NONE);
 */
#define JET_MOD_MACHINE_NONE 0u
// Firmware's power-on reset of the master-volume register names the .mod machine by its value
// (jet_core.h), since firmware never includes this header; the two must agree.
_Static_assert(JET_MOD_MACHINE_NONE == JET_AUDIO_MASTER_MOD_MACHINE_RESET,
               "firmware resets the .mod machine to JET_MOD_MACHINE_NONE");
/**
 * Constant: JET_MOD_MACHINE_AGA
 * Summary: Give it to jet_mod_set_machine when the music came off an AGA Amiga: an A1200, A600, A4000 or CD32.
 * Include: jet_audio.h
 * Notes: These machines have almost no filter of their own, so you hear the chip nearly bare: bright, and clean.
 * Example:
 *   jet_mod_set_machine(JET_MOD_MACHINE_AGA);
 */
#define JET_MOD_MACHINE_AGA 1u
/**
 * Constant: JET_MOD_MACHINE_A500
 * Summary: Give it to jet_mod_set_machine when the music came off an A500, A1000 or A2000.
 * Include: jet_audio.h
 * Notes: These machines have a fixed filter at 4.9 kHz that takes the top off everything. It is warmer than JET_MOD_MACHINE_AGA, and noticeably duller; pick it because the music was made for it, not because it sounds better.
 * Example:
 *   jet_mod_set_machine(JET_MOD_MACHINE_A500);
 */
#define JET_MOD_MACHINE_A500 2u

/**
 * Function: jet_mod_set_machine
 * Summary: Says which Amiga a .mod file's music was made on, so the host plays it back the way that machine sounded.
 * Parameters:
 *   - machine: JET_MOD_MACHINE_NONE for plain playback, JET_MOD_MACHINE_AGA for an A1200, A600, A4000 or CD32, or JET_MOD_MACHINE_A500 for an A500, A1000 or A2000.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: A .mod file is Amiga music, and an Amiga did not play a sample back the way a modern sound card does: its sound chip, Paula, held each byte for a fixed slice of time and then stepped to the next, and a step happened when the chip said so and not when the sound card was ready for one. Playing the file the plain way rounds every one of those steps onto the sound card's own grid, and that rounding is a hiss and a grit spread right across the sound — measured on DGalaga's own music at about a sixth of the music's own level, inside the range you can hear. Naming the machine hands the song to a reconstruction that puts every step back at the moment it happened, which measures about seventeen decibels cleaner. Say it once, before jet_play_mod; it stays set until you change it, and it does nothing to a .xm or .s3m file, which is not Amiga music. It costs about a tenth of one per cent of a processor core.
 * Example:
 *   jet_mod_set_machine(JET_MOD_MACHINE_AGA); // Deluxe Galaga only ever ran on AGA
 *   jet_play_mod("music.mod");
 */
void jet_mod_set_machine(uint8_t machine);

/**
 * Function: jet_mod_machine
 * Summary: Reads back the Amiga jet_mod_set_machine last named.
 * Parameters:
 *   - None.
 * Returns: The JET_MOD_MACHINE_* value last set, or JET_MOD_MACHINE_NONE if the cartridge has not set one.
 * Include: jet_audio.h
 * Example:
 *   if (jet_mod_machine() == JET_MOD_MACHINE_NONE) jet_mod_set_machine(JET_MOD_MACHINE_AGA);
 */
uint8_t jet_mod_machine(void);

// ==========================================
// OLDER CALLS, KEPT FOR OLDER CARTRIDGES
// ==========================================

/**
 * Function: jet_sfx
 * Summary: Plays a simple sound the old way, by writing a waveform straight into a channel.
 * Parameters:
 *   - channel: The channel to play on, from 0 to 15.
 *   - waveform: The shape of the wave: JET_WAVE_SQUARE, JET_WAVE_SAW, JET_WAVE_SINE or JET_WAVE_NOISE.
 *   - pitch: The base pitch, as the raw value written into the channel's pitch register. For these four waves it is not Hertz: it runs from 0, the lowest (about 3.5 Hz), to 2000, the highest (about 3.5 kHz), and anything above 2000 plays as 2000. The 440 in the example plays about 174 Hz.
 *   - vol: How loud, from 0 to 255.
 *   - attack: The envelope's attack value: how the sound rises.
 *   - decay: The envelope's decay value: how the sound dies away.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Kept for older carts and quick experiments. For a real game, make a patch in the SFX Editor and play it with jet_sfx_play or one of its siblings. The sound is exactly what these six values describe and nothing else: no patch from the SFX Editor plays with it, whatever the channel last played or the bank holds, and everything it does not name is off, from the filters and vibrato to the pitch sweep.
 * Example:
 *   jet_sfx(0, JET_WAVE_SQUARE, 440, 200, 4, 24);
 */
void jet_sfx(uint8_t channel, uint8_t waveform, uint16_t pitch, uint8_t vol, uint8_t attack, uint8_t decay);

/**
 * Function: jet_play_sfx
 * Summary: An older call that plays a sound effect from the SFX Editor on a channel you choose, with a pan.
 * Parameters:
 *   - sfx_id: The patch number in the cartridge's bank, from 0 to 255.
 *   - channel: The channel to play on, from 0 to 15.
 *   - pan: Where the sound sits between the speakers: 0 is hard left, 128 is the centre, and 255 is hard right.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Kept so older carts still work. New code should use jet_sfx_play, jet_sfx_play_ext or jet_sfx_play_on.
 * Example:
 *   jet_play_sfx(5, 2, 128);
 */
void jet_play_sfx(uint8_t sfx_id, uint8_t channel, uint8_t pan);

/**
 * Function: jet_bgm_row
 * Summary: The playhead's row within the whole tracker pattern, across the sliding window's slices.
 * Parameters:
 *   - None.
 * Returns: The row from 0 to 255, or 0 when no tracker music is playing.
 * Include: jet_audio.h
 * Notes: This is the one honest playhead. BGM_TRACKER->line_pos indexes the published 64-row window and therefore falls back to 0 at every slice crossing of a taller XM pattern, so anything that watches the playhead advance — an end-of-song detector, a row-edge tap, a chart cursor — must read it here. Pair it with BGM_TRACKER->pattern_pos for the song-order position.
 * Example:
 *   uint32_t row = jet_bgm_row();
 */
static inline uint32_t jet_bgm_row(void) {
    return (uint32_t)BGM_TRACKER->line_base + (uint32_t)BGM_TRACKER->line_pos;
}

// ====================== THE MASTER VOLUMES AND THE EQUALISER ======================
// How loud, and how coloured, the host mixes everything this module plays.

/**
 * Function: jet_host_set_volume_sfx
 * Summary: Sets the host's master volume for sound effects and the synthesiser.
 * Parameters:
 *   - volume: The volume, from 0.0f (silent) to 1.0f (full).
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Returns straight away and never waits. It writes one shared register
 *        that the host reads each frame, with no back-and-forth, so you can
 *        call it every frame to fade the sound up or down. The host clamps the
 *        value to between 0.0 and 1.0.
 * Example:
 *   jet_host_set_volume_sfx(0.8f);
 */
void jet_host_set_volume_sfx(float volume);

/**
 * Function: jet_host_set_volume_bgm
 * Summary: Sets the host's master volume for the music, which the mod-tracker engine plays.
 * Parameters:
 *   - volume: The volume, from 0.0f (silent) to 1.0f (full).
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Returns straight away and never waits. It writes one shared register
 *        that the host reads each frame, with no back-and-forth, so you can
 *        call it every frame to fade the music up or down. The host clamps the
 *        value to between 0.0 and 1.0.
 * Example:
 *   jet_host_set_volume_bgm(0.5f);
 */
void jet_host_set_volume_bgm(float volume);

/**
 * Function: jet_host_eq_set_mode
 * Summary: Chooses who controls the host's master graphic equaliser, a ten-band EQ that boosts or cuts the sound at ten frequencies.
 * Parameters:
 *   - mode: JET_AUDIO_EQ_INHERIT to use the EQ the project set up (this is how
 *           it starts), JET_AUDIO_EQ_CART to use the gains this cart sets with
 *           jet_host_eq_set_band, or JET_AUDIO_EQ_BYPASS to turn the EQ off.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: Returns straight away and never waits. It writes a shared block of
 *        memory that the host's mixer reads once per audio buffer, the same
 *        way the volume register works, so you can call it every frame. The
 *        host puts the block back to JET_AUDIO_EQ_INHERIT every time a cart
 *        starts or restarts, so if you saved an EQ, set it again in _init.
 * Example:
 *   jet_host_eq_set_mode(JET_AUDIO_EQ_CART);
 */
void jet_host_eq_set_mode(uint32_t mode);

/**
 * Function: jet_host_eq_mode
 * Summary: Tells you which equaliser mode is set right now.
 * Parameters:
 *   - None.
 * Returns: The JET_AUDIO_EQ_* mode that was written last, or JET_AUDIO_EQ_INHERIT if nothing has been written since the cart started.
 * Include: jet_audio.h
 * Example:
 *   bool eq_on = jet_host_eq_mode() == JET_AUDIO_EQ_CART;
 */
uint32_t jet_host_eq_mode(void);

/**
 * Function: jet_host_eq_set_band
 * Summary: Boosts or cuts one band of the host's master graphic equaliser, in decibels.
 * Parameters:
 *   - band: Which band, from 0 to JET_AUDIO_EQ_BAND_COUNT - 1, counted from the
 *           lowest frequency to the highest. jet_host_eq_band_hz tells you
 *           each band's centre frequency.
 *   - db: How much to boost (positive) or cut (negative), in decibels. The
 *         host clamps it to plus or minus JET_AUDIO_EQ_BAND_DB_MAX.
 * Returns: Nothing.
 * Include: jet_audio.h
 * Notes: It only changes the sound while the mode is JET_AUDIO_EQ_CART. It
 *        returns straight away and never waits, so you can call it every
 *        frame, for example while the player drags a slider; the host only
 *        rebuilds its filters when a gain actually changes.
 * Example:
 *   jet_host_eq_set_band(0, 6.0f); // a warm boost to the bass, at 31 Hz
 */
void jet_host_eq_set_band(int band, float db);

/**
 * Function: jet_host_eq_band
 * Summary: Tells you the gain set on one band of the master graphic equaliser, in decibels.
 * Parameters:
 *   - band: Which band, from 0 to JET_AUDIO_EQ_BAND_COUNT - 1.
 * Returns: The gain that was written last; 0.0f if nothing has been written since the cart started, or if band is out of range.
 * Include: jet_audio.h
 * Example:
 *   float bass = jet_host_eq_band(0);
 */
float jet_host_eq_band(int band);

/**
 * Function: jet_host_eq_band_hz
 * Summary: Tells you the centre frequency of one band of the master graphic equaliser, in hertz.
 * Parameters:
 *   - band: Which band, from 0 to JET_AUDIO_EQ_BAND_COUNT - 1.
 * Returns: The band's centre frequency, from 31 Hz up to 16 kHz, one octave
 *          apart on the standard ISO ladder; or 0.0f if band is out of range.
 * Include: jet_audio.h
 * Notes: The numbers come from the host's own band table, so a label you draw
 *        from this call can never disagree with what the engine is doing.
 * Example:
 *   int hz = (int)jet_host_eq_band_hz(3); // 250
 */
float jet_host_eq_band_hz(int band);

// Short names kept so older carts still build.
#define sfx jet_sfx
#define play_sfx jet_play_sfx
#define play_mod jet_play_mod
#define stop_mod jet_stop_mod

#endif // JET_AUDIO_H
