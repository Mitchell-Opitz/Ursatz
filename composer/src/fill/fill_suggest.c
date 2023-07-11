#include <ursatz_composer/fill_suggest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"

#include <ursatz_composer/composer.h>

#include "fill/fill_internal.h"

struct FillSuggestions {
    FillTarget target;
    const FillOperatorRegistry *registry;
    FillEntries entries;   /* every gathered entry, owning the payloads */
    size_t *ranked;        /* indices into entries, best first */
    size_t count;
    CandidateSet *set;
};

void fill_suggestions_destroy(FillSuggestions *suggestions) {
    if (!suggestions) {
        return;
    }
    candidate_set_destroy(suggestions->set);
    fill_entries_destroy(&suggestions->entries);
    free(suggestions->ranked);
    free(suggestions);
}

const CandidateSet *fill_suggestions_set(const FillSuggestions *suggestions) {
    return suggestions ? suggestions->set : NULL;
}

size_t fill_suggestions_count(const FillSuggestions *suggestions) {
    return suggestions ? suggestions->count : 0;
}

FillTarget fill_suggestions_target(const FillSuggestions *suggestions) {
    FillTarget none = {FILL_TARGET_NODE, 0};
    return suggestions ? suggestions->target : none;
}

const char *fill_suggestion_operator(const FillSuggestions *suggestions, size_t index) {
    if (!suggestions || index >= suggestions->count) {
        return NULL;
    }
    size_t operator_index = suggestions->entries.items[suggestions->ranked[index]].operator_index;
    return fill_registry_at(suggestions->registry, operator_index)->generator.name;
}

const char *fill_suggestion_label(const FillSuggestions *suggestions, size_t index) {
    if (!suggestions || index >= suggestions->count) {
        return NULL;
    }
    return suggestions->entries.items[suggestions->ranked[index]].candidate.label;
}

static const char *target_word(FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? "node" : "lane-span";
}

/* Builds the Ursatz CandidateSet for the ranked entries. */
static FillResult build_set(FillSuggestions *suggestions, unsigned long seed) {
    size_t count = suggestions->count;
    Candidate **candidates = count ? calloc(count, sizeof(Candidate *)) : NULL;
    if (count && !candidates) {
        return FILL_ERR_ALLOC;
    }
    char text[96];
    bool ok = true;
    for (size_t k = 0; ok && k < count; k++) {
        const FillCheck *check = &suggestions->entries.items[suggestions->ranked[k]].check;
        EntityID *id = NULL;
        EntityID *material = NULL;
        ProvenanceReference *provenance = NULL;
        snprintf(text, sizeof(text), "composer-suggestion-%s-%zu-%zu", target_word(suggestions->target),
                 suggestions->target.id, k);
        ok = entity_id_create(text, &id) == ENTITY_ID_OK;
        snprintf(text, sizeof(text), "composer-candidate-%s-%zu-%zu", target_word(suggestions->target),
                 suggestions->target.id, k);
        ok = ok && entity_id_create(text, &material) == ENTITY_ID_OK &&
             provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK &&
             candidate_create(id, material, NULL, 0, NULL, 0, check->score_numerator, check->score_denominator,
                              provenance, &candidates[k]) == CANDIDATE_OK;
        if (!ok) {
            entity_id_destroy(id);
            entity_id_destroy(material);
            provenance_reference_destroy(provenance);
        }
    }

    EntityID *set_id = NULL;
    EntityID *input_id = NULL;
    EntityID *inputs[1];
    ProvenanceReference *provenance = NULL;
    CandidateSet *set = NULL;
    if (ok) {
        snprintf(text, sizeof(text), "composer-suggestions-%s-%zu", target_word(suggestions->target),
                 suggestions->target.id);
        ok = entity_id_create(text, &set_id) == ENTITY_ID_OK;
        snprintf(text, sizeof(text), "composer-%s-%zu", target_word(suggestions->target), suggestions->target.id);
        ok = ok && entity_id_create(text, &input_id) == ENTITY_ID_OK &&
             provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;
        inputs[0] = input_id;
        ok = ok && candidate_set_create(set_id, candidates, count, seed, composer_version(), NULL, inputs, 1, NULL,
                                        provenance, &set) == CANDIDATE_SET_OK;
    }
    if (!ok) {
        for (size_t k = 0; k < count; k++) {
            candidate_destroy(candidates[k]);
        }
        entity_id_destroy(set_id);
        entity_id_destroy(input_id);
        provenance_reference_destroy(provenance);
        free(candidates);
        return FILL_ERR_ALLOC;
    }
    free(candidates);
    suggestions->set = set;
    return FILL_OK;
}

FillResult fill_suggest(const PiecePlan *plan, const FillOperatorRegistry *registry, const FillConfig *config,
                        FillTarget target, size_t top_n, FillSuggestions **out) {
    if (!plan || !registry || !out) {
        return FILL_ERR_NULL;
    }
    *out = NULL;
    if (!fill_target_exists(plan, target) || fill_target_state(plan, target) != PLAN_STATE_CONSTRAINED) {
        return FILL_ERR_TARGET_NOT_OPEN;
    }
    FillSuggestions *suggestions = calloc(1, sizeof(FillSuggestions));
    if (!suggestions) {
        return FILL_ERR_ALLOC;
    }
    suggestions->target = target;
    suggestions->registry = registry;

    unsigned long seed = config ? config->seed : 0;
    FillRng rng;
    fill_rng_seed(&rng, seed);
    FillResult result = fill_gather(plan, registry, config, target, &rng, &suggestions->entries);
    if (result != FILL_OK) {
        free(suggestions);
        return result;
    }

    /* Rank the entries that were not rejected, best first; ties keep the order proposed. */
    const FillEntries *entries = &suggestions->entries;
    suggestions->ranked = entries->count ? calloc(entries->count, sizeof(size_t)) : NULL;
    if (entries->count && !suggestions->ranked) {
        fill_suggestions_destroy(suggestions);
        return FILL_ERR_ALLOC;
    }
    size_t ranked = 0;
    for (size_t i = 0; i < entries->count; i++) {
        if (entries->items[i].check.hard_violation) {
            continue;
        }
        size_t position = ranked++;
        while (position > 0 &&
               fill_check_scores_higher(&entries->items[i].check, &entries->items[suggestions->ranked[position - 1]].check)) {
            suggestions->ranked[position] = suggestions->ranked[position - 1];
            position--;
        }
        suggestions->ranked[position] = i;
    }
    suggestions->count = ranked < top_n ? ranked : top_n;

    result = build_set(suggestions, seed);
    if (result != FILL_OK) {
        fill_suggestions_destroy(suggestions);
        return result;
    }
    *out = suggestions;
    return FILL_OK;
}

FillResult fill_apply_suggestion(PiecePlan *plan, const FillSuggestions *suggestions, size_t index) {
    if (!plan || !suggestions || index >= suggestions->count) {
        return FILL_ERR_NULL;
    }
    if (!fill_target_exists(plan, suggestions->target) ||
        fill_target_state(plan, suggestions->target) != PLAN_STATE_CONSTRAINED) {
        return FILL_ERR_TARGET_NOT_OPEN;
    }
    return fill_apply_entry(plan, suggestions->registry, suggestions->target,
                            &suggestions->entries.items[suggestions->ranked[index]]);
}
