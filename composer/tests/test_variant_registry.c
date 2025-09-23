#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/variant_registry.h>

#include "analysis/motive_relation.h"

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
    assert(variant_registry_count() == 27);
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
    assert(variant_registry_at(27) == NULL);
    assert(variant_registry_find("nope") == NULL);
    assert(strcmp(variant_category_name(VARIANT_CATEGORY_MELODY_ADAPTATION), "melody-adaptation") == 0);
    assert(strcmp(variant_category_name(VARIANT_CATEGORY_EXACT), "exact") == 0);
}

static void test_registry_forms(void) {
    /* A rhythm motive takes exact, retrograde, fragment, liquidate, augment, diminish, the rhythm rows and condense; a
       contour motive exact, invert, retrograde, fragment, add-interval, omit-interval and repeat-feature. */
    static const char *const RHYTHM[] = {"exact",       "retrograde",   "fragment",     "liquidate",
                                         "augment",     "diminish",     "note-repeat",  "rhythm-repeat",
                                         "rhythm-shift", "add-upbeat",  "change-meter", "condense"};
    static const char *const CONTOUR[] = {"exact",        "invert",        "retrograde",     "fragment",
                                          "add-interval", "omit-interval", "repeat-feature"};
    size_t rhythm = 0;
    size_t contour = 0;
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        rhythm += (operation->forms & VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM)) != 0;
        contour += (operation->forms & VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR)) != 0;
    }
    assert(rhythm == 12 && contour == 7);
    for (size_t i = 0; i < 12; i++) {
        assert((variant_registry_find(RHYTHM[i])->forms & VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM)) != 0);
    }
    for (size_t i = 0; i < 7; i++) {
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

/* ----------------------------------------------------- kept, changed and kind */

static bool listed(const char *const *names, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static bool kept(const VariantRecord *record, const char *name) {
    return listed(record->kept, record->kept_count, name);
}

static bool changed(const VariantRecord *record, const char *name) {
    return listed(record->changed, record->changed_count, name);
}

static VariantResult run_one(const Motive *source, VariantStep step, unsigned long seed, Motive *out,
                             VariantRecord *record) {
    VariantChain chain = one_step(step);
    return variant_apply_chain(source, &chain, seed, out, record);
}

static void test_transpose_tonal_is_exact(void) {
    Motive source = source_motive();
    VariantStep step = step_of("transpose");
    assert(variant_step_set(&step, "tonal", 1, 0));
    assert(variant_step_set(&step, "by", 7, 0));
    Motive out;
    VariantRecord record;
    assert(run_one(&source, step, 0, &out, &record) == VARIANT_OK);
    assert(out.notes[0].midi_pitch == 74);
    assert(record.kind == VARIANT_KIND_EXACT && record.changed_count == 0);
    assert(kept(&record, "rhythm") && kept(&record, "intervals") && kept(&record, "contour") && kept(&record, "harmony"));
    assert(!record.rhythm_scaled);
    assert(strcmp(variant_kind_name(record.kind), "exact") == 0);
}

static void test_invert_changes_contour(void) {
    Motive source = source_motive();
    VariantStep step = step_of("invert");
    assert(variant_step_set(&step, "tonal", 1, 0));
    assert(variant_step_set(&step, "axis", 62, 0));
    Motive out;
    VariantRecord record;
    assert(run_one(&source, step, 0, &out, &record) == VARIANT_OK);
    assert(record.kind == VARIANT_KIND_MOTIVE_FORM);
    assert(changed(&record, "contour") && changed(&record, "intervals"));
    assert(kept(&record, "rhythm") && !record.rhythm_scaled);
    assert(strcmp(variant_kind_name(record.kind), "motive-form") == 0);
    assert(record.recognizability.denominator >= 1 && record.recognizability.numerator <= record.recognizability.denominator);
}

static void test_own_harmony_in_kept_and_changed(void) {
    Motive source = source_motive();
    MotiveError error;
    assert(motive_harmony_parse("I V@2", &source.meter, source.harmony, &source.harmony_count, &error) == MOTIVE_OK);
    Motive out;
    VariantRecord record;

    /* Moving the pitches leaves the chords where they were: kept, whatever chord the new notes imply. */
    VariantStep step = step_of("transpose");
    assert(variant_step_set(&step, "by", 2, 0));
    assert(run_one(&source, step, 0, &out, &record) == VARIANT_OK);
    assert(out.harmony_count == 2 && out.harmony[1].degree == 5);
    assert(kept(&record, "harmony") && !changed(&record, "harmony"));

    /* Scaled in time the chords move with the notes, and their onsets are no longer the source's. */
    step = step_of("augment");
    assert(variant_step_set(&step, "ratio", 2, 1));
    assert(run_one(&source, step, 0, &out, &record) == VARIANT_OK);
    assert(out.harmony_count == 2 && out.harmony[1].onset.num == 1 && out.harmony[1].onset.den == 1);
    assert(changed(&record, "harmony") && !kept(&record, "harmony"));

    /* A cut that loses a chord changes the harmony. */
    step = step_of("fragment");
    assert(variant_step_set(&step, "position", 1, 0));
    assert(variant_step_set(&step, "length", 2, 0));
    assert(run_one(&source, step, 0, &out, &record) == VARIANT_OK);
    assert(out.harmony_count == 1 && changed(&record, "harmony"));
}

static void test_augment_keeps_rhythm_proportions(void) {
    Motive source = source_motive();
    VariantStep step = step_of("augment");
    assert(variant_step_set(&step, "ratio", 2, 1));
    Motive out;
    VariantRecord record;
    assert(run_one(&source, step, 0, &out, &record) == VARIANT_OK);
    assert(kept(&record, "rhythm") && record.rhythm_scaled);
    assert(kept(&record, "intervals") && kept(&record, "contour"));
    assert(record.kind == VARIANT_KIND_EXACT || changed(&record, "harmony"));

    /* a fragment is a different rhythm and different intervals */
    VariantStep fragment = step_of("fragment");
    assert(variant_step_set(&fragment, "position", 1, 0));
    assert(variant_step_set(&fragment, "length", 3, 0));
    assert(run_one(&source, fragment, 0, &out, &record) == VARIANT_OK);
    assert(changed(&record, "rhythm") && changed(&record, "intervals"));
    assert(record.kind == VARIANT_KIND_MOTIVE_FORM);
}

static void test_forms_have_only_their_dimensions(void) {
    Motive rhythm = load("x:q x:e x:e x:h", "4/4", NULL, MOTIVE_FORMAT_RHYTHM);
    VariantStep step = step_of("augment");
    assert(variant_step_set(&step, "ratio", 2, 1));
    Motive out;
    VariantRecord record;
    assert(run_one(&rhythm, step, 0, &out, &record) == VARIANT_OK);
    assert(record.kept_count == 1 && kept(&record, "rhythm") && record.changed_count == 0 && record.rhythm_scaled);
    assert(out.form == MOTIVE_FORM_RHYTHM);

    Motive contour = load("0 2 1 3", "4/4", NULL, MOTIVE_FORMAT_CONTOUR);
    assert(run_one(&contour, step_of("invert"), 2, &out, &record) == VARIANT_OK);
    assert(record.kept_count + record.changed_count == 1);
    assert(changed(&record, "contour") || kept(&record, "contour"));
    assert(out.form == MOTIVE_FORM_CONTOUR);
}

static void test_derivation_sentence(void) {
    Motive source = source_motive();
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.steps[0] = step_of("invert");
    assert(variant_step_set(&chain.steps[0], "axis", 62, 0));
    assert(variant_step_set(&chain.steps[0], "tonal", 1, 0));
    chain.steps[1] = step_of("augment");
    assert(variant_step_set(&chain.steps[1], "ratio", 2, 1));
    chain.step_count = 2;
    Motive out;
    VariantRecord record;
    assert(variant_apply_chain(&source, &chain, 0, &out, &record) == VARIANT_OK);
    static const char EXPECTED[] = "Inverted about D4 in the key's scale, then augmented 2:1. Kept the rhythm's proportions";
    assert(strncmp(record.derivation, EXPECTED, strlen(EXPECTED)) == 0);
    assert(strstr(record.derivation, "changed ") != NULL);
    assert(strstr(record.derivation, "the intervals") != NULL && strstr(record.derivation, "the contour") != NULL);
    assert(record.derivation[strlen(record.derivation) - 1] == '.');
}

/* ------------------------------------------------------- random chains, JSON */

/* The source with a labelled feature on its last two notes. */
static Motive featured_source(void) {
    Motive source = source_motive();
    snprintf(source.features[0].label, sizeof(source.features[0].label), "a");
    source.features[0].first = 2;
    source.features[0].last = 3;
    source.feature_count = 1;
    return source;
}

static void test_random_chains(void) {
    Motive source = source_motive();
    Motive rhythm = load("x:q x:e x:e x:h", "4/4", NULL, MOTIVE_FORMAT_RHYTHM);
    Motive contour = load("0 2 1 3", "4/4", NULL, MOTIVE_FORMAT_CONTOUR);
    const Motive *sources[] = {&source, &rhythm, &contour};
    bool two_steps = false;
    bool one_step_seen = false;
    for (size_t m = 0; m < 3; m++) {
        for (unsigned long seed = 0; seed < 200; seed++) {
            VariantChain a;
            VariantChain b;
            assert(variant_random_chain(sources[m], seed, &a) && variant_random_chain(sources[m], seed, &b));
            assert(a.step_count >= 1 && a.step_count <= 2 && a.step_count == b.step_count);
            two_steps = two_steps || a.step_count == 2;
            one_step_seen = one_step_seen || a.step_count == 1;
            for (size_t s = 0; s < a.step_count; s++) {
                const VariantOperation *operation = variant_registry_find(a.steps[s].op);
                assert(operation && strcmp(a.steps[s].op, b.steps[s].op) == 0);
                assert(strcmp(operation->name, "exact") != 0);
                assert(strcmp(operation->name, "change-meter") != 0);
                assert((operation->forms & VARIANT_FORM_BIT(sources[m]->form)) != 0);
                for (size_t p = 0; p < VARIANT_PARAM_MAX; p++) {
                    assert(!a.steps[s].values[p].set);
                }
                assert(s == 0 || strcmp(a.steps[s].op, a.steps[0].op) != 0);
            }
        }
    }
    assert(two_steps && one_step_seen);

    /* A feature operation is only drawn for a motive that has a labelled feature. */
    Motive featured = featured_source();
    bool feature_drawn = false;
    for (unsigned long seed = 0; seed < 200; seed++) {
        VariantChain chain;
        assert(variant_random_chain(&source, seed, &chain));
        for (size_t s = 0; s < chain.step_count; s++) {
            assert(strcmp(chain.steps[s].op, "repeat-feature") != 0 && strcmp(chain.steps[s].op, "shift-feature") != 0);
        }
        assert(variant_random_chain(&featured, seed, &chain));
        for (size_t s = 0; s < chain.step_count; s++) {
            feature_drawn = feature_drawn || strcmp(chain.steps[s].op, "repeat-feature") == 0 ||
                            strcmp(chain.steps[s].op, "shift-feature") == 0;
        }
    }
    assert(feature_drawn);
}

static void test_json(void) {
    char *registry = variant_registry_to_json();
    assert(registry);
    assert(strstr(registry, "\"operations\"") && strstr(registry, "\"interval-change\""));
    assert(strstr(registry, "\"melody-adaptation\"") && strstr(registry, "ch. VIII, Ex. 52"));
    assert(strstr(registry, "\"keep-contour\"") && strstr(registry, "\"forms\""));
    assert(strstr(registry, "\"relation\": \"interval-change\"") && strstr(registry, "\"relation\": \"note-repeat\""));
    free(registry);

    Motive source = source_motive();
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.steps[0] = step_of("invert");
    assert(variant_step_set(&chain.steps[0], "tonal", 1, 0));
    chain.steps[1] = step_of("augment");
    chain.step_count = 2;
    Motive out;
    VariantRecord record;
    assert(variant_apply_chain(&source, &chain, 5, &out, &record) == VARIANT_OK);
    char *json = variant_record_to_json(&record);
    assert(json);
    assert(strstr(json, "\"seed\": 5") && strstr(json, "\"op\": \"invert\"") && strstr(json, "\"tonal\": true"));
    assert(strstr(json, "\"open\": [") && strstr(json, "\"axis\"") && strstr(json, "\"ratio\""));
    assert(strstr(json, "\"kind\"") && strstr(json, "\"functions\": []") && strstr(json, "\"derivation\""));
    assert(strstr(json, "\"recognizability\""));
    free(json);
    assert(variant_record_to_json(NULL) == NULL);
}

/* ------------------------------------------------ rhythm and interval rows */

static void test_rows_name_their_relations(void) {
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        assert(operation->relation >= 0 && operation->relation < MOTIVE_RELATION_KIND_COUNT);
        assert(strcmp(motive_relation_name((MotiveRelationKind)operation->relation), operation->name) == 0);
        assert(operation->param_count == motive_relation_param_count((MotiveRelationKind)operation->relation));
    }
}

typedef struct {
    const char *name;
    long value;
    long den;
} Setting;

typedef struct {
    const char *op;
    const char *citation;
    VariantCategory category;
    Setting settings[3];
    size_t setting_count;
    size_t notes; /* the variant's note count */
} Row;

static const Row ROWS[] = {
    {"note-repeat", "ch. III, Exs. 17h, i, k, l, n", VARIANT_CATEGORY_RHYTHM, {{"note", 2, 0}, {"count", 2, 0}}, 2, 5},
    {"rhythm-repeat", "ch. III, Exs. 17l, m, 18e", VARIANT_CATEGORY_RHYTHM,
     {{"first", 0, 0}, {"length", 1, 0}, {"target", 3, 0}}, 3, 4},
    {"rhythm-shift", "ch. III, Ex. 23", VARIANT_CATEGORY_RHYTHM, {{"offset", 1, 4}}, 1, 4},
    {"add-upbeat", "ch. III, Ex. 22", VARIANT_CATEGORY_RHYTHM, {{"count", 2, 0}, {"approach", 1, 0}}, 2, 6},
    {"change-meter", "ch. III, Ex. 24", VARIANT_CATEGORY_RHYTHM, {{"num", 3, 0}, {"den", 4, 0}}, 2, 4},
    {"add-interval", "ch. III, Ex. 21", VARIANT_CATEGORY_INTERVALS, {{"interval", 1, 0}, {"steps", 1, 0}}, 2, 5},
    {"omit-interval", "ch. III, Ex. 21", VARIANT_CATEGORY_INTERVALS, {{"interval", 2, 0}}, 1, 3},
    {"ancillary-fill", "ch. III, Exs. 18, 20", VARIANT_CATEGORY_INTERVALS, {{"interval", 3, 0}, {"kind", 0, 0}}, 2, 6},
    {"condense", "ch. III, Ex. 21; ch. VIII, Ex. 52", VARIANT_CATEGORY_INTERVALS,
     {{"first", 1, 0}, {"last", 3, 0}, {"ratio", 1, 2}}, 3, 4},
    {"repeat-feature", "ch. III, Exs. 20h, 22a, b, d", VARIANT_CATEGORY_INTERVALS,
     {{"feature", 0, 0}, {"times", 1, 0}, {"steps", 1, 0}}, 3, 6},
    {"shift-feature", "ch. III, Ex. 23", VARIANT_CATEGORY_INTERVALS, {{"feature", 0, 0}, {"offset", 1, 4}}, 2, 4}};

#define ROW_COUNT (sizeof(ROWS) / sizeof(ROWS[0]))

static void test_each_row_applies(void) {
    Motive source = featured_source();
    Motive before = source;
    for (size_t i = 0; i < ROW_COUNT; i++) {
        const Row *row = &ROWS[i];
        const VariantOperation *operation = variant_registry_find(row->op);
        assert(operation && operation->category == row->category);
        assert(strcmp(operation->citation, row->citation) == 0);
        assert(operation->label && operation->describe && operation->apply);
        VariantStep step = step_of(row->op);
        for (size_t k = 0; k < row->setting_count; k++) {
            assert(variant_step_set(&step, row->settings[k].name, row->settings[k].value, row->settings[k].den));
        }
        Motive out;
        VariantRecord record;
        assert(run_one(&source, step, 1, &out, &record) == VARIANT_OK);
        assert(out.note_count == row->notes);
        assert(!motive_equal(&source, &out));
        assert(motive_equal(&source, &before));
        assert(record.step_count == 1 && record.derivation[0] >= 'A' && record.derivation[0] <= 'Z');
        for (size_t p = 0; p < operation->param_count; p++) {
            assert(!record.steps[0].open[p]);
        }
    }
}

static void test_rows_draw_open_parameters(void) {
    Motive source = featured_source();
    for (size_t i = 0; i < ROW_COUNT; i++) {
        unsigned ok = 0;
        for (unsigned long seed = 0; seed < 40; seed++) {
            Motive out;
            VariantRecord record;
            if (run_one(&source, step_of(ROWS[i].op), seed, &out, &record) == VARIANT_OK) {
                ok++;
                for (size_t p = 0; p < record.steps[0].param_count; p++) {
                    assert(record.steps[0].values[p].set);
                }
            }
        }
        assert(ok > 0);
    }
}

static void test_row_forms(void) {
    Motive rhythm = load("x:q x:e x:e x:h", "4/4", NULL, MOTIVE_FORMAT_RHYTHM);
    Motive contour = load("0 2 1 3", "4/4", NULL, MOTIVE_FORMAT_CONTOUR);
    Motive out;
    VariantRecord record;

    /* A rhythm motive takes the rhythm rows and condense; a contour motive add-interval, omit-interval and
       repeat-feature. */
    static const char *const RHYTHM_OK[] = {"note-repeat", "rhythm-repeat", "rhythm-shift",
                                            "add-upbeat",  "change-meter",  "condense"};
    static const char *const RHYTHM_NO[] = {"add-interval", "omit-interval", "ancillary-fill", "repeat-feature",
                                            "shift-feature"};
    static const char *const CONTOUR_OK[] = {"add-interval", "omit-interval", "repeat-feature"};
    static const char *const CONTOUR_NO[] = {"note-repeat", "rhythm-repeat", "rhythm-shift", "add-upbeat",
                                             "change-meter", "ancillary-fill", "condense", "shift-feature"};
    for (size_t i = 0; i < sizeof(RHYTHM_OK) / sizeof(RHYTHM_OK[0]); i++) {
        assert((variant_registry_find(RHYTHM_OK[i])->forms & VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM)) != 0);
    }
    for (size_t i = 0; i < sizeof(RHYTHM_NO) / sizeof(RHYTHM_NO[0]); i++) {
        assert(run_one(&rhythm, step_of(RHYTHM_NO[i]), 1, &out, &record) == VARIANT_ERR_FORM);
    }
    for (size_t i = 0; i < sizeof(CONTOUR_OK) / sizeof(CONTOUR_OK[0]); i++) {
        assert((variant_registry_find(CONTOUR_OK[i])->forms & VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR)) != 0);
    }
    for (size_t i = 0; i < sizeof(CONTOUR_NO) / sizeof(CONTOUR_NO[0]); i++) {
        assert(run_one(&contour, step_of(CONTOUR_NO[i]), 1, &out, &record) == VARIANT_ERR_FORM);
    }
    for (size_t i = 0; i < ROW_COUNT; i++) {
        assert((variant_registry_find(ROWS[i].op)->forms & VARIANT_FORM_BIT(MOTIVE_FORM_FULL)) != 0);
    }

    VariantStep repeat = step_of("note-repeat");
    assert(variant_step_set(&repeat, "note", 1, 0) && variant_step_set(&repeat, "count", 2, 0));
    assert(run_one(&rhythm, repeat, 1, &out, &record) == VARIANT_OK && out.note_count == 5);
    VariantStep omit = step_of("omit-interval");
    assert(variant_step_set(&omit, "interval", 2, 0));
    assert(run_one(&contour, omit, 1, &out, &record) == VARIANT_OK && out.note_count == 3);
}

