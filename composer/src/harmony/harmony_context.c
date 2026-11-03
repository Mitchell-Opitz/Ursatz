#include "harmony/harmony_internal.h"

#include <string.h>

/* Reads the numeral of one span: a diatonic one, else an applied one. The cells are not counted. */
static bool read_numeral(const PiecePlan *plan, LaneSpanId id, PlanKeyMode mode, PhraseChord *out) {
    const PlanHarmony *harmony = &plan_lane_span_payload(plan, id)->harmony;
    memset(out, 0, sizeof(*out));
    memcpy(out->roman, harmony->roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
    out->roman[PLAN_ROMAN_NUMERAL_MAX - 1] = '\0';
    out->function = harmony->function;
    out->has_function = harmony->has_function;
    if (!harmony_roman_parse(mode, out->roman, &out->chord)) {
        out->applied = true;
        if (!harmony_roman_parse_applied_inverted(mode, out->roman, &out->applied_chord, &out->applied_inversion)) {
            return false;
        }
    }
    return true;
}

bool harmony_read_chord(const PiecePlan *plan, LaneSpanId id, PlanKeyMode mode, Rational cell_length,
                       PhraseChord *out) {
    return read_numeral(plan, id, mode, out) &&
           harmony_count_cells(cell_length, plan_lane_span_span(plan, id).length, &out->cells);
}

bool harmony_read_chord_in(const PiecePlan *plan, LaneSpanId id, PlanKeyMode mode, const PhraseMetre *metre,
                           PhraseChord *out) {
    size_t first;
    return read_numeral(plan, id, mode, out) &&
           harmony_metre_find_cells(metre, plan_lane_span_span(plan, id), &first, &out->cells);
}

bool harmony_context_read(const PiecePlan *plan, LaneSpanId target, PhraseContext *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_HARMONY ||
        plan_lane_span_voice(plan, target) != 0 ||
        plan_lane_span_find_constraint(plan, target, PLAN_CONSTRAINT_EMBELLISH) == NULL) {
        return false;
    }
    NodeId owner = plan_lane_span_owner(plan, target);
    LaneSpanId found[HARMONY_MAX_CHORDS];
    LaneSpanId same_owner[HARMONY_MAX_CHORDS];
    size_t total = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, owner), found, HARMONY_MAX_CHORDS);
    if (total == 0 || total > HARMONY_MAX_CHORDS) {
        return false;
    }
    size_t count = 0;
    size_t at = HARMONY_MAX_CHORDS;
    for (size_t i = 0; i < total; i++) {
        if (plan_lane_span_owner(plan, found[i]) == owner) {
            if (found[i] == target) {
                at = count;
            }
            same_owner[count++] = found[i];
        }
    }
    if (at == HARMONY_MAX_CHORDS) {
        return false;
    }

    /* The run of spans that touch, end to start, through the target. */
    size_t low = at;
    size_t high = at;
    while (low > 0) {
        Rational end;
        PlanSpan before = plan_lane_span_span(plan, same_owner[low - 1]);
        if (plan_span_end(before, &end) != PLAN_OK ||
            rational_cmp(end, plan_lane_span_span(plan, same_owner[low]).start) != 0) {
            break;
        }
        low--;
    }
    while (high + 1 < count) {
        Rational end;
        PlanSpan here = plan_lane_span_span(plan, same_owner[high]);
        if (plan_span_end(here, &end) != PLAN_OK ||
            rational_cmp(end, plan_lane_span_span(plan, same_owner[high + 1]).start) != 0) {
            break;
        }
        high++;
    }

    PhraseContext context;
    memset(&context, 0, sizeof(context));
    context.owner = owner;
    context.count = high - low + 1;
    context.target = at - low;
    PlanSpan last = plan_lane_span_span(plan, same_owner[high]);
    Rational end;
    context.span.start = plan_lane_span_span(plan, same_owner[low]).start;
    if (plan_span_end(last, &end) != PLAN_OK ||
        rational_sub(end, context.span.start, &context.span.length) != RATIONAL_OK) {
        return false;
    }

    PlanConstraint cadence;
    if (!harmony_read_metre(plan, context.span, &context.metre) ||
        !harmony_find_mode(plan, target, owner, context.span, &context.mode) ||
        !harmony_find_constraint(plan, same_owner[high], owner, PLAN_CONSTRAINT_CADENCE, &cadence)) {
        return false;
    }
    context.cadence = cadence.params.cadence;
    for (size_t i = 0; i < context.count; i++) {
        LaneSpanId id = same_owner[low + i];
        PlanState state = plan_lane_span_state(plan, id);
        /* Every chord is settled, or is marked and so will be settled by this operator in its turn. */
        if (state == PLAN_STATE_CONSTRAINED && plan_lane_span_find_constraint(plan, id, PLAN_CONSTRAINT_EMBELLISH) == NULL) {
            return false;
        }
        context.ids[i] = id;
        if (!harmony_read_chord_in(plan, id, context.mode, &context.metre, &context.chords[i])) {
            return false;
        }
    }
    *out = context;
    return true;
}
