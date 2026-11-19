#include "melody/melody_internal.h"

#include <stdlib.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/motive_tones.h>
#include <plan/plan_state.h>
#include <ursatz_composer/seam_hear.h>
#include "structure/plan_util.h"
#include "util/util.h"

void melody_context_zero(MelodyContext *context) {
    memset(context, 0, sizeof(*context));
    context->length = POSITION_ZERO;
    context->cell = POSITION_ZERO;
    context->pickup = POSITION_ZERO;
    context->motive_length = POSITION_ZERO;
    context->phase = POSITION_ZERO;
    context->phrase_start = POSITION_ZERO;
    context->phrase_length = POSITION_ZERO;
    for (size_t i = 0; i <= HARMONY_MAX_CHORDS; i++) {
        context->chord_start[i] = POSITION_ZERO;
    }
    for (size_t i = 0; i < MOTIVE_MAX_NOTES; i++) {
        context->motive_onset[i] = POSITION_ZERO;
        context->motive_duration[i] = POSITION_ZERO;
        context->motive_note_id[i] = PLAN_NOTE_NONE;
    }
    for (size_t i = 0; i < MELODY_BAR_NOTES; i++) {
        context->repeat_onset[i] = POSITION_ZERO;
        context->repeat_duration[i] = POSITION_ZERO;
    }
}

/* The length of a note the probes look through: the shortest note the operator writes. */
static bool probe_length(Rational *out) {
    return rational_make(1, MELODY_FINEST_NOTE_DENOMINATOR, out) == RATIONAL_OK;
}

size_t melody_chord_at(const MelodyContext *context, Rational position) {
    size_t chord = 0;
    for (size_t i = 1; i < context->count; i++) {
        if (rational_cmp(context->chord_start[i], position) <= 0) {
            chord = i;
        }
    }
    return chord;
}

unsigned melody_need_mask(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *note) {
    unsigned mask = ~0u;
    Rational end = position_add(note->onset, note->duration);
    /* A suspension is held into a chord it is not a tone of. */
    if (params->nct_types & MELODY_NCT_SUSPENSION) {
        return context->chord_mask[melody_chord_at(context, note->onset)];
    }
    for (size_t c = melody_chord_at(context, note->onset); c < context->count && rational_cmp(context->chord_start[c], end) < 0; c++) {
        mask &= context->chord_mask[c];
    }
    return mask & 0xFFFu;
}

/* The bass lane over the phrase: every note, from the start of the phrase, tiling it. */
static bool read_bass(const Plan *plan, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, context->span, ids, HARMONY_MAX_CHORDS);
    Rational origin = context->span.start;
    Rational at = POSITION_ZERO;
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        Rational base;
        const PlanNotes *notes;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !plan_span_contains(context->span, plan_lane_span_span(plan, ids[i]))) {
            return false;
        }
        base = plan_lane_span_span(plan, ids[i]).start;
        notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational offset = notes->notes[n].offset;
            Rational duration = notes->notes[n].duration;
            if (context->bass_count >= MELODY_MAX_BASS || !position_is_positive(duration) ||
                rational_cmp(position_add(position_sub(base, origin), offset), at) != 0) {
                return false;
            }
            context->bass[context->bass_count].onset = at;
            context->bass[context->bass_count].end = position_add(at, duration);
            context->bass[context->bass_count].pitch = notes->notes[n].midi;
            context->bass[context->bass_count].chord = melody_chord_at(context, at);
            context->bass_count++;
            at = position_add(at, duration);
        }
    }
    return rational_cmp(at, context->length) == 0;
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
static void read_tones(const Plan *plan, NodeId node, MelodyContext *context) {
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
        if (source->has_step) {
            switch (source->step.kind) {
            case MOTIVE_RELATION_TRANSPOSE:
            case MOTIVE_RELATION_INVERT:
            case MOTIVE_RELATION_AUGMENT:
            case MOTIVE_RELATION_DIMINISH:
            case MOTIVE_RELATION_EXACT:
            case MOTIVE_RELATION_INTERVAL_CHANGE:
            case MOTIVE_RELATION_DOMINANT_FORM: /* the notes keep their roles in the dominant (R-209) */
                break;
            case MOTIVE_RELATION_RETROGRADE:
                reversed = !reversed;
                break;
            default:
                return;
            }
        }
        node = source->motive;
    }
}

