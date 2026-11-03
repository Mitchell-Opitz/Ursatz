#include <ursatz_composer/render.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <core/provenance_reference.h>
#include <core/voice_reference.h>
#include <identity/entity_id.h>
#include <pitch/key_signature.h>
#include <pitch/pitch_arithmetic.h>
#include <pitch/pitch_identity.h>
#include <pitch/pitch_spelling.h>
#include <pitch/tuning_system.h>
#include <standard-tonality/tonal_key.h>
#include <time/duration.h>
#include <time/time_rational.h>
#include <time/note_value.h>
#include <time/temporal_anchor.h>
#include <time/time_signature.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/plan_state.h>
#include "util/plan_util.h"

const char *rendered_part_lane_name(const RenderedPart *part) {
    return plan_lane_name(part->lane);
}

void rendered_part_name(const RenderedPart *part, char *out, size_t size) {
    if (part->lane == PLAN_LANE_INNER_VOICE) {
        snprintf(out, size, "%s_%d", rendered_part_lane_name(part), part->voice);
    } else {
        snprintf(out, size, "%s", rendered_part_lane_name(part));
    }
}

void rendered_part_voice_id(const RenderedPart *part, char *out, size_t size) {
    snprintf(out, size, "composer-voice-%s-%d", rendered_part_lane_name(part), part->voice);
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
    free(rendered->spans);
    free(rendered->parts);
    meter_map_destroy(rendered->meter_map);
    for (size_t i = 0; i < rendered->key_count; i++) {
        musical_time_destroy(rendered->keys[i].position);
    }
    free(rendered->keys);
    free(rendered->skipped);
    for (size_t i = 0; i < rendered->section_count; i++) {
        free(rendered->sections[i].label);
        musical_time_destroy(rendered->sections[i].position);
    }
    free(rendered->sections);
    free(rendered->form_name);
    memset(rendered, 0, sizeof(*rendered));
}

/* ----------------------------------------------------------------- material */

static bool section_is_material(const PiecePlan *plan, NodeId id) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        if (plan_node_constraint_at(plan, id, i)->kind == PLAN_CONSTRAINT_MATERIAL) {
            return true;
        }
    }
    return false;
}

/* Whether the section is the seam: the copy of the body's first bar after the loop's end, which is not part of the piece. */
static bool section_is_seam(const PiecePlan *plan, NodeId id) {
    return plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM);
}

/* True for a node that is, or sits under, the seam section. */
static bool node_is_seam(const PiecePlan *plan, NodeId id) {
    for (; id != PLAN_NODE_NONE; id = plan_node_parent(plan, id)) {
        if (plan_node_kind(plan, id) == PLAN_NODE_SECTION && section_is_seam(plan, id)) {
            return true;
        }
    }
    return false;
}

/* True for a node that is, or sits under, a section marked MATERIAL. */
static bool node_is_material(const PiecePlan *plan, NodeId id) {
    for (; id != PLAN_NODE_NONE; id = plan_node_parent(plan, id)) {
        if (plan_node_kind(plan, id) == PLAN_NODE_SECTION && section_is_material(plan, id)) {
            return true;
        }
    }
    return false;
}

/*
 * Whether the lane sounds where the node is: false only when the section the node is in carries a LAYERS constraint
 * whose mask leaves the lane out (a layer of a layered form). A section with no LAYERS sounds every lane.
 */
static bool lane_sounds_in(const PiecePlan *plan, NodeId id, PlanLaneKind lane) {
    for (; id != PLAN_NODE_NONE; id = plan_node_parent(plan, id)) {
        if (plan_node_kind(plan, id) != PLAN_NODE_SECTION) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            if (constraint->kind == PLAN_CONSTRAINT_LAYERS) {
                return (constraint->params.layers.lanes & PLAN_LAYER_BIT(lane)) != 0;
            }
        }
        return true;
    }
    return true;
}

