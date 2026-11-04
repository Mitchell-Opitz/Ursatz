#include "analysis_grid.h"

#include <math.h>
#include <stdlib.h>
#include "pitch/midi_pitch.h"

void standard_counterpoint_grid_destroy(StandardCounterpointPreparedGrid *prepared) {
    if (prepared == NULL) {
        return;
    }
    if (prepared->spellings != NULL && prepared->grid != NULL) {
        size_t total = prepared->grid->segment_count * prepared->grid->voice_count;
        for (size_t i = 0; i < total; i++) {
            pitch_spelling_destroy(prepared->spellings[i]);
        }
    }
    free(prepared->spellings);
    prepared->spellings = NULL;
    prepared->grid = NULL;
}

StandardCounterpointGridStatus standard_counterpoint_grid_prepare(const StandardCounterpointVoiceGrid *grid,
                                                                  StandardCounterpointPreparedGrid *out) {
    if (grid == NULL || out == NULL) {
        return STANDARD_COUNTERPOINT_GRID_ERR_NULL_ARGUMENT;
    }
    if (grid->segment_count == 0 || grid->voice_count == 0 || grid->cells == NULL || grid->segment_ids == NULL) {
        return STANDARD_COUNTERPOINT_GRID_ERR_BAD_GRID;
    }
    size_t total = grid->segment_count * grid->voice_count;
    for (size_t s = 0; s < grid->segment_count; s++) {
        if (grid->segment_ids[s] == NULL || grid->segment_ids[s][0] == '\0') {
            return STANDARD_COUNTERPOINT_GRID_ERR_BAD_GRID;
        }
    }
    for (size_t i = 0; i < total; i++) {
        const StandardCounterpointVoiceCell *cell = &grid->cells[i];
        if (!cell->present) {
            continue;
        }
        if ((cell->pitch == NULL && cell->spelling == NULL) || cell->note_id == NULL || cell->note_id[0] == '\0' ||
            !(cell->confidence >= 0.0 && cell->confidence <= 1.0)) {
            return STANDARD_COUNTERPOINT_GRID_ERR_BAD_GRID;
        }
        if (cell->spelling == NULL && pitch_numeric_value_position_denominator(cell->pitch) != 1) {
            return STANDARD_COUNTERPOINT_GRID_ERR_BAD_GRID;
        }
    }

    StandardCounterpointPreparedGrid prepared;
    prepared.grid = grid;
    prepared.spellings = (PitchSpelling **)calloc(total, sizeof(PitchSpelling *));
    if (prepared.spellings == NULL) {
        return STANDARD_COUNTERPOINT_GRID_ERR_ALLOC;
    }
    for (size_t i = 0; i < total; i++) {
        const StandardCounterpointVoiceCell *cell = &grid->cells[i];
        if (!cell->present) {
            continue;
        }
        bool created;
        if (cell->spelling != NULL) {
            created = pitch_spelling_create(pitch_spelling_letter(cell->spelling), pitch_spelling_accidental(cell->spelling),
                                            pitch_spelling_octave(cell->spelling), &prepared.spellings[i]) == PITCH_SPELLING_OK;
        } else {
            /* the sharp-preferring spelling; MIDI 60 is C4 */
            created = pitch_spelling_of_midi((int)pitch_numeric_value_position_numerator(cell->pitch), 0,
                                             &prepared.spellings[i]);
        }
        if (!created) {
            standard_counterpoint_grid_destroy(&prepared);
            return STANDARD_COUNTERPOINT_GRID_ERR_ALLOC;
        }
    }
    *out = prepared;
    return STANDARD_COUNTERPOINT_GRID_OK;
}

const StandardCounterpointVoiceCell *standard_counterpoint_grid_cell(const StandardCounterpointPreparedGrid *prepared,
                                                                     size_t segment, size_t voice) {
    if (prepared == NULL || prepared->grid == NULL || segment >= prepared->grid->segment_count ||
        voice >= prepared->grid->voice_count) {
        return NULL;
    }
    const StandardCounterpointVoiceCell *cell = &prepared->grid->cells[segment * prepared->grid->voice_count + voice];
    return cell->present ? cell : NULL;
}

const PitchSpelling *standard_counterpoint_grid_spelling(const StandardCounterpointPreparedGrid *prepared,
                                                         size_t segment, size_t voice) {
    if (standard_counterpoint_grid_cell(prepared, segment, voice) == NULL) {
        return NULL;
    }
    return prepared->spellings[segment * prepared->grid->voice_count + voice];
}
