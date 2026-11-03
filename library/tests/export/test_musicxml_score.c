#include "score_test_util.h"

/* musicxml_export_write_score: measures from a MeterMap, ties, rests, divisions, staves, parts and markings. */

static char *write_bag(const TestBag *bag, const MeterMap *meter, const MusicXmlExportPart *parts, size_t part_count,
                       bool with_part_arrays) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    if (with_part_arrays) {
        score.part_index = bag->part;
        score.staff = bag->staff;
    }
    score.parts = parts;
    score.part_count = part_count;
    score.meter_map = meter;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(xml != NULL);
    return xml;
}

static void test_minimal_document_exact(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "n0", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    static const char expected[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" "
        "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
        "<score-partwise version=\"3.1\">\n"
        "  <identification>\n"
        "    <miscellaneous>\n"
        "      <miscellaneous-field name=\"ursatz:meter-defaulted\">true</miscellaneous-field>\n"
        "    </miscellaneous>\n"
        "  </identification>\n"
        "  <part-list>\n"
        "    <score-part id=\"P1\">\n"
        "      <part-name print-object=\"no\"></part-name>\n"
        "    </score-part>\n"
        "  </part-list>\n"
        "  <part id=\"P1\">\n"
        "    <measure number=\"1\">\n"
        "      <attributes>\n"
        "        <divisions>1</divisions>\n"
        "        <time>\n"
        "          <beats>4</beats>\n"
        "          <beat-type>4</beat-type>\n"
        "        </time>\n"
        "      </attributes>\n"
        "      <note xml:id=\"n0\">\n"
        "        <pitch>\n"
        "          <step>C</step>\n"
        "          <octave>4</octave>\n"
        "        </pitch>\n"
        "        <duration>1</duration>\n"
        "        <voice>1</voice>\n"
        "        <type>quarter</type>\n"
        "      </note>\n"
        "      <note>\n"
        "        <rest/>\n"
        "        <duration>1</duration>\n"
        "        <voice>1</voice>\n"
        "        <type>quarter</type>\n"
        "      </note>\n"
        "      <note>\n"
        "        <rest/>\n"
        "        <duration>2</duration>\n"
        "        <voice>1</voice>\n"
        "        <type>half</type>\n"
        "      </note>\n"
        "    </measure>\n"
        "  </part>\n"
        "</score-partwise>\n";
    assert(strcmp(xml, expected) == 0);
    free(xml);
    bag_free(&bag);
}

static void test_three_four_measures(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    char id[16];
    for (int k = 0; k < 6; k++) {
        snprintf(id, sizeof(id), "n%d", k);
        bag_add_note(&bag, id, k, 4, 1, 4, PITCH_LETTER_C, NULL);
    }
    const TuMeter meters[] = {{3, 4, 0, 1}};
    MeterMap *meter = tu_meter_map(meters, 1);
    char *xml = write_bag(&bag, meter, NULL, 0, false);
    assert(TU_HAS(xml, "<measure number=\"1\">") && TU_HAS(xml, "<measure number=\"2\">"));
    assert(!TU_HAS(xml, "<measure number=\"3\">") && !TU_HAS(xml, "<measure number=\"0\""));
    assert(tu_count(xml, "<beats>3</beats>") == 1 && tu_count(xml, "<time>") == 1);
    assert(!TU_HAS(xml, "<rest/>") && !TU_HAS(xml, "meter-defaulted"));
    free(xml);
    meter_map_destroy(meter);
    bag_free(&bag);
}

static void test_pickup_is_measure_zero(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    char id[16];
    for (int k = 0; k < 5; k++) {
        snprintf(id, sizeof(id), "n%d", k);
        bag_add_note(&bag, id, k, 4, 1, 4, PITCH_LETTER_G, NULL);
    }
    const TuMeter meters[] = {{1, 4, 0, 1}, {4, 4, 1, 4}};
    MeterMap *meter = tu_meter_map(meters, 2);
    assert(meter_map_has_pickup(meter));
    char *xml = write_bag(&bag, meter, NULL, 0, false);
    assert(TU_HAS(xml, "<measure number=\"0\" implicit=\"yes\">"));
    assert(TU_HAS(xml, "<measure number=\"1\">") && !TU_HAS(xml, "<measure number=\"2\">"));
    assert(tu_count(xml, "<time>") == 2 && tu_count(xml, "implicit=") == 1);
    free(xml);
    meter_map_destroy(meter);
    bag_free(&bag);
}

static void test_short_measure_at_meter_change(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    char id[16];
    for (int k = 0; k < 9; k++) {
        snprintf(id, sizeof(id), "n%d", k);
        bag_add_note(&bag, id, k, 4, 1, 4, PITCH_LETTER_E, NULL);
    }
    /* 4/4, then 3/4 from the first barline, then 4/4 from one quarter into the second 3/4 measure: measure 3 is a
       short measure of one quarter, measure 4 starts at the change */
    const TuMeter meters[] = {{4, 4, 0, 1}, {3, 4, 1, 1}, {4, 4, 2, 1}};
    MeterMap *meter = tu_meter_map(meters, 3);
    assert(!meter_map_has_pickup(meter));
    char *xml = write_bag(&bag, meter, NULL, 0, false);
    assert(!TU_HAS(xml, "<measure number=\"0\"") && !TU_HAS(xml, "<measure number=\"5\">"));
    for (int m = 1; m <= 4; m++) {
        char needle[32];
        snprintf(needle, sizeof(needle), "<measure number=\"%d\">", m);
        assert(TU_HAS(xml, needle));
    }
    assert(tu_count(xml, "<time>") == 3 && tu_count(xml, "<beats>4</beats>") == 2 && tu_count(xml, "<beats>3</beats>") == 1);
    free(xml);
    meter_map_destroy(meter);
    bag_free(&bag);
}

static void test_tie_across_barline(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "n0", 3, 4, 1, 2, PITCH_LETTER_G, NULL);
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    assert(TU_HAS(xml, "<note xml:id=\"n0_a\">") && TU_HAS(xml, "<note xml:id=\"n0_b\">"));
    assert(tu_count(xml, "<tie type=\"start\"/>") == 1 && tu_count(xml, "<tie type=\"stop\"/>") == 1);
    assert(TU_HAS(xml, "<measure number=\"2\">"));
    /* the leading rest is one dotted half; the closing rest is a quarter and a half, as it starts on beat 2 */
    assert(tu_count(xml, "<rest/>") == 3 && tu_count(xml, "<duration>3</duration>") == 1);
    free(xml);
    bag_free(&bag);
}

