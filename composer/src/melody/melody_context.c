#include "melody/melody_internal.h"

#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/motive_tones.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/seam_hear.h>

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

/* The motive_tones constraint of a node, or NULL. */
static const PlanMotiveTones *node_tones(const PiecePlan *plan, NodeId node) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_TONES) {
            return &constraint->params.motive_tones;
        }
    }
    return NULL;
}

/* The tone of a label of the plan: MELODY_TONE_CHORD, MELODY_TONE_NONE or the NonChordTone it names. */
static int tone_of_label(const char *label) {
    NonChordTone type;
    if (strcmp(label, "chord") == 0) {
        return MELODY_TONE_CHORD;
    }
    return motive_tone_type_parse(label, &type) ? (int)type : MELODY_TONE_NONE;
}

/*
 * The tones of the motive node: its own motive_tones, else those of the motive it was made from (its motive_ref) when the
 * transform keeps the notes (a retrograde reverses the labels). A motive with a different number of notes than labels
 * has none.
 */
static void read_tones(const PiecePlan *plan, NodeId node, MelodyContext *context) {
    bool reversed = false;
    context->motive_tone_count = 0;
    for (size_t depth = 0; depth < 16; depth++) {
        const PlanMotiveTones *tones = node_tones(plan, node);
        if (tones) {
            if (tones->label_count != context->motive_count) {
                return;
            }
            for (size_t k = 0; k < tones->label_count; k++) {
                context->motive_tone[k] = tone_of_label(tones->labels[reversed ? tones->label_count - 1 - k : k]);
            }
            context->motive_tone_count = tones->label_count;
            return;
        }
        const PlanMotiveRef *source = NULL;
        for (size_t i = 0; !source && i < plan_node_constraint_count(plan, node); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
            source = constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF ? &constraint->params.motive : NULL;
        }
        if (!source || !plan_has_node(plan, source->motive)) {
            return;
        }
        switch (source->transform) {
        case PLAN_TRANSFORM_NONE:
        case PLAN_TRANSFORM_TRANSPOSED:
        case PLAN_TRANSFORM_INVERTED:
        case PLAN_TRANSFORM_AUGMENTED:
        case PLAN_TRANSFORM_DIMINISHED:
        case PLAN_TRANSFORM_RESTATED:
        case PLAN_TRANSFORM_INTERVAL_CHANGED:
            break;
        case PLAN_TRANSFORM_RETROGRADE:
            reversed = !reversed;
            break;
        default:
            return;
        }
        node = source->motive;
    }
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
    read_tones(plan, constraint.params.motive.motive, context);
    return true;
}

bool melody_phrase_read(const PiecePlan *plan, NodeId owner, PlanSpan span, PlanKey key, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t cells = 0;
    long offset;
    memset(context, 0, sizeof(*context));
    context->owner = owner;
    context->span = span;
    context->key = key;
    if (!harmony_read_lane(plan, span, key.mode, context->chords, &context->count, &context->metre) ||
        !melody_ticks(span.length, &context->length) || !melody_ticks(context->metre.cell_length, &context->cell) ||
        context->cell <= 0 || !melody_ticks(context->metre.offset, &offset)) {
        return false;
    }
    /* The strong beats are the barline grid's: a span that starts between two cell boundaries has a lead. */
    context->phase = offset % context->cell;
    /* The harmony is written: no chord span under the melody is still open. */
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, HARMONY_MAX_CHORDS);
    if (found != context->count || context->count == 0) {
        return false;
    }
    for (size_t i = 0; i < context->count; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !melody_chord_mask(&context->key, &context->chords[i], &context->chord_mask[i],
                               &context->chord_seventh[i])) {
            return false;
        }
        PlanTime start;
        PlanTime length;
        long chord_length;
        if (!harmony_cells_of_span(&context->metre, cells, context->chords[i].cells, &start, &length) ||
            !melody_ticks(start, &context->chord_start[i]) || !melody_ticks(length, &chord_length) ||
            chord_length <= 0) {
            return false;
        }
        cells += context->chords[i].cells;
    }
    context->chord_start[context->count] = context->length;
    if (cells != context->metre.cells) {
        return false;
    }
    PlanConstraint cadence;
    if (harmony_find_constraint(plan, ids[context->count - 1], owner, PLAN_CONSTRAINT_CADENCE, &cadence)) {
        context->has_cadence = true;
        context->cadence = cadence.params.cadence;
    }
    return read_bass(plan, context);
}

