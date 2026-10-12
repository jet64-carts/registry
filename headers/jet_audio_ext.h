// runtime/jet_audio_ext.h
#ifndef JET_AUDIO_EXT_H
#define JET_AUDIO_EXT_H

// ============================================================================
// Jet64 audio extensions: fades, crossfades, ducking, tremolo and glides
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// It is deliberately kept out of jet64.h so it costs nothing to compile unless
// a cart asks for it: the cart resolver only compiles jet_audio_ext.c when a
// cart's #include tree reaches this header. Carts that never fade audio pay
// zero compile time and zero binary size.
// </nocodex>
//
//   #include "jet64.h"
//   #include "jet_audio_ext.h"   // <-- explicit opt-in
//
// Make sound come and go smoothly instead of switching on and off. With these
// calls you can fade the music, a sound effect, or the whole mix in or out
// over a number of seconds; crossfade from one song or sound to another; duck
// the music (dip it under a louder sound, then bring it back); give a sound a
// tremolo (a steady throb in its volume); and glide a sound's pitch up or down
// like a siren. This header is not part of jet64.h; include it yourself, as
// shown above, when you want it.
//
// <nocodex>
// Everything here is a pure cart-side convenience layer riding the existing
// read-only jet_ API (live SFX register volume rides, the host BGM/SFX master
// volume levers, and the steal-proof SFX voice-generation handles). It adds no
// new host capability and touches no runtime ABI.
// </nocodex>
//
// ----------------------------------------------------------------------------
// THE ONE INTEGRATION RULE
// ----------------------------------------------------------------------------
// Call jet_audio_ext_update(dt) once, and only once, in every _update, and
// pass it the same dt the runtime gives you. That one call is what moves every
// fade, crossfade, duck, tremolo and glide along. Without it, nothing here
// moves at all. If your audio snaps to its new volume instead of fading, you
// forgot this call.
//
//   void _update(float dt) {
//       jet_audio_ext_update(dt);
//       // ... your game logic ...
//   }
//
// ----------------------------------------------------------------------------
// STEAL-SAFETY (SFX)
// ----------------------------------------------------------------------------
// Every sound-effect fade, tremolo and glide names its sound by a HANDLE from
// jet_sfx_handle, never by a bare channel number. A handle is a small value
// that stands for one particular playing of a sound. A channel the runtime
// picked for you can be taken over, or "stolen", by a newer sound, so a
// channel number you kept can quietly start meaning a different sound. A
// handle remembers which playing it was made for, so the moment its channel
// is handed to another sound, any fade on that handle cancels itself and does
// no harm. It can never fade, stop, or mangle a sound it was not made for.
//
//   int h = jet_sfx_handle(jet_sfx_play_on(15, SFX_ENGINE, 1.0f, 128, 200));
//   jet_sfx_fade_out(h, 1.5f, JET_FADE_EXP);   // h, not the raw channel
//
// ----------------------------------------------------------------------------
// VOLUME OWNERSHIP
// ----------------------------------------------------------------------------
// Music fades work by moving the host's master volume for music. Master fades
// and ducking work by moving the host's master volumes for music and for
// sound effects. Once you use these calls, let this header own those master
// volumes: if you move them yourself with jet_host_set_volume_bgm or
// jet_host_set_volume_sfx in the middle of a fade, the two of you will fight.
// A fade on a single sound effect moves only that voice's own volume and never
// touches the masters.
//
// Complete music-fade example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_audio_ext.h"
//
//   void _init(void) {
//       jet_bgm_play_fade_in("theme.flxm", 2.0f, JET_FADE_SINE);
//   }
//
//   void _update(float dt) {
//       jet_audio_ext_update(dt);
//       if (btnp(BTN_B)) jet_bgm_fade_out(1.5f, JET_FADE_EXP);
//   }
//
//   void _draw(void) {
//       jet_cls(0);
//       jet_print("PRESS B TO FADE OUT", 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_audio_ext_reset();
//   }
// ============================================================================

#include "jet_core.h"  // JetAudioRegister, AUDIO, hardware pointers, bool/stdint
#include "jet_audio.h" // jet_sfx_handle_channel/stop_handle, jet_play_mod/stop
#include "jet_host.h"  // jet_host_set_volume_bgm/sfx

