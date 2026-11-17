#include "io/tests/export/score_test_util.h"

/* musicxml_export_write_score: notes and rests are written as notated values (type, dots, ties, triplets). */

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

static void test_eighth_stays_one_eighth(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "e", 0, 1, 1, 8, PITCH_LETTER_C, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<type>eighth</type>") == 2); /* the note, and the eighth rest that completes the beat */
    assert(!TU_HAS(xml, "<tie") && !TU_HAS(xml, "<tied") && !TU_HAS(xml, "<dot/>"));
    assert(TU_HAS(xml, "<duration>1</duration>\n        <voice>1</voice>\n        <type>eighth</type>"));
    free(xml);
    bag_free(&bag);
}

static void test_dotted_quarter_on_a_beat(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "d", 0, 1, 3, 8, PITCH_LETTER_C, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(TU_HAS(xml, "<type>quarter</type>\n        <dot/>\n"));
    assert(tu_count(xml, "<dot/>") == 1 && !TU_HAS(xml, "<tied"));
    free(xml);
    bag_free(&bag);
}

static void test_four_four_half_bar(void) {
    /* a half note on beat 1 is one value */
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "h", 0, 1, 1, 2, PITCH_LETTER_C, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<type>half</type>") == 2); /* the note and the half rest after it */
    assert(!TU_HAS(xml, "<tied"));
    free(xml);
    bag_free(&bag);

    /* on beat 2 it would cross beat 3: two tied quarters */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "h", 1, 4, 1, 2, PITCH_LETTER_C, NULL);
    xml = write_notes(&bag, NULL);
    assert(!TU_HAS(xml, "<type>half</type>"));
    assert(TU_HAS(xml, "<note xml:id=\"h_a\">") && TU_HAS(xml, "<note xml:id=\"h_b\">"));
    assert(tu_count(xml, "<tied type=\"start\"/>") == 1 && tu_count(xml, "<tied type=\"stop\"/>") == 1);
    assert(tu_count(xml, "<type>quarter</type>") == 4); /* leading rest, the two tied notes, trailing rest */
    free(xml);
    bag_free(&bag);

    /* a dotted half on beat 1 may go through beat 3 */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "d", 0, 1, 3, 4, PITCH_LETTER_C, NULL);
    xml = write_notes(&bag, NULL);
    assert(TU_HAS(xml, "<type>half</type>\n        <dot/>\n") && !TU_HAS(xml, "<tied"));
    free(xml);
    bag_free(&bag);
}

static void test_six_eight_grouping(void) {
    const TuMeter six_eight[] = {{6, 8, 0, 1}};
    MeterMap *meter = tu_meter_map(six_eight, 1);

    /* a quarter that ends on the second dotted-quarter beat stays whole */
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "q", 1, 8, 1, 4, PITCH_LETTER_C, NULL);
    char *xml = write_notes(&bag, meter);
    assert(!TU_HAS(xml, "<tied") && TU_HAS(xml, "<type>quarter</type>"));
    free(xml);
    bag_free(&bag);

    /* a quarter across the middle of a beat is two tied eighths */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "q", 1, 4, 1, 4, PITCH_LETTER_C, NULL);
    xml = write_notes(&bag, meter);
    assert(TU_HAS(xml, "<note xml:id=\"q_a\">") && TU_HAS(xml, "<note xml:id=\"q_b\">"));
    assert(tu_count(xml, "<tied type=\"start\"/>") == 1);
    free(xml);
    bag_free(&bag);

    /* a dotted half on beat 1 covers both beats as one value */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "d", 0, 1, 3, 4, PITCH_LETTER_C, NULL);
    xml = write_notes(&bag, meter);
    assert(TU_HAS(xml, "<type>half</type>\n        <dot/>\n") && !TU_HAS(xml, "<tied") && !TU_HAS(xml, "<rest"));
    free(xml);
    bag_free(&bag);
    meter_map_destroy(meter);
}

static void test_empty_measure_is_a_measure_rest(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 1, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 2, 1, 1, 1, PITCH_LETTER_D, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<rest measure=\"yes\"/>") == 1);
    assert(TU_HAS(xml, "<rest measure=\"yes\"/>\n        <duration>4</duration>"));
    assert(tu_count(xml, "<type>whole</type>") == 2 && !TU_HAS(xml, "<rest/>"));
    free(xml);
    bag_free(&bag);
}

static void test_triplet_eighths(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "t0", 0, 1, 1, 12, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "t1", 1, 12, 1, 12, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "t2", 1, 6, 1, 12, PITCH_LETTER_E, NULL);
    bag_add_note(&bag, "q", 1, 4, 1, 4, PITCH_LETTER_F, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<type>eighth</type>") == 3);
    assert(tu_count(xml, "<time-modification>") == 3);
    assert(tu_count(xml, "<actual-notes>3</actual-notes>") == 3 && tu_count(xml, "<normal-notes>2</normal-notes>") == 3);
    assert(tu_count(xml, "<tuplet type=\"start\"/>") == 1 && tu_count(xml, "<tuplet type=\"stop\"/>") == 1);
    /* the group opens on the first note and closes on the third, in MusicXML element order */
    const char *first = strstr(xml, "<note xml:id=\"t0\">");
    const char *start = strstr(xml, "<tuplet type=\"start\"/>");
    const char *stop = strstr(xml, "<tuplet type=\"stop\"/>");
    const char *third = strstr(xml, "<note xml:id=\"t2\">");
    const char *quarter = strstr(xml, "<note xml:id=\"q\">");
    assert(first && start && stop && third && quarter);
    assert(first < start && start < third && third < stop && stop < quarter);
    assert(TU_HAS(xml, "</time-modification>\n        <beam number=\"1\">begin</beam>\n        <notations>\n          <tuplet type=\"start\"/>"));
    assert(tu_count(xml, "<beam number=\"1\">continue</beam>") == 1 && tu_count(xml, "<beam number=\"1\">end</beam>") == 1);
    free(xml);
    bag_free(&bag);
}

