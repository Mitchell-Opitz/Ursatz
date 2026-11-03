#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/variant_registry.h>
#include "support/test_support.h"

/*
 * The `variants` command, run as a process: the operation list, variants from a chain file, that the same seed gives
 * the same output and another seed does not, the JSON shape, and the exit status and message for a refused step.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define THEME FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\""

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" variants %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file, err_file);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

/* The standard output of a command that must succeed. */
static char *output_of(const char *arguments) {
    assert(run(arguments, "variants_cli_out.txt", "variants_cli_err.txt") == 0);
    char *err = test_read_file("variants_cli_err.txt", NULL);
    assert(err[0] == '\0');
    free(err);
    return test_read_file("variants_cli_out.txt", NULL);
}

/* The standard output of a command that must succeed; stderr may hold notes on skipped draws. */
static char *output_with_notes(const char *arguments) {
    assert(run(arguments, "variants_cli_out.txt", "variants_cli_err.txt") == 0);
    return test_read_file("variants_cli_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    assert(run(arguments, "variants_cli_out.txt", "variants_cli_err.txt") == status);
    char *err = test_read_file("variants_cli_err.txt", NULL);
    if (strstr(err, message) == NULL) {
        fprintf(stderr, "'%s': expected '%s' in: %s\n", arguments, message, err);
    }
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
}

static size_t count_of(const char *text, const char *needle) {
    size_t count = 0;
    for (const char *at = strstr(text, needle); at; at = strstr(at + 1, needle)) {
        count++;
    }
    return count;
}

static void test_list_ops(void) {
    char *out = output_of("--list-ops");
    static const char *const NAMES[] = {"exact", "transpose", "invert", "retrograde", "interval-change",
                                        "fragment", "liquidate", "augment", "diminish"};
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        char line[64];
        snprintf(line, sizeof(line), "\n%s  ", NAMES[i]);
        assert(i == 0 ? strncmp(out, line + 1, strlen(line) - 1) == 0 : strstr(out, line) != NULL);
    }
    assert(strstr(out, "(intervals; ch. III, Exs. 14, 19)") != NULL);
    assert(strstr(out, "forms: full, contour") != NULL);
    free(out);

    out = output_of("--list-ops --json");
    assert(strstr(out, "\"operations\"") != NULL);
    assert(count_of(out, "\"citation\"") == 33);
    free(out);
}

static void test_chain_file(void) {
    test_write_file("variants_cli_chain.json",
               "{\"steps\": [{\"op\": \"invert\", \"params\": {\"tonal\": true}}, "
               "{\"op\": \"augment\", \"params\": {\"ratio\": \"2:1\"}}]}");
    char *out = output_of(THEME " --chain variants_cli_chain.json --count 2 --seed 1");
    assert(strstr(out, "Source: D4:q F4:e A4:e D5:h") != NULL);
    assert(strstr(out, "Seed: 1") != NULL);
    assert(strstr(out, "Variant 1 (seed 1): invert, augment") != NULL);
    assert(strstr(out, "Variant 2 (seed 2): invert, augment") != NULL);
    assert(strstr(out, "Inverted about") != NULL && strstr(out, "then augmented 2:1") != NULL);
    assert(strstr(out, "Kept the rhythm's proportions") != NULL);
    free(out);
}

/* The same input, options and seed give the same output, and another seed does not. */
static void test_seeds(void) {
    char *first = output_of(THEME " --count 4 --seed 9");
    assert(count_of(first, "\nVariant ") == 4);
    char *again = output_of(THEME " --count 4 --seed 9");
    assert(strcmp(first, again) == 0);
    free(again);

    bool differs = false;
    for (int seed = 10; seed <= 20 && !differs; seed++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments), THEME " --count 4 --seed %d", seed);
        char *other = output_of(arguments);
        differs = strcmp(first, other) != 0;
        free(other);
    }
    assert(differs);
    free(first);

    /* The default is six variants, and the seed is printed. */
    char *six = output_of(THEME " --seed 2");
    assert(count_of(six, "\nVariant ") == 6 && strstr(six, "Seed: 2") != NULL);
    free(six);
}