/**
 * Enum: JetFadeShape
 * Summary: The shape of a fade: how the volume travels from where it starts to where it is going.
 * Include: jet_audio_ext.h
 * Notes: Every shape moves in one direction only and never overshoots, so a fade can never go past silent or past full. (The curves in jet_ease.h are made for motion, and their bounce, elastic and back shapes would push a volume below zero or past full, so do not use them for sound.) Pick a shape by ear:
 *          - JET_FADE_LINEAR: the volume changes by the same amount every second. Simple, but it seems to fade a little fast near the top, because hearing works on a logarithmic scale: each halving of the volume sounds like the same-sized step down.
 *          - JET_FADE_SINE: a smooth S-curve that starts and ends gently. A pleasant everyday choice.
 *          - JET_FADE_EXP: the volume changes by the same number of decibels every second, across a 60 dB range. This is the classic film-style music fade, and the most natural-sounding way to fade a song in or out.
 *          - JET_FADE_EQUAL_POWER: a square-law curve, shaped so that the two halves of a crossfade add up to the same loudness all the way through, with no dip in the middle. Best for crossfades.
 *        JET_FADE_EXP and JET_FADE_EQUAL_POWER know which way they are going, so both halves of a crossfade come out right.
 * Example:
 *   jet_bgm_crossfade("boss.flxm", 1.0f, 1.5f, JET_FADE_EQUAL_POWER);
 */
typedef enum {
    JET_FADE_LINEAR = 0,      // the same change in volume every second
    JET_FADE_SINE = 1,        // a smooth S-curve, gentle at both ends
    JET_FADE_EXP = 2,         // the same change in decibels every second; the film-style music fade
    JET_FADE_EQUAL_POWER = 3, // keeps a crossfade's loudness steady all the way through
} JetFadeShape;

// ==========================================
// THE UPDATE CALL (you must make it)
// ==========================================

/**
 * Function: jet_audio_ext_update
 * Summary: Moves every running fade, crossfade, duck, tremolo and glide forward by dt seconds.
 * Parameters:
 *   - dt: How much time has passed, in seconds: the same dt the runtime gives your _update.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Call it once in every _update, and nowhere else. It is the only thing that moves the fades along; without it, nothing in this header does anything.
 *
 *        When nothing is fading it costs almost nothing, just a few checks of on-or-off flags. It writes the host's master volume only on the updates where a music, master or duck fade is actually moving, and that write is a plain store that never waits. A negative dt is treated as 0.
 * Example:
 *   void _update(float dt) {
 *       jet_audio_ext_update(dt);
 *   }
 */
void jet_audio_ext_update(float dt);

/**
 * Function: jet_audio_ext_reset
 * Summary: Cancels every running fade, duck, tremolo and glide, and puts the music and sound-effect master volumes back to full.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Use it when you reset a scene from scratch, or when you want to go back to controlling the volume with plain jet_ calls. It does not stop any sound; it only lets go of the volumes this header was holding. The volume of each sound-effect voice is left wherever it is.
 * Example:
 *   jet_audio_ext_reset();
 */
void jet_audio_ext_reset(void);

// ==========================================
// BACKGROUND MUSIC (BGM) FADES
// ==========================================

/**
 * Function: jet_bgm_fade_to
 * Summary: Fades the music's master volume from where it is now to a level you choose, over a number of seconds.
 * Parameters:
 *   - level: The level to fade to, from 0.0 (silent) to 1.0 (full). A value outside that range is clamped to it.
 *   - seconds: How long the fade takes, in seconds. 0 or less jumps to the level at once.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: This is the all-purpose music fade: use it for a swell, to bring a song down under a cutscene, or for a fade that stops partway. Fading to 0 makes the music silent but does not stop it; use jet_bgm_fade_out when you want the song to stop as well. If a music fade is already running, this one replaces it and starts from wherever the volume is at that moment.
 * Example:
 *   jet_bgm_fade_to(0.3f, 1.0f, JET_FADE_SINE); // duck the music down to 30%
 */
