#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "io/midi_importer.h"
#include "time/tempo_map.h"

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include "support/test_support.h"

/*
 * The `theme` command, run as a process (docs/cli-theme.md, docs/theme-request.md): themes from up to 16 motives, named
 * by the slots and the single ideas, and a brief.
 *
 *   one motive   a request with the one motive is the theme `grow` makes of it with the same options and seed
 *   slots        a motive in the contrasting slot of a period gives the antecedent's contrasting idea and the consequent's;
 *                one motive may be in the basic and the contrasting slot
 *   ideas        an idea with a motive of its own (the repetition of a sentence, the consequent basic idea of a period)
 *                asks for that motive, exactly, and its melody is not the idea before it; unknown ideas, ideas the pairing
 *                has not and a different basic idea are refused
 *   chords       chords given per bar are kept in the plan's harmony lane, a bar left blank is filled, and what the
 *                harmony operators cannot write (three chords in a bar, an onset off the cells of the meter or off a half
 *                beat, a cadence the chords contradict) is refused with the bar named
 *   v2           saved forms and the version 1 request are refused
 *   brief        the done-when case: two motives under a fast minor style with the mode from the style, checked by the
 *                Analyzer: D minor, the style's tempo in the MIDI file, the style's note density in the melody
 *   errors       the exit codes and messages of requests that do not read or cannot be grown
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define THEME(name) COMPOSER_FIXTURE_DIR "/theme/" name

/* Runs the command with these arguments; stdout and stderr go to files. Returns the exit status. */
static int run(const char *command, const char *arguments, const char *out_file, const char *err_file) {
    char line[4096];
    snprintf(line, sizeof(line), "\"%s\" %s %s >%s 2>%s", COMPOSER_CLI_PATH, command, arguments, out_file, err_file);
    int status = system(line);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

/* The standard output of a theme command that must succeed. */
static char *theme_output(const char *arguments) {
    int status = run("theme", arguments, "theme_out.txt", "theme_err.txt");
    if (status != 0) {
        char *err = test_read_file("theme_err.txt", NULL);
        fprintf(stderr, "'theme %s' exited %d: %s\n", arguments, status, err);
        free(err);
    }
    assert(status == 0);
    return test_read_file("theme_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    int got = run("theme", arguments, "theme_out.txt", "theme_err.txt");
    char *err = test_read_file("theme_err.txt", NULL);
    if (got != status || strstr(err, message) == NULL) {
        fprintf(stderr, "'theme %s': expected status %d with '%s', got %d: %s\n", arguments, status, message, got, err);
    }
    assert(got == status);
    assert(strstr(err, message) != NULL);
    assert(strncmp(err, "Error: ", 7) == 0);
    free(err);
}

/* The line of the slots table for an idea: from "<idea>: " to the end of the line. */
static char *slot_line(const char *out, const char *idea, char *line, size_t size) {
    char key[96];
    snprintf(key, sizeof(key), "\n  %s: ", idea);
    const char *slots = strstr(out, "\nslots (");
    assert(slots);
    const char *at = strstr(slots, key);
    assert(at);
    at += 3;
    size_t length = strcspn(at, "\n");
    assert(length < size);
    memcpy(line, at, length);
    line[length] = '\0';
    return line;
}

/* ------------------------------------------------------------- one motive */

static void test_one_motive(void) {
    /* The request is the file motive_d_minor.txt as degrees; the plan is byte for byte what grow writes. */
    for (int seed = 1; seed <= 3; seed++) {
        char arguments[1024];
        snprintf(arguments, sizeof(arguments), "%s --seed %d --out theme_one.json", THEME("one-motive.json"), seed);
        char *themed = theme_output(arguments);
        assert(strstr(themed, "\nslots (the motive each idea is made from)\n  basic idea: basic slot, motive A, motive\n"));
        assert(strstr(themed, "brief: mode minor (request)"));
        free(themed);
        snprintf(arguments, sizeof(arguments),
                 "%s --into sentence --key \"D minor\" --meter 4/4 --seed %d --out grow_one.json", FIXTURE("motive_d_minor.txt"),
                 seed);
        assert(run("grow", arguments, "grow_out.txt", "grow_err.txt") == 0);
        char *theme_plan = test_read_file("theme_one.json", NULL);
        char *grow_plan = test_read_file("grow_one.json", NULL);
        assert(strcmp(theme_plan, grow_plan) == 0);
        free(theme_plan);
        free(grow_plan);
    }
}

/* ----------------------------------------------------------------- slots */

static void test_contrasting_slot(void) {
    for (int seed = 1; seed <= 4; seed++) {
        char arguments[1024];
        snprintf(arguments, sizeof(arguments), "%s --seed %d", THEME("period-contrast.json"), seed);
        char *out = theme_output(arguments);
        char line[512];
        /* The antecedent's contrasting idea is B or a form of B, the consequent's derives from B, the rest from A. */
        slot_line(out, "contrasting idea", line, sizeof(line));
        assert(strstr(line, "contrasting slot, motive B, motive") != NULL ||
               strstr(line, "contrasting slot, motive B, registry") != NULL);
        slot_line(out, "consequent contrasting idea", line, sizeof(line));
        assert(strstr(line, "contrasting slot, motive B,") != NULL);
        slot_line(out, "consequent basic idea", line, sizeof(line));
        assert(strstr(line, "basic slot, motive A, motive") != NULL);
        free(out);
    }
    /* The first idea of the slot is B itself. */
    char *out = theme_output(THEME("period-contrast.json") " --seed 1");
    char line[512];
    slot_line(out, "contrasting idea", line, sizeof(line));
    assert(strstr(line, "motive B, motive") != NULL);
    free(out);
}

/* ----------------------------------------------------------------- ideas */

static void test_same_motive(void) {
    /* One motive in the basic and the contrasting slot: the contrasting ideas are made from it. */
    for (int seed = 1; seed <= 3; seed++) {
        char arguments[512];
        char line[512];
        snprintf(arguments, sizeof(arguments), "%s --seed %d", THEME("same-motive.json"), seed);
        char *out = theme_output(arguments);
        slot_line(out, "basic idea", line, sizeof(line));
        assert(strstr(line, "basic slot, motive A,") != NULL);
        slot_line(out, "contrasting idea", line, sizeof(line));
        assert(strstr(line, "contrasting slot, motive A,") != NULL);
        slot_line(out, "consequent contrasting idea", line, sizeof(line));
        assert(strstr(line, "contrasting slot, motive A,") != NULL);
        free(out);
    }
}

/* The melody pitches of the idea named, from the plan; the name of the motive node its melody asks for and how. */
static size_t idea_melody(const PiecePlan *plan, const char *idea, int *pitches, size_t size, char *node, size_t node_size,
                          PlanMotiveTransform *transform) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) != PLAN_NODE_IDEA || !name || strcmp(name, idea) != 0) {
            continue;
        }
        LaneSpanId lanes[16];
        size_t found = plan_node_lane_spans(plan, id, lanes, 16);
        for (size_t i = 0; i < found && i < 16; i++) {
            if (plan_lane_span_lane(plan, lanes[i]) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, lanes[i]) != 0) {
                continue;
            }
            for (size_t k = 0; k < plan_lane_span_constraint_count(plan, lanes[i]); k++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, lanes[i], k);
                if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
                    const char *motive = plan_node_name(plan, constraint->params.motive.motive);
                    snprintf(node, node_size, "%s", motive ? motive : "");
                    *transform = constraint->params.motive.transform;
                }
            }
            const PlanNotes *notes = &plan_lane_span_payload(plan, lanes[i])->notes;
            assert(notes->note_count > 0 && notes->note_count <= size);
            for (size_t n = 0; n < notes->note_count; n++) {
                pitches[n] = notes->notes[n].midi_pitch;
            }
            return notes->note_count;
        }
    }
    assert(!"idea not found");
    return 0;
}

