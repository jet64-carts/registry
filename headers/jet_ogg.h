// runtime/jet_ogg.h
#ifndef JET_OGG_H
#define JET_OGG_H

// ============================================================================
// Jet64 Ogg Vorbis music: recorded music at full quality (opt-in)
// ============================================================================
//
// <nocodex>
// This is an OPT-IN cartridge extension, NOT part of the core jet_ firmware.
// Like jet_pcm.h it is deliberately kept out of jet64.h, so it costs nothing
// unless a cart asks for it: the cart resolver compiles jet_ogg.c, and with it
// the vendored stb_vorbis decoder (runtime/vendor/stb_vorbis.h), only when a
// cart's #include tree reaches this header. A cart that never plays an .ogg
// ships a player with not one byte of the decoder in it.
//
// The decoding is the cart's, not the host's (docs/audio/ogg.md): jet_ogg.c
// runs inside the cart, turns the file into sound, converts it to the sound
// device's rate with a band-limited resampler, and streams the result into a
// jet_pcm channel as JET_PCM_F32_STEREO. The host only ever plays PCM, so the
// three runtimes share one decoder, byte for byte.
// </nocodex>
//
// Play recorded music at full quality. An .ogg file is recorded sound squeezed
// small, the way an MP3 is: a three-minute song that would take 30 MB as a WAV
// takes about 4 MB as an .ogg, and still sounds like the recording. This
// header plays those files, streaming the music out of the file a little at a
// time, so even a long track costs no more working memory than its file.
//
// Everything runs through a player, a JetOgg. Open one in _init, telling it
// which PCM channel to play on and how big your largest .ogg file is. Then
// play a track on it, and call jet_ogg_update in every _update, which is what
// keeps the music flowing: the player turns a little more of the file into
// sound each time, just enough to stay a tenth of a second ahead of the
// speakers. Stop calling it and the music stops a moment later.
//
// A track plays once or loops. A looping track goes back to its start when it
// reaches its end, with no gap and no click. A track can also carry its own
// loop, so that an introduction plays once and the rest repeats: tag the file
// with LOOPSTART and LOOPLENGTH, both counted in frames at the file's own rate,
// the convention RPG Maker made common, and the player loops there instead.
//
// Stopping, pausing, resuming and changing tracks all fade over 5 ms, so none
// of them clicks, and each takes effect within about a tenth of a second,
// once the sound already on its way to the speakers has played. Volume and
// pan change at once.
//
// This header is opt-in, and it brings jet_pcm.h with it: include it yourself,
// after jet64.h, and only when you want it.
//
//   #include "jet64.h"
//   #include "jet_ogg.h"     // <-- explicit opt-in
//
// Complete music-player example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_ogg.h"
//
//   enum { MUSIC_CHANNEL = 0, LARGEST_TRACK_BYTES = 8 * 1024 * 1024 };
//   static JetOgg* music;
//
//   void _init(void) {
//       music = jet_ogg_open(MUSIC_CHANNEL, LARGEST_TRACK_BYTES);
//       jet_ogg_play(music, "music/theme.ogg", JET_OGG_LOOP);
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) {
//           if (jet_ogg_paused(music)) {
//               jet_ogg_resume(music);
//           } else {
//               jet_ogg_pause(music);
//           }
//       }
//       jet_ogg_update(music);
//   }
//
//   void _draw(void) {
//       char line[48];
//       snprintf(line, sizeof line, "%.1f OF %.1f SECONDS", jet_ogg_position(music),
//                jet_ogg_length(music));
//       jet_cls(0);
//       jet_print(line, 16, 16, 15);
//       jet_print("A PAUSES AND RESUMES", 16, 32, 15);
//   }
//
//   void _end(void) {
//       jet_ogg_stop(music);
//   }

#include "jet_pcm.h" // IWYU pragma: keep (channel numbers and the stream a player feeds; jet_core.h's bool/stdint)

/**
 * Type: JetOgg
 * Summary: One music player: a track, the decoder that turns it into sound, and the PCM channel the sound plays on.
 * Include: jet_ogg.h
 * Notes: Get one from jet_ogg_open, keep the pointer for the whole run, and hand it to the other jet_ogg calls. Do not look inside it: the player keeps all of its state in WRAM, the cartridge's working memory, where jet_ogg_open reserved it. NULL means the open failed, and every call in this header takes NULL safely and does nothing, so a cart whose music could not start still runs. Open one player for each piece of music you want sounding at the same time: two players on two channels can cross-fade from one track to the next.
 * Example:
 *   static JetOgg* music;
 *   void _init(void) { music = jet_ogg_open(0, 8 * 1024 * 1024); }
 */
typedef struct JetOgg JetOgg;

/**
 * Enum: JetOggRepeat
 * Summary: Whether a track plays once or loops.
 * Include: jet_ogg.h
 * Notes: JET_OGG_ONCE plays the track to its end, and then the player falls quiet and jet_ogg_playing turns false. JET_OGG_LOOP plays it over and over, until you stop it or play something else, joining the end to the start with no gap. If the file carries LOOPSTART and LOOPLENGTH tags, a looping track plays once from its very beginning and then repeats only the part those tags mark out. A value that is not one of the two is read as JET_OGG_ONCE.
 * Example:
 *   jet_ogg_play(music, "music/boss.ogg", JET_OGG_LOOP);
 */
typedef enum {
    JET_OGG_ONCE = 0, // play to the end, then fall quiet
    JET_OGG_LOOP = 1, // repeat for ever, at the file's own loop if it has one
} JetOggRepeat;

/**
 * Function: jet_ogg_open
 * Summary: Makes a music player that plays on one PCM channel, reserving all the working memory it will ever need.
 * Parameters:
 *   - channel: The PCM channel the music plays on, from 0 to JET_PCM_CHANNEL_COUNT-1. The player owns it from now on, so leave it to the player and play your other sounds on other channels.
 *   - max_file_bytes: The size, in bytes, of the largest .ogg file you will play on this player. The whole file sits in WRAM while it plays, so this is how much room to keep for it.
 * Returns: A player, or NULL if the channel is out of range, max_file_bytes is 0, or WRAM does not have room, in which case not one byte of WRAM has been taken. A failure logs its reason.
 * Include: jet_ogg.h
 * Notes: CALL IT ONLY IN _init, because it reserves WRAM, which is handed out from the front and never given back. It reserves max_file_bytes for the file plus a little under 1 MiB for the decoder, the resampler and the sound on its way to the speakers, once, so playing track after track never asks for more. A file bigger than max_file_bytes is refused when you try to play it, so size it for your largest track; the size of each file is in your project's data folder. The player starts silent, at full volume, in the centre.
 * Example:
 *   music = jet_ogg_open(0, 8 * 1024 * 1024); // room for tracks up to 8 MiB
 *   if (music == NULL) LOG_WARN("no room for music");
 */
JetOgg* jet_ogg_open(uint8_t channel, uint32_t max_file_bytes);

/**
 * Function: jet_ogg_play
 * Summary: Loads an .ogg file from the cartridge bundle and starts playing it, in place of whatever the player was playing.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 *   - rel_path: The file's path from inside the cartridge's data/ folder, such as "music/theme.ogg" for data/music/theme.ogg. Leave data/ off: the runtime adds it, exactly as jet_asset_open does.
 *   - repeat: JET_OGG_ONCE to play the track to its end, or JET_OGG_LOOP to play it for ever.
 * Returns: true if the track is playing; false if the file is missing, bigger than the player's max_file_bytes, or not a mono or stereo Ogg Vorbis file. Each failure logs its reason, and a failure leaves the player silent.
 * Include: jet_ogg.h
 * Notes: You may call it at any time, not only in _init. If a track is already playing, it fades out over 5 ms and the new one starts straight after it, from its beginning. Loading reads the whole file into the player's own WRAM and sets the decoder up, which takes a few milliseconds, so start a track at a quiet moment, such as between levels, rather than in the middle of the action. The music plays at the sound device's own rate whatever rate the file was made at: the player converts it, carefully enough that you cannot hear the difference. It decodes only files bundled with the cartridge, the ones you put there yourself.
 * Example:
 *   if (!jet_ogg_play(music, "music/level1.ogg", JET_OGG_LOOP)) show_message("MUSIC MISSING");
 */
bool jet_ogg_play(JetOgg* ogg, const char* rel_path, JetOggRepeat repeat);

/**
 * Function: jet_ogg_update
 * Summary: Keeps the music flowing: turns just enough of the file into sound to stay a tenth of a second ahead of the speakers.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: Nothing.
 * Include: jet_ogg.h
 * Notes: CALL IT IN EVERY _update, for every player you opened, whether or not it is playing. Each call does only the work that the sound played since the last one needs, so it costs about the same every frame; a player that is stopped or paused costs almost nothing. If the calls stop, the music runs out a tenth of a second later and falls silent, and it carries on from the same place when they start again.
 * Example:
 *   void _update(float dt) {
 *       (void)dt;
 *       jet_ogg_update(music);
 *   }
 */
void jet_ogg_update(JetOgg* ogg);

/**
 * Function: jet_ogg_stop
 * Summary: Stops the track with a 5 ms fade, and forgets it.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: Nothing.
 * Include: jet_ogg.h
 * Notes: The sound already on its way to the speakers, a tenth of a second at most, plays out first, then the fade. After a stop, jet_ogg_playing is false, jet_ogg_position and jet_ogg_length answer 0, and jet_ogg_resume does nothing; play a track to start again. Stopping a player that is not playing does nothing.
 * Example:
 *   void _end(void) { jet_ogg_stop(music); }
 */
void jet_ogg_stop(JetOgg* ogg);

/**
 * Function: jet_ogg_pause
 * Summary: Pauses the track with a 5 ms fade, keeping its place so jet_ogg_resume can carry on from there.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: Nothing.
 * Include: jet_ogg.h
 * Notes: The sound already on its way to the speakers, a tenth of a second at most, plays out first, then the fade. While paused, jet_ogg_playing is false and jet_ogg_paused is true, and jet_ogg_update costs almost nothing. Pausing a player that is not playing does nothing.
 * Example:
 *   if (menu_opened) jet_ogg_pause(music);
 */
void jet_ogg_pause(JetOgg* ogg);

/**
 * Function: jet_ogg_resume
 * Summary: Carries on playing a paused track from where it was paused, fading in over 5 ms.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: Nothing.
 * Include: jet_ogg.h
 * Notes: Only a paused track resumes; on a player that is playing, stopped or has finished its track, the call does nothing.
 * Example:
 *   if (menu_closed) jet_ogg_resume(music);
 */
void jet_ogg_resume(JetOgg* ogg);

/**
 * Function: jet_ogg_playing
 * Summary: Tells you whether the player is playing a track.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: true from jet_ogg_play until the track is stopped, paused, or, when it plays once, has finished and fallen quiet; otherwise false.
 * Include: jet_ogg.h
 * Notes: A track playing once stays playing until its last sound has reached the speakers, so the moment this turns false is the moment it really ended: a good time to start the next track.
 * Example:
 *   if (!jet_ogg_playing(music) && !jet_ogg_paused(music)) jet_ogg_play(music, "music/next.ogg", JET_OGG_ONCE);
 */
bool jet_ogg_playing(const JetOgg* ogg);

/**
 * Function: jet_ogg_paused
 * Summary: Tells you whether the player's track is paused.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: true from jet_ogg_pause until jet_ogg_resume, jet_ogg_stop or jet_ogg_play; otherwise false.
 * Include: jet_ogg.h
 * Notes: A paused track is not playing, so jet_ogg_playing is false while this is true.
 * Example:
 *   if (jet_ogg_paused(music)) jet_print("PAUSED", 16, 16, 15);
 */
bool jet_ogg_paused(const JetOgg* ogg);

/**
 * Function: jet_ogg_set_volume
 * Summary: Sets how loud the player's music is, at once, for this track and every track after it.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 *   - vol: How loud, from 0 (silent) to 255 (full).
 * Returns: Nothing.
 * Include: jet_ogg.h
 * Notes: It takes effect straight away, even on the sound already on its way to the speakers, so changing it a little every frame makes a smooth fade. Use this rather than jet_pcm_set_volume on the player's channel: the player remembers this volume and keeps it when you play another track. A new player starts at 255.
 * Example:
 *   jet_ogg_set_volume(music, 128); // half volume, under the dialogue
 */
void jet_ogg_set_volume(JetOgg* ogg, uint8_t vol);

/**
 * Function: jet_ogg_set_pan
 * Summary: Sets the balance between the speakers, at once, for this track and every track after it.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 *   - pan: The balance, from 0 to 255: 128 plays both sides just as they were recorded, and turning towards 0 or 255 fades the other side out.
 * Returns: Nothing.
 * Include: jet_ogg.h
 * Notes: Music is played in stereo, so pan works as a balance, the way it does for any stereo JetPcmFormat. A mono file is spread evenly across both speakers, exactly as loud as a mono sound in the centre of a PCM channel. Use this rather than jet_pcm_set_pan on the player's channel: the player remembers it across tracks. A new player starts at 128.
 * Example:
 *   jet_ogg_set_pan(music, 96); // lean the music a little to the left
 */
void jet_ogg_set_pan(JetOgg* ogg, uint8_t pan);

/**
 * Function: jet_ogg_position
 * Summary: How far into the track the music you can hear has got, in seconds.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: The place in the track, in seconds from its start, of the sound reaching the speakers now; 0 when no track is playing or paused. A looping track counts back from its loop point each time round, so the answer always names a place in the file.
 * Include: jet_ogg.h
 * Notes: It follows the sound as it plays, which the sound device's own clock sets, so like the mouse or a joystick it is a real-time input: two runs never read quite the same, and it must never steer your game's rules. Use it to show a track's progress, or to time pictures to the music.
 * Example:
 *   float done = jet_ogg_position(music) / jet_ogg_length(music); // 0 to 1, for a progress bar
 */
float jet_ogg_position(const JetOgg* ogg);

/**
 * Function: jet_ogg_length
 * Summary: How long the player's track is, in seconds.
 * Parameters:
 *   - ogg: The player, from jet_ogg_open.
 * Returns: The track's whole length, in seconds, from its first sound to its last; 0 when no track is playing or paused.
 * Include: jet_ogg.h
 * Notes: The length is the file's, read when the track starts, and it does not change as the track loops.
 * Example:
 *   snprintf(line, sizeof line, "%.0f SECONDS", jet_ogg_length(music));
 */
float jet_ogg_length(const JetOgg* ogg);

#endif // JET_OGG_H
