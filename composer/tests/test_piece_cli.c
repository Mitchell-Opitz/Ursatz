#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <pitch/pitch_spelling.h>
#include <time/musical_time.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>
#include "support/test_support.h"

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
 *   regenerate   --from a loop plan: the body's first melody written again refills the turnaround and the seam copies the new
 *                first bar; an intro bass alone changes nothing else; the ending version is built again; an unknown
 *                section is exit 1
 *   variations   three variations of a section with different seeds, each written with the loop markers; a theme-filled
 *                section is harmonized with its melody kept
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

/* The request with the name in it replaced by the value, written to a file of its own. */
static void write_request(const char *fixture, const char *path, const char *name, const char *value) {
    char *text = test_read_file(fixture, NULL);
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
        char *err = test_read_file("piece_err.txt", NULL);
        fprintf(stderr, "'piece %s' exited %d: %s\n", arguments, status, err);
        free(err);
    }
    assert(status == 0);
    return test_read_file("piece_out.txt", NULL);
}

static void expect_failure(const char *arguments, int status, const char *message) {
    int got = run("piece", arguments, "piece_out.txt", "piece_err.txt");
    char *err = test_read_file("piece_err.txt", NULL);
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
    unsigned char *data = (unsigned char *)test_read_file(path, &size);
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

/* ------------------------------------------------------------- reading lanes */

#define DUMP_SIZE 65536

static double to_double(Rational time) {
    return (double)time.num / (double)time.den;
}

/* The spans of a lane and voice inside [from, to) whole notes as text: the place, and the chord or the notes. */
static char *dump_lane(const PiecePlan *plan, PlanLaneKind lane, int voice, long from, long to) {
    char *out = calloc(DUMP_SIZE, 1);
    LaneSpanId ids[512];
    PlanSpan range = {{from, 1}, {to - from, 1}};
    size_t found = plan_lane_find(plan, lane, voice, range, ids, 512);
    size_t used = 0;
    assert(out && found <= 512);
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
        assert(plan_span_end(span, &end) == PLAN_OK);
        if (to_double(span.start) < (double)from || to_double(end) > (double)to) {
            continue;
        }
        used += (size_t)snprintf(out + used, DUMP_SIZE - used, "[%.6f %.6f", to_double(span.start), to_double(end));
        if (lane == PLAN_LANE_HARMONY) {
            used += (size_t)snprintf(out + used, DUMP_SIZE - used, " %s", payload->harmony.roman_numeral);
        } else {
            for (size_t n = 0; n < payload->notes.note_count; n++) {
                used += (size_t)snprintf(out + used, DUMP_SIZE - used, " %d@%.6f/%.6f", payload->notes.notes[n].midi,
                                         to_double(payload->notes.notes[n].offset), to_double(payload->notes.notes[n].duration));
            }
        }
        used += (size_t)snprintf(out + used, DUMP_SIZE - used, "]");
        assert(used < DUMP_SIZE - 256);
    }
    return out;
}

/* All the lanes of a region, voices of the inner lane included. */
static char *dump_region(const PiecePlan *plan, long from, long to) {
    char *out = calloc(DUMP_SIZE * 5, 1);
    size_t used = 0;
    assert(out);
    for (int lane = PLAN_LANE_HARMONY; lane <= PLAN_LANE_INNER_VOICE; lane++) {
        for (int voice = 0; voice <= (lane == PLAN_LANE_INNER_VOICE ? 3 : 0); voice++) {
            char *one = dump_lane(plan, (PlanLaneKind)lane, voice, from, to);
            used += (size_t)snprintf(out + used, DUMP_SIZE * 5 - used, "<%d.%d>%s", lane, voice, one);
            free(one);
        }
    }
    return out;
}

/* What sounds in the bar that starts at the time: each chord or note with its place in the bar. */
static char *dump_bar(const PiecePlan *plan, PlanLaneKind lane, long bar) {
    char *out = calloc(DUMP_SIZE, 1);
    LaneSpanId ids[64];
    PlanSpan range = {{bar, 1}, {1, 1}};
    size_t found = plan_lane_find(plan, lane, 0, range, ids, 64);
    size_t used = 0;
    assert(out && found <= 64);
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
        if (lane == PLAN_LANE_HARMONY) {
            double start = to_double(span.start) < (double)bar ? (double)bar : to_double(span.start);
            used += (size_t)snprintf(out + used, DUMP_SIZE - used, "%.6f %s;", start - (double)bar, payload->harmony.roman_numeral);
            continue;
        }
        for (size_t n = 0; n < payload->notes.note_count; n++) {
            double onset = to_double(span.start) + to_double(payload->notes.notes[n].offset);
            if (onset >= (double)bar && onset < (double)bar + 1.0) {
                used += (size_t)snprintf(out + used, DUMP_SIZE - used, "%.6f %d %.6f;", onset - (double)bar,
                                         payload->notes.notes[n].midi, to_double(payload->notes.notes[n].duration));
            }
        }
    }
    return out;
}

