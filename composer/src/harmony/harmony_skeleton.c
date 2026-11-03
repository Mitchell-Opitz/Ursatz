#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

#define PREFERENCE_SCALE 1000

bool harmony_skeleton_read(const PiecePlan *plan, LaneSpanId target, SkeletonContext *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_HARMONY ||
        plan_lane_span_voice(plan, target) != 0 ||
        plan_lane_span_find_constraint(plan, target, PLAN_CONSTRAINT_EMBELLISH) != NULL) {
        return false;
    }
    SkeletonContext context;
    memset(&context, 0, sizeof(context));
    context.target = target;
    context.owner = plan_lane_span_owner(plan, target);
    context.span = plan_lane_span_span(plan, target);
    PlanSpan owner_span = plan_node_span(plan, context.owner);
    /* A span with a lead or a tail is not a whole number of cells from a cell: harmony_read_cells refuses it, so the schema
       and sequence operators have no candidates for it, and it is left to the operators that read part cells. */
    if (!harmony_find_key(plan, target, context.owner, context.span, &context.key) ||
        !harmony_read_cells(plan, context.span, &context.metre)) {
        return false;
    }
    context.at_start = rational_cmp(context.span.start, owner_span.start) == 0;
    PlanConstraint cadence;
    if (harmony_find_constraint(plan, target, context.owner, PLAN_CONSTRAINT_CADENCE, &cadence)) {
        context.has_cadence = true;
        context.cadence = cadence.params.cadence;
    }

    /* The chords before the span: the run of written spans of the owner that touch it, back to the start. */
    LaneSpanId found[HARMONY_MAX_CHORDS];
    LaneSpanId same_owner[HARMONY_MAX_CHORDS];
    size_t total = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, owner_span, found, HARMONY_MAX_CHORDS);
    if (total == 0 || total > HARMONY_MAX_CHORDS) {
        return false;
    }
    size_t count = 0;
    size_t at = HARMONY_MAX_CHORDS;
    for (size_t i = 0; i < total; i++) {
        if (plan_lane_span_owner(plan, found[i]) == context.owner) {
            if (found[i] == target) {
                at = count;
            }
            same_owner[count++] = found[i];
        }
    }
    if (at == HARMONY_MAX_CHORDS) {
        return false;
    }
    size_t low = at;
    while (low > 0) {
        Rational end;
        PlanSpan before = plan_lane_span_span(plan, same_owner[low - 1]);
        if (plan_span_end(before, &end) != PLAN_OK ||
            rational_cmp(end, plan_lane_span_span(plan, same_owner[low]).start) != 0 ||
            plan_lane_span_state(plan, same_owner[low - 1]) == PLAN_STATE_CONSTRAINED) {
            break;
        }
        low--;
    }
    context.before_count = at - low;
    for (size_t i = 0; i < context.before_count; i++) {
        if (!harmony_read_chord(plan, same_owner[low + i], context.key.mode, context.metre.cell_length,
                                &context.before[i])) {
            return false;
        }
    }
    /* A span that starts the owner has nothing before it; any other has the chords back to the owner's start. */
    if (context.at_start ? context.before_count != 0
                         : (context.before_count == 0 ||
                            rational_cmp(plan_lane_span_span(plan, same_owner[low]).start, owner_span.start) != 0)) {
        return false;
    }

    /* The bass lane over the span is empty, or one Constrained placeholder the span's size. */
    LaneSpanId basses[2];
    size_t bass_count = plan_lane_find(plan, PLAN_LANE_BASS, 0, context.span, basses, 2);
    if (bass_count > 1) {
        return false;
    }
    if (bass_count == 1) {
        PlanSpan bass_span = plan_lane_span_span(plan, basses[0]);
        if (plan_lane_span_state(plan, basses[0]) != PLAN_STATE_CONSTRAINED ||
            rational_cmp(bass_span.start, context.span.start) != 0 ||
            rational_cmp(bass_span.length, context.span.length) != 0) {
            return false;
        }
        context.bass_placeholder = basses[0];
    }
    *out = context;
    return true;
}

