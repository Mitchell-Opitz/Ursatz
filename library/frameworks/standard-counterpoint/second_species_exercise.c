#include "second_species_exercise.h"

#include <stddef.h>

#include "counterpoint_pitch.h"

#define SLOTS STANDARD_COUNTERPOINT_SECOND_SPECIES_SLOTS_PER_BAR

bool standard_counterpoint_second_species_exercise_valid(const StandardCounterpointSecondSpeciesExercise *exercise) {
    if (exercise == NULL || !standard_counterpoint_line_valid(exercise->cantus) || exercise->counterpoint == NULL ||
        exercise->counterpoint->slots == NULL) {
        return false;
    }
    if (exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
        exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER) {
        return false;
    }
    size_t bars = exercise->cantus->count;
    if (bars < 2 || bars > STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_BARS || exercise->counterpoint->count != SLOTS * bars) {
        return false;
    }

    const StandardCounterpointSlot *slots = exercise->counterpoint->slots;
    for (size_t i = 0; i < exercise->counterpoint->count; i++) {
        switch (slots[i].kind) {
            case STANDARD_COUNTERPOINT_SLOT_ATTACK:
                if (slots[i].pitch == NULL) {
                    return false;
                }
                break;
            case STANDARD_COUNTERPOINT_SLOT_TIE:
                if (i == 0 || i % SLOTS == 0 || slots[i].pitch == NULL ||
                    slots[i - 1].kind == STANDARD_COUNTERPOINT_SLOT_REST ||
                    !standard_counterpoint_same_pitch(slots[i].pitch, slots[i - 1].pitch)) {
                    return false;
                }
                break;
            case STANDARD_COUNTERPOINT_SLOT_REST:
                if (i != 0 || slots[i].pitch != NULL) {
                    return false;
                }
                break;
            default:
                return false;
        }
    }
    return true;
}

size_t standard_counterpoint_second_species_bar_count(const StandardCounterpointSecondSpeciesExercise *exercise) {
    return standard_counterpoint_second_species_exercise_valid(exercise) ? exercise->cantus->count : 0;
}

BeatStrength standard_counterpoint_second_species_slot_strength(size_t slot) {
    return slot % SLOTS == 0 ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_WEAK;
}

const PitchSpelling *standard_counterpoint_second_species_cantus(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                                 size_t bar) {
    if (exercise == NULL || exercise->cantus == NULL || bar >= exercise->cantus->count) {
        return NULL;
    }
    return exercise->cantus->notes[bar];
}

const PitchSpelling *standard_counterpoint_second_species_downbeat(
    const StandardCounterpointSecondSpeciesExercise *exercise, size_t bar) {
    if (exercise == NULL || exercise->counterpoint == NULL || SLOTS * bar + 1 >= exercise->counterpoint->count) {
        return NULL;
    }
    const StandardCounterpointSlot *slot = &exercise->counterpoint->slots[SLOTS * bar];
    return slot->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : slot->pitch;
}

const PitchSpelling *standard_counterpoint_second_species_weak(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                               size_t bar) {
    if (exercise == NULL || exercise->counterpoint == NULL || SLOTS * bar + 1 >= exercise->counterpoint->count) {
        return NULL;
    }
    const StandardCounterpointSlot *slot = &exercise->counterpoint->slots[SLOTS * bar + 1];
    return slot->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : slot->pitch;
}

bool standard_counterpoint_second_species_weak_is_attack(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                         size_t bar) {
    if (exercise == NULL || exercise->counterpoint == NULL || SLOTS * bar + 1 >= exercise->counterpoint->count) {
        return false;
    }
    return exercise->counterpoint->slots[SLOTS * bar + 1].kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

static bool interval_against_cantus(const StandardCounterpointSecondSpeciesExercise *exercise, size_t bar,
                                    const PitchSpelling *pitch, StandardCounterpointInterval *out) {
    const PitchSpelling *cantus = standard_counterpoint_second_species_cantus(exercise, bar);
    if (cantus == NULL || pitch == NULL) {
        return false;
    }
    return standard_counterpoint_interval_between(cantus, pitch, out);
}

bool standard_counterpoint_second_species_downbeat_interval(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                            size_t bar, StandardCounterpointInterval *out) {
    return interval_against_cantus(exercise, bar, standard_counterpoint_second_species_downbeat(exercise, bar), out);
}

bool standard_counterpoint_second_species_weak_interval(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                        size_t bar, StandardCounterpointInterval *out) {
    return interval_against_cantus(exercise, bar, standard_counterpoint_second_species_weak(exercise, bar), out);
}

bool standard_counterpoint_second_species_attacks(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                  const PitchSpelling **notes, size_t *slots, size_t capacity,
                                                  size_t *count) {
    if (!standard_counterpoint_second_species_exercise_valid(exercise) || notes == NULL || slots == NULL ||
        count == NULL) {
        return false;
    }
    size_t found = 0;
    for (size_t i = 0; i < exercise->counterpoint->count; i++) {
        if (exercise->counterpoint->slots[i].kind != STANDARD_COUNTERPOINT_SLOT_ATTACK) {
            continue;
        }
        if (found >= capacity) {
            return false;
        }
        notes[found] = exercise->counterpoint->slots[i].pitch;
        slots[found] = i;
        found++;
    }
    *count = found;
    return true;
}
