#include "score_test_util.h"

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
    assert(TU_HAS(xml, "</time-modification>\n        <notations>\n          <tuplet type=\"start\"/>"));
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
    printf("test_musicxml_score_values: all tests passed\n");
    return 0;
}