static void test_triplets_set_the_divisions(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "t0", 0, 1, 1, 12, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "t1", 1, 12, 1, 12, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "t2", 1, 6, 1, 12, PITCH_LETTER_E, NULL);
    bag_add_note(&bag, "q", 1, 4, 1, 4, PITCH_LETTER_F, NULL);
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    assert(TU_HAS(xml, "<divisions>3</divisions>"));
    assert(tu_count(xml, "<duration>1</duration>") == 3 && tu_count(xml, "<duration>3</duration>") == 1);
    assert(tu_count(xml, "<duration>6</duration>") == 1); /* the rest to the barline */
    free(xml);
    bag_free(&bag);
}

static void test_gap_is_a_rest(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "b", 1, 2, 1, 4, PITCH_LETTER_D, NULL);
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    assert(tu_count(xml, "<rest/>") == 2); /* the gap, and the rest to the barline */
    free(xml);
    bag_free(&bag);
}

static void test_two_voices_back_up(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 2, PITCH_LETTER_C, "voice-a");
    bag_add_note(&bag, "b", 0, 1, 1, 4, PITCH_LETTER_E, "voice-b");
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    assert(TU_HAS(xml, "<voice>1</voice>") && TU_HAS(xml, "<voice>2</voice>"));
    assert(tu_count(xml, "<backup>\n        <duration>4</duration>") == 1);
    free(xml);
    bag_free(&bag);
}

static void test_two_staves(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("r", 0, 1, 1, 4, PITCH_LETTER_E, 0, 5, "v1"), 0, 1);
    bag_add(&bag, tu_note("l", 0, 1, 1, 4, PITCH_LETTER_C, 0, 3, "v2"), 0, 2);
    char *xml = write_bag(&bag, NULL, NULL, 0, true);
    assert(TU_HAS(xml, "<staves>2</staves>"));
    assert(tu_count(xml, "<staff>1</staff>") >= 1 && tu_count(xml, "<staff>2</staff>") >= 1);
    assert(tu_count(xml, "<part id=") == 1 && TU_HAS(xml, "<backup>"));
    free(xml);
    bag_free(&bag);
}