/* The end of the last section that is not material or the seam; zero when the form has no such section. */
static Rational piece_end_of(const PiecePlan *plan) {
    Rational end = {0, 1};
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId section = plan_node_child_at(plan, root, i);
        Rational section_end;
        if (plan_node_kind(plan, section) == PLAN_NODE_SECTION && !section_is_material(plan, section) &&
            !section_is_seam(plan, section) && plan_span_end(plan_node_span(plan, section), &section_end) == PLAN_OK &&
            rational_cmp(section_end, end) > 0) {
            end = section_end;
        }
    }
    return end;
}

/* ----------------------------------------------------------------- range */

/* A time measured from the start of the range, which is the origin of a range render; unchanged without a range. */
static Rational from_range_start(Rational time, const PlanSpan *range) {
    Rational shifted = time;
    Rational back = {range ? -range->start.num : 0, range ? range->start.den : 1};
    if (range && rational_add(time, back, &shifted) != RATIONAL_OK) {
        return time;
    }
    return shifted;
}

/* A key or meter span's start in a range render: the later of its own and the range's, from the range's start. */
static Rational span_start_in_range(PlanSpan span, const PlanSpan *range) {
    if (range && rational_cmp(span.start, range->start) < 0) {
        return from_range_start(range->start, range);
    }
    return from_range_start(span.start, range);
}

static Rational range_end(const PlanSpan *range) {
    Rational end = range->start;
    plan_span_end(*range, &end);
    return end;
}

/* Whether a span of a lane belongs to a range render: a key or meter span that overlaps it, any other that starts in it. */
static bool in_range(PlanSpan span, PlanLaneKind lane, const PlanSpan *range) {
    if (!range) {
        return true;
    }
    Rational end;
    if (lane == PLAN_LANE_KEY || lane == PLAN_LANE_METER) {
        return plan_span_end(span, &end) == PLAN_OK && rational_cmp(end, range->start) > 0 &&
               rational_cmp(span.start, range_end(range)) < 0;
    }
    return rational_cmp(span.start, range->start) >= 0 && rational_cmp(span.start, range_end(range)) < 0;
}

/* ----------------------------------------------------------------- keys */

/* The tonic of a key as the library spells it: the fewest accidentals in the signature, sharps on a tie. */
static bool key_tonic(const AnalysisKey *key, PitchLetter *letter, int *accidental) {
    return standard_tonality_key_tonic_of_pitch_class(analysis_key_tonic_pitch_class(key), key->mode == KEY_STRUCTURE_MODE_MINOR, letter,
                                                      accidental);
}

/* The signature of a key as sharps (positive) or flats (negative); 0 for a tonic the library cannot spell. */
static int key_sharps_flats(const AnalysisKey *key) {
    PitchLetter letter;
    int accidental;
    int fifths = 0;
    if (!key_tonic(key, &letter, &accidental) ||
        !standard_tonality_key_fifths(letter, accidental, key->mode == KEY_STRUCTURE_MODE_MINOR, &fifths)) {
        return 0;
    }
    return fifths;
}

typedef struct {
    LaneSpanId span;
    PlanSpan plan_span;
    AnalysisKey key;
} KeySource;

static bool starts_before(PlanSpan a, PlanSpan b) {
    return rational_cmp(a.start, b.start) < 0;
}

/* The last key that starts at or before onset, or NULL when none does. */
static const AnalysisKey *key_in_force(const KeySource *keys, size_t count, Rational onset) {
    const AnalysisKey *in_force = NULL;
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(keys[i].plan_span.start, onset) <= 0) {
            in_force = &keys[i].key;
        }
    }
    return in_force;
}

/* --------------------------------------------------------------- spelling */

typedef struct {
    PitchLetter letter;
    int accidental;
} Spelling;

/* The line of fifths a chromatic note is spelled on: sharps in a sharp key or none, flats in a flat key. */
#define SHARPS_LOWEST_FIFTHS (-1)
#define FLATS_LOWEST_FIFTHS (-6)

