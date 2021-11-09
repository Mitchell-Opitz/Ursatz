#include "fifth_species_exercise.h"

#include <stddef.h>

#include "counterpoint_pitch.h"

#define SLOTS STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR

typedef StandardCounterpointFifthSpeciesExercise Exercise;
typedef StandardCounterpointFifthSpeciesNote Note;

/* True when a note of `duration` eighths may start at eighth `start` of a bar
   (the note is not an eighth, and is not in the last bar). */
static bool longer_note_fits(size_t start, size_t duration) {
    if (start % 2 != 0) {
        return false;
    }
    switch (start) {
        case 0:
            return duration == 2 || duration == 4 || duration == 6 || duration == 8;
        case 2:
            return duration == 2 || duration == 4 || duration == 6;
        case 4:
            return duration == 2 || duration == 4 || duration == 6 || duration == 8;
        case 6:
            return duration == 2 || duration == 4 || duration == 6;
        default:
            return false;
    }
}

/* Reads the notes of a slot line whose count and pitches are already checked.
   False when the slots are not a sequence of attacks and their ties, or a rest
   is out of place. */
static bool read_notes(const StandardCounterpointSlotLine *line, size_t first, Note *notes, size_t capacity,
                       size_t *count) {
    size_t found = 0;
    for (size_t i = first; i < line->count; i++) {
        const StandardCounterpointSlot *slot = &line->slots[i];
        if (slot->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK) {
            if (found >= capacity) {
                return false;
            }
            notes[found].slot = i;
            notes[found].duration = 1;
            notes[found].pitch = slot->pitch;
            found++;
        } else if (slot->kind == STANDARD_COUNTERPOINT_SLOT_TIE) {
            if (found == 0 || !standard_counterpoint_same_pitch(slot->pitch, notes[found - 1].pitch)) {
                return false;
            }
            notes[found - 1].duration++;
        } else {
            return false;
        }
    }
    *count = found;
    return true;
}

bool standard_counterpoint_fifth_species_exercise_valid(const Exercise *exercise) {
    if (exercise == NULL || !standard_counterpoint_line_valid(exercise->cantus) || exercise->counterpoint == NULL ||
        exercise->counterpoint->slots == NULL) {
        return false;
    }
    if (exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
        exercise->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER) {
        return false;
    }
    size_t bars = exercise->cantus->count;
    if (bars < 3 || bars > STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_BARS || exercise->counterpoint->count != SLOTS * bars) {
        return false;
    }

    const StandardCounterpointSlotLine *line = exercise->counterpoint;
    size_t rests = 0;
    while (rests < line->count && line->slots[rests].kind == STANDARD_COUNTERPOINT_SLOT_REST) {
        if (line->slots[rests].pitch != NULL) {
            return false;
        }
        rests++;
    }
    if (rests != 0 && rests != 2 && rests != 4 && rests != 6) {
        return false;
    }
    for (size_t i = rests; i < line->count; i++) {
        if (line->slots[i].pitch == NULL) {
            return false;
        }
    }

    Note notes[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t count;
    if (!read_notes(line, rests, notes, STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES, &count) || count == 0) {
        return false;
    }

    size_t last_bar_start = SLOTS * (bars - 1);
    for (size_t n = 0; n < count; n++) {
        size_t start = notes[n].slot % SLOTS;
        size_t duration = notes[n].duration;
        size_t end = notes[n].slot + duration;
        if (notes[n].slot >= last_bar_start) {
            if (notes[n].slot != last_bar_start || duration != SLOTS) {
                return false;
            }
            continue;
        }
        if (end > last_bar_start) {
            return false;
        }
        if (duration == 1) {
            /* An eighth note is one of a pair on the second or fourth beat. */
            if (start != 2 && start != 3 && start != 6 && start != 7) {
                return false;
            }
            bool first_of_pair = start % 2 == 0;
            size_t other = first_of_pair ? n + 1 : n - 1;
            if ((first_of_pair && n + 1 >= count) || (!first_of_pair && n == 0) || notes[other].duration != 1 ||
                notes[other].slot != (first_of_pair ? notes[n].slot + 1 : notes[n].slot - 1)) {
                return false;
            }
            continue;
        }
        if (!longer_note_fits(start, duration)) {
            return false;
        }
    }
    return true;
}

size_t standard_counterpoint_fifth_species_bar_count(const Exercise *exercise) {
    return standard_counterpoint_fifth_species_exercise_valid(exercise) ? exercise->cantus->count : 0;
}

BeatStrength standard_counterpoint_fifth_species_slot_strength(size_t slot) {
    size_t eighth = slot % SLOTS;
    if (eighth == 0) {
        return BEAT_STRENGTH_STRONG;
    }
    return eighth == 4 ? BEAT_STRENGTH_MEDIUM : BEAT_STRENGTH_WEAK;
}

const PitchSpelling *standard_counterpoint_fifth_species_cantus(const Exercise *exercise, size_t bar) {
    if (exercise == NULL || exercise->cantus == NULL || bar >= exercise->cantus->count) {
        return NULL;
    }
    return exercise->cantus->notes[bar];
}

static const StandardCounterpointSlot *slot_of(const Exercise *exercise, size_t slot) {
    if (exercise == NULL || exercise->counterpoint == NULL || exercise->counterpoint->slots == NULL ||
        slot >= exercise->counterpoint->count) {
        return NULL;
    }
    return &exercise->counterpoint->slots[slot];
}

const PitchSpelling *standard_counterpoint_fifth_species_pitch(const Exercise *exercise, size_t slot) {
    const StandardCounterpointSlot *entry = slot_of(exercise, slot);
    return entry == NULL || entry->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : entry->pitch;
}

bool standard_counterpoint_fifth_species_slot_is_attack(const Exercise *exercise, size_t slot) {
    const StandardCounterpointSlot *entry = slot_of(exercise, slot);
    return entry != NULL && entry->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

bool standard_counterpoint_fifth_species_slot_is_tie(const Exercise *exercise, size_t slot) {
    const StandardCounterpointSlot *entry = slot_of(exercise, slot);
    return entry != NULL && entry->kind == STANDARD_COUNTERPOINT_SLOT_TIE;
}

bool standard_counterpoint_fifth_species_interval(const Exercise *exercise, size_t slot,
                                                  StandardCounterpointInterval *out) {
    const PitchSpelling *pitch = standard_counterpoint_fifth_species_pitch(exercise, slot);
    const PitchSpelling *cantus = standard_counterpoint_fifth_species_cantus(exercise, slot / SLOTS);
    if (pitch == NULL || cantus == NULL) {
        return false;
    }
    return standard_counterpoint_interval_between(cantus, pitch, out);
}

bool standard_counterpoint_fifth_species_notes(const Exercise *exercise, Note *notes, size_t capacity, size_t *count) {
    if (!standard_counterpoint_fifth_species_exercise_valid(exercise) || notes == NULL || count == NULL) {
        return false;
    }
    size_t first = 0;
    while (exercise->counterpoint->slots[first].kind == STANDARD_COUNTERPOINT_SLOT_REST) {
        first++;
    }
    return read_notes(exercise->counterpoint, first, notes, capacity, count);
}

bool standard_counterpoint_fifth_species_attacks(const Exercise *exercise, const PitchSpelling **pitches, size_t *slots,
                                                 size_t capacity, size_t *count) {
    if (!standard_counterpoint_fifth_species_exercise_valid(exercise) || pitches == NULL || slots == NULL ||
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
        pitches[found] = exercise->counterpoint->slots[i].pitch;
        slots[found] = i;
        found++;
    }
    *count = found;
    return true;
}
