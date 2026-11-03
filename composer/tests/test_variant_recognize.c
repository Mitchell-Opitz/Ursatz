#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/variant_registry.h>

/*
 * The round trip of R-139: every variant the Composer makes is recovered. Four fixtures (a full motive in major, a full
 * motive in minor with a pickup and harmony I-V, a rhythm and a contour; features on the full ones) and an accompaniment
 * variant of the major one. Each fixture gets every registry row it takes, on seeds 1 to 20, and random chains on seeds 1
 * to 50; each result is recognized, the chain it names is replayed through the registry, and the replay must equal the
 * result. A failure prints the operation, the seed and both chains. With --smoke, seeds 1 to 3 only.
 */

typedef struct {
    const char *name;
    Motive motive;
} Fixture;

static size_t checked;
static size_t refused;
static size_t failures;
static size_t long_checked; /* random chains of three or more steps (R-149) */
static size_t long_failures;

static Motive load(const char *notes, const char *meter, const char *key, MotiveFormat format,
                   const MotiveOptions *extra) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    if (extra) {
        options = *extra;
    }
    if (!motive_parse_meter(meter, &options.meter)) {
        fprintf(stderr, "bad meter %s\n", meter);
        exit(2);
    }
    options.has_meter = true;
    options.has_key = key && motive_parse_key(key, &options.key);
    Motive motive;
    MotiveError error;
    if (motive_load(notes, strlen(notes), format, &options, &motive, &error) != MOTIVE_OK) {
        fprintf(stderr, "cannot load '%s': %s\n", notes, error.message);
        exit(2);
    }
    return motive;
}

static void add_feature(MotiveOptions *options, const char *label, size_t first, size_t last) {
    MotiveSketchFeature *feature = &options->features[options->feature_count++];
    snprintf(feature->label, sizeof(feature->label), "%s", label);
    feature->first = first;
    feature->last = last;
    options->has_features = true;
}

static void make_fixtures(Fixture *fixtures) {
    MotiveOptions major;
    memset(&major, 0, sizeof(major));
    add_feature(&major, "a", 0, 1);
    add_feature(&major, "b", 3, 4);
    fixtures[0].name = "full major";
    fixtures[0].motive = load("C4:q E4:e G4:e F4:q E4:q", "4/4", "C major", MOTIVE_FORMAT_NOTES, &major);

    MotiveOptions minor;
    memset(&minor, 0, sizeof(minor));
    minor.has_pickup = true;
    minor.pickup = (Rational){1, 4};
    add_feature(&minor, "a", 0, 1);
    PlanMeter meter;
    MotiveError error;
    if (!motive_parse_meter("4/4", &meter) ||
        motive_harmony_parse("I V@2", &meter, minor.harmony, &minor.harmony_count, &error) != MOTIVE_OK) {
        fprintf(stderr, "cannot read the minor fixture's harmony\n");
        exit(2);
    }
    minor.has_harmony = true;
    fixtures[1].name = "full minor, pickup, harmony I-V";
    fixtures[1].motive = load("D4:q F4:e A4:e D5:q C5:q", "4/4", "D minor", MOTIVE_FORMAT_NOTES, &minor);

    fixtures[2].name = "rhythm";
    fixtures[2].motive = load("x:q x:e x:e x:q x:q", "4/4", NULL, MOTIVE_FORMAT_RHYTHM, NULL);
    fixtures[3].name = "contour";
    fixtures[3].motive = load("0 2 1 3 2", "4/4", NULL, MOTIVE_FORMAT_CONTOUR, NULL);

    fixtures[4].name = "accompaniment";
    fixtures[4].motive = fixtures[0].motive;
    fixtures[4].motive.accompaniment = true;
}

static void print_chain(const char *label, const VariantChain *chain) {
    fprintf(stderr, "  %s:", label);
    for (size_t s = 0; s < chain->step_count; s++) {
        fprintf(stderr, " %s", chain->steps[s].op);
        const char *separator = "(";
        const VariantOperation *operation = variant_registry_find(chain->steps[s].op);
        for (size_t p = 0; operation && p < operation->param_count; p++) {
            if (chain->steps[s].values[p].set) {
                fprintf(stderr, "%s%s=%ld", separator, operation->params[p].name, chain->steps[s].values[p].value);
                if (chain->steps[s].values[p].den) {
                    fprintf(stderr, ":%ld", chain->steps[s].values[p].den);
                }
                separator = ", ";
            }
        }
        fprintf(stderr, "%s", separator[0] == '(' ? "" : ")");
    }
    fprintf(stderr, "\n");
}

