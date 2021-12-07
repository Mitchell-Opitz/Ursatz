#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_FIFTH_SPECIES_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_FIFTH_SPECIES_TEST_UTIL_H

#include "counterpoint_test_util.h"
#include "fifth_species_exercise.h"

/* Slots for a fifth-species counterpoint, on an eighth-note grid. ATK(p) is a
   new note, TIE(p) a tie to the note before it, REST() an eighth rest. The
   note macros give a whole note's worth of slots: HALF(p) is four slots,
   QUARTER(p) two, EIGHTH(p) one, WHOLE(p) eight, and T2(p) and the like are
   that many ties. */
#define ATK(pitch) {STANDARD_COUNTERPOINT_SLOT_ATTACK, (pitch)}
#define TIE(pitch) {STANDARD_COUNTERPOINT_SLOT_TIE, (pitch)}
#define REST() {STANDARD_COUNTERPOINT_SLOT_REST, NULL}

#define T2(pitch) TIE(pitch), TIE(pitch)
#define T3(pitch) TIE(pitch), TIE(pitch), TIE(pitch)
#define T5(pitch) T3(pitch), T2(pitch)
#define T7(pitch) T5(pitch), T2(pitch)

#define REST4() REST(), REST(), REST(), REST()
#define EIGHTH(pitch) ATK(pitch)
#define QUARTER(pitch) ATK(pitch), TIE(pitch)
#define HALF(pitch) ATK(pitch), T3(pitch)
#define WHOLE(pitch) ATK(pitch), T7(pitch)

/* An exercise named `name` over the cantus firmus notes `cantus_array` and
   the slots `slot_array`, with the cantus firmus in `part`. */
#define EXERCISE(name, cantus_array, slot_array, part)                                                    \
    StandardCounterpointLine name##_cantus = LINE_OF(cantus_array);                                       \
    StandardCounterpointSlotLine name##_line = {(slot_array), sizeof(slot_array) / sizeof((slot_array)[0])}; \
    StandardCounterpointFifthSpeciesExercise name = {&name##_cantus, &name##_line, (part)}

#define CANTUS_LOWER STANDARD_COUNTERPOINT_CANTUS_IN_LOWER
#define CANTUS_UPPER STANDARD_COUNTERPOINT_CANTUS_IN_UPPER

/* A fifth-species exercise over Fux's Dorian cantus firmus (D3 to D3), with
   the counterpoint above it. It begins with a half rest and la, tied across
   the bar line, and uses every note value: a whole note, dotted half, half and
   quarter notes, and a pair of eighths. It has a passing quarter, a neighbor
   eighth, two 7-6 suspensions (the second the penultimate bar), and ends with
   ti over re and a whole note on do, and breaks no hard rule. Notes are
   interned in the test pool, so these macros may be used once per test
   function. */
#define GOOD_FIFTH_SPECIES_CANTUS(name)                                                                     \
    const PitchSpelling *name[] = {P(D, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(G, 0, 3), P(F, 0, 3), \
                                   P(A, 0, 3), P(G, 0, 3), P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)}
#define GOOD_FIFTH_SPECIES_SLOTS(name)                                                                       \
    const StandardCounterpointSlot name[] = {                                                                \
        /* bar 0 */ REST4(), HALF(P(A, 0, 3)),                                                               \
        /* bar 1 */ T3(P(A, 0, 3)), TIE(P(A, 0, 3)), QUARTER(P(C, 0, 4)), QUARTER(P(D, 0, 4)),              \
        /* bar 2 */ HALF(P(E, 0, 4)), QUARTER(P(D, 0, 4)), QUARTER(P(C, 0, 4)),                              \
        /* bar 3 */ TIE(P(C, 0, 4)), TIE(P(C, 0, 4)), QUARTER(P(B, 0, 3)), HALF(P(D, 0, 4)),                 \
        /* bar 4 */ WHOLE(P(B, 0, 3)),                                                                       \
        /* bar 5 */ QUARTER(P(A, 0, 3)), QUARTER(P(G, 0, 3)), QUARTER(P(F, 0, 3)), QUARTER(P(A, 0, 3)),      \
        /* bar 6 */ HALF(P(C, 0, 4)), QUARTER(P(E, 0, 4)), EIGHTH(P(E, 0, 4)), EIGHTH(P(D, 0, 4)),           \
        /* bar 7 */ HALF(P(E, 0, 4)), QUARTER(P(D, 0, 4)), QUARTER(P(E, 0, 4)),                              \
        /* bar 8 */ HALF(P(F, 0, 4)), QUARTER(P(E, 0, 4)), QUARTER(P(D, 0, 4)),                              \
        /* bar 9 */ TIE(P(D, 0, 4)), TIE(P(D, 0, 4)), ATK(P(C, 1, 4)), T5(P(C, 1, 4)),                       \
        /* bar 10 */ WHOLE(P(D, 0, 4))}

#endif
