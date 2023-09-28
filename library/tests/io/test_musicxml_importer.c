#include "io/musicxml_importer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * MusicXMLImporter tests. Each mapping rule has a small hand-written fixture under tests/fixtures/musicxml/
 * (URSATZ_MUSICXML_DIR is passed in by CMake); small error cases are inline documents. Times are checked as exact
 * whole-note fractions.
 */

static MusicXmlImport import_fixture(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_MUSICXML_DIR, name);
    MusicXmlImport import;
    assert(musicxml_import_file(path, &import) == MUSICXML_IMPORT_OK);
    return import;
}

static bool equals(long numerator, unsigned long denominator, long expected_n, unsigned long expected_d) {
    return numerator * (long)expected_d == expected_n * (long)denominator;
}

static void assert_note(const MusicXmlImport *import, size_t i, long onset_n, unsigned long onset_d, long length_n,
                        unsigned long length_d, long midi) {
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(import->events[i]));
    const Duration *length = note_event_duration(import->events[i]);
    assert(equals(musical_time_numerator(onset), musical_time_denominator(onset), onset_n, onset_d));
    assert(equals(duration_numerator(length), duration_denominator(length), length_n, length_d));
    assert(pitch_numeric_value_position_numerator(import->pitch_values[i]) == midi);
    assert(pitch_numeric_value_position_denominator(import->pitch_values[i]) == 1);
}

static void test_chords_and_backup(void) {
    MusicXmlImport import = import_fixture("chords_backup.musicxml");
    assert(import.count == 6 && import.warnings.count == 0 && import.error_message[0] == '\0');
    /* time order: the chord and the second voice's C3 start together; then D4; then A3 after the forward */
    assert_note(&import, 0, 0, 1, 1, 4, 60);
    assert_note(&import, 1, 0, 1, 1, 4, 64);
    assert_note(&import, 2, 0, 1, 1, 4, 67);
    assert_note(&import, 3, 0, 1, 1, 2, 48);
    assert_note(&import, 4, 1, 4, 1, 4, 62);
    assert_note(&import, 5, 3, 4, 1, 4, 57);
    for (size_t i = 0; i < import.count; i++) {
        assert(import.part_index[i] == 0);
    }
    assert(strcmp(entity_id_value(note_event_id(import.events[0])), "musicxml-P1-0") == 0);
    assert(strcmp(entity_id_value(note_event_id(import.events[3])), "musicxml-P1-4") == 0);
    musicxml_import_destroy(&import);
    assert(import.events == NULL && import.count == 0);
    musicxml_import_destroy(&import);
}

static void test_ties_merge_across_barlines(void) {
    MusicXmlImport import = import_fixture("ties_barline.musicxml");
    assert(import.count == 2 && import.warnings.count == 0);
    assert_note(&import, 0, 0, 1, 1, 2, 60);
    assert_note(&import, 1, 1, 2, 7, 4, 62); /* 4 + 8 + 2 divisions of 8 per whole note */
    musicxml_import_destroy(&import);
}

static void test_a_divisions_change(void) {
    MusicXmlImport import = import_fixture("divisions_change.musicxml");
    assert(import.count == 3 && import.warnings.count == 0);
    assert_note(&import, 0, 0, 1, 1, 4, 60);
    assert_note(&import, 1, 1, 4, 1, 4, 62);
    assert_note(&import, 2, 1, 2, 1, 8, 64);
    musicxml_import_destroy(&import);
}

static void test_rests_cues_unpitched_and_grace_notes(void) {
    MusicXmlImport import = import_fixture("grace_rest_cue.musicxml");
    assert(import.count == 1);
    assert_note(&import, 0, 3, 4, 1, 4, 60); /* a rest, a cue note and an unpitched note took three quarters */
    assert(import.warnings.count == 2); /* the unpitched note and the grace note; the cue note is not a warning */
    assert(import.warnings.message_count == 2);
    assert(strstr(import.warnings.messages[0], "unpitched") != NULL);
    assert(strstr(import.warnings.messages[1], "grace") != NULL);
    assert(strstr(import.warnings.messages[1], "part P1, measure 1") != NULL);
    musicxml_import_destroy(&import);
}

static void assert_spelling(const MusicXmlImport *import, size_t i, PitchLetter letter, int accidental, int octave) {
    const PitchSpelling *spelling = note_event_optional_pitch_spelling(import->events[i]);
    assert(spelling != NULL);
    assert(pitch_spelling_letter(spelling) == letter);
    assert(pitch_spelling_accidental(spelling) == accidental);
    assert(pitch_spelling_octave(spelling) == octave);
}

