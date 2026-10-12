// runtime/jet_io.h
#ifndef JET_IO_H
#define JET_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Files: saving your game's own data, and reading the assets bundled with the
// cartridge. Every cartridge gets a private folder of its own, its AppData
// folder, and that folder is all it can see. jet_fopen reads and writes there,
// and it cannot reach any other path on the computer; that is what the
// sandbox means. Bundled assets are the files under your project's data/
// folder. jet_asset_open and jet_asset_read_to_wram read them, and they read
// the same bytes in the Workstation, in a native export and in a web export.
// Both name a file the same way, from inside data/: "misc/level0.bin" is the
// file data/misc/level0.bin, because the runtime adds the data/ itself.
//
// Each open that succeeds gives you a JET_FILE handle, and the runtime can only
// have so many open at once, so close every one with jet_fclose when you are
// done with it. Check everything a call gives back: a failure comes back as
// NULL, false, zero records or zero entries. Nothing stops your program for
// you, so a result you do not check is a failure you never see.
//
// Complete save-file example (copy into main.c):
//
//   #include "jet64.h"
//
//   static uint32_t high_score;
//
//   static void save_high_score(void) {
//       JET_FILE file = jet_fopen("highscore.bin", "wb");
//       if (file != NULL) {
//           (void)jet_fwrite(&high_score, sizeof high_score, 1, file);
//           jet_fclose(file);
//       }
//   }
//
//   void _init(void) {
//       JET_FILE file = jet_fopen("highscore.bin", "rb");
//       if (file != NULL) {
//           if (jet_fread(&high_score, sizeof high_score, 1, file) != 1) high_score = 0;
//           jet_fclose(file);
//       }
//   }
//
//   void _update(float dt) {
//       (void)dt;
//       if (btnp(BTN_A)) { high_score += 100; save_high_score(); }
//   }
//
//   void _draw(void) {
//       char line[32];
//       snprintf(line, sizeof line, "HIGH SCORE %u", high_score);
//       jet_cls(0);
//       jet_print(line, 16, 16, 15);
//   }
//
//   void _end(void) {}

/**
 * Type: JET_FILE
 * Summary: A handle for one open file: either a file in the cartridge's own AppData folder, or a read-only asset bundled with the cartridge.
 * Include: jet_io.h
 * Notes: Get one from jet_fopen or jet_asset_open. NULL means the open failed, and NULL is safe to hand to jet_fclose. Any other value is yours until you call jet_fclose on it, exactly once. After that the value means nothing and must not be used again. Keep the handle, pass it to the read, write, size and close calls, and do not look inside it: the host's own file object is kept out of the cartridge's reach on purpose.
 * Example:
 *   JET_FILE file = jet_fopen("save.bin", "rb");
 *   if (file != NULL) { jet_fclose(file); }
 */
typedef void* JET_FILE;

/**
 * Function: jet_fopen
 * Summary: Opens a file in the cartridge's own AppData folder, for reading or writing, with the same mode strings as C's fopen.
 * Parameters:
 *   - filename: The file's path, relative to the AppData folder. A path that starts at the root of the disk, or that climbs out of the folder with "..", is refused.
 *   - mode: A mode string in fopen's style, such as "rb" to read, "wb" to write a fresh file, "ab" to add to the end, or "r+b" to read and write.
 * Returns: A JET_FILE handle, or NULL if the open failed or the path was refused.
 * Include: jet_io.h
 * Notes: This is for data that changes, such as saves and settings, kept in the AppData folder. For the read-only assets bundled with the cartridge, use jet_asset_open. The call never creates a folder that is missing from the path, so call jet_mkdir first. The handle is yours until you call jet_fclose on it, and must not be used after that.
 * Example:
 *   JET_FILE f = jet_fopen("saves/slot0.bin", "wb");
 *   if (f) {
 *       jet_fwrite(&state, sizeof(state), 1, f);
 *       jet_fclose(f);
 *   }
 */
JET_FILE jet_fopen(const char* filename, const char* mode);

/**
 * Function: jet_fclose
 * Summary: Closes a file you opened with jet_fopen or jet_asset_open.
 * Parameters:
 *   - file: The handle to close. NULL is ignored, so it is safe to pass a failed open.
 * Returns: Nothing.
 * Include: jet_io.h
 * Notes: Always close a file when you are finished with it, so the operating system can write out anything it was still holding and free the file's slot. After this call the handle means nothing: do not read it, write it, measure it, or close it a second time.
 * Example:
 *   JET_FILE f = jet_fopen("settings.bin", "rb");
 *   if (f) jet_fclose(f);
 */