/* The motive named by the span's motive_ref constraint. */
static bool read_motive(const Plan *plan, MelodyContext *context) {
    PlanConstraint constraint;
    LaneSpanId span;
    if (!harmony_find_constraint(plan, context->target, context->owner, PLAN_CONSTRAINT_MOTIVE_REF, &constraint) ||
        !plan_has_node(plan, constraint.params.motive.motive) ||
        !plan_melody_span_of(plan, constraint.params.motive.motive, &span)) {
        return false;
    }
    const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
    if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
        return false;
    }
    context->motive_node = constraint.params.motive.motive;
    context->restated = plan_motive_ref_restates(&constraint.params.motive);
    /* A residue: the span's ref, or the motive node's own, names the liquidation's steps. */
    context->liquidated = constraint.params.motive.steps > 0;
    for (size_t i = 0; !context->liquidated && i < plan_node_constraint_count(plan, context->motive_node); i++) {
        const PlanConstraint *own = plan_node_constraint_at(plan, context->motive_node, i);
        context->liquidated = own->kind == PLAN_CONSTRAINT_MOTIVE_REF && own->params.motive.steps > 0;
    }
    context->motive_given = plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN;
    context->motive_count = notes->note_count;
    context->motive_length = POSITION_ZERO;
    for (size_t i = 0; i < notes->note_count; i++) {
        context->motive_onset[i] = notes->notes[i].offset;
        context->motive_duration[i] = notes->notes[i].duration;
        context->motive_note_id[i] = notes->notes[i].id;
        if (!position_is_positive(context->motive_duration[i]) || notes->notes[i].midi < 0 ||
            notes->notes[i].midi > 127) {
            return false;
        }
        context->motive_pitch[i] = notes->notes[i].midi;
        if (i > 0 && rational_cmp(context->motive_onset[i],
                                  position_add(context->motive_onset[i - 1], context->motive_duration[i - 1])) < 0) {
            return false;
        }
        context->motive_length = position_add(context->motive_onset[i], context->motive_duration[i]);
    }
    read_tones(plan, constraint.params.motive.motive, context);
    return true;
}

/* The target's cadence is the last of its section: the span ends where the section node it is in ends (R-478). */
static bool ends_its_section(const Plan *plan, NodeId owner, PlanSpan span) {
    Rational end;
    if (plan_span_end(span, &end) != PLAN_OK) {
        return false;
    }
    for (NodeId node = owner; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (plan_node_kind(plan, node) == PLAN_NODE_SECTION) {
            Rational section_end;
            return plan_span_end(plan_node_span(plan, node), &section_end) == PLAN_OK &&
                   rational_cmp(end, section_end) == 0;
        }
    }
    return false;
}

bool melody_phrase_read(const Plan *plan, NodeId owner, PlanSpan span, SpelledKey key, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t cells = 0;
    melody_context_zero(context);
    context->owner = owner;
    context->span = span;
    context->key = key;
    if (!harmony_read_lane(plan, span, harmony_key_mode(&key), context->chords, &context->count, &context->metre)) {
        return false;
    }
    context->length = span.length;
    context->cell = context->metre.cell_length;
    if (!position_is_positive(context->cell)) {
        return false;
    }
    /* The strong beats are the barline grid's: a span that starts between two cell boundaries has a lead. */
    context->phase = position_remainder(context->metre.offset, context->cell);
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
        Rational length;
        if (!harmony_cells_of_span(&context->metre, cells, context->chords[i].cells, &context->chord_start[i], &length) ||
            !position_is_positive(length)) {
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
        context->last_cadence = ends_its_section(plan, owner, span);
    }
    return read_bass(plan, context);
}

/* ---------------------------------------------------------- idea targets */

