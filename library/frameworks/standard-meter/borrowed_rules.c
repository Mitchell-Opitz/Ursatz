#include "borrowed_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_BORROWED_RULE_COUNT] = {"borrowed-borrowed", "borrowed-level",
                                                                       "borrowed-typical-level"};

static StandardMeterRulePair g_pairs[STANDARD_METER_BORROWED_RULE_COUNT];
static bool g_built[STANDARD_METER_BORROWED_RULE_COUNT];

static bool valid_rule(StandardMeterBorrowedRule which) {
    return which >= 0 && which < STANDARD_METER_BORROWED_RULE_COUNT;
}

static void ensure_built(StandardMeterBorrowedRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_borrowed_rule(StandardMeterBorrowedRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_borrowed_constraint(StandardMeterBorrowedRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static bool is_triplet(const Tuplet *tuplet) {
    return tuplet != NULL && tuplet_actual(tuplet) == 3 && tuplet_normal(tuplet) == 2;
}

static bool is_duplet(const Tuplet *tuplet) {
    return tuplet != NULL && tuplet_actual(tuplet) == 2 && tuplet_normal(tuplet) == 3;
}

bool standard_meter_tuplet_borrowed(const Tuplet *tuplet, MeterType type) {
    return (type == METER_TYPE_SIMPLE && is_triplet(tuplet)) || (type == METER_TYPE_COMPOUND && is_duplet(tuplet));
}

bool standard_meter_tuplet_level(const TimeSignature *time_signature, const Duration *span, TupletLevel *out) {
    if (time_signature == NULL || span == NULL || duration_numerator(span) == 0) {
        return false;
    }
    Duration *beat = NULL;
    if (!standard_meter_beat_duration(time_signature, &beat)) {
        return false;
    }

    /* span / beat = (span_n * beat_d) / (span_d * beat_n) */
    unsigned long up = (unsigned long)duration_numerator(span) * duration_denominator(beat);
    unsigned long down = duration_denominator(span) * (unsigned long)duration_numerator(beat);
    duration_destroy(beat);

    bool ok = true;
    if (up == down) {
        *out = TUPLET_LEVEL_BEAT;
    } else if (up > down && up % down == 0) {
        *out = TUPLET_LEVEL_MULTI_BEAT;
    } else if (up < down && down % up == 0) {
        *out = TUPLET_LEVEL_SUBDIVISION; /* the beat is a whole number of spans */
    } else {
        ok = false;
    }
    return ok;
}

bool standard_meter_tuplet_level_typical(const Tuplet *tuplet, TupletLevel level) {
    if (is_triplet(tuplet)) {
        return true;
    }
    if (is_duplet(tuplet)) {
        return level == TUPLET_LEVEL_BEAT || level == TUPLET_LEVEL_SUBDIVISION;
    }
    return false;
}
