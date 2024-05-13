#include <ursatz_composer/accompaniment.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "texture/texture_internal.h"

/* The fewest semitones a voice moves per chord change, on average, for the candidate to score for it. */
#define SMOOTH_LIMIT 3

AccompanimentParams accompaniment_params_default(void) {
    AccompanimentParams params = {ACCOMPANIMENT_DEFAULT_CANDIDATES};
    return params;
}

/* ------------------------------------------------------------ reading */

size_t accomp_chord_at(const AccompContext *context, long tick) {
    size_t chord = 0;
    for (size_t i = 1; i < context->chord_count; i++) {
        if (context->chord_start[i] <= tick) {
            chord = i;
        }
    }
    return chord;
}

/* The pitch classes of a chord in the key, root first. */
static bool chord_tones(const PlanKey *key, const PhraseChord *chord, int pcs[4], size_t *count) {
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
static bool read_line(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span, long origin, long length, AccompLine *out,
                      size_t *count) {
    LaneSpanId ids[ACCOMP_MAX_LINE];
    size_t found = plan_lane_find(plan, lane, 0, span, ids, ACCOMP_MAX_LINE);
    *count = 0;
    if (found == 0 || found > ACCOMP_MAX_LINE) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        long base;
        const PlanNotes *notes;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !melody_ticks(plan_lane_span_span(plan, ids[i]).start, &base)) {
            return false;
        }
        notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            long offset;
            long duration;
            if (!melody_ticks(notes->notes[n].offset, &offset) || !melody_ticks(notes->notes[n].duration, &duration) ||
                duration <= 0) {
                return false;
            }
            long onset = base - origin + offset;
            long end = onset + duration;
            if (end <= 0 || onset >= length) {
                continue;
            }
            if (*count >= ACCOMP_MAX_LINE) {
                return false;
            }
            out[*count].onset = onset < 0 ? 0 : onset;
            out[*count].end = end > length ? length : end;
            out[*count].pitch = notes->notes[n].midi_pitch;
            (*count)++;
        }
    }
    return true;
}

/* The span of voice (2 or 3) that is to be written with the target: none, or an open one over the same time. */
static bool read_voice(const PiecePlan *plan, int voice, PlanSpan span, LaneSpanId *out) {
    LaneSpanId ids[2];
    size_t found = plan_lane_find(plan, PLAN_LANE_INNER_VOICE, voice, span, ids, 2);
    *out = PLAN_LANE_SPAN_NONE;
    if (found == 0) {
        return true;
    }
    PlanSpan other = plan_lane_span_span(plan, ids[0]);
    if (found != 1 || plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED ||
        plan_time_compare(other.start, span.start) != 0 || plan_time_compare(other.length, span.length) != 0) {
        return false;
    }
    *out = ids[0];
    return true;
}

bool accomp_read(const PiecePlan *plan, LaneSpanId target, AccompContext *out) {
    PhraseChord chords[HARMONY_MAX_CHORDS];
    PhraseMetre metre;
    LaneSpanId harmony[HARMONY_MAX_CHORDS];
    LaneSpanId meters[2];
    PlanConstraint constraint;
    size_t count;
    long origin;
    long meter_start;
    if (!plan || !out || !plan_has_lane_span(plan, target) ||
        plan_lane_span_lane(plan, target) != PLAN_LANE_INNER_VOICE || plan_lane_span_voice(plan, target) != 1 ||
        plan_lane_span_state(plan, target) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    memset(out, 0, sizeof(*out));
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
    long cell;
    if (!melody_ticks(out->span.length, &out->length) || !melody_ticks(out->span.start, &origin) ||
        !melody_ticks(metre.cell_length, &cell) || !melody_ticks(metre.bar_length, &out->bar) || cell <= 0 ||
        out->length <= 0 || out->bar <= 0 || out->meter.beat_unit <= 0 || MELODY_TICKS % out->meter.beat_unit != 0) {
        return false;
    }
    out->beat = out->compound ? 3 * (MELODY_TICKS / 8) : MELODY_TICKS / out->meter.beat_unit;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, out->span, meters, 2) != 1 ||
        !melody_ticks(plan_lane_span_span(plan, meters[0]).start, &meter_start)) {
        return false;
    }
    out->phase = ((origin - meter_start) % out->bar + out->bar) % out->bar;
    long at = 0;
    for (size_t i = 0; i < count; i++) {
        unsigned seventh;
        if (plan_lane_span_state(plan, harmony[i]) == PLAN_STATE_CONSTRAINED ||
            !chord_tones(&out->key, &chords[i], out->chord_pcs[i], &out->chord_pc_count[i]) ||
            !melody_chord_mask(&out->key, &chords[i], &out->chord_mask[i], &seventh)) {
            return false;
        }
        out->chord_start[i] = at;
        at += (long)chords[i].cells * cell;
    }
    out->chord_count = count;
    out->chord_start[count] = at;
    if (at != out->length) {
        return false;
    }

    /* The bass and the melody are written. */
    if (!read_line(plan, PLAN_LANE_BASS, out->span, origin, out->length, out->bass, &out->bass_count) ||
        !read_line(plan, PLAN_LANE_MELODY, out->span, origin, out->length, out->melody, &out->melody_count)) {
        return false;
    }
    return read_voice(plan, 2, out->span, &out->voice_span[1]) && read_voice(plan, 3, out->span, &out->voice_span[2]);
}

