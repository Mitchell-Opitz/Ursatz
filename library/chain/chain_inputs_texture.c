#include "chain/chain_inputs_internal.h"

#include <stdlib.h>
#include <string.h>

#include "theory/meter/segmentation_beat_grid.h"

/* --- the texture input --- */

void chain_texture_input_destroy(ChainTextureInput *input) {
    if (input == NULL) {
        return;
    }
    free(input->notes);
    voice_assignment_destroy(&input->assignment);
    memset(input, 0, sizeof(*input));
}

ChainInputStatus chain_build_texture_input(const AnalysisNoteSet *notes, const ChainPitches *pitches, ChainTextureInput *out) {
    if (notes == NULL || pitches == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0 || pitches->count != notes->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    size_t *stream = calloc(notes->count, sizeof(size_t));
    if (stream == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    size_t highest = 0;
    for (size_t i = 0; i < notes->count; i++) {
        stream[i] = notes->notes[i].track == ANALYSIS_INDEX_NONE ? 0 : notes->notes[i].track;
        highest = stream[i] > highest ? stream[i] : highest;
    }
    ChainTextureInput build;
    memset(&build, 0, sizeof(build));
    build.stream_count = highest + 1;
    if (voice_separate(pitches->events, pitches->values, notes->count, stream, notes->map, standard_meter_segmentation_beat_grid,
                       NULL, NULL, &build.assignment) != VOICE_SEPARATION_OK) {
        free(stream);
        return CHAIN_INPUT_ERR_SEGMENTATION;
    }
    build.notes = (AnalysisNote *)calloc(notes->count, sizeof(AnalysisNote));
    ChainInputStatus status = build.notes != NULL ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_ALLOC;
    for (size_t i = 0; i < notes->count && status == CHAIN_INPUT_OK; i++) {
        AnalysisNote *note = &build.notes[i];
        const VoiceAssignmentEntry *entry = &build.assignment.entries[i];
        *note = notes->notes[i];
        note->track = stream[i];
        note->voice = entry->voice;
        note->voice_confidence = entry->confidence;
        /* A note the texture analysis cannot read is refused here, where the input is built. */
        if (note->midi == ANALYSIS_PITCH_UNKNOWN || note->note_id[0] == '\0' ||
            strlen(note->note_id) >= STANDARD_TEXTURE_ID_MAX || !(note->voice_confidence >= 0.0 && note->voice_confidence <= 1.0)) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    build.set.notes = build.notes;
    build.set.count = notes->count;
    build.set.map = notes->map;
    free(stream);
    if (status != CHAIN_INPUT_OK) {
        chain_texture_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}