static PiecePlan *read_plan(const char *path) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file(path, &plan, &error) == PLAN_OK);
    return plan;
}

static void test_idea_motives(void) {
    for (int seed = 1; seed <= 3; seed++) {
        char arguments[512];
        char line[512];
        /* The repetition of a sentence has the motive R: its melody asks for R exactly, and is not the basic idea's. */
        snprintf(arguments, sizeof(arguments), "%s --seed %d --out theme_idea.json", THEME("idea-repetition.json"), seed);
        char *out = theme_output(arguments);
        slot_line(out, "repetition", line, sizeof(line));
        assert(strstr(line, "basic slot, motive R, idea") != NULL);
        slot_line(out, "basic idea", line, sizeof(line));
        assert(strstr(line, "basic slot, motive A, motive") != NULL);
        slot_line(out, "fragment 1", line, sizeof(line));
        assert(strstr(line, "motive A,") != NULL);
        free(out);
        PiecePlan *plan = read_plan("theme_idea.json");
        int basic[64];
        int repetition[64];
        char node[32];
        PlanMotiveTransform transform = PLAN_TRANSFORM_NONE;
        size_t basic_count = idea_melody(plan, "basic idea", basic, 64, node, sizeof(node), &transform);
        size_t count = idea_melody(plan, "repetition", repetition, 64, node, sizeof(node), &transform);
        assert(strcmp(node, "i1") == 0 && transform == PLAN_TRANSFORM_RESTATED);
        assert(count != basic_count || memcmp(basic, repetition, count * sizeof(int)) != 0);
        plan_destroy(plan);

        /* The consequent basic idea of a period has the motive C. */
        snprintf(arguments, sizeof(arguments), "%s --seed %d --out theme_idea.json", THEME("idea-consequent.json"), seed);
        out = theme_output(arguments);
        slot_line(out, "consequent basic idea", line, sizeof(line));
        assert(strstr(line, "basic slot, motive C, idea") != NULL);
        slot_line(out, "contrasting idea", line, sizeof(line));
        assert(strstr(line, "contrasting slot, motive B,") != NULL);
        free(out);
        plan = read_plan("theme_idea.json");
        idea_melody(plan, "consequent basic idea", basic, 64, node, sizeof(node), &transform);
        assert(strcmp(node, "i3") == 0 && transform == PLAN_TRANSFORM_RESTATED);
        plan_destroy(plan);
    }
    /* The JSON names the source of the idea. */
    char *out = theme_output(THEME("idea-repetition.json") " --seed 1 --format json");
    assert(strstr(out, "{\"idea\":\"repetition\",\"slot\":\"basic\",\"motive\":\"R\",\"source\":\"idea\"") != NULL);
    free(out);
}

