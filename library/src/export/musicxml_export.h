#ifndef MUSIC_EXPORT_MUSICXML_EXPORT_H
#define MUSIC_EXPORT_MUSICXML_EXPORT_H

#include <stddef.h>

#include "events/dynamics.h"
#include "events/note_event.h"
#include "events/tempo_marking.h"
#include "export/score_quantize.h"
#include "persistence/persistence.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/tempo_map.h"

/*
 * MusicXmlExport (music-export): serializes a piece's persisted
 * NoteEvent sequence -- pitch (via PitchSpelling) and duration -- into
 * a single-part MusicXML 3.1 document (musicxml_export_write), or an in-memory score into a multi-part one
 * (musicxml_export_write_score). NoteEvents sharing the same
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
 * METER. Barlines and measure numbers come from a MeterMap (time/meter_map.h) and nowhere else. musicxml_export_write
 * has no MeterMap to read -- a PersistenceHandle holds NoteEvents only -- so it lays measures from
 * meter_map_create_defaulted (one 4/4 region marked defaulted) and flags the output with
 * <miscellaneous-field name="ursatz:meter-defaulted"> under <identification><miscellaneous>, so a reader can tell a
 * default from an authored meter. A caller with a real MeterMap (and parts, staves, key signatures, tempo and
 * dynamics) uses musicxml_export_write_score below, whose output carries the flag only when the map it was given
 * is entirely defaulted. See docs/modules/export.md.
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

/*
 * One <part>. id and name are borrowed (the document escapes them); staff_count is at least 1. midi_program is a
 * General MIDI program, 0 to 127 (written 1-based as <midi-program> in a <score-instrument> and <midi-instrument>);
 * -1 writes no instrument. A zeroed struct is program 0.
 */
typedef struct {
    const char *id;
    const char *name;
    int staff_count;
    int midi_program;
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
 *   - events / count, and optionally pitch_values index-paired with them as the importers return them. Pitch is
 *     written from each event's PitchSpelling; pitch_values (which may be NULL) is checked for NULL entries and not
 *     otherwise read.
 *   - part_index / staff: optional, parallel to events (a NULL array means part 0, or staff 1).
 *   - parts / part_count: the parts; none means one part "P1" whose staff_count is the highest staff used.
 *   - meter_map: the measure layout. NULL lays measures from a defaulted 4/4 map and flags the output meter-defaulted.
 *   - tempo_map: written as <sound tempo> at each region that is not defaulted, in part 0.
 *   - tempo_markings (+ tempo_marking_part): written as <direction> text and <metronome>; part arrays default to 0.
 *   - dynamics (+ dynamics_part, dynamics_staff): written at the start of the Dynamics' span, or at its event.
 *   - key_signatures: written in their own part.
 *   - quantize: NULL writes every note at its exact played start and duration. When set, score_quantize (see
 *     export/score_quantize.h) turns the played times into notated times on the beat grid of meter_map first, and the
 *     document is written from those; the events themselves, their ids and their order are untouched.
 *   - title / composer: written as <work><work-title> and <identification><creator type="composer"> (escaped); NULL or
 *     "" writes nothing.
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
    const ScoreQuantizeOptions *quantize;
    const char *title;    /* written as <work><work-title>; NULL or "" writes nothing */
    const char *composer; /* written as <creator type="composer">; NULL or "" writes nothing */
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
 * duration an integer, and nothing is rounded (unless score->quantize is set, which writes notated times instead of
 * played ones; ScoreQuantizeResult failures other than allocation are MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE). On any result other than OK *out_xml is not written.
 *
 * Notes and rests are written as notated values: after the cut at barlines, each piece is cut by the beat of its
 * measure (rules in docs/modules/export.md) into values with a <type>, up to two <dot/>s and, for two thirds of a
 * plain value, a <time-modification> with <tuplet> start and stop around a beat's group. A value is tied to the next
 * piece of its note, and a measure no voice sounds in is one <rest measure="yes"/>. A length no value can write keeps
 * its exact <duration> and has no <type>.
 *
 * Notes written as an eighth or shorter are beamed: back to back inside one beat group (a beat, or the dotted beat of
 * a compound meter), two or more form a <beam> group, level 1 for an eighth, 2 for a 16th, 3 for a 32nd, with
 * begin, continue and end, and a forward or backward hook on a lone short value. A rest or a longer note ends a
 * group, a lone note is not beamed, and a chord is beamed once, on its first note.
 *
 * Chords and overlaps are written, never refused (MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES is not returned here). Within
 * a voice (part, staff, VoiceReference), notes with the same start and end are a chord: the first is a plain <note>
 * and each other is a <note> that begins with <chord/>. Any other note that starts while an earlier one still sounds
 * goes to the lowest lane of that voice that is free, a new lane when none is; each lane after the first is written
 * as an extra <voice>, numbered after the voices the part already uses, on the same staff.
 */
MusicXmlExportResult musicxml_export_write_score(const MusicXmlExportScore *score, char **out_xml);

#endif
