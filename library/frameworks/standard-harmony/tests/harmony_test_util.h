#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_TEST_UTIL_H

#include <stddef.h>

#include "harmony_chord.h"
#include "harmony_framework.h"

/* Test helpers shared by the standard-harmony tests. */

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* A chord on a scale degree: root position, no leading-tone mark. */
#define CH(degree) ((HarmonyChord){(degree), 0, false, false})
/* The same with an inversion (1 first, 2 second, 3 third). */
#define CHI(degree, inversion) ((HarmonyChord){(degree), (inversion), false, false})
/* A seventh chord on a scale degree (root position), and with an inversion. */
#define CH7(degree) ((HarmonyChord){(degree), 0, false, true})
#define CH7I(degree, inversion) ((HarmonyChord){(degree), (inversion), false, true})
/* The leading-tone chords, in a major key (degree 7 is always the leading tone): the
   triad, and the fully diminished seventh chord, with an inversion. */
#define VII(inversion) ((HarmonyChord){7, (inversion), false, false})
#define VII7(inversion) ((HarmonyChord){7, (inversion), false, true})
/* The same in minor, built on ti (the raised seventh degree). */
#define VIIM(inversion) ((HarmonyChord){7, (inversion), true, false})
#define VIIM7(inversion) ((HarmonyChord){7, (inversion), true, true})
/* The half-diminished seventh chord (a major key), with an inversion. */
#define VII_HALF(inversion) ((HarmonyChord){7, (inversion), false, true, false, true})
/* The cadential 6/4: the tonic triad in second inversion, marked cadential. */
#define CADENTIAL_CHORD ((HarmonyChord){1, 2, false, false, true})
/* The minor-key chord on the raised, leading-tone seventh degree (vii). */
#define LEADING_TONE_CHORD ((HarmonyChord){7, 0, true, false})

#endif
