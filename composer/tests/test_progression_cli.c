#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "io/midi_importer.h"
#include "io/musicxml_importer.h"
#include "pitch/pitch_numeric_value.h"

#include <galant_prototypes.h>

#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_json.h>

/*
 * The `progression` command, run as a process. What it wrote is read back from the files and judged by the same
 * framework recognizers and checkers the operators use, not by looking at the text it printed.
 */

typedef struct {
    const char *key;
    int tonic;
    PlanKeyMode mode;
    const char *meter;
    int bars;
    const char *cadence;
    PlanCadenceType cadence_type;
} Combination;

static const Combination COMBINATIONS[] = {
    {"C major", 0, PLAN_MODE_MAJOR, "4/4", 8, "PAC", PLAN_CADENCE_PAC},
    {"D minor", 2, PLAN_MODE_MINOR, "4/4", 8, "PAC", PLAN_CADENCE_PAC},
    {"F# major", 6, PLAN_MODE_MAJOR, "3/4", 6, "IAC", PLAN_CADENCE_IAC},
    {"Bb minor", 10, PLAN_MODE_MINOR, "6/8", 4, "HC", PLAN_CADENCE_HC},
    {"a minor", 9, PLAN_MODE_MINOR, "2/4", 5, "dc", PLAN_CADENCE_DC},
    {"Eb major", 3, PLAN_MODE_MAJOR, "4/4", 4, "PC", PLAN_CADENCE_PC},
    {"G major", 7, PLAN_MODE_MAJOR, "6/8", 12, "pac", PLAN_CADENCE_PAC},
    {"C# minor", 1, PLAN_MODE_MINOR, "3/4", 8, "HC", PLAN_CADENCE_HC},
};
#define COMBINATION_COUNT (sizeof(COMBINATIONS) / sizeof(COMBINATIONS[0]))

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" progression %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file,
             err_file);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static char *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *data = malloc((size_t)length + 1);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    data[length] = '\0';
    fclose(file);
    if (size) {
        *size = (size_t)length;
    }
    return data;
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
    }
    return file != NULL;
}

static bool same_file(const char *a, const char *b) {
    size_t size_a;
    size_t size_b;
    char *data_a = read_file(a, &size_a);
    char *data_b = read_file(b, &size_b);
    bool same = data_a && data_b && size_a == size_b && memcmp(data_a, data_b, size_a) == 0;
    free(data_a);
    free(data_b);
    return same;
}

static bool contains(const char *path, const char *text) {
    char *data = read_file(path, NULL);
    bool found = data && strstr(data, text) != NULL;
    free(data);
    return found;
}

/* The seed the command printed, from its text output. */
static unsigned long printed_seed(const char *out_file) {
    char *data = read_file(out_file, NULL);
    assert(data);
    const char *at = strstr(data, "\nseed ");
    assert(at);
    unsigned long seed = strtoul(at + 6, NULL, 10);
    free(data);
    return seed;
}

static PiecePlan *read_plan(const char *path) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file(path, &plan, &error) == PLAN_OK);
    return plan;
}

static size_t bass_note_count(const PiecePlan *plan, PlanSpan phrase) {
    LaneSpanId ids[64];
    size_t count = plan_lane_find(plan, PLAN_LANE_BASS, 0, phrase, ids, 64);
    size_t notes = 0;
    for (size_t i = 0; i < count; i++) {
        notes += plan_lane_span_payload(plan, ids[i])->notes.note_count;
    }
    return notes;
}

static CadenceType expected_heard(PlanCadenceType cadence) {
    switch (cadence) {
    case PLAN_CADENCE_PAC:
        return CADENCE_TYPE_PERFECT_AUTHENTIC;
    case PLAN_CADENCE_IAC:
        return CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    case PLAN_CADENCE_HC:
        return CADENCE_TYPE_HALF;
    case PLAN_CADENCE_DC:
        return CADENCE_TYPE_DECEPTIVE;
    case PLAN_CADENCE_PC:
        return CADENCE_TYPE_PLAGAL;
    }
    return CADENCE_TYPE_UNKNOWN;
}

