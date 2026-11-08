#include "export/musicxml_export.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_importer.h"
#include "persistence/persistence.h"
#include "persistence/persistence_note_events.h"

/*
 * Direct regression for the false-positive overlap bug: Mozart 545.mid
 * (tests/fixtures/framework-v1-corpus/) is a two-track type-1 file --
 * a conductor track with no notes, plus one note-bearing track holding
 * both hands of a piano part on a single channel -- so its two hands
 * legitimately overlap in time. Before per-voice overlap grouping, this
 * failed musicxml_export_write with MUSICXML_EXPORT_ERR_OVERLAPPING_
 * NOTES; it must now import and export cleanly.
 */

#define FIXTURE_PATH "../fixtures/framework-v1-corpus/Mozart 545.mid"

static unsigned char *read_fixture(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size > 0);
    assert(fseek(f, 0, SEEK_SET) == 0);

    unsigned char *buf = malloc((size_t)size);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, f) == (size_t)size);
    fclose(f);

    *out_size = (size_t)size;
    return buf;
}

int main(void) {
    size_t size = 0;
    unsigned char *data = read_fixture(FIXTURE_PATH, &size);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = midi_importer_import(data, size, &events, &pitch_values, &count);
    free(data);
    assert(import_result == MIDI_IMPORT_OK);
    assert(count > 0);

    const char *path = "test_musicxml_export_mozart545.sqlite";
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(persistence_store_note_events(handle, events, count) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    MusicXmlExportResult export_result = musicxml_export_write(handle, &xml);
    assert(export_result == MUSICXML_EXPORT_OK);
    assert(xml != NULL);
    assert(strstr(xml, "<score-partwise version=\"3.1\">") != NULL);

    free(xml);

    /* The score export with notated times: measure 4, beat 2 is three sixteenth triplets and two sixteenths, so it
       carries one 3:2 group. */
    ScoreQuantizeOptions quantize = score_quantize_options_default();
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = events;
    score.pitch_values = pitch_values;
    score.count = count;
    score.quantize = &quantize;
    char *score_xml = NULL;
    assert(musicxml_export_write_score(&score, &score_xml) == MUSICXML_EXPORT_OK);
    const char *measure = strstr(score_xml, "<measure number=\"4\"");
    assert(measure != NULL);
    const char *measure_end = strstr(measure, "</measure>");
    assert(measure_end != NULL);
    size_t starts = 0;
    size_t stops = 0;
    size_t sixteenth_triplets = 0;
    for (const char *at = measure; at < measure_end; at++) {
        if (strncmp(at, "<tuplet type=\"start\"/>", 22) == 0) {
            starts++;
        } else if (strncmp(at, "<tuplet type=\"stop\"/>", 21) == 0) {
            stops++;
        } else if (strncmp(at, "<actual-notes>3</actual-notes>", 30) == 0) {
            sixteenth_triplets++;
        }
    }
    assert(starts == 1 && stops == 1 && sixteenth_triplets == 3);
    free(score_xml);

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
    persistence_close(handle);
    remove(path);

    printf("musicxml_export_mozart545: ok (%zu notes)\n", count);
    return 0;
}
