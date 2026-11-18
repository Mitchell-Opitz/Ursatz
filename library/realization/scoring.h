#ifndef URSATZ_REALIZATION_SCORING_H
#define URSATZ_REALIZATION_SCORING_H

#include <stdbool.h>
#include <stddef.h>

#include "model/time/time_span.h"
#include "realization/line.h"
#include "realization/performer_part.h"
#include "realization/realization.h"

/*
 * Scoring (target architecture 4.11, R-534): a scoring assignment places a line, or a span of it, into a performer part, on a staff
 * and in a voice, with an optional transform. `realization_score` makes the performer notes from the assignments. One line can go to
 * several parts (doubling), several lines to one part (a reduction), spans hand a line from part to part, and an octave transform
 * moves what a part plays. `divisi` is notation only: it changes no note.
 */

typedef enum {
    SCORING_TRANSFORM_NONE,
    SCORING_TRANSFORM_OCTAVE,
    SCORING_TRANSFORM_INTERVAL /* needs spelling in a key: realization_score refuses it with NOT_BUILT */
} ScoringTransformKind;

typedef struct {
    ScoringTransformKind kind;
    int octaves;   /* OCTAVE: -4 to 4, the pitch moves by 12 times this */
    int semitones; /* INTERVAL */
    int steps;     /* INTERVAL */
} ScoringTransform;

typedef struct {
    char id[REALIZATION_ID_MAX];
    char line[REALIZATION_ID_MAX]; /* the id of the line */
    char part[REALIZATION_ID_MAX]; /* the id of the performer part */
    bool whole;                    /* the whole line; `span` is then not read */
    TimeSpan span;                 /* the line notes whose onset is in [start, end) */
    int staff;                     /* from 1 */
    int voice;                     /* from 1 */
    ScoringTransform transform;
    bool divisi;
} ScoringAssignment;

/* BAD_ID (an id of the assignment is invalid), BAD_VALUE (staff or voice below 1, a transform kind out of range, octaves outside -4
   to 4, a span that does not hold unless `whole`), NULL for no assignment. */
RealizationResult scoring_assignment_check(const ScoringAssignment *assignment);

/*
 * Scores lines into parts. All checks come first and nothing changes on a failure: a bad assignment (scoring_assignment_check),
 * duplicate ids among the assignments, the lines or the parts (DUPLICATE_ID), an unknown line or part (UNKNOWN_REFERENCE), a staff
 * above the part's staff count, a pitch the transform takes outside 0 to 127 or a generated id longer than an id may be (BAD_VALUE), an
 * INTERVAL transform (NOT_BUILT), a run that is not printable text of at most 63 characters (BAD_VALUE).
 *
 * Then, per part: notes that are edited, detached or have no source (independent material) stay; every other note goes. Then for
 * each assignment to the part, in assignment order, each note of its line, in line order, whose onset is in [span.start, span.end)
 * (every note when `whole`) gives a performer note with id "<part id>-<line note id>" ("-2", "-3" and on for the same id again in
 * the part), the line note's onset and duration (not clipped at the span's end), its pitch plus 12 times the octaves, its spelling,
 * the assignment's staff and voice, and derivation sources [line note id], operation "score", run `run`. A generated note whose id
 * a kept note holds is not added; the kept note gets `review` when its pitch, onset or duration differ from the generated one, and so
 * does a kept note with a source that is no longer among the assigned line notes. The part's notes end ordered by onset, ties by
 * assignment order then line note order, kept notes after generated ones at an equal onset in their earlier order. Two runs over
 * the same inputs give the same ids and order.
 */
RealizationResult realization_score(Line *const *lines, size_t line_count, const ScoringAssignment *assignments,
                                    size_t assignment_count, PerformerPart *const *parts, size_t part_count, const char *run);

#endif
