#ifndef MUSIC_IO_MUSICXML_IMPORTER_H
#define MUSIC_IO_MUSICXML_IMPORTER_H

#include <stddef.h>

#include <stdint.h>

#include "events/dynamics.h"
#include "events/note_event.h"
#include "events/tempo_marking.h"
#include "pitch/key_signature.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/tempo_map.h"

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
 * Meter. Each <time> gives a TimeSignature at its position, and the MeterMap is built from them (declared, not
 * defaulted); with no <time> anywhere it is the defaulted 4/4 map, as for a MIDI file with no time signature event.
 * A first measure marked implicit="yes" and shorter than a full measure is the PICKUP, measure 0 (the MeterMap
 * detects a pickup only from a short first region, so the importer ends that region at the pickup's length). The
 * measure numbers, and where every barline falls, come from the MeterMap and the time signatures alone: a measure
 * of the document whose length differs from its signature's (other than an implicit one) cannot be represented
 * and is a warning. Additive meters (3+2) are summed, with a warning; senza-misura and beat types that are not
 * a power of two are skipped.
 *
 * Keys. Each <key> gives a key-signature event {KeySignature, mode hint, position}, the same entry shape as
 * midi_key_signature_scan_all: <fifths> is the sharps/flats count (-7..7), <mode> major or minor gives the
 * hint, and any other mode (dorian ...) or none is UNKNOWN, as it makes no claim the signature does not. Events
 * that repeat exactly (same position, fifths and mode, in other staves or parts) are kept once, on the lowest
 * part.
 *
 * Parts, staves, voices. Each <part> is a part (parts[i], in document order, with its id from <score-part id>, its
 * <part-name> and the number of staves its <staves> declares, 1 by default). Each note keeps its part index and its
 * <staff> number (1 when it names none). A notated <voice> within a part is a Voice: the NoteEvent carries a
 * VoiceReference whose target id is "musicxml-voice-<part id>-<voice>" (none when the note names no voice). A voice
 * is notated, not found: nothing here separates voices, and the MIDIImporter's per-track voices are untouched.
 *
 * Transposing parts. A <transpose> (<chromatic> plus 12 * <octave-change>) shifts the SOUNDING pitch, the
 * PitchNumericValue and the PitchIdentity the NoteEvent carries; the PitchSpelling stays the WRITTEN pitch. Each
 * distinct transposition of a part is a warning, since downstream analysis reads sounding pitch.
 *
 * Tempo. <sound tempo="..."> (quarter notes per minute) gives a TempoMap region at its position: microseconds per
 * quarter note = 60,000,000 / bpm, rounded to the nearest whole microsecond (with a warning when it was not
 * exact, as a TempoMap holds whole microseconds). Several parts that give the same position the same tempo are one
 * region; a conflict keeps the first part's and warns. With no tempo anywhere the TempoMap is the defaulted 120
 * quarter notes per minute. A <metronome> (beat unit, dots, per-minute) gives a MetronomeMarking and a TempoMarking
 * at its position, and so does a <sound tempo> that has <words> with it. <words> are tempo text ONLY when their
 * <direction> also holds a <metronome> or a <sound tempo>; other direction text is not classified (io carries no
 * tempo vocabulary) and is counted in one warning. A <metronome> with no <words> is a TempoMarking with only the
 * metronome marking. A <direction>'s <offset> moves a marking's position, never a <sound tempo>'s.
 *
 * Dynamics. Each <dynamics> child (p, mf, sfz, ...; other-dynamics gives its text) is a Dynamics (events/
 * dynamics.h) whose marking is that name. A Dynamics targets a TimeSpan, so each marking holds from its position to
 * the next LATER marking in the same part, or to the part's end ("in force until the next"); markings at one
 * position share a span. One with no extent (at or after the part's end) is skipped with a warning. Wedges
 * (crescendo and diminuendo) are not read.
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

typedef enum {
    MUSICXML_MODE_HINT_MAJOR = 0,
    MUSICXML_MODE_HINT_MINOR,
    MUSICXML_MODE_HINT_UNKNOWN
} MusicXmlModeHint;

typedef struct {
    KeySignature *key_signature;  /* owned: built as midi_key_signature_build builds it */
    MusicXmlModeHint mode_hint;   /* next to the signature, never part of it */
    MusicalTime *position;        /* owned; whole notes */
    int8_t sharps_flats;          /* the raw <fifths>, -7..7 */
    size_t part_index;            /* the lowest part that declared it */
} MusicXmlKeySignatureEvent;

typedef struct {
    char *id;                     /* owned: the <score-part id> (or "part-<n>" when the <part> names none) */
    char *name;                   /* owned: the <part-name>, or "" */
    int staff_count;              /* <staves>, 1 when none */
} MusicXmlPart;

typedef struct {
    NoteEvent **events;                  /* owned; count entries, in time order */
    PitchNumericValue **pitch_values;    /* owned; parallel to events: the sounding pitch */
    size_t count;
    size_t *part_index;                  /* owned; parallel to events: the part (0-based, document order) */
    int *staff;                          /* owned; parallel to events: the <staff> number, 1 when none */
    MusicXmlPart *parts;                 /* owned; in document order */
    size_t part_count;
    MeterMap *meter_map;                 /* owned */
    TempoMap *tempo_map;                 /* owned */
    TempoMarking **tempo_markings;       /* owned; in time order */
    size_t *tempo_marking_part;          /* owned; parallel to tempo_markings */
    size_t tempo_marking_count;
    Dynamics **dynamics;                 /* owned; in time order */
    size_t *dynamics_part;               /* owned; parallel to dynamics */
    int *dynamics_staff;                 /* owned; parallel to dynamics: the <staff> of its <direction>, 1 when none */
    size_t dynamics_count;
    MusicXmlKeySignatureEvent *key_signatures; /* owned; in time order; NULL when there are none */
    size_t key_signature_count;
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
