#include <ursatz_composer/motive_develop.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/augmentation.h"
#include "transform/contraction.h"
#include "transform/diminution.h"
#include "transform/fragmentation.h"
#include "transform/intervallic_manipulation.h"
#include "transform/inversion.h"
#include "transform/retrograde.h"
#include "transform/transform_result.h"
#include "transform/transposition.h"

#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_internal.h"

/*
 * The transformations of a motive. The real ones go through Ursatz: the motive becomes NoteEvents under a MIDI note
 * number tuning, Ursatz's transform is applied, and the NoteEvents it returns are read back as notes. A NoteEvent's
 * pitch is opaque (a PitchIdentity), so a pitch is read back by finding which of the 128 MIDI pitches it equals.
 */

#define MIDI_PITCHES 128

typedef struct {
    TuningSystem *tuning;
    NoteEvent *events[MOTIVE_MAX_NOTES];
    PitchNumericValue *values[MOTIVE_MAX_NOTES];
    size_t count;
    PitchIdentity *table[MIDI_PITCHES]; /* the identity of each MIDI pitch, to read a pitch back */
} Bridge;

static void bridge_close(Bridge *bridge) {
    for (size_t i = 0; i < bridge->count; i++) {
        note_event_destroy(bridge->events[i]);
        pitch_numeric_value_destroy(bridge->values[i]);
    }
    for (int pitch = 0; pitch < MIDI_PITCHES; pitch++) {
        pitch_identity_destroy(bridge->table[pitch]);
    }
    tuning_system_destroy(bridge->tuning);
    memset(bridge, 0, sizeof(*bridge));
}