static void test_feature_rows_need_a_feature(void) {
    Motive source = source_motive();
    static const char *const OPS[] = {"repeat-feature", "shift-feature"};
    for (size_t i = 0; i < 2; i++) {
        Motive out;
        VariantRecord record;
        assert(run_one(&source, step_of(OPS[i]), 1, &out, &record) == VARIANT_ERR_REFUSED);
        char expected[64];
        snprintf(expected, sizeof(expected), "%s needs a labelled feature", OPS[i]);
        assert(record.failed_step == 0 && strcmp(record.error, expected) == 0);
    }
}

static void test_row_phrases(void) {
    char text[96];
    VariantValue values[VARIANT_PARAM_MAX];
    memset(values, 0, sizeof(values));
    values[0] = (VariantValue){true, 2, 0};
    values[1] = (VariantValue){true, 2, 0};
    variant_registry_find("note-repeat")->describe(values, text, sizeof(text));
    assert(strcmp(text, "repeated the third note twice") == 0);
    values[0] = (VariantValue){true, 1, 4};
    variant_registry_find("rhythm-shift")->describe(values, text, sizeof(text));
    assert(strcmp(text, "shifted a quarter note later in the bar") == 0);
    values[0] = (VariantValue){true, 2, 0};
    values[1] = (VariantValue){true, 0, 0};
    variant_registry_find("ancillary-fill")->describe(values, text, sizeof(text));
    assert(strcmp(text, "filled the leap after note 2 with passing notes") == 0);
}

