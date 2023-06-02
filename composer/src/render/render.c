#include <ursatz_composer/render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

#include <ursatz_composer/plan_state.h>

static const char *const LANE_NAMES[] = {"meter", "key", "harmony", "bass", "melody", "inner_voice"};

const char *rendered_part_lane_name(const RenderedPart *part) {
    return LANE_NAMES[part->lane];
}

void rendered_plan_destroy(RenderedPlan *rendered) {
    if (!rendered) {
        return;
    }
    for (size_t i = 0; i < rendered->count; i++) {
        note_event_destroy(rendered->events[i]);
        pitch_numeric_value_destroy(rendered->pitch_values[i]);
    }
    free(rendered->events);
    free(rendered->pitch_values);
    free(rendered->part_index);
    free(rendered->parts);
    meter_map_destroy(rendered->meter_map);
    for (size_t i = 0; i < rendered->key_count; i++) {
        musical_time_destroy(rendered->keys[i].position);
    }
    free(rendered->keys);
    free(rendered->skipped);
    memset(rendered, 0, sizeof(*rendered));
}

/* ----------------------------------------------------------------- keys */

/* The signature of each major key by tonic pitch class; ties between sharps and flats (F# or Gb) go to sharps. */
static const int MAJOR_SHARPS_FLATS[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};

static int key_sharps_flats(const PlanKey *key) {
    int tonic = key->mode == PLAN_MODE_MINOR ? (key->tonic_pitch_class + 3) % 12 : key->tonic_pitch_class;
    return MAJOR_SHARPS_FLATS[tonic];
}

typedef struct {
    LaneSpanId span;
    PlanSpan plan_span;
    PlanKey key;
} KeySource;

static bool starts_before(PlanSpan a, PlanSpan b) {
    return plan_time_compare(a.start, b.start) < 0;
}

/* The sharps_flats of the last key that starts at or before onset, or 0 when none does. */
static int key_in_force(const KeySource *keys, size_t count, PlanTime onset) {
    int sharps_flats = 0;
    for (size_t i = 0; i < count; i++) {
        if (plan_time_compare(keys[i].plan_span.start, onset) <= 0) {
            sharps_flats = key_sharps_flats(&keys[i].key);
        }
    }
    return sharps_flats;
}

/* --------------------------------------------------------------- spelling */

typedef struct {
    PitchLetter letter;
    int accidental;
} Spelling;

static Spelling spell(int midi_pitch, int sharps_flats) {
    static const Spelling SHARPS[12] = {
        {PITCH_LETTER_C, 0}, {PITCH_LETTER_C, 1}, {PITCH_LETTER_D, 0}, {PITCH_LETTER_D, 1},
        {PITCH_LETTER_E, 0}, {PITCH_LETTER_F, 0}, {PITCH_LETTER_F, 1}, {PITCH_LETTER_G, 0},
        {PITCH_LETTER_G, 1}, {PITCH_LETTER_A, 0}, {PITCH_LETTER_A, 1}, {PITCH_LETTER_B, 0},
    };
    static const Spelling FLATS[12] = {
        {PITCH_LETTER_C, 0}, {PITCH_LETTER_D, -1}, {PITCH_LETTER_D, 0}, {PITCH_LETTER_E, -1},
        {PITCH_LETTER_E, 0}, {PITCH_LETTER_F, 0},  {PITCH_LETTER_G, -1}, {PITCH_LETTER_G, 0},
        {PITCH_LETTER_A, -1}, {PITCH_LETTER_A, 0}, {PITCH_LETTER_B, -1}, {PITCH_LETTER_B, 0},
    };
    return (sharps_flats < 0 ? FLATS : SHARPS)[midi_pitch % 12];
}

/* ------------------------------------------------------------------ notes */

typedef struct {
    PlanTime onset;
    PlanTime duration;
    int midi_pitch;
    size_t part;
    LaneSpanId span;
    size_t index;
} NoteSource;

static int compare_notes(const void *left, const void *right) {
    const NoteSource *a = left;
    const NoteSource *b = right;
    int by_time = plan_time_compare(a->onset, b->onset);
    if (by_time != 0) {
        return by_time;
    }
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->span != b->span) {
        return a->span < b->span ? -1 : 1;
    }
    return (a->index > b->index) - (a->index < b->index);
}

static int lane_rank(PlanLaneKind lane) {
    return lane == PLAN_LANE_MELODY ? 0 : lane == PLAN_LANE_BASS ? 1 : 2;
}

static int compare_parts(const void *left, const void *right) {
    const RenderedPart *a = left;
    const RenderedPart *b = right;
    if (lane_rank(a->lane) != lane_rank(b->lane)) {
        return lane_rank(a->lane) - lane_rank(b->lane);
    }
    return (a->voice > b->voice) - (a->voice < b->voice);
}

