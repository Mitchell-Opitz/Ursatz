#include "io/midi_importer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "io/midi_event_decoder.h"
#include "io/smf_reader.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * MIDIImporter: see midi_importer.h and docs/modules/io.md for scope.
 *
 * Tempo-independent units: this importer does not read tempo meta-events
 * (out of scope, see docs/modules/io.md), and does not assume one --
 * MusicalTime/Duration are produced as an exact whole-note fraction
 * derived from tick counts and the header's ticks-per-quarter-note
 * (PPQN) division field alone: a quarter note is PPQN ticks, a whole
 * note is 4 * PPQN ticks, so position/duration in ticks becomes
 * numerator = ticks, denominator = 4 * PPQN (Law 19 compliant exact
 * rational). No BPM/seconds conversion is performed; the same tick
 * count always yields the same Duration regardless of tempo, since PPQN
 * is fixed per file and MIDI ticks carry no tempo information of their
 * own until combined with a tempo meta-event (not read here).
 *
 * The provenance entity id "midi-importer-v1" identifies this importer
 * as the source of every NoteEvent it produces. No ProvenanceActivity/
 * ProvenanceAgent type exists in this codebase yet (deliberately not
 * added in this pass); every NoteEvent's ProvenanceReference points at
 * this one stable EntityID instead.
 *
 * Spelling convention: a MIDI note number carries no key context, so
 * this importer always spells the 5 black-key pitch classes as the
 * sharp of the natural letter below (accidental = 1) rather than the
 * flat of the letter above -- e.g. MIDI 61 is spelled C#, never Db.
 * Octave follows the standard MIDI convention (note 60 = C4):
 * octave = note / 12 - 1.
 *
 * Voice assignment (MusicXML-export-grouping only, no musical/analytical
 * meaning -- a placeholder pending Deliverable F): a single-track,
 * single-channel MIDI import may still contain temporally overlapping
 * notes (e.g. two-handed piano recorded onto one track/channel).
 * MusicXML cannot represent simultaneous notes in one voice, so such
 * imports get each NoteEvent's optional_voice assigned via greedy
 * interval-graph coloring by timing: processing notes in start-time
 * order, a note is assigned the lowest-numbered voice slot not occupied
 * by a still-sounding note at its start time, opening a new slot only
 * when every existing slot is occupied. Multi-track or multi-channel
 * imports are left with optional_voice == NULL -- voice separation
 * there is a track/channel-mapping decision for a future pass, not this
 * one.
 */

