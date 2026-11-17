#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_SEGMENTATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_SEGMENTATION_RULES_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Segmentation analysis rules for standard-form-v1. A segmentation analysis shows the
 * grouping units of a passage, from phrase endings down to the idea level, using square
 * brackets above the staff. It is a procedure over existing types (Phrase, Idea, Cadence),
 * not a stored entity:
 *
 *   STEP_ORDER    the four steps run in order: identify points of closure, divide each
 *                 phrase into smaller units, bracket those units, then verify and label
 *                 the cadences
 *   IDEA_LENGTH   an idea is often two measures long but may be longer or shorter
 *   IDEA_DIVISION the ideas of a phrase are non-empty and together fill the phrase exactly
 *
 * Lengths are whole measures.
 */

/* IDEA_LENGTH: true for any length of at least one measure; ideas may be longer or shorter
   than the usual two. */
bool standard_form_idea_length_valid(size_t measures);

/* IDEA_DIVISION: true when idea_measures has at least one entry, every entry is a valid idea
   length, and the entries add up to phrase_measures. */
bool standard_form_ideas_fill_phrase(const size_t *idea_measures, size_t idea_count, size_t phrase_measures);

#endif
