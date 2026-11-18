#include "io/imported_score.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "model/identity/entity_id.h"
#include "model/time/temporal_anchor.h"

/*
 * The imported score (R-614): a MIDI and a MusicXML fixture read by file and by buffer give equal scores, the format is
 * told by extension and by bytes, a compressed MusicXML is refused, a reading rescales the score and its markings, and
 * a view borrows its events and builds a 4/4 map and a key signature. The library prints nothing.
 */

#define MIDI_FIXTURE URSATZ_FIXTURES_DIR "/framework-v1-corpus/Bach 114.mid"
#define XML_METRONOME URSATZ_FIXTURES_DIR "/musicxml/tempo_metronome.musicxml"
#define XML_DYNAMICS URSATZ_FIXTURES_DIR "/musicxml/dynamics.musicxml"
#define XML_PICKUP URSATZ_FIXTURES_DIR "/musicxml/pickup.musicxml"
#define XML_MALFORMED URSATZ_FIXTURES_DIR "/musicxml/malformed.musicxml"

static unsigned char *read_all(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    assert(length > 0);
    rewind(file);
    unsigned char *bytes = malloc((size_t)length);
    assert(bytes != NULL && fread(bytes, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return bytes;
}

static bool event_equal(const NoteEvent *a, const NoteEvent *b) {
    const MusicalTime *start_a = temporal_anchor_position(note_event_temporal_anchor(a));
    const MusicalTime *start_b = temporal_anchor_position(note_event_temporal_anchor(b));
    return rational_equals(start_a->value, start_b->value) &&
           rational_equals(note_event_duration(a)->value, note_event_duration(b)->value) &&
           entity_id_equals(note_event_id(a), note_event_id(b));
}

static void assert_scores_equal(const ImportedScore *a, const ImportedScore *b) {
    assert(a->count == b->count && a->count > 0);
    assert(a->source.format == b->source.format);
    for (size_t i = 0; i < a->count; i++) {
        assert(event_equal(a->events[i], b->events[i]));
        assert(a->streams[i] == b->streams[i]);
        assert(pitch_numeric_value_position_numerator(a->pitch_values[i]) ==
               pitch_numeric_value_position_numerator(b->pitch_values[i]));
    }
    assert(a->source.part_count == b->source.part_count);
    for (size_t i = 0; i < a->source.part_count; i++) {
        assert(strcmp(a->source.parts[i].id, b->source.parts[i].id) == 0);
        assert(strcmp(a->source.parts[i].name, b->source.parts[i].name) == 0);
        assert(a->source.parts[i].staff_count == b->source.parts[i].staff_count);
    }
    assert(a->source.tempo_marking_count == b->source.tempo_marking_count);
    for (size_t i = 0; i < a->source.tempo_marking_count; i++) {
        const ImportedTempoMarking *x = &a->source.tempo_markings[i];
        const ImportedTempoMarking *y = &b->source.tempo_markings[i];
        assert(rational_equals(x->position, y->position) && rational_equals(x->bpm, y->bpm) &&
               rational_equals(x->beat_unit, y->beat_unit) && x->has_metronome == y->has_metronome);
    }
    assert(a->source.dynamics_count == b->source.dynamics_count);
    for (size_t i = 0; i < a->source.dynamics_count; i++) {
        assert(rational_equals(a->source.dynamics[i].start, b->source.dynamics[i].start));
        assert(rational_equals(a->source.dynamics[i].end, b->source.dynamics[i].end));
    }
    assert(a->source.warning_count == b->source.warning_count);
    assert(a->source.warning_line_count == b->source.warning_line_count);
    assert(musical_time_cmp(a->meter.end_time, b->meter.end_time) == 0);
    assert(a->key_signature_events.count == b->key_signature_events.count);
}

static void check_file_and_buffer(const char *path, ImportedFormat format) {
    ImportedScore from_file;
    ImportedScore from_buffer;
    ImportedScoreError error = {{0}};
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &from_file, &error) == IMPORTED_SCORE_OK);
    size_t size = 0;
    unsigned char *bytes = read_all(path, &size);
    assert(imported_score_import_buffer(bytes, size, IMPORTED_FORMAT_AUTO, &from_buffer, &error) == IMPORTED_SCORE_OK);
    assert(from_file.source.format == format);
    assert_scores_equal(&from_file, &from_buffer);
    free(bytes);
    imported_score_destroy(&from_file);
    imported_score_destroy(&from_buffer);
}

