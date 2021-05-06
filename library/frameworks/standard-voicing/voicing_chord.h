#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_CHORD_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_CHORD_H

#include "pitch/pitch_spelling.h"
#include "semantics/voice_part.h"

/*
 * A chord written in SATB style: one sounding pitch per voice part,
 * indexed by VoicePart (soprano, alto, tenor, bass). The spellings are
 * borrowed, not owned; every entry must be non-NULL for a rule to give an
 * answer.
 */

typedef struct {
    const PitchSpelling *voice[VOICE_PART_COUNT];
} StandardVoicingChord;

#endif