/* ----------------------------------------------------------------- harmony */

static Motive harmonized_source(void) {
    Motive source = load("C4:q E4:q G4:q E4:q", "4/4", "C major", MOTIVE_FORMAT_NOTES);
    MotiveError error;
    assert(motive_harmony_parse("I V@2", &source.meter, source.harmony, &source.harmony_count, &error) == MOTIVE_OK);
    return source;
}

typedef struct {
    const char *op;
    const char *citation;
    VariantCategory category;
    Setting settings[3];
    size_t setting_count;
    const char *harmony; /* the variant's own harmony */
} HarmonyRow;

static const HarmonyRow HARMONY_ROWS[] = {
    {"harmony-inversion", "ch. III, Exs. 25a, b", VARIANT_CATEGORY_HARMONY, {{"chord", 0, 0}, {"inversion", 1, 0}}, 2,
     "I6 V@2"},
    {"harmony-append", "ch. III, Exs. 25c to i", VARIANT_CATEGORY_HARMONY, {{"degree", 1, 0}, {"seventh", 0, 0}}, 2,
     "I V@2 I@3"},
    {"harmony-insert", "ch. III, Ex. 26", VARIANT_CATEGORY_HARMONY, {{"chord", 0, 0}, {"degree", 4, 0}}, 2,
     "I IV@1 V@2"},
    {"harmony-substitute", "ch. III, Exs. 27a to i", VARIANT_CATEGORY_HARMONY,
     {{"chord", 1, 0}, {"degree", 4, 0}}, 2, "I IV@2"},
    {"passing-harmony", "ch. III, Ex. 29", VARIANT_CATEGORY_MELODY_ADAPTATION, {{"chord", 0, 0}}, 1,
     "I VII@1 V@2"},
    {"dominant-form", "ch. V, the dominant form", VARIANT_CATEGORY_HARMONY, {{0, 0, 0}}, 0, "V I@2"}};

