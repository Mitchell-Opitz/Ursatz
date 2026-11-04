#include "analysis_grid.h"

#include <math.h>
#include <stdlib.h>
#include "pitch/midi_pitch.h"

/* The sharp-preferring spelling of a semitone: the letter, the accidental (0 or 1), and the octave. */
static void derive(long semitone, PitchLetter *letter, int *accidental, int *octave) {
    static const PitchLetter LETTERS[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                            PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                            PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
    static const int ACCIDENTALS[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    long pitch_class = pitch_class_of_midi(semitone);
    long floor_octave = (semitone - pitch_class) / 12;
    *letter = LETTERS[pitch_class];
    *accidental = ACCIDENTALS[pitch_class];
    *octave = (int)(floor_octave - 1); /* MIDI 60 is C4 */
}

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
        PitchLetter letter;
        int accidental;
        int octave;
        if (cell->spelling != NULL) {
            letter = pitch_spelling_letter(cell->spelling);
            accidental = pitch_spelling_accidental(cell->spelling);
            octave = pitch_spelling_octave(cell->spelling);
        } else {
            derive(pitch_numeric_value_position_numerator(cell->pitch), &letter, &accidental, &octave);
        }
        if (pitch_spelling_create(letter, accidental, octave, &prepared.spellings[i]) != PITCH_SPELLING_OK) {
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
