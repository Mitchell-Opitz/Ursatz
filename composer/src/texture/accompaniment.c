#include <ursatz_composer/accompaniment.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>

#include "texture/texture_internal.h"

/* The fewest semitones a voice moves per chord change, on average, for the candidate to score for it. */
#define SMOOTH_LIMIT 3

AccompanimentParams accompaniment_params_default(void) {
    AccompanimentParams params = {ACCOMPANIMENT_DEFAULT_CANDIDATES};
    return params;
}

/* ------------------------------------------------------------ reading */

void accomp_context_zero(AccompContext *context) {
    memset(context, 0, sizeof(*context));
    context->length = POSITION_ZERO;
    context->beat = POSITION_ZERO;
    context->cell = POSITION_ZERO;
    context->bar = POSITION_ZERO;
    context->phase = POSITION_ZERO;
    context->motive_length = POSITION_ZERO;
    context->liquidate_from = POSITION_ZERO;
    for (size_t i = 0; i <= HARMONY_MAX_CHORDS; i++) {
        context->chord_start[i] = POSITION_ZERO;
    }
    for (size_t i = 0; i < ACCOMP_MAX_LINE; i++) {
        context->bass[i].onset = context->bass[i].end = POSITION_ZERO;
        context->melody[i].onset = context->melody[i].end = POSITION_ZERO;
    }
    for (size_t i = 0; i < ACCOMP_MAX_MOTIVE; i++) {
        context->motive[i].onset = context->motive[i].duration = POSITION_ZERO;
    }
}

size_t accomp_chord_at(const AccompContext *context, Rational position) {
    size_t chord = 0;
    for (size_t i = 1; i < context->chord_count; i++) {
        if (rational_cmp(context->chord_start[i], position) <= 0) {
            chord = i;
        }
    }
    return chord;
}

/* The pitch classes of a chord in the key, root first. */
bool accomp_chord_tones(const PlanKey *key, const PhraseChord *chord, int pcs[4], size_t *count) {
    int intervals[4];
    int root;
    if (chord->applied) {
        if (!harmony_applied_intervals(&chord->applied_chord, intervals, count)) {
            return false;
        }
        root = harmony_applied_root_semitone(harmony_mode_of(key->mode), &chord->applied_chord);
    } else {
        if (!harmony_diatonic_intervals(key->mode, &chord->chord, intervals, count)) {
            return false;
        }
        /* The chord on the leading tone of a minor key stands on the raised seventh degree. */
        root = key->mode == PLAN_MODE_MINOR && chord->chord.leading_tone
                   ? 11
                   : harmony_applied_scale_semitone(harmony_mode_of(key->mode), chord->chord.degree);
    }
    if (root < 0 || *count < 3) {
        return false;
    }
    for (size_t k = 0; k < *count; k++) {
        pcs[k] = (key->tonic_pitch_class + root + intervals[k]) % 12;
    }
    return true;
}

/* The notes of a lane over the target that sound inside it, from its start. False if any span there is open. */
bool accomp_read_line(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, Rational origin,
                      Rational length, AccompLine *out, size_t *count) {
    LaneSpanId ids[ACCOMP_MAX_LINE];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, ACCOMP_MAX_LINE);
    *count = 0;
    if (found == 0 || found > ACCOMP_MAX_LINE) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        Rational base;
        const PlanNotes *notes;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
        base = plan_lane_span_span(plan, ids[i]).start;
        notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational duration = notes->notes[n].duration;
            if (!position_is_positive(duration)) {
                return false;
            }
            Rational onset = position_add(position_sub(base, origin), notes->notes[n].offset);
            Rational end = position_add(onset, duration);
            if (rational_cmp(end, POSITION_ZERO) <= 0 || rational_cmp(onset, length) >= 0) {
                continue;
            }
            if (*count >= ACCOMP_MAX_LINE) {
                return false;
            }
            out[*count].onset = rational_cmp(onset, POSITION_ZERO) < 0 ? POSITION_ZERO : onset;
            out[*count].end = rational_cmp(end, length) > 0 ? length : end;
            out[*count].pitch = notes->notes[n].midi_pitch;
            (*count)++;
        }
    }
    return true;
}

