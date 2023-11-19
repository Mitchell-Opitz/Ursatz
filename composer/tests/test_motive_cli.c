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

static void test_help(void) {
    assert(run("--help", "motive_cli_out.txt", "motive_cli_err.txt") == 0);
    char *out = read_file("motive_cli_out.txt");
    assert(strstr(out, "--meter") != NULL && strstr(out, "--format") != NULL && strstr(out, "--json") != NULL);
    free(out);
}

int main(void) {
    test_text_output();
    test_json_output();
    test_bad_arguments();
    test_bad_input();
    test_help();
    puts("test_motive_cli: ok");
    return 0;
}