/* The nearest ancestor of node (itself included) that is a phrase. */
static NodeId phrase_of(const Plan *plan, NodeId node) {
    for (; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (plan_node_kind(plan, node) == PLAN_NODE_PHRASE) {
            return node;
        }
    }
    return PLAN_NODE_NONE;
}

/* The bass notes under the target, from its start, the first and last clipped to it. */
static bool read_idea_bass(const Plan *plan, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, context->span, ids, HARMONY_MAX_CHORDS);
    Rational origin = context->span.start;
    Rational at = POSITION_ZERO;
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
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
            Rational offset = notes->notes[n].offset;
            Rational duration = notes->notes[n].duration;
            if (!position_is_positive(duration)) {
                return false;
            }
            Rational onset = position_add(position_sub(base, origin), offset);
            Rational end = position_add(onset, duration);
            if (rational_cmp(end, POSITION_ZERO) <= 0 || rational_cmp(onset, context->length) >= 0) {
                continue;
            }
            onset = rational_cmp(onset, POSITION_ZERO) < 0 ? POSITION_ZERO : onset;
            end = rational_cmp(end, context->length) > 0 ? context->length : end;
            if (context->bass_count >= MELODY_MAX_BASS || rational_cmp(onset, at) != 0) {
                return false;
            }
            context->bass[context->bass_count].onset = onset;
            context->bass[context->bass_count].end = end;
            context->bass[context->bass_count].pitch = notes->notes[n].midi;
            context->bass[context->bass_count].chord = melody_chord_at(context, onset);
            context->bass_count++;
            at = end;
        }
    }
    return rational_cmp(at, context->length) == 0;
}

/* The chords under the target: the written harmony spans that overlap it, the first and last clipped to it. */
static bool read_idea_chords(const Plan *plan, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, context->span, ids, HARMONY_MAX_CHORDS);
    Rational origin = context->span.start;
    Rational position = POSITION_ZERO;
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan chord_span = plan_lane_span_span(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !harmony_read_chord(plan, ids[i], harmony_key_mode(&context->key), context->metre.cell_length, &context->chords[i]) ||
            !melody_chord_mask(&context->key, &context->chords[i], &context->chord_mask[i],
                               &context->chord_seventh[i])) {
            return false;
        }
        Rational start = position_sub(chord_span.start, origin);
        /* The chords run on from the start of the target to its end without a gap. */
        if (i == 0 ? rational_cmp(start, POSITION_ZERO) > 0 : rational_cmp(start, position) != 0) {
            return false;
        }
        context->chord_start[i] = rational_cmp(start, POSITION_ZERO) < 0 ? POSITION_ZERO : start;
        position = position_add(start, chord_span.length);
    }
    context->count = found;
    context->chord_start[found] = context->length;
    return rational_cmp(position, context->length) >= 0;
}

/* The k-th smallest pitch (from 0) of a written span's notes, by counting, so it needs no storage. */
static int kth_pitch(const PlanNotes *notes, size_t k) {
    for (size_t i = 0; i < notes->note_count; i++) {
        size_t below = 0;
        size_t equal = 0;
        for (size_t j = 0; j < notes->note_count; j++) {
            below += notes->notes[j].midi < notes->notes[i].midi;
            equal += notes->notes[j].midi == notes->notes[i].midi;
        }
        if (below <= k && k < below + equal) {
            return notes->notes[i].midi;
        }
    }
    return 0;
}

/* The median pitch of a written span's notes (the mean of the middle two for an even count); 0 for none. */
static int median_pitch(const PlanNotes *notes) {
    size_t count = notes->note_count;
    return count == 0 ? 0 : (kth_pitch(notes, (count - 1) / 2) + kth_pitch(notes, count / 2)) / 2;
}

