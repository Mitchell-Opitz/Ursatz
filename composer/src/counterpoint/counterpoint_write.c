#include <ursatz_composer/counterpoint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-counterpoint/counterpoint_pitch.h>
#include <standard-counterpoint/consonance_rules.h>
#include <standard-voicing/range_rules.h>
#include <standard-voicing/voicing_pitch.h>
#include <semantics/voice_part.h>

#include <ursatz_composer/plan_state.h>

#include "counterpoint/counterpoint_internal.h"

CounterpointParams counterpoint_params_default(void) {
    CounterpointParams params = {COUNTERPOINT_DEFAULT_CANDIDATES, COUNTERPOINT_DEFAULT_BEAM};
    return params;
}

/* ------------------------------------------------------------ reading */

void counterpoint_context_free(CounterpointContext *context) {
    if (!context) {
        return;
    }
    for (size_t p = 0; p < 128; p++) {
        pitch_spelling_destroy(context->spell[p]);
    }
    pitch_spelling_destroy(context->tonic);
    memset(context, 0, sizeof(*context));
}

size_t counterpoint_slot_note(const CounterpointContext *context, size_t slot) {
    return context->per_bar == 1 ? slot : slot / context->per_bar;
}

/* The scale degree of a pitch class above the tonic, 1 to 7, or 0 when the key's scale has no such note. */
static int scale_degree(KeyStructureMode mode, int above_tonic) {
    static const int MAJOR[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};
    /* The minor scale as it is written in counterpoint: the natural minor, with the raised sixth and seventh. */
    static const int MINOR[12] = {1, 0, 2, 3, 0, 4, 0, 5, 6, 6, 7, 7};
    return mode == KEY_STRUCTURE_MODE_MINOR ? MINOR[above_tonic] : MAJOR[above_tonic];
}

static bool read_spellings(const AnalysisKey *key, CounterpointContext *out) {
    for (int pitch = 0; pitch < 128; pitch++) {
        int degree = scale_degree(key->mode, ((pitch - analysis_key_tonic_pitch_class(key)) % 12 + 12) % 12);
        if (degree > 0) {
            out->spell[pitch] = harmony_spell_pitch(key, degree, pitch);
            if (!out->spell[pitch]) {
                return false;
            }
        }
    }
    out->tonic = harmony_spell_tonic(key, 4);
    return out->tonic != NULL;
}

/* The given line over the target: written notes that follow one another from its start to its end. */
static bool read_line(const PiecePlan *plan, CounterpointContext *out, const char **why) {
    AccompLine lines[ACCOMP_MAX_LINE];
    size_t count = 0;
    out->length = out->span.length;
    if (!position_is_positive(out->length)) {
        *why = "the span has no length";
        return false;
    }
    if (!accomp_read_line(plan, out->above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, out->span, out->span.start,
                          out->length, lines, &count) ||
        count == 0) {
        *why = out->above ? "the bass under the span is not written" : "the melody over the span is not written";
        return false;
    }
    if (count > CP_MAX_NOTES) {
        *why = "the given line has too many notes";
        return false;
    }
    Rational at = POSITION_ZERO;
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(lines[i].onset, at) != 0) {
            *why = "the given line has a rest or an overlap";
            return false;
        }
        out->onset[i] = lines[i].onset;
        out->duration[i] = position_sub(lines[i].end, lines[i].onset);
        out->line[i] = lines[i].pitch;
        at = lines[i].end;
    }
    if (rational_cmp(at, out->length) != 0) {
        *why = "the given line does not fill the span";
        return false;
    }
    out->count = count;
    return true;
}

