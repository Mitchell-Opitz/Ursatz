#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/piece_structure.h>

/*
 * Theme and progression fills (R-170): a piece structure whose sections are given a composed theme or a progression.
 *
 *   fixture      the theme plan the other cases use: the `theme` command run on test_theme_cli's one-motive request
 *                (D minor, an 8-bar sentence), read back as a plan with a realized theme section
 *   theme        that theme given to an 8-bar body section of a piece in E minor, seeds 1 to 10: after compose its melody,
 *                harmony and bass are unchanged (GivenSnapshot) and are the theme's moved up a whole tone; the piece's basic
 *                motive is the theme's m0 in E minor
 *   refused      a theme of another length is GIVEN_ERR_LENGTH, a plan not yet realized GIVEN_ERR_NOT_REALIZED, a theme in
 *                another mode or meter and a section that already has nodes are refused, and the plan is left as it was
 *   progression  `I vi IV V`, 2 cells each, in a 4-bar section of C major, seeds 1 to 10: the chords are kept and the
 *                melody sounds a chord tone on the downbeat of each bar
 *   partial      `I IV V I` in an 8-bar section: the four chords are kept and the harmony stage writes the rest
 */

#define THEME_REQUEST COMPOSER_FIXTURE_DIR "/theme/one-motive.json"
#define THEME_PLAN "piece_fills_theme.json"

/* The theme command on the request with this seed; its plan goes to THEME_PLAN. */
static void write_theme_plan(unsigned long seed) {
    char line[4096];
    snprintf(line, sizeof(line), "\"%s\" theme \"%s\" --seed %lu --out " THEME_PLAN " >piece_fills_out.txt 2>piece_fills_err.txt",
             COMPOSER_CLI_PATH, THEME_REQUEST, seed);
    int status = system(line);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/* The theme plan the command wrote, read back. */
static PiecePlan *read_theme_plan(unsigned long seed) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    write_theme_plan(seed);
    assert(plan_read_json_file(THEME_PLAN, &plan, &error) == PLAN_OK);
    return plan;
}

/* The theme section of a plan: the first section node that is not the material. */
static NodeId theme_section(const PiecePlan *plan) {
    NodeId form = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        bool material = false;
        for (size_t c = 0; c < plan_node_constraint_count(plan, child); c++) {
            material = material || plan_node_constraint_at(plan, child, c)->kind == PLAN_CONSTRAINT_MATERIAL;
        }
        if (plan_node_kind(plan, child) == PLAN_NODE_SECTION && !material) {
            return child;
        }
    }
    return PLAN_NODE_NONE;
}

static void test_fixture(void) {
    PiecePlan *plan = read_theme_plan(1);
    NodeId theme = theme_section(plan);
    assert(theme != PLAN_NODE_NONE);
    assert(plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    Rational eight_bars;
    assert(rational_make(8, 1, &eight_bars) == RATIONAL_OK);
    assert(rational_cmp(plan_node_span(plan, theme).length, eight_bars) == 0);
    plan_destroy(plan);
}

#define SEEDS 10

typedef struct {
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS];
    PieceStructure structure;
} Fixture;

static void fixture_init(Fixture *fixture, int tonic, KeyStructureMode mode) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->structure.key = plan_key_of_pitch_class(tonic, mode);
    fixture->structure.meter.beats_per_bar = 4;
    fixture->structure.meter.beat_unit = 4;
    fixture->structure.sections = fixture->sections;
}

static PieceSection *add_section(Fixture *fixture, const char *id, const char *letter, PiecePart part, size_t beats,
                                 PieceFillKind fill) {
    PieceSection *section = &fixture->sections[fixture->structure.section_count++];
    memset(section, 0, sizeof(*section));
    snprintf(section->id, sizeof(section->id), "%s", id);
    snprintf(section->letter, sizeof(section->letter), "%s", letter);
    section->part = part;
    section->beats = beats;
    section->pickup = (Rational){0, 1};
    section->fill = fill;
    return section;
}