/* The pitches of the melody written right before the target, and the highest one written in its phrase. */
static bool read_idea_melody(const Plan *plan, MelodyContext *context, NodeId phrase) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    PlanSpan phrase_span = phrase == PLAN_NODE_NONE ? context->span : plan_node_span(plan, phrase);
    Rational origin = context->span.start;
    context->phrase_length = phrase_span.length;
    context->phrase_start = position_sub(phrase_span.start, origin);

    /* Ideas are written in time order: every melody span of the phrase before the target is written. */
    if (rational_cmp(phrase_span.start, context->span.start) < 0) {
        PlanSpan before = {phrase_span.start, {0, 1}};
        if (rational_sub(context->span.start, phrase_span.start, &before.length) != RATIONAL_OK) {
            return false;
        }
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
                int pitch = notes->notes[n].midi;
                context->phrase_top = !context->phrase_has_top || pitch > context->phrase_top ? pitch : context->phrase_top;
                context->phrase_has_top = true;
            }
        }
    }

    /* The last notes of the span that ends where the target starts. */
    Rational tick;
    if (rational_cmp(origin, POSITION_ZERO) > 0 && probe_length(&tick)) {
        PlanSpan last_tick = {{0, 1}, tick};
        if (rational_sub(context->span.start, tick, &last_tick.start) != RATIONAL_OK) {
            return false;
        }
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, last_tick, ids, HARMONY_MAX_CHORDS);
        if (found == 1 && plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
            size_t take = notes->note_count < 2 ? notes->note_count : 2;
            /* A Given line is the user's: only the note the target joins is asked of, not the line's own rules. */
            take = plan_lane_span_state(plan, ids[0]) == PLAN_STATE_GIVEN && take > 1 ? 1 : take;
            for (size_t k = 0; k < take; k++) {
                context->lead_in[k] = notes->notes[notes->note_count - take + k].midi;
            }
            context->lead_in_count = take;
            context->before_register = median_pitch(notes);
            context->has_before_register = notes->note_count > 0;
        }
    }
    context->seam_bass = -1;
    context->seam_next = -1;
    /* The first note of the melody that starts where the target ends: the target's last note leads to it. */
    Rational end;
    Rational tick_after;
    if (plan_span_end(context->span, &end) == PLAN_OK && probe_length(&tick_after)) {
        PlanSpan first_tick = {end, tick_after};
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, first_tick, ids, HARMONY_MAX_CHORDS);
        /* A motive's melody (m0 in the material, which starts where the piece ends) is not melody of the piece. */
        if (found == 1 && rational_cmp(plan_lane_span_span(plan, ids[0]).start, end) == 0 &&
            plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED &&
            plan_node_kind(plan, plan_lane_span_owner(plan, ids[0])) != PLAN_NODE_MOTIVE) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
            if (notes->note_count > 0 && notes->notes[0].offset.num == 0) {
                context->lead_out = notes->notes[0].midi;
                context->has_lead_out = true;
                context->seam_next = notes->note_count > 1 ? notes->notes[1].midi : -1;
            }
        }
        context->at_seam = context->has_lead_out && seam_section_at(plan, end) != PLAN_NODE_NONE;
        if (context->at_seam) {
            LaneSpanId bass[2];
            if (plan_lane_find(plan, PLAN_LANE_BASS, 0, first_tick, bass, 2) >= 1 &&
                plan_lane_span_payload(plan, bass[0])->notes.note_count > 0) {
                context->seam_bass = plan_lane_span_payload(plan, bass[0])->notes.notes[0].midi;
            }
        }
    }
    return true;
}

/* Melody spans looked through for the one a target restates. */
#define MELODY_MAX_RESTATED 16

/* The motive a motive node is made from, from its own motive_ref; PLAN_NODE_NONE when it has none. */
static NodeId motive_source(const Plan *plan, NodeId node) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return constraint->params.motive.motive;
        }
    }
    return PLAN_NODE_NONE;
}

