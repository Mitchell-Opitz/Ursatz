#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_NAME_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_NAME_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/note_value.h"

/*
 * Note value names for standard-rhythm-v1. Two naming systems write the
 * same values:
 *
 *   exponent  American                British
 *   -1        double whole note       breve
 *    0        whole note              semibreve
 *    1        half note               minim
 *    2        quarter note            crotchet
 *    3        eighth note             quaver
 *    4        sixteenth note          semiquaver
 *    5        thirty-second note      demisemiquaver
 *    6        sixty-fourth note       hemidemisemiquaver
 *
 * Names stop at the sixty-fourth; NoteValue itself reaches further
 * (exponent 10), and a value past 6 is simply unnamed here rather than
 * given an invented name. The name of a dotted value is its base value's
 * name -- the dot is not part of the name. Names are matched
 * case-insensitively.
 *
 * NOTE_NAMES is the one rule: a value's name in each system.
 */

typedef enum {
    STANDARD_RHYTHM_NAME_RULE_NOTE_NAMES = 0,
    STANDARD_RHYTHM_NAME_RULE_COUNT
} StandardRhythmNameRule;

typedef enum {
    STANDARD_RHYTHM_NAMING_AMERICAN,
    STANDARD_RHYTHM_NAMING_BRITISH
} StandardRhythmNaming;

#define STANDARD_RHYTHM_MAX_NAMED_EXPONENT 6

const Rule *standard_rhythm_name_rule(StandardRhythmNameRule which);
const Constraint *standard_rhythm_name_constraint(StandardRhythmNameRule which);

/* NOTE_NAMES: the name of the value in the given system, or NULL when the
   value is past the sixty-fourth (exponent > 6) or the system is invalid. */
const char *standard_rhythm_note_name(const NoteValue *note_value, StandardRhythmNaming naming);

/* NOTE_NAMES: the exponent a name denotes, in either system. False,
   leaving *out_exponent untouched, for an unrecognized name. */
bool standard_rhythm_note_exponent_from_name(const char *name, int *out_exponent);

#endif
