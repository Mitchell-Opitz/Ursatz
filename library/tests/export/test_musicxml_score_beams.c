#include "score_test_util.h"

/* musicxml_export_write_score: short notes are beamed by beat group, with hooks for a lone short value. */

static char *write_notes(const TestBag *bag, const MeterMap *meter) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    score.meter_map = meter;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(xml != NULL);
    return xml;
}

static void add_run(TestBag *bag, const char *prefix, long first, unsigned long count, unsigned long unit) {
    for (unsigned long i = 0; i < count; i++) {
        char id[48];
        snprintf(id, sizeof(id), "%s%lu", prefix, i);
        bag_add_note(bag, id, first + (long)i, unit, 1, unit, PITCH_LETTER_C, NULL);
    }
}

static void test_three_four_eighths_per_beat(void) {
    const TuMeter three_four[] = {{3, 4, 0, 1}};
    MeterMap *meter = tu_meter_map(three_four, 1);
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    add_run(&bag, "e", 0, 6, 8);
    char *xml = write_notes(&bag, meter);
    assert(tu_count(xml, "<beam number=\"1\">begin</beam>") == 3);
    assert(tu_count(xml, "<beam number=\"1\">end</beam>") == 3);
    assert(!TU_HAS(xml, "continue") && !TU_HAS(xml, "<beam number=\"2\">"));
    /* the first beam closes on the second note, so the third note opens the next one */
    const char *end = strstr(xml, "<beam number=\"1\">end</beam>");
    const char *second_begin = strstr(strstr(xml, "<beam number=\"1\">begin</beam>") + 1, "<beam number=\"1\">begin</beam>");
    assert(end && second_begin && end < second_begin);
    /* element order: <staff> is absent for one staff, <beam> follows <voice>/<type> and precedes <notations> */
    assert(TU_HAS(xml, "<type>eighth</type>\n        <beam number=\"1\">begin</beam>\n      </note>"));
    free(xml);
    bag_free(&bag);
    meter_map_destroy(meter);
}

static void test_sixteenths_have_two_levels(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    add_run(&bag, "s", 0, 4, 16);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<beam number=\"1\">begin</beam>") == 1 && tu_count(xml, "<beam number=\"2\">begin</beam>") == 1);
    assert(tu_count(xml, "<beam number=\"1\">continue</beam>") == 2 && tu_count(xml, "<beam number=\"2\">continue</beam>") == 2);
    assert(tu_count(xml, "<beam number=\"1\">end</beam>") == 1 && tu_count(xml, "<beam number=\"2\">end</beam>") == 1);
    assert(!TU_HAS(xml, "<beam number=\"3\">"));
    free(xml);
    bag_free(&bag);
}

static void test_thirty_seconds_have_three_levels(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    add_run(&bag, "t", 0, 2, 32);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<beam number=\"3\">begin</beam>") == 1 && tu_count(xml, "<beam number=\"3\">end</beam>") == 1);
    free(xml);
    bag_free(&bag);
}

static void test_hooks_for_a_lone_short_value(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "d", 0, 1, 3, 16, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "s", 3, 16, 1, 16, PITCH_LETTER_D, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<beam number=\"1\">begin</beam>") == 1 && tu_count(xml, "<beam number=\"1\">end</beam>") == 1);
    assert(tu_count(xml, "<beam number=\"2\">backward hook</beam>") == 1 && !TU_HAS(xml, "forward hook"));
    free(xml);
    bag_free(&bag);

    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "s", 0, 1, 1, 16, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "d", 1, 16, 3, 16, PITCH_LETTER_D, NULL);
    xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<beam number=\"2\">forward hook</beam>") == 1 && !TU_HAS(xml, "backward hook"));
    free(xml);
    bag_free(&bag);
}

static void test_rest_and_lone_note_break_groups(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 8, PITCH_LETTER_C, NULL); /* alone in beat 1: the eighth rest after it ends the group */
    bag_add_note(&bag, "b", 1, 4, 1, 8, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "c", 3, 8, 1, 8, PITCH_LETTER_E, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<beam ") == 2);
    const char *a = strstr(xml, "<note xml:id=\"a\">");
    const char *b = strstr(xml, "<note xml:id=\"b\">");
    assert(a && b && strstr(a, "<beam ") > b);
    free(xml);
    bag_free(&bag);

    /* a quarter between two eighths of different beats breaks nothing it should not: no beam crosses a beat */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 1, 8, 1, 8, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 1, 4, 1, 8, PITCH_LETTER_D, NULL);
    xml = write_notes(&bag, NULL);
    assert(!TU_HAS(xml, "<beam "));
    free(xml);
    bag_free(&bag);
}

static void test_six_eight_groups_of_three(void) {
    const TuMeter six_eight[] = {{6, 8, 0, 1}};
    MeterMap *meter = tu_meter_map(six_eight, 1);
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    add_run(&bag, "e", 0, 6, 8);
    char *xml = write_notes(&bag, meter);
    assert(tu_count(xml, "<beam number=\"1\">begin</beam>") == 2);
    assert(tu_count(xml, "<beam number=\"1\">continue</beam>") == 2);
    assert(tu_count(xml, "<beam number=\"1\">end</beam>") == 2);
    free(xml);
    bag_free(&bag);
    meter_map_destroy(meter);
}

static void test_chord_is_beamed_once(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "c1", 0, 1, 1, 8, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "c2", 0, 1, 1, 8, PITCH_LETTER_E, NULL);
    bag_add_note(&bag, "n", 1, 8, 1, 8, PITCH_LETTER_G, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(TU_HAS(xml, "<chord/>"));
    assert(tu_count(xml, "<beam ") == 2);
    assert(tu_count(xml, "<beam number=\"1\">begin</beam>") == 1 && tu_count(xml, "<beam number=\"1\">end</beam>") == 1);
    free(xml);
    bag_free(&bag);
}

static void test_longer_notes_are_not_beamed(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    add_run(&bag, "q", 0, 4, 4);
    char *xml = write_notes(&bag, NULL);
    assert(!TU_HAS(xml, "<beam "));
    free(xml);
    bag_free(&bag);
}

int main(void) {
    test_three_four_eighths_per_beat();
    test_sixteenths_have_two_levels();
    test_thirty_seconds_have_three_levels();
    test_hooks_for_a_lone_short_value();
    test_rest_and_lone_note_break_groups();
    test_six_eight_groups_of_three();
    test_chord_is_beamed_once();
    test_longer_notes_are_not_beamed();
    printf("test_musicxml_score_beams: all tests passed\n");
    return 0;
}