static void test_harmony_rows(void) {
    Motive source = harmonized_source();
    Motive before = source;
    for (size_t i = 0; i < sizeof(HARMONY_ROWS) / sizeof(HARMONY_ROWS[0]); i++) {
        const HarmonyRow *row = &HARMONY_ROWS[i];
        const VariantOperation *operation = variant_registry_find(row->op);
        assert(operation && operation->category == row->category);
        assert(strcmp(operation->citation, row->citation) == 0);
        assert(operation->forms == VARIANT_FORM_BIT(MOTIVE_FORM_FULL));
        VariantStep step = step_of(row->op);
        for (size_t k = 0; k < row->setting_count; k++) {
            assert(variant_step_set(&step, row->settings[k].name, row->settings[k].value, row->settings[k].den));
        }
        Motive out;
        VariantRecord record;
        assert(run_one(&source, step, 1, &out, &record) == VARIANT_OK);
        assert(motive_equal(&source, &before) && !motive_equal(&source, &out));
        /* The chords the row made are the variant's own, and the record writes them as text. */
        assert(out.harmony_count > 0);
        assert(strcmp(record.harmony, row->harmony) == 0);
        assert(changed(&record, "harmony"));
        assert(out.note_count == source.note_count);
        assert(kept(&record, "rhythm"));
    }
    VariantRecord record;
    Motive out;
    assert(run_one(&source, step_of("exact"), 1, &out, &record) == VARIANT_OK);
    assert(strcmp(record.harmony, "I V@2") == 0);
}

