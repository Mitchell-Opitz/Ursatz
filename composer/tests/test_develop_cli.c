#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "io/midi_importer.h"
#include "io/musicxml_importer.h"
#include "pitch/pitch_numeric_value.h"

/*
 * The `develop` command, run as a process: what it prints for each operation, that it is reproducible from the seed,
 * the files it writes read back, and the exit status and message for bad arguments and for nothing to develop.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define THEME FIXTURE("motive_d_minor.txt") " --meter 4/4 --key \"D minor\""

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" develop %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file, err_file);
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

/* The standard output of a command that must succeed. */
static char *output_of(const char *arguments) {
    assert(run(arguments, "develop_cli_out.txt", "develop_cli_err.txt") == 0);
    char *err = read_file("develop_cli_err.txt");
    assert(err[0] == '\0');
    free(err);
    return read_file("develop_cli_out.txt");
}

static void expect_failure(const char *arguments, int status, const char *message) {
    assert(run(arguments, "develop_cli_out.txt", "develop_cli_err.txt") == status);
    char *err = read_file("develop_cli_err.txt");
    if (strstr(err, message) == NULL) {
        fprintf(stderr, "'%s': expected '%s' in: %s\n", arguments, message, err);
    }
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
}

static void test_each_operation(void) {
    char *out = output_of(THEME " --op restate --by 3 --suggest 1");
    assert(strstr(out, "Source: D4:q F4:e A4:e D5:h") != NULL);
    assert(strstr(out, "Operation: restate (seed 0)") != NULL);
    assert(strstr(out, "1. transpose +3 semitones  score 1/1 (100%)") != NULL);
    assert(strstr(out, "   F4:q G#4:e C5:e F5:h") != NULL);
    free(out);

    out = output_of(THEME " --op restate --degree 3 --suggest 1");
    assert(strstr(out, "transpose +2 steps (tonal)") != NULL && strstr(out, "   F4:q A4:e C5:e F5:h") != NULL);
    free(out);

    out = output_of(THEME " --op restate --tonal --by -2 --suggest 1");
    assert(strstr(out, "transpose -2 steps (tonal)") != NULL && strstr(out, "   A#3:q D4:e F4:e A#4:h") != NULL);
    free(out);

    out = output_of(THEME " --op invert --axis D4 --suggest 1");
    assert(strstr(out, "invert about 62") != NULL && strstr(out, "   D4:q B3:e G3:e D3:h") != NULL);
    free(out);

    out = output_of(THEME " --op retrograde");
    assert(strstr(out, "1. retrograde") != NULL && strstr(out, "   D5:h A4:e F4:e D4:q") != NULL);
    free(out);

    out = output_of(THEME " --op interval --interval 2 --amount 1");
    assert(strstr(out, "interval change +1 on interval 2") != NULL && strstr(out, "   D4:q F4:e A#4:e D#5:h") != NULL);
    free(out);

    out = output_of(THEME " --op fragment --position leading --length 3");
    assert(strstr(out, "fragment leading 3 notes") != NULL && strstr(out, "   D4:q F4:e A4:e") != NULL);
    free(out);

    out = output_of(THEME " --op liquidate --drop 1 --keep-rhythm");
    assert(strstr(out, "liquidate drop 1 (keep rhythm)") != NULL && strstr(out, "   D4:q F4:e r:e D5:h") != NULL);
    free(out);

    out = output_of(THEME " --op augment");
    assert(strstr(out, "augment 2:1") != NULL && strstr(out, "   D4:h F4:q A4:q D5:w") != NULL);
    free(out);

    out = output_of(THEME " --op diminish --ratio 1:2");
    assert(strstr(out, "diminish 1:2") != NULL && strstr(out, "   D4:e F4:s A4:s D5:q") != NULL);
    free(out);
}

/* The best first, the same for the same seed, and not the same for every seed. */
static void test_candidates_and_seeds(void) {
    char *three = output_of(THEME " --op restate --suggest 3 --seed 4");
    assert(strstr(three, "1. ") && strstr(three, "2. ") && strstr(three, "3. ") && !strstr(three, "4. "));
    unsigned long previous_num = 1;
    unsigned long previous_den = 0;
    for (const char *at = strstr(three, "score "); at; at = strstr(at + 1, "score ")) {
        unsigned long num;
        unsigned long den;
        assert(sscanf(at, "score %lu/%lu", &num, &den) == 2 && den >= 1 && num <= den);
        if (previous_den != 0) {
            assert(previous_num * den >= num * previous_den);
        }
        previous_num = num;
        previous_den = den;
    }
    char *again = output_of(THEME " --op restate --suggest 3 --seed 4");
    assert(strcmp(three, again) == 0);
    free(again);

    bool differs = false;
    for (int seed = 5; seed <= 12 && !differs; seed++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments), THEME " --op restate --suggest 3 --seed %d", seed);
        char *other = output_of(arguments);
        differs = strcmp(three, other) != 0;
        free(other);
    }
    assert(differs);
    free(three);

    /* Every operation with nothing set runs and says which one it ran. */
    static const char *const OPS[] = {"restate", "invert", "retrograde", "interval",
                                      "fragment", "liquidate", "augment", "diminish"};
    for (size_t i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments), THEME " --op %s --seed 2", OPS[i]);
        char *out = output_of(arguments);
        char expected[64];
        snprintf(expected, sizeof(expected), "Operation: %s (seed 2)", OPS[i]);
        assert(strstr(out, expected) != NULL && strstr(out, "1. ") != NULL);
        free(out);
    }
}

