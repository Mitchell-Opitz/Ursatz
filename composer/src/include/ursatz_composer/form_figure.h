#ifndef URSATZ_COMPOSER_FORM_FIGURE_H
#define URSATZ_COMPOSER_FORM_FIGURE_H

#include <stdbool.h>
#include <stddef.h>

#include <core/rational.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The motive of variation (R-497): a section that returns another with the variation FIGURATE keeps the course of the
 * returned one and rewrites its melody from a skeleton of it, each cell of the meter circumscribed by one figure.
 *
 * The skeleton has one principal tone per cell of the meter (the cell the harmony operators place chords on: a quarter
 * in 2/4, 3/4 and 4/4, a dotted quarter in 6/8), a tone of the chord over the cell.
 */

/* The most cells a skeleton holds. */
#define FORM_SKELETON_MAX 256

typedef struct {
    size_t count;
    int pitch[FORM_SKELETON_MAX];     /* the principal tone of each cell, in the section's key */
    Rational onset[FORM_SKELETON_MAX]; /* where each cell starts, from the start of the section */
    bool kept[FORM_SKELETON_MAX];     /* the cell is in the last bar of the section or of its first half: the theme's notes stay */
} FormSkeleton;

/*
 * The skeleton of a section that returns another with the variation FIGURATE (form_return_copies holds) and whose
 * harmony lane is written, in one key. The returned section's melody is read moved by the key difference; per cell the
 * principal tone is
 *   - the moved note sounding at the cell's start, when it is a tone of the chord over the cell;
 *   - else the first moved note that starts inside the cell and is a chord tone;
 *   - else the chord tone nearest the note sounding at the cell's start (the lower of two equally near).
 * kept is true for the cells of the section's last bar and of the last bar of its first half. False when the section is
 * not such a return, the harmony or the returned melody cannot be read, the section does not start on a barline, or it
 * has more than FORM_SKELETON_MAX cells.
 */
bool form_figure_skeleton(const PiecePlan *plan, NodeId section, FormSkeleton *out);

/*
 * Rewrites the melody spans of a section (voice 0) from its skeleton and its FIGURE constraint. Each cell not kept holds
 * N equal notes, N the cell over an eighth or a sixteenth (2 or 4 in 2/4, 3/4 and 4/4; 3 or 6 in 6/8), the first the
 * skeleton tone; a kept cell keeps the notes of the returned section, moved, as they are (a note that sounds into a kept
 * cell from the cell before it starts again at the cell, and a note ends where the next cell that is not kept begins).
 *   neighbor      the odd positions are the tone's neighbor in the key's scale, above or below: the first side is drawn
 *                 once for the section, the sides then alternate from one cell to the next. A neighbor more than two
 *                 semitones away takes the other side.
 *   scale         stepwise in the scale from the tone toward the next cell's first note (the next skeleton tone, or the
 *                 first note of a kept cell), turning back once it is reached; toward the same tone, up or down as drawn
 *                 once for the section, half of the notes away and the rest back.
 *   broken-chord  the tones of the chord over the cell from the tone, up or down as drawn once for the section, past the
 *                 octave as needed.
 * The scale is the key's major or natural minor; in minor, over a chord that holds the leading tone (V, vii), the harmonic
 * minor. A cell with a note outside range->range_low to range->range_high is made again the other way (the other side,
 * the other direction); if that is outside too, the write fails. With the climax, the whole melody is then moved an octave
 * up when every note stays inside the range. range NULL gives the default range. False when the section has no skeleton,
 * a note length that does not divide the cell, a span the notes do not fit, or a Given melody span; the plan may then be
 * partly written.
 */
bool form_figure_write(PiecePlan *plan, NodeId section, const MelodyFillParams *range, FillRng *rng);

/*
 * Whether the section's melody is a figure over the skeleton: every cell not kept holds N equal notes from the skeleton
 * tone (an octave up for a raised climax) and keeps to the kind of its figure (neighbor: even positions the tone, odd
 * ones 1 or 2 semitones from it; scale: every interval inside the cell a step, 1 or 2 semitones, 3 for the augmented
 * second of the harmonic minor; broken-chord: every note a tone of the chord over the cell), and every kept cell holds the
 * notes of the returned section, moved (an octave up for a raised climax). On false, reason (size bytes) holds the first
 * fault.
 */
bool form_figure_check(const PiecePlan *plan, NodeId section, char *reason, size_t size);

#endif