/* The section nodes of the piece (not the material or the seam), in time order. */
static size_t piece_sections(const PiecePlan *plan, NodeId *out, size_t max) {
    size_t count = 0;
    NodeId form = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, form) && count < max; i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        bool material = false;
        for (size_t c = 0; c < plan_node_constraint_count(plan, child); c++) {
            PlanConstraintKind kind = plan_node_constraint_at(plan, child, c)->kind;
            material = material || kind == PLAN_CONSTRAINT_MATERIAL || kind == PLAN_CONSTRAINT_SEAM;
        }
        if (!material) {
            out[count++] = child;
        }
    }
    return count;
}

/* The lane spans of a lane and voice 0 inside the span, in time order; up to max. */
static size_t lane_spans(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span, LaneSpanId *out, size_t max) {
    size_t found = plan_lane_find(plan, lane, 0, span, out, max);
    assert(found <= max);
    return found;
}

/* Every note of the lane inside the span, as pitch and absolute onset, in time order. */
typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} Sounded;

static size_t sounded(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span, Sounded *out, size_t max) {
    LaneSpanId ids[64];
    size_t spans = lane_spans(plan, lane, span, ids, 64);
    size_t count = 0;
    for (size_t i = 0; i < spans; i++) {
        PlanSpan owner = plan_lane_span_span(plan, ids[i]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(count < max);
            out[count].pitch = notes->notes[n].midi_pitch;
            out[count].duration = notes->notes[n].duration;
            assert(rational_add(owner.start, notes->notes[n].offset, &out[count].onset) == RATIONAL_OK);
            count++;
        }
    }
    return count;
}

/* Whether every Given run the snapshot found is still there, and how many runs there were. */
static size_t runs_unchanged(const GivenSnapshot *snapshot, const PiecePlan *plan) {
    GivenReport reports[64];
    size_t runs = given_snapshot_report(snapshot, plan, reports, 64);
    assert(runs <= 64);
    for (size_t i = 0; i < runs; i++) {
        assert(reports[i].unchanged);
    }
    return runs;
}

static bool has_run(const GivenSnapshot *snapshot, const PiecePlan *plan, PlanLaneKind lane) {
    GivenReport reports[64];
    size_t runs = given_snapshot_report(snapshot, plan, reports, 64);
    for (size_t i = 0; i < runs && i < 64; i++) {
        if (reports[i].lane == lane) {
            return true;
        }
    }
    return false;
}

/* The m0 node's melody notes of a plan. */
static size_t motive_notes(const PiecePlan *plan, Sounded *out, size_t max) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, "m0") == 0) {
            return sounded(plan, PLAN_LANE_MELODY, plan_node_span(plan, id), out, max);
        }
    }
    return 0;
}

/* A piece in E minor: the D minor theme as an 8-bar body section, then a 2-bar turnaround from the piece's motive. */
static void theme_structure(Fixture *fixture, const PiecePlan *theme) {
    fixture_init(fixture, 4, KEY_STRUCTURE_MODE_MINOR);
    PieceSection *body = add_section(fixture, "a", "A", PIECE_PART_BODY, 32, PIECE_FILL_THEME);
    body->theme_plan = theme;
    add_section(fixture, "t", "T", PIECE_PART_TURNAROUND, 8, PIECE_FILL_EMPTY);
}