static void test_files(void) {
    remove("develop_cli.mid");
    remove("develop_cli.xml");
    char *out = output_of(THEME " --op retrograde --midi develop_cli.mid --musicxml develop_cli.xml");
    assert(strstr(out, "MIDI written to develop_cli.mid") != NULL && strstr(out, "MusicXML written to develop_cli.xml"));
    free(out);

    /* The source, then its retrograde. */
    static const long EXPECTED[8] = {62, 65, 69, 74, 74, 69, 65, 62};
    char *midi_bytes = read_file("develop_cli.mid");
    FILE *file = fopen("develop_cli.mid", "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fclose(file);
    NoteEvent **events = NULL;
    PitchNumericValue **values = NULL;
    size_t count = 0;
    assert(midi_importer_import((const unsigned char *)midi_bytes, size, &events, &values, &count) == MIDI_IMPORT_OK);
    assert(count == 8);
    for (size_t i = 0; i < count; i++) {
        assert(pitch_numeric_value_position_numerator(values[i]) == EXPECTED[i]);
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    free(events);
    free(values);
    free(midi_bytes);

    MusicXmlImport imported;
    memset(&imported, 0, sizeof(imported));
    assert(musicxml_import_file("develop_cli.xml", &imported) == MUSICXML_IMPORT_OK);
    assert(imported.count == 8);
    for (size_t i = 0; i < imported.count; i++) {
        assert(pitch_numeric_value_position_numerator(imported.pitch_values[i]) == EXPECTED[i]);
    }
    assert(imported.key_signature_count == 1 && imported.key_signatures[0].sharps_flats == -1);
    musicxml_import_destroy(&imported);
}

static void test_bad_arguments(void) {
    expect_failure("", 1, "needs an input file");
    expect_failure(THEME, 1, "needs --op");
    expect_failure(THEME " --op sing", 1, "unknown operation 'sing'");
    expect_failure(THEME " --op retrograde --tonal", 1, "--tonal does not apply to retrograde");
    expect_failure(THEME " --op fragment --ratio 2:1", 1, "--ratio does not apply to fragment");
    expect_failure(THEME " --op restate --by 0", 1, "--by must be");
    expect_failure(THEME " --op restate --degree 9", 1, "--degree must be");
    expect_failure(THEME " --op restate --degree 1", 1, "already on degree 1");
    expect_failure(THEME " --op restate --degree 3 --by 2", 1, "give one");
    expect_failure(THEME " --op augment --ratio 3", 1, "--ratio must be n:m");
    expect_failure(THEME " --op invert --axis H9", 1, "--axis must be");
    expect_failure(THEME " --op fragment --position side", 1, "--position must be");
    expect_failure(THEME " --op retrograde --seed x", 1, "--seed must be");
    expect_failure(THEME " --op retrograde --suggest 0", 1, "--suggest must be");
    expect_failure(FIXTURE("motive_d_minor.txt") " --meter 4/4 --op restate --degree 3", 1, "needs a key");
    expect_failure(FIXTURE("motive_d_minor.txt") " --meter 4/4 --op restate --tonal", 1, "needs a key");
    expect_failure(THEME " --op retrograde --key \"H major\"", 1, "unknown key");
    expect_failure(THEME " --op retrograde --nope", 1, "unknown option '--nope'");
    expect_failure(THEME " --op", 1, "--op needs a value");
}

static void test_nothing_to_develop(void) {
    expect_failure(FIXTURE("motive_d_minor.txt") " --op retrograde", 2, "(meter)");
    expect_failure(FIXTURE("no_such_file.mid") " --op retrograde", 2, "cannot load");
    expect_failure(FIXTURE("motive_polyphonic.mid") " --op retrograde", 2, "polyphonic");

    FILE *file = fopen("develop_cli_single.txt", "wb");
    assert(file);
    fputs("C4:q\n", file);
    fclose(file);
    const char *single = "develop_cli_single.txt --meter 4/4 --key \"C major\" ";
    char arguments[256];
    snprintf(arguments, sizeof(arguments), "%s--op interval", single);
    expect_failure(arguments, 2, "no interval development");
    snprintf(arguments, sizeof(arguments), "%s--op fragment", single);
    expect_failure(arguments, 2, "no fragment development");
    snprintf(arguments, sizeof(arguments), "%s--op liquidate", single);
    expect_failure(arguments, 2, "no liquidate development");
    snprintf(arguments, sizeof(arguments), "%s--op retrograde", single);
    expect_failure(arguments, 2, "no retrograde development");
    /* What one note can do. */
    snprintf(arguments, sizeof(arguments), "%s--op augment --suggest 1", single);
    char *out = output_of(arguments);
    assert(strstr(out, "   C4:h") != NULL);
    free(out);

    /* A fragment of three notes is asked of a motive with three: there is nothing shorter that is long enough. */
    expect_failure(THEME " --op fragment --length 4", 2, "no fragment development");
    expect_failure(THEME " --op fragment --length 2 --min 3", 2, "no fragment development");

    /* A development that cannot be written goes nowhere. */
    expect_failure(THEME " --op retrograde --midi /no/such/directory/out.mid", 3, "MIDI export failed");
}

static void test_help(void) {
    assert(run("--help", "develop_cli_out.txt", "develop_cli_err.txt") == 0);
    char *out = read_file("develop_cli_out.txt");
    assert(strstr(out, "--op") != NULL && strstr(out, "--suggest") != NULL && strstr(out, "--keep-rhythm") != NULL);
    free(out);
}

int main(void) {
    test_each_operation();
    test_candidates_and_seeds();
    test_files();
    test_bad_arguments();
    test_nothing_to_develop();
    test_help();
    puts("test_develop_cli: ok");
    return 0;
}