void jet_bgm_fade_to(float level, float seconds, JetFadeShape shape);

/**
 * Function: jet_bgm_fade_in
 * Summary: Drops the music's master volume to silence at once, then fades it up to full over a number of seconds.
 * Parameters:
 *   - seconds: How long the fade takes, in seconds. 0 or less jumps straight to full.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: The volume is set to 0 before this call returns, so there is never a first frame of music blasting at full volume. Call it right after jet_play_mod, or use jet_bgm_play_fade_in, which does both in one call.
 * Example:
 *   jet_play_mod("theme.flxm");
 *   jet_bgm_fade_in(5.0f, JET_FADE_LINEAR);
 */
void jet_bgm_fade_in(float seconds, JetFadeShape shape);

/**
 * Function: jet_bgm_fade_out
 * Summary: Fades the music's master volume down to silence over a number of seconds, then stops the song.
 * Parameters:
 *   - seconds: How long the fade takes, in seconds. 0 or less stops the song at once.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Once the fade reaches silence it calls jet_stop_mod for you, so the song ends cleanly with nothing left hanging. To fade the music down but keep the song playing, ready to swell back up later, use jet_bgm_fade_to(0.0f, ...) instead.
 * Example:
 *   jet_bgm_fade_out(2.0f, JET_FADE_EXP); // graceful end-of-level fade
 */
void jet_bgm_fade_out(float seconds, JetFadeShape shape);

/**
 * Function: jet_bgm_play_fade_in
 * Summary: Starts a song at silence and fades it up to full: the one-call way to fade music in.
 * Parameters:
 *   - filename: The name of the song file, a .flxm or .mod file in data/mod/, just as jet_play_mod takes it.
 *   - seconds: How long the fade takes, in seconds.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: The play's ticket, exactly as jet_play_mod returns it, for jet_bgm_song, jet_bgm_refused, jet_bgm_traversals and jet_bgm_title; JET_BGM_NO_SONG for a NULL filename, which changes nothing.
 * Include: jet_audio_ext.h
 * Notes: It does the same as setting the music's master volume to 0, calling jet_play_mod(filename), then calling jet_bgm_fade_in(seconds, shape), in that order, so the song can never be heard at full volume for a frame before the fade begins. A good fit for _init.
 * Example:
 *   void _init(void) { theme = jet_bgm_play_fade_in("theme.flxm", 5.0f, JET_FADE_LINEAR); }
 */
uint32_t jet_bgm_play_fade_in(const char* filename, float seconds, JetFadeShape shape);

/**
 * Function: jet_bgm_crossfade
 * Summary: Fades the song that is playing out, switches to a new song, and fades that one in.
 * Parameters:
 *   - filename: The name of the new song, a .flxm or .mod file in data/mod/.
 *   - out_seconds: How long the old song takes to fade out, in seconds.
 *   - in_seconds: How long the new song takes to fade in, in seconds.
 *   - shape: The shape of both fades, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: The two fades happen one after the other, not at the same time. The host can play only one song at a time, so the two songs never overlap, and the whole change takes out_seconds + in_seconds. The switch to the new song happens by itself at the silent point in the middle, so this call cannot hand you the new song's ticket: jet_bgm_song names it once the host has started it. (A crossfade in which both songs are heard at once would need a second music player inside the host, which carts do not have.)
 * Example:
 *   jet_bgm_crossfade("boss.flxm", 1.5f, 1.5f, JET_FADE_EQUAL_POWER);
 */
void jet_bgm_crossfade(const char* filename, float out_seconds, float in_seconds, JetFadeShape shape);

/**
 * Function: jet_bgm_fade_active
 * Summary: Tells you whether a music fade, or either half of a music crossfade, is running right now.
 * Parameters:
 *   - None.
 * Returns: true while a music fade is running, or a crossfade is still waiting to switch songs; otherwise false.
 * Include: jet_audio_ext.h
 * Notes: This answers only for music fades and crossfades, the ones jet_audio_ext_update moves along. Ducking is kept separately and is not counted.
 * Example:
 *   if (!jet_bgm_fade_active()) jet_bgm_fade_out(2.0f, JET_FADE_EXP);
 */
