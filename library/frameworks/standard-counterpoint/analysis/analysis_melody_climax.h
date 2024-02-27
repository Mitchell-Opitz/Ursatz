#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_CLIMAX_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_CLIMAX_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_melody.h"

/*
 * The climax of each span (the melody adapter, step 2). The climax of a span is its HIGHEST pitch, the definition of the
 * melodic rules (melodic_rules.h: "A climax is a line's highest pitch; the first note at that pitch is the climax's
 * position"). Rests are skipped. Per span the claim records:
 *
 *   pitch               the highest pitch (semitones)
 *   first_index         the entry of the first note at that pitch
 *   occurrence_count    how many notes in the span are at that pitch; occurrences[] lists their entry indices (up to
 *                       STANDARD_COUNTERPOINT_CLIMAX_LISTED, in order; the count is always complete). Repeated top notes are
 *                       LISTED, never collapsed into one
 *   unique              exactly one occurrence (the UNIQUE_CLIMAX rule's condition)
 *   relative_position   the first occurrence's place among the span's NOTES: its rank (0-based, rests not counted) over
 *                       (notes - 1); 0 for a span of one note. By note order, not by time: a rhythm-weighted position needs
 *                       the phrase and rhythm analyses that do not exist yet
 *   has_strength/strength   the BeatStrength of the first occurrence, when the note has one
 *   by_leap             the first occurrence is approached by a leap from a note in the span (CLIMAX_BY_LEAP's condition)
 *   final_note          the first occurrence is the span's last note (what CLIMAX_NOT_FINAL forbids)
 *
 * The shell holds every occurrence's note id. uncertainty = 1 - the least confidence of the span's notes (the climax
 * depends on all their pitches).
 */

#define STANDARD_COUNTERPOINT_CLIMAX_CLAIM_TYPE "standard-counterpoint/melodic-climax"
#define STANDARD_COUNTERPOINT_CLIMAX_LISTED 16

typedef struct {
    int span_first;
    int span_last;
    long pitch;
    int first_index;
    int occurrence_count;
    int occurrences[STANDARD_COUNTERPOINT_CLIMAX_LISTED];
    int unique;
    double relative_position;
    int has_strength;
    int strength;
    int by_leap;
    int final_note;
} StandardCounterpointClimaxClaim;

typedef struct {
    char span_id[64];
    StandardCounterpointClimaxClaim claim;
    double uncertainty;
    Structure *shell;               /* owned: the occurrences' note ids */
    Interpretation *interpretation; /* owned */
} StandardCounterpointClimax;

typedef struct {
    StandardCounterpointClimax *items; /* owned; in span order; NULL when none */
    size_t count;
} StandardCounterpointClimaxes;

/* One climax per span that holds a note. spans as in standard_counterpoint_melody_contour_summaries (NULL for the whole
   line). intervals is the line's intervals (for by_leap). */
StandardCounterpointMelodyStatus standard_counterpoint_melody_climaxes(const StandardCounterpointMelodyLine *line,
                                                                       const StandardCounterpointMelodicIntervals *intervals,
                                                                       const StandardCounterpointMelodySpan *spans,
                                                                       size_t span_count,
                                                                       StandardCounterpointClimaxes *out);
void standard_counterpoint_climaxes_destroy(StandardCounterpointClimaxes *climaxes);

bool standard_counterpoint_climax_claim_read(const Interpretation *interpretation,
                                             StandardCounterpointClimaxClaim *out);

#endif
