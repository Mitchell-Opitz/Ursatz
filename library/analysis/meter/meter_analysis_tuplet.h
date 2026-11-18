#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_TUPLET_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_ANALYSIS_METER_ANALYSIS_TUPLET_H

#include <stdbool.h>
#include <stddef.h>

#include "model/interpretation/interpretation.h"
#include "analysis/meter/meter_analysis_input.h"
#include "theory/meter/meter_class_rules.h"
#include "analysis/notes/analysis_note.h"
#include "theory/meter/borrowed_rules.h"
#include "model/structure/structure.h"

/*
 * Tuplets and duplets from real music. Every note is read on a grid of 96 steps to the whole note (a twelfth is a triplet
 * eighth, a 24th a sixteenth triplet): an onset snapped per standard_meter_config_default() that lies within a quarter of a
 * step of the grid is read, anything further (a performance's rubato) is left out. A release is read on the grid too when it is
 * on it, from the note's own span, not the snapped one, since the release snap would pull a triplet's onto its 1/32 grid.
 *
 *   triplet   a note whose attack, or distance to the next attack, is off the 1/32 grid (3:2, sextuplets included)
 *   duplet    in a compound meter a note on the beat whose next attack is half a beat later, and the note there lasts, or is
 *             followed by an attack after, half a beat (2:3). A dotted figure and a short note do not match
 *   run       the consecutive attacks of one ratio; a single attack is no group and is dropped
 *
 * The result is in the order of smaller `actual` first, then measure, then beat. `name` is "triplet", "duplet" or "tuplet";
 * `meter` the type of the signature at the run's first note; `borrowed` whether the ratio is borrowed in that type; `level` is
 * where the run's span sits against the beat when the rules give one (the caller renders its name). All pointers are owned by
 * the result except `name`, which is static.
 */

typedef struct {
    size_t *notes; /* owned: indices into the note set */
    size_t note_count;
    unsigned actual;
    unsigned normal;
    const char *name;
    unsigned attacks;
    int measure;
    double beat;
    Rational first; /* where the run's first note is attacked, in whole notes: orders the tuplets of one measure */
    MeterType meter;
    bool borrowed;
    bool has_level;
    TupletLevel level;
    bool level_typical;
} StandardMeterTuplet;

typedef struct {
    StandardMeterTuplet *items;
    size_t count;
} StandardMeterTuplets;

/* Finds the tuplets. `in_line` (one entry per note) is NULL for all notes; false skips a note. *out is untouched on failure. */
StandardMeterAnalysisStatus standard_meter_find_tuplets(const AnalysisNoteSet *notes, const bool *in_line,
                                                        StandardMeterTuplets *out);

void standard_meter_tuplets_destroy(StandardMeterTuplets *tuplets);

#endif
