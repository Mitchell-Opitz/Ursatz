#include "pitch/generic_interval.h"

#include <stdlib.h>

struct GenericInterval {
    int size;
};

GenericIntervalResult generic_interval_create(int size, GenericInterval **out) {
    if (size < 1) {
        return GENERIC_INTERVAL_ERR_INVALID_SIZE;
    }

    GenericInterval *interval = (GenericInterval *)malloc(sizeof(GenericInterval));
    if (interval == NULL) {
        return GENERIC_INTERVAL_ERR_ALLOC;
    }
    interval->size = size;

    *out = interval;
    return GENERIC_INTERVAL_OK;
}

/* Place of the letter in an octave counted from C: C 0 ... B 6 (an
   octave number starts at C). */
static int letter_index(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_C:
            return 0;
        case PITCH_LETTER_D:
            return 1;
        case PITCH_LETTER_E:
            return 2;
        case PITCH_LETTER_F:
            return 3;
        case PITCH_LETTER_G:
            return 4;
        case PITCH_LETTER_A:
            return 5;
        case PITCH_LETTER_B:
        default:
            return 6;
    }
}

static int staff_place(const PitchSpelling *spelling) {
    return GENERIC_INTERVAL_LETTERS_PER_OCTAVE * pitch_spelling_octave(spelling) +
           letter_index(pitch_spelling_letter(spelling));
}

GenericIntervalResult generic_interval_between(const PitchSpelling *lower, const PitchSpelling *upper,
                                               GenericInterval **out) {
    if (lower == NULL || upper == NULL) {
        return GENERIC_INTERVAL_ERR_NULL_SPELLING;
    }
    int size = staff_place(upper) - staff_place(lower) + 1;
    if (size < 1) {
        return GENERIC_INTERVAL_ERR_UPPER_BELOW_LOWER;
    }
    return generic_interval_create(size, out);
}

void generic_interval_destroy(GenericInterval *interval) {
    free(interval);
}

int generic_interval_size(const GenericInterval *interval) {
    return interval->size;
}

bool generic_interval_is_simple(const GenericInterval *interval) {
    return interval->size <= GENERIC_INTERVAL_OCTAVE_SIZE;
}

bool generic_interval_is_compound(const GenericInterval *interval) {
    return !generic_interval_is_simple(interval);
}

int generic_interval_simple_size(const GenericInterval *interval) {
    int size = interval->size;
    while (size > GENERIC_INTERVAL_OCTAVE_SIZE) {
        size -= GENERIC_INTERVAL_LETTERS_PER_OCTAVE;
    }
    return size;
}

GenericIntervalResult generic_interval_add_octave(const GenericInterval *interval, GenericInterval **out) {
    return generic_interval_create(interval->size + GENERIC_INTERVAL_LETTERS_PER_OCTAVE, out);
}

bool generic_interval_equals(const GenericInterval *a, const GenericInterval *b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    return a->size == b->size;
}
