#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_FOURTH_SPECIES_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_FOURTH_SPECIES_TEST_UTIL_H

#include "counterpoint_test_util.h"
#include "fourth_species_exercise.h"

/* Slots for a fourth-species counterpoint: ATK(P(C, 0, 4)) is a new note,
   TIE(P(C, 0, 4)) a tie to the note before it, REST() a half rest. */
#define ATK(pitch) {STANDARD_COUNTERPOINT_SLOT_ATTACK, (pitch)}
#define TIE(pitch) {STANDARD_COUNTERPOINT_SLOT_TIE, (pitch)}
#define REST() {STANDARD_COUNTERPOINT_SLOT_REST, NULL}

/* A bar that is one whole note: an attack and a tie. */
#define WHOLE(pitch) ATK(pitch), TIE(pitch)

/* An exercise named `name` over the cantus firmus notes `cantus_array` and
   the slots `slot_array`, with the cantus firmus in `part`. */
#define EXERCISE(name, cantus_array, slot_array, part)                                                    \
    StandardCounterpointLine name##_cantus = LINE_OF(cantus_array);                                       \
    StandardCounterpointSlotLine name##_line = {(slot_array), sizeof(slot_array) / sizeof((slot_array)[0])}; \
    StandardCounterpointFourthSpeciesExercise name = {&name##_cantus, &name##_line, (part)}

#define CANTUS_LOWER STANDARD_COUNTERPOINT_CANTUS_IN_LOWER
#define CANTUS_UPPER STANDARD_COUNTERPOINT_CANTUS_IN_UPPER

/* A fourth-species exercise over Fux's Dorian cantus firmus (D3 to D3), with
   the counterpoint above it. It begins with a half rest and la, is a chain of
   suspensions that falls from la to re (a 4-3 or 7-6 in most bars), breaks
   species once in the second to last bar, ends with a tied do, then ti, over
   re (a 7-6 suspension), and a whole note on do, and breaks no hard rule.
   Notes are interned in the test pool, so these macros may be used once per
   test function. */
#define GOOD_FOURTH_SPECIES_CANTUS(name)                                                                    \
    const PitchSpelling *name[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(G, 0, 3), P(F, 0, 3), \
                                   P(A, 0, 3), P(G, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)}
#define GOOD_FOURTH_SPECIES_SLOTS(name)                                                                      \
    const StandardCounterpointSlot name[] = {                                                                \
        REST(), ATK(P(A, 0, 3)), TIE(P(A, 0, 3)), ATK(P(A, 0, 4)), TIE(P(A, 0, 4)), ATK(P(G, 0, 4)),          \
        TIE(P(G, 0, 4)), ATK(P(F, 0, 4)), TIE(P(F, 0, 4)), ATK(P(E, 0, 4)), TIE(P(E, 0, 4)), ATK(P(D, 0, 4)),  \
        TIE(P(D, 0, 4)), ATK(P(C, 0, 4)), TIE(P(C, 0, 4)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)), ATK(P(D, 0, 4)),  \
        TIE(P(D, 0, 4)), ATK(P(C, 1, 4)), ATK(P(D, 0, 4)), TIE(P(D, 0, 4))}

#endif