/* ---------------------------------------------------------- idea targets */

static PlanTime time_minus(PlanTime a, PlanTime b) {
    PlanTime out = {0, 1};
    if (plan_time_make(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

/* The nearest ancestor of node (itself included) that is a phrase. */
static NodeId phrase_of(const PiecePlan *plan, NodeId node) {
    for (; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (plan_node_kind(plan, node) == PLAN_NODE_PHRASE) {
            return node;
        }
    }
    return PLAN_NODE_NONE;
}

/* The bass notes under the target, from its start, the first and last clipped to it. */
static bool read_idea_bass(const PiecePlan *plan, MelodyContext *context) {
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
            if (end <= 0 || onset >= context->length) {
                continue;
            }
            onset = onset < 0 ? 0 : onset;
            end = end > context->length ? context->length : end;
            if (context->bass_count >= MELODY_MAX_BASS || onset != at) {
                return false;
            }
            context->bass[context->bass_count].onset = onset;
            context->bass[context->bass_count].end = end;
            context->bass[context->bass_count].pitch = notes->notes[n].midi_pitch;
            context->bass[context->bass_count].chord = melody_chord_at(context, onset);
            context->bass_count++;
            at = end;
        }
    }
    return at == context->length;
}

/* The chords under the target: the written harmony spans that overlap it, the first and last clipped to it. */
static bool read_idea_chords(const PiecePlan *plan, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, context->span, ids, HARMONY_MAX_CHORDS);
    long origin;
    long position = 0;
    if (found == 0 || found > HARMONY_MAX_CHORDS || !melody_ticks(context->span.start, &origin)) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan chord_span = plan_lane_span_span(plan, ids[i]);
        long start;
        long length;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED || !melody_ticks(chord_span.start, &start) ||
            !melody_ticks(chord_span.length, &length) ||
            !harmony_read_chord(plan, ids[i], context->key.mode, context->metre.cell_length, &context->chords[i]) ||
            !melody_chord_mask(&context->key, &context->chords[i], &context->chord_mask[i],
                               &context->chord_seventh[i])) {
            return false;
        }
        start -= origin;
        /* The chords run on from the start of the target to its end without a gap. */
        if ((i == 0 ? start > 0 : start != position)) {
            return false;
        }
        context->chord_start[i] = start < 0 ? 0 : start;
        position = start + length;
    }
    context->count = found;
    context->chord_start[found] = context->length;
    return position >= context->length;
}

