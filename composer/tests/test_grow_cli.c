#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <io/midi_importer.h>
#include <io/musicxml_importer.h>
#include <pitch/pitch_numeric_value.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include "support/test_support.h"

/*
 * The `grow` command, run as a process: each form and a hybrid from the D minor motive fixture, the files it writes
 * read back, the same seed giving the same output, the usage errors and exit codes, and --suggest giving different
 * themes.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define BASE FIXTURE("motive_d_minor.txt") " --key \"D minor\" --meter 4/4"

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *arguments, const char *out_file, const char *err_file) {
    char command[2048];
    snprintf(command, sizeof(command), "\"%s\" grow %s >%s 2>%s", COMPOSER_CLI_PATH, arguments, out_file, err_file);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

/* The standard output of a command that must succeed. */
static char *output_of(const char *arguments) {
    int status = run(arguments, "grow_cli_out.txt", "grow_cli_err.txt");
    if (status != 0) {
        char *err = test_read_file("grow_cli_err.txt", NULL);
        fprintf(stderr, "'%s' exited %d: %s\n", arguments, status, err);
        free(err);
    }
    assert(status == 0);
    return test_read_file("grow_cli_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    int got = run(arguments, "grow_cli_out.txt", "grow_cli_err.txt");
    char *err = test_read_file("grow_cli_err.txt", NULL);
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

/* The seed printed for the first theme. */
static unsigned long printed_seed(const char *out) {
    const char *at = strstr(out, "\nseed ");
    assert(at);
    return strtoul(at + 6, NULL, 10);
}

static void test_each_form(void) {
    static const struct {
        const char *arguments;
        const char *heading;
        const char *nodes[3];
    } FORMS[] = {
        {"--into sentence", "sentence (presentation + continuation)", {"presentation (subphrase)", "continuation (subphrase)", NULL}},
        {"--into period", "period (antecedent + consequent)", {"antecedent (phrase)", "consequent (phrase)", NULL}},
        {"--into hybrid --hybrid cbi+continuation", "hybrid (compound basic idea + continuation)",
         {"compound basic idea (subphrase)", "continuation (subphrase)", NULL}},
        {"--into hybrid --hybrid presentation+consequent", "hybrid (presentation + consequent)",
         {"presentation (subphrase)", "consequent (subphrase)", NULL}},
        {"--into hybrid --hybrid antecedent+cadential", "hybrid (antecedent + cadential)",
         {"antecedent (phrase)", "cadential (subphrase)", NULL}},
    };
    for (size_t i = 0; i < sizeof(FORMS) / sizeof(FORMS[0]); i++) {
        char arguments[512];
        snprintf(arguments, sizeof(arguments), BASE " %s --seed 3", FORMS[i].arguments);
        char *out = output_of(arguments);
        if (strstr(out, FORMS[i].heading) != &out[0]) {
            fprintf(stderr, "'%s': expected heading '%s' in: %s\n", arguments, FORMS[i].heading, out);
        }
        assert(strstr(out, FORMS[i].heading) == &out[0]);
        assert(strstr(out, "key D minor, meter 4/4, 8 bars, cadence PAC") != NULL);
        assert(strstr(out, "\nseed ") != NULL);
        assert(strstr(out, "theme (section) bars 0-8") != NULL && strstr(out, "basic idea (idea) bars 0-2") != NULL);
        for (size_t n = 0; FORMS[i].nodes[n]; n++) {
            assert(strstr(out, FORMS[i].nodes[n]) != NULL);
        }
        assert(strstr(out, "bar  beat chord") != NULL);
        /* The first chord is the tonic, in bar 1, and the theme ends on one. */
        assert(strstr(out, "\n1    1    i ") != NULL);
        assert(strstr(out, "motive developments (m0 is the motive given)\n  m1: ") != NULL);
        assert(strstr(out, "standard-form rule score ") != NULL);
        assert(strstr(out, "\nheard as ") != NULL || strstr(out, "\nno theme heard\n") != NULL);
        free(out);
    }

    /* A period says its weak cadence, and a weaker one can be asked for. */
    char *out = output_of(BASE " --into period --first-cadence IAC --seed 3");
    assert(strstr(out, "first cadence IAC") != NULL);
    free(out);
    out = output_of(BASE " --into period --seed 3");
    assert(strstr(out, "first cadence HC") != NULL);
    free(out);

    /* A sentence heard as one says so. */
    out = output_of(BASE " --into sentence --seed 8");
    assert(strstr(out, "\nheard as sentence\n") != NULL);
    free(out);
}

/* All seven pairings of the hybrids can be grown. */
static void test_every_hybrid(void) {
    static const char *const BEGINNINGS[] = {"presentation", "antecedent", "cbi"};
    static const char *const ENDINGS[] = {"continuation", "consequent", "cadential"};
    for (int b = 0; b < 3; b++) {
        for (int e = 0; e < 3; e++) {
            if ((b == 0 && e == 0) || (b == 1 && e == 1)) {
                continue;
            }
            char arguments[512];
            snprintf(arguments, sizeof(arguments), BASE " --into hybrid --hybrid %s+%s --seed 1", BEGINNINGS[b], ENDINGS[e]);
            char *out = output_of(arguments);
            assert(strstr(out, "standard-form rule score ") != NULL);
            free(out);
        }
    }
}

static void test_four_bars_and_cadences(void) {
    char *out = output_of(BASE " --into sentence --bars 4 --cadence IAC --seed 2");
    assert(strstr(out, "4 bars, cadence IAC") != NULL && strstr(out, "theme (section) bars 0-4") != NULL);
    free(out);
    write_text("grow_cli_waltz.txt", "F4:q A4:e G4:e E4:q\n");
    out = output_of("grow_cli_waltz.txt --into period --key \"D minor\" --meter 3/4 --seed 4");
    assert(strstr(out, "key D minor, meter 3/4") != NULL);
    free(out);
}

static void test_same_seed_same_output(void) {
    char *first = output_of(BASE " --into period --seed 11");
    char *again = output_of(BASE " --into period --seed 11");
    assert(strcmp(first, again) == 0);
    free(again);

    /* The seed that worked is printed, and giving it again gives the same theme. */
    unsigned long seed = printed_seed(first);
    assert(seed >= 11 && seed < 11 + 16);
    char arguments[256];
    snprintf(arguments, sizeof(arguments), BASE " --into period --seed %lu", seed);
    char *printed = output_of(arguments);
    assert(strcmp(first, printed) == 0 || seed != 11);
    char *from_printed = strstr(printed, "\nstructure");
    char *from_first = strstr(first, "\nstructure");
    assert(from_printed && from_first && strcmp(from_printed, from_first) == 0);
    free(printed);

    bool differs = false;
    for (int other = 12; other <= 20 && !differs; other++) {
        snprintf(arguments, sizeof(arguments), BASE " --into period --seed %d", other);
        char *out = output_of(arguments);
        differs = strcmp(strstr(out, "\nstructure"), from_first) != 0;
        free(out);
    }
    assert(differs);
    free(first);

    /* Without a seed one is chosen and printed. */
    char *chosen = output_of(BASE " --into sentence");
    assert(strstr(chosen, "\nseed ") != NULL);
    free(chosen);
}

static void test_files(void) {
    remove("grow_cli.json");
    remove("grow_cli.mid");
    remove("grow_cli.xml");
    char *out = output_of(BASE " --into sentence --seed 7 --out grow_cli.json --midi grow_cli.mid --musicxml grow_cli.xml");
    assert(strstr(out, "wrote grow_cli.json") != NULL && strstr(out, "wrote grow_cli.mid") != NULL &&
           strstr(out, "wrote grow_cli.xml") != NULL);
    free(out);

    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file("grow_cli.json", &plan, &error) == PLAN_OK);
    NodeId theme = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (name && strcmp(name, "theme") == 0) {
            theme = id;
        }
    }
    assert(theme != PLAN_NODE_NONE && plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    LaneSpanId spans[64];
    assert(plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), spans, 64) > 0);
    assert(plan_lane_find(plan, PLAN_LANE_BASS, 0, plan_node_span(plan, theme), spans, 64) > 0);
    plan_destroy(plan);

    /* The melody and the bass, eight bars of them: nothing of the source material. */
    char *midi_bytes = test_read_file("grow_cli.mid", NULL);
    FILE *file = fopen("grow_cli.mid", "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fclose(file);
    NoteEvent **events = NULL;
    PitchNumericValue **values = NULL;
    size_t count = 0;
    assert(midi_importer_import((const unsigned char *)midi_bytes, size, &events, &values, &count) == MIDI_IMPORT_OK);
    assert(count > 8);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    free(events);
    free(values);
    free(midi_bytes);

    MusicXmlImport imported;
    memset(&imported, 0, sizeof(imported));
    assert(musicxml_import_file("grow_cli.xml", &imported) == MUSICXML_IMPORT_OK);
    assert(imported.count == count);
    musicxml_import_destroy(&imported);
}

