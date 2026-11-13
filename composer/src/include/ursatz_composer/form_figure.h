#ifndef URSATZ_COMPOSER_FORM_FIGURE_H
#define URSATZ_COMPOSER_FORM_FIGURE_H

#include <stdbool.h>
#include <stddef.h>

#include <core/rational.h>

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

#endif
