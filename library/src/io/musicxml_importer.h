#ifndef MUSIC_IO_MUSICXML_IMPORTER_H
#define MUSIC_IO_MUSICXML_IMPORTER_H

#include <stddef.h>

#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"

/*
 * MusicXMLImporter (L9, music-io): reads an uncompressed score-partwise MusicXML document into the same
 * canonical outputs as MIDIImporter (io/midi_importer.h), so every analysis downstream runs unchanged. An
 * external format is an adapter and defines nothing of the canonical domain. See docs/modules/io.md for the
 * supported subset and every mapping rule. Compressed .mxl and score-timewise are not read.
 *
 * Time. Durations in <divisions> convert EXACTLY to whole notes (MusicalTime / Duration): a duration of d
 * divisions, with D divisions per quarter note, is d / (4 * D) whole notes. <divisions> may change; <backup>,
 * <forward> and <chord/> are handled exactly. Rests and cue notes advance time and make no NoteEvent. A grace note
 * is skipped (and warned about); so is an unpitched note.
 *
 * Ties. A tie start and stop merge into ONE NoteEvent, across barlines and in chains, when the same voice has the
 * same sounding pitch and the next note starts exactly where the last ended. A stop with no start, or a start
 * that never stops, is a warning.
 *
 * Pitch. Each note has the written PitchSpelling (step, alter, octave) and, under the MIDI note number tuning
 * (the tuning the MIDIImporter uses), a PitchNumericValue of the SOUNDING pitch, whose PitchIdentity the NoteEvent
 * carries. An <alter> that is not a whole number is rounded (and warned about).
 *
 * Order. Notes come out in time order (stable by onset, then part, then document order), as the MIDIImporter
 * returns them; they are NOT unfolded for repeats and endings. Note ids are "musicxml-<part id>-<n>", n counting
 * from 0 in document order within the part.
 *
 * Warnings. Anything skipped, rounded or inconsistent is counted in warnings.count, and the first
 * MUSICXML_IMPORT_MAX_WARNING_MESSAGES messages are kept. A warning never stops the import; a structural error
 * does: malformed XML, a root that is not <score-partwise>, a duration-bearing element before any <divisions>, a
 * value that cannot be read, an input over MUSICXML_IMPORT_MAX_BYTES. On an error *out holds only the status's
 * message in error_message, no notes, and nothing to destroy.
 *
 * The XML parser is vendored under third_party/ezxml and is used by musicxml_importer.c only: no type of it
 * appears here.
 */

#define MUSICXML_IMPORT_MAX_BYTES (256u * 1024u * 1024u)
#define MUSICXML_IMPORT_MAX_WARNING_MESSAGES 16
#define MUSICXML_IMPORT_WARNING_MESSAGE_SIZE 192
#define MUSICXML_IMPORT_ERROR_MESSAGE_SIZE 256

typedef enum {
    MUSICXML_IMPORT_OK = 0,
    MUSICXML_IMPORT_ERR_NULL_ARGUMENT,
    MUSICXML_IMPORT_ERR_TOO_LARGE,
    MUSICXML_IMPORT_ERR_IO,
    MUSICXML_IMPORT_ERR_MALFORMED_XML,
    MUSICXML_IMPORT_ERR_WRONG_ROOT,
    MUSICXML_IMPORT_ERR_MISSING_DIVISIONS,
    MUSICXML_IMPORT_ERR_BAD_VALUE,
    MUSICXML_IMPORT_ERR_OVERFLOW,
    MUSICXML_IMPORT_ERR_ALLOC
} MusicXmlImportStatus;

typedef struct {
    size_t count;                                   /* every warning, including those without a message */
    size_t message_count;                           /* at most MUSICXML_IMPORT_MAX_WARNING_MESSAGES */
    char messages[MUSICXML_IMPORT_MAX_WARNING_MESSAGES][MUSICXML_IMPORT_WARNING_MESSAGE_SIZE];
} MusicXmlImportWarnings;

typedef struct {
    NoteEvent **events;                  /* owned; count entries, in time order */
    PitchNumericValue **pitch_values;    /* owned; parallel to events: the sounding pitch */
    size_t count;
    size_t *part_index;                  /* owned; parallel to events: the part (0-based, document order) */
    MusicXmlImportWarnings warnings;
    char error_message[MUSICXML_IMPORT_ERROR_MESSAGE_SIZE]; /* set on every error, empty on OK */
} MusicXmlImport;

/* data[0..size) is a MusicXML document (UTF-8 or UTF-16, with or without a byte order mark). It is not
   modified. On OK, *out owns its arrays (musicxml_import_destroy); otherwise it holds only error_message. */
MusicXmlImportStatus musicxml_import_buffer(const char *data, size_t size, MusicXmlImport *out);

/* As musicxml_import_buffer, reading the file at path (ERR_IO when it cannot be read). */
MusicXmlImportStatus musicxml_import_file(const char *path, MusicXmlImport *out);

/* Destroys every NoteEvent and PitchNumericValue and the arrays. NULL is a no-op; the struct may be reused. */
void musicxml_import_destroy(MusicXmlImport *import);

#endif
