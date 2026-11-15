#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "support/test_support.h"

/*
 * The `motive` command, run as a process: what it prints for each input format, and the exit status and message
 * for bad arguments and bad input.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" motive %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file, err_file);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    assert(run(arguments, "motive_cli_out.txt", "motive_cli_err.txt") == status);
    char *err = test_read_file("motive_cli_err.txt", NULL);
    if (strstr(err, message) == NULL) {
        fprintf(stderr, "'%s': expected '%s' in: %s\n", arguments, message, err);
    }
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    assert(out[0] == '\0');
    free(out);
}

static void test_text_output(void) {
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\"", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Notes: D4 F4 A4 D5") != NULL);
    assert(strstr(out, "Meter: 4/4") != NULL);
    assert(strstr(out, "Key: D minor (given)") != NULL);
    assert(strstr(out, "Contour: ascending (up, up, up)") != NULL);
    assert(strstr(out, "starts on 1, ends on 1") != NULL);
    assert(strstr(out, "1. i (tonic) fits 100%") != NULL);
    char *err = test_read_file("motive_cli_err.txt", NULL);
    assert(err[0] == '\0');
    free(err);

    /* The files carry the meter and the key themselves, and read as the same motive. */
    assert(run(FIXTURE("motive_d_minor.mid"), "motive_cli_mid.txt", "motive_cli_err.txt") == 0);
    char *from_midi = test_read_file("motive_cli_mid.txt", NULL);
    assert(strcmp(from_midi, out) == 0);
    free(from_midi);
    free(out);

    /* Without a key the output says so and gives semitones only. */
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Key: unspecified") != NULL);
    assert(strstr(out, "Implied harmony: unknown (no key)") != NULL);
    free(out);
}

static void test_json_output(void) {
    assert(run(FIXTURE("motive_d_minor.json") " --json", "motive_cli_json.txt", "motive_cli_err.txt") == 0);
    char *json = test_read_file("motive_cli_json.txt", NULL);
    assert(strstr(json, "\"analysis\"") != NULL && strstr(json, "\"shape\": \"ascending\"") != NULL);
    assert(strstr(json, "\"roman_numeral\": \"i\"") != NULL);
    assert(json[strlen(json) - 1] == '\n');

    /* The same motive from every format gives the same bytes. */
    const char *others[] = {FIXTURE("motive_d_minor.mid") " --json", FIXTURE("motive_d_minor.musicxml") " --json",
                            FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" --json",
                            FIXTURE("motive_d_minor.mid") " --format midi --json"};
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++) {
        assert(run(others[i], "motive_cli_other.txt", "motive_cli_err.txt") == 0);
        char *other = test_read_file("motive_cli_other.txt", NULL);
        assert(strcmp(other, json) == 0);
        free(other);
    }
    free(json);
}

static void test_bad_arguments(void) {
    expect_failure("", 1, "needs an input file");
    expect_failure(FIXTURE("motive_d_minor.mid") " --meter 9/5", 1, "meter '9/5'");
    expect_failure(FIXTURE("motive_d_minor.mid") " --key \"H major\"", 1, "unknown key 'H major'");
    expect_failure(FIXTURE("motive_d_minor.mid") " --format xml", 1, "--format must be");
    expect_failure(FIXTURE("motive_d_minor.mid") " --nope", 1, "unknown option '--nope'");
    expect_failure(FIXTURE("motive_d_minor.mid") " --meter", 1, "--meter needs a value");
    expect_failure(FIXTURE("motive_d_minor.mid") " " FIXTURE("motive_d_minor.txt"), 1, "unexpected argument");
}

