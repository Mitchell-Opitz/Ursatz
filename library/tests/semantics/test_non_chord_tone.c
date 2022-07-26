#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/non_chord_tone.h"

static void test_validity(void) {
    for (int i = 0; i < NON_CHORD_TONE_COUNT; i++) {
        assert(non_chord_tone_valid((NonChordTone)i));
    }
    assert(!non_chord_tone_valid(NON_CHORD_TONE_COUNT));
    assert(!non_chord_tone_valid((NonChordTone)-1));
}

static void test_names(void) {
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_PASSING), "passing") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_NEIGHBOR), "neighbor") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_DOUBLE_NEIGHBOR), "double-neighbor") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_CHANGING_TONE), "changing-tone") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_SUSPENSION), "suspension") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_ANTICIPATION), "anticipation") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_APPOGGIATURA), "appoggiatura") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_ESCAPE_TONE), "escape-tone") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_RETARDATION), "retardation") == 0);
    assert(strcmp(non_chord_tone_name(NON_CHORD_TONE_PEDAL), "pedal") == 0);
    assert(non_chord_tone_name(NON_CHORD_TONE_COUNT) == NULL);
}

static void test_involves_leap(void) {
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_PASSING));
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_NEIGHBOR));
    assert(non_chord_tone_involves_leap(NON_CHORD_TONE_DOUBLE_NEIGHBOR));
    assert(non_chord_tone_involves_leap(NON_CHORD_TONE_CHANGING_TONE));
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_SUSPENSION));
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_ANTICIPATION));
    assert(non_chord_tone_involves_leap(NON_CHORD_TONE_APPOGGIATURA));
    assert(non_chord_tone_involves_leap(NON_CHORD_TONE_ESCAPE_TONE));
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_RETARDATION));
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_PEDAL));
    assert(!non_chord_tone_involves_leap(NON_CHORD_TONE_COUNT));
}

static void test_involves_static_note(void) {
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_PASSING));
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_NEIGHBOR));
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_DOUBLE_NEIGHBOR));
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_CHANGING_TONE));
    assert(non_chord_tone_involves_static_note(NON_CHORD_TONE_SUSPENSION));
    assert(non_chord_tone_involves_static_note(NON_CHORD_TONE_ANTICIPATION));
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_APPOGGIATURA));
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_ESCAPE_TONE));
    assert(non_chord_tone_involves_static_note(NON_CHORD_TONE_RETARDATION));
    assert(non_chord_tone_involves_static_note(NON_CHORD_TONE_PEDAL));
    assert(!non_chord_tone_involves_static_note(NON_CHORD_TONE_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_involves_leap();
    test_involves_static_note();
    printf("test_non_chord_tone: ok\n");
    return 0;
}
