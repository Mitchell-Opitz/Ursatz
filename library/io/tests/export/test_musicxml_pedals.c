#include "io/tests/export/score_test_util.h"

/*
 * Sustain pedal markings (MusicXmlExportScore.pedals): start and stop in one measure, across a barline, at the last measure's end
 * (the stop adds no measure), on staff 2 of a grand staff, a time inside a note (the note is cut and tied there), and the errors.
 */

#define START "<pedal type=\"start\" line=\"yes\"/>"
#define STOP "<pedal type=\"stop\" line=\"yes\"/>"

static TimeSpan span(long long a, long long b, long long c, long long d) {
    TimeSpan s;
    assert(time_span_make(tu_time_value(a, b), tu_time_value(c, d), &s) == TIME_SPAN_OK);
    return s;
}

/* The 4/4 measure of a quarter-note run: `count` quarter notes from 0 in part 0 staff 1 (and `bass` more in staff 2). */
static void quarters(TestBag *bag, int count, int bass_count) {
    char id[16];
    for (int i = 0; i < count; i++) {
        snprintf(id, sizeof(id), "n%d", i);
        bag_add(bag, tu_note(id, i, 4, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 1);
    }
    for (int i = 0; i < bass_count; i++) {
        snprintf(id, sizeof(id), "b%d", i);
        bag_add(bag, tu_note(id, i, 4, 1, 4, PITCH_LETTER_C, 0, 3, NULL), 0, 2);
    }
}

static MusicXmlExportResult write(TestBag *bag, const MusicXmlExportPart *parts, size_t part_count, const MusicXmlExportPedal *pedals,
                                  size_t pedal_count, char **xml) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    score.part_index = bag->part;
    score.staff = bag->staff;
    score.parts = parts;
    score.part_count = part_count;
    score.pedals = pedals;
    score.pedal_count = pedal_count;
    return musicxml_export_write_score(&score, xml);
}

static const char *nth(const char *xml, const char *needle, size_t n) {
    const char *at = xml;
    for (size_t i = 0; i <= n; i++) {
        at = strstr(at, needle);
        if (at == NULL) {
            return NULL;
        }
        at += strlen(needle);
    }
    return at;
}

static void test_one_measure(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    MusicXmlExportPedal pedal = {span(0, 1, 1, 2), 0, 1};
    char *xml = NULL;
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_OK);
    assert(tu_count(xml, START) == 1 && tu_count(xml, STOP) == 1 && tu_count(xml, "<measure ") == 1);
    const char *start = strstr(xml, START), *stop = strstr(xml, STOP);
    assert(start < stop && stop < strstr(xml, "</measure>"));
    assert(strstr(xml, "<direction placement=\"below\">\n        <direction-type>\n          " START) != NULL);
    assert(!TU_HAS(xml, "<staff>") && !TU_HAS(xml, "<offset>")); /* one staff: no <staff>; both times are note starts */
    /* the start comes before the first note, the stop before the third note: the notes are 4 quarters, the stop at the half */
    assert(start < nth(xml, "<note", 0) && stop > nth(xml, "<note", 1) && stop < nth(xml, "<note", 2));
    free(xml);
    bag_free(&bag);
}

static void test_across_a_barline(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    char id[8];
    for (int i = 0; i < 4; i++) {
        snprintf(id, sizeof(id), "m%d", i);
        bag_add(&bag, tu_note(id, 4 + i, 4, 1, 4, PITCH_LETTER_D, 0, 4, NULL), 0, 1);
    }
    MusicXmlExportPedal pedal = {span(1, 2, 5, 4), 0, 1};
    char *xml = NULL;
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_OK);
    assert(tu_count(xml, "<measure ") == 2);
    const char *second = strstr(xml, "<measure number=\"2\"");
    assert(second != NULL && strstr(xml, START) < second && strstr(xml, STOP) > second);
    free(xml);
    bag_free(&bag);
}

static void test_stop_at_the_last_measures_end(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    MusicXmlExportPedal pedal = {span(1, 4, 1, 1), 0, 1};
    char *xml = NULL;
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_OK);
    assert(tu_count(xml, "<measure ") == 1); /* the stop on the barline adds no measure */
    const char *stop = strstr(xml, STOP);
    assert(stop != NULL && strstr(xml, START) < stop && stop > strstr(xml, "<note") && stop < strstr(xml, "</measure>"));
    assert(strstr(stop, "<note") == NULL); /* written after the last note, at the measure's end */
    assert(!TU_HAS(xml, "<offset>"));      /* its time is the end of the measure */
    free(xml);
    bag_free(&bag);
}