static void test_bad_input(void) {
    expect_failure(FIXTURE("motive_d_minor.txt"), 2, "at meter");
    expect_failure(FIXTURE("motive_d_minor.mid") " --meter 3/4", 2, "conflicts");
    expect_failure(FIXTURE("motive_polyphonic.mid"), 2, "polyphonic");
    expect_failure(FIXTURE("motive_polyphonic.musicxml"), 2, "one voice");
    expect_failure(FIXTURE("motive_truncated.mid"), 2, "midi");
    expect_failure(FIXTURE("motive_malformed.musicxml"), 2, "musicxml");
    expect_failure(FIXTURE("motive_d_minor.txt") " --format midi", 2, "cannot load");
    expect_failure(FIXTURE("no_such_file.mid"), 2, "cannot open");
}

static void write_input(const char *text) {
    FILE *file = fopen("motive_cli_in.txt", "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static char *run_json(const char *arguments) {
    assert(run(arguments, "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    return test_read_file("motive_cli_out.txt", NULL);
}

static void test_forms_and_pickup(void) {
    /* Text output names the form and the pickup. */
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\"", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Form: full") != NULL && strstr(out, "Pickup: none") != NULL);
    free(out);

    /* A degree list. */
    write_input("1:q b3:e 5:e 1':h");
    out = run_json("motive_cli_in.txt --format degrees --meter 4/4 --key \"D minor\" --json");
    assert(strstr(out, "\"form\": \"full\"") && strstr(out, "\"pickup\": \"0\""));
    assert(strstr(out, "\"notes_text\": \"D4:q E4:e A4:e D5:h\"") && strstr(out, "\"degrees_text\": \"1:q 2:e 5:e 1':h\""));
    assert(!strstr(out, "\"rhythm_text\"") && !strstr(out, "\"contour_text\""));
    free(out);

    /* Without a key there is no degree text. */
    write_input("D4:q F4:q");
    out = run_json("motive_cli_in.txt --meter 4/4 --json");
    assert(strstr(out, "\"degrees_text\": null") && strstr(out, "\"notes_text\": \"D4:q F4:q\""));
    free(out);

    /* --form rhythm reads a rhythm; the pickup is in beats of the meter. */
    write_input("x:q x:h.");
    out = run_json("motive_cli_in.txt --form rhythm --meter 3/4 --pickup 1 --json");
    assert(strstr(out, "\"form\": \"rhythm\"") && strstr(out, "\"pickup\": \"1\"") &&
           strstr(out, "\"rhythm_text\": \"x:q x:h.\"") && strstr(out, "\"intervals\": null"));
    assert(strstr(out, "\"bar\": -1"));
    free(out);
    assert(run("motive_cli_in.txt --form rhythm --meter 3/4 --pickup 1", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Form: rhythm") && strstr(out, "Pickup: 1") && strstr(out, "bar 0 beat 3"));
    free(out);

    /* --form contour reads a contour; a half beat pickup is "1/2". */
    write_input("0 2 1 3");
    out = run_json("motive_cli_in.txt --form contour --meter 4/4 --pickup 1/2 --key \"C major\" --json");
    assert(strstr(out, "\"form\": \"contour\"") && strstr(out, "\"pickup\": \"1/2\"") &&
           strstr(out, "\"contour_text\": \"0 2 1 3\"") && strstr(out, "\"rhythm\": null"));
    free(out);

    /* --format rhythm and --form rhythm are the same; full leaves the format alone. */
    write_input("x:q x:q");
    out = run_json("motive_cli_in.txt --format rhythm --meter 4/4 --json");
    assert(strstr(out, "\"form\": \"rhythm\""));
    free(out);
    write_input("D4:q F4:q");
    out = run_json("motive_cli_in.txt --form full --meter 4/4 --json");
    assert(strstr(out, "\"form\": \"full\""));
    free(out);

    /* A file that declares its own meter takes the pickup in that meter's beats. */
    out = run_json(FIXTURE("motive_d_minor.mid") " --pickup 1/2 --json");
    assert(strstr(out, "\"pickup\": \"1/2\""));
    free(out);
}

static void test_forms_and_pickup_errors(void) {
    expect_failure(FIXTURE("motive_d_minor.mid") " --form chord", 1, "--form must be");
    expect_failure(FIXTURE("motive_d_minor.mid") " --form rhythm --format contour", 1, "--form rhythm reads a rhythm");
    expect_failure(FIXTURE("motive_d_minor.mid") " --form full --format rhythm", 1, "--form full does not read");
    expect_failure(FIXTURE("motive_d_minor.mid") " --pickup x", 1, "pickup 'x'");
    expect_failure(FIXTURE("motive_d_minor.mid") " --pickup -1", 1, "pickup '-1'");
    expect_failure(FIXTURE("motive_d_minor.mid") " --pickup 1/0", 1, "pickup '1/0'");
    expect_failure(FIXTURE("motive_d_minor.mid") " --pickup", 1, "--pickup needs a value");
    expect_failure(FIXTURE("motive_d_minor.mid") " --pickup 4", 2, "pickup must be");
    write_input("0 a");
    expect_failure("motive_cli_in.txt --form contour --meter 4/4", 2, "not a rank");
    write_input("1:q 9:q");
    expect_failure("motive_cli_in.txt --format degrees --meter 4/4", 2, "not a scale degree");
}

static void test_render(void) {
    /* A 3/4 motive with a one-beat pickup: measure 0 is implicit and holds the pickup, measure 1 the full bar. */
    remove("motive_cli_out.xml");
    remove("motive_cli_out.mid");
    write_input("1:q 3:h.");
    char *out = run_json("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --pickup 1 "
                         "--musicxml motive_cli_out.xml --midi motive_cli_out.mid --tempo 90 --json");
    assert(strstr(out, "\"rendered\"") && strstr(out, "\"key\": \"D minor\""));
    assert(strstr(out, "\"midi_pitch\": 62") && strstr(out, "\"midi_pitch\": 65"));
    free(out);
    char *xml = test_read_file("motive_cli_out.xml", NULL);
    char *zero = strstr(xml, "<measure number=\"0\" implicit=\"yes\">");
    char *one = strstr(xml, "<measure number=\"1\">");
    assert(zero && one && zero < one && !strstr(xml, "<measure number=\"2\">"));
    assert(strstr(xml, "<beats>1</beats>") && strstr(xml, "<beats>3</beats>") && strstr(xml, "<sound tempo=\"89.99"));
    char *duration = strstr(zero, "<duration>");
    assert(duration && duration < one && strncmp(duration, "<duration>1</duration>", 22) == 0);
    duration = strstr(one, "<duration>");
    assert(duration && strncmp(duration, "<duration>3</duration>", 22) == 0);
    free(xml);
    FILE *midi = fopen("motive_cli_out.mid", "rb");
    assert(midi);
    char header[4];
    assert(fread(header, 1, 4, midi) == 4 && memcmp(header, "MThd", 4) == 0);
    fclose(midi);

    /* No pickup: no implicit measure. */
    write_input("1:q 3:h.");
    out = run_json("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --musicxml motive_cli_out.xml --json");
    assert(strstr(out, "\"rendered\""));
    free(out);
    xml = test_read_file("motive_cli_out.xml", NULL);
    assert(!strstr(xml, "implicit") && strstr(xml, "<measure number=\"1\">"));
    free(xml);

    /* The rendered notes are in the render key, with offsets from the first note; the analysis stays in the source's. */
    out = run_json("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --render-key \"G minor\" --json");
    assert(strstr(out, "\"key\": \"G minor\"") && strstr(out, "\"midi_pitch\": 67") && strstr(out, "\"midi_pitch\": 70"));
    assert(strstr(out, "\"tonic_pitch_class\": 2"));
    free(out);

    /* Without a render option there is no rendered object. */
    out = run_json("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --json");
    assert(!strstr(out, "\"rendered\""));
    free(out);

    expect_failure("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --render-key H", 1, "unknown key");
    expect_failure("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --tempo 5 --midi motive_cli_out.mid", 1,
                   "--tempo");
    write_input("D4:q F4:q");
    expect_failure("motive_cli_in.txt --meter 4/4 --render-key \"G minor\"", 2, "it has no key");
    write_input("1:q 3:h.");
    expect_failure("motive_cli_in.txt --format degrees --meter 3/4 --key \"D minor\" --midi /nonexistent/dir/o.mid", 3,
                   "cannot write");
}

static void test_help(void) {
    assert(run("--help", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "--meter") != NULL && strstr(out, "--format") != NULL && strstr(out, "--json") != NULL);
    assert(strstr(out, "--form") != NULL && strstr(out, "--pickup") != NULL);
    assert(strstr(out, "--render-key") != NULL && strstr(out, "--midi") != NULL && strstr(out, "--musicxml") != NULL);
    free(out);
}

/* ------------------------------------------- harmony, features and role */

/* Runs another subcommand; the output goes to the files of this test. Returns the exit status. */
static int run_sub(const char *subcommand, const char *arguments) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" %s %s >motive_cli_out.txt 2>motive_cli_err.txt", COMPOSER_CLI_PATH,
             subcommand, arguments);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static void expect_sub_failure(const char *subcommand, const char *arguments, int status, const char *message) {
    assert(run_sub(subcommand, arguments) == status);
    char *err = test_read_file("motive_cli_err.txt", NULL);
    if (strstr(err, message) == NULL) {
        fprintf(stderr, "%s '%s': expected '%s' in: %s\n", subcommand, arguments, message, err);
    }
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
}

#define EXTRAS " --harmony \"I V6@2\" --feature a:0-1 --feature b:2-3 --role accompaniment"

static void test_extras_in_motive(void) {
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\"" EXTRAS, "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Own harmony: I V6@2") != NULL);
    assert(strstr(out, "  a: notes 0 to 1") != NULL && strstr(out, "  b: notes 2 to 3") != NULL);
    assert(strstr(out, "Role: accompaniment") != NULL);
    free(out);

    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" --json" EXTRAS, "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "\"harmony\": \"I V6@2\"") != NULL && strstr(out, "\"harmony_text\": \"I V6@2\"") != NULL);
    assert(strstr(out, "\"label\": \"b\"") != NULL && strstr(out, "\"role\": \"accompaniment\"") != NULL);
    free(out);

    /* Nothing asked, nothing printed. */
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" --json", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "harmony_text") == NULL && strstr(out, "\"role\": \"accompaniment\"") == NULL && strstr(out, "\"features\"") == NULL);
    free(out);

    /* The onsets are in beats of the file's own meter; a pickup given at the same time still applies. */
    assert(run(FIXTURE("motive_d_minor.mid") " --harmony \"I V@2\" --pickup 1/2", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Own harmony: I V@2") != NULL && strstr(out, "Pickup: 1/2") != NULL);
    free(out);

    /* An empty text clears the harmony. */
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" --harmony \"\"", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "Own harmony") == NULL);
    free(out);
}