/* The span of voice (2 or 3) that is to be written with the target: none, or an open one over the same time. */
bool accomp_read_voice(const PiecePlan *plan, int voice, PlanSpan span, LaneSpanId *out) {
    LaneSpanId ids[2];
    size_t found = plan_lane_find(plan, PLAN_LANE_INNER_VOICE, voice, span, ids, 2);
    *out = PLAN_LANE_SPAN_NONE;
    if (found == 0) {
        return true;
    }
    PlanSpan other = plan_lane_span_span(plan, ids[0]);
    if (found != 1 || plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED ||
        rational_cmp(other.start, span.start) != 0 || rational_cmp(other.length, span.length) != 0) {
        return false;
    }
    *out = ids[0];
    return true;
}

/* The Given melody span of the node named `name`, read as the accompaniment motive; false when there is none. */
static bool read_motive(const PiecePlan *plan, const char *name, AccompContext *out) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        LaneSpanId ids[4];
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_MOTIVE || !found || strcmp(found, name) != 0) {
            continue;
        }
        size_t spans = plan_node_lane_spans(plan, id, ids, 4);
        for (size_t i = 0; i < spans && i < 4; i++) {
            if (plan_lane_span_lane(plan, ids[i]) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, ids[i]) != 0 ||
                plan_lane_span_state(plan, ids[i]) != PLAN_STATE_GIVEN) {
                continue;
            }
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
            if (notes->note_count == 0 || notes->note_count > ACCOMP_MAX_MOTIVE) {
                return false;
            }
            for (size_t n = 0; n < notes->note_count; n++) {
                AccompMotiveNote *note = &out->motive[n];
                note->onset = notes->notes[n].offset;
                note->duration = notes->notes[n].duration;
                if (!position_is_positive(note->duration)) {
                    return false;
                }
                note->pitch = notes->notes[n].midi_pitch;
                out->motive_length = position_add(note->onset, note->duration);
            }
            out->motive_count = notes->note_count;
            return true;
        }
    }
    return false;
}

/*
 * Where the last bar of the section the target is in begins, from the target's start; of the target itself
 * when it is in no section.
 */
static Rational liquidation_start(const PiecePlan *plan, const AccompContext *context, Rational origin) {
    NodeId section = plan_section_of(plan, context->owner);
    if (section == PLAN_NODE_NONE) {
        return position_sub(context->length, context->bar);
    }
    PlanSpan whole = plan_node_span(plan, section);
    Rational end;
    if (rational_add(whole.start, whole.length, &end) != RATIONAL_OK) {
        return context->length;
    }
    return position_sub(position_sub(end, origin), context->bar);
}