/* The pitches of the melody written right before the target, and the highest one written in its phrase. */
static bool read_idea_melody(const PiecePlan *plan, MelodyContext *context, NodeId phrase) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    PlanSpan phrase_span = phrase == PLAN_NODE_NONE ? context->span : plan_node_span(plan, phrase);
    long origin;
    long start;
    if (!melody_ticks(context->span.start, &origin) || !melody_ticks(phrase_span.start, &start) ||
        !melody_ticks(phrase_span.length, &context->phrase_length)) {
        return false;
    }
    context->phrase_start = start - origin;

    /* Ideas are written in time order: every melody span of the phrase before the target is written. */
    if (plan_time_compare(phrase_span.start, context->span.start) < 0) {
        PlanSpan before = {phrase_span.start, time_minus(context->span.start, phrase_span.start)};
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, before, ids, HARMONY_MAX_CHORDS);
        if (found > HARMONY_MAX_CHORDS) {
            return false;
        }
        for (size_t i = 0; i < found; i++) {
            if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
                return false;
            }
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
            context->phrase_before_notes += notes->note_count;
            for (size_t n = 0; n < notes->note_count; n++) {
                int pitch = notes->notes[n].midi_pitch;
                context->phrase_top = !context->phrase_has_top || pitch > context->phrase_top ? pitch : context->phrase_top;
                context->phrase_has_top = true;
            }
        }
    }

    /* The last notes of the span that ends where the target starts. */
    PlanTime tick;
    if (origin > 0 && melody_time(1, &tick) == PLAN_OK) {
        PlanSpan last_tick = {time_minus(context->span.start, tick), tick};
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, last_tick, ids, HARMONY_MAX_CHORDS);
        if (found == 1 && plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
            size_t take = notes->note_count < 2 ? notes->note_count : 2;
            /* A Given line is the user's: only the note the target joins is asked of, not the line's own rules. */
            take = plan_lane_span_state(plan, ids[0]) == PLAN_STATE_GIVEN && take > 1 ? 1 : take;
            for (size_t k = 0; k < take; k++) {
                context->lead_in[k] = notes->notes[notes->note_count - take + k].midi_pitch;
            }
            context->lead_in_count = take;
        }
    }
    context->seam_bass = -1;
    context->seam_next = -1;
    /* The first note of the melody that starts where the target ends: the target's last note leads to it. */
    PlanTime end;
    PlanTime tick_after;
    if (plan_span_end(context->span, &end) == PLAN_OK && melody_time(1, &tick_after) == PLAN_OK) {
        PlanSpan first_tick = {end, tick_after};
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, first_tick, ids, HARMONY_MAX_CHORDS);
        /* A motive's melody (m0 in the material, which starts where the piece ends) is not melody of the piece. */
        if (found == 1 && plan_time_compare(plan_lane_span_span(plan, ids[0]).start, end) == 0 &&
            plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED &&
            plan_node_kind(plan, plan_lane_span_owner(plan, ids[0])) != PLAN_NODE_MOTIVE) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
            if (notes->note_count > 0 && notes->notes[0].offset.num == 0) {
                context->lead_out = notes->notes[0].midi_pitch;
                context->has_lead_out = true;
                context->seam_next = notes->note_count > 1 ? notes->notes[1].midi_pitch : -1;
            }
        }
        context->at_seam = context->has_lead_out && seam_section_at(plan, end) != PLAN_NODE_NONE;
        if (context->at_seam) {
            LaneSpanId bass[2];
            if (plan_lane_find(plan, PLAN_LANE_BASS, 0, first_tick, bass, 2) >= 1 &&
                plan_lane_span_payload(plan, bass[0])->notes.note_count > 0) {
                context->seam_bass = plan_lane_span_payload(plan, bass[0])->notes.notes[0].midi_pitch;
            }
        }
    }
    return true;
}

/* Melody spans looked through for the one a target restates. */
#define MELODY_MAX_RESTATED 16

/* The motive a motive node is made from, from its own motive_ref; PLAN_NODE_NONE when it has none. */
static NodeId motive_source(const PiecePlan *plan, NodeId node) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return constraint->params.motive.motive;
        }
    }
    return PLAN_NODE_NONE;
}

/* The motive a chain of motive nodes starts from: the node itself when it has no motive_ref. */
static NodeId motive_root(const PiecePlan *plan, NodeId node) {
    for (size_t depth = 0; depth < MELODY_MAX_RESTATED; depth++) {
        NodeId source = motive_source(plan, node);
        if (source == PLAN_NODE_NONE || !plan_has_node(plan, source)) {
            break;
        }
        node = source;
    }
    return node;
}

/* Whether the melody span asks for a motive made from the same motive as the node (not an idea with a motive of its own). */
static bool span_from_root(const PiecePlan *plan, LaneSpanId span, NodeId root) {
    for (size_t k = 0; k < plan_lane_span_constraint_count(plan, span); k++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, span, k);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && plan_has_node(plan, constraint->params.motive.motive)) {
            return motive_root(plan, constraint->params.motive.motive) == root;
        }
    }
    return false;
}

/*
 * The written melody span, as long as the target and before it, that the target restates: the latest one that asks
 * for the motive the target's own motive is a restatement of, else the one right before when it asks for a motive made
 * from the same motive (an idea with a motive of its own restates that motive, not the idea before). False when there
 * is none.
 */