/*
 * A pitch as written in a key (none: C major). A note of the key's scale takes its signature spelling; any other is
 * placed on the line of fifths, its sharp spelling in a sharp key or none, its flat spelling in a flat key.
 */
static Spelling spell(int midi_pitch, const AnalysisKey *key) {
    int pitch_class = ((midi_pitch % PITCH_SEMITONES_PER_OCTAVE) + PITCH_SEMITONES_PER_OCTAVE) % PITCH_SEMITONES_PER_OCTAVE;
    int sharps_flats = key ? key_sharps_flats(key) : 0;
    PitchLetter tonic_letter = PITCH_LETTER_C;
    int tonic_accidental = 0;
    KeySignature *signature = NULL;
    if (key && key_tonic(key, &tonic_letter, &tonic_accidental) &&
        standard_tonality_key_signature_of(tonic_letter, tonic_accidental, key->mode == KEY_STRUCTURE_MODE_MINOR, &signature)) {
        for (int step = 0; step < 7; step++) {
            PitchLetter letter = pitch_letter_offset(PITCH_LETTER_C, step);
            int accidental = 0;
            for (size_t i = 0; i < key_signature_entry_count(signature); i++) {
                if (key_signature_letter_at(signature, i) == letter) {
                    accidental = key_signature_accidental_at(signature, i);
                }
            }
            if (pitch_class_of_letter(letter, accidental) == pitch_class) {
                key_signature_destroy(signature);
                Spelling written = {letter, accidental};
                return written;
            }
        }
        key_signature_destroy(signature);
    }
    int lowest = sharps_flats < 0 ? FLATS_LOWEST_FIFTHS : SHARPS_LOWEST_FIFTHS;
    int fifths = lowest;
    while (((fifths * 7) % PITCH_SEMITONES_PER_OCTAVE + PITCH_SEMITONES_PER_OCTAVE) % PITCH_SEMITONES_PER_OCTAVE !=
           pitch_class) {
        fifths++;
    }
    Spelling written;
    pitch_spelling_from_fifths(fifths, &written.letter, &written.accidental);
    return written;
}

/* The octave that makes the spelling sound at midi_pitch (octave 4 C is 60), as pitch_spelling_midi_number counts. */
static int spelling_octave(int midi_pitch, Spelling written) {
    int semitones = pitch_letter_semitone(written.letter) + written.accidental;
    return (midi_pitch - semitones) / PITCH_SEMITONES_PER_OCTAVE - 1;
}

/* ------------------------------------------------------------------ notes */

typedef struct {
    Rational onset;
    Rational duration;
    int midi_pitch;
    size_t part;
    LaneSpanId span;
    size_t index;
} NoteSource;

