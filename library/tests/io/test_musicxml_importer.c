#include "io/musicxml_importer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"
#include "events/dynamics.h"
#include "events/tempo_marking.h"
#include "time/meter_map.h"
#include "time/metronome_marking.h"
#include "time/tempo_map.h"
#include "time/time_span.h"
#include "time/time_signature.h"
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

/* --- measures, time signatures and key signatures --- */

static void assert_time(const MusicalTime *time, long numerator, unsigned long denominator) {
    assert(equals(musical_time_numerator(time), musical_time_denominator(time), numerator, denominator));
}

static MusicalTime *at(long numerator, unsigned long denominator) {
    MusicalTime *time = NULL;
    assert(musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK);
    return time;
}

static int measure_at(const MeterMap *map, long numerator, unsigned long denominator) {
    MusicalTime *time = at(numerator, denominator);
    int measure = -99;
    assert(meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK);
    musical_time_destroy(time);
    return measure;
}

static void test_no_time_signature_is_a_defaulted_map(void) {
    MusicXmlImport import = import_fixture("chords_backup.musicxml");
    assert(import.meter_map != NULL && meter_map_is_defaulted(import.meter_map));
    assert(import.key_signature_count == 0 && import.key_signatures == NULL);
    musicxml_import_destroy(&import);
    assert(import.meter_map == NULL);
}

static void test_a_pickup_is_measure_zero(void) {
    MusicXmlImport import = import_fixture("pickup.musicxml");
    assert(import.count == 3 && import.warnings.count == 0);
    assert(!meter_map_is_defaulted(import.meter_map));
    assert(meter_map_has_pickup(import.meter_map) && meter_map_first_measure_number(import.meter_map) == 0);
    assert(measure_at(import.meter_map, 0, 1) == 0);
    assert(measure_at(import.meter_map, 1, 4) == 1); /* a barline is the measure that starts there */
    assert(measure_at(import.meter_map, 5, 4) == 2);
    assert_note(&import, 0, 0, 1, 1, 4, 67);
    assert_note(&import, 1, 1, 4, 1, 1, 72);
    musicxml_import_destroy(&import);
}

static void test_a_full_implicit_first_measure_is_not_a_pickup(void) {
    MusicXmlImport import;
    assert(import_text("<score-partwise><part-list/><part id=\"P1\"><measure number=\"1\" implicit=\"yes\">"
                       "<attributes><divisions>4</divisions><time><beats>4</beats><beat-type>4</beat-type></time></attributes>"
                       "<note><rest/><duration>16</duration></note></measure>"
                       "<measure number=\"2\"><note><rest/><duration>16</duration></note></measure></part></score-partwise>",
                       &import) == MUSICXML_IMPORT_OK);
    assert(!meter_map_has_pickup(import.meter_map) && meter_map_first_measure_number(import.meter_map) == 1);
    musicxml_import_destroy(&import);
}

static void test_a_meter_change(void) {
    MusicXmlImport import = import_fixture("meter_change.musicxml");
    assert(import.count == 3 && import.warnings.count == 0);
    assert(!meter_map_is_defaulted(import.meter_map) && meter_map_region_count(import.meter_map) == 3);
    const TimeSignature *first = meter_map_region_time_signature(meter_map_region(import.meter_map, 0));
    const TimeSignature *second = meter_map_region_time_signature(meter_map_region(import.meter_map, 1));
    const TimeSignature *third = meter_map_region_time_signature(meter_map_region(import.meter_map, 2));
    assert(time_signature_top_number(first) == 4 && time_signature_bottom_number(first) == 4);
    assert(time_signature_top_number(second) == 3 && time_signature_bottom_number(second) == 4);
    assert(time_signature_top_number(third) == 6 && time_signature_bottom_number(third) == 8);
    assert_time(meter_map_region_start(meter_map_region(import.meter_map, 1)), 1, 1);
    assert_time(meter_map_region_start(meter_map_region(import.meter_map, 2)), 7, 4);
    assert(measure_at(import.meter_map, 1, 1) == 2 && measure_at(import.meter_map, 7, 4) == 3);
    musicxml_import_destroy(&import);
}

