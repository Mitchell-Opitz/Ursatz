#include "third_species_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_limits.h"
#include "harmonic_rules.h"
#include "melodic_rules.h"
#include "third_species_ends_rules.h"
#include "third_species_line_rules.h"
#include "third_species_strong_rules.h"
#include "third_species_beat_rules.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT] = {
    "cantus-firmus", "spacing", "strong-beats", "other-beats", "ends", "counterpoint-line"};

const char *standard_counterpoint_third_species_group_name(StandardCounterpointThirdSpeciesGroup group) {
    return group >= 0 && group < STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT ? NAMES[group] : NULL;
}

static void record(StandardCounterpointResult *result, StandardCounterpointThirdSpeciesGroup group, unsigned hard,
                   unsigned soft) {
    if (hard != 0) {
        result->hard_violations |= 1u << group;
    }
    if (soft != 0) {
        result->soft_violations |= 1u << group;
    }
}

/* Every note the counterpoint sounds (a tie counts), each against the
   cantus firmus note of its bar, as a pair of lines of equal length. */
static bool spacing_check(const StandardCounterpointThirdSpeciesExercise *exercise, unsigned *hard,
                          unsigned *soft) {
    const PitchSpelling *cantus_notes[BUFFER_SIZE];
    const PitchSpelling *counterpoint_notes[BUFFER_SIZE];
    size_t count = 0;
    for (size_t slot = 0; slot < exercise->counterpoint->count; slot++) {
        const StandardCounterpointSlot *entry = &exercise->counterpoint->slots[slot];
        if (entry->kind == STANDARD_COUNTERPOINT_SLOT_REST) {
            continue;
        }
        cantus_notes[count] = exercise->cantus->notes[slot / STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR];
        counterpoint_notes[count] = entry->pitch;
        count++;
    }

    StandardCounterpointLine cantus = {cantus_notes, count};
    StandardCounterpointLine counterpoint = {counterpoint_notes, count};
    StandardCounterpointPair pair;
    if (exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER) {
        pair.lower = &cantus;
        pair.upper = &counterpoint;
    } else {
        pair.lower = &counterpoint;
        pair.upper = &cantus;
    }

    static const StandardCounterpointHarmonicRule rules[] = {
        STANDARD_COUNTERPOINT_HARMONIC_RULE_MAX_GAP, STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE,
        STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING, STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION};
    *hard = 0;
    *soft = 0;
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        if (standard_counterpoint_harmonic_rule_holds(rules[i], &pair)) {
            continue;
        }
        bool is_soft = rules[i] == STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE ||
                       rules[i] == STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION;
        if (is_soft) {
            *soft |= 1u << i;
        } else {
            *hard |= 1u << i;
        }
    }
    return true;
}

bool standard_counterpoint_third_species_check(const StandardCounterpointThirdSpeciesInput *input,
                                                StandardCounterpointResult *out) {
    if (input == NULL || out == NULL || !standard_counterpoint_third_species_exercise_valid(input->exercise)) {
        return false;
    }
    const StandardCounterpointThirdSpeciesExercise *exercise = input->exercise;
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    StandardCounterpointResult result = {0, 0};
    unsigned hard, soft;

    if (input->tonic != NULL) {
        if (!standard_counterpoint_cantus_check(exercise->cantus, input->tonic, &hard, &soft)) {
            return false;
        }
        record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_CANTUS_FIRMUS, hard, soft);
    }
    if (!standard_counterpoint_melodic_check(exercise->cantus, &limits, &hard, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_CANTUS_FIRMUS, hard, soft);

    if (!spacing_check(exercise, &hard, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_SPACING, hard, soft);

    if (!standard_counterpoint_third_species_strong_check(exercise, &hard, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_STRONG_BEATS, hard, soft);

    if (!standard_counterpoint_third_species_beat_check(exercise, &hard, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_OTHER_BEATS, hard, soft);

    if (input->tonic != NULL) {
        if (!standard_counterpoint_third_species_ends_check(exercise, input->tonic, &hard)) {
            return false;
        }
        record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_ENDS, hard, 0);

        if (!standard_counterpoint_third_species_line_check(exercise, input->tonic, &hard, &soft)) {
            return false;
        }
        record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNTERPOINT_LINE, hard, soft);
    }

    *out = result;
    return true;
}

bool standard_counterpoint_third_species_valid(const StandardCounterpointThirdSpeciesInput *input) {
    StandardCounterpointResult result;
    return standard_counterpoint_third_species_check(input, &result) && result.hard_violations == 0;
}