bool harmony_skeleton_bass(SkeletonCandidate *skeleton, const HarmonyBassParams *params, FillRng *rng) {
    const SkeletonContext *context = &skeleton->context;
    BassContext bass;
    size_t cells = 0;
    memset(&bass, 0, sizeof(bass));
    for (size_t i = 0; i < skeleton->count; i++) {
        cells += skeleton->chords[i].cells;
        bass.chords[i] = skeleton->chords[i];
    }
    bass.count = skeleton->count;
    bass.target = context->bass_placeholder;
    bass.owner = context->owner;
    bass.span.start = context->span.start;
    if (harmony_cells_span(context->span.start, context->metre.cell_length, 0, cells, &bass.span) != PLAN_OK) {
        return false;
    }
    bass.key = context->key;
    bass.metre.meter = context->metre.meter;
    bass.metre.bar_length = context->metre.bar_length;
    bass.metre.cells_per_bar = context->metre.cells_per_bar;
    bass.metre.cell_length = context->metre.cell_length;
    bass.metre.cells = cells;
    bass.metre.bars = cells / context->metre.cells_per_bar;
    /* A skeleton is written on whole cells, from a cell boundary of the bar. */
    bass.metre.start = context->span.start;
    bass.metre.lead = (Rational){0, 1};
    bass.metre.tail = (Rational){0, 1};
    if (rational_mul_int(context->metre.cell_length, (long long)(context->metre.first_cell % context->metre.cells_per_bar), &bass.metre.offset) != RATIONAL_OK) {
        return false;
    }
    return harmony_bass_build(&bass, params, rng, &skeleton->bass);
}

bool harmony_skeleton_judge(const SkeletonCandidate *skeleton, PhraseVerdict *verdict) {
    const SkeletonContext *context = &skeleton->context;
    PhraseChord *all = malloc(HARMONY_MAX_CHORDS * sizeof(*all));
    bool exempt[HARMONY_MAX_CHORDS];
    PhraseVerdict harmony;
    PhraseVerdict bass;
    memset(verdict, 0, sizeof(*verdict));
    if (!all) {
        return false;
    }
    bool ends = skeleton->remainder_cells == 0 && context->has_cadence;
    /* A skeleton that leaves the span open is a phrase not yet finished: the rest will bring the dominant and the
       tonic, so they are counted in for the phrase model, as chords that need no reason of their own. */
    size_t virtual_count = ends ? 0 : 2;
    size_t total = context->before_count + skeleton->count + virtual_count;
    if (total > HARMONY_MAX_CHORDS) {
        free(all);
        verdict->hard = "too many chords";
        return true;
    }
    for (size_t i = 0; i < context->before_count; i++) {
        all[i] = context->before[i];
        exempt[i] = false;
    }
    for (size_t i = 0; i < skeleton->count; i++) {
        all[context->before_count + i] = skeleton->chords[i];
        exempt[context->before_count + i] = true;
    }
    if (virtual_count == 2) {
        size_t at = context->before_count + skeleton->count;
        if (!harmony_make_chord(context->key.mode, 5, 0, false, false, 1, &all[at]) ||
            !harmony_make_chord(context->key.mode, 1, 0, false, false, 1, &all[at + 1])) {
            free(all);
            verdict->hard = "no cadence to come";
            return true;
        }
        exempt[at] = true;
        exempt[at + 1] = true;
    }

    /* The cadence is heard only when the skeleton ends the span; otherwise the rest of the span will bring it. */
    HeardPhrase phrase = {all, total, context->key.mode, context->has_cadence ? context->cadence : CADENCE_TYPE_PERFECT_AUTHENTIC,
                          context->metre.cells_per_bar, context->metre.cell_length, !ends, exempt, false, false, NULL, NULL, false, NULL};
    bool ok = harmony_judge(&phrase, &harmony);
    free(all);
    if (!ok) {
        return false;
    }
    if (harmony.hard) {
        verdict->hard = harmony.hard;
        return true;
    }
    if (skeleton->remainder_cells > 0 && context->has_cadence &&
        skeleton->remainder_cells < harmony_phrase_room_cells(context->cadence, context->metre.cells_per_bar)) {
        verdict->hard = "no room left for the cadence";
        return true;
    }
    if (!harmony_bass_judge(&skeleton->bass, &bass)) {
        return false;
    }
    if (bass.hard) {
        verdict->hard = bass.hard;
        return true;
    }
    verdict->soft_held = harmony.soft_held + bass.soft_held;
    verdict->soft_total = harmony.soft_total + bass.soft_total;
    verdict->heard = harmony.heard;
    return true;
}