/* ------------------------------------------------------------ judging */

void accomp_judge(const AccompContext *context, const AccompCandidate *candidate, AccompVerdict *verdict) {
    memset(verdict, 0, sizeof(*verdict));
    for (size_t voice = 0; voice < ACCOMP_VOICES && !verdict->hard; voice++) {
        for (size_t n = 0; n < candidate->note_count[voice] && !verdict->hard; n++) {
            const AccompNote *note = &candidate->notes[voice][n];
            long end = note->onset + note->duration;
            if (note->pitch < context->low || note->pitch > context->high) {
                verdict->hard = "a note outside the range";
            } else if (note->onset < 0 || end > context->length || note->duration <= 0) {
                verdict->hard = "a note outside the span";
            } else if (!(context->chord_mask[accomp_chord_at(context, note->onset)] & (1u << (note->pitch % 12)))) {
                verdict->hard = "a note that is not a tone of its chord";
            }
            for (size_t i = 0; i < context->bass_count && !verdict->hard; i++) {
                const AccompLine *line = &context->bass[i];
                if (line->onset < end && line->end > note->onset && note->pitch <= line->pitch) {
                    verdict->hard = "a note not above the bass";
                }
            }
            for (size_t i = 0; i < context->melody_count && !verdict->hard; i++) {
                const AccompLine *line = &context->melody[i];
                if (line->onset < end && line->end > note->onset && note->pitch >= line->pitch) {
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
    size_t previous_chord = 0;
    for (size_t b = 0; b < candidate->beat_count; b++) {
        if (!candidate->voiced[b]) {
            continue;
        }
        size_t chord = accomp_chord_at(context, (long)b * context->beat);
        if (previous && chord != previous_chord) {
            for (int v = 0; v < 3; v++) {
                moved += labs((long)candidate->voicing[b][v] - previous[v]);
            }
            changes += 3;
        }
        previous = candidate->voicing[b];
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
        for (PlanAccompanimentPattern p = PLAN_ACCOMPANIMENT_BLOCK; !applies && p <= PLAN_ACCOMPANIMENT_OOM_PAH; p++) {
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
                a->notes[voice][n].onset != b->notes[voice][n].onset) {
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
            used += snprintf(label + used, size - (size_t)used, " %d-%d-%d", candidate->voicing[b][0],
                             candidate->voicing[b][1], candidate->voicing[b][2]);
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
    PlanAccompanimentPattern allowed[4];
    size_t allowed_count = 0;
    bool ok = read && scratch;
    if (wanted > 16) {
        wanted = 16;
    }
    if (ok && (target.kind != FILL_TARGET_LANE_SPAN || !accomp_read(plan, (LaneSpanId)target.id, read))) {
        ok = false;
    }
    for (PlanAccompanimentPattern p = PLAN_ACCOMPANIMENT_BLOCK; ok && p <= PLAN_ACCOMPANIMENT_OOM_PAH; p++) {
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
        PlanResult result = melody_time(candidate->notes[voice][n].onset, &notes[n].offset);
        if (result == PLAN_OK) {
            result = melody_time(candidate->notes[voice][n].duration, &notes[n].duration);
        }
        if (result != PLAN_OK) {
            return result;
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