/* The bar of the meter under the target, and whether the given line is a cantus firmus in it. */
static bool read_cantus(const PiecePlan *plan, CounterpointContext *out, const char **why) {
    LaneSpanId meters[2];
    Rational origin = out->span.start;
    Rational meter_start;
    PlanMeter meter;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, out->span, meters, 2) != 1) {
        *why = "no single meter under the span";
        return false;
    }
    meter_start = plan_lane_span_span(plan, meters[0]).start;
    meter = plan_lane_span_payload(plan, meters[0])->meter;
    if (meter.beats_per_bar <= 0 || meter.beat_unit <= 0) {
        *why = "the meter is not one the operator reads";
        return false;
    }
    out->bar = position_mul(rational_ratio(1, meter.beat_unit), (long long)meter.beats_per_bar);
    out->per_bar = out->species == 3 ? 4 : 2;
    if (out->species == 3 && meter.beats_per_bar % 4 != 0) {
        *why = "third species is four notes to a bar and needs a meter of four beats";
        return false;
    }
    if (!position_is_multiple(position_sub(origin, meter_start), out->bar) ||
        !position_is_multiple(out->length, out->bar)) {
        *why = "the span does not start on a barline and fill whole bars";
        return false;
    }
    if (out->count < 3 || out->count != (size_t)position_floor_units(out->length, out->bar)) {
        *why = "species 2 to 4 need a cantus firmus of three bars or more, one whole note a bar";
        return false;
    }
    for (size_t i = 0; i < out->count; i++) {
        if (rational_cmp(out->duration[i], out->bar) != 0) {
            *why = "species 2 to 4 need a cantus firmus of three bars or more, one whole note a bar";
            return false;
        }
    }
    return true;
}

static bool read_target(const PiecePlan *plan, LaneSpanId target, CounterpointContext *out, const char **why,
                        bool written) {
    PlanConstraint constraint;
    const char *reason = "";
    int low;
    int high;
    if (!why) {
        why = &reason;
    }
    if (!plan || !out) {
        *why = "no plan";
        return false;
    }
    if (!plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_INNER_VOICE ||
        plan_lane_span_voice(plan, target) != 1 ||
        (written ? plan_lane_span_state(plan, target) == PLAN_STATE_CONSTRAINED
                 : plan_lane_span_state(plan, target) != PLAN_STATE_CONSTRAINED)) {
        *why = "not an open span of the inner voice 1";
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->length = POSITION_ZERO;
    out->bar = POSITION_ZERO;
    for (size_t i = 0; i < CP_MAX_NOTES; i++) {
        out->onset[i] = POSITION_ZERO;
        out->duration[i] = POSITION_ZERO;
    }
    out->target = target;
    out->owner = plan_lane_span_owner(plan, target);
    out->span = plan_lane_span_span(plan, target);
    if (!harmony_find_constraint(plan, target, out->owner, PLAN_CONSTRAINT_COUNTERPOINT, &constraint)) {
        *why = "no counterpoint constraint";
        return false;
    }
    if (!harmony_find_key(plan, target, out->owner, out->span, &out->key)) {
        *why = "no key under the span";
        return false;
    }
    out->species = constraint.params.counterpoint.species;
    out->above = constraint.params.counterpoint.above;
    out->per_bar = 1;
    if (!read_spellings(&out->key, out)) {
        *why = "the key cannot be spelled";
        counterpoint_context_free(out);
        return false;
    }
    if (!read_line(plan, out, why) || (out->species > 1 && !read_cantus(plan, out, why))) {
        counterpoint_context_free(out);
        return false;
    }
    if (out->count < 2) {
        *why = "the given line has one note";
        counterpoint_context_free(out);
        return false;
    }
    /* The counter-melody sings in the range of a soprano above the given line and of a tenor below it. */
    if (!standard_voicing_range_semitones(out->above ? VOICE_PART_SOPRANO : VOICE_PART_TENOR, &low, &high)) {
        *why = "no voice range";
        counterpoint_context_free(out);
        return false;
    }
    /* standard-voicing counts semitones from C0 (C4 is 48), MIDI from C-1 (C4 is 60). */
    out->low = low + STANDARD_VOICING_SEMITONES_PER_OCTAVE;
    out->high = high + STANDARD_VOICING_SEMITONES_PER_OCTAVE;
    for (size_t i = 0; i < out->count; i++) {
        if (out->line[i] < 0 || out->line[i] > 127 || !out->spell[out->line[i]]) {
            *why = "a note of the given line is not in the key's scale";
            counterpoint_context_free(out);
            return false;
        }
        for (int pitch = out->low; pitch <= out->high; pitch++) {
            const PitchSpelling *lower = out->above ? out->spell[out->line[i]] : out->spell[pitch];
            const PitchSpelling *upper = out->above ? out->spell[pitch] : out->spell[out->line[i]];
            bool ordered = out->above ? pitch >= out->line[i] : pitch <= out->line[i];
            out->consonant[i][pitch] = out->spell[pitch] && ordered && standard_counterpoint_pitches_consonant(lower, upper);
        }
    }
    return true;
}

bool counterpoint_read(const PiecePlan *plan, LaneSpanId target, CounterpointContext *out, const char **why) {
    return read_target(plan, target, out, why, false);
}

bool counterpoint_read_written(const PiecePlan *plan, LaneSpanId target, CounterpointContext *out,
                               CounterpointCandidate *line, const char **why) {
    const PlanNotes *notes;
    const char *reason = "";
    Rational slot_length;
    if (!why) {
        why = &reason;
    }
    if (!line || !read_target(plan, target, out, why, true)) {
        return false;
    }
    notes = &plan_lane_span_payload(plan, target)->notes;
    slot_length = out->per_bar == 1 ? POSITION_ZERO : position_div(out->bar, (long long)out->per_bar);
    memset(line, 0, sizeof(*line));
    line->magic = CP_PAYLOAD_MAGIC;
    line->slots = out->count * out->per_bar;
    for (size_t slot = 0; slot < line->slots; slot++) {
        Rational at = out->per_bar == 1 ? out->onset[slot] : position_mul(slot_length, (long long)slot);
        line->kind[slot] = CP_SLOT_REST;
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational onset = notes->notes[n].offset;
            Rational duration = notes->notes[n].duration;
            if (rational_cmp(onset, at) <= 0 && rational_cmp(at, position_add(onset, duration)) < 0) {
                line->kind[slot] = rational_cmp(onset, at) == 0 ? CP_SLOT_ATTACK : CP_SLOT_TIE;
                line->pitch[slot] = notes->notes[n].midi;
            }
        }
    }
    return true;
}