static void test_extras_errors(void) {
    const char *input = FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" ";
    char arguments[512];
    struct {
        const char *extra;
        int status;
        const char *message;
    } cases[] = {
        {"--harmony \"I V6\"", 1, "--harmony 'I V6' at token 2 (column 3)"},
        {"--harmony \"I iv@1\"", 1, "token 2 (column 3)"},
        {"--harmony \"I V@2 I@1\"", 1, "must come after"},
        {"--harmony \"I V@2\" --harmony \"I\"", 1, "--harmony given twice"},
        {"--harmony", 1, "--harmony needs a value"},
        {"--harmony \"I V@4\"", 2, "harmony[1]"},
        {"--feature a", 1, "--feature 'a'"},
        {"--feature a:3-1", 1, "runs backwards"},
        {"--feature abcdefghi:1", 1, "1 to 8 characters"},
        {"--feature a:0-1 --feature a:2-3", 1, "label 'a' is used twice"},
        {"--feature a:0-4", 2, "features[0]"},
        {"--feature", 1, "--feature needs a value"},
        {"--role bass", 1, "--role must be melodic or accompaniment, not 'bass'"},
        {"--role melodic --role accompaniment", 1, "--role given twice"},
        {"--feature a:0 --feature b:0 --feature c:0 --feature d:0 --feature e:0 --feature f:0 --feature g:0 "
         "--feature h:0 --feature i:0",
         1, "at most 8 features"},
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        snprintf(arguments, sizeof(arguments), "%s%s", input, cases[c].extra);
        expect_failure(arguments, cases[c].status, cases[c].message);
    }
}

