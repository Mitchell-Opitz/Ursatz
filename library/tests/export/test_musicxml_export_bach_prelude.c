#include "export/musicxml_export.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "io/midi_importer.h"
#include "../io/midi_import_split.h"
#include "ursatz_test.h"

/*
 * Regression for the multi-track voice-assignment bug: Bach_Prelude_C_
 * Major.mid (tests/fixtures/arpeggiation-investigation/) is a type-1,
 * single-channel file with RH and LH on two separate note-bearing
 * tracks. Before per-track voice assignment, assign_voice_slots only
 * ran when the whole file was single-track, so every note here kept
 * optional_voice == NULL, the export grouped RH and LH into one implicit
 * voice, and their overlapping notes tripped MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES. It must now import with RH/LH
 * assigned to distinct voices and export cleanly.
 */

#define FIXTURE_PATH URSATZ_FIXTURES_DIR "/arpeggiation-investigation/Bach_Prelude_C_Major.mid"

int main(void) {
    size_t size = 0;
    unsigned char *data = (unsigned char *)test_must_read_file(FIXTURE_PATH, &size);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = test_midi_import_parts(data, size, &events, &pitch_values, &count, NULL, NULL);
    free(data);
    assert(import_result == MIDI_IMPORT_OK);
    assert(count > 0);

    /* Every note must have been assigned a voice, and at least two
       distinct voice ids must appear -- RH and LH landed in separate
       voices rather than all sharing the single implicit one. */
    const char *first_voice_id = NULL;
    bool saw_distinct_voice = false;
    for (size_t i = 0; i < count; i++) {
        const VoiceReference *voice = note_event_optional_voice(events[i]);
        assert(voice != NULL);
        const char *voice_id = entity_id_value(voice_reference_target_id(voice));
        if (first_voice_id == NULL) {
            first_voice_id = voice_id;
        } else if (strcmp(first_voice_id, voice_id) != 0) {
            saw_distinct_voice = true;
        }
    }
    assert(saw_distinct_voice);

    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = events;
    score.pitch_values = pitch_values;
    score.count = count;
    char *xml = NULL;
    MusicXmlExportResult export_result = musicxml_export_write_score(&score, &xml);
    assert(export_result == MUSICXML_EXPORT_OK);
    assert(xml != NULL);
    assert(strstr(xml, "<score-partwise version=\"3.1\">") != NULL);

    free(xml);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);

    printf("musicxml_export_bach_prelude: ok (%zu notes)\n", count);
    return 0;
}
