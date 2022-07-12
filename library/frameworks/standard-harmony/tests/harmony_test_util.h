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
/* The minor-key chord on the raised, leading-tone seventh degree (vii). */
#define LEADING_TONE_CHORD ((HarmonyChord){7, 0, true, false})

#endif