static void test_two_parts(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("f", 0, 1, 1, 4, PITCH_LETTER_C, 0, 5, NULL), 0, 1);
    bag_add(&bag, tu_note("c", 0, 1, 1, 4, PITCH_LETTER_C, 0, 3, NULL), 1, 1);
    const MusicXmlExportPart parts[] = {{"P1", "Flute", 1}, {"P2", "Cello & Bass", 1}};
    char *xml = write_bag(&bag, NULL, parts, 2, true);
    assert(tu_count(xml, "<part id=") == 2 && TU_HAS(xml, "<score-part id=\"P2\">"));
    assert(TU_HAS(xml, "<part-name>Cello &amp; Bass</part-name>"));
    assert(tu_count(xml, "<measure number=\"1\">") == 2);
    free(xml);
    bag_free(&bag);
}

static char *write_titled(const TestBag *bag, const char *title, const char *composer, const MusicXmlExportPart *parts,
                          size_t part_count, const MeterMap *meter) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    score.parts = parts;
    score.part_count = part_count;
    score.meter_map = meter;
    score.title = title;
    score.composer = composer;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    return xml;
}

static void test_title_and_composer(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "n0", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    char *xml = write_titled(&bag, "Fugue <1> & Co", "J. S. Bach", NULL, 0, NULL);
    assert(TU_HAS(xml, "<work>\n    <work-title>Fugue &lt;1&gt; &amp; Co</work-title>\n  </work>"));
    assert(TU_HAS(xml, "<creator type=\"composer\">J. S. Bach</creator>"));
    assert(tu_count(xml, "<identification>") == 1 && TU_HAS(xml, "ursatz:meter-defaulted"));
    assert(strstr(xml, "<work>") < strstr(xml, "<identification>"));
    assert(strstr(xml, "<creator") < strstr(xml, "<miscellaneous>"));
    free(xml);

    const TuMeter declared[] = {{4, 4, 0, 1}};
    MeterMap *meter = tu_meter_map(declared, 1);
    xml = write_titled(&bag, "", NULL, NULL, 0, meter);
    assert(!TU_HAS(xml, "<work>") && !TU_HAS(xml, "<identification>"));
    free(xml);
    xml = write_titled(&bag, NULL, "Anon", NULL, 0, meter);
    assert(!TU_HAS(xml, "<work>") && TU_HAS(xml, "<creator type=\"composer\">Anon</creator>"));
    assert(!TU_HAS(xml, "<miscellaneous>"));
    free(xml);
    meter_map_destroy(meter);
    bag_free(&bag);
}

static void test_instruments_and_unnamed_part(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("f", 0, 1, 1, 4, PITCH_LETTER_C, 0, 5, NULL), 0, 1);
    bag_add(&bag, tu_note("c", 0, 1, 1, 4, PITCH_LETTER_C, 0, 3, NULL), 1, 1);
    bag_add(&bag, tu_note("x", 0, 1, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 2, 1);
    const MusicXmlExportPart parts[] = {{"P1", "Flute", 1, 73}, {"P2", "", 1, -1}, {"P3", "Organ", 1, 0}};
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag.events;
    score.count = bag.count;
    score.part_index = bag.part;
    score.staff = bag.staff;
    score.parts = parts;
    score.part_count = 3;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(TU_HAS(xml, "<score-instrument id=\"P1-I1\">\n        <instrument-name>Flute</instrument-name>"));
    assert(TU_HAS(xml, "<midi-instrument id=\"P1-I1\">\n        <midi-program>74</midi-program>"));
    assert(TU_HAS(xml, "<midi-instrument id=\"P3-I1\">\n        <midi-program>1</midi-program>"));
    assert(!TU_HAS(xml, "P2-I1"));
    assert(tu_count(xml, "<score-instrument") == 2);
    assert(TU_HAS(xml, "<part-name print-object=\"no\"></part-name>") && TU_HAS(xml, "<part-name>Flute</part-name>"));
    free(xml);
    bag_free(&bag);
}

static void test_default_part_is_unnamed_without_instrument(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "n0", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    assert(TU_HAS(xml, "<part-name print-object=\"no\"></part-name>") && !TU_HAS(xml, "Ursatz Export"));
    assert(!TU_HAS(xml, "<score-instrument") && !TU_HAS(xml, "<midi-instrument"));
    free(xml);
    bag_free(&bag);
}