static void test_dominant_form_answers_i_v(void) {
    Motive source = harmonized_source();
    Motive out;
    VariantRecord record;
    assert(run_one(&source, step_of("dominant-form"), 1, &out, &record) == VARIANT_OK);
    assert(out.harmony_count == 2 && out.harmony[0].degree == 5 && out.harmony[1].degree == 1);
    assert(plan_time_compare(out.harmony[1].onset, source.harmony[1].onset) == 0);
    /* The melody moves with the harmony, and the rhythm and the contour stay. */
    assert(kept(&record, "rhythm") && kept(&record, "contour") && changed(&record, "intervals"));
    assert(out.notes[0].midi_pitch != source.notes[0].midi_pitch);
}

static void test_harmony_rows_need_a_key(void) {
    Motive source = load("C4:q E4:q G4:q E4:q", "4/4", NULL, MOTIVE_FORMAT_NOTES);
    for (size_t i = 0; i < sizeof(HARMONY_ROWS) / sizeof(HARMONY_ROWS[0]); i++) {
        const HarmonyRow *row = &HARMONY_ROWS[i];
        VariantStep step = step_of(row->op);
        for (size_t k = 0; k < row->setting_count; k++) {
            assert(variant_step_set(&step, row->settings[k].name, row->settings[k].value, row->settings[k].den));
        }
        Motive out;
        VariantRecord record;
        assert(run_one(&source, step, 1, &out, &record) == VARIANT_ERR_REFUSED);
        assert(record.failed_step == 0 && strstr(record.error, "no key") != NULL);
    }
}