/* The plan in the file ends in the cadence asked for (as the framework's recognizer hears it), its harmony passes
   standard-harmony and, with a bass, its bass passes the melodic rules. */
static PiecePlan *check_plan(const char *path, const Combination *combination, bool bass) {
    PiecePlan *plan = read_plan(path);
    PlanSpan phrase = plan_node_span(plan, plan_root(plan));
    CadenceType heard;
    bool at_end;
    const char *violation = "unread";
    assert(harmony_hear_cadence(plan, phrase, combination->mode, combination->cadence_type, &heard, &at_end));
    assert(at_end && heard == expected_heard(combination->cadence_type));
    assert(harmony_check_lane(plan, phrase, combination->mode, combination->cadence_type, &violation));
    assert(violation == NULL);
    if (bass) {
        PlanKey key = {combination->tonic, combination->mode};
        violation = "unread";
        assert(harmony_check_bass(plan, phrase, key, NULL, &violation));
        assert(violation == NULL);
    } else {
        assert(bass_note_count(plan, phrase) == 0);
    }
    return plan;
}

static void test_combinations(void) {
    for (size_t i = 0; i < COMBINATION_COUNT; i++) {
        const Combination *combination = &COMBINATIONS[i];
        char arguments[512];
        snprintf(arguments, sizeof(arguments),
                 "--key \"%s\" --meter %s --bars %d --cadence %s --seed %zu --out cli_plan.json", combination->key,
                 combination->meter, combination->bars, combination->cadence, i + 1);
        assert(run(arguments, "cli_out.txt", "cli_err.txt") == 0);
        assert(contains("cli_out.txt", "seed "));
        assert(contains("cli_out.txt", "bar  beat chord"));
        assert(contains("cli_out.txt", "wrote cli_plan.json"));
        PiecePlan *plan = check_plan("cli_plan.json", combination, true);
        plan_destroy(plan);
    }
}

static void test_same_seed_same_bytes(void) {
    const char *arguments = "--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 42 --embellish high "
                            "--out cli_a.json --midi cli_a.mid --musicxml cli_a.xml";
    assert(run(arguments, "cli_a.txt", "cli_a.err") == 0);
    arguments = "--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 42 --embellish high "
                "--out cli_b.json --midi cli_b.mid --musicxml cli_b.xml";
    assert(run(arguments, "cli_b.txt", "cli_b.err") == 0);
    assert(same_file("cli_a.json", "cli_b.json"));
    assert(same_file("cli_a.mid", "cli_b.mid"));
    assert(same_file("cli_a.xml", "cli_b.xml"));

    /* The printed seed reproduces the run, so a run without --seed can be repeated. */
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --embellish high --out cli_c.json", "cli_c.txt",
               "cli_c.err") == 0);
    char again[512];
    snprintf(again, sizeof(again),
             "--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --embellish high --seed %lu --out cli_d.json",
             printed_seed("cli_c.txt"));
    assert(run(again, "cli_d.txt", "cli_d.err") == 0);
    assert(same_file("cli_c.json", "cli_d.json"));
    assert(contains("cli_d.txt", "seed "));
}

static void test_other_seeds_can_differ(void) {
    int different = 0;
    assert(run("--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed 1 --embellish medium --out cli_s0.json",
               "cli_o.txt", "cli_e.txt") == 0);
    for (int seed = 2; seed <= 12; seed++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments),
                 "--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed %d --embellish medium --out cli_s1.json",
                 seed);
        assert(run(arguments, "cli_o.txt", "cli_e.txt") == 0);
        different += !same_file("cli_s0.json", "cli_s1.json");
    }
    assert(different > 0);
}