/* The motive a chain of motive nodes starts from: the node itself when it has no motive_ref. */
static NodeId motive_root(const Plan *plan, NodeId node) {
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
static bool span_from_root(const Plan *plan, LaneSpanId span, NodeId root) {
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
static bool restated_span(const Plan *plan, const MelodyContext *context, LaneSpanId *out) {
    LaneSpanId before[MELODY_MAX_RESTATED];
    Rational tick;
    if (rational_cmp(context->span.start, POSITION_ZERO) <= 0 || !probe_length(&tick)) {
        return false;
    }
    NodeId source = motive_source(plan, context->motive_node);
    PlanSpan earlier = {{0, 1}, context->span.start};
    size_t found = source == PLAN_NODE_NONE ? 0 : plan_lane_find(plan, PLAN_LANE_MELODY, 0, earlier, before, MELODY_MAX_RESTATED);
    for (size_t i = found; i > 0 && found <= MELODY_MAX_RESTATED; i--) {
        LaneSpanId id = before[i - 1];
        for (size_t k = 0; plan_lane_span_state(plan, id) != PLAN_STATE_CONSTRAINED &&
                           rational_cmp(plan_lane_span_span(plan, id).length, context->span.length) == 0 &&
                           k < plan_lane_span_constraint_count(plan, id);
             k++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, k);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == source &&
                !constraint->params.motive.has_step) {
                *out = id;
                return true;
            }
        }
    }
    PlanSpan last_tick = {{0, 1}, tick};
    if (rational_sub(context->span.start, tick, &last_tick.start) != RATIONAL_OK) {
        return false;
    }
    return plan_lane_find(plan, PLAN_LANE_MELODY, 0, last_tick, before, 2) == 1 &&
           plan_lane_span_state(plan, before[0]) != PLAN_STATE_CONSTRAINED &&
           span_from_root(plan, before[0], motive_root(plan, context->motive_node)) &&
           rational_cmp(plan_lane_span_span(plan, before[0]).length, context->span.length) == 0 &&
           ((*out = before[0]), true);
}

/*
 * An idea that restates another, exactly (a RESTATED motive_ref), has that idea's melody for its motive, all of it,
 * so the two are heard as a repetition: the same intervals and the same rhythm, as far as the chords allow. The idea
 * restated is the basic idea the motive comes from, however far back it is, else the idea right before. Left alone
 * when it is not written or is not as long.
 */
static void restate_previous(const Plan *plan, MelodyContext *context) {
    LaneSpanId before;
    if (!context->restated || !restated_span(plan, context, &before)) {
        return;
    }
    const PlanNotes *notes = &plan_lane_span_payload(plan, before)->notes;
    if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
        return;
    }
    Rational onset[MOTIVE_MAX_NOTES];
    Rational duration[MOTIVE_MAX_NOTES];
    for (size_t i = 0; i < notes->note_count; i++) {
        onset[i] = notes->notes[i].offset;
        duration[i] = notes->notes[i].duration;
        if (!position_is_positive(duration[i]) ||
            (i > 0 && rational_cmp(onset[i], position_add(onset[i - 1], duration[i - 1])) < 0)) {
            return;
        }
    }
    for (size_t i = 0; i < notes->note_count; i++) {
        context->motive_pitch[i] = notes->notes[i].midi;
        context->motive_onset[i] = onset[i];
        context->motive_duration[i] = duration[i];
        context->motive_note_id[i] = notes->notes[i].id;
    }
    context->motive_count = notes->note_count;
    context->motive_length = position_add(onset[notes->note_count - 1], duration[notes->note_count - 1]);
    /* Its first notes are the motive's, which keep their tones; a melody with fewer notes than labels has none. */
    if (context->motive_tone_count > notes->note_count) {
        context->motive_tone_count = 0;
    }
    context->motive_given = false;
    context->restating = true;
}

