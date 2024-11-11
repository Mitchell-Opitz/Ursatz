#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/plan_json.h>

/*
 * The `plan` and `fill` commands, run as processes: every form planned and filled to MIDI and MusicXML, the same plan and
 * seed giving the same bytes, a plan from `grow --out` filling, the options, and the usage errors, exit codes and
 * messages (1 usage, 2 refused or unreadable or stuck, 3 not written, as `grow`).
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define OUT "plan_cli_out.txt"
#define ERR "plan_cli_err.txt"

/* Runs "ursatz-composer <command> <arguments>"; stdout and stderr go to files. Returns the exit status. */
static int run(const char *command, const char *arguments) {
    char line[4096];
    snprintf(line, sizeof(line), "\"%s\" %s %s >" OUT " 2>" ERR, COMPOSER_CLI_PATH, command, arguments);
    int status = system(line);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static char *read_file(const char *path, size_t *length_out) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open %s\n", path);
    }
    assert(file);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)length + 1);
    assert(text);
    size_t read = fread(text, 1, (size_t)length, file);
    assert(read == (size_t)length);
    text[length] = '\0';
    fclose(file);
    if (length_out) {
        *length_out = (size_t)length;
    }
    return text;
}

static bool exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
    }
    return file != NULL;
}

/* The standard output of a command that must succeed. */
static char *output_of(const char *command, const char *arguments) {
    int status = run(command, arguments);
    if (status != 0) {
        char *err = read_file(ERR, NULL);
        fprintf(stderr, "'%s %s' exited %d: %s\n", command, arguments, status, err);
        free(err);
    }
    assert(status == 0);
    return read_file(OUT, NULL);
}

static void expect_failure(const char *command, const char *arguments, int status, const char *message) {
    int got = run(command, arguments);
    char *err = read_file(ERR, NULL);
    if (got != status || strstr(err, message) == NULL) {
        fprintf(stderr, "'%s %s': expected status %d with '%s', got %d: %s\n", command, arguments, status, message, got,
                err);
    }
    assert(got == status);
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
}

static void expect_same_file(const char *a, const char *b) {
    size_t length_a;
    size_t length_b;
    char *text_a = read_file(a, &length_a);
    char *text_b = read_file(b, &length_b);
    assert(length_a > 0);
    assert(length_a == length_b);
    assert(memcmp(text_a, text_b, length_a) == 0);
    free(text_a);
    free(text_b);
}

static const char *const FORMS[] = {"binary", "rounded-binary", "ternary", "variations", "loop", "layered"};

/* Every form: the plan is written, `fill` completes it, and the files are what they should be. */
static void test_every_form(void) {
    for (size_t f = 0; f < sizeof(FORMS) / sizeof(FORMS[0]); f++) {
        char arguments[1024];
        snprintf(arguments, sizeof(arguments), "--form %s --key C --meter 4/4 --seed 1 --out plan_cli_%s.json", FORMS[f],
                 FORMS[f]);
        char *out = output_of("plan", arguments);
        assert(strstr(out, FORMS[f]) == out);
        assert(strstr(out, "motive invented at fill") != NULL);
        assert(strstr(out, "sections") != NULL);
        free(out);

        PiecePlan *plan = NULL;
        char path[256];
        snprintf(path, sizeof(path), "plan_cli_%s.json", FORMS[f]);
        PlanJsonError error;
        assert(plan_read_json_file(path, &plan, &error) == PLAN_OK);
        plan_destroy(plan);

        snprintf(arguments, sizeof(arguments),
                 "plan_cli_%s.json --seed 1 --out plan_cli_%s_done.json --midi plan_cli_%s.mid --musicxml plan_cli_%s.xml "
                 "--record plan_cli_%s_record.json",
                 FORMS[f], FORMS[f], FORMS[f], FORMS[f], FORMS[f]);
        out = output_of("fill", arguments);
        assert(strstr(out, "seed 1") == out);
        assert(strstr(out, "sections (form and cadences heard)") != NULL);
        assert(strstr(out, "returns heard") != NULL);
        free(out);

        char name[256];
        snprintf(name, sizeof(name), "plan_cli_%s.mid", FORMS[f]);
        char *midi = read_file(name, NULL);
        assert(strncmp(midi, "MThd", 4) == 0);
        free(midi);
        snprintf(name, sizeof(name), "plan_cli_%s.xml", FORMS[f]);
        char *xml = read_file(name, NULL);
        assert(strstr(xml, "<score-partwise") != NULL);
        free(xml);
        snprintf(name, sizeof(name), "plan_cli_%s_record.json", FORMS[f]);
        char *record = read_file(name, NULL);
        assert(record[0] == '[');
        assert(strstr(record, "\"stage\":\"structure\"") != NULL);
        assert(strstr(record, "\"stage\":\"melody\"") != NULL);
        free(record);
        /* The realized plan is a plan and has nothing left open. */
        snprintf(name, sizeof(name), "plan_cli_%s_done.json", FORMS[f]);
        assert(plan_read_json_file(name, &plan, &error) == PLAN_OK);
        plan_destroy(plan);
        snprintf(arguments, sizeof(arguments), "%s", name);
        out = output_of("fill", arguments);
        assert(strstr(out, "already realized") != NULL);
        free(out);
    }
}

