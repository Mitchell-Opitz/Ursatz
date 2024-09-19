#include "counterpoint_species.h"

#include <stddef.h>

static const char *const NAMES[COUNTERPOINT_SPECIES_COUNT] = {"first", "second", "third", "fourth", "fifth"};

static const int NOTES_PER_CANTUS_NOTE[COUNTERPOINT_SPECIES_COUNT] = {1, 2, 4, 2, 0};

bool counterpoint_species_valid(CounterpointSpecies species) {
    return species >= 0 && species < COUNTERPOINT_SPECIES_COUNT;
}

const char *counterpoint_species_name(CounterpointSpecies species) {
    return counterpoint_species_valid(species) ? NAMES[species] : NULL;
}

int counterpoint_species_notes_per_cantus_note(CounterpointSpecies species) {
    return counterpoint_species_valid(species) ? NOTES_PER_CANTUS_NOTE[species] : 0;
}

bool counterpoint_species_precedes(CounterpointSpecies a, CounterpointSpecies b) {
    return counterpoint_species_valid(a) && counterpoint_species_valid(b) && a < b;
}