/* A target that stands for an idea or a subphrase: it is one of several melody spans in its phrase. */
static bool melody_idea_read(const Plan *plan, LaneSpanId target, MelodyContext *context) {
    NodeId owner = plan_lane_span_owner(plan, target);
    PlanSpan span = plan_lane_span_span(plan, target);
    LaneSpanId meters[2];
    PlanConstraint constraint;
    melody_context_zero(context);
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
    metre->meter = plan_lane_span_payload(plan, meters[0])->meter;
    if (plan_meter_bar_length(&metre->meter, &metre->bar_length) != PLAN_OK ||
        !harmony_rhythm_cells_per_bar(&metre->meter, &metre->cells_per_bar) ||
        rational_div_int(metre->bar_length, metre->cells_per_bar, &metre->cell_length) !=
            RATIONAL_OK ||
        !position_is_positive(span.length) || !position_is_positive(metre->cell_length)) {
        return false;
    }
    context->length = span.length;
    context->cell = metre->cell_length;
    context->phase = position_remainder(position_sub(span.start, meter_span.start), context->cell);
    NodeId phrase = phrase_of(plan, owner);

    /* A cadence is asked for by the target, or by the idea it stands for; the chords' own cadence is the phrase's. */
    if (harmony_find_constraint(plan, target, owner, PLAN_CONSTRAINT_CADENCE, &constraint)) {
        context->has_cadence = true;
        context->cadence = constraint.params.cadence;
        context->last_cadence = ends_its_section(plan, owner, span);
    }
    return read_idea_chords(plan, context) && read_idea_bass(plan, context) &&
           read_idea_melody(plan, context, phrase) && read_motive(plan, context) &&
           (restate_previous(plan, context), true) && ((context->motive_waived = melody_motive_waived(context)), true);
}

/*
 * The anacrusis before a phrase that starts `pickup` after `start`: shorter than a bar, and over
 * the chord of the harmony span that sounds there, when one is written (none starts a plan).
 */
static bool read_pickup(const Plan *plan, Rational start, Rational pickup, MelodyContext *context) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    PlanSpan under = {start, pickup};
    if (!position_is_positive(pickup) || rational_cmp(pickup, context->metre.bar_length) >= 0) {
        return false;
    }
    context->pickup = pickup;
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, under, ids, HARMONY_MAX_CHORDS);
    if (found > HARMONY_MAX_CHORDS) {
        return false;
    }
    if (found > 0 && plan_lane_span_state(plan, ids[found - 1]) != PLAN_STATE_CONSTRAINED) {
        PhraseChord chord;
        unsigned seventh;
        if (!harmony_read_chord(plan, ids[found - 1], harmony_key_mode(&context->key), context->metre.cell_length, &chord) ||
            !melody_chord_mask(&context->key, &chord, &context->pickup_mask, &seventh)) {
            return false;
        }
    }
    return true;
}

bool melody_context_read(const Plan *plan, LaneSpanId target, MelodyContext *out) {
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
    SpelledKey key;
    bool has_pickup = harmony_find_constraint(plan, target, owner, PLAN_CONSTRAINT_PICKUP, &pickup);

    /* A pickup is the start of the span: the harmony, the key and the notes after it are the phrase's. */
    if (has_pickup && (rational_add(span.start, pickup.params.pickup, &body.start) != RATIONAL_OK ||
                       rational_sub(span.length, pickup.params.pickup, &body.length) != RATIONAL_OK ||
                       body.length.num <= 0)) {
        return false;
    }
    /* The target is the only melody in the phrase. */
    if (plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, melody, 2) != 1 ||
        !harmony_find_key(plan, target, owner, body, &key) || !melody_phrase_read(plan, owner, body, key, out)) {
        return false;
    }
    out->target = target;
    return (!has_pickup || read_pickup(plan, span.start, pickup.params.pickup, out)) && read_motive(plan, out) &&
           ((out->motive_waived = melody_motive_waived(out)), true);
}

/* ---------------------------------------------------------- rhythm repetition */

#define REPEAT_SPANS 128
#define REPEAT_NOTES 512

typedef struct {
    Rational onset; /* from the section's start */
    Rational duration;
} RepeatNote;

