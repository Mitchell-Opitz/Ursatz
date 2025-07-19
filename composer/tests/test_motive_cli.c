#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
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
    return text;
}

static void expect_failure(const char *arguments, int status, const char *message) {
    assert(run(arguments, "motive_cli_out.txt", "motive_cli_err.txt") == status);
    char *err = read_file("motive_cli_err.txt");
    if (strstr(err, message) == NULL) {
        fprintf(stderr, "'%s': expected '%s' in: %s\n", arguments, message, err);
    }
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
    char *out = read_file("motive_cli_out.txt");
    assert(out[0] == '\0');
    free(out);
}

static void test_text_output(void) {
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\"", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    char *out = read_file("motive_cli_out.txt");
    assert(strstr(out, "Notes: D4 F4 A4 D5") != NULL);
    assert(strstr(out, "Meter: 4/4") != NULL);
    assert(strstr(out, "Key: D minor (given)") != NULL);
    assert(strstr(out, "Contour: ascending (up, up, up)") != NULL);
    assert(strstr(out, "starts on 1, ends on 1") != NULL);
    assert(strstr(out, "1. i (tonic) fits 100%") != NULL);
    char *err = read_file("motive_cli_err.txt");
    assert(err[0] == '\0');
    free(err);

    /* The files carry the meter and the key themselves, and read as the same motive. */
    assert(run(FIXTURE("motive_d_minor.mid"), "motive_cli_mid.txt", "motive_cli_err.txt") == 0);
    char *from_midi = read_file("motive_cli_mid.txt");
    assert(strcmp(from_midi, out) == 0);
    free(from_midi);
    free(out);

    /* Without a key the output says so and gives semitones only. */
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    out = read_file("motive_cli_out.txt");
    assert(strstr(out, "Key: unspecified") != NULL);
    assert(strstr(out, "Implied harmony: unknown (no key)") != NULL);
    free(out);
}

static void test_json_output(void) {
    assert(run(FIXTURE("motive_d_minor.json") " --json", "motive_cli_json.txt", "motive_cli_err.txt") == 0);
    char *json = read_file("motive_cli_json.txt");
    assert(strstr(json, "\"analysis\"") != NULL && strstr(json, "\"shape\": \"ascending\"") != NULL);
    assert(strstr(json, "\"roman_numeral\": \"i\"") != NULL);
    assert(json[strlen(json) - 1] == '\n');

    /* The same motive from every format gives the same bytes. */
    const char *others[] = {FIXTURE("motive_d_minor.mid") " --json", FIXTURE("motive_d_minor.musicxml") " --json",
                            FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\" --json",
                            FIXTURE("motive_d_minor.mid") " --format midi --json"};
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++) {
        assert(run(others[i], "motive_cli_other.txt", "motive_cli_err.txt") == 0);
        char *other = read_file("motive_cli_other.txt");
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
    return read_file("motive_cli_out.txt");
}

static void test_forms_and_pickup(void) {
    /* Text output names the form and the pickup. */
    assert(run(FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\"", "motive_cli_out.txt",
               "motive_cli_err.txt") == 0);
    char *out = read_file("motive_cli_out.txt");
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
    out = read_file("motive_cli_out.txt");
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

static void test_help(void) {
    assert(run("--help", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    char *out = read_file("motive_cli_out.txt");
    assert(strstr(out, "--meter") != NULL && strstr(out, "--format") != NULL && strstr(out, "--json") != NULL);
    assert(strstr(out, "--form") != NULL && strstr(out, "--pickup") != NULL);
    free(out);
}

int main(void) {
    test_text_output();
    test_json_output();
    test_bad_arguments();
    test_bad_input();
    test_forms_and_pickup();
    test_forms_and_pickup_errors();
    test_help();
    puts("test_motive_cli: ok");
    return 0;
}
