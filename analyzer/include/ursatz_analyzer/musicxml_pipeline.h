#ifndef URSATZ_ANALYZER_MUSICXML_PIPELINE_H
#define URSATZ_ANALYZER_MUSICXML_PIPELINE_H

#include <ursatz_analyzer/imported_notes.h>

typedef enum {
    MUSICXML_PIPELINE_OK = 0,
    MUSICXML_PIPELINE_ERR_CANNOT_READ_FILE,
    MUSICXML_PIPELINE_ERR_COMPRESSED,
    MUSICXML_PIPELINE_ERR_EMPTY_SCORE,
    MUSICXML_PIPELINE_ERR_IMPORT_FAILED
} MusicXmlPipelineResult;

/*
 * Reads the file at path and imports it via Ursatz's MusicXMLImporter
 * into the same ImportedNotes the MIDI pipeline produces. The notes
 * of every part are merged into one texture; parts, staves and voices
 * stay on the notes (see ImportedSource). The time signatures and key
 * signatures are the score's own (meter_notated, key_notated), and the
 * pitch spellings are the score's (spelling_notated).
 *
 * Importer warnings (a skipped grace note, a rounded tempo, ...) are
 * written to stderr and do not fail the import. On MUSICXML_PIPELINE_OK,
 * *out is owned by the caller (imported_notes_destroy). On any other
 * result, a human-readable message has already been written to stderr
 * and *out is left untouched.
 *
 * A compressed .mxl is recognized and reported as unsupported: the
 * Ursatz importer reads uncompressed score-partwise documents only.
 */
MusicXmlPipelineResult musicxml_pipeline_import_file(const char *path, ImportedNotes *out);

#endif
