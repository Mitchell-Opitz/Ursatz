#include "chain/chain_inputs_internal.h"

#include <stdlib.h>
#include <string.h>

#include "theory/meter/segmentation_beat_grid.h"

/* --- the voice grid --- */

void chain_voice_grid_destroy(ChainVoiceGrid *grid) {
    if (grid == NULL) {
        return;
    }
    voice_assignment_destroy(&grid->assignment);
    free(grid->cells);
    free(grid->segment_ids);
    memset(grid, 0, sizeof(*grid));
}

ChainInputStatus chain_build_voice_grid(const AnalysisNoteSet *notes, const ChainPitches *pitches, const ChainSegments *segments,
                                        size_t count, ChainVoiceGrid *out) {
    if (notes == NULL || pitches == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0 || pitches->count != notes->count || count == 0 ||
        count > segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainVoiceGrid built;
    memset(&built, 0, sizeof(built));
    if (voice_separate(pitches->events, pitches->values, notes->count, NULL, notes->map, standard_meter_segmentation_beat_grid, NULL,
                       NULL, &built.assignment) != VOICE_SEPARATION_OK) {
        return CHAIN_INPUT_ERR_SEGMENTATION;
    }
    size_t voices = 0;
    for (size_t s = 0; s < built.assignment.stream_count; s++) {
        voices = built.assignment.streams[s].voice_count > voices ? built.assignment.streams[s].voice_count : voices;
    }
    if (voices == 0) {
        chain_voice_grid_destroy(&built);
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    built.cells = calloc(count * voices, sizeof(AnalysisVoiceCell));
    built.segment_ids = calloc(count, sizeof(char *));
    if (built.cells == NULL || built.segment_ids == NULL) {
        chain_voice_grid_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t s = 0; s < count; s++) {
        built.segment_ids[s] = segments->ids[s];
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        Rational start;
        if (!chain_rational_of(time_span_start(window), &start)) {
            chain_voice_grid_destroy(&built);
            return CHAIN_INPUT_ERR_SEGMENTATION;
        }
        for (size_t i = 0; i < notes->count; i++) {
            const AnalysisNote *note = &notes->notes[i];
            Rational on;
            Rational off;
            if (!chain_rational_of(&note->span.start, &on) || !chain_rational_of(&note->span.end, &off)) {
                chain_voice_grid_destroy(&built);
                return CHAIN_INPUT_ERR_SEGMENTATION;
            }
            if (!(rational_cmp(on, start) <= 0 && rational_cmp(start, off) < 0)) {
                continue;
            }
            size_t voice = built.assignment.entries[i].voice;
            if (voice >= voices || built.cells[s * voices + voice].present) {
                continue; /* an extra note in the same voice: the first stands */
            }
            AnalysisVoiceCell *cell = &built.cells[s * voices + voice];
            cell->present = true;
            cell->pitch = pitches->values[i];
            cell->spelling = note->spelling;
            cell->note_id = note->note_id;
            cell->attacked = rational_cmp(on, start) == 0;
            cell->confidence = built.assignment.entries[i].confidence;
        }
    }
    built.grid.segment_count = count;
    built.grid.voice_count = voices;
    built.grid.segment_ids = built.segment_ids;
    built.grid.cells = built.cells;
    *out = built;
    return CHAIN_INPUT_OK;
}