static void test_key_signature_events(void) {
    MusicXmlImport import = import_fixture("key_change.musicxml");
    assert(import.count == 6 && import.warnings.count == 0);
    assert(import.key_signature_count == 3); /* the second part's repeat of the first is not a new event */
    const MusicXmlKeySignatureEvent *events = import.key_signatures;
    assert_time(events[0].position, 0, 1);
    assert(events[0].sharps_flats == 0 && events[0].mode_hint == MUSICXML_MODE_HINT_MAJOR && events[0].part_index == 0);
    assert(key_signature_entry_count(events[0].key_signature) == 0);
    assert_time(events[1].position, 1, 1);
    assert(events[1].sharps_flats == -3 && events[1].mode_hint == MUSICXML_MODE_HINT_MINOR);
    assert(key_signature_entry_count(events[1].key_signature) == 3); /* B E A flats */
    assert_time(events[2].position, 2, 1);
    assert(events[2].sharps_flats == 2 && events[2].mode_hint == MUSICXML_MODE_HINT_UNKNOWN);
    assert(key_signature_entry_count(events[2].key_signature) == 2); /* F C sharps */
    musicxml_import_destroy(&import);
}

static void test_a_bad_key_is_skipped(void) {
    MusicXmlImport import;
    assert(import_text(DOC(DIV4 "<attributes><key><fifths>9</fifths></key></attributes>" NOTE("C", "4", "4", "")), &import) ==
           MUSICXML_IMPORT_OK);
    assert(import.key_signature_count == 0 && import.warnings.count == 1);
    musicxml_import_destroy(&import);
}