static void test_spelling_and_sounding_pitch(void) {
    MusicXmlImport import = import_fixture("spelling.musicxml");
    assert(import.count == 5 && import.warnings.count == 0);
    assert_note(&import, 0, 0, 1, 1, 4, 60);
    assert_note(&import, 1, 1, 4, 1, 4, 66);
    assert_note(&import, 2, 1, 2, 1, 4, 58);
    assert_note(&import, 3, 3, 4, 1, 4, 59); /* Cb4 sounds as B3 */
    assert_note(&import, 4, 1, 1, 1, 4, 60); /* B#3 sounds as C4 */
    assert_spelling(&import, 0, PITCH_LETTER_C, 0, 4);
    assert_spelling(&import, 1, PITCH_LETTER_F, 1, 4);
    assert_spelling(&import, 2, PITCH_LETTER_B, -1, 3);
    assert_spelling(&import, 3, PITCH_LETTER_C, -1, 4); /* the written spelling is kept */
    assert_spelling(&import, 4, PITCH_LETTER_B, 1, 3);
    musicxml_import_destroy(&import);
}

#define DOC(body) "<?xml version=\"1.0\"?><score-partwise><part-list/><part id=\"P1\"><measure number=\"1\">" body \
                  "</measure></part></score-partwise>"
#define DIV4 "<attributes><divisions>4</divisions></attributes>"
#define NOTE(step, octave, duration, extra) \
    "<note><pitch><step>" step "</step><octave>" octave "</octave></pitch><duration>" duration "</duration>" extra "</note>"

static MusicXmlImportStatus import_text(const char *text, MusicXmlImport *out) {
    return musicxml_import_buffer(text, strlen(text), out);
}

static void test_structural_errors(void) {
    MusicXmlImport import;
    char path[1024];
    snprintf(path, sizeof(path), "%s/malformed.musicxml", URSATZ_MUSICXML_DIR);
    assert(musicxml_import_file(path, &import) == MUSICXML_IMPORT_ERR_MALFORMED_XML);
    assert(strstr(import.error_message, "malformed XML") != NULL);
    assert(import.events == NULL && import.pitch_values == NULL && import.part_index == NULL && import.count == 0);
    musicxml_import_destroy(&import);

    assert(import_text("<score-timewise/>", &import) == MUSICXML_IMPORT_ERR_WRONG_ROOT);
    assert(strstr(import.error_message, "score-timewise") != NULL);
    assert(import_text("<opus><part/></opus>", &import) == MUSICXML_IMPORT_ERR_WRONG_ROOT);
    assert(import.events == NULL);

    assert(import_text(DOC(NOTE("C", "4", "4", "")), &import) == MUSICXML_IMPORT_ERR_MISSING_DIVISIONS);
    assert(strstr(import.error_message, "divisions") != NULL && import.events == NULL);
    assert(import_text(DOC("<attributes><divisions>0</divisions></attributes>"), &import) == MUSICXML_IMPORT_ERR_BAD_VALUE);
    assert(import_text(DOC(DIV4 "<note><pitch><step>H</step><octave>4</octave></pitch><duration>4</duration></note>"),
                       &import) == MUSICXML_IMPORT_ERR_BAD_VALUE);
    assert(import_text(DOC(DIV4 "<note><pitch><step>C</step></pitch><duration>4</duration></note>"), &import) ==
           MUSICXML_IMPORT_ERR_BAD_VALUE);
    assert(import_text(DOC(DIV4 "<note><pitch><step>C</step><octave>4</octave></pitch></note>"), &import) ==
           MUSICXML_IMPORT_ERR_BAD_VALUE);
    assert(import_text(DOC(DIV4 NOTE("C", "4", "-4", "")), &import) == MUSICXML_IMPORT_ERR_BAD_VALUE);
    assert(strstr(import.error_message, "part P1, measure 1") != NULL);

    assert(import_text("", &import) == MUSICXML_IMPORT_ERR_MALFORMED_XML);
    assert(import_text("   \n", &import) == MUSICXML_IMPORT_ERR_MALFORMED_XML);
    assert(musicxml_import_buffer("a\0b", 3, &import) == MUSICXML_IMPORT_ERR_MALFORMED_XML);
    assert(import_text("not xml at all", &import) == MUSICXML_IMPORT_ERR_MALFORMED_XML);
    assert(import_text("<score-partwise><part></score-partwise>", &import) == MUSICXML_IMPORT_ERR_MALFORMED_XML);

    assert(musicxml_import_buffer(NULL, 4, &import) == MUSICXML_IMPORT_ERR_NULL_ARGUMENT);
    assert(musicxml_import_buffer("<a/>", 4, NULL) == MUSICXML_IMPORT_ERR_NULL_ARGUMENT);
    assert(musicxml_import_file("tests/definitely/not/here.musicxml", &import) == MUSICXML_IMPORT_ERR_IO);
    assert(musicxml_import_file(NULL, &import) == MUSICXML_IMPORT_ERR_IO);
    assert(musicxml_import_buffer("x", (size_t)MUSICXML_IMPORT_MAX_BYTES + 1, &import) == MUSICXML_IMPORT_ERR_TOO_LARGE);
    musicxml_import_destroy(NULL);
}

