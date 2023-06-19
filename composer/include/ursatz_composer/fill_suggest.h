#ifndef URSATZ_COMPOSER_FILL_SUGGEST_H
#define URSATZ_COMPOSER_FILL_SUGGEST_H

#include <stddef.h>

#include "generation/candidate.h"

#include <ursatz_composer/fill_loop.h>

/*
 * Suggestion mode: for one target, the top N candidates, scored and ranked,
 * instead of a choice. The plan is not changed.
 *
 * The candidates come back as an Ursatz CandidateSet, best first. Each Candidate
 * names its material by id and carries its score. It has no Interpretations,
 * since there is no framework to explain a score yet; the label and operator
 * are on the FillSuggestions beside it. The payloads stay in the FillSuggestions
 * so that a suggestion the user likes can be applied, by its index in the set.
 *
 * Candidates are found, checked and rejected the way the fill loop does (same
 * operators, same checker, same seeded stream from config->seed), so a rejected
 * candidate is never suggested. Equal scores keep the order they were proposed in.
 */

typedef struct FillSuggestions FillSuggestions;

/*
 * Suggests up to top_n candidates for target, which must be open (Constrained):
 * FILL_ERR_TARGET_NOT_OPEN otherwise. With nothing to suggest the set is empty,
 * which is not an error. *out is destroyed with fill_suggestions_destroy.
 */
FillResult fill_suggest(const PiecePlan *plan, const FillOperatorRegistry *registry, const FillConfig *config,
                        FillTarget target, size_t top_n, FillSuggestions **out);

/* The suggested candidates, best first. Owned by the suggestions. */
const CandidateSet *fill_suggestions_set(const FillSuggestions *suggestions);
size_t fill_suggestions_count(const FillSuggestions *suggestions);
FillTarget fill_suggestions_target(const FillSuggestions *suggestions);
/* The operator that proposed suggestion index, and its label. NULL past the end. */
const char *fill_suggestion_operator(const FillSuggestions *suggestions, size_t index);
const char *fill_suggestion_label(const FillSuggestions *suggestions, size_t index);

/*
 * Applies suggestion index to plan the way the loop would: it applies the
 * candidate, marks the target Realized, and propagates. The plan must be the
 * one the suggestions were made for, unchanged since. FILL_ERR_TARGET_NOT_OPEN
 * if the target is no longer open, FILL_ERR_NULL for an index past the end.
 */
FillResult fill_apply_suggestion(PiecePlan *plan, const FillSuggestions *suggestions, size_t index);

void fill_suggestions_destroy(FillSuggestions *suggestions);

#endif