static void test_json(void) {
    char *out = output_of(THEME " --count 2 --seed 3 --json");
    assert(strncmp(out, "{\n  \"seed\": 3,\n  \"source\": {", 28) == 0);
    assert(strstr(out, "\"variants\": [") != NULL);
    assert(count_of(out, "\"derivation\"") == 2);
    assert(count_of(out, "\"functions\": [") == 2 && count_of(out, "\"remoteness\"") == 2);
    assert(count_of(out, "\"recognizability\"") == 2);
    assert(count_of(out, "\"kind\"") == 2);
    /* The source and each variant carry the motive document: the motive, and the analysis with its text forms. */
    assert(count_of(out, "\"notes_text\"") == 3 && count_of(out, "\"degrees_text\"") == 3);
    assert(strstr(out, "\"notes_text\": \"D4:q F4:e A4:e D5:h\"") != NULL);
    free(out);
}

static void test_bad_arguments(void) {
    expect_failure(THEME " --count 0", 1, "--count must be a whole number from 1 to 16");
    expect_failure(THEME " --count 17", 1, "--count must be a whole number from 1 to 16");
    expect_failure(THEME " --seed x", 1, "--seed must be a whole number");
    expect_failure(THEME " --chain missing.json", 1, "cannot read chain file 'missing.json'");
    expect_failure("--list-ops " FIXTURE("motive_d_minor.txt"), 1, "--list-ops takes no motive input");
    expect_failure("", 1, "variants needs --list-ops or a motive input");
    expect_failure(FIXTURE("motive_missing.txt") " --meter 4/4", 2, "cannot load");

    test_write_file("variants_cli_bad.json", "{\"steps\": [{\"op\": \"spin\"}]}");
    expect_failure(THEME " --chain variants_cli_bad.json", 1, "step 1: unknown operation 'spin'");
    test_write_file("variants_cli_bad.json", "{\"steps\": [{\"op\": \"invert\", \"params\": {\"by\": 2}}]}");
    expect_failure(THEME " --chain variants_cli_bad.json", 1, "invert has no parameter 'by'");
    test_write_file("variants_cli_bad.json", "{\"steps\": [{\"op\": \"augment\", \"params\": {\"ratio\": 2}}]}");
    expect_failure(THEME " --chain variants_cli_bad.json", 1, "'ratio' of augment must be a ratio text");
}

/* A step an operation does not apply to says which step and why. */
static void test_refused_step(void) {
    test_write_file("variants_cli_refused.json",
               "{\"steps\": [{\"op\": \"retrograde\"}, {\"op\": \"interval-change\"}]}");
    test_write_file("variants_cli_contour.txt", "0 2 1 3\n");
    expect_failure("variants_cli_contour.txt --form contour --meter 4/4 --chain variants_cli_refused.json", 2,
                   "step 2 of the chain: interval-change does not apply to a contour motive");
    char *out = test_read_file("variants_cli_out.txt", NULL);
    assert(out[0] == '\0');
    free(out);
}

/* A harmony row writes the variant's own harmony in the record and in its motive; a variant without some has neither. */
static void test_harmony_json(void) {
    test_write_file("variants_cli_harmony.json",
               "{\"steps\": [{\"op\": \"harmony-substitute\", \"params\": {\"chord\": 1, \"degree\": 4}}]}");
    char *out = output_of(THEME " --harmony \"I V@2\" --chain variants_cli_harmony.json --count 1 --seed 1 --json");
    /* The source's harmony, then the record's, then the variant's motive. */
    assert(count_of(out, "\"harmony\": \"I V@2\"") == 1);
    assert(count_of(out, "\"harmony\": \"I IV@2\"") == 2);
    assert(count_of(out, "\"harmony_text\": \"I IV@2\"") == 1);
    const char *changed = strstr(out, "\"changed\"");
    assert(changed != NULL && strstr(changed, "\"harmony\"") != NULL);
    free(out);

    out = output_of(THEME " --harmony \"I V@2\" --chain variants_cli_harmony.json --count 1 --seed 1");
    assert(strstr(out, "Substituted IV for the second chord, melody adapted.") != NULL);
    free(out);

    test_write_file("variants_cli_transpose.json", "{\"steps\": [{\"op\": \"transpose\", \"params\": {\"by\": 2}}]}");
    out = output_of(THEME " --chain variants_cli_transpose.json --count 1 --seed 1 --json");
    assert(count_of(out, "\"harmony\": \"") == 0);
    free(out);
}

