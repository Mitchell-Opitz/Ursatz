#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "romantext.h"
#include "time/note_value.h"
#include "time/time_signature.h"

static MeterMap *make_map(unsigned top, int bottom_exponent, bool pickup) {
    NoteValue *unit = NULL;
    assert(note_value_create(bottom_exponent, &unit) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create(top, unit, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(unit);
    MusicalTime *zero = NULL;
    MusicalTime *later = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    assert(musical_time_create(1, 4, &later) == MUSICAL_TIME_OK);
    MeterMapChangePoint points[2] = {{signature, zero}, {signature, later}};
    EntityID *id = NULL;
    assert(entity_id_create("romantext-test-meter", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, pickup ? 2 : 1, &map) == METER_MAP_OK);
    time_signature_destroy(signature);
    musical_time_destroy(zero);
    musical_time_destroy(later);
    return map;
}

static bool time_is(const MusicalTime *t, long n, unsigned long d) {
    return musical_time_numerator(t) * (long)d == n * (long)musical_time_denominator(t);
}

static void test_metadata(void) {
    const char *text = "Composer: J. S. Bach\nTitle: Aus meines Herzens Grunde\nTime Signature: 3/4\n\nm1 G: I\n";
    RomanTextAnalysis a;
    assert(romantext_parse_metadata(text, &a) == ROMANTEXT_OK);
    assert(strcmp(a.composer, "J. S. Bach") == 0);
    assert(strcmp(a.title, "Aus meines Herzens Grunde") == 0);
    assert(a.time_signature_top == 3 && a.time_signature_bottom == 4);
}

static void test_chords_keys_and_beats(void) {
    MeterMap *map = make_map(4, 2, false);
    const char *text =
        "Composer: X\nTime Signature: 4/4\n\n"
        "m1 G: I b2 V6 b3 IV b3.5 viio6\n"
        "m2 V7 b3 V65 b4 I64 ||\n"
        "m3 a: i b2 ii%7 b3 V43\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(a.key_count == 2 && a.chord_count == 10 && a.unsupported_count == 0);
    assert(a.keys[0].tonic_pitch_class == 7 && !a.keys[0].minor);
    assert(a.keys[1].tonic_pitch_class == 9 && a.keys[1].minor);
    assert(time_is(a.keys[1].time, 2, 1)); /* measure 3 starts at 2 whole notes */

    const RomanTextChord *c = a.chords;
    assert(c[0].supported && c[0].degree == 1 && c[0].upper && c[0].inversion == 0 && !c[0].seventh);
    assert(time_is(c[0].time, 0, 1));
    assert(c[1].degree == 5 && c[1].inversion == 1 && time_is(c[1].time, 1, 4));
    assert(c[3].degree == 7 && c[3].marker == 'o' && c[3].inversion == 1 && time_is(c[3].time, 5, 8));
    assert(c[4].degree == 5 && c[4].seventh && c[4].inversion == 0 && time_is(c[4].time, 1, 1));
    assert(c[5].seventh && c[5].inversion == 1);
    assert(c[6].degree == 1 && c[6].inversion == 2 && !c[6].seventh);
    assert(c[7].degree == 1 && !c[7].upper && c[7].tonic_pitch_class == 9 && c[7].minor);
    assert(c[8].degree == 2 && c[8].marker == '%' && c[8].seventh && !c[8].upper);
    assert(c[9].degree == 5 && c[9].seventh && c[9].inversion == 2);
    assert(c[0].tonic_pitch_class == 7 && !c[0].minor);
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_applied_and_altered(void) {
    MeterMap *map = make_map(3, 2, false);
    const char *text = "m1 V7/V b2 viio65/ii b3 bVII\nm2 #iv6 b2 bII6/V\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(a.chord_count == 5 && a.key_count == 0);
    assert(a.keys == NULL);
    assert(a.chords[0].key_index == (size_t)-1);
    assert(a.chords[0].applied && a.chords[0].target_degree == 5 && a.chords[0].seventh);
    assert(a.chords[1].applied && a.chords[1].degree == 7 && a.chords[1].target_degree == 2 &&
           a.chords[1].seventh && a.chords[1].inversion == 1);
    assert(a.chords[2].degree == 7 && a.chords[2].alteration == -1 && !a.chords[2].applied);
    assert(a.chords[3].degree == 4 && a.chords[3].alteration == 1 && a.chords[3].inversion == 1);
    assert(a.chords[4].degree == 2 && a.chords[4].alteration == -1 && a.chords[4].applied &&
           a.chords[4].target_degree == 5);
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_slashed_figures_and_keys(void) {
    MeterMap *map = make_map(3, 2, false);
    const char *text = "m1 Bb: I b2 vi6/5 b3 V4/3\nm2 f#: i6/4 b2 V4/2 b3 V7/IV\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(a.key_count == 2 && a.keys[0].tonic_pitch_class == 10 && a.keys[1].tonic_pitch_class == 6 && a.keys[1].minor);
    assert(a.chords[1].degree == 6 && a.chords[1].seventh && a.chords[1].inversion == 1 && !a.chords[1].applied);
    assert(a.chords[2].seventh && a.chords[2].inversion == 2);
    assert(a.chords[3].degree == 1 && !a.chords[3].seventh && a.chords[3].inversion == 2);
    assert(a.chords[4].seventh && a.chords[4].inversion == 3);
    assert(a.chords[5].applied && a.chords[5].target_degree == 4);
    assert(a.unsupported_count == 0);
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_pickup(void) {
    MeterMap *map = make_map(3, 2, true); /* a quarter-note pickup, then 3/4 */
    assert(meter_map_has_pickup(map));
    const char *text = "m0 b3 G: I\nm1 IV b2 V\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(a.chord_count == 3);
    assert(time_is(a.chords[0].time, 0, 1)); /* the pickup starts the piece */
    assert(time_is(a.chords[1].time, 1, 4));
    assert(time_is(a.chords[2].time, 1, 2));
    assert(a.chords[0].measure == 0 && a.last_measure == 1);
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_compound_meter(void) {
    MeterMap *map = make_map(6, 3, false); /* 6/8: two dotted-quarter beats */
    const char *text = "m1 D: I b2 V\nm2 I\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(time_is(a.chords[1].time, 3, 8));
    assert(time_is(a.chords[2].time, 3, 4));
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_unsupported_tokens_are_kept_and_counted(void) {
    MeterMap *map = make_map(4, 2, false);
    const char *text =
        "m1 C: I b2 Ger65 b3 N6 b4 Cad64\n"
        "m2 V7[no5] b2 V9 b3 V/V/V\n"
        "m3 = m1\n"
        "m4-5 I\n"
        "m6 I\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(a.chord_count == 8);
    assert(a.unsupported_chords == 6);
    assert(a.unsupported_count == 8); /* six chords, the repeat line and the range line */
    assert(strcmp(a.chords[1].text, "Ger65") == 0 && !a.chords[1].supported);
    assert(!a.chords[2].supported && !a.chords[3].supported && !a.chords[4].supported);
    assert(!a.chords[5].supported && !a.chords[6].supported);
    assert(a.chords[0].supported && a.chords[7].supported && a.chords[7].measure == 6);
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_ignored_tokens_and_lines(void) {
    MeterMap *map = make_map(4, 2, false);
    const char *text =
        "Note: a comment\n"
        "m1 C: I || b3 V :||\n"
        "Phrase: something\n"
        "m2 var1 IV b3 ||: I\n";
    RomanTextAnalysis a;
    assert(romantext_parse(text, map, &a) == ROMANTEXT_OK);
    assert(a.chord_count == 4 && a.unsupported_count == 0);
    romantext_destroy(&a);
    meter_map_destroy(map);
}

static void test_errors(void) {
    MeterMap *map = make_map(3, 2, false);
    RomanTextAnalysis a;
    assert(romantext_parse("m1 b5 I\n", map, &a) == ROMANTEXT_ERR_BAD_POSITION);
    assert(strstr(a.error_message, "line 1") != NULL);
    assert(a.chords == NULL);
    assert(romantext_parse("m2 I\nm1 V\n", map, &a) == ROMANTEXT_ERR_BAD_POSITION);
    assert(romantext_parse(NULL, map, &a) == ROMANTEXT_ERR_NULL_ARGUMENT);
    assert(romantext_load("/nonexistent/x.rntxt", map, &a) == ROMANTEXT_ERR_IO);
    meter_map_destroy(map);
}

int main(void) {
    test_metadata();
    test_chords_keys_and_beats();
    test_applied_and_altered();
    test_slashed_figures_and_keys();
    test_pickup();
    test_compound_meter();
    test_unsupported_tokens_are_kept_and_counted();
    test_ignored_tokens_and_lines();
    test_errors();
    printf("romantext: all tests passed\n");
    return 0;
}
