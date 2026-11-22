#include "io/tests/export/score_roundtrip_util.h"

/*
 * Notated tuplets (<time-modification>) survive the importer and the export at any ratio: the importer keeps the
 * ratio on the NoteEvent, the export classifies a note by it, and a round trip gives the same ratios back. The files:
 * the analyzer's triplet.musicxml (three 3:2 quarters and a half), and tests/fixtures/musicxml/quintuplet.musicxml
 * (one 4/4 measure: five 5:4 sixteenths, six 6:4 sixteenths, a half note with no ratio).
 */

typedef struct {
    bool has;
    unsigned actual;
    unsigned normal;
} Hint;

static void import_file(const char *directory, const char *name, MusicXmlImport *import) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(musicxml_import_file(path, import) == MUSICXML_IMPORT_OK);
}

/* The index of the event that starts at n/d of a whole note with the given pitch letter. */
static size_t event_at(const MusicXmlImport *import, long n, unsigned long d) {
    for (size_t i = 0; i < import->count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(import->events[i]));
        if (position->value.num * (long long)d == n * (long long)position->value.den) {
            return i;
        }
    }
    assert(!"no event at that position");
    return 0;
}

static Hint hint_of(const NoteEvent *event) {
    Hint hint;
    hint.actual = 0;
    hint.normal = 0;
    hint.has = note_event_tuplet(event, &hint.actual, &hint.normal);
    return hint;
}

static void assert_hint(const MusicXmlImport *import, long n, unsigned long d, bool has, unsigned actual, unsigned normal) {
    Hint hint = hint_of(import->events[event_at(import, n, d)]);
    assert(hint.has == has && hint.actual == actual && hint.normal == normal);
}

static void assert_duration(const MusicXmlImport *import, long n, unsigned long d, long dur_n, unsigned long dur_d) {
    const Duration *duration = note_event_duration(import->events[event_at(import, n, d)]);
    assert(duration->value.num * (long long)dur_d == dur_n * (long long)duration->value.den);
}

static void test_import_triplet(void) {
    MusicXmlImport import;
    import_file(URSATZ_SCORES_DIR, "triplet.musicxml", &import);
    assert(import.count == 4);
    /* three quarter-note triplets (each 1/6 of a whole note) and a half note with no ratio */
    assert_hint(&import, 0, 1, true, 3, 2);
    assert_hint(&import, 1, 6, true, 3, 2);
    assert_hint(&import, 1, 3, true, 3, 2);
    assert_hint(&import, 1, 2, false, 0, 0);
    assert_duration(&import, 0, 1, 1, 6);
    assert_duration(&import, 1, 2, 1, 2);
    musicxml_import_destroy(&import);
}

static void test_import_quintuplet(void) {
    MusicXmlImport import;
    import_file(URSATZ_MUSICXML_DIR, "quintuplet.musicxml", &import);
    assert(import.count == 12);
    for (long i = 0; i < 5; i++) {
        assert_hint(&import, i, 20, true, 5, 4);
        assert_duration(&import, i, 20, 1, 20);
    }
    for (long i = 0; i < 6; i++) {
        /* the six start at the second beat: 1/4 + i/24 */
        assert_hint(&import, 6 + i, 24, true, 6, 4);
        assert_duration(&import, 6 + i, 24, 1, 24);
    }
    assert_hint(&import, 1, 2, false, 0, 0);
    musicxml_import_destroy(&import);
}