/* Counter-line is refused for a melodic motive and works on an accompaniment. */
static void test_counter_line(void) {
    test_write_file("variants_cli_counter.json", "{\"steps\": [{\"op\": \"counter-line\"}]}");
    test_write_file("variants_cli_repeated.txt", "C4:q C4:q C4:q C4:q\n");
    expect_failure("variants_cli_repeated.txt --meter 4/4 --key \"C major\" --chain variants_cli_counter.json", 2,
                   "step 1 of the chain: counter-line needs an accompaniment motive");
    char *out = output_of("variants_cli_repeated.txt --meter 4/4 --key \"C major\" --role accompaniment "
                          "--chain variants_cli_counter.json --seed 1");
    assert(strstr(out, "Variant 1 (seed 1): counter-line") != NULL);
    free(out);
    expect_failure(THEME " --chain variants_cli_counter.json", 2, "step 1 of the chain: counter-line needs an accompaniment");
}

/* --function keeps the variants that suit a function, drawing more chains to fill the count. */
static void test_function_filter(void) {
    char *out = output_of(THEME " --count 3 --seed 1 --function cadential");
    size_t variants = count_of(out, "\nVariant ");
    assert(variants >= 1 && variants <= 3 && count_of(out, "  Functions:") == variants);
    assert(count_of(out, "cadential") == variants);
    free(out);

    /* Repeated, a variant suits any of them; in JSON each carries what it suits. */
    out = output_of(THEME " --count 4 --seed 1 --function continuation --function contrast --json");
    assert(count_of(out, "\"derivation\"") >= 1);
    assert(count_of(out, "\"functions\": [\"") == count_of(out, "\"derivation\""));
    free(out);

    expect_failure(THEME " --function dramatic", 1, "--function must be beginning, continuation, contrast or cadential");
    expect_failure(THEME " --function", 1, "--function needs a value");
    expect_failure(THEME " --function cadential --function cadential --function cadential --function cadential "
                         "--function cadential", 1, "--function is given at most 4 times");
}

/* Every "Remoteness: n/d" line of the output is inside the band: over low and at most high, as fractions. */
static size_t check_remoteness(const char *out, int low_num, int low_den, int high_num, int high_den) {
    size_t lines = 0;
    for (const char *at = strstr(out, "Remoteness: "); at; at = strstr(at + 1, "Remoteness: ")) {
        int num = 0;
        int den = 1;
        assert(sscanf(at, "Remoteness: %d/%d", &num, &den) == 2);
        assert(num * low_den > low_num * den && num * high_den <= high_num * den);
        lines++;
    }
    return lines;
}

/* The chain length of every variant: the number of operations on each "Variant n (seed s):" line. */
static void check_steps(const char *out, size_t min_steps, size_t max_steps) {
    size_t lines = 0;
    for (const char *at = strstr(out, "\nVariant "); at; at = strstr(at + 1, "\nVariant ")) {
        const char *colon = strstr(at, "):");
        const char *end = strchr(at + 1, '\n');
        end = end ? strchr(end + 1, '\n') : NULL;
        assert(colon && end && colon < end);
        size_t steps = 1;
        for (const char *c = colon; c < end; c++) {
            steps += *c == ',';
        }
        assert(steps >= min_steps && steps <= max_steps);
        lines++;
    }
    assert(lines >= 1);
}