static const PitchLetter SHARPS_ONLY_LETTER[12] = {
    PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
    PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
static const int SHARPS_ONLY_ACCIDENTAL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static PitchSpellingResult spell_midi_note(uint8_t midi_note, PitchSpelling **out) {
    int pitch_class = midi_note % 12;
    int octave = midi_note / 12 - 1;
    return pitch_spelling_create(SHARPS_ONLY_LETTER[pitch_class], SHARPS_ONLY_ACCIDENTAL[pitch_class], octave, out);
}

#define MIDI_IMPORTER_PROVENANCE_ID "midi-importer-v1"

typedef struct {
    uint32_t start_tick;
    uint32_t duration_ticks;
    uint8_t note;
    uint8_t channel;
    size_t order;
    int voice_slot; /* -1 when no voice assigned (multi-track/multi-channel import) */
} MatchedNote;

static int matched_note_compare(const void *a, const void *b) {
    const MatchedNote *ma = a;
    const MatchedNote *mb = b;
    if (ma->start_tick != mb->start_tick) {
        return (ma->start_tick < mb->start_tick) ? -1 : 1;
    }
    if (ma->order != mb->order) {
        return (ma->order < mb->order) ? -1 : 1;
    }
    return 0;
}

/* Pairs note-on/note-off events (matched by channel+note, earliest
   unmatched note-on to earliest following note-off, per channel+note)
   within one track's already-decoded event list. Returns 0 on success
   (an unmatched note-on at end of track is a malformed-file condition)
   and writes *out_count matched notes into a newly malloc'd array. */
static int pair_notes(const MidiNoteEvent *events, size_t event_count, MatchedNote **out_matched,
                       size_t *out_count) {
    unsigned char *used = calloc(event_count, sizeof(unsigned char));
    if (used == NULL && event_count > 0) {
        return -1;
    }

    MatchedNote *matched = malloc(sizeof(MatchedNote) * (event_count / 2 + 1));
    if (matched == NULL) {
        free(used);
        return -1;
    }
    size_t matched_count = 0;

    for (size_t i = 0; i < event_count; i++) {
        if (events[i].kind != MIDI_NOTE_ON || used[i]) {
            continue;
        }
        used[i] = 1;

        size_t off_index = event_count;
        for (size_t j = i + 1; j < event_count; j++) {
            if (used[j]) {
                continue;
            }
            if (events[j].kind == MIDI_NOTE_OFF && events[j].channel == events[i].channel &&
                events[j].note == events[i].note) {
                off_index = j;
                break;
            }
        }

        if (off_index == event_count) {
            /* Unmatched note-on: malformed for this importer's scope. */
            free(used);
            free(matched);
            return -1;
        }
        used[off_index] = 1;

        matched[matched_count].start_tick = events[i].tick;
        matched[matched_count].duration_ticks = events[off_index].tick - events[i].tick;
        matched[matched_count].note = events[i].note;
        matched[matched_count].channel = events[i].channel;
        matched[matched_count].order = i;
        matched[matched_count].voice_slot = -1;
        matched_count++;
    }

    free(used);
    *out_matched = matched;
    *out_count = matched_count;
    return 0;
}

/* Greedy interval-graph coloring by timing: matched must already be
   sorted by start_tick (ties by order). Assigns each entry's voice_slot
   to the lowest-numbered slot whose previously assigned note has
   already finished sounding by this note's start, opening a new slot
   otherwise. */
static int assign_voice_slots(MatchedNote *matched, size_t count) {
    if (count == 0) {
        return 0;
    }
    uint32_t *slot_end_tick = malloc(sizeof(uint32_t) * count);
    if (slot_end_tick == NULL) {
        return -1;
    }
    size_t slot_count = 0;

    for (size_t i = 0; i < count; i++) {
        size_t chosen = slot_count;
        for (size_t s = 0; s < slot_count; s++) {
            if (slot_end_tick[s] <= matched[i].start_tick) {
                chosen = s;
                break;
            }
        }
        if (chosen == slot_count) {
            slot_count++;
        }
        matched[i].voice_slot = (int)chosen;
        slot_end_tick[chosen] = matched[i].start_tick + matched[i].duration_ticks;
    }

    free(slot_end_tick);
    return 0;
}

static void destroy_note_events(NoteEvent **events, size_t count) {
    if (events == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);
}

static void destroy_pitch_values(PitchNumericValue **values, size_t count) {
    if (values == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        pitch_numeric_value_destroy(values[i]);
    }
    free(values);
}

static NoteEventResult build_note_event(const MatchedNote *matched, unsigned long ticks_denominator,
                                         const TuningSystem *tuning_system, size_t sequence_number, NoteEvent **out,
                                         PitchNumericValue **out_pitch_value) {
    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "midi-note-%zu", sequence_number);

    EntityID *id = NULL;
    if (entity_id_create(id_buf, &id) != ENTITY_ID_OK) {
        return NOTE_EVENT_ERR_ALLOC;
    }

    MusicalTime *position = NULL;
    if (musical_time_create((long)matched->start_tick, ticks_denominator, &position) != MUSICAL_TIME_OK) {
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    Duration *duration = NULL;
    if (duration_create((long)matched->duration_ticks, ticks_denominator, &duration) != DURATION_OK) {
        temporal_anchor_destroy(anchor);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    PitchNumericValue *numeric = NULL;
    if (pitch_numeric_value_create(tuning_system, matched->note, 1, &numeric) != PITCH_NUMERIC_VALUE_OK) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    PitchIdentity *pitch = NULL;
    PitchNumericValueToIdentityResult to_identity_result = pitch_numeric_value_to_identity(numeric, &pitch);
    if (to_identity_result != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK) {
        pitch_numeric_value_destroy(numeric);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    PitchSpelling *spelling = NULL;
    if (spell_midi_note(matched->note, &spelling) != PITCH_SPELLING_OK) {
        pitch_numeric_value_destroy(numeric);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    EntityID *provenance_id = NULL;
    if (entity_id_create(MIDI_IMPORTER_PROVENANCE_ID, &provenance_id) != ENTITY_ID_OK) {
        pitch_spelling_destroy(spelling);
        pitch_numeric_value_destroy(numeric);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    ProvenanceReference *provenance = NULL;
    if (provenance_reference_create_present(provenance_id, &provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(provenance_id);
        pitch_spelling_destroy(spelling);
        pitch_numeric_value_destroy(numeric);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        entity_id_destroy(id);
        return NOTE_EVENT_ERR_ALLOC;
    }

    VoiceReference *voice = NULL;
    if (matched->voice_slot >= 0) {
        char voice_id_buf[32];
        snprintf(voice_id_buf, sizeof(voice_id_buf), "midi-voice-%d", matched->voice_slot);
        EntityID *voice_id = NULL;
        if (entity_id_create(voice_id_buf, &voice_id) != ENTITY_ID_OK) {
            pitch_spelling_destroy(spelling);
            pitch_numeric_value_destroy(numeric);
            pitch_identity_destroy(pitch);
            duration_destroy(duration);
            temporal_anchor_destroy(anchor);
            entity_id_destroy(id);
            return NOTE_EVENT_ERR_ALLOC;
        }
        if (voice_reference_create(voice_id, &voice) != VOICE_REFERENCE_OK) {
            entity_id_destroy(voice_id);
            pitch_spelling_destroy(spelling);
            pitch_numeric_value_destroy(numeric);
            pitch_identity_destroy(pitch);
            duration_destroy(duration);
            temporal_anchor_destroy(anchor);
            entity_id_destroy(id);
            return NOTE_EVENT_ERR_ALLOC;
        }
    }

    NoteEventResult result = note_event_create(id, anchor, duration, pitch, spelling, voice, provenance, out);
    if (result != NOTE_EVENT_OK) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_numeric_value_destroy(numeric);
        return result;
    }

    *out_pitch_value = numeric;
    return NOTE_EVENT_OK;
}

MidiImportResult midi_importer_import(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                       PitchNumericValue ***out_pitch_values, size_t *out_count) {
    if (data == NULL || out_events == NULL || out_pitch_values == NULL || out_count == NULL) {
        return MIDI_IMPORT_ERR_NULL_BUFFER;
    }

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult smf_result = smf_reader_parse(data, size, &header, &tracks, &track_count);
    if (smf_result != SMF_READER_OK) {
        switch (smf_result) {
        case SMF_READER_ERR_NULL_BUFFER:
            return MIDI_IMPORT_ERR_NULL_BUFFER;
        case SMF_READER_ERR_TRUNCATED:
            return MIDI_IMPORT_ERR_TRUNCATED;
        case SMF_READER_ERR_UNSUPPORTED_FORMAT:
            return MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT;
        case SMF_READER_ERR_UNSUPPORTED_DIVISION:
            return MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION;
        case SMF_READER_ERR_BAD_TRACK:
            return MIDI_IMPORT_ERR_BAD_TRACK;
        case SMF_READER_ERR_BAD_HEADER:
        default:
            return MIDI_IMPORT_ERR_BAD_HEADER;
        }
    }

    unsigned long ticks_denominator = 4UL * (unsigned long)header.division;

    EntityID *tuning_id = NULL;
    if (entity_id_create("midi-note-number", &tuning_id) != ENTITY_ID_OK) {
        free(tracks);
        return MIDI_IMPORT_ERR_ALLOC;
    }
    TuningSystem *tuning_system = NULL;
    if (tuning_system_create(tuning_id, "MIDI note number", &tuning_system) != TUNING_SYSTEM_OK) {
        entity_id_destroy(tuning_id);
        free(tracks);
        return MIDI_IMPORT_ERR_ALLOC;
    }

    MatchedNote *all_matched = NULL;
    size_t all_matched_count = 0;
    size_t all_matched_capacity = 0;
    MidiImportResult failure = MIDI_IMPORT_OK;

    for (size_t t = 0; t < track_count && failure == MIDI_IMPORT_OK; t++) {
        MidiNoteEvent *events = NULL;
        size_t event_count = 0;
        MidiEventDecoderResult decode_result =
            midi_event_decoder_decode(tracks[t].data, tracks[t].size, &events, &event_count);
        if (decode_result != MIDI_EVENT_DECODER_OK) {
            failure = MIDI_IMPORT_ERR_BAD_TRACK;
            break;
        }

        MatchedNote *matched = NULL;
        size_t matched_count = 0;
        int pair_result = pair_notes(events, event_count, &matched, &matched_count);
        free(events);
        if (pair_result != 0) {
            failure = MIDI_IMPORT_ERR_BAD_TRACK;
            break;
        }

        if (all_matched_count + matched_count > all_matched_capacity) {
            size_t new_capacity = all_matched_count + matched_count;
            MatchedNote *grown = realloc(all_matched, sizeof(MatchedNote) * new_capacity);
            if (grown == NULL) {
                free(matched);
                failure = MIDI_IMPORT_ERR_ALLOC;
                break;
            }
            all_matched = grown;
            all_matched_capacity = new_capacity;
        }
        for (size_t i = 0; i < matched_count; i++) {
            matched[i].order = all_matched_count + i;
            all_matched[all_matched_count + i] = matched[i];
        }
        all_matched_count += matched_count;
        free(matched);
    }

    free(tracks);

    if (failure != MIDI_IMPORT_OK) {
        free(all_matched);
        tuning_system_destroy(tuning_system);
        return failure;
    }

    qsort(all_matched, all_matched_count, sizeof(MatchedNote), matched_note_compare);

    bool single_channel = true;
    for (size_t i = 1; i < all_matched_count; i++) {
        if (all_matched[i].channel != all_matched[0].channel) {
            single_channel = false;
            break;
        }
    }
    if (track_count == 1 && single_channel) {
        if (assign_voice_slots(all_matched, all_matched_count) != 0) {
            free(all_matched);
            tuning_system_destroy(tuning_system);
            return MIDI_IMPORT_ERR_ALLOC;
        }
    }

    NoteEvent **note_events = NULL;
    PitchNumericValue **pitch_values = NULL;
    if (all_matched_count > 0) {
        note_events = malloc(sizeof(NoteEvent *) * all_matched_count);
        pitch_values = malloc(sizeof(PitchNumericValue *) * all_matched_count);
        if (note_events == NULL || pitch_values == NULL) {
            free(note_events);
            free(pitch_values);
            free(all_matched);
            tuning_system_destroy(tuning_system);
            return MIDI_IMPORT_ERR_ALLOC;
        }
    }

    size_t built_count = 0;
    for (size_t i = 0; i < all_matched_count; i++) {
        NoteEvent *note_event = NULL;
        PitchNumericValue *pitch_value = NULL;
        NoteEventResult r =
            build_note_event(&all_matched[i], ticks_denominator, tuning_system, i, &note_event, &pitch_value);
        if (r != NOTE_EVENT_OK) {
            destroy_note_events(note_events, built_count);
            destroy_pitch_values(pitch_values, built_count);
            free(all_matched);
            tuning_system_destroy(tuning_system);
            return MIDI_IMPORT_ERR_ALLOC;
        }
        note_events[built_count] = note_event;
        pitch_values[built_count] = pitch_value;
        built_count++;
    }

    free(all_matched);
    tuning_system_destroy(tuning_system);

    *out_events = note_events;
    *out_pitch_values = pitch_values;
    *out_count = built_count;
    return MIDI_IMPORT_OK;
}
