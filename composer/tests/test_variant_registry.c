#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/variant_registry.h>

/*
 * The variant registry and chains: the 9 development operations with their categories and citations, chains applied
 * from a seed (the same chain and seed give the same variant, open parameters are drawn and recorded), and what each
 * variant kept and changed.
 */

static Motive load(const char *notes, const char *meter, const char *key, MotiveFormat format) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = motive_parse_meter(meter, &options.meter);
    options.has_key = key && motive_parse_key(key, &options.key);
    assert(options.has_meter);
    Motive motive;
    MotiveError error;
    MotiveResult result = motive_load(notes, strlen(notes), format, &options, &motive, &error);
    assert(result == MOTIVE_OK);
    return motive;
}

static Motive source_motive(void) {
    return load("D4:q F4:e A4:e D5:h", "4/4", "D minor", MOTIVE_FORMAT_NOTES);
}

static VariantStep step_of(const char *op) {
    VariantStep step;
    bool ok = variant_step_init(&step, op);
    assert(ok);
    return step;
}

static VariantChain one_step(VariantStep step) {
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.steps[0] = step;
    chain.step_count = 1;
    return chain;
}

/* ------------------------------------------------------------------ registry */

typedef struct {
    const char *name;
    VariantCategory category;
    const char *citation;
} Entry;