static void test_grand_staff(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 4);
    MusicXmlExportPart part = {"P1", "Piano", 2, 0};
    MusicXmlExportPedal pedal = {span(0, 1, 3, 4), 0, 2};
    char *xml = NULL;
    assert(write(&bag, &part, 1, &pedal, 1, &xml) == MUSICXML_EXPORT_OK);
    assert(TU_HAS(xml, "<staves>2</staves>"));
    const char *start = strstr(xml, START);
    assert(start != NULL && strncmp(strstr(start, "</direction-type>") + strlen("</direction-type>\n"), "        <staff>2</staff>", 22) == 0);
    const char *stop = strstr(xml, STOP);
    assert(stop != NULL && strstr(stop, "<staff>2</staff>") < strstr(stop, "</direction>"));
    free(xml);
    bag_free(&bag);
}

static void test_a_time_inside_a_note(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    MusicXmlExportPedal pedal = {span(1, 8, 3, 8), 0, 1}; /* inside the first and the second note */
    char *xml = NULL;
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_OK);
    assert(tu_count(xml, START) == 1 && tu_count(xml, STOP) == 1 && !TU_HAS(xml, "<offset>"));
    /* the first note is cut at the pedal's start and tied; the marking sits between its two pieces */
    const char *first = nth(xml, "<note", 0), *second = nth(xml, "<note", 1);
    assert(strstr(xml, START) > first && strstr(xml, START) < second && tu_count(xml, "<tie type=\"start\"/>") == 2);
    free(xml);
    bag_free(&bag);
    /* a time inside the last note of a measure stays in that measure */
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    MusicXmlExportPedal late = {span(7, 8, 15, 16), 0, 1};
    assert(write(&bag, NULL, 0, &late, 1, &xml) == MUSICXML_EXPORT_OK);
    assert(tu_count(xml, "<measure ") == 1 && strstr(xml, START) < strstr(xml, "</measure>") && strstr(xml, STOP) < strstr(xml, "</measure>"));
    free(xml);
    bag_free(&bag);
}

static void test_two_pedals_and_a_repedal(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    MusicXmlExportPedal pedals[2] = {{span(0, 1, 1, 4), 0, 1}, {span(1, 4, 1, 2), 0, 1}};
    char *xml = NULL;
    assert(write(&bag, NULL, 0, pedals, 2, &xml) == MUSICXML_EXPORT_OK);
    assert(tu_count(xml, START) == 2 && tu_count(xml, STOP) == 2);
    /* at the shared time the first pedal's stop comes before the second's start */
    const char *first_note = nth(xml, "<note", 0), *second_note = nth(xml, "<note", 1);
    const char *stop = strstr(first_note, STOP);
    const char *start = strstr(first_note, START);
    assert(stop != NULL && start != NULL && stop < start && start < second_note);
    free(xml);
    bag_free(&bag);
}

static void test_errors(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    quarters(&bag, 4, 0);
    char *xml = (char *)(uintptr_t)0x1;
    MusicXmlExportPedal pedal = {span(0, 1, 1, 2), 1, 1};
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_ERR_UNKNOWN_PART && xml == (char *)(uintptr_t)0x1);
    pedal.part_index = 0;
    pedal.staff = 2; /* one staff */
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_ERR_BAD_STAFF);
    pedal.staff = 0;
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_ERR_BAD_STAFF);
    pedal.staff = 1;
    pedal.span.end = pedal.span.start; /* does not hold */
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    pedal.span.start = tu_time_value(-1, 4);
    pedal.span.end = tu_time_value(1, 4);
    assert(write(&bag, NULL, 0, &pedal, 1, &xml) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    assert(write(&bag, NULL, 0, NULL, 1, &xml) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);
    assert(xml == (char *)(uintptr_t)0x1);
    /* a count of 0 with an array writes no pedal */
    pedal.span = span(0, 1, 1, 2);
    assert(write(&bag, NULL, 0, &pedal, 0, &xml) == MUSICXML_EXPORT_OK && !TU_HAS(xml, "<pedal"));
    free(xml);
    bag_free(&bag);
}

int main(void) {
    test_one_measure();
    test_across_a_barline();
    test_stop_at_the_last_measures_end();
    test_grand_staff();
    test_a_time_inside_a_note();
    test_two_pedals_and_a_repedal();
    test_errors();
    printf("test_musicxml_pedals: all tests passed\n");
    return 0;
}
