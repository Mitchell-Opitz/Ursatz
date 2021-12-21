#include "fifth_species_pattern.h"

#include <stddef.h>

#define SLOTS STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR

static const char *const VALUE_NAMES[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT] = {"eighth", "quarter", "half",
                                                                                "dotted-half", "whole"};
static const size_t VALUE_EIGHTHS[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT] = {1, 2, 4, 6, 8};
static const char *const PATTERN_NAMES[STANDARD_COUNTERPOINT_BAR_PATTERN_COUNT] = {
    "whole", "halves", "half-quarters", "quarters-half", "quarters", "eighths", "syncopated", "rest-start", "other"};

const char *standard_counterpoint_note_value_name(StandardCounterpointNoteValue value) {
    return value >= 0 && value < STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT ? VALUE_NAMES[value] : NULL;
}

size_t standard_counterpoint_note_value_eighths(StandardCounterpointNoteValue value) {
    return value >= 0 && value < STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT ? VALUE_EIGHTHS[value] : 0;
}

bool standard_counterpoint_note_value_from_eighths(size_t eighths, StandardCounterpointNoteValue *out) {
    if (out == NULL) {
        return false;
    }
    for (int i = 0; i < STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT; i++) {
        if (VALUE_EIGHTHS[i] == eighths) {
            *out = (StandardCounterpointNoteValue)i;
            return true;
        }
    }
    return false;
}

const char *standard_counterpoint_bar_pattern_name(StandardCounterpointBarPattern pattern) {
    return pattern >= 0 && pattern < STANDARD_COUNTERPOINT_BAR_PATTERN_COUNT ? PATTERN_NAMES[pattern] : NULL;
}

static bool matches(const size_t *durations, size_t count, const size_t *expected, size_t expected_count) {
    if (count != expected_count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (durations[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_fifth_species_bar_pattern(const StandardCounterpointFifthSpeciesExercise *exercise, size_t bar,
                                                     StandardCounterpointBarPattern *out) {
    StandardCounterpointFifthSpeciesNote notes[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t count;
    if (out == NULL || !standard_counterpoint_fifth_species_notes(exercise, notes, STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES,
                                                                  &count) ||
        bar >= exercise->cantus->count) {
        return false;
    }
    if (standard_counterpoint_fifth_species_slot_is_tie(exercise, SLOTS * bar)) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_SYNCOPATED;
        return true;
    }
    if (standard_counterpoint_fifth_species_pitch(exercise, SLOTS * bar) == NULL) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_REST_START;
        return true;
    }

    size_t durations[SLOTS];
    size_t found = 0;
    bool eighths = false;
    for (size_t n = 0; n < count; n++) {
        if (notes[n].slot / SLOTS != bar) {
            continue;
        }
        size_t start = notes[n].slot % SLOTS;
        eighths = eighths || notes[n].duration == 1;
        durations[found++] = notes[n].duration < SLOTS - start ? notes[n].duration : SLOTS - start;
    }
    if (eighths) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_EIGHTHS;
        return true;
    }
    static const size_t whole[] = {8};
    static const size_t halves[] = {4, 4};
    static const size_t half_quarters[] = {4, 2, 2};
    static const size_t quarters_half[] = {2, 2, 4};
    static const size_t quarters[] = {2, 2, 2, 2};
    if (matches(durations, found, whole, 1)) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_WHOLE;
    } else if (matches(durations, found, halves, 2)) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_HALVES;
    } else if (matches(durations, found, half_quarters, 3)) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_HALF_QUARTERS;
    } else if (matches(durations, found, quarters_half, 3)) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_QUARTERS_HALF;
    } else if (matches(durations, found, quarters, 4)) {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_QUARTERS;
    } else {
        *out = STANDARD_COUNTERPOINT_BAR_PATTERN_OTHER;
    }
    return true;
}

bool standard_counterpoint_fifth_species_value_counts(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                      size_t counts[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT]) {
    StandardCounterpointFifthSpeciesNote notes[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t count;
    if (counts == NULL || !standard_counterpoint_fifth_species_notes(exercise, notes, STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES,
                                                                     &count)) {
        return false;
    }
    size_t tally[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT] = {0};
    for (size_t n = 0; n < count; n++) {
        StandardCounterpointNoteValue value;
        if (!standard_counterpoint_note_value_from_eighths(notes[n].duration, &value)) {
            return false;
        }
        tally[value]++;
    }
    for (int i = 0; i < STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT; i++) {
        counts[i] = tally[i];
    }
    return true;
}
