#include "io/midi_importer.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "io/midi_event_decoder.h"
#include "io/midi_key_signature.h"
#include "io/smf_reader.h"
#include "pitch/midi_pitch.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/tempo_map.h"
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
 * meaning -- a placeholder pending Deliverable F): a
 * MIDI import may contain temporally overlapping notes (e.g. a
 * two-staff piano piece with RH/LH on separate tracks, or two-handed
 * piano recorded onto one track). MusicXML cannot represent simultaneous
 * notes in one voice, so every import gets each NoteEvent's optional_voice
 * assigned via greedy interval-graph coloring by timing, scoped per
 * source MTrk chunk: within each track, processing its notes in
 * start-time order, a note is assigned the lowest-numbered voice slot
 * not occupied by a still-sounding note (from that same track) at its
 * start time, opening a new slot only when every existing slot for that
 * track is occupied. Voice slots are numbered independently per track --
 * a single-track file is simply the one-track case of this. Channels
 * inside one track share that track's slots: there is no channel
 * mapping (R-386).
 */

#define MIDI_IMPORTER_PROVENANCE_ID "midi-importer-v1"

typedef struct {
    uint32_t start_tick;
    uint32_t duration_ticks;
    uint8_t note;
    uint8_t channel;
    size_t order;
    size_t track_index;
    int voice_slot; /* the voice slot within the note's track (assign_voice_slots_per_track); -1 until assigned */
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

/* Greedy interval-graph coloring by timing over a single group of notes:
   matched must already be sorted by start_tick (ties by order). Assigns
   each entry's voice_slot to the lowest-numbered slot whose previously
   assigned note has already finished sounding by this note's start,
   opening a new slot otherwise. Voice slots are local to this group. */
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

/* Runs assign_voice_slots independently within each source track, so
   voice slots are colored per-track rather than across the whole file.
   all_matched must already be sorted by start_tick (ties by order);
   that order is preserved within each per-track subset. */
static int assign_voice_slots_per_track(MatchedNote *all_matched, size_t all_matched_count) {
    if (all_matched_count == 0) {
        return 0;
    }

    MatchedNote *subset = malloc(sizeof(MatchedNote) * all_matched_count);
    if (subset == NULL) {
        return -1;
    }

    size_t *subset_index = malloc(sizeof(size_t) * all_matched_count);
    if (subset_index == NULL) {
        free(subset);
        return -1;
    }

    bool *done = calloc(all_matched_count, sizeof(bool));
    if (done == NULL) {
        free(subset);
        free(subset_index);
        return -1;
    }

    int result = 0;
    for (size_t i = 0; i < all_matched_count; i++) {
        if (done[i]) {
            continue;
        }
        size_t track_index = all_matched[i].track_index;
        size_t subset_count = 0;
        for (size_t j = i; j < all_matched_count; j++) {
            if (all_matched[j].track_index == track_index) {
                subset[subset_count] = all_matched[j];
                subset_index[subset_count] = j;
                subset_count++;
                done[j] = true;
            }
        }
        if (assign_voice_slots(subset, subset_count) != 0) {
            result = -1;
            break;
        }
        for (size_t k = 0; k < subset_count; k++) {
            all_matched[subset_index[k]].voice_slot = subset[k].voice_slot;
        }
    }

    free(subset);
    free(subset_index);
    free(done);
    return result;
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
    if (!pitch_spelling_of_midi(matched->note, 0, &spelling)) {
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
        char voice_id_buf[48];
        snprintf(voice_id_buf, sizeof(voice_id_buf), "midi-voice-%zu-%d", matched->track_index, matched->voice_slot);
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

static MidiImportResult map_smf_failure(SmfReaderResult smf_result) {
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

static MidiImportResult map_decoder_failure(MidiEventDecoderResult decoded) {
    return decoded == MIDI_EVENT_DECODER_ERR_ALLOC ? MIDI_IMPORT_ERR_ALLOC : MIDI_IMPORT_ERR_BAD_TRACK;
}

/* Pairs, sorts, voices and builds the notes of already-scanned tracks. On OK the outputs are new
   arrays (NULL when there are no notes); otherwise they are untouched. */
static MidiImportResult build_notes(uint16_t division, const MidiTrackScan *scan_tracks, size_t track_count,
                                    NoteEvent ***out_events, PitchNumericValue ***out_pitch_values,
                                    size_t **out_note_tracks, uint8_t **out_note_channels, size_t *out_count) {
    unsigned long ticks_denominator = 4UL * (unsigned long)division;

    EntityID *tuning_id = NULL;
    if (entity_id_create("midi-note-number", &tuning_id) != ENTITY_ID_OK) {
        return MIDI_IMPORT_ERR_ALLOC;
    }
    TuningSystem *tuning_system = NULL;
    if (tuning_system_create(tuning_id, "MIDI note number", &tuning_system) != TUNING_SYSTEM_OK) {
        entity_id_destroy(tuning_id);
        return MIDI_IMPORT_ERR_ALLOC;
    }

    MatchedNote *all_matched = NULL;
    size_t all_matched_count = 0;
    size_t all_matched_capacity = 0;
    MidiImportResult failure = MIDI_IMPORT_OK;

    for (size_t t = 0; t < track_count && failure == MIDI_IMPORT_OK; t++) {
        MatchedNote *matched = NULL;
        size_t matched_count = 0;
        if (pair_notes(scan_tracks[t].notes, scan_tracks[t].note_count, &matched, &matched_count) != 0) {
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
            matched[i].track_index = t;
            all_matched[all_matched_count + i] = matched[i];
        }
        all_matched_count += matched_count;
        free(matched);
    }

    if (failure != MIDI_IMPORT_OK) {
        free(all_matched);
        tuning_system_destroy(tuning_system);
        return failure;
    }

    qsort(all_matched, all_matched_count, sizeof(MatchedNote), matched_note_compare);

    if (assign_voice_slots_per_track(all_matched, all_matched_count) != 0) {
        free(all_matched);
        tuning_system_destroy(tuning_system);
        return MIDI_IMPORT_ERR_ALLOC;
    }

    NoteEvent **note_events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t *note_tracks = NULL;
    uint8_t *note_channels = NULL;
    if (all_matched_count > 0) {
        note_events = malloc(sizeof(NoteEvent *) * all_matched_count);
        pitch_values = malloc(sizeof(PitchNumericValue *) * all_matched_count);
        note_tracks = malloc(sizeof(size_t) * all_matched_count);
        note_channels = malloc(sizeof(uint8_t) * all_matched_count);
        if (note_events == NULL || pitch_values == NULL || note_tracks == NULL || note_channels == NULL) {
            free(note_events);
            free(pitch_values);
            free(note_tracks);
            free(note_channels);
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
            free(note_tracks);
            free(note_channels);
            free(all_matched);
            tuning_system_destroy(tuning_system);
            return MIDI_IMPORT_ERR_ALLOC;
        }
        note_events[built_count] = note_event;
        pitch_values[built_count] = pitch_value;
        note_tracks[built_count] = all_matched[i].track_index;
        note_channels[built_count] = all_matched[i].channel;
        built_count++;
    }

    free(all_matched);
    tuning_system_destroy(tuning_system);

    *out_events = note_events;
    *out_pitch_values = pitch_values;
    *out_note_tracks = note_tracks;
    *out_note_channels = note_channels;
    *out_count = built_count;
    return MIDI_IMPORT_OK;
}

void midi_meter_import_destroy(MidiMeterImport *meter) {
    if (meter == NULL) {
        return;
    }
    meter_map_destroy(meter->meter_map);
    musical_time_destroy(meter->end_time);
    free(meter->conflict_ticks);
    meter->meter_map = NULL;
    meter->end_time = NULL;
    meter->conflict_ticks = NULL;
    meter->conflict_count = 0;
}

/* Builds the meter from signatures already collected (they stay owned by the caller). denominator is
   4 * division; end_tick is the tick of the last note-off. */
static MidiImportResult build_meter_from(unsigned long denominator, uint32_t end_tick,
                                         const MidiTimeSignatureEvents *signatures, MidiMeterImport *out) {
    MidiMeterImport result = {NULL, NULL, NULL, 0};
    MidiImportResult failure = MIDI_IMPORT_OK;

    EntityID *map_id = NULL;
    if (entity_id_create("midi-meter-map", &map_id) != ENTITY_ID_OK) {
        failure = MIDI_IMPORT_ERR_ALLOC;
    } else if (signatures->count == 0) {
        if (meter_map_create_defaulted(map_id, &result.meter_map) != METER_MAP_OK) {
            entity_id_destroy(map_id);
            failure = MIDI_IMPORT_ERR_ALLOC;
        }
    } else {
        MeterMapChangePoint *points = calloc(signatures->count, sizeof(MeterMapChangePoint));
        MusicalTime **starts = calloc(signatures->count, sizeof(MusicalTime *));
        if (points == NULL || starts == NULL) {
            failure = MIDI_IMPORT_ERR_ALLOC;
        } else {
            for (size_t i = 0; i < signatures->count && failure == MIDI_IMPORT_OK; i++) {
                if (musical_time_create((long)signatures->events[i].tick, denominator, &starts[i]) !=
                    MUSICAL_TIME_OK) {
                    failure = MIDI_IMPORT_ERR_ALLOC;
                    break;
                }
                points[i].time_signature = signatures->events[i].time_signature;
                points[i].start = starts[i];
            }
            if (failure == MIDI_IMPORT_OK &&
                meter_map_create(map_id, points, signatures->count, &result.meter_map) != METER_MAP_OK) {
                failure = MIDI_IMPORT_ERR_ALLOC;
            }
        }
        if (starts != NULL) {
            for (size_t i = 0; i < signatures->count; i++) {
                musical_time_destroy(starts[i]);
            }
        }
        free(starts);
        free(points);
        if (failure != MIDI_IMPORT_OK) {
            entity_id_destroy(map_id);
        }
    }

    if (failure == MIDI_IMPORT_OK &&
        musical_time_create((long)end_tick, denominator, &result.end_time) != MUSICAL_TIME_OK) {
        failure = MIDI_IMPORT_ERR_ALLOC;
    }
    if (failure == MIDI_IMPORT_OK && signatures->conflict_count > 0) {
        result.conflict_ticks = malloc(sizeof(uint64_t) * signatures->conflict_count);
        if (result.conflict_ticks == NULL) {
            failure = MIDI_IMPORT_ERR_ALLOC;
        } else {
            for (size_t i = 0; i < signatures->conflict_count; i++) {
                result.conflict_ticks[i] = signatures->conflict_ticks[i];
            }
            result.conflict_count = signatures->conflict_count;
        }
    }

    if (failure != MIDI_IMPORT_OK) {
        midi_meter_import_destroy(&result);
        return failure;
    }
    *out = result;
    return MIDI_IMPORT_OK;
}

/* The tick of the last note-off over all scanned tracks, 0 with no notes. */
static uint32_t last_note_off_tick(const MidiTrackScan *tracks, size_t track_count) {
    uint32_t end_tick = 0;
    for (size_t t = 0; t < track_count; t++) {
        for (size_t i = 0; i < tracks[t].note_count; i++) {
            if (tracks[t].notes[i].kind == MIDI_NOTE_OFF && tracks[t].notes[i].tick > end_tick) {
                end_tick = tracks[t].notes[i].tick;
            }
        }
    }
    return end_tick;
}

void midi_tempo_import_destroy(MidiTempoImport *tempo) {
    if (tempo == NULL) {
        return;
    }
    tempo_map_destroy(tempo->tempo_map);
    free(tempo->conflict_ticks);
    tempo->tempo_map = NULL;
    tempo->conflict_ticks = NULL;
    tempo->conflict_count = 0;
}

/* Builds the tempo map from tempos already collected (they stay owned by the caller). */
static MidiImportResult build_tempo_from(unsigned long denominator, const MidiTempoEvents *tempos,
                                         MidiTempoImport *out) {
    MidiTempoImport result = {NULL, NULL, 0};
    MidiImportResult failure = MIDI_IMPORT_OK;

    EntityID *map_id = NULL;
    if (entity_id_create("midi-tempo-map", &map_id) != ENTITY_ID_OK) {
        failure = MIDI_IMPORT_ERR_ALLOC;
    } else if (tempos->count == 0) {
        if (tempo_map_create_defaulted(map_id, &result.tempo_map) != TEMPO_MAP_OK) {
            entity_id_destroy(map_id);
            failure = MIDI_IMPORT_ERR_ALLOC;
        }
    } else {
        TempoMapChangePoint *points = calloc(tempos->count, sizeof(TempoMapChangePoint));
        MusicalTime **starts = calloc(tempos->count, sizeof(MusicalTime *));
        if (points == NULL || starts == NULL) {
            failure = MIDI_IMPORT_ERR_ALLOC;
        } else {
            for (size_t i = 0; i < tempos->count && failure == MIDI_IMPORT_OK; i++) {
                if (tempos->events[i].tick > (uint64_t)LONG_MAX ||
                    musical_time_create((long)tempos->events[i].tick, denominator, &starts[i]) != MUSICAL_TIME_OK) {
                    failure = MIDI_IMPORT_ERR_ALLOC;
                    break;
                }
                points[i].start = starts[i];
                points[i].microseconds_per_quarter = tempos->events[i].microseconds_per_quarter;
            }
            if (failure == MIDI_IMPORT_OK &&
                tempo_map_create(map_id, points, tempos->count, &result.tempo_map) != TEMPO_MAP_OK) {
                failure = MIDI_IMPORT_ERR_ALLOC;
            }
        }
        if (starts != NULL) {
            for (size_t i = 0; i < tempos->count; i++) {
                musical_time_destroy(starts[i]);
            }
        }
        free(starts);
        free(points);
        if (failure != MIDI_IMPORT_OK) {
            entity_id_destroy(map_id);
        }
    }

    if (failure == MIDI_IMPORT_OK && tempos->conflict_count > 0) {
        result.conflict_ticks = malloc(sizeof(uint64_t) * tempos->conflict_count);
        if (result.conflict_ticks == NULL) {
            failure = MIDI_IMPORT_ERR_ALLOC;
        } else {
            for (size_t i = 0; i < tempos->conflict_count; i++) {
                result.conflict_ticks[i] = tempos->conflict_ticks[i];
            }
            result.conflict_count = tempos->conflict_count;
        }
    }

    if (failure != MIDI_IMPORT_OK) {
        midi_tempo_import_destroy(&result);
        return failure;
    }
    *out = result;
    return MIDI_IMPORT_OK;
}

void midi_import_destroy(MidiImport *import) {
    if (import == NULL) {
        return;
    }
    destroy_note_events(import->events, import->count);
    destroy_pitch_values(import->pitch_values, import->count);
    free(import->note_tracks);
    free(import->note_channels);
    midi_meter_import_destroy(&import->meter);
    midi_tempo_import_destroy(&import->tempo);
    midi_key_signature_events_destroy(&import->key_signatures);
    if (import->tracks != NULL) {
        for (size_t t = 0; t < import->track_count; t++) {
            free(import->tracks[t].name);
            for (size_t p = 0; p < import->tracks[t].program_count; p++) {
                musical_time_destroy(import->tracks[t].programs[p].time);
            }
            free(import->tracks[t].programs);
        }
        free(import->tracks);
    }
    memset(import, 0, sizeof(*import));
}

/* Moves each scanned track's name and program changes into MidiTrackInfo, with the exact time of each
   program change. */
static MidiImportResult build_track_info(unsigned long denominator, MidiFileScan *scan, MidiImport *result) {
    if (scan->track_count == 0) {
        return MIDI_IMPORT_OK;
    }
    result->tracks = calloc(scan->track_count, sizeof(MidiTrackInfo));
    if (result->tracks == NULL) {
        return MIDI_IMPORT_ERR_ALLOC;
    }
    result->track_count = scan->track_count;

    for (size_t t = 0; t < scan->track_count; t++) {
        MidiTrackInfo *info = &result->tracks[t];
        MidiTrackScan *scanned = &scan->tracks[t];
        info->index = t;
        info->name = scanned->name;
        scanned->name = NULL;
        if (scanned->program_count == 0) {
            continue;
        }
        info->programs = calloc(scanned->program_count, sizeof(MidiProgramChange));
        if (info->programs == NULL) {
            return MIDI_IMPORT_ERR_ALLOC;
        }
        for (size_t p = 0; p < scanned->program_count; p++) {
            MidiProgramChange *change = &info->programs[p];
            if (musical_time_create((long)scanned->programs[p].tick, denominator, &change->time) !=
                MUSICAL_TIME_OK) {
                return MIDI_IMPORT_ERR_ALLOC;
            }
            change->tick = scanned->programs[p].tick;
            change->channel = scanned->programs[p].channel;
            change->program = scanned->programs[p].program;
            info->program_count++;
        }
    }
    return MIDI_IMPORT_OK;
}

MidiImportResult midi_importer_import_complete(const unsigned char *data, size_t size, MidiImport *out) {
    if (data == NULL || out == NULL) {
        return MIDI_IMPORT_ERR_NULL_BUFFER;
    }

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult smf_result = smf_reader_parse(data, size, &header, &tracks, &track_count);
    if (smf_result != SMF_READER_OK) {
        return map_smf_failure(smf_result);
    }
    if (header.division == 0) {
        free(tracks);
        return MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION;
    }
    unsigned long denominator = 4UL * (unsigned long)header.division;

    MidiKeySignatureCollector *keys = midi_key_signature_collector_create();
    if (keys == NULL) {
        free(tracks);
        return MIDI_IMPORT_ERR_ALLOC;
    }
    MidiFileScan scan;
    MidiEventDecoderResult scanned = midi_event_decoder_scan(tracks, track_count, keys, &scan);
    free(tracks);
    if (scanned != MIDI_EVENT_DECODER_OK) {
        midi_key_signature_collector_destroy(keys);
        return map_decoder_failure(scanned);
    }

    MidiImport result;
    memset(&result, 0, sizeof(result));
    result.format = header.format;
    result.division = header.division;

    MidiImportResult failure = build_notes(header.division, scan.tracks, scan.track_count, &result.events,
                                           &result.pitch_values, &result.note_tracks, &result.note_channels,
                                           &result.count);
    if (failure == MIDI_IMPORT_OK) {
        failure = build_meter_from(denominator, last_note_off_tick(scan.tracks, scan.track_count),
                                   &scan.time_signatures, &result.meter);
    }
    if (failure == MIDI_IMPORT_OK) {
        failure = build_tempo_from(denominator, &scan.tempos, &result.tempo);
    }
    if (failure == MIDI_IMPORT_OK) {
        MidiKeySignatureScanAllResult finished =
            midi_key_signature_collector_finish(keys, header.division, &result.key_signatures);
        if (finished != MIDI_KEY_SIGNATURE_SCAN_ALL_OK) {
            failure = MIDI_IMPORT_ERR_ALLOC;
        }
    }
    if (failure == MIDI_IMPORT_OK) {
        failure = build_track_info(denominator, &scan, &result);
    }
    midi_file_scan_destroy(&scan);
    midi_key_signature_collector_destroy(keys);

    if (failure != MIDI_IMPORT_OK) {
        midi_import_destroy(&result);
        return failure;
    }
    *out = result;
    return MIDI_IMPORT_OK;
}

/* Moves the notes out of a complete import into the old entry points' outputs and destroys the rest. */
static void take_notes(MidiImport *import, NoteEvent ***out_events, PitchNumericValue ***out_pitch_values,
                       size_t *out_count) {
    *out_events = import->events;
    *out_pitch_values = import->pitch_values;
    *out_count = import->count;
    import->events = NULL;
    import->pitch_values = NULL;
    import->count = 0;
}

MidiImportResult midi_importer_import(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                       PitchNumericValue ***out_pitch_values, size_t *out_count) {
    if (out_events == NULL || out_pitch_values == NULL || out_count == NULL) {
        return MIDI_IMPORT_ERR_NULL_BUFFER;
    }

    MidiImport import;
    MidiImportResult imported = midi_importer_import_complete(data, size, &import);
    if (imported != MIDI_IMPORT_OK) {
        return imported;
    }
    take_notes(&import, out_events, out_pitch_values, out_count);
    midi_import_destroy(&import);
    return MIDI_IMPORT_OK;
}

MidiImportResult midi_importer_import_with_meter(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                                  PitchNumericValue ***out_pitch_values, size_t *out_count,
                                                  MidiMeterImport *out_meter) {
    if (out_events == NULL || out_pitch_values == NULL || out_count == NULL || out_meter == NULL) {
        return MIDI_IMPORT_ERR_NULL_BUFFER;
    }

    MidiImport import;
    MidiImportResult imported = midi_importer_import_complete(data, size, &import);
    if (imported != MIDI_IMPORT_OK) {
        return imported;
    }
    take_notes(&import, out_events, out_pitch_values, out_count);
    *out_meter = import.meter;
    memset(&import.meter, 0, sizeof(import.meter));
    midi_import_destroy(&import);
    return MIDI_IMPORT_OK;
}

MidiImportResult midi_importer_import_with_tempo(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                                  PitchNumericValue ***out_pitch_values, size_t *out_count,
                                                  MidiTempoImport *out_tempo) {
    if (out_events == NULL || out_pitch_values == NULL || out_count == NULL || out_tempo == NULL) {
        return MIDI_IMPORT_ERR_NULL_BUFFER;
    }

    MidiImport import;
    MidiImportResult imported = midi_importer_import_complete(data, size, &import);
    if (imported != MIDI_IMPORT_OK) {
        return imported;
    }
    take_notes(&import, out_events, out_pitch_values, out_count);
    *out_tempo = import.tempo;
    memset(&import.tempo, 0, sizeof(import.tempo));
    midi_import_destroy(&import);
    return MIDI_IMPORT_OK;
}