/* The same plan and seed give the same bytes. */
static void test_same_seed(void) {
    static const char *const SEEDED[] = {"ternary", "variations", "layered"};
    for (size_t f = 0; f < sizeof(SEEDED) / sizeof(SEEDED[0]); f++) {
        for (int run_number = 0; run_number < 2; run_number++) {
            char arguments[1024];
            snprintf(arguments, sizeof(arguments),
                     "plan_cli_%s.json --seed 3 --out plan_cli_same_%d.json --midi plan_cli_same_%d.mid --musicxml "
                     "plan_cli_same_%d.xml --record plan_cli_same_%d_record.json",
                     SEEDED[f], run_number, run_number, run_number, run_number);
            free(output_of("fill", arguments));
        }
        expect_same_file("plan_cli_same_0.json", "plan_cli_same_1.json");
        expect_same_file("plan_cli_same_0.mid", "plan_cli_same_1.mid");
        expect_same_file("plan_cli_same_0.xml", "plan_cli_same_1.xml");
        expect_same_file("plan_cli_same_0_record.json", "plan_cli_same_1_record.json");
    }
    /* The same options give the same plan, keys and cadences planned from the seed included. */
    for (int run_number = 0; run_number < 2; run_number++) {
        char arguments[512];
        snprintf(arguments, sizeof(arguments), "--form ternary --key A --mode minor --meter 3/4 --modulate --seed 4 --out "
                                               "plan_cli_planned_%d.json", run_number);
        free(output_of("plan", arguments));
    }
    expect_same_file("plan_cli_planned_0.json", "plan_cli_planned_1.json");
}

/* A plan from `grow --out` is realized already: fill exports it. */
static void test_grow_plan(void) {
    free(output_of("grow", FIXTURE("motive_d_minor.txt") " --into sentence --key \"D minor\" --meter 4/4 --seed 7 --out "
                           "plan_cli_grown.json"));
    char *out = output_of("fill", "plan_cli_grown.json --midi plan_cli_grown.mid --musicxml plan_cli_grown.xml");
    assert(strstr(out, "already realized") != NULL);
    assert(strstr(out, "wrote plan_cli_grown.mid") != NULL);
    free(out);
    char *midi = read_file("plan_cli_grown.mid", NULL);
    assert(strncmp(midi, "MThd", 4) == 0);
    free(midi);
}