static void test_flag_follows_the_map(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "n0", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    MeterMap *defaulted = NULL;
    assert(meter_map_create_defaulted(tu_id("tu-defaulted"), &defaulted) == METER_MAP_OK);
    char *xml = write_bag(&bag, defaulted, NULL, 0, false);
    assert(TU_HAS(xml, "ursatz:meter-defaulted") && TU_HAS(xml, "<beats>4</beats>"));
    free(xml);
    meter_map_destroy(defaulted);

    const TuMeter declared[] = {{4, 4, 0, 1}};
    MeterMap *meter = tu_meter_map(declared, 1);
    xml = write_bag(&bag, meter, NULL, 0, false);
    assert(!TU_HAS(xml, "ursatz:meter-defaulted") && !TU_HAS(xml, "<identification>"));
    free(xml);
    meter_map_destroy(meter);
    bag_free(&bag);
}

static void test_zero_notes(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    const MusicXmlExportPart parts[] = {{"P1", "One", 1}, {"P2", "Two", 1}};
    char *xml = write_bag(&bag, NULL, parts, 2, false);
    assert(tu_count(xml, "<part id=") == 2 && tu_count(xml, "<measure number=\"1\">") == 2);
    assert(!TU_HAS(xml, "<note"));
    free(xml);
}

static void test_key_tempo_marking_and_dynamics(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    char id[16];
    for (int k = 0; k < 4; k++) {
        snprintf(id, sizeof(id), "n%d", k);
        bag_add_note(&bag, id, k, 4, 1, 4, PITCH_LETTER_C, NULL);
    }
    const TuMeter meters[] = {{4, 4, 0, 1}};
    MeterMap *meter = tu_meter_map(meters, 1);
    const TuTempo tempos[] = {{0, 1, 500000}, {1, 2, 400000}};
    TempoMap *tempo_map = tu_tempo_map(tempos, 2);

    MusicXmlExportKeySignature keys[2];
    MusicalTime *key_at_start = tu_time(0, 1);
    MusicalTime *key_mid = tu_time(1, 4);
    keys[0].sharps_flats = -1;
    keys[0].mode = MUSICXML_EXPORT_MODE_MAJOR;
    keys[0].position = key_at_start;
    keys[0].part_index = 0;
    keys[1].sharps_flats = 2;
    keys[1].mode = MUSICXML_EXPORT_MODE_MINOR;
    keys[1].position = key_mid;
    keys[1].part_index = 0;

    TempoMarking *markings[1] = {tu_tempo_marking(0, 1, "Allegro", 1, 4, 120, 1)};
    Dynamics *dynamics[2] = {tu_dynamics("mf", 0, 1, 1, 2), tu_dynamics("poco", 1, 2, 1, 1)};

    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag.events;
    score.count = bag.count;
    score.meter_map = meter;
    score.tempo_map = tempo_map;
    score.tempo_markings = markings;
    score.tempo_marking_count = 1;
    score.dynamics = dynamics;
    score.dynamics_count = 2;
    score.key_signatures = keys;
    score.key_signature_count = 2;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);

    /* the key at the start of the measure shares its <attributes> with the time signature, before it */
    assert(TU_HAS(xml, "<key>\n          <fifths>-1</fifths>\n          <mode>major</mode>\n        </key>\n"
                       "        <time>"));
    /* the mid-measure key is its own <attributes> */
    assert(TU_HAS(xml, "<attributes>\n        <key>\n          <fifths>2</fifths>\n          <mode>minor</mode>"));
    /* the first tempo shares the marking's <direction>; the second stands alone */
    assert(tu_count(xml, "<sound tempo=\"120\"/>") == 1 && tu_count(xml, "<sound tempo=\"150\"/>") == 1);
    assert(TU_HAS(xml, "<words>Allegro</words>") && TU_HAS(xml, "<beat-unit>quarter</beat-unit>"));
    assert(TU_HAS(xml, "<per-minute>120</per-minute>"));
    assert(TU_HAS(xml, "<dynamics><mf/></dynamics>"));
    assert(TU_HAS(xml, "<dynamics><other-dynamics>poco</other-dynamics></dynamics>"));
    /* the notes are cut at the markings' positions only where a marking falls inside one: none do here */
    assert(!TU_HAS(xml, "<tie"));
    free(xml);

    musical_time_destroy(key_at_start);
    musical_time_destroy(key_mid);
    tempo_marking_destroy(markings[0]);
    dynamics_destroy(dynamics[0]);
    dynamics_destroy(dynamics[1]);
    tempo_map_destroy(tempo_map);
    meter_map_destroy(meter);
    bag_free(&bag);
}