static void test_file_and_buffer_agree(void) {
    check_file_and_buffer(MIDI_FIXTURE, IMPORTED_FORMAT_MIDI);
    check_file_and_buffer(XML_METRONOME, IMPORTED_FORMAT_MUSICXML);
    check_file_and_buffer(XML_DYNAMICS, IMPORTED_FORMAT_MUSICXML);
    check_file_and_buffer(XML_PICKUP, IMPORTED_FORMAT_MUSICXML);

    /* The markings are Rationals read from the notation. */
    ImportedScore score;
    assert(imported_score_import_file(XML_METRONOME, IMPORTED_FORMAT_AUTO, &score, NULL) == IMPORTED_SCORE_OK);
    assert(score.source.tempo_marking_count >= 1);
    const ImportedTempoMarking *marking = &score.source.tempo_markings[0];
    assert(marking->has_metronome && marking->bpm.num > 0 && marking->bpm.den > 0 && marking->beat_unit.num > 0);
    imported_score_destroy(&score);
}

static void test_format_detection(void) {
    ImportedFormat format = IMPORTED_FORMAT_AUTO;
    assert(imported_format_parse("midi", &format) && format == IMPORTED_FORMAT_MIDI);
    assert(imported_format_parse("musicxml", &format) && format == IMPORTED_FORMAT_MUSICXML);
    format = IMPORTED_FORMAT_AUTO;
    assert(!imported_format_parse("abc", &format) && format == IMPORTED_FORMAT_AUTO);

    /* By extension, whatever the file holds (the files need not exist). */
    assert(imported_format_detect("/nowhere/piece.mid") == IMPORTED_FORMAT_MIDI);
    assert(imported_format_detect("/nowhere/piece.MIDI") == IMPORTED_FORMAT_MIDI);
    assert(imported_format_detect("/nowhere/piece.musicxml") == IMPORTED_FORMAT_MUSICXML);
    assert(imported_format_detect("/nowhere/piece.xml") == IMPORTED_FORMAT_MUSICXML);
    assert(imported_format_detect("/nowhere/piece.mxl") == IMPORTED_FORMAT_MUSICXML);
    assert(imported_format_detect("/nowhere/piece.txt") == IMPORTED_FORMAT_AUTO);

    /* By the first bytes when the extension says nothing. */
    char path[] = "/tmp/ursatz-imported-score-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    const char *midi_head = "MThd\0\0\0\6";
    assert(write(fd, midi_head, 8) == 8);
    close(fd);
    assert(imported_format_detect(path) == IMPORTED_FORMAT_MIDI);
    fd = open(path, O_WRONLY | O_TRUNC);
    assert(fd >= 0);
    const char *xml_head = "  <?xml version=\"1.0\"?><score-partwise/>";
    assert(write(fd, xml_head, strlen(xml_head)) == (ssize_t)strlen(xml_head));
    close(fd);
    assert(imported_format_detect(path) == IMPORTED_FORMAT_MUSICXML);
    fd = open(path, O_WRONLY | O_TRUNC);
    assert(fd >= 0);
    assert(write(fd, "hello", 5) == 5);
    close(fd);
    assert(imported_format_detect(path) == IMPORTED_FORMAT_AUTO);
    unlink(path);
}