bool accomp_read(const PiecePlan *plan, LaneSpanId target, AccompContext *out) {
    PhraseChord chords[HARMONY_MAX_CHORDS];
    PhraseMetre metre;
    LaneSpanId harmony[HARMONY_MAX_CHORDS];
    LaneSpanId meters[2];
    PlanConstraint constraint;
    size_t count;
    Rational origin;
    Rational meter_start;
    if (!plan || !out || !plan_has_lane_span(plan, target) ||
        plan_lane_span_lane(plan, target) != PLAN_LANE_INNER_VOICE || plan_lane_span_voice(plan, target) != 1 ||
        plan_lane_span_state(plan, target) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    accomp_context_zero(out);
    out->target = target;
    out->owner = plan_lane_span_owner(plan, target);
    out->span = plan_lane_span_span(plan, target);
    if (!harmony_find_constraint(plan, target, out->owner, PLAN_CONSTRAINT_ACCOMPANIMENT, &constraint) ||
        !harmony_find_key(plan, target, out->owner, out->span, &out->key)) {
        return false;
    }
    out->pattern = constraint.params.accompaniment.pattern;
    out->texture = constraint.params.accompaniment.texture;
    out->low = constraint.params.accompaniment.low ? constraint.params.accompaniment.low : PLAN_ACCOMPANIMENT_DEFAULT_LOW;
    out->high =
        constraint.params.accompaniment.high ? constraint.params.accompaniment.high : PLAN_ACCOMPANIMENT_DEFAULT_HIGH;

    /* The chords tile the target, all written. */
    if (!harmony_read_lane(plan, out->span, out->key.mode, chords, &count, &metre) || count == 0 ||
        count > HARMONY_MAX_CHORDS ||
        plan_lane_find(plan, PLAN_LANE_HARMONY, 0, out->span, harmony, HARMONY_MAX_CHORDS) != count) {
        return false;
    }
    out->meter = metre.meter;
    out->compound = out->meter.beat_unit == 8 && out->meter.beats_per_bar >= 6 && out->meter.beats_per_bar % 3 == 0;
    size_t cells = 0;
    out->length = out->span.length;
    origin = out->span.start;
    out->cell = metre.cell_length;
    out->bar = metre.bar_length;
    if (!position_is_positive(out->cell) || !position_is_positive(out->length) || !position_is_positive(out->bar) ||
        out->meter.beat_unit <= 0) {
        return false;
    }
    out->beat = out->compound ? rational_ratio(3, 8) : rational_ratio(1, out->meter.beat_unit);
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, out->span, meters, 2) != 1) {
        return false;
    }
    meter_start = plan_lane_span_span(plan, meters[0]).start;
    out->phase = position_remainder(position_sub(origin, meter_start), out->bar);
    Rational at = POSITION_ZERO;
    for (size_t i = 0; i < count; i++) {
        unsigned seventh;
        Rational chord_from;
        Rational chord_length;
        if (plan_lane_span_state(plan, harmony[i]) == PLAN_STATE_CONSTRAINED ||
            !accomp_chord_tones(&out->key, &chords[i], out->chord_pcs[i], &out->chord_pc_count[i]) ||
            !melody_chord_mask(&out->key, &chords[i], &out->chord_mask[i], &seventh) ||
            !harmony_cells_of_span(&metre, cells, chords[i].cells, &chord_from, &chord_length) ||
            !position_is_positive(chord_length)) {
            return false;
        }
        out->chord_start[i] = at;
        at = position_add(at, chord_length);
        cells += chords[i].cells;
    }
    out->chord_count = count;
    out->chord_start[count] = at;
    if (rational_cmp(at, out->length) != 0) {
        return false;
    }

    /* The bass and the melody are written. */
    if (!accomp_read_line(plan, PLAN_LANE_BASS, 0, out->span, origin, out->length, out->bass, &out->bass_count) ||
        !accomp_read_line(plan, PLAN_LANE_MELODY, 0, out->span, origin, out->length, out->melody, &out->melody_count)) {
        return false;
    }
    if (out->pattern == PLAN_ACCOMPANIMENT_MOTIVE) {
        if (!read_motive(plan, constraint.params.accompaniment.motive_node, out)) {
            return false;
        }
        out->liquidate_from = liquidation_start(plan, out, origin);
    }
    return accomp_read_voice(plan, 2, out->span, &out->voice_span[1]) && accomp_read_voice(plan, 3, out->span, &out->voice_span[2]);
}

/* ------------------------------------------------------------ judging */

void accomp_judge(const AccompContext *context, const AccompCandidate *candidate, AccompVerdict *verdict) {
    memset(verdict, 0, sizeof(*verdict));
    for (size_t voice = 0; voice < ACCOMP_VOICES && !verdict->hard; voice++) {
        for (size_t n = 0; n < candidate->note_count[voice] && !verdict->hard; n++) {
            const AccompNote *note = &candidate->notes[voice][n];
            Rational end = position_add(note->onset, note->duration);
            if (note->pitch < context->low || note->pitch > context->high) {
                verdict->hard = "a note outside the range";
            } else if (rational_cmp(note->onset, POSITION_ZERO) < 0 || rational_cmp(end, context->length) > 0 ||
                       !position_is_positive(note->duration)) {
                verdict->hard = "a note outside the span";
            } else if (!(context->chord_mask[accomp_chord_at(context, note->onset)] & (1u << (note->pitch % 12)))) {
                verdict->hard = "a note that is not a tone of its chord";
            }
            for (size_t i = 0; i < context->bass_count && !verdict->hard; i++) {
                const AccompLine *line = &context->bass[i];
                if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, note->onset) > 0 &&
                    note->pitch <= line->pitch) {
                    verdict->hard = "a note not above the bass";
                }
            }
            for (size_t i = 0; i < context->melody_count && !verdict->hard; i++) {
                const AccompLine *line = &context->melody[i];
                if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, note->onset) > 0 &&
                    note->pitch >= line->pitch) {
                    verdict->hard = "a note not below the melody";
                }
            }
        }
    }
    if (verdict->hard) {
        return;
    }
    /* Smooth: the voicing moves little from one chord to the next, on average. */
    long moved = 0;
    long changes = 0;
    const int *previous = NULL;
    size_t previous_size = 0;
    size_t previous_chord = 0;
    for (size_t b = 0; b < candidate->beat_count; b++) {
        if (!candidate->voiced[b]) {
            continue;
        }
        size_t chord = accomp_chord_at(context, candidate->beat_start[b]);
        if (previous && chord != previous_chord) {
            size_t voices = candidate->voicing_size[b] < previous_size ? candidate->voicing_size[b] : previous_size;
            moved += accomp_voicing_distance(candidate->voicing[b], candidate->voicing_size[b], previous, previous_size);
            changes += (long)voices;
        }
        previous = candidate->voicing[b];
        previous_size = candidate->voicing_size[b];
        previous_chord = chord;
    }
    verdict->total = 1;
    verdict->held = changes == 0 || moved < SMOOTH_LIMIT * changes ? 1 : 0;
    /* Heard: the texture the target asks for. A candidate the analysis cannot be run on does not hold. */
    if (context->texture >= 0) {
        bool heard = false;
        verdict->total++;
        verdict->held += accomp_hear(context, candidate, &heard) && heard ? 1 : 0;
    }
}