static void test_marking_inside_a_note_cuts_it(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "n0", 0, 1, 1, 2, PITCH_LETTER_C, NULL); /* a half note */
    Dynamics *dynamics[1] = {tu_dynamics("f", 1, 4, 1, 2)};      /* at the second quarter */
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag.events;
    score.count = bag.count;
    score.dynamics = dynamics;
    score.dynamics_count = 1;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(TU_HAS(xml, "<note xml:id=\"n0_a\">") && TU_HAS(xml, "<note xml:id=\"n0_b\">"));
    assert(tu_count(xml, "<tie type=\"start\"/>") == 1 && TU_HAS(xml, "<dynamics><f/></dynamics>"));
    free(xml);
    dynamics_destroy(dynamics[0]);
    bag_free(&bag);
}

static void test_note_xml_id_format(void) {
    char out[32];
    assert(musicxml_export_note_xml_id("12", out, sizeof(out)) && strcmp(out, "n12") == 0);
    assert(musicxml_export_note_xml_id("a:b", out, sizeof(out)) && strcmp(out, "a_b") == 0);
    assert(musicxml_export_note_xml_id("_x", out, sizeof(out)) && strcmp(out, "_x") == 0);
    assert(musicxml_export_note_xml_id("Ab.c-9", out, sizeof(out)) && strcmp(out, "Ab.c-9") == 0);
    assert(musicxml_export_note_xml_id("", out, sizeof(out)) && strcmp(out, "n") == 0);
    /* exactly full is fine, one short is refused, and nothing is cut silently */
    assert(musicxml_export_note_xml_id("12", out, 4) && strcmp(out, "n12") == 0);
    assert(!musicxml_export_note_xml_id("12", out, 3));
    assert(!musicxml_export_note_xml_id("abc", out, 3));
    assert(!musicxml_export_note_xml_id("abc", out, 0));
    assert(!musicxml_export_note_xml_id(NULL, out, sizeof(out)));
    assert(!musicxml_export_note_xml_id("abc", NULL, sizeof(out)));
}

static void test_every_note_id_is_written(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    /* a plain note, ids that need the prefix and the replacement, and a note tied across the barline */
    bag_add_note(&bag, "n0", 0, 4, 1, 4, PITCH_LETTER_C, NULL);
    bag_add_note(&bag, "12", 1, 4, 1, 4, PITCH_LETTER_D, NULL);
    bag_add_note(&bag, "a:b", 2, 4, 1, 4, PITCH_LETTER_E, NULL);
    bag_add_note(&bag, "tied", 7, 8, 1, 2, PITCH_LETTER_G, NULL);
    char *xml = write_bag(&bag, NULL, NULL, 0, false);
    for (size_t i = 0; i < bag.count; i++) {
        char base[64];
        char exact[96];
        char fragment[96];
        assert(musicxml_export_note_xml_id(entity_id_value(note_event_id(bag.events[i])), base, sizeof(base)));
        snprintf(exact, sizeof(exact), "xml:id=\"%s\"", base);
        snprintf(fragment, sizeof(fragment), "xml:id=\"%s_", base);
        assert(TU_HAS(xml, exact) || TU_HAS(xml, fragment));
    }
    assert(TU_HAS(xml, "xml:id=\"n12\"") && TU_HAS(xml, "xml:id=\"a_b\""));
    assert(TU_HAS(xml, "xml:id=\"tied_a\"") && TU_HAS(xml, "xml:id=\"tied_b\""));
    free(xml);
    bag_free(&bag);
}

int main(void) {
    test_minimal_document_exact();
    test_three_four_measures();
    test_pickup_is_measure_zero();
    test_short_measure_at_meter_change();
    test_tie_across_barline();
    test_triplets_set_the_divisions();
    test_gap_is_a_rest();
    test_two_voices_back_up();
    test_two_staves();
    test_two_parts();
    test_title_and_composer();
    test_instruments_and_unnamed_part();
    test_default_part_is_unnamed_without_instrument();
    test_flag_follows_the_map();
    test_zero_notes();
    test_key_tempo_marking_and_dynamics();
    test_marking_inside_a_note_cuts_it();
    test_note_xml_id_format();
    test_every_note_id_is_written();
    printf("test_musicxml_score: all tests passed\n");
    return 0;
}
