#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_THIRD_SPECIES_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_THIRD_SPECIES_TEST_UTIL_H

#include "counterpoint_test_util.h"
#include "third_species_exercise.h"

/* Slots for a third-species counterpoint: ATK(P(C, 0, 4)) is a new note,
   TIE(P(C, 0, 4)) a tie to the note before it, REST() a quarter rest. */
#define ATK(pitch) {STANDARD_COUNTERPOINT_SLOT_ATTACK, (pitch)}
#define TIE(pitch) {STANDARD_COUNTERPOINT_SLOT_TIE, (pitch)}
#define REST() {STANDARD_COUNTERPOINT_SLOT_REST, NULL}

/* A bar that is one whole note: an attack and three ties. */
#define WHOLE(pitch) ATK(pitch), TIE(pitch), TIE(pitch), TIE(pitch)

/* An exercise named `name` over the cantus firmus notes `cantus_array` and
   the slots `slot_array`, with the cantus firmus in `part`. */
#define EXERCISE(name, cantus_array, slot_array, part)                                                    \
    StandardCounterpointLine name##_cantus = LINE_OF(cantus_array);                                       \
    StandardCounterpointSlotLine name##_line = {(slot_array), sizeof(slot_array) / sizeof((slot_array)[0])}; \
    StandardCounterpointThirdSpeciesExercise name = {&name##_cantus, &name##_line, (part)}

#define CANTUS_LOWER STANDARD_COUNTERPOINT_CANTUS_IN_LOWER
#define CANTUS_UPPER STANDARD_COUNTERPOINT_CANTUS_IN_UPPER

#endif