bool jet_bgm_fade_active(void);

// ==========================================
// MASTER / SCENE FADES (BGM + SFX together)
// ==========================================

/**
 * Function: jet_master_fade_to
 * Summary: Fades everything you can hear, music and sound effects together, to a level you choose over a number of seconds.
 * Parameters:
 *   - level: The level to fade to, from 0.0 (silent) to 1.0 (full). A value outside that range is clamped to it.
 *   - seconds: How long the fade takes, in seconds. 0 or less jumps to the level at once.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Think of it as a dimmer over the whole scene. It sits on top of any music fade and any duck, and the levels multiply, so fading the whole mix to silence for a cutscene works even while the music is in the middle of its own fade. It goes well with a fade on the screen.
 * Example:
 *   jet_master_fade_to(0.0f, 0.5f, JET_FADE_SINE); // duck everything for a hit-stop
 */
void jet_master_fade_to(float level, float seconds, JetFadeShape shape);

/**
 * Function: jet_master_fade_out
 * Summary: Fades everything you can hear, music and sound effects together, down to silence over a number of seconds.
 * Parameters:
 *   - seconds: How long the fade takes, in seconds.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: A short way of writing jet_master_fade_to(0.0f, ...). Nothing stops playing; only the master volumes go to silence, so a fade back in picks up exactly where the sound has got to.
 * Example:
 *   jet_master_fade_out(1.0f, JET_FADE_SINE); // fade to black on player death
 */
void jet_master_fade_out(float seconds, JetFadeShape shape);

/**
 * Function: jet_master_fade_in
 * Summary: Drops everything you can hear to silence at once, then fades it back up to full over a number of seconds.
 * Parameters:
 *   - seconds: How long the fade takes, in seconds.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Both master volumes are set to 0 before this call returns, so a scene opens from true silence. It is the partner of jet_master_fade_out for changing scenes.
 * Example:
 *   jet_master_fade_in(1.0f, JET_FADE_SINE);
 */
void jet_master_fade_in(float seconds, JetFadeShape shape);

/**
 * Function: jet_master_fade_active
 * Summary: Tells you whether a master fade, the one that dims music and sound effects together, is running right now.
 * Parameters:
 *   - None.
 * Returns: true while a master fade is running; otherwise false.
 * Include: jet_audio_ext.h
 * Notes: This answers only for the master fade. Fades on single sound-effect voices and music ducking are not counted.
 * Example:
 *   if (jet_master_fade_active()) return; // wait for the transition to finish
 */
bool jet_master_fade_active(void);

// ==========================================
// BGM DUCKING (the music dips under a louder sound)
// ==========================================

/**
 * Function: jet_bgm_duck
 * Summary: Dips the music under a louder sound, holds it down for a moment, then brings it back up to full by itself.
 * Parameters:
 *   - depth: How far to dip, from 0.0 (no dip) to 1.0 (all the way to silence). 0.6 takes the music down to 40 percent.
 *   - hold_seconds: How long the music stays down, in seconds, before it starts to come back.
 *   - release_seconds: How long the climb back to full takes, in seconds.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: This is the ducker that just works, for a line of speech, a short musical hit, or an announcement: call it when the loud sound starts, and the music gets out of the way and comes back on its own. The dip down is quick and always the same speed. Ducking multiplies with any music fade and any master fade, so the three work together without fighting. When you do not know how long the loud thing will last, use jet_bgm_duck_to instead.
 * Example:
 *   jet_sfx_play(SFX_VOICEOVER);
 *   jet_bgm_duck(0.7f, 1.2f, 0.6f); // get the music out from under the line, then bring it back
 */
void jet_bgm_duck(float depth, float hold_seconds, float release_seconds);