static void test_theme(void) {
    PiecePlan *theme = read_theme_plan(1);
    NodeId theme_node = theme_section(theme);
    PlanSpan theme_span = plan_node_span(theme, theme_node);
    Sounded was_melody[512];
    Sounded was_bass[512];
    size_t melody_count = sounded(theme, PLAN_LANE_MELODY, theme_span, was_melody, 512);
    size_t bass_count = sounded(theme, PLAN_LANE_BASS, theme_span, was_bass, 512);
    assert(melody_count > 8 && bass_count > 4);
    Sounded was_motive[16];
    size_t motive_count = motive_notes(theme, was_motive, 16);
    assert(motive_count >= 2);

    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Fixture fixture;
        PiecePlan *plan;
        ComposeConfig config;
        ComposeOutcome outcome;
        theme_structure(&fixture, theme);
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
        NodeId sections[4];
        assert(piece_sections(plan, sections, 4) == 2);
        /* The theme is in place before any stage runs: Realized, with the nodes it had. */
        assert(plan_node_state(plan, sections[0]) == PLAN_STATE_REALIZED);
        assert(plan_node_child_count(plan, sections[0]) == plan_node_child_count(theme, theme_node));
        GivenSnapshot *snapshot = given_snapshot(plan);
        assert(snapshot);
        assert(has_run(snapshot, plan, PLAN_LANE_MELODY) && has_run(snapshot, plan, PLAN_LANE_HARMONY) &&
               has_run(snapshot, plan, PLAN_LANE_BASS));

        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(compose_run(plan, &config, &outcome) == PIECE_OK);
        compose_outcome_destroy(&outcome);
        assert(runs_unchanged(snapshot, plan) > 0);
        given_snapshot_destroy(snapshot);

        /* The notes are the theme's, a whole tone up (D minor to E minor), at the same times. */
        PlanSpan span = plan_node_span(plan, sections[0]);
        Sounded now[512];
        assert(sounded(plan, PLAN_LANE_MELODY, span, now, 512) == melody_count);
        for (size_t i = 0; i < melody_count; i++) {
            assert(now[i].pitch == was_melody[i].pitch + 2);
            assert(rational_cmp(now[i].onset, was_melody[i].onset) == 0);
            assert(rational_cmp(now[i].duration, was_melody[i].duration) == 0);
        }
        assert(sounded(plan, PLAN_LANE_BASS, span, now, 512) == bass_count);
        for (size_t i = 0; i < bass_count; i++) {
            assert(now[i].pitch == was_bass[i].pitch + 2);
        }
        LaneSpanId was_chords[64];
        LaneSpanId chords[64];
        size_t chord_count = lane_spans(theme, PLAN_LANE_HARMONY, theme_span, was_chords, 64);
        assert(lane_spans(plan, PLAN_LANE_HARMONY, span, chords, 64) == chord_count);
        for (size_t i = 0; i < chord_count; i++) {
            const PlanHarmony *was = &plan_lane_span_payload(theme, was_chords[i])->harmony;
            const PlanHarmony *chord = &plan_lane_span_payload(plan, chords[i])->harmony;
            assert(strcmp(was->roman_numeral, chord->roman_numeral) == 0);
        }

        /* The theme's m0 is the piece's basic motive, in E minor. */
        Sounded motive[16];
        assert(motive_notes(plan, motive, 16) == motive_count);
        for (size_t i = 0; i < motive_count; i++) {
            assert(motive[i].pitch == was_motive[i].pitch + 2);
        }
        /* The turnaround, after it, is written by the stages. */
        PlanSpan after = plan_node_span(plan, sections[1]);
        assert(sounded(plan, PLAN_LANE_MELODY, after, now, 512) > 0);
        plan_destroy(plan);
    }
    plan_destroy(theme);
}

/* Builds the structure's plan with an empty body of this many beats, in this key. */
static PiecePlan *empty_body(Fixture *fixture, int tonic, KeyStructureMode mode, size_t beats) {
    PiecePlan *plan;
    fixture_init(fixture, tonic, mode);
    add_section(fixture, "a", "A", PIECE_PART_BODY, beats, PIECE_FILL_EMPTY);
    assert(piece_structure_build_plan(&fixture->structure, &plan) == PIECE_OK);
    return plan;
}

