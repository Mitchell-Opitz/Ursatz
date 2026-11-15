#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/musicxml_pipeline.h>

#include "support/test_support.h"

/*
 * The tempo map on imported notes: a MIDI file with two tempos carries two regions, one with no tempo event carries
 * the one defaulted region, and a MusicXML file carries none. imported_notes_destroy releases it.
 */

#define NOTE_ON_C4 0x00, 0x90, 0x3c, 0x50
#define NOTE_OFF_C4_AFTER_QUARTER 0x83, 0x60, 0x80, 0x3c, 0x00
#define END_OF_TRACK 0x00, 0xff, 0x2f, 0x00

static void import_bytes(const unsigned char *body, size_t body_size, ImportedNotes *notes) {
    unsigned char file[256];
    size_t size = test_smf_type0(file, 480, body, body_size);
    const char *path = "test_tempo_map_import.mid";
    FILE *out = fopen(path, "wb");
    assert(out != NULL);
    assert(fwrite(file, 1, size, out) == size);
    fclose(out);
    assert(midi_pipeline_import_file(path, notes) == MIDI_PIPELINE_OK);
    remove(path);
}

static void test_two_tempos(void) {
    const unsigned char body[] = {0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20, NOTE_ON_C4, NOTE_OFF_C4_AFTER_QUARTER,
                                  0x00, 0xff, 0x51, 0x03, 0x03, 0xd0, 0x90, NOTE_ON_C4, NOTE_OFF_C4_AFTER_QUARTER,
                                  END_OF_TRACK};
    ImportedNotes notes;
    import_bytes(body, sizeof body, &notes);
    assert(notes.tempo_map != NULL);
    assert(!tempo_map_is_defaulted(notes.tempo_map));
    assert(tempo_map_region_count(notes.tempo_map) == 2);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(notes.tempo_map, 0)) == 500000);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(notes.tempo_map, 1)) == 250000);
    assert(notes.source.tempo_declared);
    imported_notes_destroy(&notes);
    assert(notes.tempo_map == NULL);
}

static void test_no_tempo(void) {
    const unsigned char body[] = {NOTE_ON_C4, NOTE_OFF_C4_AFTER_QUARTER, END_OF_TRACK};
    ImportedNotes notes;
    import_bytes(body, sizeof body, &notes);
    assert(notes.tempo_map != NULL);
    assert(tempo_map_is_defaulted(notes.tempo_map));
    assert(tempo_map_region_count(notes.tempo_map) == 1);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(notes.tempo_map, 0)) == 500000);
    assert(!notes.source.tempo_declared);
    imported_notes_destroy(&notes);
}

static void test_musicxml_has_none(void) {
    ImportedNotes notes;
    assert(musicxml_pipeline_import_file(URSATZ_MUSICXML_DIR "/chorale-voice-leading.musicxml", &notes) ==
           MUSICXML_PIPELINE_OK);
    assert(notes.tempo_map == NULL);
    imported_notes_destroy(&notes);
}

int main(void) {
    test_two_tempos();
    test_no_tempo();
    test_musicxml_has_none();
    return 0;
}