/* ------------------------------------------------------------------ v2 */

/* The request of a fixture with the text `from` replaced by `to`, written to `path`. */
static void write_changed(const char *fixture, const char *from, const char *to, const char *path) {
    char *text = test_read_file(fixture, NULL);
    char *at = strstr(text, from);
    assert(at);
    FILE *file = fopen(path, "wb");
    assert(file);
    fwrite(text, 1, (size_t)(at - text), file);
    fputs(to, file);
    fputs(at + strlen(from), file);
    fclose(file);
    free(text);
}

static void test_request_v2(void) {
    /* Saved forms are no longer read, whatever they hold. */
    expect_failure(THEME("forms-refused.json"), 1, "saved forms are no longer read; leave \"forms\" out");
    /* The version 1 request is refused. */
    write_changed(THEME("one-motive.json"), "\"schema_version\": 2", "\"schema_version\": 1", "theme_v1.json");
    expect_failure("theme_v1.json", 1, "the request must have \"schema_version\": 2 (see docs/theme-request.md)");
    /* An idea that is not named so, one the pairing has not, and a basic idea that is not the basic slot's. */
    write_changed(THEME("idea-repetition.json"), "\"repetition\": \"R\"", "\"first idea\": \"R\"", "theme_idea_unknown.json");
    expect_failure("theme_idea_unknown.json", 1, "unknown idea 'first idea'");
    write_changed(THEME("idea-repetition.json"), "\"repetition\": \"R\"", "\"repetition\": \"Z\"", "theme_idea_motive.json");
    expect_failure("theme_idea_motive.json", 1, "which the request does not have");
    write_changed(THEME("idea-repetition.json"), "\"into\": \"sentence\"", "\"into\": \"period\"", "theme_idea_pairing.json");
    expect_failure("theme_idea_pairing.json --seed 1", 1, "idea 'repetition' is not in this theme");
    write_changed(THEME("idea-repetition.json"), "\"repetition\": \"R\"", "\"basic idea\": \"R\"", "theme_idea_basic.json");
    expect_failure("theme_idea_basic.json", 1, "the basic idea is the basic slot");
    /* The basic idea may name the basic slot's own motive. */
    write_changed(THEME("idea-repetition.json"), "\"repetition\": \"R\"", "\"basic idea\": \"A\", \"repetition\": \"R\"",
                  "theme_idea_basic_ok.json");
    free(theme_output("theme_idea_basic_ok.json --seed 1"));
}