static void test_embellish_levels(void) {
    static const char *const LEVELS[4] = {"none", "low", "medium", "high"};
    Combination combination = COMBINATIONS[0];
    int changed = 0;
    for (int level = 0; level < 4; level++) {
        for (int seed = 1; seed <= 6; seed++) {
            char arguments[256];
            snprintf(arguments, sizeof(arguments),
                     "--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed %d --embellish %s --out cli_level.json",
                     seed, LEVELS[level]);
            assert(run(arguments, "cli_o.txt", "cli_e.txt") == 0);
            PiecePlan *plan = check_plan("cli_level.json", &combination, true);
            plan_destroy(plan);
            if (level == 0) {
                snprintf(arguments, sizeof(arguments),
                         "--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed %d --out cli_none.json", seed);
                assert(run(arguments, "cli_o.txt", "cli_e.txt") == 0);
                /* Without the option the progression is the one with --embellish none. */
                assert(same_file("cli_level.json", "cli_none.json"));
            } else if (level == 3) {
                snprintf(arguments, sizeof(arguments),
                         "--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed %d --out cli_none.json", seed);
                assert(run(arguments, "cli_o.txt", "cli_e.txt") == 0);
                changed += !same_file("cli_level.json", "cli_none.json");
            }
        }
    }
    assert(changed > 0);
}

static void test_no_bass_and_json_format(void) {
    Combination combination = COMBINATIONS[1];
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 3 --no-bass --format json "
               "--out cli_nobass.json",
               "cli_json.txt", "cli_e.txt") == 0);
    PiecePlan *plan = check_plan("cli_nobass.json", &combination, false);
    plan_destroy(plan);
    char *json = read_file("cli_json.txt", NULL);
    assert(json && json[0] == '{' && strstr(json, "\"seed\":3,") && strstr(json, "\"cadence\":\"PAC\"") &&
           strstr(json, "\"chords\":[") && strstr(json, "\"key\":\"D minor\""));
    free(json);
    /* The text form has no bass column. */
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 3 --no-bass", "cli_text.txt", "cli_e.txt") == 0);
    assert(contains("cli_text.txt", "function"));
    assert(!contains("cli_text.txt", "bass"));
}

static void test_midi_and_musicxml(void) {
    Combination combination = COMBINATIONS[1];
    remove("cli_files.mid");
    remove("cli_files.xml");
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 5 --embellish medium --out cli_files.json "
               "--midi cli_files.mid --musicxml cli_files.xml",
               "cli_o.txt", "cli_e.txt") == 0);
    PiecePlan *plan = check_plan("cli_files.json", &combination, true);
    size_t notes = bass_note_count(plan, plan_node_span(plan, plan_root(plan)));
    assert(notes > 0);
    plan_destroy(plan);

    /* Both files read back with Ursatz's own importers, with the bass notes of the plan. */
    size_t size = 0;
    unsigned char *data = (unsigned char *)read_file("cli_files.mid", &size);
    NoteEvent **events = NULL;
    PitchNumericValue **pitches = NULL;
    size_t count = 0;
    assert(data && midi_importer_import(data, size, &events, &pitches, &count) == MIDI_IMPORT_OK);
    assert(count == notes);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitches[i]);
    }
    free(events);
    free(pitches);
    free(data);

    MusicXmlImport imported;
    memset(&imported, 0, sizeof(imported));
    assert(musicxml_import_file("cli_files.xml", &imported) == MUSICXML_IMPORT_OK);
    assert(imported.count == notes);
    musicxml_import_destroy(&imported);
    assert(!file_exists("cli_files.mid.tmp") && !file_exists("cli_files.xml.tmp") && !file_exists("cli_files.json.tmp"));
}

typedef struct {
    const char *arguments;
    int status;
} ErrorCase;