const char *counterpoint_why_not(const PiecePlan *plan, LaneSpanId target) {
    CounterpointContext *read = calloc(1, sizeof(*read));
    const char *why = NULL;
    if (!read) {
        return "out of memory";
    }
    if (counterpoint_read(plan, target, read, &why)) {
        why = NULL;
        counterpoint_context_free(read);
    }
    free(read);
    return why;
}

/* ------------------------------------------------------- the operator */

static bool operator_applies(const PiecePlan *plan, FillTarget target, void *context) {
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && counterpoint_why_not(plan, (LaneSpanId)target.id) == NULL;
}

#define MAX_CANDIDATES 16

static void candidate_label(const CounterpointCandidate *candidate, char *label, size_t size) {
    int used = snprintf(label, size, "species");
    for (size_t i = 0, shown = 0; used > 0 && (size_t)used + 1 < size && i < candidate->slots && shown < 6; i++) {
        if (candidate->kind[i] == CP_SLOT_ATTACK) {
            used += snprintf(label + used, size - (size_t)used, " %d", candidate->pitch[i]);
            shown++;
        }
    }
}

static bool operator_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                              FillCandidateList *out) {
    const CounterpointParams *params = context;
    size_t wanted = params && params->candidates > 0 ? params->candidates : COUNTERPOINT_DEFAULT_CANDIDATES;
    size_t beam = params && params->beam > 0 ? params->beam : COUNTERPOINT_DEFAULT_BEAM;
    CounterpointContext *read = calloc(1, sizeof(*read));
    CounterpointCandidate *found[MAX_CANDIDATES];
    size_t found_count = 0;
    bool ok = read && target.kind == FILL_TARGET_LANE_SPAN && counterpoint_read(plan, (LaneSpanId)target.id, read, NULL);
    if (wanted > MAX_CANDIDATES) {
        wanted = MAX_CANDIDATES;
    }
    ok = ok && counterpoint_search(read, rng, wanted, beam, found, &found_count);
    for (size_t i = 0; i < found_count; i++) {
        char label[FILL_LABEL_MAX];
        candidate_label(found[i], label, sizeof(label));
        /* The candidate list owns the payload, also when it cannot be added. */
        ok = fill_candidates_add(out, label, found[i], free) && ok;
    }
    counterpoint_context_free(read);
    free(read);
    return ok;
}