/* A <time-modification> that is not a tuplet ratio is warned about and left off; the duration stays exact. */
static void test_import_bad_ratio(void) {
    static const char document[] =
        "<?xml version=\"1.0\"?><score-partwise version=\"3.1\"><part-list><score-part id=\"P1\"><part-name>P</part-name>"
        "</score-part></part-list><part id=\"P1\"><measure number=\"1\"><attributes><divisions>2</divisions>"
        "<time><beats>4</beats><beat-type>4</beat-type></time></attributes>"
        "<note><pitch><step>C</step><octave>4</octave></pitch><duration>2</duration><voice>1</voice>"
        "<time-modification><actual-notes>3</actual-notes><normal-notes>3</normal-notes></time-modification></note>"
        "<note><pitch><step>D</step><octave>4</octave></pitch><duration>2</duration><voice>1</voice>"
        "<time-modification><actual-notes>x</actual-notes><normal-notes>2</normal-notes></time-modification></note>"
        "<note><pitch><step>E</step><octave>4</octave></pitch><duration>2</duration><voice>1</voice>"
        "<time-modification><actual-notes>3</actual-notes><normal-notes>2</normal-notes></time-modification></note>"
        "</measure></part></score-partwise>";
    MusicXmlImport import;
    assert(musicxml_import_buffer(document, strlen(document), &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 3 && import.warnings.count >= 2);
    assert_hint(&import, 0, 1, false, 0, 0);
    assert_hint(&import, 1, 4, false, 0, 0);
    assert_hint(&import, 1, 2, true, 3, 2);
    assert_duration(&import, 0, 1, 1, 4);
    musicxml_import_destroy(&import);
}

/* Import, export, import again: the same notes (rt_round_trip) and the same ratios. Returns the document. */
static char *round_trip_with_hints(const MusicXmlImport *first) {
    char *xml = rt_round_trip(first);
    MusicXmlImport second;
    assert(musicxml_import_buffer(xml, strlen(xml), &second) == MUSICXML_IMPORT_OK);
    assert(second.count == first->count);
    for (size_t i = 0; i < first->count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(first->events[i]));
        size_t other = event_at(&second, (long)position->value.num, (unsigned long)position->value.den);
        Hint a = hint_of(first->events[i]);
        Hint b = hint_of(second.events[other]);
        assert(a.has == b.has && a.actual == b.actual && a.normal == b.normal);
    }
    musicxml_import_destroy(&second);
    return xml;
}

static void test_round_trip_triplet(void) {
    MusicXmlImport import;
    import_file(URSATZ_SCORES_DIR, "triplet.musicxml", &import);
    char *xml = round_trip_with_hints(&import);
    assert(tu_count(xml, "<actual-notes>3</actual-notes>") == 3 && tu_count(xml, "<normal-notes>2</normal-notes>") == 3);
    assert(tu_count(xml, "<tuplet type=\"start\"/>") == 1 && tu_count(xml, "<tuplet type=\"stop\"/>") == 1);
    free(xml);
    musicxml_import_destroy(&import);
}

static void test_round_trip_quintuplet(void) {
    MusicXmlImport import;
    import_file(URSATZ_MUSICXML_DIR, "quintuplet.musicxml", &import);
    char *xml = round_trip_with_hints(&import);
    assert(tu_count(xml, "<time-modification>") == 11);
    assert(tu_count(xml, "<actual-notes>5</actual-notes>") == 5 && tu_count(xml, "<normal-notes>4</normal-notes>") == 11);
    assert(tu_count(xml, "<actual-notes>6</actual-notes>") == 6);
    assert(tu_count(xml, "<type>16th</type>") == 11 && tu_count(xml, "<type>half</type>") == 1);
    assert(tu_count(xml, "<tuplet type=\"start\"/>") == 2 && tu_count(xml, "<tuplet type=\"stop\"/>") == 2);
    /* the groups are one beam group each: a quintuplet and a sextuplet of sixteenths */
    assert(tu_count(xml, "<beam number=\"2\">begin</beam>") == 2 && tu_count(xml, "<beam number=\"2\">end</beam>") == 2);
    assert(!TU_HAS(xml, "<tied"));
    free(xml);
    musicxml_import_destroy(&import);
}

int main(void) {
    test_import_triplet();
    test_import_quintuplet();
    test_import_bad_ratio();
    test_round_trip_triplet();
    test_round_trip_quintuplet();
    printf("test_musicxml_tuplets: all tests passed\n");
    return 0;
}
