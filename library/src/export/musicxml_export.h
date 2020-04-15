#ifndef MUSIC_EXPORT_MUSICXML_EXPORT_H
#define MUSIC_EXPORT_MUSICXML_EXPORT_H

#include "persistence/persistence.h"

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
    MUSICXML_EXPORT_ERR_ALLOC
} MusicXmlExportResult;

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

#endif