static void test_errors(void) {
    static const ErrorCase CASES[] = {
        {"", 1},
        {"--key \"C major\" --meter 4/4 --bars 8", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --bogus", 1},
        {"--key \"H major\" --meter 4/4 --bars 8 --cadence PAC", 1},
        {"--key \"D dorian\" --meter 4/4 --bars 8 --cadence PAC", 1},
        {"--key D --meter 4/4 --bars 8 --cadence PAC", 1},
        {"--key \"D#b minor\" --meter 4/4 --bars 8 --cadence PAC", 1},
        {"--key \"C major\" --meter 5/4 --bars 8 --cadence PAC", 1},
        {"--key \"C major\" --meter 4-4 --bars 8 --cadence PAC", 1},
        {"--key \"C major\" --meter 4/4 --bars 0 --cadence PAC", 1},
        {"--key \"C major\" --meter 4/4 --bars -2 --cadence PAC", 1},
        {"--key \"C major\" --meter 4/4 --bars many --cadence PAC", 1},
        {"--key \"C major\" --meter 4/4 --bars 65 --cadence PAC", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence XYZ", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed -1", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed soon", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --embellish loud", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --format xml", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --no-bass --midi cli_err.mid", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --no-bass --musicxml cli_err.xml", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed", 1},
        {"--key \"C major\" --key \"D major\" --meter 4/4 --bars 8 --cadence PAC", 1},
        {"--key \"C major\" --meter 4/4 --bars 1 --cadence PAC", 2},
        {"--key \"C major\" --meter 3/4 --bars 2 --cadence HC --out cli_short.json", 2},
        {"--key \"C major\" --meter 3/4 --bars 4 --cadence PC --out cli_short.json", 2},
        {"--key \"C major\" --meter 6/8 --bars 2 --cadence PC --out cli_short.json", 2},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --out /no_such_directory/plan.json", 3},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --out cli_partial.json --midi /no_such_directory/x.mid", 3},
    };
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        remove("cli_short.json");
        remove("cli_partial.json");
        remove("cli_err.mid");
        remove("cli_err.xml");
        int status = run(CASES[i].arguments, "cli_o.txt", "cli_e.txt");
        if (status != CASES[i].status) {
            fprintf(stderr, "case %zu '%s': exit %d, wanted %d\n", i, CASES[i].arguments, status, CASES[i].status);
        }
        assert(status == CASES[i].status);
        /* An error says what is wrong on stderr, writes nothing on stdout, and leaves no file behind. */
        assert(contains("cli_e.txt", "Error:"));
        size_t out_size = 0;
        free(read_file("cli_o.txt", &out_size));
        assert(out_size == 0);
        assert(!file_exists("cli_short.json") && !file_exists("cli_partial.json") && !file_exists("cli_err.mid") &&
               !file_exists("cli_err.xml"));
        assert(!file_exists("cli_partial.json.tmp") && !file_exists("cli_short.json.tmp"));
    }
}

static void test_help(void) {
    static const char *const FLAGS[] = {"--key", "--meter", "--bars", "--cadence", "--seed", "--embellish", "--bass",
                                        "--no-bass", "--out", "--midi", "--musicxml", "--format", "--help", "Example:",
                                        "Exit status"};
    assert(run("--help", "cli_help.txt", "cli_e.txt") == 0);
    for (size_t i = 0; i < sizeof(FLAGS) / sizeof(FLAGS[0]); i++) {
        assert(contains("cli_help.txt", FLAGS[i]));
    }
    /* The command also appears in the program's own usage. */
    char command[1024];
    snprintf(command, sizeof(command), "\"%s\" --help >cli_usage.txt 2>&1", COMPOSER_CLI_PATH);
    assert(system(command) == 0);
    assert(contains("cli_usage.txt", "progression"));
}

/* The schema constraint of the first chord that has one, or -1. */
static int schema_in_plan(const PiecePlan *plan) {
    LaneSpanId ids[64];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, plan_root(plan)), ids, 64);
    for (size_t i = 0; i < count; i++) {
        for (size_t k = 0; k < plan_lane_span_constraint_count(plan, ids[i]); k++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, ids[i], k);
            if (constraint->kind == PLAN_CONSTRAINT_SCHEMA) {
                return constraint->params.schema.schema;
            }
        }
    }
    return -1;
}

/* The names under a heading of the --list-schemas output, up to the next blank line. */
static size_t listed_names(const char *heading, char names[][64], size_t capacity) {
    char *text = read_file("cli_list.txt", NULL);
    assert(text);
    char *at = strstr(text, heading);
    assert(at);
    at = strchr(at, '\n') + 1;
    size_t count = 0;
    while (*at == ' ' && count < capacity) {
        size_t length = 0;
        at += 2;
        while (at[length] && at[length] != ' ' && at[length] != '\n') {
            length++;
        }
        memcpy(names[count], at, length);
        names[count][length] = '\0';
        count++;
        at = strchr(at, '\n') + 1;
    }
    free(text);
    return count;
}

