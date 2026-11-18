#include "chain/chain_result_voicing_check.h"

#include "theory/vocabulary/voice_part.h"
#include "theory/framework/framework.h"
#include "theory/voicing/framework.h"
#include "theory/voicing/spacing_rules.h"
#include "theory/voicing/voicing_chord.h"


int chain_voicing_first_wide_spacing(const PitchSpelling *const voices[4]) {
    StandardVoicingChord chord;
    for (int part = 0; part < VOICE_PART_COUNT; part++) {
        if (!voices[part]) {
            return VOICING_SPACING_UNJUDGED;
        }
        chord.voice[part] = voices[part];
    }
    VoicePart wide = standard_voicing_first_wide_spacing(&chord);
    return wide == VOICE_PART_COUNT ? VOICING_SPACING_VALID : (int)wide;
}

const char *chain_voicing_framework_name(void) {
    return framework_name(standard_voicing_framework());
}
