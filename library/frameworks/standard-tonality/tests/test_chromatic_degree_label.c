#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tonal_key.h"

/* Chromatic degree labels: the twelve labels, and any integer reduced mod 12. */

int main(void) {
    static const char *const EXPECTED[12] = {"1", "b2", "2", "b3", "3", "4", "#4", "5", "b6", "6", "b7", "7"};
    for (int semitones = 0; semitones < 12; semitones++) {
        assert(strcmp(standard_tonality_chromatic_degree_label(semitones), EXPECTED[semitones]) == 0);
        assert(strcmp(standard_tonality_chromatic_degree_label(semitones + 12), EXPECTED[semitones]) == 0);
        assert(strcmp(standard_tonality_chromatic_degree_label(semitones - 12), EXPECTED[semitones]) == 0);
        assert(strcmp(standard_tonality_chromatic_degree_label(semitones + 120), EXPECTED[semitones]) == 0);
    }
    assert(strcmp(standard_tonality_chromatic_degree_label(-1), "7") == 0);
    assert(strcmp(standard_tonality_chromatic_degree_label(-6), "#4") == 0);
    printf("All chromatic degree label tests passed.\n");
    return 0;
}