static bool restated_span(const PiecePlan *plan, const MelodyContext *context, LaneSpanId *out) {
    LaneSpanId before[MELODY_MAX_RESTATED];
    PlanTime tick;
    long origin;
    if (!melody_ticks(context->span.start, &origin) || origin <= 0 || melody_time(1, &tick) != PLAN_OK) {
        return false;
    }
    NodeId source = motive_source(plan, context->motive_node);
    PlanSpan earlier = {{0, 1}, context->span.start};
    size_t found = source == PLAN_NODE_NONE ? 0 : plan_lane_find(plan, PLAN_LANE_MELODY, 0, earlier, before, MELODY_MAX_RESTATED);
    for (size_t i = found; i > 0 && found <= MELODY_MAX_RESTATED; i--) {
        LaneSpanId id = before[i - 1];
        for (size_t k = 0; plan_lane_span_state(plan, id) != PLAN_STATE_CONSTRAINED &&
                           plan_time_compare(plan_lane_span_span(plan, id).length, context->span.length) == 0 &&
                           k < plan_lane_span_constraint_count(plan, id);
             k++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, k);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == source &&
                constraint->params.motive.transform == PLAN_TRANSFORM_NONE) {
                *out = id;
                return true;
            }
        }
    }
    PlanSpan last_tick = {time_minus(context->span.start, tick), tick};
    return plan_lane_find(plan, PLAN_LANE_MELODY, 0, last_tick, before, 2) == 1 &&
           plan_lane_span_state(plan, before[0]) != PLAN_STATE_CONSTRAINED &&
           span_from_root(plan, before[0], motive_root(plan, context->motive_node)) &&
           plan_time_compare(plan_lane_span_span(plan, before[0]).length, context->span.length) == 0 &&
           ((*out = before[0]), true);
}

/*
 * An idea that restates another, exactly (a RESTATED motive_ref), has that idea's melody for its motive, all of it,
 * so the two are heard as a repetition: the same intervals and the same rhythm, as far as the chords allow. The idea
 * restated is the basic idea the motive comes from, however far back it is, else the idea right before. Left alone
 * when it is not written or is not as long.
 */
static void restate_previous(const PiecePlan *plan, MelodyContext *context) {
    LaneSpanId before;
    if (context->transform != PLAN_TRANSFORM_RESTATED || !restated_span(plan, context, &before)) {
        return;
    }
    const PlanNotes *notes = &plan_lane_span_payload(plan, before)->notes;
    if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
        return;
    }
    long onset[MOTIVE_MAX_NOTES];
    long duration[MOTIVE_MAX_NOTES];
    for (size_t i = 0; i < notes->note_count; i++) {
        if (!melody_ticks(notes->notes[i].offset, &onset[i]) || !melody_ticks(notes->notes[i].duration, &duration[i]) ||
            duration[i] <= 0 || (i > 0 && onset[i] < onset[i - 1] + duration[i - 1])) {
            return;
        }
    }
    for (size_t i = 0; i < notes->note_count; i++) {
        context->motive_pitch[i] = notes->notes[i].midi_pitch;
        context->motive_onset[i] = onset[i];
        context->motive_duration[i] = duration[i];
    }
    context->motive_count = notes->note_count;
    context->motive_length = onset[notes->note_count - 1] + duration[notes->note_count - 1];
    /* Its first notes are the motive's, which keep their tones; a melody with fewer notes than labels has none. */
    if (context->motive_tone_count > notes->note_count) {
        context->motive_tone_count = 0;
    }
    context->motive_given = false;
    context->restating = true;
}

/* A target that stands for an idea or a subphrase: it is one of several melody spans in its phrase. */
static bool melody_idea_read(const PiecePlan *plan, LaneSpanId target, MelodyContext *context) {
    NodeId owner = plan_lane_span_owner(plan, target);
    PlanSpan span = plan_lane_span_span(plan, target);
    LaneSpanId meters[2];
    PlanConstraint constraint;
    long cell_phase;
    memset(context, 0, sizeof(*context));
    context->idea = true;
    context->target = target;
    context->owner = owner;
    context->span = span;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, meters, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, meters[0]), span) ||
        !harmony_find_key(plan, target, owner, span, &context->key)) {
        return false;
    }
    PlanSpan meter_span = plan_lane_span_span(plan, meters[0]);
    PhraseMetre *metre = &context->metre;
    long origin;
    long meter_start;
    metre->meter = plan_lane_span_payload(plan, meters[0])->meter;
    if (plan_time_make(metre->meter.beats_per_bar, (unsigned long)metre->meter.beat_unit, &metre->bar_length) !=
            PLAN_OK ||
        !harmony_rhythm_cells_per_bar(&metre->meter, &metre->cells_per_bar) ||
        plan_time_make(metre->bar_length.num, metre->bar_length.den * metre->cells_per_bar, &metre->cell_length) !=
            PLAN_OK ||
        !melody_ticks(span.length, &context->length) || !melody_ticks(metre->cell_length, &context->cell) ||
        context->cell <= 0 || context->length <= 0 || !melody_ticks(span.start, &origin) ||
        !melody_ticks(meter_span.start, &meter_start)) {
        return false;
    }
    cell_phase = (origin - meter_start) % context->cell;
    context->phase = cell_phase < 0 ? cell_phase + context->cell : cell_phase;
    NodeId phrase = phrase_of(plan, owner);

    /* A cadence is asked for by the target, or by the idea it stands for; the chords' own cadence is the phrase's. */
    if (harmony_find_constraint(plan, target, owner, PLAN_CONSTRAINT_CADENCE, &constraint)) {
        context->has_cadence = true;
        context->cadence = constraint.params.cadence;
    }
    return read_idea_chords(plan, context) && read_idea_bass(plan, context) &&
           read_idea_melody(plan, context, phrase) && read_motive(plan, context) &&
           (restate_previous(plan, context), true);
}

