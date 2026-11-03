#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include "support/test_support.h"

/*
 * The `counterpoint` command, run as a process on Fux's D Dorian cantus firmus: the text and the JSON it prints, the same
 * seed giving the same output, --suggest, the files it writes read back, and the exit codes.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define BASE FIXTURE("cantus_d_dorian.txt") " --key \"D minor\" --meter 4/4"

static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" counterpoint %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file,
             err_file);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static char *output_of(const char *arguments) {
    int status = run(arguments, "cp_cli_out.txt", "cp_cli_err.txt");
    if (status != 0) {
        char *err = test_read_file("cp_cli_err.txt", NULL);
        fprintf(stderr, "'%s' exited %d: %s\n", arguments, status, err);
        free(err);
    }
    assert(status == 0);
    return test_read_file("cp_cli_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    int got = run(arguments, "cp_cli_out.txt", "cp_cli_err.txt");
    char *err = test_read_file("cp_cli_err.txt", NULL);
    if (got != status || strstr(err, message) == NULL) {
        fprintf(stderr, "'%s': expected status %d with '%s', got %d: %s\n", arguments, status, message, got, err);
    }
    assert(got == status);
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static void test_text_and_json(void) {
    char *text = output_of(BASE " --species 2 --above --seed 4");
    char *again = output_of(BASE " --species 2 --above --seed 4");
    char *json;
    char *other;
    assert(strstr(text, "species 2 counterpoint above the line, key D minor, meter 4/4"));
    assert(strstr(text, "seed 4"));
    assert(strstr(text, "counter-melody:"));
    assert(strstr(text, "rules (standard-counterpoint, species 2):"));
    assert(strstr(text, "strong-beats") && strstr(text, "ends") && !strstr(text, "BROKEN"));
    assert(strcmp(text, again) == 0);
    other = output_of(BASE " --species 2 --above --seed 5");
    assert(strcmp(text, other) != 0);
    json = output_of(BASE " --species 3 --seed 4 --format json");
    assert(strncmp(json, "{\"species\":3,\"position\":\"below\",\"key\":\"D minor\",\"meter\":\"4/4\"", 60) == 0);
    assert(strstr(json, "\"counter_melody\":[{") && strstr(json, "\"hard_held\":true") && !strstr(json, "\"hard_held\":false"));
    free(text);
    free(again);
    free(other);
    free(json);
}

static void test_each_species(void) {
    for (int species = 1; species <= 4; species++) {
        char arguments[256];
        char *text;
        snprintf(arguments, sizeof(arguments), BASE " --species %d --seed 2", species);
        text = output_of(arguments);
        assert(strstr(text, "counter-melody:") && !strstr(text, "BROKEN"));
        free(text);
    }
}

static void test_suggest(void) {
    char *text = output_of(BASE " --species 1 --seed 1 --suggest 3");
    assert(strstr(text, "counter-melody 1 of 3, seed ") && strstr(text, "counter-melody 3 of 3, seed "));
    free(text);
}

static void test_files(void) {
    char *text = output_of(BASE " --species 2 --seed 3 --out cp_cli.json --midi cp_cli.mid --musicxml cp_cli.xml");
    PiecePlan *plan = NULL;
    PlanJsonError error;
    char *midi;
    LaneSpanId ids[4];
    PlanSpan all;
    assert(strstr(text, "wrote cp_cli.json") && strstr(text, "wrote cp_cli.mid") && strstr(text, "wrote cp_cli.xml"));
    assert(plan_read_json_file("cp_cli.json", &plan, &error) == PLAN_OK);
    rational_make(0, 1, &all.start);
    rational_make(11, 1, &all.length);
    /* The line and the counter-melody are in the plan, the counter-melody written. */
    assert(plan_lane_find(plan, PLAN_LANE_MELODY, 0, all, ids, 4) == 1);
    assert(plan_lane_find(plan, PLAN_LANE_INNER_VOICE, 1, all, ids, 4) == 1);
    assert(plan_lane_span_state(plan, ids[0]) == PLAN_STATE_REALIZED);
    assert(plan_lane_span_payload(plan, ids[0])->notes.note_count >= 11);
    plan_destroy(plan);
    /* The MIDI file holds the line and the counter-melody: two tracks of notes besides any of its own. */
    midi = test_read_file("cp_cli.mid", NULL);
    assert(strncmp(midi, "MThd", 4) == 0);
    {
        int tracks = ((unsigned char)midi[10] << 8) | (unsigned char)midi[11];
        assert(tracks >= 2);
    }
    free(midi);
    free(text);
}

static void test_failures(void) {
    write_text("cp_cli_halves.txt", "D4:h F4:h E4:w D4:w G4:w\n");
    write_text("cp_cli_unreadable.txt", "this is not a line\n");
    expect_failure(BASE " --species 5", 1, "--species must be 1, 2, 3 or 4");
    expect_failure(BASE " --species 2 --above --below", 1, "cannot both be given");
    expect_failure(FIXTURE("cantus_d_dorian.txt") " --species 2 --key \"D minor\"", 1, "needs --species, --key and --meter");
    expect_failure(BASE " --species 2 --suggest 0", 1, "--suggest must be a whole number");
    expect_failure(BASE " --species 2 --format xml", 1, "--format must be text or json");
    expect_failure(BASE " --species 2 --bogus", 1, "unknown option");
    expect_failure("cp_cli_missing.txt --species 1 --key \"D minor\" --meter 4/4", 2, "cannot load");
    expect_failure("cp_cli_unreadable.txt --species 1 --key \"D minor\" --meter 4/4", 2, "cannot load");
    /* A line that is not a cantus firmus, and a meter the species does not fit. */
    expect_failure("cp_cli_halves.txt --species 2 --key \"D minor\" --meter 4/4", 2,
                   "cannot be written as asked: species 2 to 4 need a cantus firmus");
    expect_failure(FIXTURE("cantus_d_dorian.txt") " --species 3 --key \"D minor\" --meter 3/4", 2,
                   "cannot be written as asked: third species is four notes to a bar");
}

int main(void) {
    test_text_and_json();
    test_each_species();
    test_suggest();
    test_files();
    test_failures();
    puts("ok");
    return 0;
}
