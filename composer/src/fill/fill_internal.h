#ifndef URSATZ_COMPOSER_FILL_INTERNAL_H
#define URSATZ_COMPOSER_FILL_INTERNAL_H

#include <ursatz_composer/fill_loop.h>

/* One candidate with the operator that proposed it and the checker's verdict. */
typedef struct {
    size_t operator_index;
    FillCandidate candidate;
    FillCheck check;
} FillEntry;

typedef struct {
    FillEntry *items;
    size_t count;
    size_t capacity;
    size_t applicable; /* operators that applied to the target */
} FillEntries;

void fill_entries_destroy(FillEntries *entries);

/* Collects and checks the candidates of every applicable operator. */
FillResult fill_gather(const PiecePlan *plan, const FillOperatorRegistry *registry, const FillConfig *config,
                       FillTarget target, FillEntries *out);

/* True when a scores higher than b. */
bool fill_check_scores_higher(const FillCheck *a, const FillCheck *b);

/* Applies an entry's candidate, marks the target Realized if it is still open, and propagates. */
FillResult fill_apply_entry(PiecePlan *plan, const FillOperatorRegistry *registry, FillTarget target,
                            const FillEntry *entry);

#endif
