#include "analysis_cadence_readings.h"

#include <math.h>

#include "harmony_functions.h"
#include "time/musical_time.h"

#define SHARE_MIN 50
#define SHARE_MAX 950

static double whole_notes(const MusicalTime *time) {
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

static int clamp_share(int share) {
    return share < SHARE_MIN ? SHARE_MIN : (share > SHARE_MAX ? SHARE_MAX : share);
}

/* The length of a beat in whole notes at the segment: its own beat_length, or the config's. */
static double beat_of(const HarmonySegment *segment, const HarmonyCadenceConfig *config) {
    const Duration *length = segment->input->beat_length;
    if (length != NULL) {
        return (double)duration_numerator(length) / (double)duration_denominator(length);
    }
    return config->beat_length;
}

/* The length of the run [first, last] in beats. */
static double run_beats(const HarmonySegment *segments, size_t first, size_t last, const HarmonyCadenceConfig *config) {
    double length = whole_notes(time_span_end(segments[last].input->span)) -
                    whole_notes(time_span_start(segments[first].input->span));
    return length / beat_of(&segments[first], config);
}

static bool has_function(const HarmonySegment *segment, HarmonicFunction function) {
    HarmonicFunction found;
    return segment->valid && !segment->applied && harmony_function_of(segment->input->mode, &segment->chord, &found) &&
           found == function;
}

static bool is_tonic_six(const HarmonySegment *segment) {
    return segment->valid && !segment->applied && segment->chord.degree == 1 && segment->chord.inversion == 1 &&
           !segment->chord.seventh && !segment->chord.cadential;
}

/* Whether an authentic cadence arrives within `beats` beats after the segment `from` starts: a root-position tonic
   run after a root-position V run that is a cadence point by the usual rule. */
static bool authentic_follows(const HarmonySegment *segments, size_t count, const HarmonyRun *runs, size_t run_count,
                              size_t from_run, const HarmonyCadenceConfig *config) {
    double start = whole_notes(time_span_start(segments[runs[from_run].first].input->span));
    double window = config->reading_followup_beats * beat_of(&segments[runs[from_run].first], config);
    for (size_t r = from_run + 1; r < run_count; r++) {
        double at = whole_notes(time_span_start(segments[runs[r].first].input->span));
        if (at - start > window + 1e-9) {
            break;
        }
        if (harmony_cadence_is_tonic_root(&segments[runs[r].first]) &&
            harmony_cadence_is_dominant_root(&segments[runs[r - 1].first]) &&
            harmony_cadence_is_arrival(segments, count, runs[r - 1].first, runs[r].first, runs[r].last, config)) {
            return true;
        }
    }
    return false;
}

int harmony_cadence_evaded_share(const HarmonySegment *segments, size_t count, const HarmonyRun *runs,
                                 size_t run_count, size_t dominant_run, const HarmonyCadenceConfig *config) {
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (segments == NULL || runs == NULL || dominant_run + 1 >= run_count ||
        !harmony_cadence_is_dominant_root(&segments[runs[dominant_run].first]) ||
        !is_tonic_six(&segments[runs[dominant_run + 1].first])) {
        return 0;
    }
    size_t six = dominant_run + 1;
    int share = 500;

    share += segments[runs[six].first].input->beat_strength == BEAT_STRENGTH_STRONG ? config->reading_strong_arrival
                                                                                   : -config->reading_strong_arrival;

    bool six_four = dominant_run >= 1 && harmony_cadence_is_cadential_six_four(&segments[runs[dominant_run - 1].first]);
    size_t unit_run = six_four ? dominant_run - 1 : dominant_run;
    bool predominant = unit_run >= 1 && has_function(&segments[runs[unit_run - 1].first], HARMONIC_FUNCTION_PREDOMINANT);
    share += six_four || predominant ? config->reading_cadential_approach : -config->reading_cadential_approach;

    if (authentic_follows(segments, count, runs, run_count, six, config)) {
        share += config->reading_followup_cadence;
    }
    if (six + 1 < run_count) {
        const HarmonySegment *after = &segments[runs[six + 1].first];
        if (harmony_cadence_is_tonic_root(after) || is_tonic_six(after)) {
            share -= config->reading_tonic_continues;
        }
    }
    return clamp_share(share);
}

int harmony_cadence_boundary_half_share(const HarmonySegment *segments, size_t count, const HarmonyRun *runs,
                                        size_t run_count, size_t dominant_run, const HarmonyCadenceConfig *config) {
    (void)count;
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (segments == NULL || runs == NULL || dominant_run + 1 >= run_count ||
        !harmony_cadence_is_dominant_root(&segments[runs[dominant_run].first])) {
        return 0;
    }
    const HarmonySegment *tonic = &segments[runs[dominant_run + 1].first];
    double beats = run_beats(segments, runs[dominant_run].first, runs[dominant_run].last, config);
    int share = 500;

    if (beats + 1e-9 >= config->reading_long_dominant_beats) {
        share += config->reading_long_dominant;
    }
    if (segments[runs[dominant_run].last].input->soprano_degree == 0 || tonic->input->soprano_degree == 0) {
        share += config->reading_break_after_dominant;
    }
    if (tonic->input->beat_strength == BEAT_STRENGTH_STRONG && tonic->input->soprano_degree == 1) {
        share -= config->reading_pac_shape;
    }
    if (beats + 1e-9 < 2.0) {
        share -= config->reading_short_dominant;
    }
    return clamp_share(share);
}