static void test_errors(void) {
    ImportedScore score;
    ImportedScoreError error = {{0}};

    const unsigned char zip[] = {'P', 'K', 3, 4, 0, 0, 0, 0};
    assert(imported_score_import_buffer(zip, sizeof zip, IMPORTED_FORMAT_AUTO, &score, &error) ==
           IMPORTED_SCORE_ERR_COMPRESSED);
    assert(strstr(error.message, "compressed MusicXML") != NULL);

    const unsigned char text[] = "hello";
    assert(imported_score_import_buffer(text, sizeof text - 1, IMPORTED_FORMAT_AUTO, &score, &error) ==
           IMPORTED_SCORE_ERR_UNKNOWN_FORMAT);
    assert(imported_score_import_buffer(text, sizeof text - 1, IMPORTED_FORMAT_MIDI, &score, &error) ==
           IMPORTED_SCORE_ERR_FORMAT);
    assert(strstr(error.message, "failed to import the input") != NULL);
    assert(imported_score_import_buffer(NULL, 4, IMPORTED_FORMAT_AUTO, &score, &error) == IMPORTED_SCORE_ERR_ARGUMENT);
    assert(imported_score_import_buffer(text, 0, IMPORTED_FORMAT_AUTO, &score, &error) == IMPORTED_SCORE_ERR_ARGUMENT);
    assert(imported_score_import_buffer(text, 5, IMPORTED_FORMAT_AUTO, NULL, &error) == IMPORTED_SCORE_ERR_ARGUMENT);

    assert(imported_score_import_file("/nowhere/missing.mid", IMPORTED_FORMAT_AUTO, &score, &error) ==
           IMPORTED_SCORE_ERR_IO);
    assert(strcmp(error.message, "Error: cannot read file '/nowhere/missing.mid'") == 0);
    assert(imported_score_import_file(XML_MALFORMED, IMPORTED_FORMAT_AUTO, &score, &error) == IMPORTED_SCORE_ERR_FORMAT);
    assert(strstr(error.message, "malformed.musicxml") != NULL);
    /* The error is optional. */
    assert(imported_score_import_file(XML_MALFORMED, IMPORTED_FORMAT_AUTO, &score, NULL) == IMPORTED_SCORE_ERR_FORMAT);
}

static void test_reading_rescales(void) {
    ImportedScore score;
    ImportedScore before;
    assert(imported_score_import_file(XML_DYNAMICS, IMPORTED_FORMAT_AUTO, &score, NULL) == IMPORTED_SCORE_OK);
    assert(imported_score_import_file(XML_DYNAMICS, IMPORTED_FORMAT_AUTO, &before, NULL) == IMPORTED_SCORE_OK);
    assert(before.source.dynamics_count > 0);

    ImportedReading reading = imported_reading_none();
    assert(reading.scale.num == 1 && reading.scale.den == 1 && !reading.has_meter && !reading.has_pickup);
    assert(imported_reading_parse_scale("2", &reading));
    assert(reading.scale.num == 2 && reading.scale.den == 1);
    assert(imported_reading_apply(&score, &reading) == IMPORTED_READING_OK);
    assert(score.source.reading_applied && rational_equals(score.source.reading.scale, reading.scale));
    for (size_t i = 0; i < score.count; i++) {
        Rational doubled;
        assert(rational_mul(temporal_anchor_position(note_event_temporal_anchor(before.events[i]))->value,
                            reading.scale, &doubled) == RATIONAL_OK);
        assert(rational_equals(temporal_anchor_position(note_event_temporal_anchor(score.events[i]))->value, doubled));
        assert(entity_id_equals(note_event_id(score.events[i]), note_event_id(before.events[i])));
    }
    for (size_t i = 0; i < score.source.dynamics_count; i++) {
        Rational start;
        assert(rational_mul(before.source.dynamics[i].start, reading.scale, &start) == RATIONAL_OK);
        assert(rational_equals(score.source.dynamics[i].start, start));
    }

    ImportedReading pickup = imported_reading_none();
    assert(imported_reading_parse_meter("3/4", &pickup) && imported_reading_parse_pickup("1/4", &pickup));
    assert(pickup.has_pickup && pickup.pickup.num == 1 && pickup.pickup.den == 4);
    ImportedReading bad = imported_reading_none();
    bad.scale = rational_ratio(3, 1);
    assert(imported_reading_apply(&before, &bad) == IMPORTED_READING_ERR_INVALID);
    imported_score_destroy(&score);
    imported_score_destroy(&before);
}