/* ------------------------------------------------------- the operator */

static bool operator_applies(const PiecePlan *plan, FillTarget target, void *context) {
    AccompContext *read;
    bool applies = false;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN) {
        return false;
    }
    read = malloc(sizeof(*read));
    if (read && accomp_read(plan, (LaneSpanId)target.id, read)) {
        /* A pattern the meter does not allow, or the texture does not, is one the operator cannot write. */
        for (PlanAccompanimentPattern p = PLAN_ACCOMPANIMENT_BLOCK; !applies && p <= PLAN_ACCOMPANIMENT_MOTIVE; p++) {
            applies = accomp_pattern_allowed(read, p);
        }
    }
    free(read);
    return applies;
}

static bool same_notes(const AccompCandidate *a, const AccompCandidate *b) {
    if (a->pattern != b->pattern) {
        return false;
    }
    for (size_t voice = 0; voice < ACCOMP_VOICES; voice++) {
        if (a->note_count[voice] != b->note_count[voice]) {
            return false;
        }
        for (size_t n = 0; n < a->note_count[voice]; n++) {
            if (a->notes[voice][n].pitch != b->notes[voice][n].pitch ||
                rational_cmp(a->notes[voice][n].onset, b->notes[voice][n].onset) != 0) {
                return false;
            }
        }
    }
    return true;
}

static void candidate_label(const AccompCandidate *candidate, char *label, size_t size) {
    int used = snprintf(label, size, "%s", accomp_pattern_name(candidate->pattern));
    for (size_t b = 0; used > 0 && (size_t)used + 1 < size && b < candidate->beat_count && b < 3; b++) {
        if (candidate->voiced[b]) {
            used += snprintf(label + used, size - (size_t)used, candidate->voicing_size[b] == 3 ? " %d-%d-%d" : " %d-%d",
                             candidate->voicing[b][0], candidate->voicing[b][1], candidate->voicing[b][2]);
        }
    }
}

static bool operator_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                              FillCandidateList *out) {
    const AccompanimentParams *params = context;
    size_t wanted = params && params->candidates > 0 ? params->candidates : ACCOMPANIMENT_DEFAULT_CANDIDATES;
    AccompContext *read = malloc(sizeof(*read));
    AccompCandidate *scratch = malloc(sizeof(*scratch));
    AccompCandidate *kept[16];
    size_t kept_count = 0;
    PlanAccompanimentPattern allowed[5];
    size_t allowed_count = 0;
    bool ok = read && scratch;
    if (wanted > 16) {
        wanted = 16;
    }
    if (ok && (target.kind != FILL_TARGET_LANE_SPAN || !accomp_read(plan, (LaneSpanId)target.id, read))) {
        ok = false;
    }
    for (PlanAccompanimentPattern p = PLAN_ACCOMPANIMENT_BLOCK; ok && p <= PLAN_ACCOMPANIMENT_MOTIVE; p++) {
        if (accomp_pattern_allowed(read, p)) {
            allowed[allowed_count++] = p;
        }
    }
    /* The patterns are tried in turn from a drawn one, so each is proposed before one is proposed twice. */
    size_t first = ok && allowed_count > 0 ? (size_t)fill_rng_below(rng, allowed_count) : 0;
    for (size_t attempt = 0; ok && allowed_count > 0 && attempt < wanted * 3 && kept_count < wanted; attempt++) {
        AccompVerdict verdict;
        bool repeat = false;
        if (!accomp_build(read, allowed[(first + attempt) % allowed_count], rng, scratch)) {
            continue;
        }
        accomp_judge(read, scratch, &verdict);
        if (verdict.hard) {
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_notes(kept[i], scratch);
        }
        if (repeat) {
            continue;
        }
        AccompCandidate *payload = malloc(sizeof(*payload));
        char label[FILL_LABEL_MAX];
        if (!payload) {
            ok = false;
            break;
        }
        *payload = *scratch;
        candidate_label(payload, label, sizeof(label));
        ok = fill_candidates_add(out, label, payload, free);
        kept[kept_count++] = payload;
    }
    free(scratch);
    free(read);
    return ok;
}

