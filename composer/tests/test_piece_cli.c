#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pitch/pitch_spelling.h"
#include "time/musical_time.h"

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

/*
 * The `piece` command, run as a process (docs/cli-piece.md, docs/piece-request.md): the loop version and the ending version
 * of a looping game track.
 *
 *   done-when    intro 2 bars, a body of an 8-bar sentence from motive A and a 4-bar free phrase from motive B, a turnaround
 *                of 2 bars, the ending by default, D minor 4/4, the fast minor style, --check: exit 0, both versions
 *                written, loopStart at bar 3 and loopEnd after the turnaround in the loop MIDI (none in the ending's), the
 *                two versions the same note for note up to the body's end, the Analyzer check passing on both
 *   theme        a section filled with a composed theme keeps the theme's chords
 *   progression  a section filled with a progression has its chords from the section's start
 *   structure    a request of empty fills and no motive is exit 0 with an invented motive
 *   errors       a request without a body, a bad pickup and an unknown fill are exit 1
 *
 * The request fixtures are in tests/fixtures/piece; the @STYLE@ and @THEME@ in them are replaced by the test.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define PIECE(name) COMPOSER_FIXTURE_DIR "/piece/" name
/* A whole note in MIDI ticks at 480 to the quarter. */
#define WHOLE_TICKS 1920u

static int run(const char *command, const char *arguments, const char *out_file, const char *err_file) {
    char line[4096];
    snprintf(line, sizeof(line), "\"%s\" %s %s >%s 2>%s", COMPOSER_CLI_PATH, command, arguments, out_file, err_file);
    int status = system(line);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static char *read_file(const char *path, size_t *size) {
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
    if (size) {
        *size = read;
    }
    return text;
}

static bool exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
    }
    return file != NULL;
}

/* The request with the name in it replaced by the value, written to a file of its own. */
static void write_request(const char *fixture, const char *path, const char *name, const char *value) {
    char *text = read_file(fixture, NULL);
    FILE *file = fopen(path, "wb");
    char *at = strstr(text, name);
    assert(file && at);
    fwrite(text, 1, (size_t)(at - text), file);
    fputs(value, file);
    fputs(at + strlen(name), file);
    fclose(file);
    free(text);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

/* The standard output of a piece command that must succeed. */
static char *piece_output(const char *arguments) {
    int status = run("piece", arguments, "piece_out.txt", "piece_err.txt");
    if (status != 0) {
        char *err = read_file("piece_err.txt", NULL);
        fprintf(stderr, "'piece %s' exited %d: %s\n", arguments, status, err);
        free(err);
    }
    assert(status == 0);
    return read_file("piece_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    int got = run("piece", arguments, "piece_out.txt", "piece_err.txt");
    char *err = read_file("piece_err.txt", NULL);
    if (got != status || strstr(err, message) == NULL) {
        fprintf(stderr, "'piece %s': expected status %d with '%s', got %d: %s\n", arguments, status, message, got, err);
    }
    assert(got == status && strstr(err, message) != NULL && strncmp(err, "Error: ", 7) == 0);
    free(err);
}

/* ------------------------------------------------------------- the MIDI file */

static unsigned long read_vlq(const unsigned char *data, size_t *at) {
    unsigned long value = 0;
    unsigned char byte;
    do {
        byte = data[(*at)++];
        value = (value << 7) | (byte & 0x7Fu);
    } while (byte & 0x80u);
    return value;
}

/* The tick of the first marker of the text in the conductor track, or -1 when there is none. */
static long marker_tick(const char *path, const char *text) {
    size_t size;
    unsigned char *data = (unsigned char *)read_file(path, &size);
    assert(size >= 22 && memcmp(data, "MThd", 4) == 0 && memcmp(data + 14, "MTrk", 4) == 0);
    size_t at = 22;
    size_t end = at + (((size_t)data[18] << 24) | ((size_t)data[19] << 16) | ((size_t)data[20] << 8) | data[21]);
    unsigned long tick = 0;
    long found = -1;
    while (at < end) {
        tick += read_vlq(data, &at);
        unsigned char status = data[at++];
        if (status == 0xFF) {
            unsigned char type = data[at++];
            unsigned long length = read_vlq(data, &at);
            if (type == 0x06 && length == strlen(text) && memcmp(data + at, text, length) == 0 && found < 0) {
                found = (long)tick;
            }
            at += length;
        } else if ((status & 0xF0u) == 0xC0u || (status & 0xF0u) == 0xD0u) {
            at += 1;
        } else {
            at += 2;
        }
    }
    free(data);
    return found;
}

/* ------------------------------------------------------------- the plans */

static PiecePlan *load_plan(const char *path) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file(path, &plan, &error) == PLAN_OK);
    return plan;
}