/* ----------------------------------------------------------------- brief */

static unsigned long midi_microseconds_per_quarter(const char *path) {
    char *bytes = test_read_file(path, NULL);
    FILE *file = fopen(path, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fclose(file);
    NoteEvent **events = NULL;
    PitchNumericValue **pitches = NULL;
    size_t count = 0;
    MidiTempoImport tempo;
    assert(midi_importer_import_with_tempo((const unsigned char *)bytes, size, &events, &pitches, &count, &tempo) ==
           MIDI_IMPORT_OK);
    assert(!tempo_map_is_defaulted(tempo.tempo_map) && tempo_map_region_count(tempo.tempo_map) == 1);
    unsigned long micro = tempo_map_region_microseconds_per_quarter(tempo_map_region(tempo.tempo_map, 0));
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitches[i]);
    }
    free(events);
    free(pitches);
    midi_tempo_import_destroy(&tempo);
    free(bytes);
    return micro;
}

/* The request with the style's path in place of @STYLE@, written to a file of its own. */
static void write_request_with_style(const char *fixture, const char *path) {
    char *text = test_read_file(fixture, NULL);
    char *at = strstr(text, "@STYLE@");
    assert(at);
    FILE *file = fopen(path, "wb");
    assert(file);
    fwrite(text, 1, (size_t)(at - text), file);
    fputs(FIXTURE("style-fast-minor.json"), file);
    fputs(at + strlen("@STYLE@"), file);
    fclose(file);
    free(text);
}

/* The melody notes of the theme, from the plan the command wrote. */
static size_t melody_attacks(const char *plan_path) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file(plan_path, &plan, &error) == PLAN_OK);
    NodeId theme = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (name && strcmp(name, "theme") == 0) {
            theme = id;
        }
    }
    assert(theme != PLAN_NODE_NONE);
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), spans, 64);
    assert(found > 0 && found <= 64);
    size_t attacks = 0;
    for (size_t i = 0; i < found; i++) {
        attacks += plan_lane_span_payload(plan, spans[i])->notes.note_count;
    }
    plan_destroy(plan);
    return attacks;
}

static void test_fast_minor(void) {
    write_request_with_style(THEME("fast-minor.json"), "theme_fast_minor.json");
    char *out = theme_output("theme_fast_minor.json --seed 97 --check --out theme_fast.json --midi theme_fast.mid "
                             "--explain theme_fast_explain.txt");
    /* The key's mode and the tempo come from the style, and the slots and the brief are printed. */
    assert(strstr(out, "key D minor") != NULL);
    assert(strstr(out, "tempo: 170 (from the style)") != NULL);
    assert(strstr(out, "brief: mode minor (style), tempo 170 (style), density 2 attacks per beat") != NULL);
    char line[512];
    slot_line(out, "cadential", line, sizeof(line));
    assert(strstr(line, "cadential slot, motive B,") != NULL);
    slot_line(out, "repetition", line, sizeof(line));
    assert(strstr(line, "basic slot, motive A,") != NULL);
    free(out);
    assert(midi_microseconds_per_quarter("theme_fast.mid") == (60000000UL + 85) / 170);
    /* The Analyzer agrees with the plan: every planned cadence and the key are heard. */
    char *explained = test_read_file("theme_fast_explain.txt", NULL);
    const char *check = strstr(explained, "analyzer check: ");
    assert(check);
    unsigned agreed = 0;
    unsigned planned = 0;
    assert(sscanf(check, "analyzer check: %u of %u planned agreed", &agreed, &planned) == 2 && planned > 0 && agreed == planned);
    free(explained);
    /* The melody sounds the style's two notes a beat, give or take half a note: 32 beats in eight bars of 4/4. */
    size_t attacks = melody_attacks("theme_fast.json");
    assert(attacks >= 32 * 3 / 2 && attacks <= 32 * 5 / 2);

    /* The brief and the slots are in the JSON. */
    out = theme_output("theme_fast_minor.json --seed 97 --format json");
    assert(strstr(out, "\"brief\":{\"tempo\":{\"qpm\":170,\"from\":\"style\"},\"mode\":{\"mode\":\"minor\",\"from\":\"style\"},"
                       "\"density\":2,\"without\":[]}") != NULL);
    assert(strstr(out, "\"slots\":[{\"idea\":\"basic idea\",\"slot\":\"basic\",\"motive\":\"A\",\"source\":\"motive\"") != NULL);
    free(out);
}