/* The notes of a voice as lane notes, from the start of the target. */
static PlanResult voice_payload(const AccompCandidate *candidate, size_t voice, PlanNote *notes, PlanLanePayload *payload) {
    for (size_t n = 0; n < candidate->note_count[voice]; n++) {
        notes[n].midi_pitch = candidate->notes[voice][n].pitch;
        notes[n].offset = candidate->notes[voice][n].onset;
        notes[n].duration = candidate->notes[voice][n].duration;
        if (position_is_far(notes[n].offset) || position_is_far(notes[n].duration)) {
            return PLAN_ERR_BAD_TIME;
        }
    }
    memset(payload, 0, sizeof(*payload));
    payload->notes.notes = notes;
    payload->notes.note_count = candidate->note_count[voice];
    return PLAN_OK;
}

/* The target keeps its id and takes voice 1; voices 2 and 3 are written in the spans over the same time. */
static PlanResult operator_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const AccompCandidate *accompaniment = candidate ? candidate->payload : NULL;
    AccompContext *read;
    PlanNote *notes;
    PlanResult result = PLAN_OK;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !accompaniment || accompaniment->magic != ACCOMP_PAYLOAD_MAGIC) {
        return PLAN_ERR_NULL;
    }
    read = malloc(sizeof(*read));
    notes = calloc(ACCOMP_MAX_NOTES, sizeof(*notes));
    if (!read || !notes) {
        free(read);
        free(notes);
        return PLAN_ERR_ALLOC;
    }
    /* The plan must still be what the accompaniment was made for. */
    if (!accomp_read(plan, (LaneSpanId)target.id, read) || !accomp_pattern_allowed(read, accompaniment->pattern)) {
        result = PLAN_ERR_BAD_PAYLOAD;
    }
    for (size_t voice = 0; result == PLAN_OK && voice < ACCOMP_VOICES; voice++) {
        PlanLanePayload payload;
        LaneSpanId id = voice == 0 ? (LaneSpanId)target.id : read->voice_span[voice];
        result = voice_payload(accompaniment, voice, notes, &payload);
        if (result != PLAN_OK) {
            break;
        }
        if (id != PLAN_LANE_SPAN_NONE) {
            result = plan_lane_span_write(plan, id, &payload);
        } else if (accompaniment->note_count[voice] > 0) {
            result = plan_add_lane_span(plan, read->owner, PLAN_LANE_INNER_VOICE, (int)voice + 1, read->span, &payload, &id);
            if (result == PLAN_OK) {
                result = plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED);
            }
        }
    }
    free(read);
    free(notes);
    return result;
}

bool accompaniment_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                           const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, ACCOMPANIMENT_OPERATOR_NAME) != 0) {
        return harmony_phrase_checker(plan, target, operator_, candidate, context, out);
    }
    const AccompCandidate *accompaniment = candidate->payload;
    AccompContext *read = malloc(sizeof(*read));
    AccompVerdict verdict;
    bool ok = read && accompaniment && accompaniment->magic == ACCOMP_PAYLOAD_MAGIC && target.kind == FILL_TARGET_LANE_SPAN &&
              accomp_read(plan, (LaneSpanId)target.id, read);
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (ok) {
        accomp_judge(read, accompaniment, &verdict);
        out->hard_violation = verdict.hard != NULL;
        if (verdict.hard) {
            snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        } else {
            out->score_numerator = verdict.held;
            out->score_denominator = verdict.total ? verdict.total : 1;
        }
    }
    free(read);
    return ok;
}

FillResult accompaniment_register(FillOperatorRegistry *registry, const AccompanimentParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = ACCOMPANIMENT_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan inner voice lane span over a written harmony, bass and melody";
    operator_.generator.supported_outputs = "PiecePlan inner voice lane spans";
    operator_.generator.configuration_description = "the number of candidates";
    operator_.generator.constraints_description = "chord tones above the bass and below the melody, in range";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = operator_applies;
    operator_.generate = operator_generate;
    operator_.apply = operator_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