typedef struct {
    double onset;
    long pitch;
    size_t part;
} Note;

static int compare_notes(const void *a, const void *b) {
    const Note *x = a;
    const Note *y = b;
    if (x->onset != y->onset) {
        return x->onset < y->onset ? -1 : 1;
    }
    if (x->part != y->part) {
        return x->part < y->part ? -1 : 1;
    }
    return x->pitch < y->pitch ? -1 : x->pitch > y->pitch;
}

/* The notes that start before the time (whole notes), sorted. */
static size_t notes_before(const PiecePlan *plan, double limit, Note *out, size_t capacity) {
    RenderedPlan rendered;
    size_t count = 0;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    for (size_t i = 0; i < rendered.count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(rendered.events[i]));
        double onset = (double)musical_time_numerator(position) / (double)musical_time_denominator(position);
        if (onset < limit) {
            assert(count < capacity);
            out[count].onset = onset;
            out[count].pitch = (long)pitch_numeric_value_position_numerator(rendered.pitch_values[i]);
            out[count].part = (size_t)rendered.part_index[i];
            count++;
        }
    }
    rendered_plan_destroy(&rendered);
    qsort(out, count, sizeof(*out), compare_notes);
    return count;
}

static size_t count_of(const char *text, const char *needle) {
    size_t count = 0;
    for (const char *at = strstr(text, needle); at; at = strstr(at + 1, needle)) {
        count++;
    }
    return count;
}

/* ------------------------------------------------------------- the done-when case */

static void test_done_when(void) {
    write_request(PIECE("loop-and-ending.json"), "request.json", "@STYLE@", FIXTURE("style-fast-minor.json"));
    char *shown = piece_output("request.json --seed 1 --check --format json --out loop.json --ending-out ending.json "
                               "--midi loop.mid --ending-midi ending.mid --musicxml loop.musicxml --ending-musicxml "
                               "ending.musicxml --explain explain.json");
    assert(exists("loop.json") && exists("ending.json") && exists("loop.mid") && exists("ending.mid") &&
           exists("loop.musicxml") && exists("ending.musicxml") && exists("explain.json"));

    /* Intro 2 bars, then the body 12 bars and the turnaround 2: the loop is whole notes 2 to 16. */
    assert(strstr(shown, "\"loop\":{\"start\":\"2/1\",\"end\":\"16/1\"}"));
    assert(marker_tick("loop.mid", "loopStart") == (long)(2 * WHOLE_TICKS));
    assert(marker_tick("loop.mid", "loopEnd") == (long)(16 * WHOLE_TICKS));
    assert(marker_tick("ending.mid", "loopStart") < 0 && marker_tick("ending.mid", "loopEnd") < 0);
    /* The ending is the version after the body: the default ending of 2 bars, closing in a PAC. */
    assert(strstr(shown, "\"part\":\"ending\",\"letter\":\"E\",\"start\":\"14/1\",\"beats\":8,\"cadence\":\"PAC\""));
    assert(strstr(shown, "\"mode\":{\"mode\":\"minor\",\"from\":\"style\"}"));

    /* The Analyzer check passes on both: five items each (a cadence for each of the four sections, the key). */
    assert(count_of(shown, "\"contradicts\":false") == 2 && count_of(shown, "\"analyzer_check\":null") == 0);
    assert(count_of(shown, "\"planned\":5,\"agreed\":5") == 2);

    /* The two versions agree note for note up to the body's end (the intro and the body are Given in the ending). */
    PiecePlan *loop = load_plan("loop.json");
    PiecePlan *ending = load_plan("ending.json");
    Note *a = calloc(8192, sizeof(Note));
    Note *b = calloc(8192, sizeof(Note));
    size_t loop_count = notes_before(loop, 14.0, a, 8192);
    size_t ending_count = notes_before(ending, 14.0, b, 8192);
    assert(loop_count > 100 && loop_count == ending_count);
    for (size_t i = 0; i < loop_count; i++) {
        assert(a[i].onset == b[i].onset && a[i].pitch == b[i].pitch && a[i].part == b[i].part);
    }
    /* The ending sounds after the body. */
    assert(notes_before(ending, 1000.0, b, 8192) > ending_count);
    free(a);
    free(b);
    plan_destroy(loop);
    plan_destroy(ending);
    free(shown);

    /* The same seed gives the same plan. */
    size_t first_size;
    size_t second_size;
    char *first = read_file("loop.json", &first_size);
    free(piece_output("request.json --seed 1 --check --out loop_again.json"));
    char *second = read_file("loop_again.json", &second_size);
    assert(first_size == second_size && memcmp(first, second, first_size) == 0);
    free(first);
    free(second);

    /* Plain MIDI has no names or section markers, and keeps the loop markers. */
    free(piece_output("request.json --seed 1 --plain-midi --midi plain.mid"));
    assert(marker_tick("plain.mid", "loopStart") == (long)(2 * WHOLE_TICKS) && marker_tick("plain.mid", "A") < 0);
}