static void test_measure_length_warnings(void) {
    MusicXmlImport import;
    const char *time44 = "<attributes><divisions>4</divisions><time><beats>4</beats><beat-type>4</beat-type></time></attributes>";
    /* a 3-quarter measure in 4/4 is a warning ... */
    char text[2048];
    snprintf(text, sizeof(text),
             "<score-partwise><part-list/><part id=\"P1\"><measure number=\"1\">%s"
             "<note><rest/><duration>16</duration></note></measure><measure number=\"2\">"
             "<note><rest/><duration>12</duration></note></measure></part></score-partwise>",
             time44);
    assert(import_text(text, &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 1 && strstr(import.warnings.messages[0], "measure 2") != NULL);
    musicxml_import_destroy(&import);
    /* ... but not when that measure is marked implicit */
    snprintf(text, sizeof(text),
             "<score-partwise><part-list/><part id=\"P1\"><measure number=\"1\">%s"
             "<note><rest/><duration>16</duration></note></measure><measure number=\"2\" implicit=\"yes\">"
             "<note><rest/><duration>12</duration></note></measure></part></score-partwise>",
             time44);
    assert(import_text(text, &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 0);
    musicxml_import_destroy(&import);
}

static void test_additive_and_unsupported_meters(void) {
    MusicXmlImport import;
    assert(import_text(DOC("<attributes><divisions>2</divisions><time><beats>3+2</beats><beat-type>8</beat-type></time></attributes>"
                           "<note><rest/><duration>5</duration></note>"),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 1 && strstr(import.warnings.messages[0], "additive") != NULL);
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(import.meter_map, 0));
    assert(time_signature_top_number(signature) == 5 && time_signature_bottom_number(signature) == 8);
    musicxml_import_destroy(&import);

    assert(import_text(DOC("<attributes><divisions>4</divisions><time><beats>4</beats><beat-type>3</beat-type></time></attributes>"
                           "<note><rest/><duration>4</duration></note>"),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 1 && meter_map_is_defaulted(import.meter_map));
    musicxml_import_destroy(&import);

    assert(import_text(DOC("<attributes><divisions>4</divisions><time><senza-misura/></time></attributes>"
                           "<note><rest/><duration>4</duration></note>"),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 0 && meter_map_is_defaulted(import.meter_map));
    musicxml_import_destroy(&import);
}

static void test_parts_that_disagree_about_the_meter(void) {
    MusicXmlImport import;
    assert(import_text("<score-partwise><part-list/>"
                       "<part id=\"P1\"><measure number=\"1\"><attributes><divisions>4</divisions>"
                       "<time><beats>4</beats><beat-type>4</beat-type></time></attributes>"
                       "<note><rest/><duration>16</duration></note></measure></part>"
                       "<part id=\"P2\"><measure number=\"1\"><attributes><divisions>4</divisions>"
                       "<time><beats>3</beats><beat-type>4</beat-type></time></attributes>"
                       "<note><rest/><duration>12</duration></note></measure></part></score-partwise>",
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 1 && strstr(import.warnings.messages[0], "disagree") != NULL);
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(import.meter_map, 0));
    assert(time_signature_top_number(signature) == 4); /* the first part's wins */
    musicxml_import_destroy(&import);
}

/* --- parts, staves and voices --- */

static const char *voice_of(const MusicXmlImport *import, size_t i) {
    const VoiceReference *voice = note_event_optional_voice(import->events[i]);
    return voice == NULL ? NULL : entity_id_value(voice_reference_target_id(voice));
}

static void test_two_parts(void) {
    MusicXmlImport import = import_fixture("two_parts.musicxml");
    assert(import.part_count == 2 && import.count == 3 && import.warnings.count == 0);
    assert(strcmp(import.parts[0].id, "P1") == 0 && strcmp(import.parts[0].name, "Flute") == 0);
    assert(strcmp(import.parts[1].id, "P2") == 0 && strcmp(import.parts[1].name, "Cello") == 0);
    assert(import.parts[0].staff_count == 1 && import.parts[1].staff_count == 1);
    /* the flute's C5 and the cello's C3 start together; each part has its own divisions */
    assert_note(&import, 0, 0, 1, 1, 1, 72);
    assert_note(&import, 1, 0, 1, 1, 2, 48);
    assert_note(&import, 2, 1, 2, 1, 2, 55);
    assert(import.part_index[0] == 0 && import.part_index[1] == 1 && import.part_index[2] == 1);
    assert(strcmp(entity_id_value(note_event_id(import.events[0])), "musicxml-P1-0") == 0);
    assert(strcmp(entity_id_value(note_event_id(import.events[2])), "musicxml-P2-1") == 0);
    assert(voice_of(&import, 0) == NULL); /* no <voice> notated, no voice */
    musicxml_import_destroy(&import);
}

static void test_two_voices_on_one_staff(void) {
    MusicXmlImport import = import_fixture("two_voices_one_staff.musicxml");
    assert(import.count == 3 && import.warnings.count == 0);
    /* time order: the first voice's E5 and the second voice's C4 start together */
    assert_note(&import, 0, 0, 1, 1, 4, 76);
    assert_note(&import, 1, 0, 1, 1, 2, 60);
    assert_note(&import, 2, 1, 4, 1, 4, 79);
    assert(strcmp(voice_of(&import, 0), "musicxml-voice-P1-1") == 0);
    assert(strcmp(voice_of(&import, 1), "musicxml-voice-P1-2") == 0);
    assert(strcmp(voice_of(&import, 2), "musicxml-voice-P1-1") == 0);
    for (size_t i = 0; i < import.count; i++) {
        assert(import.staff[i] == 1);
    }
    musicxml_import_destroy(&import);
}

static void test_two_staves(void) {
    MusicXmlImport import = import_fixture("two_staves.musicxml");
    assert(import.count == 3 && import.warnings.count == 0 && import.parts[0].staff_count == 2);
    assert_note(&import, 0, 0, 1, 1, 2, 76);
    assert_note(&import, 1, 0, 1, 1, 2, 48);
    assert_note(&import, 2, 1, 2, 1, 2, 74);
    assert(import.staff[0] == 1 && import.staff[1] == 2 && import.staff[2] == 1);
    assert(strcmp(voice_of(&import, 0), "musicxml-voice-P1-1") == 0);
    assert(strcmp(voice_of(&import, 1), "musicxml-voice-P1-5") == 0);
    assert(voice_of(&import, 2) == NULL);
    musicxml_import_destroy(&import);
}

static void test_transposing_parts(void) {
    MusicXmlImport import = import_fixture("transposing_part.musicxml");
    assert(import.count == 2);
    assert(import.warnings.count == 2); /* one for each transposing part */
    assert(strstr(import.warnings.messages[0], "transposing") != NULL && strstr(import.warnings.messages[0], "-2") != NULL);
    assert(strstr(import.warnings.messages[1], "+12") != NULL);
    assert_note(&import, 0, 0, 1, 1, 1, 70); /* a clarinet's written C5 sounds as B-flat 4 */
    assert_note(&import, 1, 0, 1, 1, 1, 84); /* a piccolo's sounds an octave up */
    assert_spelling(&import, 0, PITCH_LETTER_C, 0, 5); /* the written spelling is kept */
    assert_spelling(&import, 1, PITCH_LETTER_C, 0, 5);
    musicxml_import_destroy(&import);
}

static void test_a_bad_transposition_and_staff_are_ignored(void) {
    MusicXmlImport import;
    assert(import_text(DOC("<attributes><divisions>4</divisions><transpose><chromatic>x</chromatic></transpose></attributes>"
                           NOTE("C", "4", "4", "<staff>99</staff>")),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.count == 1 && import.warnings.count == 2);
    assert_note(&import, 0, 0, 1, 1, 4, 60);
    assert(import.staff[0] == 1);
    musicxml_import_destroy(&import);
}

/* --- tempo and dynamics --- */

static void test_tempo_and_metronome(void) {
    MusicXmlImport import = import_fixture("tempo_metronome.musicxml");
    assert(import.count == 4);
    /* the tempo map: 96 from 0, 60 from measure 2, 92.5 (rounded) from measure 4 */
    assert(!tempo_map_is_defaulted(import.tempo_map) && tempo_map_region_count(import.tempo_map) == 3);
    const TempoMapRegion *first = tempo_map_region(import.tempo_map, 0);
    const TempoMapRegion *second = tempo_map_region(import.tempo_map, 1);
    const TempoMapRegion *third = tempo_map_region(import.tempo_map, 2);
    assert_time(tempo_map_region_start(first), 0, 1);
    assert(tempo_map_region_microseconds_per_quarter(first) == 625000);
    assert_time(tempo_map_region_start(second), 1, 1);
    assert(tempo_map_region_microseconds_per_quarter(second) == 1000000);
    assert_time(tempo_map_region_start(third), 3, 1);
    assert(tempo_map_region_microseconds_per_quarter(third) == 648649);
    /* the markings: "Allegro" with its metronome in measure 1, and the dotted-quarter metronome alone in measure 3 */
    assert(import.tempo_marking_count == 2);
    const TempoMarking *allegro = import.tempo_markings[0];
    assert(tempo_marking_has_text(allegro) && strcmp(tempo_marking_text(allegro), "Allegro") == 0);
    assert(tempo_marking_has_metronome_marking(allegro));
    const MetronomeMarking *quarter = tempo_marking_metronome_marking(allegro);
    assert(metronome_marking_beat_unit_numerator(quarter) == 1 && metronome_marking_beat_unit_denominator(quarter) == 4);
    assert(metronome_marking_bpm_numerator(quarter) == 96 && metronome_marking_bpm_denominator(quarter) == 1);
    assert_time(temporal_anchor_position(tempo_marking_temporal_anchor(allegro)), 0, 1);
    const TempoMarking *dotted = import.tempo_markings[1];
    assert(!tempo_marking_has_text(dotted) && tempo_marking_has_metronome_marking(dotted));
    const MetronomeMarking *three_eighths = tempo_marking_metronome_marking(dotted);
    assert(metronome_marking_beat_unit_numerator(three_eighths) == 3 && metronome_marking_beat_unit_denominator(three_eighths) == 8);
    assert(metronome_marking_bpm_numerator(three_eighths) == 60 && metronome_marking_bpm_denominator(three_eighths) == 1);
    assert_time(temporal_anchor_position(tempo_marking_temporal_anchor(dotted)), 2, 1);
    assert(import.tempo_marking_part[0] == 0 && import.tempo_marking_part[1] == 0);
    /* warnings: the rounded tempo, and the standalone "rit." that is not tempo text */
    assert(import.warnings.count == 2);
    assert(strstr(import.warnings.messages[0], "rounded") != NULL);
    assert(strstr(import.warnings.messages[1], "not classified") != NULL);
    musicxml_import_destroy(&import);
    assert(import.tempo_map == NULL && import.tempo_markings == NULL);
}

static void test_no_tempo_is_a_defaulted_map(void) {
    MusicXmlImport import = import_fixture("chords_backup.musicxml");
    assert(tempo_map_is_defaulted(import.tempo_map) && tempo_map_region_count(import.tempo_map) == 1);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo_map, 0)) == 500000);
    assert(import.tempo_marking_count == 0 && import.dynamics_count == 0);
    musicxml_import_destroy(&import);
}

static void test_dynamics(void) {
    MusicXmlImport import = import_fixture("dynamics.musicxml");
    assert(import.count == 4);
    assert(import.dynamics_count == 4); /* the pp at the end has no extent */
    assert(import.warnings.count == 1 && strstr(import.warnings.messages[0], "pp") != NULL);
    static const char *const NAMES[4] = {"p", "ff", "mf", "sfz"};
    static const long FROM_N[4] = {0, 0, 1, 9};
    static const unsigned long FROM_D[4] = {1, 1, 1, 8};
    static const long TO_N[4] = {1, 2, 9, 2};
    static const unsigned long TO_D[4] = {1, 1, 8, 1};
    static const size_t PART[4] = {0, 1, 0, 0};
    static const int STAFF[4] = {1, 2, 1, 1};
    for (size_t i = 0; i < 4; i++) {
        assert(strcmp(dynamics_marking(import.dynamics[i]), NAMES[i]) == 0);
        assert(dynamics_target_kind(import.dynamics[i]) == DYNAMICS_TARGET_TIME_SPAN);
        const TimeSpan *span = dynamics_target_time_span(import.dynamics[i]);
        assert_time(time_span_start(span), FROM_N[i], FROM_D[i]);
        assert_time(time_span_end(span), TO_N[i], TO_D[i]);
        assert(import.dynamics_part[i] == PART[i] && import.dynamics_staff[i] == STAFF[i]);
    }
    musicxml_import_destroy(&import);
    assert(import.dynamics == NULL && import.dynamics_count == 0);
}

static void test_tempo_edge_cases(void) {
    MusicXmlImport import;
    /* a bad tempo and a bad metronome are warnings, not errors */
    assert(import_text(DOC(DIV4 "<sound tempo=\"fast\"/><direction><direction-type><metronome><beat-unit>quarter</beat-unit>"
                           "<per-minute>x</per-minute></metronome></direction-type></direction>" NOTE("C", "4", "4", "")),
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 2 && tempo_map_is_defaulted(import.tempo_map) && import.tempo_marking_count == 0);
    musicxml_import_destroy(&import);
    /* two parts that disagree about the tempo at one position: the first part's wins */
    assert(import_text("<score-partwise><part-list/>"
                       "<part id=\"P1\"><measure number=\"1\"><attributes><divisions>4</divisions></attributes>"
                       "<sound tempo=\"120\"/><note><rest/><duration>4</duration></note></measure></part>"
                       "<part id=\"P2\"><measure number=\"1\"><attributes><divisions>4</divisions></attributes>"
                       "<sound tempo=\"100\"/><note><rest/><duration>4</duration></note></measure></part></score-partwise>",
                       &import) == MUSICXML_IMPORT_OK);
    assert(import.warnings.count == 1 && strstr(import.warnings.messages[0], "disagree") != NULL);
    assert(tempo_map_region_count(import.tempo_map) == 1);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo_map, 0)) == 500000);
    musicxml_import_destroy(&import);
    /* a tempo that starts after 0 leaves a defaulted region before it */
    assert(import_text(DOC(DIV4 NOTE("C", "4", "4", "") "<sound tempo=\"60\"/>" NOTE("D", "4", "4", "")), &import) ==
           MUSICXML_IMPORT_OK);
    assert(tempo_map_region_count(import.tempo_map) == 2);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo_map, 0)) == 500000);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo_map, 1)) == 1000000);
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
    test_no_time_signature_is_a_defaulted_map();
    test_a_pickup_is_measure_zero();
    test_a_full_implicit_first_measure_is_not_a_pickup();
    test_a_meter_change();
    test_key_signature_events();
    test_a_bad_key_is_skipped();
    test_measure_length_warnings();
    test_additive_and_unsupported_meters();
    test_parts_that_disagree_about_the_meter();
    test_two_parts();
    test_two_voices_on_one_staff();
    test_two_staves();
    test_transposing_parts();
    test_a_bad_transposition_and_staff_are_ignored();
    test_tempo_and_metronome();
    test_no_tempo_is_a_defaulted_map();
    test_dynamics();
    test_tempo_edge_cases();
    printf("test_musicxml_importer: ok\n");
    return 0;
}
