#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_TIME_SIGNATURE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_TIME_SIGNATURE_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/time_signature.h"

/*
 * Simple time signature rules for standard-meter-v1. In a simple meter a
 * time signature expresses how many beats a measure holds (top number)
 * and which note value is the beat (bottom number):
 *
 *   SIMPLE_BEATS     the top number is 2, 3, or 4: duple, triple, or
 *                    quadruple
 *   BEAT_UNIT        the bottom number is usually 2 (half note beat), 4
 *                    (quarter), or 8 (eighth); 16 (sixteenth) and 1
 *                    (whole) also occur
 *   COMMON_CUT       common time is 4/4 (simple quadruple) and cut time
 *                    is 2/2 (simple duple); both are written with a
 *                    symbol instead of numbers
 *   NOT_A_FRACTION   a time signature names a beat count and a beat
 *                    unit, not a ratio: 2/4 and 4/8 hold the same length
 *                    of music per measure but are different signatures
 *
 * Only simple meter is classified here; a signature that is not simple
 * (compound, or an irregular top number such as 5) is reported as not
 * simple, not as invalid -- TimeSignature itself allows it.
 */

typedef enum {
    STANDARD_METER_TIME_SIGNATURE_RULE_SIMPLE_BEATS = 0,
    STANDARD_METER_TIME_SIGNATURE_RULE_BEAT_UNIT,
    STANDARD_METER_TIME_SIGNATURE_RULE_COMMON_CUT,
    STANDARD_METER_TIME_SIGNATURE_RULE_NOT_A_FRACTION,
    STANDARD_METER_TIME_SIGNATURE_RULE_COUNT
} StandardMeterTimeSignatureRule;

typedef enum {
    TIME_SIGNATURE_SYMBOL_NONE, /* no symbol; written with numbers */
    TIME_SIGNATURE_SYMBOL_COMMON,
    TIME_SIGNATURE_SYMBOL_CUT
} TimeSignatureSymbol;

const Rule *standard_meter_time_signature_rule(StandardMeterTimeSignatureRule which);
const Constraint *standard_meter_time_signature_constraint(StandardMeterTimeSignatureRule which);

/* SIMPLE_BEATS: true when the top number is 2, 3, or 4. */
bool standard_meter_beats_are_simple(unsigned beats);

/* BEAT_UNIT: true for the usual bottom numbers 2, 4, and 8. */
bool standard_meter_beat_unit_is_usual(const TimeSignature *time_signature);

/* SIMPLE_BEATS + BEAT_UNIT: true when the signature is a simple meter --
   top number 2, 3, or 4 over a bottom number of 1, 2, 4, 8, or 16. */
bool standard_meter_is_simple(const TimeSignature *time_signature);

/* COMMON_CUT: the symbol that may stand for this signature (common for
   4/4, cut for 2/2), or TIME_SIGNATURE_SYMBOL_NONE. */
TimeSignatureSymbol standard_meter_signature_symbol(const TimeSignature *time_signature);

/* COMMON_CUT: the signature a symbol stands for. False, leaving *out
   untouched, for TIME_SIGNATURE_SYMBOL_NONE or on allocation failure. On
   success the caller owns *out. */
bool standard_meter_symbol_signature(TimeSignatureSymbol symbol, TimeSignature **out);

/* NOT_A_FRACTION: true when both signatures hold the same length of
   music per measure, whether or not they are the same signature. */
bool standard_meter_same_measure_length(const TimeSignature *a, const TimeSignature *b);

#endif