/* -------------------------------------------------------------- chords */

/* The request chords.json with its chords replaced, written to `path`. */
static void write_chords(const char *chords, const char *path) {
    write_changed(THEME("chords.json"), "@CHORDS@", chords, path);
}

/* The Given harmony spans of the theme in the plan the command wrote: their numerals in order, as "start:numeral" lines. */
static size_t given_harmony(const char *plan_path, char numerals[][32], PlanTime *starts, size_t capacity) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file(plan_path, &plan, &error) == PLAN_OK);
    NodeId theme = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (name && strcmp(name, "theme") == 0) {
            theme = id;
        }
    }
    assert(theme != PLAN_NODE_NONE);
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, theme), spans, 64);
    assert(found > 0 && found <= 64);
    size_t given = 0;
    for (size_t i = 0; i < found; i++) {
        if (plan_lane_span_state(plan, spans[i]) != PLAN_STATE_GIVEN) {
            continue;
        }
        assert(given < capacity);
        snprintf(numerals[given], 32, "%s", plan_lane_span_payload(plan, spans[i])->harmony.roman_numeral);
        starts[given++] = plan_lane_span_span(plan, spans[i]).start;
    }
    plan_destroy(plan);
    return given;
}

static void test_chords(void) {
    char numerals[16][32];
    PlanTime starts[16];
    /* I, IV then V on the third beat, vi and V (a half cadence): all of them stay in the harmony lane, where they were put. */
    write_chords("[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 1, \"text\": \"IV V@2\"}, {\"bar\": 2, \"text\": \"VI\"}, "
                 "{\"bar\": 3, \"text\": \"V\"}]",
                 "theme_chords_all.json");
    char *out = theme_output("theme_chords_all.json --seed 1 --out theme_chords_all_plan.json --format json");
    assert(strstr(out, "\"chords\":[{\"bar\":0,\"text\":\"I\"},{\"bar\":1,\"text\":\"IV V@2\"},{\"bar\":2,\"text\":\"VI\"},"
                       "{\"bar\":3,\"text\":\"V\"}]") != NULL);
    free(out);
    size_t count = given_harmony("theme_chords_all_plan.json", numerals, starts, 16);
    assert(count == 5);
    const char *expected[5] = {"I", "IV", "V", "vi", "V"};
    const long start_num[5] = {0, 1, 3, 2, 3};
    const long start_den[5] = {1, 1, 2, 1, 1};
    for (size_t i = 0; i < 5; i++) {
        assert(strcmp(numerals[i], expected[i]) == 0);
        assert(starts[i].num * start_den[i] == start_num[i] * (long)starts[i].den);
    }
    /* A bar left blank is filled; the others stay as given. */
    write_chords("[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 2, \"text\": \"VI\"}, {\"bar\": 3, \"text\": \"V\"}]",
                 "theme_chords_blank.json");
    out = theme_output("theme_chords_blank.json --seed 1 --out theme_chords_blank_plan.json");
    free(out);
    count = given_harmony("theme_chords_blank_plan.json", numerals, starts, 16);
    assert(count == 3 && strcmp(numerals[0], "I") == 0 && strcmp(numerals[1], "vi") == 0 && strcmp(numerals[2], "V") == 0);
    /* No chords given: nothing is Given in the harmony lane. */
    write_chords("[]", "theme_chords_none.json");
    out = theme_output("theme_chords_none.json --seed 1 --out theme_chords_none_plan.json");
    free(out);
    assert(given_harmony("theme_chords_none_plan.json", numerals, starts, 16) == 0);

    /* What cannot be written is refused with the bar named: three chords, an onset of 1/3 of a beat, a half beat the
       harmony operators have no cell for, a bar past the theme, a bar given twice and text that is no chord. */
    write_chords("[{\"bar\": 1, \"text\": \"I IV@2 V@3\"}]", "theme_chords_three.json");
    expect_failure("theme_chords_three.json --seed 1", 1, "chords bar 1: ");
    write_chords("[{\"bar\": 1, \"text\": \"IV V@1/3\"}]", "theme_chords_third.json");
    expect_failure("theme_chords_third.json --seed 1", 1, "chords bar 1: a chord starts on a beat or a half beat");
    write_chords("[{\"bar\": 1, \"text\": \"IV V@3/2\"}]", "theme_chords_half.json");
    expect_failure("theme_chords_half.json --seed 1", 1, "chords bar 1: in 4/4 a chord starts only every 2 beats (@2)");
    write_chords("[{\"bar\": 4, \"text\": \"I\"}]", "theme_chords_past.json");
    expect_failure("theme_chords_past.json --seed 1", 1, "chords bar 4: the theme has bars 0 to 3");
    write_chords("[{\"bar\": 1, \"text\": \"I\"}, {\"bar\": 1, \"text\": \"V\"}]", "theme_chords_twice.json");
    expect_failure("theme_chords_twice.json --seed 1", 1, "chords bar 1: given twice");
    write_chords("[{\"bar\": 2, \"text\": \"Q\"}]", "theme_chords_text.json");
    expect_failure("theme_chords_text.json --seed 1", 1, "chords bar 2: ");

    /* A half cadence asked for, the last bar a V: fine. A perfect cadence asked for with the last bar a V: no theme can
       be grown, and the message names the bar. */
    write_chords("[{\"bar\": 3, \"text\": \"V\"}]", "theme_chords_cadence.json");
    out = theme_output("theme_chords_cadence.json --seed 1");
    free(out);
    char *cadence = test_read_file("theme_chords_cadence.json", NULL);
    char *hc = strstr(cadence, "\"cadence\": \"HC\"");
    assert(hc);
    FILE *file = fopen("theme_chords_contradiction.json", "wb");
    assert(file);
    fwrite(cadence, 1, (size_t)(hc - cadence), file);
    fputs("\"cadence\": \"PAC\"", file);
    fputs(hc + strlen("\"cadence\": \"HC\""), file);
    fclose(file);
    free(cadence);
    expect_failure("theme_chords_contradiction.json --seed 1", 2, "the given chords in bar 3 do not allow the PAC");
}

