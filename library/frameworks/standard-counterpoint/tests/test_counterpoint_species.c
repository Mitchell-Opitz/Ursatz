#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "counterpoint_species.h"

static void test_validity(void) {
    for (int i = 0; i < COUNTERPOINT_SPECIES_COUNT; i++) {
        assert(counterpoint_species_valid((CounterpointSpecies)i));
    }
    assert(!counterpoint_species_valid(COUNTERPOINT_SPECIES_COUNT));
    assert(!counterpoint_species_valid((CounterpointSpecies)-1));
}

static void test_names(void) {
    assert(strcmp(counterpoint_species_name(COUNTERPOINT_SPECIES_FIRST), "first") == 0);
    assert(strcmp(counterpoint_species_name(COUNTERPOINT_SPECIES_SECOND), "second") == 0);
    assert(strcmp(counterpoint_species_name(COUNTERPOINT_SPECIES_THIRD), "third") == 0);
    assert(strcmp(counterpoint_species_name(COUNTERPOINT_SPECIES_FOURTH), "fourth") == 0);
    assert(strcmp(counterpoint_species_name(COUNTERPOINT_SPECIES_FIFTH), "fifth") == 0);
    assert(counterpoint_species_name(COUNTERPOINT_SPECIES_COUNT) == NULL);
}

static void test_notes_per_cantus_note(void) {
    assert(counterpoint_species_notes_per_cantus_note(COUNTERPOINT_SPECIES_FIRST) == 1);
    assert(counterpoint_species_notes_per_cantus_note(COUNTERPOINT_SPECIES_SECOND) == 2);
    assert(counterpoint_species_notes_per_cantus_note(COUNTERPOINT_SPECIES_THIRD) == 4);
    assert(counterpoint_species_notes_per_cantus_note(COUNTERPOINT_SPECIES_FOURTH) == 2);
    assert(counterpoint_species_notes_per_cantus_note(COUNTERPOINT_SPECIES_FIFTH) == 0);
    assert(counterpoint_species_notes_per_cantus_note(COUNTERPOINT_SPECIES_COUNT) == 0);
}

static void test_order(void) {
    assert(counterpoint_species_precedes(COUNTERPOINT_SPECIES_FIRST, COUNTERPOINT_SPECIES_SECOND));
    assert(counterpoint_species_precedes(COUNTERPOINT_SPECIES_FOURTH, COUNTERPOINT_SPECIES_FIFTH));
    assert(counterpoint_species_precedes(COUNTERPOINT_SPECIES_FIRST, COUNTERPOINT_SPECIES_FIFTH));
    assert(!counterpoint_species_precedes(COUNTERPOINT_SPECIES_SECOND, COUNTERPOINT_SPECIES_FIRST));
    assert(!counterpoint_species_precedes(COUNTERPOINT_SPECIES_THIRD, COUNTERPOINT_SPECIES_THIRD));
    assert(!counterpoint_species_precedes(COUNTERPOINT_SPECIES_COUNT, COUNTERPOINT_SPECIES_FIRST));
    assert(!counterpoint_species_precedes(COUNTERPOINT_SPECIES_FIRST, COUNTERPOINT_SPECIES_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_notes_per_cantus_note();
    test_order();
    printf("test_counterpoint_species: ok\n");
    return 0;
}