static int compare_notes(const void *left, const void *right) {
    const NoteSource *a = left;
    const NoteSource *b = right;
    int by_time = rational_cmp(a->onset, b->onset);
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

static size_t find_part(const RenderedPart *parts, size_t count, PlanLaneKind lane, int voice) {
    for (size_t i = 0; i < count; i++) {
        if (parts[i].lane == lane && parts[i].voice == voice) {
            return i;
        }
    }
    return count;
}

/* Builds one NoteEvent and its pitch value. Everything is freed again on failure. */
static PlanRenderResult build_note(const NoteSource *note, const RenderedPart *part, const AnalysisKey *key,
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
    Spelling written = spell(note->midi_pitch, key);
    bool ok = entity_id_create(text, &id) == ENTITY_ID_OK &&
              musical_time_from_rational(note->onset, &position) == TIME_RATIONAL_OK &&
              temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK;
    if (ok) {
        position = NULL;
    }
    ok = ok && duration_create(note->duration.num, note->duration.den, &duration) == DURATION_OK &&
         pitch_numeric_value_create(tuning, note->midi_pitch, 1, &value) == PITCH_NUMERIC_VALUE_OK &&
         pitch_numeric_value_to_identity(value, &identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
         pitch_spelling_create(written.letter, written.accidental, spelling_octave(note->midi_pitch, written), &spelling) ==
             PITCH_SPELLING_OK;
    rendered_part_voice_id(part, text, sizeof(text));
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
                                        const PlanSpan *range, MeterMap **out) {
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
        Rational start = span_start_in_range(span, range);
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
            } else if (musical_time_from_rational(start, &starts[i]) != TIME_RATIONAL_OK) {
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

static PlanRenderResult render_into(const PiecePlan *plan, const PlanSpan *range, RenderedPlan *out) {
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
        if (!plan_has_lane_span(plan, id)) {
            continue;
        }
        PlanLaneKind lane = plan_lane_span_lane(plan, id);
        if (!in_range(plan_lane_span_span(plan, id), lane, range)) {
            continue;
        }
        if (node_is_material(plan, plan_lane_span_owner(plan, id))) {
            if (!add_skip(&out->skipped, &out->skipped_count, &skipped_capacity, id, lane, RENDER_SKIP_MATERIAL)) {
                result = PLAN_RENDER_ERR_ALLOC;
            }
        } else if (node_is_seam(plan, plan_lane_span_owner(plan, id))) {
            if (!add_skip(&out->skipped, &out->skipped_count, &skipped_capacity, id, lane, RENDER_SKIP_SEAM)) {
                result = PLAN_RENDER_ERR_ALLOC;
            }
        } else if (lane == PLAN_LANE_HARMONY) {
            if (!add_skip(&out->skipped, &out->skipped_count, &skipped_capacity, id, lane,
                          RENDER_SKIP_LANE_NOT_RENDERED)) {
                result = PLAN_RENDER_ERR_ALLOC;
            }
        } else if (!lane_sounds_in(plan, plan_lane_span_owner(plan, id), lane)) {
            if (!add_skip(&out->skipped, &out->skipped_count, &skipped_capacity, id, lane, RENDER_SKIP_LAYER)) {
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
        } else if (plan_is_note_lane(lane)) {
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
        Rational key_start = span_start_in_range(span, range);
        const AnalysisKey *key = &plan_lane_span_payload(plan, key_spans[i])->key;
        keys[i].span = key_spans[i];
        keys[i].plan_span = span;
        keys[i].key = *key;
        out->keys[i].sharps_flats = key_sharps_flats(key);
        out->keys[i].mode = key->mode;
        if (musical_time_from_rational(key_start, &out->keys[i].position) != TIME_RATIONAL_OK) {
            result = PLAN_RENDER_ERR_ALLOC;
        } else {
            out->key_count++;
        }
    }

    /* Meter. */
    if (result == PLAN_RENDER_OK) {
        result = build_meter_map(meter_spans, meter_count, plan, range, &out->meter_map);
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
            if (rational_add(span.start, note->offset, &source->onset) != RATIONAL_OK) {
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
        out->spans = calloc(note_count, sizeof(LaneSpanId));
        if (!out->events || !out->pitch_values || !out->part_index || !out->spans) {
            result = PLAN_RENDER_ERR_ALLOC;
        }
    }
    for (size_t i = 0; result == PLAN_RENDER_OK && i < note_count; i++) {
        const AnalysisKey *key = key_in_force(keys, key_count, notes[i].onset);
        NoteSource placed = notes[i];
        placed.onset = from_range_start(placed.onset, range);
        result = build_note(&placed, &out->parts[notes[i].part], key, tuning, &out->events[i],
                            &out->pitch_values[i]);
        if (result == PLAN_RENDER_OK) {
            out->part_index[i] = notes[i].part;
            out->spans[i] = notes[i].span;
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

/* ----------------------------------------------------------------- sections */

static const char *section_label(const PiecePlan *plan, NodeId id) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == PLAN_CONSTRAINT_SECTION_LETTER && constraint->params.letter[0]) {
            return constraint->params.letter;
        }
    }
    return plan_node_name(plan, id);
}

/* The sections that are not material and start inside the range (all of them without one), earliest first. */
static PlanRenderResult render_sections(const PiecePlan *plan, const PlanSpan *range, RenderedPlan *out) {
    NodeId root = plan_root(plan);
    size_t child_count = root == PLAN_NODE_NONE ? 0 : plan_node_child_count(plan, root);
    if (child_count == 0) {
        return PLAN_RENDER_OK;
    }
    out->sections = calloc(child_count, sizeof(RenderedSection));
    if (!out->sections) {
        return PLAN_RENDER_ERR_ALLOC;
    }
    Rational *starts = calloc(child_count, sizeof(Rational));
    if (!starts) {
        return PLAN_RENDER_ERR_ALLOC;
    }
    PlanRenderResult result = PLAN_RENDER_OK;
    for (size_t i = 0; result == PLAN_RENDER_OK && i < child_count; i++) {
        NodeId section = plan_node_child_at(plan, root, i);
        if (plan_node_kind(plan, section) != PLAN_NODE_SECTION || section_is_material(plan, section) ||
            section_is_seam(plan, section)) {
            continue;
        }
        Rational start = plan_node_span(plan, section).start;
        if (range && (rational_cmp(start, range->start) < 0 || rational_cmp(start, range_end(range)) >= 0)) {
            continue;
        }
        const char *label = section_label(plan, section);
        size_t length = label ? strlen(label) : 0;
        if (length == 0) {
            continue;
        }
        Rational shifted = from_range_start(start, range);
        RenderedSection *entry = &out->sections[out->section_count];
        entry->label = malloc(length + 1);
        if (!entry->label) {
            result = PLAN_RENDER_ERR_ALLOC;
            break;
        }
        memcpy(entry->label, label, length + 1);
        if (musical_time_from_rational(shifted, &entry->position) != TIME_RATIONAL_OK) {
            free(entry->label);
            entry->label = NULL;
            result = PLAN_RENDER_ERR_ALLOC;
            break;
        }
        starts[out->section_count++] = shifted;
    }
    /* Insertion sort by start; the children are normally in order already. */
    for (size_t i = 1; result == PLAN_RENDER_OK && i < out->section_count; i++) {
        for (size_t j = i; j > 0 && rational_cmp(starts[j - 1], starts[j]) > 0; j--) {
            Rational time = starts[j];
            starts[j] = starts[j - 1];
            starts[j - 1] = time;
            RenderedSection section = out->sections[j];
            out->sections[j] = out->sections[j - 1];
            out->sections[j - 1] = section;
        }
    }
    free(starts);
    return result;
}

static PlanRenderResult render_checked(const PiecePlan *plan, const PlanSpan *range, RenderedPlan *out) {
    if (!out) {
        return PLAN_RENDER_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return PLAN_RENDER_ERR_NULL;
    }
    out->piece_end = range ? from_range_start(range_end(range), range) : piece_end_of(plan);
    const char *form_name = plan_root(plan) != PLAN_NODE_NONE ? plan_node_name(plan, plan_root(plan)) : NULL;
    if (form_name && form_name[0]) {
        out->form_name = malloc(strlen(form_name) + 1);
        if (!out->form_name) {
            return PLAN_RENDER_ERR_ALLOC;
        }
        memcpy(out->form_name, form_name, strlen(form_name) + 1);
    }
    PlanRenderResult result = render_into(plan, range, out);
    if (result == PLAN_RENDER_OK) {
        result = render_sections(plan, range, out);
    }
    if (result != PLAN_RENDER_OK) {
        rendered_plan_destroy(out);
    }
    return result;
}

PlanRenderResult plan_render(const PiecePlan *plan, RenderedPlan *out) {
    return render_checked(plan, NULL, out);
}

PlanRenderResult plan_render_range(const PiecePlan *plan, PlanSpan range, RenderedPlan *out) {
    return render_checked(plan, &range, out);
}