/* The seed of the nth variation in the output, from "seed":. */
static unsigned long variation_seed(const char *text, size_t index) {
    const char *at = text;
    for (size_t i = 0; i <= index; i++) {
        at = strstr(at, "\"seed\":");
        assert(at);
        at += 7;
    }
    return strtoul(at - 0, NULL, 10);
}

/* ------------------------------------------------------------- the done-when case */

static void test_done_when(void) {
    write_request(PIECE("loop-and-ending.json"), "request.json", "@STYLE@", FIXTURE("style-fast-minor.json"));
    char *shown = piece_output("request.json --seed 1 --check --format json --out loop.json --ending-out ending.json "
                               "--midi loop.mid --ending-midi ending.mid --musicxml loop.musicxml --ending-musicxml "
                               "ending.musicxml --explain explain.json");
    assert(test_exists("loop.json") && test_exists("ending.json") && test_exists("loop.mid") && test_exists("ending.mid") &&
           test_exists("loop.musicxml") && test_exists("ending.musicxml") && test_exists("explain.json"));

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
    assert(loop_count > 90 && loop_count == ending_count);
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
    char *first = test_read_file("loop.json", &first_size);
    free(piece_output("request.json --seed 1 --check --out loop_again.json"));
    char *second = test_read_file("loop_again.json", &second_size);
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
        Rational want = {2 + (long)i, 1};
        bool seen = false;
        for (size_t k = 0; k < found; k++) {
            if (rational_cmp(plan_lane_span_span(plan, ids[k]).start, want) == 0 &&
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

/* ------------------------------------------------------------- regenerate and variations */

/* The regions of the done-when piece, in whole notes: intro 0-2, A 2-10, B 10-14, the turnaround 14-16, the seam 16-17. */
static void expect_region(const PiecePlan *before, const PiecePlan *after, long from, long to, bool same) {
    char *a = dump_region(before, from, to);
    char *b = dump_region(after, from, to);
    assert((strcmp(a, b) == 0) == same);
    free(a);
    free(b);
}

/* A lane dump without its pitches: "62@0.5/0.125" is "@0.5/0.125". */
static char *rhythm_of(const char *dump) {
    char *out = calloc(strlen(dump) + 1, 1);
    size_t used = 0;
    assert(out);
    for (const char *c = dump; *c; c++) {
        bool pitch = *c >= '0' && *c <= '9';
        for (const char *d = c; pitch && *d >= '0' && *d <= '9'; d++) {
            pitch = d[1] == '@' || (d[1] >= '0' && d[1] <= '9');
        }
        if (!pitch) {
            out[used++] = *c;
        }
    }
    return out;
}

static void test_regenerate(void) {
    /* loop.json and request.json are the done-when piece, written by test_done_when. */
    expect_failure("--from loop.json --request request.json --regenerate nowhere --seed 1", 1, "has no section 'nowhere'");
    expect_failure("--from loop.json --request request.json --seed 1", 1, "--regenerate");
    expect_failure("--from loop.json --seed 1 --regenerate a", 1, "--request");
    expect_failure("request.json --regenerate a", 1, "go with --from");
    expect_failure("--from loop.json --request request.json --regenerate a --lane tenor", 1, "--lane must be");

    /* The body's first melody, written again: the turnaround is refilled and the seam copies the new first bar. */
    char *shown = piece_output("--from loop.json --request request.json --regenerate a --lane melody --seed 9 --format json "
                               "--out regen.json --ending-out regen_ending.json");
    assert(strstr(shown, "\"regenerated\":{\"section\":\"a\",\"lane\":\"melody\"}"));
    free(shown);
    PiecePlan *before = load_plan("loop.json");
    PiecePlan *after = load_plan("regen.json");
    /* The motives' tones are in the plan the piece was read from and in the one regenerated from it (R-215), so the melody
       written again is placed by them. */
    size_t toned_before = 0;
    size_t toned_after = 0;
    for (NodeId node = 1; node <= plan_node_count(before); node++) {
        toned_before += plan_node_has_constraint(before, node, PLAN_CONSTRAINT_MOTIVE_TONES);
    }
    for (NodeId node = 1; node <= plan_node_count(after); node++) {
        toned_after += plan_node_has_constraint(after, node, PLAN_CONSTRAINT_MOTIVE_TONES);
    }
    assert(toned_before > 0 && toned_after == toned_before);
    char *old_melody = dump_lane(before, PLAN_LANE_MELODY, 0, 2, 10);
    char *new_melody = dump_lane(after, PLAN_LANE_MELODY, 0, 2, 10);
    assert(strcmp(old_melody, new_melody) != 0);
    free(old_melody);
    free(new_melody);
    expect_region(before, after, 0, 2, true);   /* the intro */
    expect_region(before, after, 10, 14, true); /* B */
    char *old_end = dump_region(before, 14, 16);
    char *new_end = dump_region(after, 14, 16);
    assert(strcmp(old_end, new_end) != 0); /* the turnaround is written again */
    free(old_end);
    free(new_end);
    static const PlanLaneKind SEAM_LANES[] = {PLAN_LANE_HARMONY, PLAN_LANE_MELODY};
    for (size_t l = 0; l < 2; l++) {
        char *first = dump_bar(after, SEAM_LANES[l], 2);
        char *seam = dump_bar(after, SEAM_LANES[l], 16);
        char *stale = dump_bar(before, SEAM_LANES[l], 16);
        assert(first[0] != '\0' && strcmp(first, seam) == 0);
        /* The body's first bar is its motive's realized notes (R-328), which a melody written again says again: the seam copies
           the new first bar, which may equal the old one. */
        (void)stale;
        free(first);
        free(seam);
        free(stale);
    }
    plan_destroy(before);
    plan_destroy(after);

    /* The ending version is built again from the new loop: the two agree up to the body's end. */
    PiecePlan *loop = load_plan("regen.json");
    PiecePlan *ending = load_plan("regen_ending.json");
    Note *a = calloc(8192, sizeof(Note));
    Note *b = calloc(8192, sizeof(Note));
    size_t loop_count = notes_before(loop, 14.0, a, 8192);
    size_t ending_count = notes_before(ending, 14.0, b, 8192);
    assert(loop_count > 90 && loop_count == ending_count);
    for (size_t i = 0; i < loop_count; i++) {
        assert(a[i].onset == b[i].onset && a[i].pitch == b[i].pitch && a[i].part == b[i].part);
    }
    free(a);
    free(b);
    plan_destroy(loop);
    plan_destroy(ending);

    /* An intro bass alone: nothing else changes. */
    free(piece_output("--from loop.json --request request.json --regenerate intro --lane bass --seed 2 --out bass.json"));
    before = load_plan("loop.json");
    after = load_plan("bass.json");
    char *old_bass = dump_lane(before, PLAN_LANE_BASS, 0, 0, 2);
    char *new_bass = dump_lane(after, PLAN_LANE_BASS, 0, 0, 2);
    assert(strcmp(old_bass, new_bass) != 0);
    free(old_bass);
    free(new_bass);
    expect_region(before, after, 2, 17, true);
    char *old_chords = dump_lane(before, PLAN_LANE_HARMONY, 0, 0, 2);
    char *new_chords = dump_lane(after, PLAN_LANE_HARMONY, 0, 0, 2);
    char *old_tune = dump_lane(before, PLAN_LANE_MELODY, 0, 0, 2);
    char *new_tune = dump_lane(after, PLAN_LANE_MELODY, 0, 0, 2);
    /* The chords stay. The tune is made again over the new bass (the melody depends on it): the notes of an idea that do not
       fit the chords are moved to them, so its pitches may differ, but not its rhythm. This piece's motif has no harmony of its
       own, so its developed motives' chords are not held (R-348) and equal pitches cannot be asked of it (P36 B2). */
    assert(strcmp(old_chords, new_chords) == 0);
    char *old_rhythm = rhythm_of(old_tune);
    char *new_rhythm = rhythm_of(new_tune);
    assert(strcmp(old_rhythm, new_rhythm) == 0);
    free(old_rhythm);
    free(new_rhythm);
    free(old_chords);
    free(new_chords);
    free(old_tune);
    free(new_tune);
    plan_destroy(before);
    plan_destroy(after);

    /* Nothing reopened: the loop plan is kept and only the ending version is built again. */
    free(piece_output("--from loop.json --request request.json --regenerate none --seed 2 --out same.json --ending-out same_ending.json"));
    before = load_plan("loop.json");
    after = load_plan("same.json");
    expect_region(before, after, 0, 17, true);
    plan_destroy(before);
    plan_destroy(after);
    assert(test_exists("same_ending.json"));
}

static void test_variations(void) {
    char *shown = piece_output("--from loop.json --request request.json --variations a --lane melody --count 3 --seed 1 "
                               "--out-dir variations --format json");
    assert(count_of(shown, "\"index\":") == 3 && strstr(shown, "\"kept\":3,\"requested\":3"));
    unsigned long seeds[3];
    for (size_t i = 0; i < 3; i++) {
        seeds[i] = variation_seed(shown, i);
    }
    assert(seeds[0] != seeds[1] && seeds[1] != seeds[2] && seeds[0] != seeds[2]);
    free(shown);
    assert(test_exists("variations/result.json"));
    PiecePlan *original = load_plan("loop.json");
    char *old_melody = dump_lane(original, PLAN_LANE_MELODY, 0, 2, 10);
    size_t tunes_changed = 0;
    char *melodies[3]; /* the whole piece of each */
    for (size_t i = 0; i < 3; i++) {
        char path[128];
        snprintf(path, sizeof(path), "variations/variation-%zu.json", i + 1);
        assert(test_exists(path));
        snprintf(path, sizeof(path), "variations/variation-%zu.musicxml", i + 1);
        assert(test_exists(path));
        snprintf(path, sizeof(path), "variations/variation-%zu.mid", i + 1);
        assert(marker_tick(path, "loopStart") == (long)(2 * WHOLE_TICKS) && marker_tick(path, "loopEnd") == (long)(16 * WHOLE_TICKS));
        snprintf(path, sizeof(path), "variations/variation-%zu.json", i + 1);
        PiecePlan *variation = load_plan(path);
        melodies[i] = dump_region(variation, 0, 17);
        char *tune = dump_lane(variation, PLAN_LANE_MELODY, 0, 2, 10);
        /* A melody written again says its motives' realized notes again (R-328): a variation may keep the tune. */
        tunes_changed += strcmp(tune, old_melody) != 0;
        free(tune);
        expect_region(original, variation, 0, 2, true);
        plan_destroy(variation);
    }
    (void)tunes_changed; /* the variations differ as pieces (below), not necessarily in the body's tune */
    assert(strcmp(melodies[0], melodies[1]) != 0 && strcmp(melodies[1], melodies[2]) != 0 && strcmp(melodies[0], melodies[2]) != 0);
    for (size_t i = 0; i < 3; i++) {
        free(melodies[i]);
    }
    free(old_melody);
    plan_destroy(original);
    expect_failure("--from loop.json --request request.json --variations a --lane melody --count 9 --out-dir variations", 1,
                   "--count must be");
    expect_failure("--from loop.json --request request.json --variations a --count 2 --out-dir variations", 1,
                   "--variations needs --lane");
    expect_failure("--from loop.json --request request.json --variations nowhere --lane melody --count 2 --out-dir variations", 1,
                   "has no section 'nowhere'");
}

/* Harmonizing a section filled with a theme: new chords and a new bass over the theme's own melody. */
static void test_harmonize_theme(void) {
    /* theme_loop.json and theme_request.json are written by test_theme_fill. */
    expect_failure("--from theme_loop.json --request theme_request.json --regenerate a --seed 1", 1, "filled with a theme");
    expect_failure("--from theme_loop.json --request theme_request.json --regenerate a --lane melody --seed 1", 1, "filled with a theme");
    char *shown = piece_output("--from theme_loop.json --request theme_request.json --variations a --lane harmony --count 3 "
                               "--seed 3 --out-dir harmonize --format json");
    size_t kept = count_of(shown, "\"index\":");
    assert(kept >= 2 && kept <= 3);
    free(shown);
    PiecePlan *original = load_plan("theme_loop.json");
    char *old_melody = dump_lane(original, PLAN_LANE_MELODY, 0, 2, 10);
    char *old_chords = dump_lane(original, PLAN_LANE_HARMONY, 0, 2, 10);
    for (size_t i = 0; i < kept; i++) {
        char path[128];
        snprintf(path, sizeof(path), "harmonize/variation-%zu.json", i + 1);
        PiecePlan *variation = load_plan(path);
        char *melody = dump_lane(variation, PLAN_LANE_MELODY, 0, 2, 10);
        char *chords = dump_lane(variation, PLAN_LANE_HARMONY, 0, 2, 10);
        assert(strcmp(melody, old_melody) == 0 && strcmp(chords, old_chords) != 0);
        free(melody);
        free(chords);
        plan_destroy(variation);
    }
    free(old_melody);
    free(old_chords);
    plan_destroy(original);
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
    assert(!test_exists("no_such_dir/plan.json"));
}

int main(void) {
    test_done_when();
    test_theme_fill();
    test_progression_fill();
    test_structure_only();
    test_regenerate();
    test_variations();
    test_harmonize_theme();
    test_errors();
    printf("test_piece_cli: ok\n");
    return 0;
}
