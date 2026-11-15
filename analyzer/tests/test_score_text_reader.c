#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export/score_text_reader.h"

/*
 * The private score MusicXML reader of the text export (src/export/score_text_reader.h). A hand-written score holds a
 * pickup, a chord, a tie across a barline, a 3:2 triplet, two staves, two voices, a rest, a second part, a forward and
 * a time signature change, with elements the reader skips; every field the export prints is read back and checked. A
 * second, bare score checks the defaults and the notated value taken from the duration, and the refusals follow.
 */

static int failures = 0;

#define CHECK(condition, ...)                                    \
    do {                                                         \
        if (!(condition)) {                                      \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
            failures++;                                          \
        }                                                        \
    } while (0)

#define IS_RATIONAL(value, n, d) ((value).num == (n) && (value).den == (d))

/* divisions 6: a quarter is 6, a triplet eighth 2. Part 1 has two staves; part 2 has one. */
static const char SCORE[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
    "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
    "<!-- a hand-written score -->\n"
    "<score-partwise version=\"3.1\">\n"
    "  <part-list><score-part id=\"P1\"><part-name>Piano</part-name></score-part>\n"
    "    <score-part id=\"P2\"><part-name>Flute</part-name></score-part></part-list>\n"
    "  <part id=\"P1\">\n"
    "    <measure number=\"0\" implicit=\"yes\">\n"
    "      <attributes><divisions>6</divisions><key><fifths>0</fifths></key>"
    "<time><beats>4</beats><beat-type>4</beat-type></time><staves>2</staves></attributes>\n"
    "      <direction placement=\"above\"><direction-type><words>dolce</words></direction-type></direction>\n"
    "      <note xml:id=\"pickup-g\"><pitch><step>G</step><octave>4</octave></pitch><duration>6</duration>"
    "<voice>1</voice><type>quarter</type><staff>1</staff></note>\n"
    "    </measure>\n"
    "    <measure number=\"1\">\n"
    "      <note><pitch><step>C</step><octave>5</octave></pitch><duration>12</duration><voice>1</voice>"
    "<type>half</type><staff>1</staff></note>\n"
    "      <note><pitch><step>D</step><octave>5</octave></pitch><duration>2</duration><voice>1</voice>"
    "<type>eighth</type><time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes>"
    "</time-modification><staff>1</staff><beam number=\"1\">begin</beam></note>\n"
    "      <note><pitch><step>E</step><octave>5</octave></pitch><duration>2</duration><voice>1</voice>"
    "<type>eighth</type><time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes>"
    "</time-modification><staff>1</staff></note>\n"
    "      <note><pitch><step>F</step><octave>5</octave></pitch><duration>2</duration><voice>1</voice>"
    "<type>eighth</type><time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes>"
    "</time-modification><staff>1</staff></note>\n"
    "      <note><rest/><duration>6</duration><voice>1</voice><type>quarter</type><staff>1</staff></note>\n"
    "      <backup><duration>24</duration></backup>\n"
    "      <note><pitch><step>C</step><octave>4</octave></pitch><duration>24</duration><voice>2</voice>"
    "<type>whole</type><staff>1</staff></note>\n"
    "      <note><chord/><pitch><step>E</step><octave>4</octave></pitch><duration>24</duration><voice>2</voice>"
    "<type>whole</type><staff>1</staff></note>\n"
    "      <note><chord/><pitch><step>G</step><octave>4</octave></pitch><duration>24</duration><voice>2</voice>"
    "<type>whole</type><staff>1</staff></note>\n"
    "      <backup><duration>24</duration></backup>\n"
    "      <note><pitch><step>F</step><alter>1</alter><octave>2</octave></pitch><duration>9</duration>"
    "<voice>3</voice><type>quarter</type><dot/><staff>2</staff></note>\n"
    "      <note><pitch><step>B</step><alter>-1</alter><octave>2</octave></pitch><duration>3</duration>"
    "<tie type=\"start\"/><voice>3</voice><type>eighth</type><staff>2</staff>"
    "<notations><tied type=\"start\"/></notations></note>\n"
    "      <note><rest/><duration>12</duration><voice>3</voice><type>half</type><staff>2</staff></note>\n"
    "    </measure>\n"
    "    <measure number=\"2\">\n"
    "      <attributes><time><beats>3</beats><beat-type>4</beat-type></time></attributes>\n"
    "      <note xml:id=\"last-a\"><pitch><step>A</step><octave>4</octave></pitch><duration>18</duration>"
    "<voice>1</voice><type>half</type><dot/><staff>1</staff></note>\n"
    "      <backup><duration>18</duration></backup>\n"
    "      <note><pitch><step>B</step><alter>-1</alter><octave>2</octave></pitch><duration>12</duration>"
    "<tie type=\"stop\"/><voice>3</voice><type>half</type><staff>2</staff></note>\n"
    "      <note><rest/><duration>6</duration><voice>3</voice><type>quarter</type><staff>2</staff></note>\n"
    "    </measure>\n"
    "  </part>\n"
    "  <part id=\"P2\">\n"
    "    <measure number=\"0\" implicit=\"yes\"><attributes><divisions>6</divisions></attributes></measure>\n"
    "    <measure number=\"1\">\n"
    "      <note><pitch><step>B</step><octave>3</octave></pitch><duration>12</duration></note>\n"
    "      <note><pitch><step>C</step><octave>4</octave></pitch><duration>12</duration><type>half</type></note>\n"
    "    </measure>\n"
    "    <measure number=\"2\">\n"
    "      <forward><duration>6</duration></forward>\n"
    "      <note><pitch><step>D</step><octave>4</octave></pitch><duration>12</duration><type>half</type></note>\n"
    "    </measure>\n"
    "  </part>\n"
    "</score-partwise>\n";

static const char BARE[] =
    "<score-partwise><part id=\"P1\"><measure number=\"1\">"
    "<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration></note>"
    "</measure></part></score-partwise>";

/* divisions 6, no types: a dotted quarter (9), a triplet eighth (2) and a half (12). */
static const char UNTYPED[] =
    "<score-partwise><part id=\"P1\"><measure number=\"7\">"
    "<attributes><divisions>6</divisions></attributes>"
    "<note><pitch><step>C</step><octave>4</octave></pitch><duration>9</duration></note>"
    "<note><pitch><step>D</step><octave>4</octave></pitch><duration>2</duration>"
    "<time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification></note>"
    "<note><pitch><step>E</step><octave>4</octave></pitch><duration>12</duration></note>"
    "</measure></part></score-partwise>";

static void check_rationals(void) {
    TextRational half = text_rational(2, 4);
    CHECK(IS_RATIONAL(half, 1, 2), "2/4 reduces to 1/2, got %lld/%lld", (long long)half.num, (long long)half.den);
    TextRational negative = text_rational(3, -6);
    CHECK(IS_RATIONAL(negative, -1, 2), "3/-6 is -1/2");
    TextRational zero = text_rational(0, 7);
    CHECK(IS_RATIONAL(zero, 0, 1), "0/7 is 0/1");
    TextRational bad = text_rational(1, 0);
    CHECK(IS_RATIONAL(bad, 0, 1), "a zero denominator reads as 0/1");
    CHECK(IS_RATIONAL(text_rational_add(text_rational(1, 3), text_rational(1, 6)), 1, 2), "1/3 + 1/6");
    CHECK(IS_RATIONAL(text_rational_sub(text_rational(1, 4), text_rational(1, 2)), -1, 4), "1/4 - 1/2");
    CHECK(IS_RATIONAL(text_rational_mul(text_rational(2, 3), text_rational(3, 4)), 1, 2), "2/3 * 3/4");
    CHECK(text_rational_compare(text_rational(1, 3), text_rational(1, 2)) < 0, "1/3 < 1/2");
    CHECK(text_rational_compare(text_rational(2, 4), text_rational(1, 2)) == 0, "2/4 == 1/2");
    CHECK(text_rational_compare(text_rational(5, 3), text_rational(3, 2)) > 0, "5/3 > 3/2");
}

static void check_measures(const ScoreTextScore *score) {
    CHECK(score->part_count == 2, "two parts, got %zu", score->part_count);
    CHECK(score->measure_count == 3, "three measures, got %zu", score->measure_count);
    if (score->measure_count != 3) {
        return;
    }
    const ScoreTextMeasure *pickup = &score->measures[0];
    const ScoreTextMeasure *full = &score->measures[1];
    const ScoreTextMeasure *changed = &score->measures[2];

    CHECK(strcmp(pickup->number, "0") == 0 && pickup->pickup, "measure 0 is the pickup");
    CHECK(IS_RATIONAL(pickup->start, 0, 1) && IS_RATIONAL(pickup->length, 1, 4), "the pickup is a quarter long");
    CHECK(pickup->beats == 4 && pickup->beat_type == 4 && pickup->time_changed, "the first measure states 4/4");

    CHECK(strcmp(full->number, "1") == 0 && !full->pickup, "measure 1 is a full measure");
    CHECK(IS_RATIONAL(full->start, 1, 4) && IS_RATIONAL(full->length, 1, 1), "measure 1 starts after the pickup");
    CHECK(full->beats == 4 && full->beat_type == 4 && !full->time_changed, "4/4 carries on without a change");

    CHECK(strcmp(changed->number, "2") == 0, "measure 2");
    CHECK(IS_RATIONAL(changed->start, 5, 4) && IS_RATIONAL(changed->length, 3, 4), "3/4 is three quarters long");
    CHECK(changed->beats == 3 && changed->beat_type == 4 && changed->time_changed, "3/4 is a change");
}

static void check_pitch(const ScoreTextEvent *event, size_t index, char step, int alter, int octave) {
    CHECK(event->pitch_count > index, "pitch %zu exists", index);
    if (event->pitch_count > index) {
        CHECK(event->pitches[index].step == step && event->pitches[index].alter == alter &&
                  event->pitches[index].octave == octave,
              "pitch %zu is %c alter %d octave %d, got %c %d %d", index, step, alter, octave,
              event->pitches[index].step, event->pitches[index].alter, event->pitches[index].octave);
    }
}

static void check_event(const ScoreTextEvent *event, int part, int staff, int voice, char type, int dots) {
    CHECK(event->part == part && event->staff == staff && event->voice == voice, "P%d S%d V%d, got P%d S%d V%d", part,
          staff, voice, event->part, event->staff, event->voice);
    CHECK(event->type == type && event->dots == dots, "value %c with %d dots, got %c with %d", type, dots, event->type,
          event->dots);
}

static void check_pickup_events(const ScoreTextScore *score) {
    const ScoreTextMeasure *pickup = &score->measures[0];
    CHECK(pickup->event_count == 1, "the pickup holds one note, got %zu", pickup->event_count);
    if (pickup->event_count != 1) {
        return;
    }
    const ScoreTextEvent *g = &pickup->events[0];
    check_event(g, 1, 1, 1, 'q', 0);
    check_pitch(g, 0, 'G', 0, 4);
    CHECK(IS_RATIONAL(g->onset, 0, 1), "the pickup note sounds at 0");
    CHECK(IS_RATIONAL(g->beat_in_measure, 4, 1), "the pickup note is on beat 4 of its bar, got %lld/%lld",
          (long long)g->beat_in_measure.num, (long long)g->beat_in_measure.den);
    CHECK(strcmp(g->xml_id, "pickup-g") == 0, "xml:id is read, got '%s'", g->xml_id);
    CHECK(!g->rest && !g->tie_start && g->tuplet_actual == 0, "a plain note");
}

static void check_full_measure(const ScoreTextScore *score) {
    const ScoreTextMeasure *measure = &score->measures[1];
    CHECK(measure->event_count == 11, "measure 1 holds eleven events, got %zu", measure->event_count);
    if (measure->event_count != 11) {
        return;
    }
    const ScoreTextEvent *e = measure->events;

    /* Part 1, staff 1, voice 1: a half note, a triplet of eighths, a quarter rest. */
    check_event(&e[0], 1, 1, 1, 'h', 0);
    check_pitch(&e[0], 0, 'C', 0, 5);
    CHECK(IS_RATIONAL(e[0].onset, 1, 4) && IS_RATIONAL(e[0].beat_in_measure, 1, 1), "the half note starts the measure");

    const struct {
        char step;
        long long onset_num, onset_den, beat_num, beat_den;
    } TRIPLET[] = {{'D', 3, 4, 3, 1}, {'E', 5, 6, 10, 3}, {'F', 11, 12, 11, 3}};
    for (size_t i = 0; i < 3; i++) {
        const ScoreTextEvent *note = &e[1 + i];
        check_event(note, 1, 1, 1, 'e', 0);
        check_pitch(note, 0, TRIPLET[i].step, 0, 5);
        CHECK(note->tuplet_actual == 3 && note->tuplet_normal == 2, "triplet note %zu is 3:2", i);
        CHECK(IS_RATIONAL(note->onset, TRIPLET[i].onset_num, TRIPLET[i].onset_den), "triplet note %zu onset %lld/%lld",
              i, (long long)note->onset.num, (long long)note->onset.den);
        CHECK(IS_RATIONAL(note->beat_in_measure, TRIPLET[i].beat_num, TRIPLET[i].beat_den),
              "triplet note %zu beat %lld/%lld", i, (long long)note->beat_in_measure.num,
              (long long)note->beat_in_measure.den);
    }
    check_event(&e[4], 1, 1, 1, 'q', 0);
    CHECK(e[4].rest && e[4].pitch_count == 0, "a rest has no pitch");
    CHECK(IS_RATIONAL(e[4].beat_in_measure, 4, 1), "the rest is on beat 4");

    /* Voice 2 after a backup: one chord of three pitches. */
    check_event(&e[5], 1, 1, 2, 'w', 0);
    CHECK(e[5].pitch_count == 3 && !e[5].rest, "the chord has three pitches, got %zu", e[5].pitch_count);
    check_pitch(&e[5], 0, 'C', 0, 4);
    check_pitch(&e[5], 1, 'E', 0, 4);
    check_pitch(&e[5], 2, 'G', 0, 4);
    CHECK(IS_RATIONAL(e[5].onset, 1, 4) && IS_RATIONAL(e[5].beat_in_measure, 1, 1), "the chord starts the measure");

    /* Staff 2: a dotted quarter with a sharp, a tied flat eighth, a half rest. */
    check_event(&e[6], 1, 2, 3, 'q', 1);
    check_pitch(&e[6], 0, 'F', 1, 2);
    CHECK(!e[6].tie_start, "the dotted quarter is not tied");
    check_event(&e[7], 1, 2, 3, 'e', 0);
    check_pitch(&e[7], 0, 'B', -1, 2);
    CHECK(e[7].tie_start, "the eighth ties to the next note");
    CHECK(IS_RATIONAL(e[7].onset, 5, 8) && IS_RATIONAL(e[7].beat_in_measure, 5, 2), "the eighth is on beat 2.5");
    check_event(&e[8], 1, 2, 3, 'h', 0);
    CHECK(e[8].rest && IS_RATIONAL(e[8].beat_in_measure, 3, 1), "the half rest is on beat 3");

    /* Part 2: a note with no <type> takes its value from the duration; no staff or voice means 1 and 1. */
    check_event(&e[9], 2, 1, 1, 'h', 0);
    check_pitch(&e[9], 0, 'B', 0, 3);
    check_event(&e[10], 2, 1, 1, 'h', 0);
    check_pitch(&e[10], 0, 'C', 0, 4);
    CHECK(IS_RATIONAL(e[10].beat_in_measure, 3, 1), "the second flute note is on beat 3");
}

static void check_changed_measure(const ScoreTextScore *score) {
    const ScoreTextMeasure *measure = &score->measures[2];
    CHECK(measure->event_count == 4, "measure 2 holds four events, got %zu", measure->event_count);
    if (measure->event_count != 4) {
        return;
    }
    const ScoreTextEvent *e = measure->events;
    check_event(&e[0], 1, 1, 1, 'h', 1);
    check_pitch(&e[0], 0, 'A', 0, 4);
    CHECK(IS_RATIONAL(e[0].onset, 5, 4) && IS_RATIONAL(e[0].beat_in_measure, 1, 1), "the dotted half starts measure 2");
    CHECK(strcmp(e[0].xml_id, "last-a") == 0, "xml:id of the last note, got '%s'", e[0].xml_id);

    check_event(&e[1], 1, 2, 3, 'h', 0);
    check_pitch(&e[1], 0, 'B', -1, 2);
    CHECK(!e[1].tie_start, "the end of a tie does not tie on");
    CHECK(e[1].xml_id[0] == '\0', "a note with no xml:id has an empty one");
    check_event(&e[2], 1, 2, 3, 'q', 0);
    CHECK(e[2].rest && IS_RATIONAL(e[2].beat_in_measure, 3, 1), "the quarter rest is on beat 3");

    /* A forward of one quarter moves the next note to beat 2. */
    check_event(&e[3], 2, 1, 1, 'h', 0);
    CHECK(IS_RATIONAL(e[3].beat_in_measure, 2, 1), "the note after a forward is on beat 2, got %lld/%lld",
          (long long)e[3].beat_in_measure.num, (long long)e[3].beat_in_measure.den);
}

static void check_defaults(void) {
    char *error = NULL;
    ScoreTextScore *bare = score_text_read(BARE, &error);
    CHECK(bare != NULL && error == NULL, "the bare score reads");
    if (bare) {
        CHECK(bare->part_count == 1 && bare->measure_count == 1, "one part, one measure");
        const ScoreTextMeasure *measure = &bare->measures[0];
        CHECK(strcmp(measure->number, "1") == 0 && !measure->pickup, "measure 1");
        CHECK(measure->beats == 4 && measure->beat_type == 4 && measure->time_changed, "4/4 is the default meter");
        CHECK(IS_RATIONAL(measure->length, 1, 1), "a default measure is one whole note");
        /* No <divisions>: one division per quarter, so a duration of 4 is a whole note, and no <type> gives 'w'. */
        CHECK(measure->event_count == 1 && measure->events[0].type == 'w' && measure->events[0].dots == 0,
              "a whole note with no <type> is read as a whole note");
        score_text_destroy(bare);
    }

    ScoreTextScore *untyped = score_text_read(UNTYPED, &error);
    CHECK(untyped != NULL, "the untyped score reads");
    if (untyped) {
        CHECK(strcmp(untyped->measures[0].number, "7") == 0, "measure number 7 is kept as written");
        CHECK(untyped->measures[0].event_count == 3, "three notes, got %zu", untyped->measures[0].event_count);
        if (untyped->measures[0].event_count == 3) {
            const ScoreTextEvent *e = untyped->measures[0].events;
            check_event(&e[0], 1, 1, 1, 'q', 1);
            check_event(&e[1], 1, 1, 1, 'e', 0);
            CHECK(e[1].tuplet_actual == 3 && e[1].tuplet_normal == 2, "the untyped triplet note is 3:2");
            check_event(&e[2], 1, 1, 1, 'h', 0);
        }
        score_text_destroy(untyped);
    }
}

static void check_refusals(void) {
    char *error = NULL;
    CHECK(score_text_read(NULL, &error) == NULL && error != NULL, "NULL text is refused with a message");
    free(error);
    error = NULL;
    CHECK(score_text_read("<score-partwise><part>", &error) == NULL && error != NULL, "an unclosed element is refused");
    free(error);
    error = NULL;
    CHECK(score_text_read("<score-partwise><part></measure></score-partwise>", &error) == NULL && error != NULL,
          "a mismatched closing tag is refused");
    free(error);
    error = NULL;
    CHECK(score_text_read("<score-timewise></score-timewise>", &error) == NULL && error != NULL,
          "a score that is not partwise is refused");
    free(error);
    error = NULL;
    CHECK(score_text_read("<score-partwise><part-list/></score-partwise>", &error) == NULL && error != NULL,
          "a score with no measures is refused");
    free(error);
    error = NULL;
    CHECK(score_text_read("<score-partwise></score-partwise>", NULL) == NULL, "the message is optional");
    score_text_destroy(NULL);
}

int main(void) {
    check_rationals();

    char *error = NULL;
    ScoreTextScore *score = score_text_read(SCORE, &error);
    CHECK(score != NULL, "the score reads: %s", error ? error : "no message");
    free(error);
    if (score) {
        check_measures(score);
        if (score->measure_count == 3) {
            check_pickup_events(score);
            check_full_measure(score);
            check_changed_measure(score);
        }
        score_text_destroy(score);
    }
    check_defaults();
    check_refusals();
    return failures == 0 ? 0 : 1;
}