void jet_fclose(JET_FILE file);

/**
 * Function: jet_fread
 * Summary: Reads records of one fixed size from an open file into memory.
 * Parameters:
 *   - ptr: Where to put the bytes: memory of yours with room for size * count bytes.
 *   - size: The size of one record, in bytes.
 *   - count: The most records to read.
 *   - file: The handle of a file opened for reading.
 * Returns: How many whole records were read, counted the way C's fread counts them. 0 if file is NULL or nothing was read.
 * Include: jet_io.h
 * Notes: Make sure ptr really has size * count bytes, and if size or count come from data you do not trust, check that multiplying them does not overflow. Fewer records than you asked for means the end of the file, or a read error; there is no separate way to tell which. To load a bundled asset straight into WRAM, the cartridge's working memory, jet_asset_read_to_wram is the better call.
 * Example:
 *   SaveState state;
 *   if (jet_fread(&state, sizeof(state), 1, f) == 1) {
 *       apply_state(&state);
 *   }
 */
size_t jet_fread(void* ptr, size_t size, size_t count, JET_FILE file);

/**
 * Function: jet_fwrite
 * Summary: Writes records of one fixed size from memory into an open file in the cartridge's own AppData folder.
 * Parameters:
 *   - ptr: The bytes to write.
 *   - size: The size of one record, in bytes.
 *   - count: How many records to write.
 *   - file: The handle of a file opened for writing or appending.
 * Returns: How many whole records were written, counted the way C's fwrite counts them. 0 if file is NULL or nothing was written.
 * Include: jet_io.h
 * Notes: To write into a folder inside the AppData folder, call jet_mkdir first. Fewer records than you asked for means the write did not finish; treat it as a failed save. Writes only ever land in the cartridge's own AppData folder, never anywhere else on the computer.
 * Example:
 *   jet_mkdir("saves");
 *   JET_FILE f = jet_fopen("saves/slot0.bin", "wb");
 *   if (f) {
 *       jet_fwrite(&state, sizeof(state), 1, f);
 *       jet_fclose(f);
 *   }
 */
size_t jet_fwrite(const void* ptr, size_t size, size_t count, JET_FILE file);

/**
 * Function: jet_mkdir
 * Summary: Makes a folder inside the cartridge's own AppData folder, and every folder on the way to it.
 * Parameters:
 *   - path: The folder to make, relative to the AppData folder. A path that starts at the root of the disk, or that climbs out of the folder with "..", is refused.
 * Returns: true when the last folder in the path was made; otherwise false.
 * Include: jet_io.h
 * Notes: Folders on the way that do not exist yet are made as needed. If the last folder already exists, the call may answer false on some platforms, because the operating system's own mkdir made nothing new.
 * Example:
 *   jet_mkdir("saves/profile1");
 */
bool jet_mkdir(const char* path);

/**
 * Function: jet_remove
 * Summary: Deletes a file, or an empty folder, from the cartridge's own AppData folder.
 * Parameters:
 *   - path: What to delete, relative to the AppData folder.
 * Returns: true when the host deleted it; otherwise false.
 * Include: jet_io.h
 * Notes: It behaves like the Linux commands rm for a file and rmdir for an empty folder.
 * Example:
 *   jet_remove("saves/temp.bin");
 */
bool jet_remove(const char* path);

/**
 * Function: jet_format
 * Summary: Deletes everything the cartridge has stored in its own AppData folder.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_io.h
 * Notes: There is no undo, so keep it for a reset the player has asked for and confirmed. It removes every save, settings and log file that belongs to this cartridge, and never touches the assets bundled with it. Never call it on its own at startup.
 * Example:
 *   if (confirm_reset) {
 *       jet_format();
 *   }
 */
void jet_format(void);

/**
 * Function: jet_rename
 * Summary: Renames a file or folder in the cartridge's own AppData folder, or moves it to another place inside that folder.
 * Parameters:
 *   - old_path: The current path, relative to the AppData folder. A path that starts at the root of the disk, or that climbs out of the folder with "..", is refused.
 *   - new_path: The new path, relative to the AppData folder, refused on the same terms.
 * Returns: true when the host renamed it; otherwise false.
 * Include: jet_io.h
 * Notes: A file already at new_path is replaced, on every machine, in one step: write the new version beside the old one, then rename it over the old one, and a save stopped halfway leaves one version or the other whole, never a torn file. Do not rename onto a folder that already exists; machines differ on that. The folders on the way to new_path must already exist. Nothing can be moved out of the AppData folder.
 * Example:
 *   jet_rename("saves/temp.bin", "saves/slot0.bin");
 */