/*
 * The anacrusis before a phrase that starts `pickup` after `start`: shorter than a bar, a whole number of ticks, and over
 * the chord of the harmony span that sounds there, when one is written (none starts a plan).
 */
static bool read_pickup(const PiecePlan *plan, PlanTime start, PlanTime pickup, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    PlanSpan under = {start, pickup};
    long ticks;
    if (!melody_ticks(pickup, &ticks) || ticks <= 0 || plan_time_compare(pickup, context->metre.bar_length) >= 0) {
        return false;
    }
    context->pickup = ticks;
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, under, ids, HARMONY_MAX_CHORDS);
    if (found > HARMONY_MAX_CHORDS) {
        return false;
    }
    if (found > 0 && plan_lane_span_state(plan, ids[found - 1]) != PLAN_STATE_CONSTRAINED) {
        PhraseChord chord;
        unsigned seventh;
        if (!harmony_read_chord(plan, ids[found - 1], context->key.mode, context->metre.cell_length, &chord) ||
            !melody_chord_mask(&context->key, &chord, &context->pickup_mask, &seventh)) {
            return false;
        }
    }
    return true;
}

bool melody_context_read(const PiecePlan *plan, LaneSpanId target, MelodyContext *out) {
    if (plan && plan_has_lane_span(plan, target) && plan_lane_span_lane(plan, target) == PLAN_LANE_MELODY &&
        plan_lane_span_voice(plan, target) == 0 && plan_lane_span_state(plan, target) == PLAN_STATE_CONSTRAINED &&
        out && (plan_node_kind(plan, plan_lane_span_owner(plan, target)) == PLAN_NODE_IDEA ||
                plan_node_kind(plan, plan_lane_span_owner(plan, target)) == PLAN_NODE_SUBPHRASE)) {
        return melody_idea_read(plan, target, out);
    }
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_MELODY ||
        plan_lane_span_voice(plan, target) != 0 || plan_lane_span_state(plan, target) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    NodeId owner = plan_lane_span_owner(plan, target);
    PlanSpan span = plan_lane_span_span(plan, target);
    PlanSpan body = span;
    PlanConstraint pickup;
    LaneSpanId melody[2];
    PlanKey key;
    bool has_pickup = harmony_find_constraint(plan, target, owner, PLAN_CONSTRAINT_PICKUP, &pickup);

    /* A pickup is the start of the span: the harmony, the key and the notes after it are the phrase's. */
    if (has_pickup && (plan_time_add(span.start, pickup.params.pickup, &body.start) != PLAN_OK ||
                       plan_time_make(span.length.num * (long)pickup.params.pickup.den -
                                          pickup.params.pickup.num * (long)span.length.den,
                                      span.length.den * pickup.params.pickup.den, &body.length) != PLAN_OK ||
                       body.length.num <= 0)) {
        return false;
    }
    /* The target is the only melody in the phrase. */
    if (plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, melody, 2) != 1 ||
        !harmony_find_key(plan, target, owner, body, &key) || !melody_phrase_read(plan, owner, body, key, out)) {
        return false;
    }
    out->target = target;
    return (!has_pickup || read_pickup(plan, span.start, pickup.params.pickup, out)) && read_motive(plan, out);
}