static void test_suggest(void) {
    char *out = output_of(BASE " --into sentence --seed 1 --suggest 3");
    assert(strstr(out, "theme 1 of 3") && strstr(out, "theme 2 of 3") && strstr(out, "theme 3 of 3") &&
           !strstr(out, "theme 4 of 3"));
    /* Three seeds, all different. */
    unsigned long seeds[3];
    const char *at = out;
    for (int k = 0; k < 3; k++) {
        at = strstr(at, "\nseed ");
        assert(at);
        seeds[k] = strtoul(at + 6, NULL, 10);
        at += 6;
    }
    assert(seeds[0] != seeds[1] && seeds[1] != seeds[2] && seeds[0] != seeds[2]);
    free(out);

    /* With files, theme k goes to <name>-k.<extension>, each a plan that reads back, and no two the same. */
    out = output_of(BASE " --into sentence --seed 1 --suggest 3 --out grow_cli_s.json --midi grow_cli_s.mid");
    free(out);
    char *plans[3];
    for (int k = 1; k <= 3; k++) {
        char path[64];
        snprintf(path, sizeof(path), "grow_cli_s-%d.json", k);
        PiecePlan *plan = NULL;
        PlanJsonError error;
        assert(plan_read_json_file(path, &plan, &error) == PLAN_OK);
        plan_destroy(plan);
        plans[k - 1] = test_read_file(path, NULL);
        snprintf(path, sizeof(path), "grow_cli_s-%d.mid", k);
        free(test_read_file(path, NULL));
    }
    assert(strcmp(plans[0], plans[1]) != 0 && strcmp(plans[1], plans[2]) != 0 && strcmp(plans[0], plans[2]) != 0);
    for (int k = 0; k < 3; k++) {
        free(plans[k]);
    }

    /* As JSON: one object, with the themes in it. */
    out = output_of(BASE " --into period --seed 5 --suggest 2 --format json");
    assert(out[0] == '{' && out[strlen(out) - 2] == '}' && out[strlen(out) - 1] == '\n');
    assert(strstr(out, "\"themes\":[{\"seed\":") != NULL && strstr(out, "\"into\":\"period\"") != NULL);
    assert(strstr(out, "\"first_cadence\":\"HC\"") != NULL && strstr(out, "\"heard_text\":") != NULL);
    assert(strstr(out, "\"chords\":[{\"bar\":1,\"beat\":1,\"chord\":\"i\"") != NULL);
    int seeds_seen = 0;
    for (const char *p = strstr(out, "{\"seed\":"); p; p = strstr(p + 1, "{\"seed\":")) {
        seeds_seen++;
    }
    assert(seeds_seen == 2);
    free(out);
}

