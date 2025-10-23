#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/*
 * The `recognize` command, run as a process: a pair that is found (a transposition), a pair that is not found within the
 * depth, bad input (exit 1 with a message), the `--json` shape, and a contour pair.
 */

#define SOURCE "recognize_cli_source.txt"
#define TARGET "recognize_cli_target.txt"
#define MAJOR " --format notes --meter 4/4 --key \"C major\""

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" recognize %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file, err_file);
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

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

/* The standard output of a command that must succeed. */
static char *output_of(const char *arguments) {
    assert(run(arguments, "recognize_cli_out.txt", "recognize_cli_err.txt") == 0);
    char *err = read_file("recognize_cli_err.txt");
    assert(err[0] == '\0');
    free(err);
    return read_file("recognize_cli_out.txt");
}

static void expect_failure(const char *arguments, int status, const char *message) {
    assert(run(arguments, "recognize_cli_out.txt", "recognize_cli_err.txt") == status);
    char *err = read_file("recognize_cli_err.txt");
    if (strstr(err, message) == NULL) {
        fprintf(stderr, "'%s': expected '%s' in: %s\n", arguments, message, err);
    }
    assert(strstr(err, message) != NULL);
    free(err);
}

static void expect_contains(const char *text, const char *needle) {
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "expected '%s' in: %s\n", needle, text);
    }
    assert(strstr(text, needle) != NULL);
}

static void test_found(void) {
    write_file(SOURCE, "C4:q E4:e G4:e F4:q E4:q");
    write_file(TARGET, "D4:q F#4:e A4:e G4:q F#4:q");
    char *text = output_of(SOURCE " " TARGET MAJOR);
    expect_contains(text, "Chain: transpose");
    expect_contains(text, "Transposed +2 semitones");
    free(text);

    /* The same motive is exact. */
    text = output_of(SOURCE " " SOURCE MAJOR);
    expect_contains(text, "Chain: exact");
    expect_contains(text, "Restated exactly");
    free(text);
}

static void test_not_found(void) {
    write_file(SOURCE, "C4:q E4:e G4:e F4:q E4:q");
    write_file(TARGET, "G2:q B6:e");
    /* Not found is not an error: exit 0 and a derivation that says so. */
    char *text = output_of(SOURCE " " TARGET MAJOR " --depth 1");
    expect_contains(text, "No chain of up to 1 steps");
    free(text);
}

static void test_bad_input(void) {
    write_file(SOURCE, "C4:q E4:e G4:e F4:q E4:q");
    expect_failure(SOURCE " no_such_recognize_target.txt" MAJOR, 1, "cannot load 'no_such_recognize_target.txt'");
    expect_failure(SOURCE, 1, "recognize needs a source and a target motive");
    expect_failure(SOURCE " " SOURCE MAJOR " --depth 9", 1, "--depth must be a whole number from 1 to 4");
    expect_failure(SOURCE " " SOURCE " --format notes", 1, "meter");
}

static void test_json(void) {
    write_file(SOURCE, "C4:q E4:e G4:e F4:q E4:q");
    write_file(TARGET, "D4:q F#4:e A4:e G4:q F#4:q");
    char *text = output_of(SOURCE " " TARGET MAJOR " --json");
    expect_contains(text, "\"found\": true");
    expect_contains(text, "\"chain\": {");
    expect_contains(text, "\"steps\": [{");
    expect_contains(text, "\"op\": \"transpose\"");
    expect_contains(text, "\"params\": {");
    expect_contains(text, "\"by\": 2");
    expect_contains(text, "\"distance\": 0.2");
    expect_contains(text, "\"kept\": [\"rhythm\", \"intervals\", \"contour\"]");
    expect_contains(text, "\"changed\": [\"harmony\"]");
    expect_contains(text, "\"derivation\": \"Transposed +2 semitones.");
    free(text);

    write_file(TARGET, "G2:q B6:e");
    text = output_of(SOURCE " " TARGET MAJOR " --depth 1 --json");
    expect_contains(text, "\"found\": false");
    expect_contains(text, "\"steps\": []");
    expect_contains(text, "\"distance\": 1");
    free(text);
}

static void test_contour(void) {
    write_file(SOURCE, "0 2 1 3 2");
    write_file(TARGET, "0 2 1 2");
    char *text = output_of(SOURCE " " TARGET " --format contour --target-format contour --meter 4/4 --json");
    expect_contains(text, "\"found\": true");
    expect_contains(text, "\"op\": \"omit-interval\"");
    expect_contains(text, "\"interval\": 3");
    free(text);
}

int main(void) {
    test_found();
    test_not_found();
    test_bad_input();
    test_json();
    test_contour();
    printf("recognize cli: ok\n");
    return 0;
}