/* --distance: the chain length and the remoteness band of each band, on a one-bar motive. */
static void test_distance(void) {
    char *out = output_with_notes(THEME " --count 4 --seed 3 --distance close");
    check_steps(out, 1, 1);
    assert(check_remoteness(out, 0, 1, 1, 3) >= 1);
    free(out);

    out = output_with_notes(THEME " --count 4 --seed 3 --distance medium");
    check_steps(out, 2, 2);
    assert(check_remoteness(out, 1, 3, 2, 3) >= 1);
    free(out);

    out = output_with_notes(THEME " --count 4 --seed 3 --distance far");
    check_steps(out, 3, 4);
    assert(check_remoteness(out, 2, 3, 1, 1) >= 1);
    free(out);

    /* With --function both must hold, and --json carries both. */
    out = output_with_notes(THEME " --count 3 --seed 1 --distance far --function contrast --json");
    assert(count_of(out, "\"derivation\"") >= 1);
    assert(count_of(out, "\"contrast\"") == count_of(out, "\"derivation\""));
    free(out);

    /* Fewer than asked is a note on stderr, not a failure. */
    out = output_with_notes(THEME " --count 16 --seed 1 --distance far --function cadential");
    char *err = test_read_file("variants_cli_err.txt", NULL);
    assert(count_of(out, "\nVariant ") < 16 && strstr(err, "suit the function and distance") != NULL);
    free(err);
    free(out);

    expect_failure(THEME " --distance near", 1, "--distance must be close, medium or far, not 'near'");
    expect_failure(THEME " --distance", 1, "--distance needs a value");
    expect_failure(THEME " --distance far --distance far", 1, "--distance given twice");
    test_write_file("variants_cli_chain.json", "{\"steps\": [{\"op\": \"retrograde\"}]}");
    expect_failure(THEME " --distance far --chain variants_cli_chain.json", 1,
                   "--distance takes random chains, not --chain");
}

/* The note list the text output gives for the variant made by this row, or NULL. Not freed; points into out. */
static const char *notes_of_row(const char *out, const char *op, char *line, size_t size) {
    char header[64];
    snprintf(header, sizeof(header), "): %s\n  ", op);
    const char *at = strstr(out, header);
    if (!at) {
        return NULL;
    }
    at += strlen(header);
    size_t length = strcspn(at, "\n");
    snprintf(line, size, "%.*s", (int)length, at);
    return line;
}

/* Whether the JSON variant made by this row has a pickup other than 0. */
static bool row_has_pickup(const char *json, const char *op) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"op\": \"%s\"", op);
    const char *at = strstr(json, needle);
    at = at ? strstr(at, "\"pickup\": \"") : NULL;
    return at && strncmp(at + strlen("\"pickup\": \""), "0\"", 2) != 0;
}

/* --each-op on the theme: harmony rows move notes, the upbeat rows give a pickup, and a variant that leaves its harmony is marked. */
static void test_each_op_labels(void) {
    char *out = output_of(THEME " --each-op --seed 3");
    char source[256];
    assert(strncmp(out, "Source: ", 8) == 0);
    snprintf(source, sizeof(source), "%.*s", (int)strcspn(out + 8, "\n"), out + 8);
    static const char *const MOVING[] = {"harmony-append", "harmony-insert", "harmony-inversion", "reharmonize-frame"};
    for (size_t i = 0; i < sizeof(MOVING) / sizeof(MOVING[0]); i++) {
        char line[256];
        const char *notes = notes_of_row(out, MOVING[i], line, sizeof(line));
        if (!notes) {
            fprintf(stderr, "no variant for %s\n", MOVING[i]);
        }
        assert(notes && strcmp(notes, source) != 0);
    }
    assert(strstr(out, "Warning: Does not outline its harmony") != NULL);
    for (size_t i = 0; i < 5; i++) {
        static const char *const FRAME[] = {"reduce-to-frame", "add-non-chord-tone", "remove-non-chord-tone",
                                            "change-non-chord-tone", "reharmonize-frame"};
        assert(strstr(out, FRAME[i]) != NULL);
    }
    free(out);

    out = output_of(THEME " --each-op --seed 3 --json");
    assert(row_has_pickup(out, "rhythm-shift") && row_has_pickup(out, "add-upbeat"));
    assert(strstr(out, "\"warnings\": [") != NULL && strstr(out, "Does not outline its harmony") != NULL);
    free(out);
}