/* Makes the variant of the chain on the seed, recognizes it, replays the recognized chain and compares. */
static bool round_trip(const Fixture *fixture, const VariantChain *chain, unsigned long seed) {
    Motive variant;
    VariantRecord record;
    if (variant_apply_chain(&fixture->motive, chain, seed, &variant, &record) != VARIANT_OK) {
        refused++;
        return true;
    }
    checked++;
    VariantChain found;
    VariantRecognition recognition;
    Motive replay;
    bool ok = variant_recognize(&fixture->motive, &variant, &found, &recognition) == VARIANT_OK && recognition.found &&
              variant_apply_chain(&fixture->motive, &found, 0, &replay, NULL) == VARIANT_OK && motive_equal(&replay, &variant);
    if (ok) {
        return true;
    }
    failures++;
    fprintf(stderr, "NOT RECOVERED: %s, op %s, seed %lu\n", fixture->name, chain->steps[0].op, seed);
    VariantChain made;
    memset(&made, 0, sizeof(made));
    made.step_count = record.step_count;
    for (size_t s = 0; s < record.step_count; s++) {
        snprintf(made.steps[s].op, sizeof(made.steps[s].op), "%s", record.steps[s].op);
        memcpy(made.steps[s].values, record.steps[s].values, sizeof(made.steps[s].values));
    }
    print_chain("made by", &made);
    print_chain("recognized", &found);
    return false;
}

static double seconds_since(clock_t start) {
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

int main(int argc, char **argv) {
    bool smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    if (argc > 2 || (argc == 2 && !smoke)) {
        fprintf(stderr, "usage: test_variant_recognize [--smoke]\n");
        return 2;
    }
    unsigned long row_seeds = smoke ? 3 : 20;
    unsigned long chain_seeds = smoke ? 3 : 50;
    Fixture fixtures[5];
    make_fixtures(fixtures);

    clock_t start = clock();
    for (size_t f = 0; f < 5; f++) {
        for (size_t i = 0; i < variant_registry_count(); i++) {
            const VariantOperation *operation = variant_registry_at(i);
            if ((operation->forms & VARIANT_FORM_BIT(fixtures[f].motive.form)) == 0) {
                continue;
            }
            VariantChain chain;
            memset(&chain, 0, sizeof(chain));
            if (!variant_step_init(&chain.steps[0], operation->name)) {
                return 2;
            }
            chain.step_count = 1;
            for (unsigned long seed = 1; seed <= row_seeds; seed++) {
                round_trip(&fixtures[f], &chain, seed);
            }
        }
    }
    printf("rows: %zu variants recognized, %zu refused by their row, %.1f s\n", checked, refused, seconds_since(start));

    size_t rows_checked = checked;
    start = clock();
    for (size_t f = 0; f < 5; f++) {
        for (unsigned long seed = 1; seed <= chain_seeds; seed++) {
            VariantChain chain;
            if (!variant_random_chain(&fixtures[f].motive, seed, &chain)) {
                return 2;
            }
            size_t before = checked;
            bool ok = round_trip(&fixtures[f], &chain, seed);
            if (chain.step_count >= 3 && checked > before) {
                long_checked++;
                if (!ok) {
                    long_failures++;
                    failures--; /* judged by the 4 in 5 bar below */
                }
            }
        }
    }
    printf("random chains: %zu variants recognized, %.1f s\n", checked - rows_checked, seconds_since(start));
    int status = 0;
    if (failures > 0) {
        fprintf(stderr, "%zu of %zu variants not recovered (every row and every chain of one or two steps must be)\n",
                failures, checked);
        status = 1;
    }
    /* R-149: chains of three or more steps are searched by a beam; at least 4 in 5 must be recovered. */
    if (long_checked > 0) {
        printf("chains of three or more steps: %zu of %zu recovered (%.1f%%)\n", long_checked - long_failures, long_checked,
               100.0 * (double)(long_checked - long_failures) / (double)long_checked);
        if (long_failures * 5 > long_checked) {
            fprintf(stderr, "fewer than 4 in 5 chains of three or more steps recovered\n");
            status = 1;
        }
    }
    if (status == 0) {
        printf("all required variants recovered\n");
    }
    return status;
}
