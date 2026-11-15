#ifndef URSATZ_COMPOSER_FILL_INTERNAL_H
#define URSATZ_COMPOSER_FILL_INTERNAL_H

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/fill_rng.h>

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
                       FillTarget target, FillRng *rng, FillEntries *out);

/* True when a scores higher than b. */
bool fill_check_scores_higher(const FillCheck *a, const FillCheck *b);

/* Applies an entry's candidate, marks the target Realized if it is still open, and propagates. */
FillResult fill_apply_entry(PiecePlan *plan, const FillOperatorRegistry *registry, FillTarget target,
                            const FillEntry *entry);

/* Starts a record for a run with this seed. */
FillResult fill_record_begin(FillRecord **out, unsigned long seed);
/* Appends the step for an attempt. chosen is an index into entries, or -1. */
FillResult fill_record_add_step(FillRecord *record, FillTarget target, FillStepOutcome outcome,
                                const FillEntries *entries, const FillOperatorRegistry *registry, long chosen,
                                uint64_t rng_state_before, uint64_t rng_state_after);

#endif