/* ----------------------------------------------------------- tone roles */

/* Whether a pitch is a tone of the triad of the chord (D major: IV is G B D). */
static bool in_triad(int pitch, const int *triad) {
    for (int i = 0; i < 3; i++) {
        if (((pitch % 12) + 12) % 12 == triad[i]) {
            return true;
        }
    }
    return false;
}

static void test_tone_roles(void) {
    static const int TONIC[3] = {2, 6, 9};     /* D F# A */
    static const int SUBDOMINANT[3] = {7, 11, 2}; /* G B D */
    /* The chords I and then IV: the motive of the request, written for I, is placed on the chord tones of each, so the
       repetition over IV is not the basic idea over I. */
    write_chords("[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 1, \"text\": \"IV\"}]", "theme_tones_chords.json");
    for (int seed = 1; seed <= 3; seed++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments), "theme_tones_chords.json --seed %d --out theme_tones_plan.json", seed);
        free(theme_output(arguments));
        PiecePlan *plan = read_plan("theme_tones_plan.json");
        int basic[64];
        int repetition[64];
        char node[32];
        PlanMotiveTransform transform = PLAN_TRANSFORM_NONE;
        size_t basic_count = idea_melody(plan, "basic idea", basic, 64, node, sizeof(node), &transform);
        size_t count = idea_melody(plan, "repetition", repetition, 64, node, sizeof(node), &transform);
        assert(basic_count >= 4 && count >= 4);
        /* The four notes of the motive are chord tones of the chord under them. */
        for (int k = 0; k < 4; k++) {
            assert(in_triad(basic[k], TONIC));
            assert(in_triad(repetition[k], SUBDOMINANT));
        }
        assert(count != basic_count || memcmp(basic, repetition, 4 * sizeof(int)) != 0);
        plan_destroy(plan);
    }
    /* The plan keeps the tones of the motive in its material node. */
    char *plan_text = test_read_file("theme_tones_plan.json", NULL);
    assert(strstr(plan_text, "\"kind\": \"motive_tones\"") != NULL);
    free(plan_text);

    /* An override is read, applied to the labels the plan keeps, and checked: a note past the motive, a role that is not
       one, and a motive that cannot be labelled exit 1 naming the motive. */
    write_changed(THEME("one-motive.json"), "\"features\": []", "\"features\": [], \"tone_overrides\": {\"2\": \"passing\"}",
                  "theme_tones_override.json");
    free(theme_output("theme_tones_override.json --seed 1 --out theme_tones_override_plan.json"));
    plan_text = test_read_file("theme_tones_override_plan.json", NULL);
    assert(strstr(plan_text, "\"passing\"") != NULL);
    free(plan_text);
    write_changed(THEME("one-motive.json"), "\"features\": []", "\"features\": [], \"tone_overrides\": {\"9\": \"passing\"}",
                  "theme_tones_note.json");
    expect_failure("theme_tones_note.json --seed 1", 1, "motive 'A': tone_overrides \"9\": the note is a number from 1 to ");
    write_changed(THEME("one-motive.json"), "\"features\": []", "\"features\": [], \"tone_overrides\": {\"2\": \"nope\"}",
                  "theme_tones_role.json");
    expect_failure("theme_tones_role.json --seed 1", 1, "motive 'A': tone_overrides \"2\": the role is chord, passing");
    write_changed(THEME("one-motive.json"), "\"features\": []", "\"features\": [], \"tone_overrides\": [1]",
                  "theme_tones_shape.json");
    expect_failure("theme_tones_shape.json --seed 1", 1, "tone_overrides must be");
}