static void test_registry_entries(void) {
    static const Entry ENTRIES[] = {
        {"exact", VARIANT_CATEGORY_EXACT, "Schoenberg ch. III, Ex. 14"},
        {"transpose", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Exs. 14, 28"},
        {"invert", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 14, 19"},
        {"retrograde", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 14, 19"},
        {"interval-change", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21"},
        {"fragment", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21; ch. VIII, Ex. 52"},
        {"liquidate", VARIANT_CATEGORY_INTERVALS, "ch. VIII, Ex. 52"},
        {"augment", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 14, 17"},
        {"diminish", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 14, 17"}};
    assert(variant_registry_count() == 9);
    for (size_t i = 0; i < 9; i++) {
        const VariantOperation *operation = variant_registry_at(i);
        assert(operation && strcmp(operation->name, ENTRIES[i].name) == 0);
        assert(variant_registry_find(ENTRIES[i].name) == operation);
        assert(operation->category == ENTRIES[i].category);
        assert(strcmp(operation->citation, ENTRIES[i].citation) == 0);
        assert(operation->label && operation->apply && operation->describe);
        assert((operation->forms & VARIANT_FORM_BIT(MOTIVE_FORM_FULL)) != 0);
        assert(operation->param_count <= VARIANT_PARAM_MAX);
    }
    assert(variant_registry_at(9) == NULL);
    assert(variant_registry_find("nope") == NULL);
    assert(strcmp(variant_category_name(VARIANT_CATEGORY_MELODY_ADAPTATION), "melody-adaptation") == 0);
    assert(strcmp(variant_category_name(VARIANT_CATEGORY_EXACT), "exact") == 0);
}

static void test_registry_forms(void) {
    /* A rhythm motive takes exact, retrograde, fragment, liquidate, augment, diminish; a contour motive exact,
       invert, retrograde, fragment. */
    static const char *const RHYTHM[] = {"exact", "retrograde", "fragment", "liquidate", "augment", "diminish"};
    static const char *const CONTOUR[] = {"exact", "invert", "retrograde", "fragment"};
    size_t rhythm = 0;
    size_t contour = 0;
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        rhythm += (operation->forms & VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM)) != 0;
        contour += (operation->forms & VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR)) != 0;
    }
    assert(rhythm == 6 && contour == 4);
    for (size_t i = 0; i < 6; i++) {
        assert((variant_registry_find(RHYTHM[i])->forms & VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM)) != 0);
    }
    for (size_t i = 0; i < 4; i++) {
        assert((variant_registry_find(CONTOUR[i])->forms & VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR)) != 0);
    }
}

static void test_step_parameters(void) {
    VariantStep step = step_of("transpose");
    assert(!variant_step_init(&step, "nope"));
    step = step_of("transpose");
    assert(variant_step_set(&step, "by", 3, 0));
    assert(variant_step_set(&step, "tonal", 1, 0));
    assert(!variant_step_set(&step, "tonal", 2, 0));
    assert(!variant_step_set(&step, "by", 500, 0));
    assert(!variant_step_set(&step, "nope", 1, 0));
    assert(step.values[1].set && step.values[1].value == 3);
    assert(!step.values[2].set);
    step = step_of("augment");
    assert(variant_step_set(&step, "ratio", 3, 2));
    assert(step.values[0].value == 3 && step.values[0].den == 2);
    assert(!variant_step_set(&step, "ratio", 9, 2));
}

/* -------------------------------------------------------------------- chains */

static void test_same_seed_same_result(void) {
    Motive source = source_motive();
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.steps[0] = step_of("invert");
    chain.steps[1] = step_of("augment");
    chain.step_count = 2;
    size_t different = 0;
    bool have_first = false;
    Motive first;
    for (unsigned long seed = 0; seed < 12; seed++) {
        Motive a;
        Motive b;
        VariantRecord ra;
        VariantRecord rb;
        VariantResult ea = variant_apply_chain(&source, &chain, seed, &a, &ra);
        VariantResult eb = variant_apply_chain(&source, &chain, seed, &b, &rb);
        assert(ea == eb);
        if (ea != VARIANT_OK) {
            continue;
        }
        assert(motive_equal(&a, &b));
        assert(strcmp(ra.derivation, rb.derivation) == 0);
        assert(ra.step_count == 2 && ra.seed == seed);
        for (size_t s = 0; s < 2; s++) {
            for (size_t p = 0; p < ra.steps[s].param_count; p++) {
                assert(ra.steps[s].values[p].set == rb.steps[s].values[p].set);
                assert(ra.steps[s].values[p].value == rb.steps[s].values[p].value);
            }
        }
        if (!have_first) {
            first = a;
            have_first = true;
        } else if (!motive_equal(&first, &a)) {
            different++;
        }
    }
    /* Another seed is another variant. */
    assert(have_first && different >= 1);
}

static void test_open_parameters_recorded(void) {
    Motive source = source_motive();
    VariantStep step = step_of("invert");
    assert(variant_step_set(&step, "tonal", 1, 0));
    VariantChain chain = one_step(step);
    Motive out;
    VariantRecord record;
    assert(variant_apply_chain(&source, &chain, 7, &out, &record) == VARIANT_OK);
    assert(record.step_count == 1 && strcmp(record.steps[0].op, "invert") == 0);
    /* axis is open and drawn from the motive's pitches; tonal was set and is not open */
    assert(record.steps[0].open[0] && record.steps[0].values[0].set);
    int axis = (int)record.steps[0].values[0].value;
    bool is_pitch = false;
    for (size_t i = 0; i < source.note_count; i++) {
        is_pitch = is_pitch || source.notes[i].midi_pitch == axis;
    }
    assert(is_pitch);
    assert(!record.steps[0].open[1] && record.steps[0].values[1].set && record.steps[0].values[1].value == 1);

    /* a step with everything set has nothing open */
    VariantStep fixed = step_of("augment");
    assert(variant_step_set(&fixed, "ratio", 2, 1));
    chain = one_step(fixed);
    assert(variant_apply_chain(&source, &chain, 3, &out, &record) == VARIANT_OK);
    assert(!record.steps[0].open[0] && record.steps[0].values[0].value == 2 && record.steps[0].values[0].den == 1);

    /* a step with an open ratio draws it */
    chain = one_step(step_of("augment"));
    bool drawn = false;
    for (unsigned long seed = 0; seed < 8 && !drawn; seed++) {
        drawn = variant_apply_chain(&source, &chain, seed, &out, &record) == VARIANT_OK && record.steps[0].open[0] &&
                record.steps[0].values[0].set && record.steps[0].values[0].value > record.steps[0].values[0].den;
    }
    assert(drawn);
}

static void test_fixed_chain_result(void) {
    Motive source = source_motive();
    VariantStep step = step_of("retrograde");
    VariantChain chain = one_step(step);
    Motive out;
    VariantRecord record;
    assert(variant_apply_chain(&source, &chain, 0, &out, &record) == VARIANT_OK);
    assert(out.note_count == 4 && out.notes[0].midi_pitch == 74 && out.notes[3].midi_pitch == 62);
    assert(source.notes[0].midi_pitch == 62);
}

static void test_refused_steps(void) {
    Motive contour = load("0 2 1 3", "4/4", NULL, MOTIVE_FORMAT_CONTOUR);
    VariantChain chain = one_step(step_of("interval-change"));
    Motive out;
    VariantRecord record;
    assert(variant_apply_chain(&contour, &chain, 1, &out, &record) == VARIANT_ERR_FORM);
    assert(record.failed_step == 0);
    assert(strcmp(record.error, "interval-change does not apply to a contour motive") == 0);

    Motive source = source_motive();
    memset(&chain, 0, sizeof(chain));
    chain.steps[0] = step_of("retrograde");
    chain.steps[1] = step_of("liquidate");
    memcpy(chain.steps[1].op, "nope", 5);
    chain.step_count = 2;
    assert(variant_apply_chain(&source, &chain, 1, &out, &record) == VARIANT_ERR_UNKNOWN_OP);
    assert(record.failed_step == 1 && strcmp(record.error, "unknown operation 'nope'") == 0);

    chain.step_count = 0;
    assert(variant_apply_chain(&source, &chain, 1, &out, &record) == VARIANT_ERR_EMPTY);
    assert(variant_apply_chain(NULL, &chain, 1, &out, &record) == VARIANT_ERR_NULL);

    /* a step refused whatever is drawn: a fragment as long as the whole motive */
    VariantStep fragment = step_of("fragment");
    assert(variant_step_set(&fragment, "length", 4, 0));
    assert(variant_step_set(&fragment, "position", 1, 0));
    chain = one_step(fragment);
    assert(variant_apply_chain(&source, &chain, 1, &out, &record) == VARIANT_ERR_REFUSED);
    assert(record.failed_step == 0 && strncmp(record.error, "fragment: ", 10) == 0);
}

int main(void) {
    test_registry_entries();
    test_registry_forms();
    test_step_parameters();
    test_same_seed_same_result();
    test_open_parameters_recorded();
    test_fixed_chain_result();
    test_refused_steps();
    puts("test_variant_registry: ok");
    return 0;
}
