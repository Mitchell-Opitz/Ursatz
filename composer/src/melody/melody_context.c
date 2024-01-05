#include "melody/melody_internal.h"

#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/plan_state.h>

bool melody_ticks(PlanTime time, long *out) {
    long scaled = time.num * MELODY_TICKS;
    if (time.den == 0 || scaled % (long)time.den != 0) {
        return false;
    }
    *out = scaled / (long)time.den;
    return true;
}

PlanResult melody_time(long ticks, PlanTime *out) {
    return plan_time_make(ticks, (unsigned long)MELODY_TICKS, out);
}

bool melody_chord_mask(const PlanKey *key, const PhraseChord *chord, unsigned *mask, unsigned *seventh) {
    int intervals[4];
    size_t count;
    int root;
    if (chord->applied) {
        if (!harmony_applied_intervals(&chord->applied_chord, intervals, &count)) {
            return false;
        }
        root = harmony_applied_root_semitone(harmony_mode_of(key->mode), &chord->applied_chord);
    } else {
        if (!harmony_diatonic_intervals(key->mode, &chord->chord, intervals, &count)) {
            return false;
        }
        /* The chord on the leading tone of a minor key stands on the raised seventh degree. */
        root = key->mode == PLAN_MODE_MINOR && chord->chord.leading_tone
                   ? 11
                   : harmony_applied_scale_semitone(harmony_mode_of(key->mode), chord->chord.degree);
    }
    if (root < 0) {
        return false;
    }
    *mask = 0;
    *seventh = 0;
    for (size_t k = 0; k < count; k++) {
        unsigned bit = 1u << ((key->tonic_pitch_class + root + intervals[k]) % 12);
        *mask |= bit;
        if (k == 3) {
            *seventh = bit;
        }
    }
    return true;
}

size_t melody_chord_at(const MelodyContext *context, long tick) {
    size_t chord = 0;
    for (size_t i = 1; i < context->count; i++) {
        if (context->chord_start[i] <= tick) {
            chord = i;
        }
    }
    return chord;
}

unsigned melody_need_mask(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *note) {
    unsigned mask = ~0u;
    long end = note->onset + note->duration;
    /* A suspension is held into a chord it is not a tone of. */
    if (params->nct_types & MELODY_NCT_SUSPENSION) {
        return context->chord_mask[melody_chord_at(context, note->onset)];
    }
    for (size_t c = melody_chord_at(context, note->onset); c < context->count && context->chord_start[c] < end; c++) {
        mask &= context->chord_mask[c];
    }
    return mask & 0xFFFu;
}

/* The first melody lane span (voice 0) of node that is Given or Realized. */
static bool melody_span_of(const PiecePlan *plan, NodeId node, LaneSpanId *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            *out = ids[i];
            return true;
        }
    }
    return false;
}

/* The bass lane over the phrase: every note, from the start of the phrase, tiling it. */
static bool read_bass(const PiecePlan *plan, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, context->span, ids, HARMONY_MAX_CHORDS);
    long origin;
    long at = 0;
    if (found == 0 || found > HARMONY_MAX_CHORDS || !melody_ticks(context->span.start, &origin)) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        long base;
        const PlanNotes *notes;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !plan_span_contains(context->span, plan_lane_span_span(plan, ids[i])) ||
            !melody_ticks(plan_lane_span_span(plan, ids[i]).start, &base)) {
            return false;
        }
        notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            long offset;
            long duration;
            if (context->bass_count >= MELODY_MAX_BASS || !melody_ticks(notes->notes[n].offset, &offset) ||
                !melody_ticks(notes->notes[n].duration, &duration) || base - origin + offset != at || duration <= 0) {
                return false;
            }
            context->bass[context->bass_count].onset = at;
            context->bass[context->bass_count].end = at + duration;
            context->bass[context->bass_count].pitch = notes->notes[n].midi_pitch;
            context->bass[context->bass_count].chord = melody_chord_at(context, at);
            context->bass_count++;
            at += duration;
        }
    }
    return at == context->length;
}

/* The motive named by the span's motive_ref constraint, in ticks. */
static bool read_motive(const PiecePlan *plan, MelodyContext *context) {
    PlanConstraint constraint;
    LaneSpanId span;
    if (!harmony_find_constraint(plan, context->target, context->owner, PLAN_CONSTRAINT_MOTIVE_REF, &constraint) ||
        !plan_has_node(plan, constraint.params.motive.motive) ||
        !melody_span_of(plan, constraint.params.motive.motive, &span)) {
        return false;
    }
    const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
    if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
        return false;
    }
    context->motive_node = constraint.params.motive.motive;
    context->transform = constraint.params.motive.transform;
    context->motive_given = plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN;
    context->motive_count = notes->note_count;
    context->motive_length = 0;
    for (size_t i = 0; i < notes->note_count; i++) {
        long end;
        if (!melody_ticks(notes->notes[i].offset, &context->motive_onset[i]) ||
            !melody_ticks(notes->notes[i].duration, &context->motive_duration[i]) ||
            context->motive_duration[i] <= 0 || notes->notes[i].midi_pitch < 0 || notes->notes[i].midi_pitch > 127) {
            return false;
        }
        context->motive_pitch[i] = notes->notes[i].midi_pitch;
        end = context->motive_onset[i] + context->motive_duration[i];
        if (i > 0 && context->motive_onset[i] < context->motive_onset[i - 1] + context->motive_duration[i - 1]) {
            return false;
        }
        context->motive_length = end;
    }
    return true;
}

bool melody_context_read(const PiecePlan *plan, LaneSpanId target, MelodyContext *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_MELODY ||
        plan_lane_span_voice(plan, target) != 0 || plan_lane_span_state(plan, target) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    MelodyContext *context = out;
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    LaneSpanId melody[2];
    long cells = 0;
    memset(context, 0, sizeof(*context));
    context->target = target;
    context->owner = plan_lane_span_owner(plan, target);
    context->span = plan_lane_span_span(plan, target);

    /* The target is the only melody in the phrase. */
    if (plan_lane_find(plan, PLAN_LANE_MELODY, 0, context->span, melody, 2) != 1 ||
        !harmony_find_key(plan, target, context->owner, context->span, &context->key) ||
        !harmony_read_lane(plan, context->span, context->key.mode, context->chords, &context->count,
                           &context->metre) ||
        !melody_ticks(context->span.length, &context->length) ||
        !melody_ticks(context->metre.cell_length, &context->cell) || context->cell <= 0) {
        return false;
    }
    /* The harmony is written: no chord span under the melody is still open. */
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, context->span, ids, HARMONY_MAX_CHORDS);
    if (found != context->count || context->count == 0) {
        return false;
    }
    for (size_t i = 0; i < context->count; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !melody_chord_mask(&context->key, &context->chords[i], &context->chord_mask[i],
                               &context->chord_seventh[i])) {
            return false;
        }
        context->chord_start[i] = cells * context->cell;
        cells += (long)context->chords[i].cells;
    }
    context->chord_start[context->count] = cells * context->cell;
    if (context->chord_start[context->count] != context->length) {
        return false;
    }
    PlanConstraint cadence;
    if (harmony_find_constraint(plan, ids[context->count - 1], context->owner, PLAN_CONSTRAINT_CADENCE, &cadence)) {
        context->has_cadence = true;
        context->cadence = cadence.params.cadence;
    }
    return read_bass(plan, context) && read_motive(plan, context);
}