static void test_bad_arguments(void) {
    expect_failure("", 1, "needs a motive input");
    expect_failure(FIXTURE("motive_d_minor.txt"), 1, "needs --into, --key and --meter");
    expect_failure(BASE, 1, "needs --into, --key and --meter");
    expect_failure(BASE " --into rondo", 1, "--into must be sentence, period or hybrid, not 'rondo'");
    expect_failure(BASE " --into hybrid", 1, "needs --hybrid");
    expect_failure(BASE " --into sentence --hybrid cbi+cadential", 1, "--hybrid applies only to --into hybrid");
    expect_failure(BASE " --into period --hybrid cbi+cadential", 1, "--hybrid applies only to --into hybrid");
    expect_failure(BASE " --into hybrid --hybrid cbi", 1, "--hybrid must be a beginning");
    expect_failure(BASE " --into hybrid --hybrid cbi+coda", 1, "--hybrid must be a beginning");
    expect_failure(BASE " --into hybrid --hybrid motto+cadential", 1, "--hybrid must be a beginning");
    expect_failure(BASE " --into hybrid --hybrid presentation+continuation", 1, "is a sentence, not a hybrid");
    expect_failure(BASE " --into hybrid --hybrid antecedent+consequent", 1, "is a period, not a hybrid");
    expect_failure(BASE " --into sentence --first-cadence HC", 1, "--first-cadence applies only");
    expect_failure(BASE " --into hybrid --hybrid cbi+cadential --first-cadence HC", 1, "--first-cadence applies only");
    expect_failure(BASE " --into period --first-cadence PAC", 1, "--first-cadence must be HC or IAC");
    expect_failure(BASE " --into sentence --cadence DC", 1, "--cadence must be PAC, IAC or HC");
    expect_failure(BASE " --into sentence --cadence perfect", 1, "--cadence must be PAC, IAC or HC");
    expect_failure(BASE " --into sentence --bars 2", 1, "--bars must be 3 to 16");
    expect_failure(BASE " --into sentence --bars x", 1, "--bars must be 3 to 16");
    expect_failure(BASE " --into sentence --seed x", 1, "--seed must be");
    expect_failure(BASE " --into sentence --suggest 0", 1, "--suggest must be a whole number from 1 to 16");
    expect_failure(BASE " --into sentence --suggest 17", 1, "--suggest must be a whole number from 1 to 16");
    expect_failure(BASE " --into sentence --format xml", 1, "--format must be text or json");
    expect_failure(FIXTURE("motive_d_minor.txt") " --into sentence --key \"H major\" --meter 4/4", 1, "unknown key");
    expect_failure(FIXTURE("motive_d_minor.txt") " --into sentence --key \"D minor\" --meter 5/4", 1, "not supported");
    expect_failure(FIXTURE("motive_d_minor.txt") " --into sentence --key \"D minor\" --meter x", 1, "not supported");
    expect_failure(BASE " --into sentence --nope", 1, "unknown option '--nope'");
    expect_failure(BASE " --into sentence --seed", 1, "--seed needs a value");
    expect_failure(BASE " --into sentence --seed 1 --seed 2", 1, "--seed given twice");
    expect_failure(BASE " " BASE " --into sentence", 1, "unexpected argument");
}