static void test_an_empty_score_is_not_an_error(void) {
    MusicXmlImport import;
    assert(import_text("<score-partwise><part-list/><part id=\"P1\"><measure number=\"1\"/></part></score-partwise>",
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 0 && import.warnings.count == 0);
    musicxml_import_destroy(&import);
}

static void test_a_byte_order_mark_and_entities(void) {
    MusicXmlImport import;
    const char *text = "\xEF\xBB\xBF" DOC(DIV4 "<direction><direction-type><words>a &amp; b</words></direction-type></direction>"
                                           NOTE("C", "4", "4", ""));
    assert(import_text(text, &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 1);
    musicxml_import_destroy(&import);
}

static void test_tie_warnings(void) {
    MusicXmlImport import;
    /* a stop with no start (here the second note starts later than the first ended) and a start never stopped */
    assert(import_text(DOC(DIV4 NOTE("C", "4", "4", "<tie type=\"start\"/>") NOTE("D", "4", "4", "<tie type=\"stop\"/>")
                               NOTE("E", "4", "4", "<tie type=\"start\"/>")),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 3);
    assert(import.warnings.count == 3); /* C start never stops; D stop does not match; E start never stops */
    assert(strstr(import.warnings.messages[0], "tie stop") != NULL);
    musicxml_import_destroy(&import);
}

static void test_a_tie_needs_the_same_pitch_voice_and_onset(void) {
    MusicXmlImport import;
    /* the same pitch, but in another voice: not a tie */
    assert(import_text(DOC(DIV4 NOTE("C", "4", "4", "<voice>1</voice><tie type=\"start\"/>")
                               NOTE("C", "4", "4", "<voice>2</voice><tie type=\"stop\"/>")),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 2);
    musicxml_import_destroy(&import);
    /* the <tied> notation stands in when there is no <tie> */
    assert(import_text(DOC(DIV4 NOTE("C", "4", "4", "<notations><tied type=\"start\"/></notations>")
                               NOTE("C", "4", "4", "<notations><tied type=\"stop\"/></notations>")),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 1 && import.warnings.count == 0);
    assert_note(&import, 0, 0, 1, 1, 2, 60);
    musicxml_import_destroy(&import);
}

static void test_warnings_are_counted_and_capped(void) {
    MusicXmlImport import;
    char body[4096] = DIV4;
    for (int i = 0; i < 20; i++) {
        strcat(body, "<note><grace/><pitch><step>C</step><octave>4</octave></pitch></note>");
    }
    strcat(body, NOTE("C", "4", "4", ""));
    assert(import_text(DOC("") , &import) == MUSICXML_IMPORT_OK);
    musicxml_import_destroy(&import);
    char document[8192];
    snprintf(document, sizeof(document), DOC("%s"), body);
    assert(import_text(document, &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 1);
    assert(import.warnings.count == 20);
    assert(import.warnings.message_count == MUSICXML_IMPORT_MAX_WARNING_MESSAGES);
    musicxml_import_destroy(&import);
}

static void test_a_microtonal_alter_is_rounded(void) {
    MusicXmlImport import;
    assert(import_text(DOC(DIV4 "<note><pitch><step>C</step><alter>0.5</alter><octave>4</octave></pitch><duration>4</duration></note>"),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 1 && import.warnings.count == 1);
    assert(strstr(import.warnings.messages[0], "microtonal") != NULL);
    assert_note(&import, 0, 0, 1, 1, 4, 61);
    musicxml_import_destroy(&import);
}

static void test_a_backup_before_the_measure_is_stopped(void) {
    MusicXmlImport import;
    assert(import_text(DOC(DIV4 NOTE("C", "4", "4", "") "<backup><duration>16</duration></backup>" NOTE("D", "4", "4", "")),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 2 && import.warnings.count == 1);
    assert_note(&import, 1, 0, 1, 1, 4, 62);
    musicxml_import_destroy(&import);
}

int main(void) {
    test_chords_and_backup();
    test_ties_merge_across_barlines();
    test_a_divisions_change();
    test_rests_cues_unpitched_and_grace_notes();
    test_spelling_and_sounding_pitch();
    test_structural_errors();
    test_an_empty_score_is_not_an_error();
    test_a_byte_order_mark_and_entities();
    test_tie_warnings();
    test_a_tie_needs_the_same_pitch_voice_and_onset();
    test_warnings_are_counted_and_capped();
    test_a_microtonal_alter_is_rounded();
    test_a_backup_before_the_measure_is_stopped();
    printf("test_musicxml_importer: ok\n");
    return 0;
}
