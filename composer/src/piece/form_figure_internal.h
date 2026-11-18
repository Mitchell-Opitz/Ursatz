#ifndef URSATZ_COMPOSER_FORM_FIGURE_INTERNAL_H
#define URSATZ_COMPOSER_FORM_FIGURE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <model/core/rational.h>

#include "harmony/harmony_internal.h"
#include <ursatz_composer/form_figure.h>
#include <ursatz_composer/piece_plan.h>

/*
 * What form_figure.c reads a section as, and the helpers over it. The motive of variation writes through them, and the
 * tests' oracle (tests/support/test_oracles.c) reads the written figure back through them.
 */

/* A note of the returned section's melody, moved by the key difference: where it starts and ends, from its section's start. */
typedef struct {
    Rational onset;
    Rational end;
    int pitch;
} FigureSourceNote;

/* A section as the figure reads it: its key, chords and cells, the returned melody, and the skeleton. */
typedef struct {
    AnalysisKey key;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t chord_count;
    PhraseMetre metre;
    Rational cell_length;             /* the cell of the figure: a beat (a quarter, a dotted quarter in 6/8), from the section's start */
    size_t cells_per_bar;
    unsigned mask[FORM_SKELETON_MAX]; /* the pitch classes of the chord over each cell */
    FigureSourceNote *notes;          /* the returned melody, moved, in order of onset */
    size_t note_count;
    FormSkeleton skeleton;
} FigureReading;

/* A note of the written melody: from the start of the section. */
typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} FigureNote;

/* The melody of the section the target returns, moved by shift (and mapped into to_material when mode_change), in order of onset. NULL when it holds no note. */
FigureSourceNote *form_figure_read_melody(const PiecePlan *plan, NodeId source, int shift, bool mode_change, int tonic,
                                          const PitchMaterial *to_material, size_t *count);
bool form_figure_has_pitch_class(unsigned mask, int pitch);
/* The section as the figure reads it; NULL when it is not a figurate return or cannot be read. */
FigureReading *form_figure_read_section(const PiecePlan *plan, NodeId section);
void form_figure_reading_free(FigureReading *reading);
bool form_figure_cell_bounds(const FigureReading *reading, size_t cell, Rational *start, Rational *end);
bool form_figure_find(const PiecePlan *plan, NodeId section, PlanFigure *out);
/* The notes a figure has per cell; 0 when it is not a whole number from 2 to 8. */
size_t form_figure_notes_per_cell(const FigureReading *reading, PlanFigureRhythm rhythm, Rational *length);
/* The notes a kept cell holds, added to out. */
bool form_figure_kept_notes(const FigureReading *reading, size_t cell, int octave, FigureNote *out, size_t *count);

#endif