static void test_harmony_rows_without_own_harmony(void) {
    /* The best implied chord stands in at 0, so a motive without chords of its own still takes a harmony row. */
    Motive source = load("C4:q E4:q G4:q E4:q", "4/4", "C major", MOTIVE_FORMAT_NOTES);
    Motive out;
    VariantRecord record;
    assert(run_one(&source, step_of("dominant-form"), 1, &out, &record) == VARIANT_OK);
    assert(out.harmony_count == 1 && out.harmony[0].degree == 5 && strcmp(record.harmony, "V") == 0);
    /* A variant made without a harmony row has no chords of its own to write. */
    VariantStep step = step_of("transpose");
    assert(variant_step_set(&step, "by", 2, 0));
    assert(run_one(&source, step, 1, &out, &record) == VARIANT_OK && record.harmony[0] == '\0');
}

static void test_counter_line_needs_an_accompaniment(void) {
    Motive melody = load("C4:q C4:q C4:q C4:q", "4/4", "C major", MOTIVE_FORMAT_NOTES);
    Motive out;
    VariantRecord record;
    assert(run_one(&melody, step_of("counter-line"), 1, &out, &record) == VARIANT_ERR_REFUSED);
    assert(record.failed_step == 0 && strcmp(record.error, "counter-line needs an accompaniment motive") == 0);

    Motive accompaniment = melody;
    accompaniment.role = MOTIVE_ROLE_ACCOMPANIMENT;
    assert(run_one(&accompaniment, step_of("counter-line"), 1, &out, &record) == VARIANT_OK);
    assert(out.role == MOTIVE_ROLE_ACCOMPANIMENT && out.note_count == 4 && kept(&record, "rhythm"));
    assert(!motive_equal(&accompaniment, &out));

    /* A random chain draws counter-line for an accompaniment only. */
    bool drawn = false;
    for (unsigned long seed = 0; seed < 400; seed++) {
        VariantChain chain;
        assert(variant_random_chain(&melody, seed, &chain));
        for (size_t s = 0; s < chain.step_count; s++) {
            assert(strcmp(chain.steps[s].op, "counter-line") != 0);
        }
        assert(variant_random_chain(&accompaniment, seed, &chain));
        for (size_t s = 0; s < chain.step_count; s++) {
            drawn = drawn || strcmp(chain.steps[s].op, "counter-line") == 0;
        }
    }
    assert(drawn);
}

