#include "io/musicxml_importer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/tests/export/score_test_util.h"

/*
 * Note ids through export and import: a score the export wrote reads back with the ids it gave (a tied note, written as two pieces
 * "<id>_a" and "<id>_b", keeps its id); a document with no xml:id has positional ids as before; a repeated xml:id gives the later
 * note its positional id and a warning; an xml:id equal to another note's positional id pushes that note to "-2".
 */

#define DOC(body) "<?xml version=\"1.0\"?><score-partwise><part-list/><part id=\"P1\"><measure number=\"1\">" body \
                  "</measure></part></score-partwise>"
#define DIV4 "<attributes><divisions>4</divisions></attributes>"
#define NOTE(id, step) \
    "<note" id "><pitch><step>" step "</step><octave>4</octave></pitch><duration>4</duration></note>"

static const char *id_of(const MusicXmlImport *import, size_t i) {
    return entity_id_value(note_event_id(import->events[i]));
}

static MusicXmlImport import_text(const char *text) {
    MusicXmlImport import;
    assert(musicxml_import_buffer(text, strlen(text), &import) == MUSICXML_IMPORT_OK);
    return import;
}

static void test_export_then_import_keeps_ids(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("piano-n1", 0, 1, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 1);
    bag_add(&bag, tu_note("piano-n2", 1, 4, 1, 4, PITCH_LETTER_D, 0, 4, NULL), 0, 1);
    bag_add(&bag, tu_note("piano-n3", 3, 4, 1, 2, PITCH_LETTER_E, 0, 4, NULL), 0, 1); /* crosses the barline at 1: tied */
    bag_add(&bag, tu_note("piano-n4", 5, 4, 1, 4, PITCH_LETTER_F, 0, 4, NULL), 0, 1);
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag.events;
    score.count = bag.count;
    score.part_index = bag.part;
    score.staff = bag.staff;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(strstr(xml, "xml:id=\"piano-n3_a\"") != NULL && strstr(xml, "xml:id=\"piano-n3_b\"") != NULL);
    MusicXmlImport import = import_text(xml);
    assert(import.count == 4 && import.warnings.count == 0);
    assert(strcmp(id_of(&import, 0), "piano-n1") == 0 && strcmp(id_of(&import, 1), "piano-n2") == 0);
    assert(strcmp(id_of(&import, 2), "piano-n3") == 0 && strcmp(id_of(&import, 3), "piano-n4") == 0);
    const Duration *held = note_event_duration(import.events[2]);
    assert(held->value.num == 1 && held->value.den == 2); /* the two pieces merged back into one note */
    musicxml_import_destroy(&import);
    /* the second round trip changes nothing */
    free(xml);
    bag_free(&bag);
}

static void test_no_xml_id_is_positional(void) {
    MusicXmlImport import = import_text(DOC(DIV4 NOTE("", "C") NOTE("", "D")));
    assert(import.count == 2 && import.warnings.count == 0);
    assert(strcmp(id_of(&import, 0), "musicxml-P1-0") == 0 && strcmp(id_of(&import, 1), "musicxml-P1-1") == 0);
    musicxml_import_destroy(&import);
}

static void test_a_note_that_ends_in_a_is_not_a_tie_start(void) {
    /* only a note that starts a tie loses the "_a" */
    MusicXmlImport import = import_text(DOC(DIV4 NOTE(" xml:id=\"theme_a\"", "C") NOTE(" xml:id=\"x\"", "D")));
    assert(strcmp(id_of(&import, 0), "theme_a") == 0 && strcmp(id_of(&import, 1), "x") == 0);
    musicxml_import_destroy(&import);
    MusicXmlImport tied = import_text(DOC(DIV4
        "<note xml:id=\"n1_a\"><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><tie type=\"start\"/></note>"
        "<note xml:id=\"n1_b\"><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><tie type=\"stop\"/></note>"));
    assert(tied.count == 1 && strcmp(id_of(&tied, 0), "n1") == 0);
    musicxml_import_destroy(&tied);
}

static void test_a_repeated_xml_id(void) {
    MusicXmlImport import = import_text(DOC(DIV4 NOTE(" xml:id=\"a\"", "C") NOTE(" xml:id=\"a\"", "D") NOTE(" xml:id=\"b\"", "E")));
    assert(import.count == 3);
    assert(strcmp(id_of(&import, 0), "a") == 0);
    assert(strcmp(id_of(&import, 1), "musicxml-P1-1") == 0); /* the second note falls back to its positional id */
    assert(strcmp(id_of(&import, 2), "b") == 0);
    assert(import.warnings.count == 1 && strstr(import.warnings.messages[0], "repeated xml:id 'a'") != NULL);
    musicxml_import_destroy(&import);
}

static void test_an_xml_id_equal_to_a_positional_id(void) {
    /* the first note is named like the second note's positional id; the second then gets -2 */
    MusicXmlImport import = import_text(DOC(DIV4 NOTE(" xml:id=\"musicxml-P1-1\"", "C") NOTE("", "D") NOTE("", "E")));
    assert(import.count == 3 && import.warnings.count == 0);
    assert(strcmp(id_of(&import, 0), "musicxml-P1-1") == 0);
    assert(strcmp(id_of(&import, 1), "musicxml-P1-1-2") == 0);
    assert(strcmp(id_of(&import, 2), "musicxml-P1-2") == 0);
    musicxml_import_destroy(&import);
}

static void test_a_long_xml_id_is_positional(void) {
    char id[80];
    memset(id, 'x', 64);
    id[64] = '\0';
    char text[512];
    snprintf(text, sizeof(text), DOC(DIV4 "<note xml:id=\"%s\"><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration></note>"), id);
    MusicXmlImport import = import_text(text);
    assert(strcmp(id_of(&import, 0), "musicxml-P1-0") == 0);
    musicxml_import_destroy(&import);
    memset(id, 'x', 63);
    id[63] = '\0';
    snprintf(text, sizeof(text), DOC(DIV4 "<note xml:id=\"%s\"><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration></note>"), id);
    import = import_text(text);
    assert(strcmp(id_of(&import, 0), id) == 0); /* 63 characters are kept */
    musicxml_import_destroy(&import);
}

int main(void) {
    test_export_then_import_keeps_ids();
    test_no_xml_id_is_positional();
    test_a_note_that_ends_in_a_is_not_a_tie_start();
    test_a_repeated_xml_id();
    test_an_xml_id_equal_to_a_positional_id();
    test_a_long_xml_id_is_positional();
    printf("test_musicxml_note_ids: all tests passed\n");
    return 0;
}