/* One note as a NoteEvent and its pitch value. Everything is released again on failure. */
static bool build_event(Bridge *bridge, const PlanNote *note, size_t index) {
    EntityID *id = NULL;
    MusicalTime *position = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *duration = NULL;
    PitchNumericValue *value = NULL;
    PitchIdentity *identity = NULL;
    ProvenanceReference *provenance = NULL;
    char text[64];

    snprintf(text, sizeof(text), "composer-motive-note-%zu", index);
    bool ok = entity_id_create(text, &id) == ENTITY_ID_OK &&
              musical_time_create(note->offset.num, note->offset.den, &position) == MUSICAL_TIME_OK &&
              temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK;
    if (ok) {
        position = NULL;
    }
    ok = ok && duration_create(note->duration.num, note->duration.den, &duration) == DURATION_OK &&
         pitch_numeric_value_create(bridge->tuning, note->midi_pitch, 1, &value) == PITCH_NUMERIC_VALUE_OK &&
         pitch_numeric_value_to_identity(value, &identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
         provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;
    NoteEvent *event = NULL;
    if (ok && note_event_create(id, anchor, duration, identity, NULL, NULL, provenance, &event) == NOTE_EVENT_OK) {
        bridge->events[index] = event;
        bridge->values[index] = value;
        return true;
    }
    entity_id_destroy(id);
    musical_time_destroy(position);
    temporal_anchor_destroy(anchor);
    duration_destroy(duration);
    pitch_numeric_value_destroy(value);
    pitch_identity_destroy(identity);
    provenance_reference_destroy(provenance);
    return false;
}

static bool bridge_open(Bridge *bridge, const Motive *motive) {
    memset(bridge, 0, sizeof(*bridge));
    EntityID *tuning_id = NULL;
    if (entity_id_create("midi-note-number", &tuning_id) != ENTITY_ID_OK) {
        return false;
    }
    if (tuning_system_create(tuning_id, "MIDI note number", &bridge->tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(tuning_id);
        return false;
    }
    for (int pitch = 0; pitch < MIDI_PITCHES; pitch++) {
        PitchNumericValue *value = NULL;
        bool ok = pitch_numeric_value_create(bridge->tuning, pitch, 1, &value) == PITCH_NUMERIC_VALUE_OK &&
                  pitch_numeric_value_to_identity(value, &bridge->table[pitch]) ==
                      PITCH_NUMERIC_VALUE_TO_IDENTITY_OK;
        pitch_numeric_value_destroy(value);
        if (!ok) {
            bridge_close(bridge);
            return false;
        }
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        if (!build_event(bridge, &motive->notes[i], i)) {
            bridge_close(bridge);
            return false;
        }
        bridge->count++;
    }
    return true;
}

/* A transformed NoteEvent as a note. MOTIVE_DEV_ERR_RANGE when its pitch is not one of the 128 MIDI pitches. */
static MotiveDevResult read_event(const Bridge *bridge, const NoteEvent *event, PlanNote *note) {
    const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(event));
    const Duration *duration = note_event_duration(event);
    note->offset = motive_time(musical_time_numerator(position), musical_time_denominator(position));
    note->duration = motive_time(duration_numerator(duration), duration_denominator(duration));
    for (int pitch = 0; pitch < MIDI_PITCHES; pitch++) {
        if (pitch_identity_equals(note_event_pitch(event), bridge->table[pitch])) {
            note->midi_pitch = pitch;
            return MOTIVE_DEV_OK;
        }
    }
    return MOTIVE_DEV_ERR_RANGE;
}

/* The transformation's owned id and provenance, which its create function takes on success. */
static bool transform_parts(const char *name, EntityID **id, ProvenanceReference **provenance) {
    *id = NULL;
    *provenance = NULL;
    if (entity_id_create(name, id) != ENTITY_ID_OK) {
        return false;
    }
    if (provenance_reference_create_absent(provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(*id);
        *id = NULL;
        return false;
    }
    return true;
}

static MotiveDevResult read_result(const Bridge *bridge, const TransformResult *result, size_t expected,
                                   PlanNote *notes) {
    if (transform_result_event_count(result) != expected) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    for (size_t i = 0; i < expected; i++) {
        MotiveDevResult read = read_event(bridge, transform_result_event_at(result, i), &notes[i]);
        if (read != MOTIVE_DEV_OK) {
            return read;
        }
    }
    return MOTIVE_DEV_OK;
}

/* ----------------------------------------------------- Ursatz transforms */

static MotiveDevResult ursatz_transpose(const Bridge *bridge, int semitones, PlanNote *notes) {
    EntityID *id;
    ProvenanceReference *provenance;
    Transposition *transposition = NULL;
    if (!transform_parts("composer-develop-transpose", &id, &provenance)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    if (transposition_create(id, semitones, "real", false, false, false, provenance, &transposition) !=
        TRANSPOSITION_OK) {
        entity_id_destroy(id);
        provenance_reference_destroy(provenance);
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = MOTIVE_DEV_OK;
    for (size_t i = 0; status == MOTIVE_DEV_OK && i < bridge->count; i++) {
        TransformResult *result = NULL;
        if (transposition_apply(transposition, bridge->events[i], bridge->values[i], &result) !=
            TRANSPOSITION_APPLY_OK) {
            status = MOTIVE_DEV_ERR_ALLOC;
            break;
        }
        status = read_result(bridge, result, 1, &notes[i]);
        transform_result_destroy(result);
    }
    transposition_destroy(transposition);
    return status;
}

static MotiveDevResult ursatz_invert(const Bridge *bridge, int axis, PlanNote *notes) {
    EntityID *id;
    ProvenanceReference *provenance;
    Inversion *inversion = NULL;
    PitchNumericValue *axis_value = NULL;
    PitchIdentity *axis_identity = NULL;
    if (!transform_parts("composer-develop-invert", &id, &provenance)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    if (pitch_numeric_value_create(bridge->tuning, axis, 1, &axis_value) != PITCH_NUMERIC_VALUE_OK ||
        pitch_numeric_value_to_identity(axis_value, &axis_identity) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK ||
        inversion_create(id, axis_identity, "semitone", "none", false, provenance, &inversion) != INVERSION_OK) {
        entity_id_destroy(id);
        provenance_reference_destroy(provenance);
        pitch_identity_destroy(axis_identity);
        pitch_numeric_value_destroy(axis_value);
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = MOTIVE_DEV_OK;
    for (size_t i = 0; status == MOTIVE_DEV_OK && i < bridge->count; i++) {
        TransformResult *result = NULL;
        if (inversion_apply(inversion, bridge->events[i], bridge->values[i], axis_value, &result) !=
            INVERSION_APPLY_OK) {
            status = MOTIVE_DEV_ERR_ALLOC;
            break;
        }
        status = read_result(bridge, result, 1, &notes[i]);
        transform_result_destroy(result);
    }
    inversion_destroy(inversion);
    pitch_numeric_value_destroy(axis_value);
    return status;
}

static MotiveDevResult ursatz_retrograde(Bridge *bridge, PlanNote *notes) {
    EntityID *id;
    ProvenanceReference *provenance;
    Retrograde *retrograde = NULL;
    if (!transform_parts("composer-develop-retrograde", &id, &provenance)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    if (retrograde_create(id, RETROGRADE_DIMENSION_EVENT_ORDER, provenance, &retrograde) != RETROGRADE_OK) {
        entity_id_destroy(id);
        provenance_reference_destroy(provenance);
        return MOTIVE_DEV_ERR_ALLOC;
    }
    TransformResult *result = NULL;
    MotiveDevResult status = MOTIVE_DEV_ERR_ALLOC;
    if (retrograde_apply(retrograde, bridge->events, bridge->count, &result) == RETROGRADE_APPLY_OK) {
        status = read_result(bridge, result, bridge->count, notes);
        transform_result_destroy(result);
    }
    retrograde_destroy(retrograde);
    return status;
}

static MotiveDevResult ursatz_interval_change(Bridge *bridge, const long *adjustments, PlanNote *notes) {
    EntityID *id;
    ProvenanceReference *provenance;
    IntervallicManipulation *manipulation = NULL;
    if (!transform_parts("composer-develop-interval", &id, &provenance)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    if (intervallic_manipulation_create(id, adjustments, bridge->count - 1, provenance, &manipulation) !=
        INTERVALLIC_MANIPULATION_OK) {
        entity_id_destroy(id);
        provenance_reference_destroy(provenance);
        return MOTIVE_DEV_ERR_ALLOC;
    }
    const PitchNumericValue *values[MOTIVE_MAX_NOTES];
    for (size_t i = 0; i < bridge->count; i++) {
        values[i] = bridge->values[i];
    }
    TransformResult *result = NULL;
    MotiveDevResult status = MOTIVE_DEV_ERR_ALLOC;
    if (intervallic_manipulation_apply(manipulation, bridge->events, values, bridge->count, &result) ==
        INTERVALLIC_MANIPULATION_APPLY_OK) {
        status = read_result(bridge, result, bridge->count, notes);
        transform_result_destroy(result);
    }
    intervallic_manipulation_destroy(manipulation);
    return status;
}

/* The ids of the notes chosen, in the order of the notes; the array and the ids are the caller's. */
static EntityID **chosen_ids(const bool *chosen, size_t count, size_t *out_count) {
    EntityID **ids = calloc(count, sizeof(*ids));
    size_t made = 0;
    if (!ids) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        char text[64];
        if (!chosen[i]) {
            continue;
        }
        snprintf(text, sizeof(text), "composer-motive-note-%zu", i);
        if (entity_id_create(text, &ids[made]) != ENTITY_ID_OK) {
            for (size_t k = 0; k < made; k++) {
                entity_id_destroy(ids[k]);
            }
            free(ids);
            return NULL;
        }
        made++;
    }
    *out_count = made;
    return ids;
}

/*
 * The notes chosen (a fragment, through Ursatz's Fragmentation) or all the others (a liquidation, through its
 * Contraction), in the order they have in the motive. *count is how many there are.
 */
static MotiveDevResult ursatz_select(const Bridge *bridge, const bool *chosen, bool remove, PlanNote *notes,
                                     size_t *count) {
    EntityID *id = NULL;
    EntityID *structure = NULL;
    ProvenanceReference *provenance = NULL;
    size_t named = 0;
    EntityID **ids = chosen_ids(chosen, bridge->count, &named);
    if (!ids) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = MOTIVE_DEV_ERR_ALLOC;
    if (transform_parts(remove ? "composer-develop-contraction" : "composer-develop-fragmentation", &id,
                        &provenance) &&
        entity_id_create("composer-motive", &structure) == ENTITY_ID_OK) {
        TransformResult *result = NULL;
        NoteEvent **events = (NoteEvent **)bridge->events;
        if (remove) {
            Contraction *contraction = NULL;
            if (contraction_create(id, structure, ids, named, provenance, &contraction) == CONTRACTION_OK) {
                id = NULL;
                structure = NULL;
                provenance = NULL;
                /* The ids are the contraction's now; the array that held them is still ours. */
                free(ids);
                ids = NULL;
                if (contraction_apply(contraction, events, bridge->count, &result) == CONTRACTION_APPLY_OK) {
                    *count = transform_result_event_count(result);
                    status = *count == 0 ? MOTIVE_DEV_ERR_TOO_SHORT : read_result(bridge, result, *count, notes);
                    transform_result_destroy(result);
                }
                contraction_destroy(contraction);
            }
        } else {
            Fragmentation *fragmentation = NULL;
            if (fragmentation_create(id, structure, ids, named, provenance, &fragmentation) == FRAGMENTATION_OK) {
                id = NULL;
                structure = NULL;
                provenance = NULL;
                free(ids);
                ids = NULL;
                if (fragmentation_apply(fragmentation, events, bridge->count, &result) == FRAGMENTATION_APPLY_OK) {
                    *count = transform_result_event_count(result);
                    status = read_result(bridge, result, *count, notes);
                    transform_result_destroy(result);
                }
                fragmentation_destroy(fragmentation);
            }
        }
    }
    /* Whatever a create function did not take is still ours. */
    entity_id_destroy(id);
    entity_id_destroy(structure);
    provenance_reference_destroy(provenance);
    if (ids) {
        for (size_t i = 0; i < named; i++) {
            entity_id_destroy(ids[i]);
        }
        free(ids);
    }
    return status;
}

/* ------------------------------------------------------------------ tonal */

static MotiveDevResult tonal_pitch(const Motive *source, long position, int alteration, int *pitch) {
    int made = motive_position_pitch(&source->key, position, alteration);
    if (made < 0 || made >= MIDI_PITCHES) {
        return MOTIVE_DEV_ERR_RANGE;
    }
    *pitch = made;
    return MOTIVE_DEV_OK;
}

static MotiveDevResult tonal_transpose(const Motive *source, int steps, PlanNote *notes) {
    for (size_t i = 0; i < source->note_count; i++) {
        long position;
        int alteration;
        motive_scale_position(&source->key, source->notes[i].midi_pitch, &position, &alteration);
        notes[i] = source->notes[i];
        MotiveDevResult made = tonal_pitch(source, position + steps, alteration, &notes[i].midi_pitch);
        if (made != MOTIVE_DEV_OK) {
            return made;
        }
    }
    return MOTIVE_DEV_OK;
}

static MotiveDevResult tonal_invert(const Motive *source, int axis, PlanNote *notes) {
    long axis_position;
    int axis_alteration;
    motive_scale_position(&source->key, axis, &axis_position, &axis_alteration);
    for (size_t i = 0; i < source->note_count; i++) {
        long position;
        int alteration;
        motive_scale_position(&source->key, source->notes[i].midi_pitch, &position, &alteration);
        notes[i] = source->notes[i];
        MotiveDevResult made = tonal_pitch(source, 2 * axis_position - position, alteration, &notes[i].midi_pitch);
        if (made != MOTIVE_DEV_OK) {
            return made;
        }
    }
    return MOTIVE_DEV_OK;
}

/* How firmly a note sits in the bar: 2 on the downbeat, 1 on another beat, 0 off the beat. */
static int note_strength(const Motive *motive, size_t index) {
    PlanTime bar = motive_time(motive->meter.beats_per_bar, (unsigned long)motive->meter.beat_unit);
    PlanTime onset = motive->notes[index].offset;
    long bar_index = onset.num * (long)bar.den / ((long)onset.den * bar.num);
    PlanTime inside = motive_time(onset.num * (long)bar.den - bar_index * bar.num * (long)onset.den,
                                  onset.den * bar.den);
    if (inside.num == 0) {
        return 2;
    }
    return motive_time(inside.num * motive->meter.beat_unit, inside.den).den == 1 ? 1 : 0;
}

static size_t minimum_length(const MotiveDevSpec *spec) {
    return spec->min_length > 0 ? spec->min_length : MOTIVE_DEV_MIN_LENGTH;
}

static MotiveDevResult fragment_notes(const Motive *source, const MotiveDevSpec *spec, PlanNote *notes,
                                      size_t *count) {
    size_t total = source->note_count;
    if (spec->length < minimum_length(spec)) {
        return MOTIVE_DEV_ERR_TOO_SHORT;
    }
    if (spec->length >= total) {
        return MOTIVE_DEV_ERR_NOT_APPLICABLE;
    }
    size_t first;
    switch (spec->position) {
    case PLAN_FRAGMENT_LEADING:
        first = 0;
        break;
    case PLAN_FRAGMENT_TRAILING:
        first = total - spec->length;
        break;
    case PLAN_FRAGMENT_MIDDLE:
        first = (total - spec->length) / 2;
        break;
    default:
        return MOTIVE_DEV_ERR_INVALID;
    }
    bool chosen[MOTIVE_MAX_NOTES] = {false};
    for (size_t i = first; i < first + spec->length; i++) {
        chosen[i] = true;
    }
    Bridge bridge;
    if (!bridge_open(&bridge, source)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = ursatz_select(&bridge, chosen, false, notes, count);
    bridge_close(&bridge);
    if (status == MOTIVE_DEV_OK) {
        PlanTime start = notes[0].offset;
        for (size_t i = 0; i < *count; i++) {
            notes[i].offset = motive_time_minus(notes[i].offset, start);
        }
    }
    return status;
}

static MotiveDevResult liquidate_notes(const Motive *source, const MotiveDevSpec *spec, PlanNote *notes,
                                       size_t *count) {
    size_t total = source->note_count;
    if (spec->drop == 0) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    if (spec->drop >= total || total - spec->drop < minimum_length(spec)) {
        return MOTIVE_DEV_ERR_TOO_SHORT;
    }
    /* The weakest note goes first; of two equals, the later. */
    bool dropped[MOTIVE_MAX_NOTES] = {false};
    for (size_t step = 0; step < spec->drop; step++) {
        size_t weakest = total;
        for (size_t i = 0; i < total; i++) {
            if (!dropped[i] && (weakest == total || note_strength(source, i) <= note_strength(source, weakest))) {
                weakest = i;
            }
        }
        dropped[weakest] = true;
    }
    Bridge bridge;
    if (!bridge_open(&bridge, source)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = ursatz_select(&bridge, dropped, true, notes, count);
    bridge_close(&bridge);
    if (status == MOTIVE_DEV_OK && !spec->keep_rhythm) {
        PlanTime at = motive_time(0, 1);
        for (size_t i = 0; i < *count; i++) {
            notes[i].offset = at;
            at = motive_time_plus(at, notes[i].duration);
        }
    }
    return status;
}

/* ---------------------------------------------------------------- rhythm */

/* A duration a note list can write, a plain or dotted value. */
static bool notatable(PlanTime duration) {
    char name[32];
    motive_duration_name(duration, name, sizeof(name));
    return strchr(name, '/') == NULL;
}

/* A whole number of beats of the motive's meter. */
static bool whole_beats(const Motive *motive, PlanTime time) {
    return motive_time(time.num * motive->meter.beat_unit, time.den).den == 1;
}

/*
 * Scales the durations through Ursatz's Augmentation or Diminution, which keeps each note's onset, so the onsets are
 * scaled here by the same ratio. The result is kept only if it can be written in the meter.
 */
static MotiveDevResult ursatz_scale(const Motive *source, const Bridge *bridge, bool augment, unsigned long num,
                                    unsigned long den, PlanNote *notes) {
    EntityID *id;
    ProvenanceReference *provenance;
    Augmentation *augmentation = NULL;
    Diminution *diminution = NULL;
    if (!transform_parts(augment ? "composer-develop-augment" : "composer-develop-diminish", &id, &provenance)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    bool created = augment ? augmentation_create(id, AUGMENTATION_DIMENSION_DURATIONS, num, den, provenance,
                                                 &augmentation) == AUGMENTATION_OK
                           : diminution_create(id, AUGMENTATION_DIMENSION_DURATIONS, num, den, provenance,
                                               &diminution) == DIMINUTION_OK;
    if (!created) {
        entity_id_destroy(id);
        provenance_reference_destroy(provenance);
        return MOTIVE_DEV_ERR_INVALID;
    }
    MotiveDevResult status = MOTIVE_DEV_OK;
    for (size_t i = 0; status == MOTIVE_DEV_OK && i < bridge->count; i++) {
        TransformResult *result = NULL;
        bool applied = augment ? augmentation_apply(augmentation, bridge->events[i], &result) ==
                                     AUGMENTATION_APPLY_OK
                               : diminution_apply(diminution, bridge->events[i], &result) == DIMINUTION_APPLY_OK;
        if (!applied) {
            status = MOTIVE_DEV_ERR_ALLOC;
            break;
        }
        status = read_result(bridge, result, 1, &notes[i]);
        transform_result_destroy(result);
        notes[i].offset = motive_time(source->notes[i].offset.num * (long)num,
                                      source->notes[i].offset.den * den);
    }
    augmentation_destroy(augmentation);
    diminution_destroy(diminution);
    if (status != MOTIVE_DEV_OK) {
        return status;
    }
    for (size_t i = 0; i < bridge->count; i++) {
        if (!notatable(notes[i].duration) || 32 % notes[i].offset.den != 0) {
            return MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
        }
    }
    PlanTime end = motive_note_end(&notes[bridge->count - 1]);
    if (whole_beats(source, source->length) && !whole_beats(source, end)) {
        return MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
    }
    return MOTIVE_DEV_OK;
}

static MotiveDevResult scale_notes(const Motive *source, const MotiveDevSpec *spec, PlanNote *notes) {
    bool augment = spec->kind == MOTIVE_DEV_AUGMENT;
    unsigned long num = spec->ratio_num;
    unsigned long den = spec->ratio_den;
    if (num == 0 && den == 0) {
        num = augment ? 2 : 1;
        den = augment ? 1 : 2;
    }
    if (num == 0 || den == 0 || (augment && num <= den) || (!augment && num >= den)) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    Bridge bridge;
    if (!bridge_open(&bridge, source)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = ursatz_scale(source, &bridge, augment, num, den, notes);
    bridge_close(&bridge);
    return status;
}

/* --------------------------------------------------------------- develop */

/* The notes sorted by onset, which a retrograde does not keep. */
static void sort_notes(PlanNote *notes, size_t count) {
    for (size_t i = 1; i < count; i++) {
        PlanNote held = notes[i];
        size_t at = i;
        while (at > 0 && plan_time_compare(notes[at - 1].offset, held.offset) > 0) {
            notes[at] = notes[at - 1];
            at--;
        }
        notes[at] = held;
    }
}

static int direction_of(int semitones) {
    return semitones > 0 ? 1 : (semitones < 0 ? -1 : 0);
}

/* The notes of the development; *out_count is how many (the source's, unless notes were taken out). */
static MotiveDevResult develop_notes(const Motive *source, const MotiveDevSpec *spec, PlanNote *notes,
                                     size_t *out_count) {
    size_t count = source->note_count;
    *out_count = count;
    if (spec->kind == MOTIVE_DEV_FRAGMENT) {
        return fragment_notes(source, spec, notes, out_count);
    }
    if (spec->kind == MOTIVE_DEV_LIQUIDATE) {
        return liquidate_notes(source, spec, notes, out_count);
    }
    if (spec->kind == MOTIVE_DEV_AUGMENT || spec->kind == MOTIVE_DEV_DIMINISH) {
        return scale_notes(source, spec, notes);
    }
    if ((spec->tonal && source->key_source == MOTIVE_KEY_UNSPECIFIED) &&
        (spec->kind == MOTIVE_DEV_TRANSPOSE || spec->kind == MOTIVE_DEV_INVERT)) {
        return MOTIVE_DEV_ERR_NO_KEY;
    }
    if (spec->kind == MOTIVE_DEV_EXACT) {
        memcpy(notes, source->notes, count * sizeof(PlanNote));
        return MOTIVE_DEV_OK;
    }
    if (spec->kind == MOTIVE_DEV_TRANSPOSE && spec->tonal) {
        return tonal_transpose(source, spec->amount, notes);
    }
    if (spec->kind == MOTIVE_DEV_INVERT && spec->tonal) {
        return tonal_invert(source, spec->has_axis ? spec->axis : source->notes[0].midi_pitch, notes);
    }

    Bridge bridge;
    if (!bridge_open(&bridge, source)) {
        return MOTIVE_DEV_ERR_ALLOC;
    }
    MotiveDevResult status = MOTIVE_DEV_ERR_INVALID;
    switch (spec->kind) {
    case MOTIVE_DEV_TRANSPOSE:
        status = ursatz_transpose(&bridge, spec->amount, notes);
        break;
    case MOTIVE_DEV_INVERT:
        status = ursatz_invert(&bridge, spec->has_axis ? spec->axis : source->notes[0].midi_pitch, notes);
        break;
    case MOTIVE_DEV_RETROGRADE:
        status = ursatz_retrograde(&bridge, notes);
        break;
    case MOTIVE_DEV_INTERVAL_CHANGE: {
        if (count < 2) {
            status = MOTIVE_DEV_ERR_NOT_APPLICABLE;
            break;
        }
        if (spec->interval < 0 || (size_t)spec->interval > count - 1) {
            break;
        }
        /* A repeated note has no direction to widen in, so its interval is left alone. */
        long adjustments[MOTIVE_MAX_NOTES];
        for (size_t i = 0; i + 1 < count; i++) {
            bool chosen = spec->interval == 0 || (size_t)spec->interval == i + 1;
            bool moves = source->notes[i + 1].midi_pitch != source->notes[i].midi_pitch;
            adjustments[i] = chosen && moves ? spec->amount : 0;
        }
        status = ursatz_interval_change(&bridge, adjustments, notes);
        break;
    }
    default:
        break;
    }
    bridge_close(&bridge);
    return status;
}

MotiveDevResult motive_develop(const Motive *source, const MotiveDevSpec *spec, Motive *out) {
    if (!source || !spec || !out) {
        return MOTIVE_DEV_ERR_NULL;
    }
    PlanTime length;
    if (motive_check_notes(source, &length, NULL) != MOTIVE_OK) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    Motive result = *source;
    size_t count = source->note_count;
    MotiveDevResult status = develop_notes(source, spec, result.notes, &count);
    if (status != MOTIVE_DEV_OK) {
        return status;
    }
    result.note_count = count;
    sort_notes(result.notes, result.note_count);
    if (motive_check_notes(&result, &result.length, NULL) != MOTIVE_OK) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    if (spec->kind == MOTIVE_DEV_INTERVAL_CHANGE && spec->keep_contour) {
        for (size_t i = 0; i + 1 < source->note_count; i++) {
            int before = direction_of(source->notes[i + 1].midi_pitch - source->notes[i].midi_pitch);
            int after = direction_of(result.notes[i + 1].midi_pitch - result.notes[i].midi_pitch);
            if (before != after) {
                return MOTIVE_DEV_ERR_NOT_APPLICABLE;
            }
        }
    }
    if (spec->kind != MOTIVE_DEV_EXACT && motive_equal(source, &result)) {
        return MOTIVE_DEV_ERR_UNCHANGED;
    }
    *out = result;
    return MOTIVE_DEV_OK;
}

/* ----------------------------------------------------------------- names */

void motive_dev_label(const MotiveDevSpec *spec, char *out, size_t size) {
    switch (spec->kind) {
    case MOTIVE_DEV_EXACT:
        snprintf(out, size, "restate exact");
        break;
    case MOTIVE_DEV_TRANSPOSE:
        snprintf(out, size, "transpose %+d %s", spec->amount, spec->tonal ? "steps (tonal)" : "semitones");
        break;
    case MOTIVE_DEV_INVERT:
        if (spec->has_axis) {
            snprintf(out, size, "invert about %d%s", spec->axis, spec->tonal ? " (tonal)" : "");
        } else {
            snprintf(out, size, "invert about the first note%s", spec->tonal ? " (tonal)" : "");
        }
        break;
    case MOTIVE_DEV_RETROGRADE:
        snprintf(out, size, "retrograde");
        break;
    case MOTIVE_DEV_FRAGMENT: {
        static const char *const WHERE[] = {"any", "leading", "trailing", "middle"};
        snprintf(out, size, "fragment %s %zu notes", WHERE[spec->position], spec->length);
        break;
    }
    case MOTIVE_DEV_AUGMENT:
    case MOTIVE_DEV_DIMINISH: {
        bool augment = spec->kind == MOTIVE_DEV_AUGMENT;
        unsigned long num = spec->ratio_num != 0 ? spec->ratio_num : (augment ? 2 : 1);
        unsigned long den = spec->ratio_den != 0 ? spec->ratio_den : (augment ? 1 : 2);
        snprintf(out, size, "%s %lu:%lu", augment ? "augment" : "diminish", num, den);
        break;
    }
    case MOTIVE_DEV_LIQUIDATE:
        snprintf(out, size, "liquidate drop %zu%s", spec->drop, spec->keep_rhythm ? " (keep rhythm)" : "");
        break;
    case MOTIVE_DEV_INTERVAL_CHANGE:
        if (spec->interval == 0) {
            snprintf(out, size, "interval change %+d on every interval%s", spec->amount,
                     spec->keep_contour ? " (keep contour)" : "");
        } else {
            snprintf(out, size, "interval change %+d on interval %d%s", spec->amount, spec->interval,
                     spec->keep_contour ? " (keep contour)" : "");
        }
        break;
    }
}

static const char *const RESULT_NAMES[] = {"ok",        "null",      "alloc",          "no key",
                                           "range",     "unchanged", "not applicable", "too short",
                                           "not expressible", "invalid"};

const char *motive_dev_result_name(MotiveDevResult result) {
    return (size_t)result < sizeof(RESULT_NAMES) / sizeof(RESULT_NAMES[0]) ? RESULT_NAMES[result] : "unknown";
}