static void test_random_chains_skip_passing_harmony_without_two_chords(void) {
    Motive plain = load("C4:q E4:q G4:q E4:q", "4/4", "C major", MOTIVE_FORMAT_NOTES);
    Motive harmonized = harmonized_source();
    bool drawn = false;
    for (unsigned long seed = 0; seed < 400; seed++) {
        VariantChain chain;
        assert(variant_random_chain(&plain, seed, &chain));
        for (size_t s = 0; s < chain.step_count; s++) {
            assert(strcmp(chain.steps[s].op, "passing-harmony") != 0);
        }
        assert(variant_random_chain(&harmonized, seed, &chain));
        for (size_t s = 0; s < chain.step_count; s++) {
            drawn = drawn || strcmp(chain.steps[s].op, "passing-harmony") == 0;
        }
    }
    assert(drawn);
}

static void test_harmony_rows_draw_open_parameters(void) {
    Motive source = harmonized_source();
    static const char *const OPS[] = {"harmony-inversion", "harmony-append", "harmony-insert", "harmony-substitute",
                                      "passing-harmony"};
    for (size_t i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
        unsigned ok = 0;
        for (unsigned long seed = 0; seed < 40; seed++) {
            Motive out;
            VariantRecord record;
            if (run_one(&source, step_of(OPS[i]), seed, &out, &record) == VARIANT_OK) {
                ok++;
                assert(record.harmony[0] != '\0');
            }
        }
        assert(ok > 0);
    }
}

