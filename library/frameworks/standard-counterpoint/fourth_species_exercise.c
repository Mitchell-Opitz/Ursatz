#include "fourth_species_exercise.h"

#include <stddef.h>

#include "counterpoint_pitch.h"

#define SLOTS STANDARD_COUNTERPOINT_FOURTH_SPECIES_SLOTS_PER_BAR

bool standard_counterpoint_fourth_species_exercise_valid(const StandardCounterpointFourthSpeciesExercise *exercise) {
    if (exercise == NULL || !standard_counterpoint_line_valid(exercise->cantus) || exercise->counterpoint == NULL ||
        exercise->counterpoint->slots == NULL) {
        return false;
    }
    if (exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
        exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER) {
        return false;
    }
    size_t bars = exercise->cantus->count;
    if (bars < 3 || bars > STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_BARS || exercise->counterpoint->count != SLOTS * bars) {
        return false;
    }

    const StandardCounterpointSlot *slots = exercise->counterpoint->slots;
    if (slots[0].kind != STANDARD_COUNTERPOINT_SLOT_REST || slots[0].pitch != NULL ||
        slots[1].kind != STANDARD_COUNTERPOINT_SLOT_ATTACK || slots[1].pitch == NULL) {
        return false;
    }
    for (size_t bar = 1; bar < bars; bar++) {
        const StandardCounterpointSlot *strong = &slots[SLOTS * bar];
        const StandardCounterpointSlot *weak = &slots[SLOTS * bar + 1];
        const StandardCounterpointSlot *before = &slots[SLOTS * bar - 1];
        if (strong->pitch == NULL || weak->pitch == NULL) {
            return false;
        }
        bool last = bar + 1 == bars;
        if (last) {
            if (strong->kind != STANDARD_COUNTERPOINT_SLOT_ATTACK || weak->kind != STANDARD_COUNTERPOINT_SLOT_TIE ||
                !standard_counterpoint_same_pitch(weak->pitch, strong->pitch)) {
                return false;
            }
            continue;
        }
        if (weak->kind != STANDARD_COUNTERPOINT_SLOT_ATTACK) {
            return false;
        }
        if (strong->kind == STANDARD_COUNTERPOINT_SLOT_TIE) {
            if (!standard_counterpoint_same_pitch(strong->pitch, before->pitch)) {
                return false;
            }
        } else if (strong->kind != STANDARD_COUNTERPOINT_SLOT_ATTACK) {
            return false;
        }
    }
    return true;
}

size_t standard_counterpoint_fourth_species_bar_count(const StandardCounterpointFourthSpeciesExercise *exercise) {
    return standard_counterpoint_fourth_species_exercise_valid(exercise) ? exercise->cantus->count : 0;
}

BeatStrength standard_counterpoint_fourth_species_slot_strength(size_t slot) {
    return slot % SLOTS == 0 ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_WEAK;
}

const PitchSpelling *standard_counterpoint_fourth_species_cantus(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                                 size_t bar) {
    if (exercise == NULL || exercise->cantus == NULL || bar >= exercise->cantus->count) {
        return NULL;
    }
    return exercise->cantus->notes[bar];
}

static const StandardCounterpointSlot *slot_of(const StandardCounterpointFourthSpeciesExercise *exercise, size_t bar,
                                               size_t beat) {
    if (exercise == NULL || exercise->counterpoint == NULL || SLOTS * bar + beat >= exercise->counterpoint->count) {
        return NULL;
    }
    return &exercise->counterpoint->slots[SLOTS * bar + beat];
}

const PitchSpelling *standard_counterpoint_fourth_species_downbeat(
    const StandardCounterpointFourthSpeciesExercise *exercise, size_t bar) {
    const StandardCounterpointSlot *slot = slot_of(exercise, bar, 0);
    return slot == NULL || slot->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : slot->pitch;
}

const PitchSpelling *standard_counterpoint_fourth_species_weak(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                               size_t bar) {
    const StandardCounterpointSlot *slot = slot_of(exercise, bar, 1);
    return slot == NULL || slot->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : slot->pitch;
}

bool standard_counterpoint_fourth_species_downbeat_is_tie(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                          size_t bar) {
    const StandardCounterpointSlot *slot = slot_of(exercise, bar, 0);
    return slot != NULL && slot->kind == STANDARD_COUNTERPOINT_SLOT_TIE;
}

bool standard_counterpoint_fourth_species_bar_is_broken(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                        size_t bar) {
    const StandardCounterpointSlot *slot = slot_of(exercise, bar, 0);
    return slot != NULL && bar > 0 && bar + 1 < exercise->cantus->count &&
           slot->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

static bool interval_against_cantus(const StandardCounterpointFourthSpeciesExercise *exercise, size_t bar,
                                    const PitchSpelling *pitch, StandardCounterpointInterval *out) {
    const PitchSpelling *cantus = standard_counterpoint_fourth_species_cantus(exercise, bar);
    if (cantus == NULL || pitch == NULL) {
        return false;
    }
    return standard_counterpoint_interval_between(cantus, pitch, out);
}

bool standard_counterpoint_fourth_species_downbeat_interval(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                            size_t bar, StandardCounterpointInterval *out) {
    return interval_against_cantus(exercise, bar, standard_counterpoint_fourth_species_downbeat(exercise, bar), out);
}

bool standard_counterpoint_fourth_species_weak_interval(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                        size_t bar, StandardCounterpointInterval *out) {
    return interval_against_cantus(exercise, bar, standard_counterpoint_fourth_species_weak(exercise, bar), out);
}

bool standard_counterpoint_fourth_species_attacks(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                  const PitchSpelling **notes, size_t *slots, size_t capacity,
                                                  size_t *count) {
    if (!standard_counterpoint_fourth_species_exercise_valid(exercise) || notes == NULL || slots == NULL ||
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
