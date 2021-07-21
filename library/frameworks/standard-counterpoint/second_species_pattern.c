#include "second_species_pattern.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_pitch.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT] = {
    "consonant-passing-tone",
    "substitution",
    "skipped-passing-tone",
    "interval-subdivision",
    "change-of-register",
    "delay-of-melodic-progression",
    "consonant-neighbor-tone",
    "dissonant-passing-tone"};

const char *standard_counterpoint_weak_beat_pattern_name(StandardCounterpointWeakBeatPattern pattern) {
    return pattern >= 0 && pattern < STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT ? NAMES[pattern] : NULL;
}

bool standard_counterpoint_weak_beat_pattern_is_dissonant(StandardCounterpointWeakBeatPattern pattern) {
    return pattern == STANDARD_COUNTERPOINT_WEAK_BEAT_DISSONANT_PASSING_TONE;
}

static bool consonant(const StandardCounterpointInterval *interval) {
    StandardCounterpointIntervalCategory category;
    return standard_counterpoint_classify_interval(interval, &category) &&
           category != STANDARD_COUNTERPOINT_DISSONANCE;
}

bool standard_counterpoint_second_species_weak_beat_pattern(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                            size_t bar, StandardCounterpointWeakBeatPattern *pattern) {
    if (pattern == NULL || !standard_counterpoint_second_species_exercise_valid(exercise) ||
        bar + 1 >= exercise->cantus->count) {
        return false;
    }
    const PitchSpelling *d1 = standard_counterpoint_second_species_downbeat(exercise, bar);
    const PitchSpelling *w = standard_counterpoint_second_species_weak(exercise, bar);
    const PitchSpelling *d2 = standard_counterpoint_second_species_downbeat(exercise, bar + 1);
    if (d1 == NULL || w == NULL || d2 == NULL || !standard_counterpoint_second_species_weak_is_attack(exercise, bar)) {
        return false;
    }

    StandardCounterpointInterval vertical_d1, vertical_w, vertical_d2;
    StandardCounterpointInterval first, second, whole;
    if (!standard_counterpoint_second_species_downbeat_interval(exercise, bar, &vertical_d1) ||
        !standard_counterpoint_second_species_weak_interval(exercise, bar, &vertical_w) ||
        !standard_counterpoint_second_species_downbeat_interval(exercise, bar + 1, &vertical_d2) ||
        !standard_counterpoint_interval_between(d1, w, &first) ||
        !standard_counterpoint_interval_between(w, d2, &second) ||
        !standard_counterpoint_interval_between(d1, d2, &whole)) {
        return false;
    }
    if (!standard_counterpoint_interval_melodic_consonant(&first) ||
        !standard_counterpoint_interval_melodic_consonant(&second) ||
        !standard_counterpoint_interval_melodic_consonant(&whole)) {
        return false;
    }

    bool ends_consonant = consonant(&vertical_d1) && consonant(&vertical_d2);
    bool weak_consonant = consonant(&vertical_w);
    int out_direction = standard_counterpoint_direction(d1, w);
    int back_direction = standard_counterpoint_direction(w, d2);
    bool same_direction = out_direction != 0 && out_direction == back_direction;
    bool opposite_direction = out_direction != 0 && back_direction != 0 && out_direction != back_direction;

    if (!ends_consonant) {
        return false;
    }

    StandardCounterpointWeakBeatPattern found;
    if (first.size == 2 && second.size == 2 && same_direction) {
        found = weak_consonant ? STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_PASSING_TONE
                               : STANDARD_COUNTERPOINT_WEAK_BEAT_DISSONANT_PASSING_TONE;
    } else if (!weak_consonant) {
        return false;
    } else if (first.size == 4 && second.size == 2 && opposite_direction && whole.size == 3) {
        found = STANDARD_COUNTERPOINT_WEAK_BEAT_SUBSTITUTION;
    } else if (((first.size == 3 && second.size == 2) || (first.size == 2 && second.size == 3)) && same_direction &&
               whole.size == 4) {
        found = STANDARD_COUNTERPOINT_WEAK_BEAT_SKIPPED_PASSING_TONE;
    } else if (same_direction && ((first.size == 3 && second.size == 3 && whole.size == 5) ||
                                  (first.size == 3 && second.size == 4 && whole.size == 6) ||
                                  (first.size == 4 && second.size == 3 && whole.size == 6))) {
        found = STANDARD_COUNTERPOINT_WEAK_BEAT_INTERVAL_SUBDIVISION;
    } else if ((first.size == 5 || first.size == 6 || first.size == 8) && second.size == 2 && opposite_direction) {
        found = STANDARD_COUNTERPOINT_WEAK_BEAT_CHANGE_OF_REGISTER;
    } else if (first.size == 3 && second.size == 2 && opposite_direction && whole.size == 2) {
        found = STANDARD_COUNTERPOINT_WEAK_BEAT_DELAY_OF_MELODIC_PROGRESSION;
    } else if (first.size == 2 && second.size == 2 && opposite_direction && standard_counterpoint_same_pitch(d1, d2)) {
        found = STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_NEIGHBOR_TONE;
    } else {
        return false;
    }
    *pattern = found;
    return true;
}