static void test_view_borrows_its_events(void) {
    ImportedScore source;
    assert(imported_score_import_file(MIDI_FIXTURE, IMPORTED_FORMAT_AUTO, &source, NULL) == IMPORTED_SCORE_OK);
    enum { COUNT = 8 };
    const NoteEvent *events[COUNT];
    int midi[COUNT];
    size_t streams[COUNT];
    for (size_t i = 0; i < COUNT; i++) {
        events[i] = source.events[i];
        midi[i] = (int)pitch_numeric_value_position_numerator(source.pitch_values[i]);
        streams[i] = i % 2;
    }
    ImportedScoreView view = {.events = events, .count = COUNT, .midi = midi, .streams = streams, .has_key = true,
                              .key_fifths = -3, .key_minor = true};
    ImportedScore score;
    assert(imported_score_view(&view, &score) == IMPORTED_SCORE_OK);
    assert(score.count == COUNT && score.events_borrowed && !score.meter_map_borrowed);
    assert(score.source.format == IMPORTED_FORMAT_MIDI && score.source.part_count == 0);
    for (size_t i = 0; i < COUNT; i++) {
        assert(score.events[i] == source.events[i]);
        assert(score.streams[i] == i % 2);
        assert(pitch_numeric_value_position_numerator(score.pitch_values[i]) == midi[i]);
    }
    /* No meter map given: 4/4, defaulted, owned by the view. */
    assert(score.meter.meter_map != NULL && meter_map_is_defaulted(score.meter.meter_map));
    assert(score.key_signature.found && score.key_signature.sharps_flats == -3 && score.key_signature.is_minor);
    assert(score.key_signature_events.count == 1 && score.key_signature_events.events[0].sharps_flats == -3);
    /* The score ends with the latest note. */
    assert(musical_time_cmp(score.meter.end_time, musical_time_of(rational_ratio(0, 1))) > 0);

    imported_score_destroy(&score);
    /* The events are still the source's: the view freed only what it built. */
    for (size_t i = 0; i < COUNT; i++) {
        assert(note_event_id(source.events[i]) != NULL);
    }

    /* A meter map that is not defaulted is borrowed, not copied. */
    ImportedScoreView with_map = {.events = events, .count = COUNT, .midi = midi, .meter = source.meter.meter_map};
    ImportedScore mapped;
    assert(imported_score_view(&with_map, &mapped) == IMPORTED_SCORE_OK);
    assert(!meter_map_is_defaulted(source.meter.meter_map) || !mapped.meter_map_borrowed);
    if (mapped.meter_map_borrowed) {
        assert(mapped.meter.meter_map == source.meter.meter_map);
    }
    imported_score_destroy(&mapped);
    assert(source.meter.meter_map != NULL);

    /* Refused views. */
    ImportedScore refused;
    int bad_midi[COUNT];
    memcpy(bad_midi, midi, sizeof midi);
    bad_midi[3] = 128;
    ImportedScoreView bad = {.events = events, .count = COUNT, .midi = bad_midi};
    assert(imported_score_view(&bad, &refused) == IMPORTED_SCORE_ERR_ARGUMENT);
    ImportedScoreView no_midi = {.events = events, .count = COUNT};
    assert(imported_score_view(&no_midi, &refused) == IMPORTED_SCORE_ERR_ARGUMENT);
    ImportedScoreView bad_key = {.events = events, .count = COUNT, .midi = midi, .has_key = true, .key_fifths = 8};
    assert(imported_score_view(&bad_key, &refused) == IMPORTED_SCORE_ERR_ARGUMENT);
    assert(imported_score_view(NULL, &refused) == IMPORTED_SCORE_ERR_ARGUMENT);
    ImportedScoreView empty = {0};
    assert(imported_score_view(&empty, &refused) == IMPORTED_SCORE_OK && refused.count == 0);
    imported_score_destroy(&refused);
    imported_score_destroy(&source);
}

int main(void) {
    test_file_and_buffer_agree();
    test_format_detection();
    test_errors();
    test_reading_rescales();
    test_view_borrows_its_events();
    return 0;
}