/* The notes of the written melody spans of the section that end at or before `limit`, from the section's start, by onset. */
static bool read_section_notes(const Plan *plan, PlanSpan section, Rational limit, RepeatNote *list, size_t *count) {
    LaneSpanId ids[REPEAT_SPANS];
    PlanSpan before = {section.start, limit};
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, before, ids, REPEAT_SPANS);
    *count = 0;
    if (found > REPEAT_SPANS) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        Rational base;
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED || plan_span_end(span, &end) != PLAN_OK ||
            rational_cmp(span.start, section.start) < 0 || rational_sub(span.start, section.start, &base) != RATIONAL_OK ||
            rational_cmp(position_sub(end, section.start), limit) > 0) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            if (*count >= REPEAT_NOTES) {
                return false;
            }
            list[*count].onset = position_add(base, notes->notes[n].offset);
            list[*count].duration = notes->notes[n].duration;
            (*count)++;
        }
    }
    for (size_t i = 1; i < *count; i++) {
        RepeatNote note = list[i];
        size_t k = i;
        for (; k > 0 && rational_cmp(list[k - 1].onset, note.onset) > 0; k--) {
            list[k] = list[k - 1];
        }
        list[k] = note;
    }
    return true;
}

/* The notes of the bar from `start`, as onsets and durations within the bar; false when it has more than MELODY_BAR_NOTES. */
static bool bar_rhythm(const RepeatNote *list, size_t count, Rational start, Rational bar_length, Rational *onset,
                       Rational *duration, size_t *notes) {
    Rational end = position_add(start, bar_length);
    *notes = 0;
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(list[i].onset, start) < 0 || rational_cmp(list[i].onset, end) >= 0) {
            continue;
        }
        if (*notes >= MELODY_BAR_NOTES) {
            return false;
        }
        onset[*notes] = position_sub(list[i].onset, start);
        duration[*notes] = list[i].duration;
        (*notes)++;
    }
    return true;
}

static bool same_bar_rhythm(const Rational *onset_a, const Rational *duration_a, size_t count_a, const Rational *onset_b,
                            const Rational *duration_b, size_t count_b) {
    if (count_a != count_b) {
        return false;
    }
    for (size_t i = 0; i < count_a; i++) {
        if (rational_cmp(onset_a[i], onset_b[i]) != 0 || rational_cmp(duration_a[i], duration_b[i]) != 0) {
            return false;
        }
    }
    return true;
}

bool melody_context_read_repeats(const Plan *plan, MelodyContext *context) {
    RepeatNote *list = malloc(REPEAT_NOTES * sizeof(*list));
    PlanSpan section = context->span;
    Rational bar = context->metre.bar_length;
    context->repeat_aligned = false;
    context->repeat_run = 0;
    context->repeat_count = 0;
    if (!list) {
        return false;
    }
    bool has_section = false;
    for (NodeId node = context->owner; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (plan_node_kind(plan, node) == PLAN_NODE_SECTION) {
            section = plan_node_span(plan, node);
            has_section = true;
            break;
        }
    }
    Rational lead = position_sub(context->span.start, section.start);
    size_t count = 0;
    /* Only a target that starts on a barline of its section, with no pickup, counts bars before it or in it. */
    context->repeat_aligned = has_section && !position_is_positive(context->pickup) &&
                              (rational_cmp(lead, POSITION_ZERO) == 0 || position_is_multiple(lead, bar));
    if (context->repeat_aligned && rational_cmp(lead, bar) >= 0 &&
        read_section_notes(plan, section, lead, list, &count)) {
        Rational onset[MELODY_BAR_NOTES];
        Rational duration[MELODY_BAR_NOTES];
        Rational other_onset[MELODY_BAR_NOTES];
        Rational other_duration[MELODY_BAR_NOTES];
        size_t notes = 0;
        size_t other_notes = 0;
        Rational start = position_sub(lead, bar);
        if (bar_rhythm(list, count, start, bar, onset, duration, &notes) && notes >= 2) {
            size_t run = 1;
            while (rational_cmp(start, bar) >= 0) {
                start = position_sub(start, bar);
                if (!bar_rhythm(list, count, start, bar, other_onset, other_duration, &other_notes) ||
                    !same_bar_rhythm(onset, duration, notes, other_onset, other_duration, other_notes)) {
                    break;
                }
                run++;
            }
            context->repeat_run = run;
            context->repeat_count = notes;
            for (size_t i = 0; i < notes; i++) {
                context->repeat_onset[i] = onset[i];
                context->repeat_duration[i] = duration[i];
            }
        }
    }
    free(list);
    return true;
}