static void test_cannot_be_satisfied(void) {
    /* A motive longer than the basic idea (two bars of eight). */
    write_text("grow_cli_long.txt", "D4:w D4:w D4:q\n");
    expect_failure("grow_cli_long.txt --into sentence --key \"D minor\" --meter 4/4", 2, "the motif is 3 bars; a 8-bar theme takes a motif of up to 2 bars, or of 4 for its presentation or antecedent");
    /* One note is not a motive to grow a theme from. */
    write_text("grow_cli_single.txt", "D4:q\n");
    expect_failure("grow_cli_single.txt --into sentence --key \"D minor\" --meter 4/4", 2, "the basic motif needs at least two notes");
    /* An input that cannot be loaded as a motive. */
    expect_failure(FIXTURE("no_such_file.mid") " --into sentence --key \"D minor\" --meter 4/4", 2, "cannot load");
    expect_failure(FIXTURE("motive_polyphonic.mid") " --into sentence --key \"D minor\" --meter 4/4", 2, "polyphonic");
    write_text("grow_cli_long34.txt", "D4:w D4:w D4:q\n");
    expect_failure("grow_cli_long34.txt --into sentence --key \"D minor\" --meter 3/4", 2, "the motif is 3 bars; a 8-bar theme takes a motif of up to 2 bars, or of 4 for its presentation or antecedent");
    /* An antecedent cannot end more strongly than the theme. */
    expect_failure(BASE " --into period --first-cadence IAC --cadence IAC", 2, "an antecedent ends in a cadence weaker than the theme's");
    expect_failure(BASE " --into period --cadence HC", 2, "an antecedent ends in a cadence weaker than the theme's");
    /* No file is left behind by a request that failed. */
    remove("grow_cli_never.json");
    expect_failure(BASE " --into period --cadence HC --out grow_cli_never.json", 2, "an antecedent ends in a cadence weaker than the theme's");
    assert(fopen("grow_cli_never.json", "rb") == NULL);
}