bool harmony_skeleton_check(const void *payload, FillCheck *out) {
    const SkeletonCandidate *skeleton = payload;
    PhraseVerdict verdict;
    if (!skeleton || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!harmony_skeleton_judge(skeleton, &verdict)) {
        return false;
    }
    if (verdict.hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        return true;
    }
    /* The preference puts a skeleton ahead of the plain operators' candidates, which score at most 1. */
    unsigned total = verdict.soft_total ? verdict.soft_total : 1;
    out->score_numerator = (unsigned long)verdict.soft_held * PREFERENCE_SCALE + (unsigned long)skeleton->preference * total;
    out->score_denominator = (unsigned long)total * PREFERENCE_SCALE;
    snprintf(out->reason, sizeof(out->reason), "standard-harmony and melodic rules");
    return true;
}

PlanResult harmony_add_chord(PiecePlan *plan, NodeId owner, const PhraseChord *chord, PlanSpan span, LaneSpanId *id) {
    PlanLanePayload payload;
    harmony_chord_payload(chord, &payload);
    PlanResult result = plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, id);
    return result == PLAN_OK ? plan_lane_span_set_state(plan, *id, PLAN_STATE_REALIZED) : result;
}

static PlanResult add_schema_stage(PiecePlan *plan, LaneSpanId id, int schema, size_t stage) {
    PlanConstraint constraint = {PLAN_CONSTRAINT_SCHEMA, {0}};
    constraint.params.schema.schema = schema;
    constraint.params.schema.stage = (int)stage;
    return plan_lane_span_add_constraint(plan, id, &constraint);
}

/* Writes the harmony of the skeleton and the span left over; the first chord takes the target span. */
static PlanResult write_harmony(PiecePlan *plan, const SkeletonCandidate *skeleton, const SkeletonContext *read) {
    Rational cell = read->metre.cell_length;
    LaneSpanId first = read->target;
    LaneSpanId last = first;
    PlanResult result = PLAN_OK;
    size_t offset = 0;
    for (size_t i = 0; result == PLAN_OK && i < skeleton->count; i++) {
        PlanSpan span;
        result = harmony_cells_span(read->span.start, cell, offset, skeleton->chords[i].cells, &span);
        if (result != PLAN_OK) {
            break;
        }
        if (i == 0) {
            PlanLanePayload payload;
            harmony_chord_payload(&skeleton->chords[0], &payload);
            result = plan_lane_span_resize(plan, first, span.length);
            if (result == PLAN_OK) {
                result = plan_lane_span_write(plan, first, &payload);
            }
        } else {
            result = harmony_add_chord(plan, read->owner, &skeleton->chords[i], span, &last);
        }
        if (result == PLAN_OK && skeleton->schema >= 0) {
            result = add_schema_stage(plan, i == 0 ? first : last, skeleton->schema, i);
        }
        offset += skeleton->chords[i].cells;
    }
    if (result != PLAN_OK) {
        return result;
    }
    if (skeleton->remainder_cells > 0) {
        /* What is left stays open, with the cadence, for the loop. */
        PlanSpan span;
        PlanLanePayload placeholder;
        LaneSpanId rest;
        result = harmony_cells_span(read->span.start, cell, offset, skeleton->remainder_cells, &span);
        memset(&placeholder, 0, sizeof(placeholder));
        strcpy(placeholder.harmony.roman_numeral, "I");
        placeholder.harmony.has_function = false;
        if (result == PLAN_OK) {
            result = plan_add_lane_span(plan, read->owner, PLAN_LANE_HARMONY, 0, span, &placeholder, &rest);
        }
        return result == PLAN_OK ? harmony_move_cadence(plan, first, rest) : result;
    }
    return skeleton->count > 1 ? harmony_move_cadence(plan, first, last) : PLAN_OK;
}