/* ---------------------------------------------------------------- errors */

static void test_errors(void) {
    FILE *file = fopen("theme_bad.json", "wb");
    assert(file);
    fputs("{\"schema\": \"something-else\", \"schema_version\": 2}", file);
    fclose(file);
    expect_failure("theme_bad.json", 1, "ursatz-theme-request");
    expect_failure("no-such-request.json", 1, "cannot read the request");
    expect_failure(THEME("one-motive.json") " --no-such-option", 1, "unknown option");
    expect_failure(THEME("one-motive.json") " --seed x", 1, "--seed");
    /* A contrasting slot has no idea in a sentence: the request cannot be grown. */
    file = fopen("theme_unused.json", "wb");
    assert(file);
    char *text = test_read_file(THEME("period-contrast.json"), NULL);
    char *into = strstr(text, "\"period\"");
    assert(into);
    fwrite(text, 1, (size_t)(into - text), file);
    fputs("\"sentence\"", file);
    fputs(into + strlen("\"period\""), file);
    fclose(file);
    free(text);
    expect_failure("theme_unused.json --seed 1", 2, "cannot be grown as asked");
    /* No file at all. */
    assert(run("theme", "", "theme_out.txt", "theme_err.txt") == 1);
}

int main(void) {
    test_one_motive();
    test_contrasting_slot();
    test_same_motive();
    test_idea_motives();
    test_request_v2();
    test_chords();
    test_tone_roles();
    test_fast_minor();
    test_errors();
    puts("test_theme_cli: ok");
    return 0;
}