static void test_unwritable_file(void) {
    expect_failure(BASE " --into sentence --seed 7 --out /no/such/directory/theme.json", 3, "cannot write");
    expect_failure(BASE " --into sentence --seed 7 --midi /no/such/directory/theme.mid", 3, "cannot write");
}

/* How many times a text, which may hold NUL bytes, occurs in a file. */
static size_t occurrences(const char *path, const char *needle) {
    FILE *file = fopen(path, "rb");
    size_t found = 0;
    size_t length = strlen(needle);
    size_t matched = 0;
    int c;
    assert(file);
    while ((c = fgetc(file)) != EOF) {
        matched = c == needle[matched] ? matched + 1 : (c == needle[0] ? 1 : 0);
        if (matched == length) {
            found++;
            matched = 0;
        }
    }
    fclose(file);
    return found;
}

/* An accompaniment is written in the files as a part of its own: a MIDI track and a MusicXML part more. */
static void test_accompaniment(void) {
    remove("grow_cli_plain.mid");
    remove("grow_cli_plain.xml");
    remove("grow_cli_alberti.json");
    remove("grow_cli_alberti.mid");
    remove("grow_cli_alberti.xml");
    free(output_of(BASE " --into sentence --seed 7 --midi grow_cli_plain.mid --musicxml grow_cli_plain.xml"));
    char *out = output_of(BASE " --into sentence --seed 7 --accompaniment alberti --out grow_cli_alberti.json "
                               "--midi grow_cli_alberti.mid --musicxml grow_cli_alberti.xml");
    assert(strstr(out, "accompaniment alberti") != NULL);
    free(out);

    /* The melody and the bass, and now voice 1: three parts, after the track of the tempo and the meter. */
    assert(occurrences("grow_cli_plain.mid", "MTrk") == 3 && occurrences("grow_cli_alberti.mid", "MTrk") == 4);
    assert(occurrences("grow_cli_plain.xml", "<part id=") == 2 && occurrences("grow_cli_alberti.xml", "<part id=") == 3);

    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file("grow_cli_alberti.json", &plan, &error) == PLAN_OK);
    PlanSpan all = plan_node_span(plan, plan_root(plan));
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_INNER_VOICE, 1, all, spans, 64);
    assert(found > 0 && found <= 64);
    for (size_t i = 0; i < found; i++) {
        assert(plan_lane_span_state(plan, spans[i]) == PLAN_STATE_REALIZED);
        assert(plan_lane_span_payload(plan, spans[i])->notes.note_count > 0);
    }
    assert(plan_lane_find(plan, PLAN_LANE_INNER_VOICE, 2, all, spans, 64) == 0);
    plan_destroy(plan);

    /* A texture alone asks for an accompaniment of any pattern; the name is any case. */
    out = output_of(BASE " --into sentence --seed 7 --texture \"Melody and accompaniment\"");
    assert(strstr(out, "accompaniment any (melody and accompaniment)") != NULL);
    free(out);
    out = output_of(BASE " --into sentence --seed 7 --accompaniment block --texture homorhythm --format json");
    assert(strstr(out, "\"accompaniment\":\"block\"") != NULL && strstr(out, "\"texture\":\"homorhythm\"") != NULL);
    free(out);

    expect_failure(BASE " --into sentence --accompaniment waltz", 1, "--accompaniment must be");
    expect_failure(BASE " --into sentence --texture polyphony", 1, "is not one an accompaniment can have");
    expect_failure(BASE " --into sentence --texture nonsense", 1, "--texture must be");
    expect_failure(FIXTURE("motive_d_minor.txt") " --key \"D minor\" --meter 6/8 --into sentence --accompaniment alberti", 1,
                   "needs a simple meter");
}

/* --style-without needs --style, and only names a style category can have. */
static void test_style_without(void) {
    expect_failure(BASE " --into sentence --style-without motive-degrees", 1, "--style-without needs --style");
    expect_failure(BASE " --into sentence --style " FIXTURE("styles/chorales-motives.style.json")
                       " --style-without no-such-category",
                   1, "is not a style category");
    char *out = output_of(BASE " --into sentence --seed 7 --style " FIXTURE("styles/chorales-motives.style.json")
                               " --style-without motive-degrees --style-without motive-nct-figure");
    assert(strstr(out, "style distance") != NULL);
    free(out);
}

