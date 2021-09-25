#include "third_species_exercise.h"

#include <stddef.h>

#include "counterpoint_pitch.h"

#define SLOTS STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR

static bool bar_valid(const StandardCounterpointSlot *bar, bool first_bar) {
    if (bar[0].kind == STANDARD_COUNTERPOINT_SLOT_REST) {
        if (!first_bar || bar[0].pitch != NULL) {
            return false;
        }
        for (size_t i = 1; i < SLOTS; i++) {
            if (bar[i].kind != STANDARD_COUNTERPOINT_SLOT_ATTACK || bar[i].pitch == NULL) {
                return false;
            }
        }
        return true;
    }
    if (bar[0].kind != STANDARD_COUNTERPOINT_SLOT_ATTACK || bar[0].pitch == NULL) {
        return false;
    }
    bool whole = bar[1].kind == STANDARD_COUNTERPOINT_SLOT_TIE;
    for (size_t i = 1; i < SLOTS; i++) {
        if (bar[i].pitch == NULL) {
            return false;
        }
        if (whole) {
            if (bar[i].kind != STANDARD_COUNTERPOINT_SLOT_TIE || !standard_counterpoint_same_pitch(bar[i].pitch, bar[0].pitch)) {
                return false;
            }
        } else if (bar[i].kind != STANDARD_COUNTERPOINT_SLOT_ATTACK) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_third_species_exercise_valid(const StandardCounterpointThirdSpeciesExercise *exercise) {
    if (exercise == NULL || !standard_counterpoint_line_valid(exercise->cantus) || exercise->counterpoint == NULL ||
        exercise->counterpoint->slots == NULL) {
        return false;
    }
    if (exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
        exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER) {
        return false;
    }
    size_t bars = exercise->cantus->count;
    if (bars < 2 || bars > STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS || exercise->counterpoint->count != SLOTS * bars) {
        return false;
    }
    for (size_t bar = 0; bar < bars; bar++) {
        if (!bar_valid(&exercise->counterpoint->slots[SLOTS * bar], bar == 0)) {
            return false;
        }
    }
    return true;
}

size_t standard_counterpoint_third_species_bar_count(const StandardCounterpointThirdSpeciesExercise *exercise) {
    return standard_counterpoint_third_species_exercise_valid(exercise) ? exercise->cantus->count : 0;
}

BeatStrength standard_counterpoint_third_species_slot_strength(size_t slot) {
    switch (slot % SLOTS) {
        case 0:
            return BEAT_STRENGTH_STRONG;
        case 2:
            return BEAT_STRENGTH_MEDIUM;
        default:
            return BEAT_STRENGTH_WEAK;
    }
}

const PitchSpelling *standard_counterpoint_third_species_cantus(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                                size_t bar) {
    if (exercise == NULL || exercise->cantus == NULL || bar >= exercise->cantus->count) {
        return NULL;
    }
    return exercise->cantus->notes[bar];
}

static const StandardCounterpointSlot *slot_of(const StandardCounterpointThirdSpeciesExercise *exercise, size_t bar,
                                               size_t beat) {
    if (exercise == NULL || exercise->counterpoint == NULL || beat >= SLOTS || SLOTS * bar + beat >= exercise->counterpoint->count) {
        return NULL;
    }
    return &exercise->counterpoint->slots[SLOTS * bar + beat];
}

const PitchSpelling *standard_counterpoint_third_species_beat(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                              size_t bar, size_t beat) {
    const StandardCounterpointSlot *slot = slot_of(exercise, bar, beat);
    if (slot == NULL || slot->kind == STANDARD_COUNTERPOINT_SLOT_REST) {
        return NULL;
    }
    return slot->pitch;
}

bool standard_counterpoint_third_species_beat_is_attack(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                        size_t bar, size_t beat) {
    const StandardCounterpointSlot *slot = slot_of(exercise, bar, beat);
    return slot != NULL && slot->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

bool standard_counterpoint_third_species_beat_interval(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                       size_t bar, size_t beat, StandardCounterpointInterval *out) {
    const PitchSpelling *cantus = standard_counterpoint_third_species_cantus(exercise, bar);
    const PitchSpelling *pitch = standard_counterpoint_third_species_beat(exercise, bar, beat);
    if (cantus == NULL || pitch == NULL) {
        return false;
    }
    return standard_counterpoint_interval_between(cantus, pitch, out);
}

bool standard_counterpoint_third_species_attacks(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                 const PitchSpelling **notes, size_t *slots, size_t capacity,
                                                 size_t *count) {
    if (!standard_counterpoint_third_species_exercise_valid(exercise) || notes == NULL || slots == NULL ||
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
