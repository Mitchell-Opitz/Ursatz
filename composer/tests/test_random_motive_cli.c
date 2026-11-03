#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "support/test_support.h"

/*
 * The `random-motive` command, run as a process: that the same seed gives the same output and another seed does not,
 * the text and JSON shapes in each form, a key of the mode asked for, a style, and the exit status and message for a
 * bad argument.
 */

#define STYLE COMPOSER_FIXTURE_DIR "/styles/chorales.style.json"

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" random-motive %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file,
             err_file);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

/* The standard output of a command that must succeed. */
static char *output_of(const char *arguments) {
    assert(run(arguments, "random_motive_cli_out.txt", "random_motive_cli_err.txt") == 0);
    char *err = test_read_file("random_motive_cli_err.txt", NULL);
    assert(err[0] == '\0');
    free(err);
    return test_read_file("random_motive_cli_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    assert(run(arguments, "random_motive_cli_out.txt", "random_motive_cli_err.txt") == status);
    char *err = test_read_file("random_motive_cli_err.txt", NULL);
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

/* The same options and seed give the same output, and another seed does not. */
static void test_seeds(void) {
    char *first = output_of("--mode minor --meter 3/4 --count 4 --seed 9");
    assert(strncmp(first, "Seed: 9\n", 8) == 0);
    assert(count_of(first, "\nMotive ") == 4 && count_of(first, "  degrees: ") == 4);
    char *again = output_of("--mode minor --meter 3/4 --count 4 --seed 9");
    assert(strcmp(first, again) == 0);
    free(again);

    bool differs = false;
    for (int seed = 10; seed <= 20 && !differs; seed++) {
        char arguments[128];
        snprintf(arguments, sizeof(arguments), "--mode minor --meter 3/4 --count 4 --seed %d", seed);
        char *other = output_of(arguments);
        differs = strcmp(first, other) != 0;
        free(other);
    }
    assert(differs);
    free(first);

    /* One motive by default, and a seed from the clock is printed. */
    char *one = output_of("--mode major --meter 4/4");
    assert(count_of(one, "\nMotive ") == 1 && strncmp(one, "Seed: ", 6) == 0);
    free(one);
}

static void test_forms(void) {
    char *rhythm = output_of("--mode major --meter 4/4 --form rhythm --count 3 --seed 4");
    assert(count_of(rhythm, "\nMotive ") == 3 && count_of(rhythm, "x:") >= 6);
    assert(strstr(rhythm, "degrees:") == NULL);
    free(rhythm);

    char *contour = output_of("--mode major --meter 4/4 --form contour --count 3 --seed 4");
    assert(count_of(contour, "\nMotive ") == 3 && strstr(contour, "x:") == NULL && strstr(contour, "degrees:") == NULL);
    free(contour);
}

static void test_json(void) {
    char *out = output_of("--mode minor --meter 3/4 --key \"D minor\" --count 2 --seed 3 --json");
    assert(strncmp(out, "{\n  \"seed\": 3,\n  \"motives\": [", 29) == 0);
    assert(count_of(out, "\"notes_text\"") == 2 && count_of(out, "\"degrees_text\"") == 2);
    assert(count_of(out, "\"tonic_pitch_class\": 2") == 4 && count_of(out, "\"mode\": \"minor\"") == 4);
    assert(count_of(out, "\"beats_per_bar\": 3") == 2);
    /* Each motive carries its base harmony (I, proposed) and a tone label for each note. */
    assert(count_of(out, "\"base_harmony\"") == 2 && count_of(out, "\"text\": \"I\"") >= 2);
    assert(count_of(out, "\"given\": false") >= 2 && count_of(out, "\"tones\"") == 2);
    assert(count_of(out, "\"role\": \"chord\"") >= 4 && count_of(out, "\"warnings\"") == 2);
    assert(count_of(out, "\"unresolved\": true") == 0);
    free(out);

    /* Over some seeds a figure is named. */
    out = output_of("--mode major --meter 4/4 --count 16 --seed 1 --json");
    assert(count_of(out, "\"role\": \"non-chord\"") > 0 && count_of(out, "\"unresolved\": true") == 0);
    free(out);

    out = output_of("--mode major --meter 4/4 --form rhythm --seed 3 --json");
    assert(count_of(out, "\"form\": \"rhythm\"") >= 1 && count_of(out, "\"rhythm_text\"") == 1);
    free(out);
}

static void test_style(void) {
    char *styled = output_of("--mode major --meter 4/4 --count 3 --seed 2 --style " STYLE);
    char *again = output_of("--mode major --meter 4/4 --count 3 --seed 2 --style " STYLE);
    char *plain = output_of("--mode major --meter 4/4 --count 3 --seed 2");
    assert(count_of(styled, "\nMotive ") == 3);
    assert(strcmp(styled, again) == 0 && strcmp(styled, plain) != 0);
    free(styled);
    free(again);
    free(plain);
}

static void test_bad_arguments(void) {
    expect_failure("--meter 4/4", 1, "random-motive needs --mode and --meter");
    expect_failure("--mode major", 1, "random-motive needs --mode and --meter");
    expect_failure("--mode dorian --meter 4/4", 1, "--mode must be major or minor, not 'dorian'");
    expect_failure("--mode major --meter 4", 1, "meter '4' is not a meter");
    expect_failure("--mode major --meter 4/4 --key \"D minor\"", 1, "key 'D minor' is not major");
    expect_failure("--mode major --meter 4/4 --key H", 1, "unknown key 'H'");
    expect_failure("--mode major --meter 4/4 --form chords", 1, "--form must be full, rhythm or contour");
    expect_failure("--mode major --meter 4/4 --count 0", 1, "--count must be a whole number from 1 to 16");
    expect_failure("--mode major --meter 4/4 --count 17", 1, "--count must be a whole number from 1 to 16");
    expect_failure("--mode major --meter 4/4 --seed x", 1, "--seed must be a whole number");
    expect_failure("--mode major --meter 4/4 --mode minor", 1, "--mode given twice");
    expect_failure("--mode major --meter 4/4 --length 4", 1, "unknown option '--length'");
    expect_failure("--mode major --meter 4/4 --style missing.style.json", 2, "cannot read style 'missing.style.json'");
}

int main(void) {
    test_seeds();
    test_forms();
    test_json();
    test_style();
    test_bad_arguments();
    printf("random-motive cli tests passed\n");
    return 0;
}