/* ------------------------------------------------------------- the other fills */

static void test_theme_fill(void) {
    assert(run("theme", FIXTURE("theme/one-motive.json") " --seed 1 --out theme.json", "theme_out.txt", "theme_err.txt") == 0);
    char absolute[1024];
    char *cwd = getcwd(NULL, 0);
    assert(cwd);
    snprintf(absolute, sizeof(absolute), "%s/theme.json", cwd);
    free(cwd);
    write_request(PIECE("theme-fill.json"), "theme_request.json", "@THEME@", absolute);
    char *shown = piece_output("theme_request.json --seed 1 --format json --out theme_loop.json");
    assert(strstr(shown, "\"id\":\"a\",\"part\":\"body\",\"letter\":\"A\",\"start\":\"2/1\",\"beats\":32,\"cadence\":"));
    assert(strstr(shown, "\"fill\":\"theme\",\"source\":\""));
    free(shown);

    /* The chords of the composed theme are the section's, moved to its start. */
    PiecePlan *theme = load_plan("theme.json");
    PiecePlan *piece = load_plan("theme_loop.json");
    LaneSpanId from[64];
    LaneSpanId to[64];
    PlanSpan theme_span = {{0, 1}, {8, 1}};
    PlanSpan piece_span = {{2, 1}, {8, 1}};
    size_t theme_count = plan_lane_find(theme, PLAN_LANE_HARMONY, 0, theme_span, from, 64);
    size_t piece_count = plan_lane_find(piece, PLAN_LANE_HARMONY, 0, piece_span, to, 64);
    assert(theme_count > 0 && theme_count == piece_count && theme_count <= 64);
    for (size_t i = 0; i < theme_count; i++) {
        assert(strcmp(plan_lane_span_payload(theme, from[i])->harmony.roman_numeral,
                      plan_lane_span_payload(piece, to[i])->harmony.roman_numeral) == 0);
    }
    plan_destroy(theme);
    plan_destroy(piece);
}

static void test_progression_fill(void) {
    free(piece_output(PIECE("progression-fill.json") " --seed 1 --out progression.json"));
    PiecePlan *plan = load_plan("progression.json");
    static const char *const CHORDS[4] = {"i", "iv", "V", "i"};
    LaneSpanId ids[64];
    PlanSpan section = {{2, 1}, {4, 1}};
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, section, ids, 64);
    assert(found >= 4 && found <= 64);
    /* Two cells each, and a cell is half a bar in 4/4: a bar a chord, from the section's start. */
    for (size_t i = 0; i < 4; i++) {
        PlanTime want = {2 + (long)i, 1};
        bool seen = false;
        for (size_t k = 0; k < found; k++) {
            if (plan_time_compare(plan_lane_span_span(plan, ids[k]).start, want) == 0 &&
                strcmp(plan_lane_span_payload(plan, ids[k])->harmony.roman_numeral, CHORDS[i]) == 0) {
                seen = true;
            }
        }
        assert(seen);
    }
    plan_destroy(plan);
}