static void write_chain(const char *text) {
    FILE *file = fopen("motive_cli_chain.json", "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static void test_extras_in_variants(void) {
    const char *input = FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\"" EXTRAS;
    char arguments[1024];
    write_chain("{\"steps\": [{\"op\": \"fragment\", \"params\": {\"position\": \"trailing\", \"length\": 2}}]}");
    snprintf(arguments, sizeof(arguments), "%s --chain motive_cli_chain.json --count 1 --seed 3 --json", input);
    assert(run_sub("variants", arguments) == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    /* The source keeps all of it. The fragment keeps the feature it holds whole, the chords that land on its notes (the
       one sounding at its start at 0), and the role. */
    assert(strstr(out, "\"harmony\": \"I V6@2\"") != NULL);
    assert(strstr(out, "\"harmony\": \"I V6@1/2\"") != NULL);
    char *fragment = strstr(out, "\"variants\"");
    assert(fragment != NULL);
    assert(strstr(fragment, "\"label\": \"b\"") != NULL && strstr(fragment, "\"label\": \"a\"") == NULL);
    assert(strstr(fragment, "\"role\": \"accompaniment\"") != NULL);
    free(out);

    snprintf(arguments, sizeof(arguments), "%s --count 2 --seed 3", input);
    assert(run_sub("variants", arguments) == 0);
    expect_sub_failure("variants", FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" --harmony \"I V6\"", 1,
                       "token 2");
    expect_sub_failure("variants", FIXTURE("motive_d_minor.txt") " --meter 4/4 --role bass", 1, "--role must be");
    expect_sub_failure("variants", "--list-ops --role accompaniment", 1, "takes no motive input");
}

static void test_extras_in_grow_and_random_motive(void) {
    const char *grow = FIXTURE("motive_d_minor.txt") " --into sentence --key \"D minor\" --meter 4/4 --seed 1";
    char arguments[1024];
    snprintf(arguments, sizeof(arguments), "%s --harmony \"I V@2\" --feature a:0-1 --role melodic", grow);
    assert(run_sub("grow", arguments) == 0);
    snprintf(arguments, sizeof(arguments), "%s --harmony \"I V\"", grow);
    expect_sub_failure("grow", arguments, 1, "token 2");
    snprintf(arguments, sizeof(arguments), "%s --feature a:0-9", grow);
    expect_sub_failure("grow", arguments, 2, "features[0]");

    assert(run_sub("random-motive", "--mode major --meter 4/4 --seed 5 --count 2 --role accompaniment --json") == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    const char *first = strstr(out, "\"role\": \"accompaniment\"");
    assert(first != NULL && strstr(first + 1, "\"role\": \"accompaniment\"") != NULL);
    free(out);
    assert(run_sub("random-motive", "--mode major --meter 4/4 --seed 5 --role melodic --json") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "\"role\": \"accompaniment\"") == NULL);
    free(out);
    expect_sub_failure("random-motive", "--mode major --meter 4/4 --role bass", 1, "--role must be");
    expect_sub_failure("random-motive", "--mode major --meter 4/4 --harmony I", 1, "unknown option '--harmony'");
    expect_sub_failure("random-motive", "--mode major --meter 4/4 --feature a:0", 1, "unknown option '--feature'");
}

static void test_extras_help(void) {
    const char *commands[] = {"motive", "variants", "grow"};
    for (size_t c = 0; c < sizeof(commands) / sizeof(commands[0]); c++) {
        assert(run_sub(commands[c], "--help") == 0);
        char *out = test_read_file("motive_cli_out.txt", NULL);
        assert(strstr(out, "--harmony") != NULL && strstr(out, "--feature") != NULL && strstr(out, "--role") != NULL);
        free(out);
    }
    assert(run_sub("random-motive", "--help") == 0);
    char *out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "--role") != NULL && strstr(out, "--harmony") == NULL);
    free(out);
}