static bool is_note_lane(PlanLaneKind lane) {
    return lane == PLAN_LANE_BASS || lane == PLAN_LANE_MELODY || lane == PLAN_LANE_INNER_VOICE;
}

static size_t find_part(const RenderedPart *parts, size_t count, PlanLaneKind lane, int voice) {
    for (size_t i = 0; i < count; i++) {
        if (parts[i].lane == lane && parts[i].voice == voice) {
            return i;
        }
    }
    return count;
}

/* Builds one NoteEvent and its pitch value. Everything is freed again on failure. */
static PlanRenderResult build_note(const NoteSource *note, const RenderedPart *part, int sharps_flats,
                                   const TuningSystem *tuning, NoteEvent **out_event,
                                   PitchNumericValue **out_value) {
    EntityID *id = NULL;
    MusicalTime *position = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *duration = NULL;
    PitchNumericValue *value = NULL;
    PitchIdentity *identity = NULL;
    PitchSpelling *spelling = NULL;
    EntityID *voice_id = NULL;
    VoiceReference *voice = NULL;
    ProvenanceReference *provenance = NULL;
    char text[96];

    snprintf(text, sizeof(text), "composer-span-%zu-note-%zu", note->span, note->index);
    Spelling written = spell(note->midi_pitch, sharps_flats);
    bool ok = entity_id_create(text, &id) == ENTITY_ID_OK &&
              musical_time_create(note->onset.num, note->onset.den, &position) == MUSICAL_TIME_OK &&
              temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK;
    if (ok) {
        position = NULL;
    }
    ok = ok && duration_create(note->duration.num, note->duration.den, &duration) == DURATION_OK &&
         pitch_numeric_value_create(tuning, note->midi_pitch, 1, &value) == PITCH_NUMERIC_VALUE_OK &&
         pitch_numeric_value_to_identity(value, &identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
         pitch_spelling_create(written.letter, written.accidental, note->midi_pitch / 12 - 1, &spelling) ==
             PITCH_SPELLING_OK;
    snprintf(text, sizeof(text), "composer-voice-%s-%d", rendered_part_lane_name(part), part->voice);
    ok = ok && entity_id_create(text, &voice_id) == ENTITY_ID_OK &&
         voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK;
    if (ok) {
        voice_id = NULL;
    }
    ok = ok && provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;

    NoteEvent *event = NULL;
    if (ok && note_event_create(id, anchor, duration, identity, spelling, voice, provenance, &event) ==
                  NOTE_EVENT_OK) {
        *out_event = event;
        *out_value = value;
        return PLAN_RENDER_OK;
    }
    entity_id_destroy(id);
    musical_time_destroy(position);
    temporal_anchor_destroy(anchor);
    duration_destroy(duration);
    pitch_numeric_value_destroy(value);
    pitch_identity_destroy(identity);
    pitch_spelling_destroy(spelling);
    entity_id_destroy(voice_id);
    voice_reference_destroy(voice);
    provenance_reference_destroy(provenance);
    return PLAN_RENDER_ERR_ALLOC;
}

/* ------------------------------------------------------------------ meter */

static PlanRenderResult build_meter_map(const LaneSpanId *meter_spans, size_t count, const PiecePlan *plan,
                                        MeterMap **out) {
    *out = NULL;
    if (count == 0) {
        return PLAN_RENDER_OK;
    }
    TimeSignature **signatures = calloc(count, sizeof(TimeSignature *));
    MusicalTime **starts = calloc(count, sizeof(MusicalTime *));
    MeterMapChangePoint *points = calloc(count, sizeof(MeterMapChangePoint));
    PlanRenderResult result = PLAN_RENDER_OK;
    if (!signatures || !starts || !points) {
        result = PLAN_RENDER_ERR_ALLOC;
    }
    for (size_t i = 0; result == PLAN_RENDER_OK && i < count; i++) {
        const PlanMeter *meter = &plan_lane_span_payload(plan, meter_spans[i])->meter;
        PlanSpan span = plan_lane_span_span(plan, meter_spans[i]);
        int exponent = -1;
        for (int candidate = TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT;
             candidate <= TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT; candidate++) {
            if (meter->beat_unit == (1 << candidate)) {
                exponent = candidate;
            }
        }
        NoteValue *unit = NULL;
        if (exponent < 0 || meter->beats_per_bar > TIME_SIGNATURE_MAX_TOP_NUMBER) {
            result = PLAN_RENDER_ERR_NOT_REPRESENTABLE;
        } else if (note_value_create(exponent, &unit) != NOTE_VALUE_OK) {
            result = PLAN_RENDER_ERR_ALLOC;
        } else {
            TimeSignatureResult made = time_signature_create((unsigned)meter->beats_per_bar, unit, &signatures[i]);
            note_value_destroy(unit);
            if (made != TIME_SIGNATURE_OK) {
                result = PLAN_RENDER_ERR_NOT_REPRESENTABLE;
            } else if (musical_time_create(span.start.num, span.start.den, &starts[i]) != MUSICAL_TIME_OK) {
                result = PLAN_RENDER_ERR_ALLOC;
            }
        }
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    if (result == PLAN_RENDER_OK) {
        EntityID *id = NULL;
        if (entity_id_create("composer-meter-map", &id) != ENTITY_ID_OK) {
            result = PLAN_RENDER_ERR_ALLOC;
        } else if (meter_map_create(id, points, count, out) != METER_MAP_OK) {
            entity_id_destroy(id);
            result = PLAN_RENDER_ERR_NOT_REPRESENTABLE;
        }
    }
    for (size_t i = 0; signatures && starts && i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    free(signatures);
    free(starts);
    free(points);
    return result;
}

/* ---------------------------------------------------------------- the plan */

static bool add_skip(RenderSkip **skipped, size_t *count, size_t *capacity, LaneSpanId span, PlanLaneKind lane,
                     RenderSkipReason reason) {
    if (*count == *capacity) {
        size_t grown = *capacity ? *capacity * 2 : 8;
        RenderSkip *items = realloc(*skipped, grown * sizeof(RenderSkip));
        if (!items) {
            return false;
        }
        *skipped = items;
        *capacity = grown;
    }
    (*skipped)[*count].span = span;
    (*skipped)[*count].lane = lane;
    (*skipped)[*count].reason = reason;
    (*count)++;
    return true;
}

/* Sorts span ids by the start of their spans. */
static void sort_by_start(const PiecePlan *plan, LaneSpanId *ids, size_t count) {
    for (size_t i = 1; i < count; i++) {
        LaneSpanId moving = ids[i];
        size_t position = i;
        while (position > 0 && starts_before(plan_lane_span_span(plan, moving),
                                             plan_lane_span_span(plan, ids[position - 1]))) {
            ids[position] = ids[position - 1];
            position--;
        }
        ids[position] = moving;
    }
}

static PlanRenderResult render_into(const PiecePlan *plan, RenderedPlan *out) {
    size_t span_count = plan_lane_span_count(plan);
    size_t skipped_capacity = 0;
    LaneSpanId *meter_spans = calloc(span_count + 1, sizeof(LaneSpanId));
    LaneSpanId *key_spans = calloc(span_count + 1, sizeof(LaneSpanId));
    LaneSpanId *note_spans = calloc(span_count + 1, sizeof(LaneSpanId));
    KeySource *keys = calloc(span_count + 1, sizeof(KeySource));
    NoteSource *notes = NULL;
    TuningSystem *tuning = NULL;
    PlanRenderResult result = PLAN_RENDER_OK;
    size_t meter_count = 0;
    size_t key_count = 0;
    size_t note_span_count = 0;
    size_t note_count = 0;

    if (!meter_spans || !key_spans || !note_spans || !keys) {
        result = PLAN_RENDER_ERR_ALLOC;
    }

    /* Decide what is rendered and what is skipped. */
    for (LaneSpanId id = 1; result == PLAN_RENDER_OK && id <= span_count; id++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, id);
        if (lane == PLAN_LANE_HARMONY) {
            if (!add_skip(&out->skipped, &out->skipped_count, &skipped_capacity, id, lane,
                          RENDER_SKIP_LANE_NOT_RENDERED)) {
                result = PLAN_RENDER_ERR_ALLOC;
            }
        } else if (plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED) {
            if (!add_skip(&out->skipped, &out->skipped_count, &skipped_capacity, id, lane,
                          RENDER_SKIP_NOT_REALIZED)) {
                result = PLAN_RENDER_ERR_ALLOC;
            }
        } else if (lane == PLAN_LANE_METER) {
            meter_spans[meter_count++] = id;
        } else if (lane == PLAN_LANE_KEY) {
            key_spans[key_count++] = id;
        } else if (is_note_lane(lane)) {
            note_spans[note_span_count++] = id;
            note_count += plan_lane_span_payload(plan, id)->notes.note_count;
        }
    }

    /* Keys. */
    if (result == PLAN_RENDER_OK) {
        sort_by_start(plan, key_spans, key_count);
        sort_by_start(plan, meter_spans, meter_count);
        out->keys = key_count ? calloc(key_count, sizeof(RenderedKey)) : NULL;
        if (key_count && !out->keys) {
            result = PLAN_RENDER_ERR_ALLOC;
        }
    }
    for (size_t i = 0; result == PLAN_RENDER_OK && i < key_count; i++) {
        PlanSpan span = plan_lane_span_span(plan, key_spans[i]);
        const PlanKey *key = &plan_lane_span_payload(plan, key_spans[i])->key;
        keys[i].span = key_spans[i];
        keys[i].plan_span = span;
        keys[i].key = *key;
        out->keys[i].sharps_flats = key_sharps_flats(key);
        out->keys[i].mode = key->mode;
        if (musical_time_create(span.start.num, span.start.den, &out->keys[i].position) != MUSICAL_TIME_OK) {
            result = PLAN_RENDER_ERR_ALLOC;
        } else {
            out->key_count++;
        }
    }

    /* Meter. */
    if (result == PLAN_RENDER_OK) {
        result = build_meter_map(meter_spans, meter_count, plan, &out->meter_map);
    }

    /* Parts, then notes. */
    if (result == PLAN_RENDER_OK && note_count > 0) {
        notes = calloc(note_count, sizeof(NoteSource));
        out->parts = calloc(note_span_count, sizeof(RenderedPart));
        if (!notes || !out->parts) {
            result = PLAN_RENDER_ERR_ALLOC;
        }
    }
    for (size_t i = 0; result == PLAN_RENDER_OK && note_count > 0 && i < note_span_count; i++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, note_spans[i]);
        int voice = plan_lane_span_voice(plan, note_spans[i]);
        if (find_part(out->parts, out->part_count, lane, voice) == out->part_count) {
            out->parts[out->part_count].lane = lane;
            out->parts[out->part_count].voice = voice;
            out->part_count++;
        }
    }
    if (result == PLAN_RENDER_OK && out->part_count > 1) {
        qsort(out->parts, out->part_count, sizeof(RenderedPart), compare_parts);
    }
    size_t filled = 0;
    for (size_t i = 0; result == PLAN_RENDER_OK && note_count > 0 && i < note_span_count; i++) {
        LaneSpanId id = note_spans[i];
        PlanSpan span = plan_lane_span_span(plan, id);
        const PlanNotes *span_notes = &plan_lane_span_payload(plan, id)->notes;
        size_t part = find_part(out->parts, out->part_count, plan_lane_span_lane(plan, id),
                                plan_lane_span_voice(plan, id));
        for (size_t n = 0; result == PLAN_RENDER_OK && n < span_notes->note_count; n++) {
            const PlanNote *note = &span_notes->notes[n];
            NoteSource *source = &notes[filled++];
            if (plan_time_add(span.start, note->offset, &source->onset) != PLAN_OK) {
                result = PLAN_RENDER_ERR_NOT_REPRESENTABLE;
            }
            source->duration = note->duration;
            source->midi_pitch = note->midi_pitch;
            source->part = part;
            source->span = id;
            source->index = n;
        }
    }
    if (result == PLAN_RENDER_OK && note_count > 1) {
        qsort(notes, note_count, sizeof(NoteSource), compare_notes);
    }

    /* NoteEvents. */
    if (result == PLAN_RENDER_OK && note_count > 0) {
        EntityID *tuning_id = NULL;
        if (entity_id_create("midi-note-number", &tuning_id) != ENTITY_ID_OK) {
            result = PLAN_RENDER_ERR_ALLOC;
        } else if (tuning_system_create(tuning_id, "MIDI note number", &tuning) != TUNING_SYSTEM_OK) {
            entity_id_destroy(tuning_id);
            result = PLAN_RENDER_ERR_ALLOC;
        }
        out->events = calloc(note_count, sizeof(NoteEvent *));
        out->pitch_values = calloc(note_count, sizeof(PitchNumericValue *));
        out->part_index = calloc(note_count, sizeof(size_t));
        if (!out->events || !out->pitch_values || !out->part_index) {
            result = PLAN_RENDER_ERR_ALLOC;
        }
    }
    for (size_t i = 0; result == PLAN_RENDER_OK && i < note_count; i++) {
        int sharps_flats = key_in_force(keys, key_count, notes[i].onset);
        result = build_note(&notes[i], &out->parts[notes[i].part], sharps_flats, tuning, &out->events[i],
                            &out->pitch_values[i]);
        if (result == PLAN_RENDER_OK) {
            out->part_index[i] = notes[i].part;
            out->count++;
        }
    }

    tuning_system_destroy(tuning);
    free(notes);
    free(keys);
    free(note_spans);
    free(key_spans);
    free(meter_spans);
    return result;
}

PlanRenderResult plan_render(const PiecePlan *plan, RenderedPlan *out) {
    if (!out) {
        return PLAN_RENDER_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return PLAN_RENDER_ERR_NULL;
    }
    PlanRenderResult result = render_into(plan, out);
    if (result != PLAN_RENDER_OK) {
        rendered_plan_destroy(out);
    }
    return result;
}