static PlanResult write_bass(PiecePlan *plan, const SkeletonCandidate *skeleton, const SkeletonContext *read) {
    Rational cell = read->metre.cell_length;
    PlanResult result = PLAN_OK;
    size_t offset = 0;
    for (size_t i = 0; result == PLAN_OK && i < skeleton->count; i++) {
        PlanNote notes[BASS_MAX_NOTES_PER_CHORD];
        size_t note_count = 0;
        PlanSpan span = {{0, 1}, {0, 1}};
        PlanLanePayload payload;
        result = harmony_bass_chord_notes(&skeleton->bass, i, notes, &note_count);
        if (result == PLAN_OK && note_count > 0) {
            result = harmony_cells_span(read->span.start, cell, offset, skeleton->chords[i].cells, &span);
        } else if (result == PLAN_OK) {
            result = PLAN_ERR_BAD_PAYLOAD;
        }
        if (result != PLAN_OK) {
            break;
        }
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = notes;
        payload.notes.note_count = note_count;
        if (i == 0 && read->bass_placeholder != PLAN_LANE_SPAN_NONE) {
            /* The placeholder note is as long as the span, so the shorter notes are written before it shrinks. */
            result = plan_lane_span_write(plan, read->bass_placeholder, &payload);
            if (result == PLAN_OK) {
                result = plan_lane_span_resize(plan, read->bass_placeholder, span.length);
            }
        } else {
            LaneSpanId id;
            result = plan_add_lane_span(plan, read->owner, PLAN_LANE_BASS, 0, span, &payload, &id);
            if (result == PLAN_OK) {
                result = plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED);
            }
        }
        offset += skeleton->chords[i].cells;
    }
    if (result == PLAN_OK && skeleton->remainder_cells > 0 && read->bass_placeholder != PLAN_LANE_SPAN_NONE) {
        /* The bass of what is left is still to be written: a placeholder of its size. */
        PlanSpan span;
        PlanNote note;
        PlanLanePayload payload;
        LaneSpanId rest;
        result = harmony_cells_span(read->span.start, cell, offset, skeleton->remainder_cells, &span);
        note.midi = skeleton->bass.params.range_low;
        note.spelled = false;
        note.offset = (Rational){0, 1};
        note.duration = span.length;
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        if (result == PLAN_OK) {
            result = plan_add_lane_span(plan, read->owner, PLAN_LANE_BASS, 0, span, &payload, &rest);
        }
    }
    return result;
}

PlanResult harmony_skeleton_apply(PiecePlan *plan, const SkeletonCandidate *skeleton) {
    SkeletonContext read;
    if (!plan || !skeleton || skeleton->count == 0) {
        return PLAN_ERR_NULL;
    }
    if (!harmony_skeleton_read(plan, skeleton->context.target, &read) ||
        read.metre.cells != skeleton->context.metre.cells || read.metre.first_cell != skeleton->context.metre.first_cell ||
        read.bass_placeholder != skeleton->context.bass_placeholder) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    size_t cells = skeleton->remainder_cells;
    for (size_t i = 0; i < skeleton->count; i++) {
        cells += skeleton->chords[i].cells;
    }
    if (cells != read.metre.cells) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    /* The bass is written first, then the harmony. */
    PlanResult result = write_bass(plan, skeleton, &read);
    return result == PLAN_OK ? write_harmony(plan, skeleton, &read) : result;
}
