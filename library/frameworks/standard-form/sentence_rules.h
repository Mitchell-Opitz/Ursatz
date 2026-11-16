#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_SENTENCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_SENTENCE_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "continuation_trait.h"
#include "semantics/subphrase_role.h"

/*
 * Sentence rules for standard-form-v1. A sentence is a special kind of phrase made of two
 * subphrases, a presentation and a continuation:
 *
 *   STRUCTURE                  a presentation comes first, then a continuation
 *   PRESENTATION_LENGTH        a presentation is often four measures
 *   BASIC_IDEA_LENGTH          a basic idea is often two measures, with one and four
 *                              also occurring
 *   CONTINUATION_LENGTH        a continuation is often as long as the presentation, just as
 *                              often longer, and uncommonly shorter
 *   FRAGMENTATION              fragmentation makes unit sizes shorter than the units
 *                              before it, and says nothing about their melodic content
 *   CONTINUATION_TRAITS        a continuation is marked by fragmentation, faster durations,
 *                              sequence, and faster harmonic rhythm; it is easier to
 *                              identify with more of them, and fragmentation alone usually
 *                              signals it
 *   REPETITION_CONTOUR         a second idea with the same contour as the first is heard as
 *                              its varied repetition; a different contour is heard as
 *                              contrast
 *   BASIC_IDEA_TRANSFORMATIONS how a basic idea is commonly varied when repeated
 *   SENTENTIAL                 at a minimum a sentential phrase has a presentation and a
 *                              continuation
 *   ARCHETYPAL                 the archetypal sentence is 4 or 8 measures with a continuation
 *                              that balances the presentation and has fragmentation
 *
 * Lengths are in a single consistent unit chosen by the caller (measures in the examples).
 */

/* STRUCTURE: true only for a presentation followed by a continuation. */
bool standard_form_subphrase_roles_in_order(SubphraseRole first, SubphraseRole second);

/* PRESENTATION_LENGTH: true for the usual length of four. Any other length is unusual. */
bool standard_form_presentation_length_usual(size_t measures);

/* BASIC_IDEA_LENGTH: true for the lengths 1, 2, and 4. */
bool standard_form_basic_idea_length_valid(size_t measures);

/* CONTINUATION_LENGTH: true when the continuation is at least as long as the presentation (a
   zero-length presentation is not valid). A shorter continuation is uncommon, not wrong. */
bool standard_form_continuation_length_usual(size_t presentation, size_t continuation);

/* CONTINUATION_LENGTH: true when the two lengths are equal and non-zero, the balance of the
   archetypal sentence. */
bool standard_form_continuation_balanced(size_t presentation, size_t continuation);

/* FRAGMENTATION: true when unit_size is shorter than established_unit_size and not zero. Both
   are in the same unit. Only length counts, not melodic content. */
bool standard_form_is_fragmentation(size_t established_unit_size, size_t unit_size);

/* REPETITION_CONTOUR: true when a second idea reads as a varied repetition of the first
   (same contour); false when it reads as contrast. */
bool standard_form_idea_reads_as_repetition(bool same_contour);

/* SENTENTIAL: true when both subphrases are present. */
bool standard_form_phrase_is_sentential(bool has_presentation, bool has_continuation);

/* ARCHETYPAL: true when the sentence is 4 or 8 measures, the presentation and continuation
   fill it and balance, and the continuation has fragmentation. */
bool standard_form_sentence_is_archetypal(size_t total_measures, size_t presentation_measures,
                                          size_t continuation_measures, bool has_fragmentation);

#endif