bool jet_rename(const char* old_path, const char* new_path);

/**
 * Function: jet_ls
 * Summary: Lists the names in a folder inside the cartridge's own AppData folder.
 * Parameters:
 *   - path: The folder to look in, relative to the AppData folder. Give "" or "." for the AppData folder itself.
 *   - out_entries: An array of yours, one 256-byte text buffer for each name, that the names are written into.
 *   - max_entries: The most names to write into out_entries; give it the number of buffers in the array.
 * Returns: How many names were written into out_entries. 0 if the input was bad, the path was refused, the folder was empty, or the host failed.
 * Include: jet_io.h
 * Notes: Each entry is a name on its own, not a full path. The order is whatever the operating system gives, and it can change from one call to the next, so do not count on it.
 * Example:
 *   char names[32][256];
 *   int count = jet_ls("saves", names, 32);
 */
int jet_ls(const char* path, char out_entries[][256], int max_entries);

/**
 * Function: jet_asset_open
 * Summary: Opens one of the read-only assets bundled with the cartridge, from its data folder.
 * Parameters:
 *   - filename: The asset's path, relative to the cartridge's data folder, with data/ left off: "misc/intro.txt" opens data/misc/intro.txt. A path that starts at the root of the disk, or that climbs out of the folder with "..", is refused. So is "font.rom": it is the machine's Font ROM, which the runtime has already mapped at ROM before _init, so read its bytes there.
 * Returns: A JET_FILE handle open for reading bytes, or NULL if the open failed or the path was refused.
 * Include: jet_io.h
 * Notes: This is for the files packed with the cartridge. For saved games in the AppData folder, use jet_fopen. It reads the same bytes on every runtime: from the project folder in the Workstation, and from the image of assets built into the player in a native or web export. The runtime can only have so many handles open at once, on every runtime, so call jet_fclose on each one as soon as the read is done.
 * Example:
 *   JET_FILE f = jet_asset_open("misc/intro.txt");
 *   if (f) jet_fclose(f);
 */
JET_FILE jet_asset_open(const char* filename);

/**
 * Function: jet_file_size
 * Summary: Tells you how many bytes an open file holds, without moving your place in it.
 * Parameters:
 *   - file: The handle of the file to measure.
 * Returns: The file's size in bytes, or 0 if file is NULL or the size cannot be found out.
 * Include: jet_io.h
 * Notes: To measure the file the runtime jumps to its end and back, then puts your read position back where it was. A file that really is empty also answers 0, so 0 does not always mean failure.
 * Example:
 *   size_t bytes = jet_file_size(f);
 */
size_t jet_file_size(JET_FILE file);

/**
 * Function: jet_asset_read_to_wram
 * Summary: Loads a whole bundled asset into WRAM, the cartridge's working memory, in one go. The host streams the bytes straight in.
 * Parameters:
 *   - rel_path: The asset's path, relative to the cartridge's data folder, with data/ left off: "misc/level0.bin" loads data/misc/level0.bin. It is exactly the path jet_asset_open takes for the same file, and it is refused the same way.
 *   - out_size: Where to write how many bytes were loaded, or NULL if you do not need to know.
 * Returns: A pointer to the freshly reserved WRAM holding the asset, or NULL on failure.
 * Include: jet_io.h
 * Notes: This is the quick way to load a big binary asset, because the host writes the bytes straight into the virtual machine's memory instead of handing them to you a record at a time. The memory comes from WRAM's bump allocator, which only ever hands out more and never takes any back, so what you load stays put for the rest of the run and cannot be freed on its own. On failure you get NULL, and 0 is written to out_size if you gave one. Load the assets you keep during _init, so a call made every frame does not eat a fresh slice of WRAM each time.
 * Example:
 *   uint32_t size = 0;
 *   void* level_data = jet_asset_read_to_wram("misc/level0.bin", &size);
 */
void* jet_asset_read_to_wram(const char* rel_path, uint32_t* out_size);

#endif // JET_IO_H
