#ifndef MUSIC_EXPORT_MUSICXML_EXPORT_H
#define MUSIC_EXPORT_MUSICXML_EXPORT_H

#include <stddef.h>

#include "events/dynamics.h"
#include "events/note_event.h"
#include "events/tempo_marking.h"
#include "persistence/persistence.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/tempo_map.h"

/*
 * MusicXmlExport (music-export): serializes a piece's persisted
 * NoteEvent sequence -- pitch (via PitchSpelling) and duration -- into
 * a single-part MusicXML 3.1 document. NoteEvents sharing the same
 * optional_voice (a VoiceReference is an opaque EntityID token here --
 * see core/voice_reference.h -- carrying no musical/analytical meaning
 * at this layer, only an export-grouping label) are emitted into the
 * same MusicXML <voice>; NoteEvents with no voice reference all share
 * one implicit voice. Distinct voices within a measure are interleaved
 * via <backup> elements so simultaneous notes across voices can be
 * represented in one <part>.
 *
 * This module takes an already-open PersistenceHandle rather than a
 * "piece id": Persistence itself has no piece-id/lookup concept (one
 * handle is one persisted piece, see persistence/persistence.h), and
 * Corpus's piece_ref (corpus/persistence_corpus_membership.h) is
 * explicitly never resolved to piece data anywhere in this codebase.
 * Resolving an external "piece id" to an open PersistenceHandle is
 * therefore a caller concern (e.g. ursatz-gui's backend), not this
 * module's.
 *
 * SYNTHETIC METER (read before consuming this module's output):
 * Meter/MeterMap/Measure are not populated anywhere in this codebase
 * (containers/measure.h's meter_map_region_ref is a bare, unresolved
 * EntityID -- MeterMap does not exist yet). This module therefore lays
 * barlines on a fixed, synthetic 4/4 grid purely for display -- it is
 * NOT read from or written to any Measure/MeterMap data, and does not
 * represent an authored time signature. The output is explicitly
 * flagged as such via a <miscellaneous-field name="ursatz:synthetic-
 * meter"> entry under <identification><miscellaneous>, so downstream
 * consumers (e.g. feat/notation-rendering) can detect and surface the
 * distinction rather than presenting it as real. See
 * docs/modules/export.md.
 */

typedef enum {
    MUSICXML_EXPORT_OK = 0,
    MUSICXML_EXPORT_ERR_NULL_ARG,
    MUSICXML_EXPORT_ERR_LOAD_FAILED,
    MUSICXML_EXPORT_ERR_MISSING_SPELLING,
    MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES,
    MUSICXML_EXPORT_ERR_ALLOC,
    /* musicxml_export_write_score only (appended; the values above are unchanged) */
    MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT, /* a count with no array, a NULL entry, or parts that do not add up */
    MUSICXML_EXPORT_ERR_UNKNOWN_PART,       /* an event, marking, dynamic or key names a part that does not exist */
    MUSICXML_EXPORT_ERR_BAD_STAFF,          /* a staff below 1 or above its part's staff_count */
    MUSICXML_EXPORT_ERR_UNKNOWN_EVENT,      /* a Dynamics targets an event id that is not among the events */
    MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE,  /* a negative position, a zero duration, an unwritable value, an overflow */
    MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW  /* no <divisions> up to INT32_MAX makes every time an integer */
} MusicXmlExportResult;

/* One <part>. id and name are borrowed (the document escapes them); staff_count is at least 1. */
typedef struct {
    const char *id;
    const char *name;
    int staff_count;
} MusicXmlExportPart;

typedef enum {
    MUSICXML_EXPORT_MODE_MAJOR = 0,
    MUSICXML_EXPORT_MODE_MINOR,
    MUSICXML_EXPORT_MODE_UNKNOWN /* no <mode> is written */
} MusicXmlExportMode;