/* The slots as lane notes: an attack and the ties after it are one note, and a rest is a gap. */
static PlanResult slot_notes(const CounterpointContext *context, const CounterpointCandidate *candidate, PlanNote *notes,
                             size_t *count) {
    Rational slot_length = context->per_bar == 1 ? POSITION_ZERO : position_div(context->bar, (long long)context->per_bar);
    Rational start = POSITION_ZERO;
    Rational end = POSITION_ZERO;
    int pitch = 0;
    bool open = false;
    *count = 0;
    for (size_t i = 0; i <= candidate->slots; i++) {
        bool done = i == candidate->slots || candidate->kind[i] != CP_SLOT_TIE;
        if (open && done) {
            PlanNote *note = &notes[(*count)++];
            note->midi = pitch;
            note->spelled = false;
            note->offset = start;
            note->duration = position_sub(end, start);
            if (position_is_far(note->offset) || position_is_far(note->duration)) {
                return PLAN_ERR_BAD_TIME;
            }
            open = false;
        }
        if (i == candidate->slots) {
            break;
        }
        Rational from = context->per_bar == 1 ? context->onset[i] : position_mul(slot_length, (long long)i);
        Rational to = position_add(from, context->per_bar == 1 ? context->duration[i] : slot_length);
        if (candidate->kind[i] == CP_SLOT_ATTACK) {
            start = from;
            end = to;
            pitch = candidate->pitch[i];
            open = true;
        } else if (candidate->kind[i] == CP_SLOT_TIE && open) {
            end = to;
        }
    }
    return PLAN_OK;
}

/* The target keeps its id and takes the counter-melody. */
static PlanResult operator_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const CounterpointCandidate *line = candidate ? candidate->payload : NULL;
    CounterpointContext *read;
    PlanNote *notes;
    PlanResult result = PLAN_OK;
    size_t count = 0;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !line || line->magic != CP_PAYLOAD_MAGIC) {
        return PLAN_ERR_NULL;
    }
    read = calloc(1, sizeof(*read));
    notes = calloc(CP_MAX_SLOTS, sizeof(*notes));
    if (!read || !notes) {
        free(read);
        free(notes);
        return PLAN_ERR_ALLOC;
    }
    /* The plan must still be what the counter-melody was made for. */
    if (!counterpoint_read(plan, (LaneSpanId)target.id, read, NULL) ||
        line->slots != read->count * read->per_bar) {
        result = PLAN_ERR_BAD_PAYLOAD;
    }
    if (result == PLAN_OK) {
        PlanLanePayload payload;
        result = slot_notes(read, line, notes, &count);
        if (result == PLAN_OK) {
            memset(&payload, 0, sizeof(payload));
            payload.notes.notes = notes;
            payload.notes.note_count = count;
            result = plan_lane_span_write(plan, (LaneSpanId)target.id, &payload);
        }
    }
    counterpoint_context_free(read);
    free(read);
    free(notes);
    return result;
}

FillResult counterpoint_register(FillOperatorRegistry *registry, const CounterpointParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = COUNTERPOINT_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan inner voice lane span over a written melody or bass";
    operator_.generator.supported_outputs = "PiecePlan inner voice lane span";
    operator_.generator.configuration_description = "the number of candidates and the width of the beam";
    operator_.generator.constraints_description =
        "standard-counterpoint for the pair and the species, the given line as the cantus firmus";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = operator_applies;
    operator_.generate = operator_generate;
    operator_.apply = operator_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