/* --mode style needs a style (and a mode in it); the other modes replace the key's. */
static void test_mode(void) {
    expect_failure(BASE " --into sentence --mode style", 1, "--mode style needs --style");
    expect_failure(BASE " --into sentence --mode dorian", 1, "--mode must be major, minor or style");
    char *out = output_of(FIXTURE("motive_d_minor.txt") " --key D --mode major --meter 4/4 --into sentence --seed 1");
    assert(strstr(out, "key D major") != NULL);
    free(out);
}

static void test_help(void) {
    assert(run("--help", "grow_cli_out.txt", "grow_cli_err.txt") == 0);
    char *out = test_read_file("grow_cli_out.txt", NULL);
    assert(strstr(out, "--into") != NULL && strstr(out, "--hybrid") != NULL && strstr(out, "--suggest") != NULL &&
           strstr(out, "--first-cadence") != NULL && strstr(out, "--accompaniment") != NULL &&
           strstr(out, "--texture") != NULL);
    free(out);
}

/* --- slot motives and pool forms through grow_run (grow.h, grow_form.h) ----------------------------- */

static Motive notes_motive(const char *text) {
    MotiveOptions options;
    Motive motive;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    options.has_key = true;
    options.key = plan_key_of_pitch_class(2, KEY_STRUCTURE_MODE_MINOR);
    assert(motive_from_notes_text(text, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static GrowRequest library_request(const Motive *basic, PlanThemeBeginning beginning, PlanThemeEnding ending) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = basic;
    request.key = plan_key_of_pitch_class(2, KEY_STRUCTURE_MODE_MINOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = beginning;
    request.pairing.ending = ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.first_cadence = CADENCE_TYPE_HALF;
    return request;
}

static NodeId library_find(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* Whether the melody lane span of a motive node holds exactly these notes. */
static bool node_holds(const PiecePlan *plan, NodeId node, const Motive *motive) {
    LaneSpanId spans[8];
    size_t found = plan_node_lane_spans(plan, node, spans, 8);
    for (size_t i = 0; i < found; i++) {
        if (plan_lane_span_lane(plan, spans[i]) != PLAN_LANE_MELODY) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, spans[i])->notes;
        if (notes->note_count != motive->note_count) {
            return false;
        }
        for (size_t k = 0; k < notes->note_count; k++) {
            if (notes->notes[k].midi != motive->notes[k].pitch) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static void test_library_slots_and_pool(void) {
    Motive basic = notes_motive("D4:q F4:e A4:e D5:h");
    Motive contrast = notes_motive("A4:e G4:e F4:e E4:e D4:h");
    Motive continuation = notes_motive("F4:e G4:e A4:e Bb4:e A4:h");
    Motive cadential = notes_motive("A4:e F4:e E4:e F4:e D4:h");
    PiecePlan *plan = NULL;

    /* A period with a contrasting motive: the node s1 holds it, the antecedent's contrasting idea m1 restates it and the
       consequent's contrasting idea m3 is made from it, not from the basic motive. */
    GrowRequest request = library_request(&basic, PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT);
    request.slot_motives[GROW_SLOT_CONTRASTING] = &contrast;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    NodeId s1 = library_find(plan, "s1");
    assert(s1 != PLAN_NODE_NONE && node_holds(plan, s1, &contrast) && library_find(plan, "s2") == PLAN_NODE_NONE);
    GrowConfig config;
    GrowOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = 1;
    config.basic = &basic;
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    assert(node_holds(plan, library_find(plan, "m1"), &contrast));
    assert(node_holds(plan, library_find(plan, "m2"), &basic));
    assert(!node_holds(plan, library_find(plan, "m3"), &basic) && !node_holds(plan, library_find(plan, "m3"), &contrast));
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);

    /* A sentence with a continuation motive and a cadential one: fragment 1 and the cadential idea restate them. */
    request = library_request(&basic, PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION);
    request.slot_motives[GROW_SLOT_CONTINUATION] = &continuation;
    request.slot_motives[GROW_SLOT_CADENTIAL] = &cadential;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(library_find(plan, "s2") != PLAN_NODE_NONE && library_find(plan, "s3") != PLAN_NODE_NONE);
    config.seed = 1;
    GrowResult ran = grow_run(plan, &config, &outcome);
    assert(ran == GROW_OK);
    assert(node_holds(plan, library_find(plan, "m1"), &basic));
    assert(node_holds(plan, library_find(plan, "m2"), &continuation));
    assert(node_holds(plan, library_find(plan, "m4"), &cadential));
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);

    /* A pool is offered for a contrasting idea only (P36 B1): the repetition of a presentation is the step its ref names, so an
       approved form of the basic motive that suits a beginning is not the repetition, whatever the functions of the form. */
    Motive form = notes_motive("E4:q G4:e Bb4:e E5:h");
    GrowPoolForm pool;
    memset(&pool, 0, sizeof(pool));
    pool.motive = &form;
    pool.slot_motive_of = GROW_SLOT_BASIC;
    pool.functions = GROW_FUNCTION_BEGINNING;
    pool.approved = true;
    snprintf(pool.item, sizeof(pool.item), "mot-form");
    snprintf(pool.chain, sizeof(pool.chain), "transpose tonal=1 by=1");
    request = library_request(&basic, PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION);
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    memset(&config, 0, sizeof(config));
    config.seed = 1;
    config.basic = &basic;
    config.pool = &pool;
    config.pool_count = 1;
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    assert(!node_holds(plan, library_find(plan, "m1"), &form));
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);

    pool.functions = GROW_FUNCTION_CADENTIAL;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    assert(!node_holds(plan, library_find(plan, "m1"), &form));
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);

    /* A form of a slot's motive is not offered for the basic motive's ideas. */
    pool.functions = GROW_FUNCTION_BEGINNING;
    pool.slot_motive_of = GROW_SLOT_CONTINUATION;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    assert(!node_holds(plan, library_find(plan, "m1"), &form));
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);
}

static void test_library_bad_slots(void) {
    Motive basic = notes_motive("D4:q F4:e A4:e D5:h");
    Motive fits = notes_motive("A4:e G4:e F4:e E4:e D4:h");
    PiecePlan *plan = NULL;
    GrowRequest request = library_request(&basic, PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION);
    /* A slot the pairing has no idea for: no contrasting idea in a sentence, no continuation in a period. */
    request.slot_motives[GROW_SLOT_CONTRASTING] = &fits;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && plan == NULL);
    request = library_request(&basic, PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT);
    request.slot_motives[GROW_SLOT_CONTINUATION] = &fits;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST);
    request.slot_motives[GROW_SLOT_CONTINUATION] = NULL;
    request.slot_motives[GROW_SLOT_CADENTIAL] = &fits;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST);
    /* A motive of another meter, of another key, of one note, and one that is not a full motive. */
    request.slot_motives[GROW_SLOT_CADENTIAL] = NULL;
    Motive other = fits;
    motive_set_meter(&other, (PlanMeter){3, 4, 0});
    request.slot_motives[GROW_SLOT_CONTRASTING] = &other;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST);
    other = fits;
    AnalysisKey other_key = plan_key_of_pitch_class(7, other.minor ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR);
    motive_set_key(&other, &other_key, other.key_source);
    request.slot_motives[GROW_SLOT_CONTRASTING] = &other;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST);
    other = fits;
    other.note_count = 1;
    request.slot_motives[GROW_SLOT_CONTRASTING] = &other;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST);
    other = fits;
    other.form = MOTIVE_SKETCH_FORM_RHYTHM;
    request.slot_motives[GROW_SLOT_CONTRASTING] = &other;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST);
    /* Its own is fine, and the material is a bar longer for it. */
    request.slot_motives[GROW_SLOT_CONTRASTING] = &fits;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    plan_destroy(plan);
}

int main(void) {
    test_library_slots_and_pool();
    test_library_bad_slots();
    test_each_form();
    test_every_hybrid();
    test_four_bars_and_cadences();
    test_same_seed_same_output();
    test_files();
    test_accompaniment();
    test_suggest();
    test_bad_arguments();
    test_cannot_be_satisfied();
    test_unwritable_file();
    test_style_without();
    test_mode();
    test_help();
    puts("test_grow_cli: ok");
    return 0;
}