static void test_harmony_row_phrases(void) {
    char text[128];
    VariantValue values[VARIANT_PARAM_MAX];
    memset(values, 0, sizeof(values));
    values[0] = (VariantValue){true, 1, 0};
    values[1] = (VariantValue){true, 4, 0};
    variant_registry_find("harmony-substitute")->describe(values, text, sizeof(text));
    assert(strcmp(text, "substituted IV for the second chord, melody adapted") == 0);
    values[2] = (VariantValue){true, 5, 0};
    variant_registry_find("harmony-substitute")->describe(values, text, sizeof(text));
    assert(strcmp(text, "substituted IV then V for the second chord, melody adapted") == 0);
    values[0] = (VariantValue){true, 0, 0};
    values[1] = (VariantValue){true, 2, 0};
    variant_registry_find("harmony-inversion")->describe(values, text, sizeof(text));
    assert(strcmp(text, "put the first chord in second inversion, melody adapted") == 0);
    values[0] = (VariantValue){true, 5, 0};
    values[1] = (VariantValue){true, 1, 0};
    variant_registry_find("harmony-append")->describe(values, text, sizeof(text));
    assert(strcmp(text, "added V7 at the end, melody adapted") == 0);
    values[0] = (VariantValue){true, 0, 0};
    values[1] = (VariantValue){true, 2, 0};
    variant_registry_find("harmony-insert")->describe(values, text, sizeof(text));
    assert(strcmp(text, "inserted II after the first chord, melody adapted") == 0);
    variant_registry_find("dominant-form")->describe(values, text, sizeof(text));
    assert(strcmp(text, "put in the dominant form, harmony and melody moved to the dominant") == 0);
}

int main(void) {
    test_registry_entries();
    test_registry_forms();
    test_step_parameters();
    test_same_seed_same_result();
    test_open_parameters_recorded();
    test_fixed_chain_result();
    test_refused_steps();
    test_transpose_tonal_is_exact();
    test_invert_changes_contour();
    test_own_harmony_in_kept_and_changed();
    test_augment_keeps_rhythm_proportions();
    test_forms_have_only_their_dimensions();
    test_derivation_sentence();
    test_random_chains();
    test_json();
    test_rows_name_their_relations();
    test_each_row_applies();
    test_rows_draw_open_parameters();
    test_row_forms();
    test_feature_rows_need_a_feature();
    test_row_phrases();
    test_harmony_rows();
    test_dominant_form_answers_i_v();
    test_harmony_rows_need_a_key();
    test_harmony_rows_without_own_harmony();
    test_counter_line_needs_an_accompaniment();
    test_random_chains_skip_passing_harmony_without_two_chords();
    test_harmony_rows_draw_open_parameters();
    test_harmony_row_phrases();
    puts("test_variant_registry: ok");
    return 0;
}
