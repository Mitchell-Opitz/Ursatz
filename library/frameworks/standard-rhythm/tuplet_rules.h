#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_TUPLET_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_TUPLET_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/note_value.h"
#include "time/tuplet.h"

/*
 * Tuplet rules for standard-rhythm-v1. A tuplet fits a different number
 * of notes into the time a beat (or subdivision, or several beats) would
 * normally divide into:
 *
 *   NAMES   three in the time of two is a triplet; two in the time of
 *           three is a duplet. Any other ratio (5:4, 6:4, ...) is a
 *           tuplet with no name of its own here.
 *   NUMBER  a tuplet is notated by writing its number of notes -- 3 for
 *           a triplet, 2 for a duplet -- over the rhythm.
 *   SPAN    the group lasts as long as the notes it replaces: a triplet
 *           of eighth notes is the length of one quarter note (two
 *           eighths), a triplet of sixteenth notes one eighth, a triplet
 *           of quarter notes (multi-beat) one half note. Each note is
 *           the written value scaled by normal/actual.
 *
 * Which meter a tuplet is borrowed from, and how it is counted, is
 * frameworks/standard-meter/.
 */

typedef enum {
    STANDARD_RHYTHM_TUPLET_RULE_NAMES = 0,
    STANDARD_RHYTHM_TUPLET_RULE_NUMBER,
    STANDARD_RHYTHM_TUPLET_RULE_SPAN,
    STANDARD_RHYTHM_TUPLET_RULE_COUNT
} StandardRhythmTupletRule;

typedef enum {
    TUPLET_KIND_TRIPLET, /* 3 in the time of 2 */
    TUPLET_KIND_DUPLET,  /* 2 in the time of 3 */
    TUPLET_KIND_OTHER
} TupletKind;

const Rule *standard_rhythm_tuplet_rule(StandardRhythmTupletRule which);
const Constraint *standard_rhythm_tuplet_constraint(StandardRhythmTupletRule which);

/* NAMES: triplet for 3:2, duplet for 2:3, otherwise other. A NULL tuplet is
   other. */
TupletKind standard_rhythm_tuplet_kind(const Tuplet *tuplet);

/* NUMBER: the number written over the group: the count of notes played. */
unsigned standard_rhythm_tuplet_number(const Tuplet *tuplet);

/*
 * SPAN: the length of the whole group for notes of the written value
 * (dots included). False, leaving *out untouched, on a NULL argument or
 * allocation failure. On success the caller owns *out.
 */
bool standard_rhythm_tuplet_span(const Tuplet *tuplet, const NoteValue *written, Duration **out);

/* SPAN: the length of each note in the group. Same failure behavior. */
bool standard_rhythm_tuplet_note_duration(const Tuplet *tuplet, const NoteValue *written, Duration **out);

#endif