static void test_refused(void) {
    PiecePlan *theme = read_theme_plan(1);
    NodeId theme_node = theme_section(theme);
    Fixture fixture;
    NodeId sections[4];

    /* Another length: the theme is 8 bars, the section 4. */
    PiecePlan *plan = empty_body(&fixture, 4, KEY_STRUCTURE_MODE_MINOR, 16);
    assert(piece_sections(plan, sections, 4) == 1);
    assert(plan_give_section(plan, sections[0], theme, theme_node) == GIVEN_ERR_LENGTH);
    assert(plan_node_state(plan, sections[0]) == PLAN_STATE_CONSTRAINED);
    assert(plan_node_child_count(plan, sections[0]) == 0);
    assert(plan_node_lane_spans(plan, sections[0], NULL, 0) == 0);
    plan_destroy(plan);
    theme_structure(&fixture, theme);
    fixture.sections[0].beats = 16;
    assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_ERR_BAD_REQUEST);

    /* A plan whose section is not realized yet. */
    PiecePlan *open = empty_body(&fixture, 2, KEY_STRUCTURE_MODE_MINOR, 32);
    NodeId open_node;
    assert(piece_sections(open, &open_node, 1) == 1);
    plan = empty_body(&fixture, 4, KEY_STRUCTURE_MODE_MINOR, 32);
    assert(piece_sections(plan, sections, 4) == 1);
    assert(plan_give_section(plan, sections[0], open, open_node) == GIVEN_ERR_NOT_REALIZED);
    plan_destroy(open);

    /* Another mode: the key lanes differ in shape. */
    PiecePlan *major = empty_body(&fixture, 4, KEY_STRUCTURE_MODE_MAJOR, 32);
    NodeId major_node;
    assert(piece_sections(major, &major_node, 1) == 1);
    assert(plan_give_section(major, major_node, theme, theme_node) == GIVEN_ERR_KEY);
    plan_destroy(major);

    /* Not a theme section: the material, and a node of the theme. */
    NodeId form = plan_root(plan);
    NodeId material = plan_node_child_at(plan, form, plan_node_child_count(plan, form) - 1);
    assert(plan_give_section(plan, material, theme, theme_node) == GIVEN_ERR_NOT_SECTION);
    assert(plan_give_section(plan, sections[0], theme, plan_node_child_at(theme, theme_node, 0)) == GIVEN_ERR_NOT_SECTION);
    assert(plan_give_section(plan, sections[0], NULL, theme_node) == GIVEN_ERR_NULL);

    /* A section that is given twice has nodes already, and the first stays. */
    assert(plan_give_section(plan, sections[0], theme, theme_node) == GIVEN_OK);
    size_t children = plan_node_child_count(plan, sections[0]);
    GivenSnapshot *snapshot = given_snapshot(plan);
    assert(plan_give_section(plan, sections[0], theme, theme_node) == GIVEN_ERR_OVERLAP);
    assert(plan_node_child_count(plan, sections[0]) == children);
    assert(runs_unchanged(snapshot, plan) > 0);
    given_snapshot_destroy(snapshot);
    plan_destroy(plan);
    plan_destroy(theme);
}

static const char *const NUMERALS[4] = {"I", "vi", "IV", "V"};
/* The pitch classes of those chords in C major. */
static const int CHORD_TONES[4][3] = {{0, 4, 7}, {9, 0, 4}, {5, 9, 0}, {7, 11, 2}};