static void test_list_schemas(void) {
    char schemas[GALANT_SCHEMA_COUNT][64];
    char sequences[8][64];
    assert(run("--list-schemas", "cli_list.txt", "cli_e.txt") == 0);
    size_t schema_count = listed_names("Schemas", schemas, GALANT_SCHEMA_COUNT);
    size_t sequence_count = listed_names("Sequences", sequences, 8);
    assert(schema_count >= 5 && sequence_count == 5);
    assert(contains("cli_list.txt", "romanesca") && contains("cli_list.txt", "five-six-ascending"));
    assert(!contains("cli_list.txt", "modulating"));
    assert(!contains("cli_list.txt", "quiescenza"));

    /* Every name listed is accepted. A schema is used or, when this phrase has no room for it, refused with status
       2; the result is a valid progression either way. */
    Combination major = {"C major", 0, PLAN_MODE_MAJOR, "4/4", 8, "PAC", PLAN_CADENCE_PAC};
    int used = 0;
    for (size_t i = 0; i < schema_count; i++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments),
                 "--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --seed 4 --schema %s --out cli_schema.json",
                 schemas[i]);
        remove("cli_schema.json");
        int status = run(arguments, "cli_o.txt", "cli_e.txt");
        assert(status == 0 || status == 2);
        if (status == 0) {
            PiecePlan *plan = read_plan("cli_schema.json");
            int found = schema_in_plan(plan);
            assert(found >= 0 && strcmp(galant_schema_name((GalantSchemaId)found), schemas[i]) == 0);
            plan_destroy(plan);
            plan = check_plan("cli_schema.json", &major, true);
            plan_destroy(plan);
            used++;
        } else {
            assert(!file_exists("cli_schema.json"));
        }
    }
    assert(used >= 5);
}

static void test_schema_reproducible(void) {
    const char *arguments = "--key \"G major\" --meter 4/4 --bars 8 --cadence PAC --seed 9 --schema romanesca "
                            "--embellish medium --out cli_r1.json --midi cli_r1.mid";
    assert(run(arguments, "cli_r1.txt", "cli_e.txt") == 0);
    arguments = "--key \"G major\" --meter 4/4 --bars 8 --cadence PAC --seed 9 --schema romanesca "
                "--embellish medium --out cli_r2.json --midi cli_r2.mid";
    assert(run(arguments, "cli_r2.txt", "cli_e.txt") == 0);
    assert(same_file("cli_r1.json", "cli_r2.json") && same_file("cli_r1.mid", "cli_r2.mid"));
    PiecePlan *plan = read_plan("cli_r1.json");
    assert(schema_in_plan(plan) == GALANT_SCHEMA_ROMANESCA);
    plan_destroy(plan);
}

static void test_sequences(void) {
    static const char *const METERS[3] = {"4/4", "3/4", "6/8"};
    static const char *const NAMES[5] = {"parallel-chords", "five-six-ascending", "five-six-descending",
                                         "five-six-descending-alternating", "seven-six-descending"};
    for (int name = 0; name < 5; name++) {
        for (int meter = 0; meter < 3; meter++) {
            char arguments[256];
            snprintf(arguments, sizeof(arguments),
                     "--key \"D minor\" --meter %s --bars 9 --cadence PAC --seed 3 --sequence %s --out cli_seq.json",
                     METERS[meter], NAMES[name]);
            int status = run(arguments, "cli_o.txt", "cli_e.txt");
            assert(status == 0 || status == 2);
            if (status == 0) {
                /* The sequence's inverted chords have no reason of their own for the strict check, so the phrase
                   model and the cadence are checked, and the bass. */
                PiecePlan *plan = read_plan("cli_seq.json");
                PlanSpan phrase = plan_node_span(plan, plan_root(plan));
                CadenceType heard;
                bool at_end;
                const char *violation = "unread";
                assert(harmony_hear_cadence(plan, phrase, PLAN_MODE_MINOR, PLAN_CADENCE_PAC, &heard, &at_end));
                assert(at_end && heard == CADENCE_TYPE_PERFECT_AUTHENTIC);
                assert(harmony_check_lane_without_cadence(plan, phrase, PLAN_MODE_MINOR, &violation) && violation == NULL);
                PlanKey key = {2, PLAN_MODE_MINOR};
                violation = "unread";
                assert(harmony_check_bass(plan, phrase, key, NULL, &violation) && violation == NULL);
                plan_destroy(plan);
            }
        }
    }
    /* A sequence is offered in every meter, so at least one of these is used. */
    assert(run("--key \"D minor\" --meter 3/4 --bars 9 --cadence PAC --seed 3 --sequence parallel-chords", "cli_o.txt",
               "cli_e.txt") == 0);
}