/**
 * Function: jet_bgm_duck_to
 * Summary: Fades the music's duck level to a value you choose and holds it there until you change it.
 * Parameters:
 *   - level: The duck level, a multiplier on the music's volume from 0.0 (silent) to 1.0 (no ducking at all). A value outside that range is clamped to it.
 *   - seconds: How long the change takes, in seconds. 0 or less applies it at once.
 *   - shape: The shape of the change, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: This is the ducker you control by hand. It is kept separate from music fades and master fades, so you can hold the music down under something whose length you do not know, such as an open menu. Duck down when it starts, and call jet_bgm_duck_to(1.0f, ...) to lift the duck when it ends.
 * Example:
 *   jet_bgm_duck_to(0.25f, 0.15f, JET_FADE_SINE); // hold music down while the menu is open
 */
void jet_bgm_duck_to(float level, float seconds, JetFadeShape shape);

// ==========================================
// SFX VOICE FADES (by handle, so a stolen voice does no harm)
// ==========================================

/**
 * Function: jet_sfx_fade_to
 * Summary: Fades one sound effect's volume from where it is now to a level you choose, over a number of seconds.
 * Parameters:
 *   - handle: The sound's handle, from jet_sfx_handle. A handle is a small value that stands for one particular playing of a sound. Do not pass a bare channel number.
 *   - target_vol: The volume to fade to, from 0 to 255, in the voice's own volume units.
 *   - seconds: How long the fade takes, in seconds. 0 or less jumps to the volume at once.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: This is the all-purpose fade for one sound effect. If the handle is stale, because its channel has been taken over by a newer sound, or is not a valid handle at all, the call quietly does nothing. Starting any volume fade on a channel replaces a tremolo already running there, because both write the same volume byte.
 * Example:
 *   int h = jet_sfx_handle(jet_sfx_play_on(15, SFX_ENGINE, 1.0f, 128, 200));
 *   jet_sfx_fade_to(h, 80, 0.5f, JET_FADE_SINE); // ease the engine down to idle
 */
void jet_sfx_fade_to(int handle, uint8_t target_vol, float seconds, JetFadeShape shape);

/**
 * Function: jet_sfx_fade_in
 * Summary: Fades one sound effect up from silence to the volume it was started at.
 * Parameters:
 *   - handle: The sound's handle, from jet_sfx_handle.
 *   - seconds: How long the fade takes, in seconds.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: It reads the sound's volume at the moment you call it and keeps that as the target, then drops the volume to 0 straight away, so there is no first-frame blast. So: play the sound at the volume you want it to end up at, take its handle, and call this in the same frame. It does nothing for a stale or invalid handle, or for a sound already at volume 0.
 * Example:
 *   int h = jet_sfx_handle(jet_sfx_play_on(15, SFX_WIND_LOOP, 1.0f, 128, 220));
 *   jet_sfx_fade_in(h, 2.0f, JET_FADE_EXP); // ambience swells in
 */
void jet_sfx_fade_in(int handle, float seconds, JetFadeShape shape);

/**
 * Function: jet_sfx_fade_out
 * Summary: Fades one sound effect down to silence over a number of seconds, then stops it without a click.
 * Parameters:
 *   - handle: The sound's handle, from jet_sfx_handle.
 *   - seconds: How long the fade takes, in seconds. 0 or less stops the sound at once.
 *   - shape: The shape of the fade, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Once the sound is silent it calls jet_sfx_stop_handle for you, which frees the voice cleanly. If the channel has already been given to a newer sound, this does nothing, so it can never cut that new sound off.
 * Example:
 *   jet_sfx_fade_out(engine_handle, 1.0f, JET_FADE_EXP); // engine spins down and stops
 */
void jet_sfx_fade_out(int handle, float seconds, JetFadeShape shape);

/**
 * Function: jet_sfx_crossfade
 * Summary: Fades one sound effect out, and stops it, while fading another in at the same time.
 * Parameters:
 *   - out_handle: The handle of the sound going out. It fades from its current volume to silence, then stops.
 *   - in_handle: The handle of the sound coming in. It fades from silence up to the volume it was started at.
 *   - seconds: How long the crossfade takes, in seconds.
 *   - shape: The shape of both fades, a JetFadeShape. JET_FADE_EQUAL_POWER keeps the loudness steady all the way through the blend.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: Unlike music, sound effects really can play at the same time, since there are 16 channels, so this is a true crossfade with both sounds heard at once. Start the incoming sound at the volume you want it to reach, take both handles, then call this. If either sound's voice is stolen, that half of the crossfade cancels itself and the other half carries on.
 * Example:
 *   int in = jet_sfx_handle(jet_sfx_play_on(14, SFX_RUN, 1.0f, 128, 200));
 *   jet_sfx_crossfade(walk_handle, in, 0.3f, JET_FADE_EQUAL_POWER); // walk -> run loop
 */