/* ------------------------------------------- base harmony and tone labels */

#define HOLLOW_KNIGHT "motive_cli_hollow_knight.txt"
#define HOLLOW_ARGS HOLLOW_KNIGHT " --meter 4/4 --key \"C major\""

static char *run_and_read(const char *arguments) {
    assert(run(arguments, "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    return test_read_file("motive_cli_out.txt", NULL);
}

static void test_base_harmony_and_tones(void) {
    test_write_file(HOLLOW_KNIGHT, "A5:q A5:q B5:e C6:e B5:h\n");

    /* Proposed: V holds B and B on the beats, as IV and vi hold A and A; the tie goes to the earlier. */
    char *out = run_and_read(HOLLOW_ARGS);
    assert(strstr(out, "Base harmony: V (proposed)") != NULL);
    assert(strstr(out, "Tones: 1 A5 unclassified, 2 A5 unclassified, 3 B5 chord, 4 C6 neighbor, 5 B5 chord") != NULL);
    assert(strstr(out, "Warning: Note 1 (A5) does not resolve") != NULL);
    free(out);

    out = run_and_read(HOLLOW_ARGS " --json");
    assert(strstr(out, "\"base_harmony\": {\n      \"text\": \"V\",\n      \"given\": false") != NULL);
    assert(strstr(out, "\"warnings\": [\"Note 1 (A5) does not resolve\"]") != NULL);
    assert(strstr(out, "\"type\": \"neighbor\",\n        \"resolves_to\": 5,\n        \"unresolved\": false,\n        \"overridden\": false") != NULL);
    assert(strstr(out, "\"overridden\": true") == NULL);
    free(out);

    /* A given harmony is the base harmony: over VI the B passes, and the last B has nothing after it. */
    out = run_and_read(HOLLOW_ARGS " --harmony VI");
    assert(strstr(out, "Base harmony: VI (given)") != NULL);
    assert(strstr(out, "Tones: 1 A5 chord, 2 A5 chord, 3 B5 passing, 4 C6 chord, 5 B5 unclassified") != NULL);
    assert(strstr(out, "Warning: Note 5 (B5) does not resolve") != NULL);
    free(out);
    out = run_and_read(HOLLOW_ARGS " --harmony VI --json");
    assert(strstr(out, "\"text\": \"VI\",\n      \"given\": true") != NULL);
    assert(strstr(out, "\"warnings\": [\"Note 5 (B5) does not resolve\"]") != NULL);
    free(out);

    /* A second chord inside the bar when it makes two beat notes chord tones. */
    test_write_file("motive_cli_two_chords.txt", "C5:q E5:q D5:q B4:q\n");
    out = run_and_read("motive_cli_two_chords.txt --meter 4/4 --key \"C major\"");
    assert(strstr(out, "Base harmony: I V@2 (proposed)") != NULL);
    assert(strstr(out, "Tones: 1 C5 chord, 2 E5 chord, 3 D5 chord, 4 B4 chord") != NULL);
    assert(strstr(out, "Warning") == NULL);
    free(out);

    /* A second bar gets its own chord, at its downbeat. */
    test_write_file("motive_cli_two_bars.txt", "C5:q E5:q G5:h F5:q A5:q C6:h\n");
    out = run_and_read("motive_cli_two_bars.txt --meter 4/4 --key \"C major\"");
    assert(strstr(out, "Base harmony: I IV@4 (proposed)") != NULL);
    free(out);

    /* No pitches, no base harmony. */
    test_write_file("motive_cli_rhythm.txt", "x:q x:e x:e x:h\n");
    out = run_and_read("motive_cli_rhythm.txt --meter 4/4 --form rhythm");
    assert(strstr(out, "Base harmony: none") != NULL && strstr(out, "Tones: none") != NULL);
    free(out);
    out = run_and_read("motive_cli_rhythm.txt --meter 4/4 --form rhythm --json");
    assert(strstr(out, "\"base_harmony\": null") != NULL && strstr(out, "\"tones\": []") != NULL);
    free(out);
}

static void test_too_many_harmonies_in_a_bar(void) {
    test_write_file(HOLLOW_KNIGHT, "A5:q A5:q B5:e C6:e B5:h\n");
    expect_failure(HOLLOW_ARGS " --harmony \"I V@1 I@2\"", 1, "at most 2 harmonies per bar");
    /* Two to a bar are fine, in two bars as well. */
    assert(run(HOLLOW_ARGS " --harmony \"I V@2\"", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    test_write_file("motive_cli_two_bars.txt", "C5:q E5:q G5:h F5:q A5:q C6:h\n");
    assert(run("motive_cli_two_bars.txt --meter 4/4 --key \"C major\" --harmony \"I V@2 IV@4 I@6\"", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    expect_failure("motive_cli_two_bars.txt --meter 4/4 --key \"C major\" --harmony \"I V@2 IV@3 I@4\"", 1,
                   "at most 2 harmonies per bar");
}

static void test_tone_overrides(void) {
    test_write_file(HOLLOW_KNIGHT, "A5:q A5:q B5:e C6:e B5:h\n");
    char *out = run_and_read(HOLLOW_ARGS " --harmony VI --tone 3=chord --tone 5=pedal");
    assert(strstr(out, "Tones: 1 A5 chord, 2 A5 chord, 3 B5 chord, 4 C6 chord, 5 B5 pedal") != NULL);
    free(out);
    out = run_and_read(HOLLOW_ARGS " --harmony VI --tone 3=chord --tone 5=escape --json");
    assert(strstr(out, "\"role\": \"chord\",\n        \"type\": null,\n        \"resolves_to\": null,\n        \"unresolved\": false,\n        \"overridden\": true") != NULL);
    assert(strstr(out, "\"type\": \"escape\"") != NULL);
    free(out);
    /* The later of two labels on one note wins. */
    out = run_and_read(HOLLOW_ARGS " --harmony VI --tone 3=chord --tone 3=suspension");
    assert(strstr(out, "3 B5 suspension") != NULL);
    free(out);

    expect_failure(HOLLOW_ARGS " --tone 0=chord", 1, "--tone '0=chord'");
    expect_failure(HOLLOW_ARGS " --tone 6=chord", 1, "1 to 5");
    expect_failure(HOLLOW_ARGS " --tone 3", 1, "--tone '3'");
    expect_failure(HOLLOW_ARGS " --tone 3=bogus", 1, "not 'bogus'");
    expect_failure(HOLLOW_ARGS " --tone", 1, "--tone needs a value");
    /* Nothing to label a rhythm against. */
    test_write_file("motive_cli_rhythm.txt", "x:q x:e x:e x:h\n");
    expect_failure("motive_cli_rhythm.txt --meter 4/4 --form rhythm --tone 1=chord", 1, "--tone needs a motive with a key");
    assert(run_sub("motive", "--help") == 0);
    out = test_read_file("motive_cli_out.txt", NULL);
    assert(strstr(out, "--tone") != NULL);
    free(out);
}

int main(void) {
    test_text_output();
    test_json_output();
    test_bad_arguments();
    test_bad_input();
    test_forms_and_pickup();
    test_forms_and_pickup_errors();
    test_render();
    test_help();
    test_extras_in_motive();
    test_extras_errors();
    test_extras_in_variants();
    test_extras_in_grow_and_random_motive();
    test_extras_help();
    test_base_harmony_and_tones();
    test_too_many_harmonies_in_a_bar();
    test_tone_overrides();
    puts("test_motive_cli: ok");
    return 0;
}