/* --each-op: one variant per registry row that takes the form, the others listed as skipped. */
static void test_each_op(void) {
    size_t rows = 0;
    for (size_t i = 0; i < variant_registry_count(); i++) {
        rows += (variant_registry_at(i)->forms & VARIANT_FORM_BIT(MOTIVE_FORM_FULL)) != 0;
    }
    char *out = output_of(THEME " --each-op --seed 3");
    size_t made = count_of(out, "\nVariant ");
    size_t skipped = count_of(out, "Skipped ");
    assert(made >= 1 && skipped >= 1 && made + skipped == rows);
    assert(strstr(out, "Variant 1 (seed 3): exact") != NULL);
    assert(strstr(out, "Skipped counter-line: counter-line needs an accompaniment motive") != NULL);
    assert(strstr(out, "Skipped repeat-feature: repeat-feature needs a labelled feature") != NULL);
    /* Every variant is one step. */
    check_steps(out, 1, 1);
    free(out);

    out = output_of(THEME " --each-op --seed 3 --json");
    assert(count_of(out, "\"derivation\"") == made);
    assert(strstr(out, "\"skipped\": [") != NULL && count_of(out, "\"reason\"") == skipped);
    assert(strstr(out, "\"op\": \"counter-line\"") != NULL);
    free(out);

    /* A contour motive takes fewer rows, and the skipped ones are the contour's refusals. */
    size_t contour_rows = 0;
    for (size_t i = 0; i < variant_registry_count(); i++) {
        contour_rows += (variant_registry_at(i)->forms & VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR)) != 0;
    }
    assert(contour_rows < rows);
    test_write_file("variants_cli_contour.txt", "0 2 1 3\n");
    out = output_of("variants_cli_contour.txt --form contour --meter 4/4 --each-op --seed 2");
    assert(count_of(out, "\nVariant ") + count_of(out, "Skipped ") == contour_rows);
    assert(strstr(out, "Skipped interval-change") == NULL);
    free(out);

    expect_failure(THEME " --each-op --count 2", 1, "--each-op takes no --chain, --count, --distance or --function");
    expect_failure(THEME " --each-op --distance far", 1, "--each-op takes no --chain");
    expect_failure(THEME " --each-op --function contrast", 1, "--each-op takes no --chain");
    test_write_file("variants_cli_chain.json", "{\"steps\": [{\"op\": \"retrograde\"}]}");
    expect_failure(THEME " --each-op --chain variants_cli_chain.json", 1, "--each-op takes no --chain");
}

/* Liquidation by features, from a chain file, and remoteness from a basic motive. */
static void test_liquidate_features_and_basic(void) {
    test_write_file("variants_cli_liquidate.json",
               "{\"steps\": [{\"op\": \"liquidate-features\", \"params\": {\"steps\": 1, \"shorten\": false}}]}");
    char *out = output_of(THEME " --chain variants_cli_liquidate.json --count 1 --seed 1");
    assert(strstr(out, "Variant 1 (seed 1): liquidate-features") != NULL && strstr(out, "Liquidated 1 characteristic") != NULL);
    assert(strstr(out, "Remoteness: 1/3") != NULL && strstr(out, "continuation") != NULL);
    free(out);

    /* From another basic motive the remoteness is measured from that one: the source itself is no distance. */
    out = output_of(THEME " --chain variants_cli_liquidate.json --count 1 --seed 1 --basic " FIXTURE("motive_d_minor.txt"));
    assert(strstr(out, "Remoteness: 1/3") != NULL);
    free(out);
    expect_failure(THEME " --basic-pickup 1", 1, "need --basic <file>");
    expect_failure(THEME " --basic", 1, "--basic needs a value");
}

int main(void) {
    test_list_ops();
    test_chain_file();
    test_seeds();
    test_json();
    test_bad_arguments();
    test_refused_step();
    test_harmony_json();
    test_counter_line();
    test_function_filter();
    test_liquidate_features_and_basic();
    test_distance();
    test_each_op();
    test_each_op_labels();
    printf("variants cli tests passed\n");
    return 0;
}