void jet_sfx_crossfade(int out_handle, int in_handle, float seconds, JetFadeShape shape);

/**
 * Function: jet_sfx_tremolo
 * Summary: Makes a held sound effect's volume rise and fall in a smooth sine wave, for a pulsing or throbbing effect.
 * Parameters:
 *   - handle: The sound's handle, from jet_sfx_handle.
 *   - rate_hz: How many pulses a second: 6.0 for a fast throb, 1.0 for a slow swell.
 *   - depth: How deep the pulse goes, from 0.0 (no change) to 1.0 (the volume swings all the way down to silence). A value outside that range is clamped to it.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: The sound's volume at the moment you call it becomes the top of the pulse. The tremolo runs until you call jet_sfx_tremolo_stop, start a volume fade on the same channel, or the voice is stolen. It suits looping or held sounds best: an engine, a force field, a throbbing menu. It replaces any volume fade already running on that channel.
 * Example:
 *   int h = jet_sfx_handle(jet_sfx_play_on(15, SFX_FORCE_FIELD, 1.0f, 128, 180));
 *   jet_sfx_tremolo(h, 5.0f, 0.4f); // shimmering force-field hum
 */
void jet_sfx_tremolo(int handle, float rate_hz, float depth);

/**
 * Function: jet_sfx_tremolo_stop
 * Summary: Stops a sound effect's tremolo and puts its volume back to what it was before the tremolo began.
 * Parameters:
 *   - handle: The handle the tremolo was started with.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: The sound is left playing at the top of the pulse, the volume it had before. It does nothing if no tremolo is running on that sound, or if the handle is stale or invalid.
 * Example:
 *   jet_sfx_tremolo_stop(force_field_handle);
 */
void jet_sfx_tremolo_stop(int handle);

/**
 * Function: jet_sfx_glide
 * Summary: Slides a held sound effect's pitch up or down over a number of seconds, the way a siren or a slide whistle does. Musicians call this portamento.
 * Parameters:
 *   - handle: The sound's handle, from jet_sfx_handle.
 *   - target_mult: How much to multiply the pitch by: 2.0 slides up one octave, 0.5 down one octave, and 1.0 leaves it alone.
 *   - seconds: How long the slide takes, in seconds. 0 or less applies the change at once.
 *   - shape: The shape of the slide, a JetFadeShape.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: It moves the voice's pitch_base register from its current value to current * target_mult. It is separate from volume fades, so a sound can fade and glide at the same time. For FM and wavetable sounds the pitch register is in plain Hz, so the slide is exact. For SFXR sounds the register is not a straight Hz value, so the slide is only approximate, but still good for sirens, risers and dive-bombs. If the voice is stolen, the glide cancels itself.
 * Example:
 *   int h = jet_sfx_handle(jet_note_on(SFX_SIREN, 60, 200));
 *   jet_sfx_glide(h, 1.5f, 0.8f, JET_FADE_SINE); // upward siren swoop
 */
void jet_sfx_glide(int handle, float target_mult, float seconds, JetFadeShape shape);

/**
 * Function: jet_sfx_fade_cancel
 * Summary: Stops whatever this header is doing to a sound effect, whether a fade, a tremolo or a glide, and leaves the sound as it is.
 * Parameters:
 *   - handle: The handle of the sound.
 * Returns: Nothing.
 * Include: jet_audio_ext.h
 * Notes: The sound keeps playing, frozen at whatever volume and pitch it has reached. Use it to take a sound back under your own control partway through a fade. It does nothing for a stale or invalid handle.
 * Example:
 *   jet_sfx_fade_cancel(engine_handle); // take over the engine volume manually from here
 */
void jet_sfx_fade_cancel(int handle);

#endif // JET_AUDIO_EXT_H