static void test_progression(void) {
    size_t kept = 0;
    size_t stuck = 0;
    size_t on_downbeat = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Fixture fixture;
        PiecePlan *plan;
        ComposeConfig config;
        ComposeOutcome outcome;
        fixture_init(&fixture, 0, KEY_STRUCTURE_MODE_MAJOR);
        PieceSection *body = add_section(&fixture, "p", "A", PIECE_PART_BODY, 16, PIECE_FILL_PROGRESSION);
        body->numerals = NUMERALS;
        body->numeral_count = 4;
        body->cells_per_chord = 2;
        /* The progression ends on V, so the section ends in a half cadence: the default for a section that is not the
           loop's last would be an imperfect authentic cadence, which no melody can make over a final V. */
        body->has_cadence = true;
        body->cadence = CADENCE_TYPE_HALF;
        add_section(&fixture, "t", "T", PIECE_PART_TURNAROUND, 16, PIECE_FILL_EMPTY);
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
        NodeId sections[4];
        assert(piece_sections(plan, sections, 4) == 2);
        GivenSnapshot *snapshot = given_snapshot(plan);
        assert(snapshot);
        assert(has_run(snapshot, plan, PLAN_LANE_HARMONY));

        memset(&config, 0, sizeof(config));
        config.seed = seed;
        /* The melody of a phrase that ends on V is hard to write (B4): the loop's end is filled in a stage of its own
           now, with other draws, so one seed in ten may still be stuck after the twelve rounds. */
        if (compose_run(plan, &config, &outcome) != PIECE_OK) {
            stuck++;
            compose_outcome_destroy(&outcome);
            given_snapshot_destroy(snapshot);
            plan_destroy(plan);
            continue;
        }
        compose_outcome_destroy(&outcome);
        assert(runs_unchanged(snapshot, plan) > 0);
        given_snapshot_destroy(snapshot);

        PlanSpan span = plan_node_span(plan, sections[0]);
        LaneSpanId chords[16];
        size_t chord_count = lane_spans(plan, PLAN_LANE_HARMONY, span, chords, 16);
        assert(chord_count >= 4);
        for (size_t i = 0; i < 4; i++) {
            assert(plan_lane_span_state(plan, chords[i]) == PLAN_STATE_GIVEN);
            assert(strcmp(plan_lane_span_payload(plan, chords[i])->harmony.roman_numeral, NUMERALS[i]) == 0);
        }
        kept++;

        /* The melody sounds a chord tone wherever a note starts on the downbeat of a bar. */
        Sounded notes[256];
        size_t count = sounded(plan, PLAN_LANE_MELODY, span, notes, 256);
        bool tones = count > 0;
        for (size_t i = 0; i < count; i++) {
            for (size_t bar = 0; bar < 4; bar++) {
                Rational downbeat;
                assert(rational_make((long)bar, 1, &downbeat) == RATIONAL_OK);
                if (rational_cmp(notes[i].onset, downbeat) == 0) {
                    int pc = ((notes[i].pitch % 12) + 12) % 12;
                    bool found = false;
                    for (size_t t = 0; t < 3; t++) {
                        found = found || CHORD_TONES[bar][t] == pc;
                    }
                    tones = tones && found;
                }
            }
        }
        on_downbeat += tones;
        plan_destroy(plan);
    }
    printf("progression kept in %zu of %d (%zu stuck), chord tones on the downbeats in %zu\n", kept, SEEDS, stuck, on_downbeat);
    assert(stuck <= 1 && kept == SEEDS - stuck);
    assert(on_downbeat + 2 >= kept);
}

/* A progression that ends before the section does: `I IV V I` in 8 bars; the harmony stage writes the other 4. */
static void test_partial(void) {
    static const char *const CHORDS[4] = {"I", "IV", "V", "I"};
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Fixture fixture;
        PiecePlan *plan;
        ComposeConfig config;
        ComposeOutcome outcome;
        fixture_init(&fixture, 0, KEY_STRUCTURE_MODE_MAJOR);
        PieceSection *body = add_section(&fixture, "p", "A", PIECE_PART_BODY, 32, PIECE_FILL_PROGRESSION);
        body->numerals = CHORDS;
        body->numeral_count = 4;
        body->cells_per_chord = 2;
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
        NodeId section;
        assert(piece_sections(plan, &section, 1) == 1);
        GivenSnapshot *snapshot = given_snapshot(plan);
        assert(snapshot);
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(compose_run(plan, &config, &outcome) == PIECE_OK);
        compose_outcome_destroy(&outcome);
        assert(runs_unchanged(snapshot, plan) > 0);
        given_snapshot_destroy(snapshot);

        /* The four chords Given, then chords the harmony stage wrote to the end of the section. */
        LaneSpanId chords[32];
        size_t count = lane_spans(plan, PLAN_LANE_HARMONY, plan_node_span(plan, section), chords, 32);
        assert(count > 4);
        for (size_t i = 0; i < count; i++) {
            assert((plan_lane_span_state(plan, chords[i]) == PLAN_STATE_GIVEN) == (i < 4));
            assert(plan_lane_span_state(plan, chords[i]) != PLAN_STATE_CONSTRAINED);
        }
        plan_destroy(plan);
    }
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "fixture") == 0) {
        test_fixture();
    } else if (strcmp(mode, "theme") == 0) {
        test_theme();
    } else if (strcmp(mode, "refused") == 0) {
        test_refused();
    } else if (strcmp(mode, "progression") == 0) {
        test_progression();
    } else if (strcmp(mode, "partial") == 0) {
        test_partial();
    } else {
        fprintf(stderr, "usage: test_piece_fills fixture|theme|refused|progression|partial\n");
        return 2;
    }
    printf("test_piece_fills %s: ok\n", mode);
    return 0;
}
