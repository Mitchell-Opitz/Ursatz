#ifndef URSATZ_GUI_IMPORT_CACHE_H
#define URSATZ_GUI_IMPORT_CACHE_H

#include <stdbool.h>

#include <ursatz_analyzer/input_pipeline.h>

/*
 * The imported notes of a piece, kept so that opening a piece imports its source at most once. An entry belongs to
 * one piece id and one source version (the version text the caller builds: the notation fields of meta.json plus the
 * source file's size and modification time), so a changed source or notation is never served from an old entry. Up to
 * 8 entries; the least recently used one that nobody holds is dropped to make room.
 *
 * The caller holds the piece lock (piece_lock.h) from acquire to release, so one piece is never filled or changed by
 * two threads; the table itself has its own mutex. An entry also carries the piece's exported MusicXML (with the
 * title and composer added) and whether the meter was defaulted, set by the first score request. PATCH, re-analyze
 * and delete call ursatz_gui_import_cache_invalidate under the piece lock.
 */

typedef struct UrsatzGuiImportEntry UrsatzGuiImportEntry;

/* Imports the source into *out; false when it cannot. */
typedef bool (*UrsatzGuiImportFn)(void *context, ImportedNotes *out);

/*
 * The entry of piece_id for version, imported with import(context, ...) when there is none (or the version
 * differs). NULL when the import failed (*out_import_failed true) or memory ran out (false). Release it with
 * ursatz_gui_import_cache_release.
 */
UrsatzGuiImportEntry *ursatz_gui_import_cache_acquire(const char *piece_id, const char *version,
                                                       UrsatzGuiImportFn import, void *context,
                                                       bool *out_import_failed);

void ursatz_gui_import_cache_release(UrsatzGuiImportEntry *entry);

const ImportedNotes *ursatz_gui_import_entry_notes(const UrsatzGuiImportEntry *entry);

/* The cached score: the MusicXML text (owned by the entry) or NULL when none was stored yet. */
const char *ursatz_gui_import_entry_score(const UrsatzGuiImportEntry *entry, bool *out_meter_defaulted);

/* Stores the score; the entry takes ownership of xml (malloc'd). */
void ursatz_gui_import_entry_set_score(UrsatzGuiImportEntry *entry, char *xml, bool meter_defaulted);

/* Drops the entry of piece_id, if any (freed once nobody holds it). */
void ursatz_gui_import_cache_invalidate(const char *piece_id);

/* Imports done through the cache since start (or the last reset), and the entries held; for tests. */
unsigned ursatz_gui_import_cache_imports(void);
unsigned ursatz_gui_import_cache_entries(void);
void ursatz_gui_import_cache_reset(void);

#endif
