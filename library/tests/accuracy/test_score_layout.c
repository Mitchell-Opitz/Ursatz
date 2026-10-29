#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "score_layout.h"

/* Two parts; only the first is read. Divisions 2 (eighth = 1 unit): a quarter pickup (m0), a full 3/4 measure
   (m1), a measure split into "2" (half) and "2a" (quarter), then a last short measure. */
static const char *XML =
    "<?xml version=\"1.0\"?><score-partwise><part-list><score-part id=\"P1\"/><score-part id=\"P2\"/></part-list>"
    "<part id=\"P1\">"
    "<measure number=\"0\" implicit=\"yes\"><attributes><divisions>2</divisions><time><beats>3</beats>"
    "<beat-type>4</beat-type></time></attributes><note><pitch><step>C</step><octave>4</octave></pitch>"
    "<duration>2</duration></note></measure>"
    "<measure number=\"1\"><note><rest/><duration>4</duration></note>"
    "<note><pitch><step>D</step><octave>4</octave></pitch><duration>2</duration></note></measure>"
    "<measure number=\"2\"><note><pitch><step>E</step><octave>4</octave></pitch><duration>4</duration></note>"
    "<note><chord/><pitch><step>G</step><octave>4</octave></pitch><duration>4</duration></note>"
    "<note><grace/><pitch><step>F</step><octave>4</octave></pitch></note></measure>"
    "<measure number=\"2a\"><note><pitch><step>F</step><octave>4</octave></pitch><duration>2</duration></note>"
    "</measure>"
    "<measure number=\"3\"><note><pitch><step>G</step><octave>4</octave></pitch><duration>4</duration></note>"
    "<backup><duration>4</duration></backup><note><pitch><step>C</step><octave>3</octave></pitch>"
    "<duration>6</duration></note></measure>"
    "</part>"
    "<part id=\"P2\"><measure number=\"0\"><note><rest/><duration>99</duration></note></measure></part>"
    "</score-partwise>";

static bool time_is(const MusicalTime *t, long n, unsigned long d) {
    return musical_time_numerator(t) * (long)d == n * (long)musical_time_denominator(t);
}

static void test_layout(void) {
    ScoreLayout layout;
    assert(score_layout_read(XML, strlen(XML), &layout) == SCORE_LAYOUT_OK);
    assert(layout.time_top == 3 && layout.time_bottom == 4);
    assert(layout.measure_count == 4 && layout.continued_count == 1);
    assert(layout.has_pickup && layout.irregular_count == 0);
    assert(layout.measures[0].number == 0 && layout.measures[0].start.num == 0 && layout.measures[0].length.num == 1 &&
           layout.measures[0].length.den == 4);
    assert(layout.measures[1].start.num == 1 && layout.measures[1].start.den == 4); /* m1 starts after the pickup */
    assert(layout.measures[2].start.num == 1 && layout.measures[2].start.den == 1);
    assert(layout.measures[2].number == 2 && layout.measures[2].length.num == 3 && layout.measures[2].length.den == 4);
    assert(layout.measures[3].number == 3 && layout.measures[3].length.num == 3 && layout.measures[3].length.den == 4);
    assert(layout.end.num == 5 && layout.end.den == 2 && layout.measures[3].start.num == 7 && layout.measures[3].start.den == 4);
    score_layout_destroy(&layout);
}

static void test_grid_and_meter_map(void) {
    ScoreLayout layout;
    assert(score_layout_read(XML, strlen(XML), &layout) == SCORE_LAYOUT_OK);
    RomanTextGrid grid = score_layout_grid(&layout);
    MusicalTime *t = NULL;
    assert(grid.time_at(grid.context, 0, 3000, &t) && time_is(t, 0, 1)); /* the pickup's beat 3 */
    musical_time_destroy(t);
    assert(!grid.time_at(grid.context, 0, 1000, &t)); /* before the pickup */
    /* measure 2 starts at 1/4 + 3/4 = 1; beat 3 of 3/4 is 1/2 later: 3/2 */
    assert(grid.time_at(grid.context, 2, 3000, &t) && time_is(t, 3, 2));
    musical_time_destroy(t);
    assert(!grid.time_at(grid.context, 9, 1000, &t));

    MeterMap *map = score_layout_meter_map(&layout, "score-layout-test");
    assert(map != NULL && meter_map_has_pickup(map));
    int index = -1;
    MusicalTime *at = NULL;
    assert(musical_time_create(1, 1, &at) == MUSICAL_TIME_OK);
    assert(meter_map_measure_index_at(map, at, &index) == METER_MAP_OK && index == 2);
    musical_time_destroy(at);
    meter_map_destroy(map);
    score_layout_destroy(&layout);
}

static void test_errors(void) {
    ScoreLayout layout;
    assert(score_layout_read(NULL, 0, &layout) == SCORE_LAYOUT_ERR_NULL_ARGUMENT);
    assert(score_layout_read("<score-partwise/>", 17, &layout) == SCORE_LAYOUT_ERR_NO_MEASURES);
}

int main(void) {
    test_layout();
    test_grid_and_meter_map();
    test_errors();
    printf("score layout: all tests passed\n");
    return 0;
}
