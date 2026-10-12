// runtime/jet_publish.h
#ifndef JET_PUBLISH_H
#define JET_PUBLISH_H

#include <stdbool.h>
#include <stdint.h>

#include "jet_core.h" // IWYU pragma: keep (the JET_PUBLISH_* field widths)

// Your cartridge's own publishing record: the name, publisher, version,
// copyright line and description you wrote in the Workstation's Project
// Publishing tab, and whether Enable Publishing is on. The program running
// your cartridge puts the record in place before _init runs, in the
// workstation, the native player and the web player alike, so a title
// screen, an ABOUT panel or a save file can show or stamp the version with
// one call. Nothing is read from a file, and there is nothing to parse.
//
// The record is cheap enough to ask for every frame. In the workstation it is
// also live: press Save in Project Publishing and the running cartridge's next
// call returns the new record, with no rebuild.
//
// Complete title-screen example (copy into main.c):
//
//   #include "jet64.h"
//
//   void _init(void) {}
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       const JetPublishRecord* about = jet_publish_record();
//       jet_cls(0);
//       jet_print(about->app_name, 16, 16, 15);
//       jet_print(about->version, 16, 32, 15);
//       jet_print(about->copyright, 16, 48, 15);
//   }
//
//   void _end(void) {}

/**
 * Struct: JetPublishRecord
 * Summary: Your cartridge's publishing record as you can read it: the text of each field, the version's three numbers, and whether there was a record to read.
 * Fields:
 *   - present: true when the program running your cartridge published a record. It is false only when data/publish.ron exists but could not be read, or the program is too old to publish one; every other field is then empty or 0.
 *   - published: The Enable Publishing switch in Project Publishing.
 *   - major: The first of the version's three numbers: 1 in 1.0.3. Compare these numbers, not the version text, to tell which of two versions is newer.
 *   - minor: The second number: 0 in 1.0.3. A number the version does not spell is 0.
 *   - patch: The third number: 3 in 1.0.3.
 *   - app_name: The App Name field.
 *   - publisher: The Publisher field.
 *   - version: The Version field, exactly as you wrote it, such as "1.0.3".
 *   - copyright: The Copyright field.
 *   - description: The Description field, with the line breaks you typed kept as '\n'.
 * Include: jet_publish.h
 * Notes: Every text field is a NUL-terminated UTF-8 string, so a copyright line can hold a character such as © that a font may have no picture for. A field longer than its space is shortened by whole characters and still ends in NUL, so it is always safe to print.
 * Example:
 *   const JetPublishRecord* about = jet_publish_record();
 *   if (about->major >= 2) { unlock_new_game_plus(); }
 */
typedef struct {
    bool     present;
    bool     published;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    char     app_name[JET_PUBLISH_APP_NAME_BYTES];
    char     publisher[JET_PUBLISH_PUBLISHER_BYTES];
    char     version[JET_PUBLISH_VERSION_BYTES];
    char     copyright[JET_PUBLISH_COPYRIGHT_BYTES];
    char     description[JET_PUBLISH_DESCRIPTION_BYTES];
} JetPublishRecord;

/**
 * Function: jet_publish_record
 * Summary: Gives you your cartridge's publishing record: its name, publisher, version, copyright line and description, as written in Project Publishing.
 * Parameters:
 *   - None.
 * Returns: The record, never NULL. When there is no record, present is false and every string is empty. The record belongs to the runtime: read it, but never change it. It stays exactly as it is until your next call, so keep the pointer for the rest of the frame if you like, and copy a field if you need it for longer.
 * Include: jet_publish.h
 * Notes: After the first call, each call costs one read of shared memory, and the record is copied again only when a new one has been published: never in an exported player, and in the workstation only when you save in Project Publishing. Call it from your hooks, not from a jet_thread worker.
 * Example:
 *   const JetPublishRecord* about = jet_publish_record();
 *   jet_print(about->version, 16, 16, 15);
 */
const JetPublishRecord* jet_publish_record(void);

#endif // JET_PUBLISH_H
