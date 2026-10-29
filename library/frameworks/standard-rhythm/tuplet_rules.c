#include "tuplet_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_TUPLET_RULE_COUNT] = {"tuplet-names", "tuplet-number", "tuplet-span"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_rhythm_framework, "standard-rhythm",
    NAMES, STANDARD_RHYTHM_TUPLET_RULE_COUNT);

static bool valid_rule(StandardRhythmTupletRule which) {
    return which >= 0 && which < STANDARD_RHYTHM_TUPLET_RULE_COUNT;
}

const Rule *standard_rhythm_tuplet_rule(StandardRhythmTupletRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_rhythm_tuplet_constraint(StandardRhythmTupletRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

TupletKind standard_rhythm_tuplet_kind(const Tuplet *tuplet) {
    if (tuplet == NULL) {
        return TUPLET_KIND_OTHER;
    }
    if (tuplet_actual(tuplet) == 3 && tuplet_normal(tuplet) == 2) {
        return TUPLET_KIND_TRIPLET;
    }
    if (tuplet_actual(tuplet) == 2 && tuplet_normal(tuplet) == 3) {
        return TUPLET_KIND_DUPLET;
    }
    return TUPLET_KIND_OTHER;
}

unsigned standard_rhythm_tuplet_number(const Tuplet *tuplet) {
    return tuplet_actual(tuplet);
}

bool standard_rhythm_tuplet_span(const Tuplet *tuplet, const NoteValue *written, Duration **out) {
    if (tuplet == NULL || written == NULL) {
        return false;
    }
    Duration *nominal = NULL;
    if (note_value_duration(written, &nominal) != DURATION_OK) {
        return false;
    }
    DurationResult result = tuplet_span(tuplet, nominal, out);
    duration_destroy(nominal);
    return result == DURATION_OK;
}

bool standard_rhythm_tuplet_note_duration(const Tuplet *tuplet, const NoteValue *written, Duration **out) {
    if (tuplet == NULL || written == NULL) {
        return false;
    }
    Duration *nominal = NULL;
    if (note_value_duration(written, &nominal) != DURATION_OK) {
        return false;
    }
    DurationResult result = tuplet_note_duration(tuplet, nominal, out);
    duration_destroy(nominal);
    return result == DURATION_OK;
}