static void test_structure_only(void) {
    char *shown = piece_output(PIECE("structure-only.json") " --seed 1 --format json --out structure.json --midi structure.mid");
    assert(strstr(shown, "\"fill\":\"motif\"") == NULL && count_of(shown, "\"fill\":\"empty\"") == 8);
    assert(marker_tick("structure.mid", "loopStart") == (long)(2 * WHOLE_TICKS));
    /* The motive is invented: m0 is in the plan as a written melody. */
    PiecePlan *plan = load_plan("structure.json");
    bool m0 = false;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        m0 = m0 || (plan_has_node(plan, id) && strcmp(plan_node_name(plan, id), "m0") == 0 &&
                    plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED);
    }
    assert(m0);
    plan_destroy(plan);
    free(shown);
}

/* ------------------------------------------------------------- the errors */

static void test_errors(void) {
    static const char *const HEAD = "{\"schema\":\"ursatz-piece-request\",\"schema_version\":1,\"key\":\"D\",\"mode\":\"minor\","
                                    "\"meter\":\"4/4\",\"motive\":null,";
    char text[2048];
    snprintf(text, sizeof(text),
             "%s\"sections\":[{\"id\":\"intro\",\"part\":\"intro\",\"letter\":\"I\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,"
             "\"fill\":{\"kind\":\"empty\"}}]}",
             HEAD);
    write_text("no_body.json", text);
    expect_failure("no_body.json --seed 1", 1, "they need a body");
    snprintf(text, sizeof(text),
             "%s\"sections\":[{\"id\":\"a\",\"part\":\"body\",\"letter\":\"A\",\"beats\":16,\"pickup\":\"half\",\"cadence\":null,"
             "\"fill\":{\"kind\":\"empty\"}}]}",
             HEAD);
    write_text("bad_pickup.json", text);
    expect_failure("bad_pickup.json --seed 1", 1, "pickup 'half' is not a number of beats");
    snprintf(text, sizeof(text),
             "%s\"sections\":[{\"id\":\"a\",\"part\":\"body\",\"letter\":\"A\",\"beats\":16,\"pickup\":\"0\",\"cadence\":null,"
             "\"fill\":{\"kind\":\"wobble\"}}]}",
             HEAD);
    write_text("bad_fill.json", text);
    expect_failure("bad_fill.json --seed 1", 1, "the fill kind must be empty, motif, theme or progression, not 'wobble'");
    snprintf(text, sizeof(text),
             "%s\"sections\":[{\"id\":\"a\",\"part\":\"coda\",\"letter\":\"A\",\"beats\":16,\"pickup\":\"0\",\"cadence\":null,"
             "\"fill\":{\"kind\":\"empty\"}}]}",
             HEAD);
    write_text("bad_part.json", text);
    expect_failure("bad_part.json --seed 1", 1, "the part must be intro, body, turnaround or ending");
    write_text("not_json.json", "[1, 2");
    expect_failure("not_json.json", 1, "is not a JSON object");
    write_text("wrong_schema.json", "{\"schema\":\"ursatz-theme-request\",\"schema_version\":1}");
    expect_failure("wrong_schema.json", 1, "ursatz-piece-request");
    expect_failure("missing.json", 1, "cannot read the request");
    expect_failure("", 1, "piece needs a request file");
    expect_failure(PIECE("structure-only.json") " --bogus", 1, "unknown option '--bogus'");
    expect_failure(PIECE("structure-only.json") " --seed x", 1, "--seed must be a whole number");
    /* A file that cannot be written is exit 3 and leaves nothing behind. */
    expect_failure(PIECE("structure-only.json") " --seed 1 --out no_such_dir/plan.json", 3, "cannot write");
    assert(!exists("no_such_dir/plan.json"));
}

int main(void) {
    test_done_when();
    test_theme_fill();
    test_progression_fill();
    test_structure_only();
    test_errors();
    printf("test_piece_cli: ok\n");
    return 0;
}