static void test_options(void) {
    /* A motive, an accompaniment, the mode in the key; the plan is filled from them. */
    char *out = output_of("plan", "--form binary --key \"D minor\" --meter 4/4 --motive " FIXTURE("motive_d_minor.txt")
                                  " --accompaniment alberti --seed 2 --out plan_cli_motive.json");
    assert(strstr(out, "key D minor") != NULL);
    assert(strstr(out, "motive " FIXTURE("motive_d_minor.txt")) != NULL);
    assert(strstr(out, "accompaniment alberti") != NULL);
    free(out);
    out = output_of("fill", "plan_cli_motive.json --seed 2");
    assert(strstr(out, "seed 2") == out);
    free(out);

    /* The keys and cadences planned across the sections, and the sizes. */
    out = output_of("plan", "--form ternary --key A --mode minor --meter 3/4 --modulate --seed 4 --out plan_cli_keys.json");
    assert(strstr(out, "keys and cadences planned") != NULL);
    assert(strstr(out, "returns A (exact)") != NULL);
    free(out);
    out = output_of("plan", "--form variations --key G --meter 4/4 --count 2 --out plan_cli_count.json");
    assert(strstr(out, "variations, 24 bars") == out);
    assert(strstr(out, "V2") != NULL);
    free(out);
    out = output_of("plan", "--form rounded-binary --key F --meter 4/4 --bars 24 --out plan_cli_bars.json");
    assert(strstr(out, "rounded-binary, 24 bars") == out);
    free(out);

    /* JSON output of both commands. */
    out = output_of("plan", "--form binary --key C --meter 6/8 --texture homophony --seed 1 --format json --out "
                            "plan_cli_json.json");
    assert(out[0] == '{');
    assert(strstr(out, "\"form\":\"binary\"") != NULL);
    assert(strstr(out, "\"meter\":\"6/8\"") != NULL);
    assert(strstr(out, "\"sections\":[{\"letter\":\"A\"") != NULL);
    free(out);
    out = output_of("fill", "plan_cli_binary.json --seed 1 --format json");
    assert(out[0] == '{');
    assert(strstr(out, "\"seed\":1") != NULL);
    assert(strstr(out, "\"realized\":false") != NULL);
    assert(strstr(out, "\"returns\":{\"planned\":0") != NULL);
    free(out);

    char *help = output_of("plan", "--help");
    assert(strstr(help, "--form") != NULL);
    free(help);
    help = output_of("fill", "--help");
    assert(strstr(help, "--record") != NULL);
    free(help);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static void test_plan_errors(void) {
    expect_failure("plan", "--form nope --key C --meter 4/4 --out plan_cli_bad.json", 1,
                   "unknown form 'nope'; the forms are binary, rounded-binary, ternary, variations, loop, layered");
    expect_failure("plan", "--form binary --key C --meter 4/4 --bars 12 --out plan_cli_bad.json", 1,
                   "binary has no size of 12 bars; it has 8, 16 bars");
    expect_failure("plan", "--form variations --key C --meter 4/4 --count 9 --out plan_cli_bad.json", 1,
                   "variations has no size of --count 9");
    expect_failure("plan", "--form loop --key C --meter 4/4 --count 2 --out plan_cli_bad.json", 1, "loop has no size");
    expect_failure("plan", "--form binary --key H --meter 4/4 --out plan_cli_bad.json", 1, "unknown key 'H'");
    expect_failure("plan", "--form binary --key C --mode dorian --meter 4/4 --out plan_cli_bad.json", 1,
                   "--mode must be major or minor");
    expect_failure("plan", "--form binary --key \"C minor\" --mode minor --meter 4/4 --out plan_cli_bad.json", 1,
                   "names a mode already");
    expect_failure("plan", "--form binary --key C --meter 5/4 --out plan_cli_bad.json", 1, "meter '5/4' is not supported");
    expect_failure("plan", "--form binary --key C --meter 4/4", 1, "plan needs --form, --key, --meter and --out");
    expect_failure("plan", "--form binary --key C --meter 4/4 --seed x --out plan_cli_bad.json", 1, "--seed must be");
    expect_failure("plan", "--form binary --key C --meter 4/4 --format xml --out plan_cli_bad.json", 1,
                   "--format must be text or json");
    expect_failure("plan", "--form binary --key C --meter 4/4 --bogus --out plan_cli_bad.json", 1, "unknown option '--bogus'");
    expect_failure("plan", "--form binary --key C --meter 6/8 --accompaniment alberti --out plan_cli_bad.json", 1,
                   "alberti needs a simple meter");
    expect_failure("plan", "--form binary --key C --meter 4/4 --motive plan_cli_missing.txt --out plan_cli_bad.json", 2,
                   "cannot load 'plan_cli_missing.txt'");
    expect_failure("plan", "--form binary --key C --meter 4/4 --out /nonexistent-directory/plan.json", 3,
                   "cannot write '/nonexistent-directory/plan.json'");
    assert(!exists("plan_cli_bad.json"));
}

static void test_fill_errors(void) {
    expect_failure("fill", "", 1, "fill needs a plan file");
    expect_failure("fill", "plan_cli_binary.json --bogus", 1, "unknown option '--bogus'");
    expect_failure("fill", "plan_cli_binary.json plan_cli_ternary.json", 1, "unexpected argument");
    expect_failure("fill", "plan_cli_binary.json --seed -1", 1, "--seed must be");
    expect_failure("fill", "plan_cli_missing.json", 2, "cannot read plan 'plan_cli_missing.json'");
    write_text("plan_cli_malformed.json", "{\"schema_version\":");
    expect_failure("fill", "plan_cli_malformed.json", 2, "cannot read plan 'plan_cli_malformed.json'");
    /* An analysis document describes a piece already written; it points at the loader. */
    expect_failure("fill", FIXTURE("analysis_small.json"), 2, "plan_read_analysis_document_file");
    expect_failure("fill", "plan_cli_binary.json --seed 1 --midi /nonexistent-directory/a.mid --out plan_cli_none.json", 3,
                   "cannot write '/nonexistent-directory/a.mid'");
    /* Nothing is left behind by a write that failed. */
    assert(!exists("plan_cli_none.json"));
    assert(!exists("plan_cli_none.json.tmp"));
}

/*
 * A plan no seed completes: a loop in D minor in 4/4 is stuck at the harmony stage from every one of the 16 seeds the
 * command tries (measured, as B5 records in docs/piece.md). The targets left open are named with the reason, and no file
 * is written.
 */
static void test_stuck(void) {
    free(output_of("plan", "--form loop --key D --mode minor --meter 4/4 --seed 1 --out plan_cli_stuck.json"));
    expect_failure("fill", "plan_cli_stuck.json --seed 1 --midi plan_cli_stuck.mid --out plan_cli_stuck_done.json", 2,
                   "could not be completed from seeds 1 to 16: stuck at the harmony stage");
    char *err = read_file(ERR, NULL);
    assert(strstr(err, "  open: ") != NULL);
    assert(strstr(err, "no operator applies") != NULL);
    assert(strstr(err, "every candidate was rejected") != NULL);
    free(err);
    assert(!exists("plan_cli_stuck.mid"));
    assert(!exists("plan_cli_stuck_done.json"));
}

int main(void) {
    test_every_form();
    test_same_seed();
    test_grow_plan();
    test_options();
    test_plan_errors();
    test_fill_errors();
    test_stuck();
    return 0;
}
