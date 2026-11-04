#include "dot_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_RHYTHM_DOT_RULE_COUNT] = {"dot-adds-half", "dot-subsequent", "dot-total"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_rhythm_framework, "standard-rhythm",
    NAMES, STANDARD_RHYTHM_DOT_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_rhythm_dot, StandardRhythmDotRule, STANDARD_RHYTHM_DOT_RULE_COUNT, g_table)

bool standard_rhythm_dot_added_duration(const NoteValue *note_value, unsigned n, Duration **out) {
    if (note_value == NULL || n == 0 || n > NOTE_VALUE_MAX_DOTS) {
        return false;
    }
    int exponent = note_value_exponent(note_value);
    if (exponent < 0) {
        /* A breve is 2 wholes: its nth dot adds 2 / 2^n. */
        return duration_create(2, 1UL << n, out) == DURATION_OK;
    }
    return duration_create(1, 1UL << ((unsigned)exponent + n), out) == DURATION_OK;
}

bool standard_rhythm_dotted_factor(unsigned dots, unsigned long *out_numerator, unsigned long *out_denominator) {
    if (dots > NOTE_VALUE_MAX_DOTS) {
        return false;
    }
    *out_numerator = (1UL << (dots + 1)) - 1;
    *out_denominator = 1UL << dots;
    return true;
}
