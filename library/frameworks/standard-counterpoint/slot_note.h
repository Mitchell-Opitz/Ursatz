#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SLOT_NOTE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SLOT_NOTE_H

#include <stddef.h>

#include "pitch/pitch_spelling.h"

/*
 * The slot types the counterpoint styles share (the fugue, ground bass and sixteenth-century frameworks already depend on
 * standard-counterpoint, so one definition serves all three).
 *
 * A note of a voice: the slot it begins on, the slots it lasts (ties included), and its pitch.
 *
 * A rule result: the hard and soft violations a run of rule groups found.
 */

typedef struct {
    size_t start;
    size_t length;
    const PitchSpelling *pitch;
} StandardCounterpointSlotNote;

typedef struct {
    unsigned hard_violations;
    unsigned soft_violations;
} StandardCounterpointRuleResult;

#endif
