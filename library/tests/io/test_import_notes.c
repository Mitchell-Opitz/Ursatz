#include "io/import_notes.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "io/midi_importer.h"
#include "io/musicxml_importer.h"
#include "time/time_rational.h"

/*
 * AnalysisNotes from imports: one note per imported event, sorted by analysis_note_compare, borrowing the import's event, id,
 * spelling and voice id, with a span built from the event's onset and duration, the midi number of the import's parallel
 * PitchNumericValue, and the track (MIDI) or part (MusicXML). The files are the framework-v1 corpus (MIDI) and the MusicXML
 * fixtures.
 */

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)length + 1);
    assert(data != NULL);
    assert(fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return data;
}

static size_t index_of(NoteEvent *const *events, size_t count, const NoteEvent *event) {
    for (size_t i = 0; i < count; i++) {
        if (events[i] == event) {
            return i;
        }
    }
    return (size_t)-1;
}

/* The checks both formats share. `tracks` is the track or part of each event, parallel to events. */
static void check_notes(NoteEvent *const *events, PitchNumericValue *const *values, const size_t *tracks, size_t event_count,
                        const AnalysisNote *notes, size_t count) {
    assert(count == event_count);
    bool *seen = calloc(event_count + 1, sizeof(bool));
    assert(seen != NULL);
    for (size_t i = 0; i < count; i++) {
        const AnalysisNote *note = &notes[i];
        size_t source = index_of(events, event_count, note->event);
        assert(source != (size_t)-1 && !seen[source]);
        seen[source] = true;
        assert(analysis_note_valid(note));
        assert(note->note_id == entity_id_value(note_event_id(note->event)));
        assert(note->spelling == note_event_optional_pitch_spelling(note->event));
        const VoiceReference *voice = note_event_optional_voice(note->event);
        if (voice != NULL) {
            assert(note->voice_id == entity_id_value(voice_reference_target_id(voice)) && note->voice_confidence == 1.0);
        } else {
            assert(note->voice_id == NULL && note->voice_confidence == 0.0);
        }
        assert(note->midi == pitch_numeric_value_position_numerator(values[source]));
        assert(note->track == tracks[source]);
        assert(note->voice == ANALYSIS_INDEX_NONE);
        Rational start;
        Rational end;
        Rational onset;
        Rational duration;
        assert(rational_of_musical_time(time_span_start(note->span), &start) == RATIONAL_OK);
        assert(rational_of_musical_time(time_span_end(note->span), &end) == RATIONAL_OK);
        assert(rational_of_musical_time(temporal_anchor_position(note_event_temporal_anchor(note->event)), &onset) == RATIONAL_OK);
        assert(rational_of_duration(note_event_duration(note->event), &duration) == RATIONAL_OK);
        Rational release;
        assert(rational_add(onset, duration, &release) == RATIONAL_OK);
        assert(rational_equals(start, onset) && rational_equals(end, release));
        if (i > 0) {
            assert(analysis_note_compare(&notes[i - 1], note) <= 0);
        }
    }
    free(seen);
}

static void test_midi(const char *corpus_dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/Mozart 545.mid", corpus_dir);
    size_t size = 0;
    unsigned char *data = read_file(path, &size);
    MidiImport import;
    assert(midi_importer_import_complete(data, size, &import) == MIDI_IMPORT_OK);
    free(data);
    assert(import.count > 100);

    AnalysisNote *notes = NULL;
    size_t count = 0;
    assert(midi_import_analysis_notes(&import, &notes, &count));
    check_notes(import.events, import.pitch_values, import.note_tracks, import.count, notes, count);
    AnalysisNoteSet set = {notes, count, import.meter.meter_map};
    assert(analysis_note_set_valid(&set));
    import_analysis_notes_destroy(notes, count);
    midi_import_destroy(&import);
}

static void test_musicxml(const char *dir, const char *file) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    MusicXmlImport import;
    assert(musicxml_import_file(path, &import) == MUSICXML_IMPORT_OK);
    assert(import.count > 0);
    AnalysisNote *notes = NULL;
    size_t count = 0;
    assert(musicxml_import_analysis_notes(&import, &notes, &count));
    check_notes(import.events, import.pitch_values, import.part_index, import.count, notes, count);
    AnalysisNoteSet set = {notes, count, import.meter_map};
    assert(analysis_note_set_valid(&set));
    import_analysis_notes_destroy(notes, count);
    musicxml_import_destroy(&import);
}

static void test_errors(void) {
    AnalysisNote *notes = (AnalysisNote *)0x1;
    size_t count = 7;
    MidiImport midi;
    memset(&midi, 0, sizeof(midi));
    MusicXmlImport xml;
    memset(&xml, 0, sizeof(xml));
    assert(!midi_import_analysis_notes(NULL, &notes, &count));
    assert(!midi_import_analysis_notes(&midi, NULL, &count));
    assert(!midi_import_analysis_notes(&midi, &notes, NULL));
    assert(!musicxml_import_analysis_notes(NULL, &notes, &count));
    assert(notes == (AnalysisNote *)0x1 && count == 7);

    /* an import with no notes gives no array */
    assert(midi_import_analysis_notes(&midi, &notes, &count) && notes == NULL && count == 0);
    notes = (AnalysisNote *)0x1;
    count = 7;
    assert(musicxml_import_analysis_notes(&xml, &notes, &count) && notes == NULL && count == 0);
    import_analysis_notes_destroy(NULL, 0);
}

int main(void) {
    test_errors();
    test_midi(URSATZ_CORPUS_DIR);
    test_musicxml(URSATZ_MUSICXML_DIR, "pickup.musicxml");
    test_musicxml(URSATZ_MUSICXML_DIR, "chords_backup.musicxml");
    test_musicxml(URSATZ_MUSICXML_DIR, "meter_change.musicxml");
    printf("test_import_notes: ok\n");
    return 0;
}