/* A notated tuplet is classified by its own ratio: five sixteenths in the time of four (a quintuplet), at any position. */
static void test_quintuplet_sixteenths(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    const char *ids[5] = {"q0", "q1", "q2", "q3", "q4"};
    for (long i = 0; i < 5; i++) {
        bag_add_tuplet_note(&bag, ids[i], i, 20, 1, 20, (PitchLetter)(PITCH_LETTER_C + i), 5, 4);
    }
    bag_add_note(&bag, "h", 1, 4, 1, 4, PITCH_LETTER_G, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<type>16th</type>") == 5 && !TU_HAS(xml, "<type>32nd</type>"));
    assert(tu_count(xml, "<time-modification>") == 5);
    assert(tu_count(xml, "<actual-notes>5</actual-notes>") == 5 && tu_count(xml, "<normal-notes>4</normal-notes>") == 5);
    assert(!TU_HAS(xml, "<actual-notes>3</actual-notes>") && !TU_HAS(xml, "<tied"));
    assert(tu_count(xml, "<tuplet type=\"start\"/>") == 1 && tu_count(xml, "<tuplet type=\"stop\"/>") == 1);
    const char *first = strstr(xml, "<note xml:id=\"q0\">");
    const char *start = strstr(xml, "<tuplet type=\"start\"/>");
    const char *last = strstr(xml, "<note xml:id=\"q4\">");
    const char *stop = strstr(xml, "<tuplet type=\"stop\"/>");
    assert(first && start && last && stop && first < start && start < last && last < stop);
    assert(tu_count(xml, "<beam number=\"2\">begin</beam>") == 1 && tu_count(xml, "<beam number=\"2\">end</beam>") == 1);
    free(xml);
    bag_free(&bag);
}

/* A duplet (2:3): two eighths in the time of three, each 3/16 of a whole note, across a beat line. */
static void test_duplet_eighths(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_tuplet_note(&bag, "d0", 0, 1, 3, 16, PITCH_LETTER_C, 2, 3);
    bag_add_tuplet_note(&bag, "d1", 3, 16, 3, 16, PITCH_LETTER_D, 2, 3);
    bag_add_note(&bag, "q", 3, 8, 1, 8, PITCH_LETTER_E, NULL);
    char *xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<type>eighth</type>") >= 2 && !TU_HAS(xml, "<tied"));
    assert(tu_count(xml, "<time-modification>") == 2);
    assert(tu_count(xml, "<actual-notes>2</actual-notes>") == 2 && tu_count(xml, "<normal-notes>3</normal-notes>") == 2);
    assert(tu_count(xml, "<tuplet type=\"start\"/>") == 1 && tu_count(xml, "<tuplet type=\"stop\"/>") == 1);
    assert(strstr(xml, "<tuplet type=\"start\"/>") < strstr(xml, "<note xml:id=\"d1\">"));
    free(xml);
    bag_free(&bag);
}

/* A dotted value under a ratio (the hint's value is the duration times actual over normal, plain or dotted), a 3:2 hint
   as the inferred triplet, and a ratio beyond 15 notes, which is not written. */
static void test_dotted_and_unfitting_hints(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    /* 7:4 dotted eighths: 3/16 * 4/7 = 3/28 of a whole note each */
    bag_add_tuplet_note(&bag, "s0", 0, 1, 3, 28, PITCH_LETTER_C, 7, 4);
    char *xml = write_notes(&bag, NULL);
    assert(TU_HAS(xml, "<type>eighth</type>\n        <dot/>\n"));
    assert(tu_count(xml, "<actual-notes>7</actual-notes>") == 1 && tu_count(xml, "<normal-notes>4</normal-notes>") == 1);
    free(xml);
    bag_free(&bag);

    /* a 3:2 hint on triplet eighths is written as the inferred triplet is */
    memset(&bag, 0, sizeof(bag));
    bag_add_tuplet_note(&bag, "f0", 0, 1, 1, 12, PITCH_LETTER_C, 3, 2);
    bag_add_tuplet_note(&bag, "f1", 1, 12, 1, 12, PITCH_LETTER_D, 3, 2);
    bag_add_tuplet_note(&bag, "f2", 1, 6, 1, 12, PITCH_LETTER_E, 3, 2);
    xml = write_notes(&bag, NULL);
    assert(tu_count(xml, "<type>eighth</type>") >= 3 && tu_count(xml, "<actual-notes>3</actual-notes>") == 3);
    free(xml);
    bag_free(&bag);

    /* a ratio beyond 15 notes is not written: the note is classified as if it had none (a plain eighth) */
    memset(&bag, 0, sizeof(bag));
    bag_add_tuplet_note(&bag, "w0", 0, 1, 1, 8, PITCH_LETTER_C, 20, 16);
    xml = write_notes(&bag, NULL);
    assert(!TU_HAS(xml, "<time-modification>") && TU_HAS(xml, "<type>eighth</type>"));
    free(xml);
    bag_free(&bag);
}

int main(void) {
    test_eighth_stays_one_eighth();
    test_dotted_quarter_on_a_beat();
    test_four_four_half_bar();
    test_six_eight_grouping();
    test_empty_measure_is_a_measure_rest();
    test_triplet_eighths();
    test_quintuplet_sixteenths();
    test_duplet_eighths();
    test_dotted_and_unfitting_hints();
    printf("test_musicxml_score_values: all tests passed\n");
    return 0;
}
