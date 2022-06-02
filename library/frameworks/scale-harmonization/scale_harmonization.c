#include "scale_harmonization.h"

#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

bool scale_harmonization_valid(const ScaleHarmonization *harmonization) {
    if (harmonization == NULL || harmonization->stages == NULL || harmonization->tonic == NULL ||
        harmonization->stage_count == 0 || harmonization->stage_count > SCALE_MAX_STAGES ||
        harmonization->voice_count < 2 || harmonization->voice_count > SCALE_MAX_VOICES ||
        !scale_direction_valid(harmonization->direction)) {
        return false;
    }
    for (size_t s = 0; s < harmonization->stage_count; s++) {
        for (size_t v = 0; v < harmonization->voice_count; v++) {
            if (harmonization->stages[s].pitches[v] == NULL) {
                return false;
            }
        }
    }
    return true;
}

bool scale_interval_above_bass(const ScaleHarmonization *harmonization, size_t stage, size_t voice, int *out) {
    if (out == NULL || !scale_harmonization_valid(harmonization) || stage >= harmonization->stage_count ||
        voice == 0 || voice >= harmonization->voice_count) {
        return false;
    }
    const PitchSpelling *bass = harmonization->stages[stage].pitches[0];
    const PitchSpelling *upper = harmonization->stages[stage].pitches[voice];
    if (standard_counterpoint_semitone_number(upper) < standard_counterpoint_semitone_number(bass)) {
        return false;
    }
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(bass, upper, &interval)) {
        return false;
    }
    *out = interval.simple_size;
    return true;
}