static void test_suggest(void) {
    Combination combination = COMBINATIONS[1];
    remove("cli_sg-1.json");
    remove("cli_sg-2.json");
    remove("cli_sg-3.json");
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 6 --suggest 3 --embellish low "
               "--out cli_sg.json --midi cli_sg.mid",
               "cli_sg.txt", "cli_e.txt") == 0);
    assert(contains("cli_sg.txt", "alternative 1 of 3") && contains("cli_sg.txt", "alternative 3 of 3"));
    assert(!file_exists("cli_sg.json") && !file_exists("cli_sg.mid"));
    assert(file_exists("cli_sg-1.json") && file_exists("cli_sg-3.json") && file_exists("cli_sg-2.mid"));
    PiecePlan *plan = check_plan("cli_sg-1.json", &combination, true);
    plan_destroy(plan);
    plan = check_plan("cli_sg-2.json", &combination, true);
    plan_destroy(plan);
    plan = check_plan("cli_sg-3.json", &combination, true);
    plan_destroy(plan);
    /* Three distinct alternatives. */
    assert(!same_file("cli_sg-1.json", "cli_sg-2.json") && !same_file("cli_sg-1.json", "cli_sg-3.json") &&
           !same_file("cli_sg-2.json", "cli_sg-3.json"));

    /* The same request again gives the same alternatives. */
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 6 --suggest 3 --embellish low "
               "--out cli_sh.json",
               "cli_sh.txt", "cli_e.txt") == 0);
    assert(same_file("cli_sg-1.json", "cli_sh-1.json") && same_file("cli_sg-3.json", "cli_sh-3.json"));

    /* On stdout only, as text and as JSON. */
    assert(run("--key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 6 --suggest 2 --format json", "cli_sj.txt",
               "cli_e.txt") == 0);
    char *json = read_file("cli_sj.txt", NULL);
    assert(json && strstr(json, "\"alternatives\":[{") && strstr(json, "\"seed\":6,"));
    free(json);
}

static void test_skeleton_errors(void) {
    static const ErrorCase CASES[] = {
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --schema nonsense", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --sequence nonsense", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --schema romanesca --no-bass", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --sequence parallel-chords --no-bass", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --suggest 0", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --suggest 9", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --suggest many", 1},
        {"--key \"C major\" --meter 4/4 --bars 8 --cadence PAC --schema quiescenza", 2},
        {"--key \"C major\" --meter 3/4 --bars 8 --cadence PAC --schema romanesca", 2},
        {"--key \"C major\" --meter 4/4 --bars 2 --cadence PAC --schema romanesca", 2},
    };
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        int status = run(CASES[i].arguments, "cli_o.txt", "cli_e.txt");
        if (status != CASES[i].status) {
            fprintf(stderr, "case %zu '%s': exit %d, wanted %d\n", i, CASES[i].arguments, status, CASES[i].status);
        }
        assert(status == CASES[i].status);
        assert(contains("cli_e.txt", "Error:"));
        size_t out_size = 0;
        free(read_file("cli_o.txt", &out_size));
        assert(out_size == 0);
    }
}

int main(void) {
    test_list_schemas();
    test_schema_reproducible();
    test_sequences();
    test_suggest();
    test_skeleton_errors();
    test_combinations();
    test_same_seed_same_bytes();
    test_other_seeds_can_differ();
    test_embellish_levels();
    test_no_bass_and_json_format();
    test_midi_and_musicxml();
    test_errors();
    test_help();
    return 0;
}