/* A key signature at a position, written in its own part. sharps_flats is the <fifths> value, -7 to 7. */
typedef struct {
    int sharps_flats;
    MusicXmlExportMode mode;
    const MusicalTime *position; /* borrowed; whole notes */
    size_t part_index;
} MusicXmlExportKeySignature;

/*
 * Everything musicxml_export_write_score writes. All pointers are borrowed and never mutated, and every optional
 * member may be NULL (with its count 0 where it has one). The shape follows what musicxml_import_buffer returns, so
 * an import can be written back; MusicXmlExport does not include io/, so the caller copies the members across.
 *
 *   - events / pitch_values / count: index-paired, as the importers return them. Pitch is written from each event's
 *     PitchSpelling; pitch_values is checked to be present and paired, and is not otherwise read.
 *   - part_index / staff: optional, parallel to events (a NULL array means part 0, or staff 1).
 *   - parts / part_count: the parts; none means one part "P1" whose staff_count is the highest staff used.
 *   - meter_map: the measure layout. NULL lays measures from a defaulted 4/4 map and flags the output meter-defaulted.
 *   - tempo_map: written as <sound tempo> at each region that is not defaulted, in part 0.
 *   - tempo_markings (+ tempo_marking_part): written as <direction> text and <metronome>; part arrays default to 0.
 *   - dynamics (+ dynamics_part, dynamics_staff): written at the start of the Dynamics' span, or at its event.
 *   - key_signatures: written in their own part.
 */
typedef struct {
    NoteEvent *const *events;
    PitchNumericValue *const *pitch_values;
    size_t count;
    const size_t *part_index;
    const int *staff;
    const MusicXmlExportPart *parts;
    size_t part_count;
    const MeterMap *meter_map;
    const TempoMap *tempo_map;
    TempoMarking *const *tempo_markings;
    const size_t *tempo_marking_part;
    size_t tempo_marking_count;
    Dynamics *const *dynamics;
    const size_t *dynamics_part;
    const int *dynamics_staff;
    size_t dynamics_count;
    const MusicXmlExportKeySignature *key_signatures;
    size_t key_signature_count;
} MusicXmlExportScore;

/*
 * Loads every NoteEvent persisted in handle (persistence_load_note_
 * events, original stored sequence order -- callers are responsible for
 * having stored them in time order, the same assumption every other
 * consumer of persistence_load_note_events makes) and writes a newly
 * allocated, NUL-terminated MusicXML document string to *out_xml.
 * Caller owns *out_xml (free() it).
 *
 * Fails with MUSICXML_EXPORT_ERR_MISSING_SPELLING on the first
 * NoteEvent whose optional_pitch_spelling is NULL -- there is no
 * fallback pitch source (PitchIdentity is an opaque token, not
 * convertible to letter/accidental/octave), so an unspelled note cannot
 * be serialized; this fails loud rather than silently skipping the note
 * or omitting its pitch.
 *
 * Fails with MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES if any NoteEvent
 * starts before the previous NoteEvent in the same voice ends --
 * overlap is checked per voice group, not across the whole piece, so
 * simultaneous NoteEvents in different voices are not an error; two
 * NoteEvents sharing a voice (or both lacking one) that overlap in time
 * still cannot be represented and fail.
 *
 * A piece with zero NoteEvents is not an error: *out_xml is a valid
 * MusicXML document with one empty part.
 */
MusicXmlExportResult musicxml_export_write(PersistenceHandle *handle, char **out_xml);

/*
 * Writes score as a newly allocated, NUL-terminated MusicXML 3.1 document to *out_xml (the caller free()s it).
 * Measures, their numbers (a pickup is 0), and every barline come from score->meter_map and nowhere else; a note
 * that crosses a barline is split and tied; <divisions> is the smallest whole number that makes every position and
 * duration an integer, and nothing is rounded. On any result other than OK *out_xml is not written.
 */
MusicXmlExportResult musicxml_export_write_score(const MusicXmlExportScore *score, char **out_xml);

#endif
