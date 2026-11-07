#include <string.h>

#include <ursatz_composer/variant_registry.h>

#include "grow/grow_internal.h"

/* The seed an open parameter of a developed motive is drawn from: the same motive and ref always give the same result. */
#define DEVELOP_SEED 1ul

static void set_value(VariantValue *value, long number, long den) {
    value->set = true;
    value->value = number;
    value->den = den;
}

/* The registry row that applies the relation, or NULL when the registry has none. */
static const VariantOperation *row_of(MotiveRelationKind kind) {
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *row = variant_registry_at(i);
        if (row && row->relation == (int)kind) {
            return row;
        }
    }
    return NULL;
}

/* Sets the step's parameters from the relation step. A parameter the plan leaves at its default stays open. */
static void set_values(const VariantOperation *row, const MotiveRelationStep *relation, VariantStep *step) {
    size_t count = row->param_count < VARIANT_PARAM_MAX ? row->param_count : VARIANT_PARAM_MAX;
    switch (relation->kind) {
    case MOTIVE_RELATION_TRANSPOSE:
        set_value(&step->values[0], relation->value[0] != 0 ? 1 : 0, 0);
        if (relation->value[1] != 0) {
            set_value(&step->values[1], relation->value[1], 0);
        }
        return;
    case MOTIVE_RELATION_INVERT:
        if (relation->value[0] >= 0) {
            set_value(&step->values[0], relation->value[0], 0);
        }
        set_value(&step->values[1], relation->value[1] != 0 ? 1 : 0, 0);
        return;
    case MOTIVE_RELATION_INTERVAL_CHANGE:
        set_value(&step->values[0], relation->value[0], 0);
        if (relation->value[1] != 0) {
            set_value(&step->values[1], relation->value[1], 0);
        }
        set_value(&step->values[2], relation->value[2] != 0 ? 1 : 0, 0);
        return;
    case MOTIVE_RELATION_LIQUIDATE:
        if (relation->value[0] != 0) {
            set_value(&step->values[0], relation->value[0], 0);
        }
        set_value(&step->values[1], relation->value[1] != 0 ? 1 : 0, 0);
        if (relation->value[2] != 0) {
            set_value(&step->values[2], relation->value[2], 0);
        }
        return;
    default:
        for (size_t i = 0; i < count; i++) {
            if (relation->value[i] != 0 || relation->den[i] != 0) {
                set_value(&step->values[i], relation->value[i], relation->den[i]);
            }
        }
        return;
    }
}

/* The chain a liquidation by features of `steps` steps is: each liquidates by one feature and shortens by one. */
static bool liquidation_chain(size_t steps, VariantChain *chain) {
    if (steps == 0 || steps > VARIANT_CHAIN_MAX) {
        return false;
    }
    for (size_t t = 0; t < steps; t++) {
        VariantStep *step = &chain->steps[t];
        if (!variant_step_init(step, "liquidate-features") || !variant_step_set(step, "steps", 1, 0) ||
            !variant_step_set(step, "shorten", 1, 0)) {
            return false;
        }
    }
    chain->step_count = steps;
    return true;
}

bool grow_develop_motive(const Motive *source, const PlanMotiveRef *ref, Motive *out) {
    if (!source || !ref || !out) {
        return false;
    }
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    if (ref->has_step && ref->step.kind == MOTIVE_RELATION_LIQUIDATE && ref->steps > 0) {
        if (!liquidation_chain(ref->steps, &chain)) {
            return false;
        }
    } else {
        MotiveRelationKind kind = ref->has_step ? ref->step.kind : MOTIVE_RELATION_EXACT;
        const VariantOperation *row = row_of(kind);
        if (!row || !variant_step_init(&chain.steps[0], row->name)) {
            return false;
        }
        if (ref->has_step) {
            set_values(row, &ref->step, &chain.steps[0]);
        }
        chain.step_count = 1;
    }
    return variant_apply_chain(source, &chain, DEVELOP_SEED, out, NULL) == VARIANT_OK;
}
