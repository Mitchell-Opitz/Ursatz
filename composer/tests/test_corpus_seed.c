#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/corpus_seed.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>

/*
 * Seeding from a corpus (corpus_seed.h, docs/style.md). The corpus is the output directory of `ursatz-analyzer --corpus`
 * over the two sonatinas (Clementi 36 and Mozart 545), analyzed by CMake when the test is built. The passage is the
 * opening bar of Mozart 545 (tests/fixtures/mozart545-bar1.mid, C major 4/4: C5 half, E5 quarter, G5 quarter).
 *
 *   motive       the passage seeds a motive equal to that bar in D major, in A minor the first note keeps its degree and
 *                the intervals are the bar's; the piece, measures and score are kept
 *   same         the same seed gives the same choice, every seed 1 to 20 picks a match within 1/10 of the best
 *   progression  the best match's numerals; a seed placed at B in a rounded binary form fills completely for seeds 1 to 5
 *   plan         the plan and the grown theme record the source on the motive node, and the plan JSON reads back
 *   no-match     a corpus with nothing analyzed exits 2 and writes nothing
 *   cli          the usage errors
 */

#ifndef COMPOSER_CLI_PATH
#error "COMPOSER_CLI_PATH is not defined"
#endif
#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif
#ifndef COMPOSER_SONATINAS_CORPUS
#error "COMPOSER_SONATINAS_CORPUS is not defined"
#endif
#ifndef COMPOSER_EMPTY_CORPUS
#error "COMPOSER_EMPTY_CORPUS is not defined"
#endif

#define PASSAGE COMPOSER_FIXTURE_DIR "/mozart545-bar1.mid"

static PlanMeter four_four(void) {
    PlanMeter meter = {4, 4, 0};
    return meter;
}

static PlanKey key_of(int tonic, PlanKeyMode mode) {
    PlanKey key = {tonic, mode};
    return key;
}

static Motive bar_in(const PlanMeter *meter, const PlanKey *key, const char *notes) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = *meter;
    options.has_key = true;
    options.key = *key;
    Motive motive;
    MotiveError error;
    MotiveResult result = motive_from_notes_text(notes, &options, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "%s: %s\n", motive_result_name(result), error.message);
    }
    assert(result == MOTIVE_OK);
    return motive;
}

/* ------------------------------------------------------------------ motive */

static int test_motive(void) {
    PlanMeter meter = four_four();
    PlanKey d_major = key_of(2, PLAN_MODE_MAJOR);
    CorpusSeed seed;
    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &meter, &d_major, 1, &seed) == CORPUS_SEED_OK);

    /* The bar of Mozart 545, a whole tone higher: D5 half, F#5 quarter, A5 quarter. */
    Motive expected = bar_in(&meter, &d_major, "D5:h F#5:q A5:q");
    expected.key_source = MOTIVE_KEY_GIVEN;
    if (!motive_equal(&seed.motive, &expected)) {
        char *got = motive_to_json(&seed.motive);
        fprintf(stderr, "the seeded motive is not the bar in D major:\n%s\n", got ? got : "(null)");
        free(got);
        return 1;
    }
    assert(strcmp(seed.piece, "Mozart 545.mid") == 0);
    assert(seed.start_measure >= 1 && seed.end_measure >= seed.start_measure);
    assert(seed.score_denominator > 0 && 2 * seed.score_numerator >= seed.score_denominator);
    /* The passage is in the corpus note for note, so the best match is exact. */
    assert(seed.score_numerator == seed.score_denominator);

    /* In the key it was heard in, nothing moves. */
    PlanKey c_major = key_of(0, PLAN_MODE_MAJOR);
    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &meter, &c_major, 1, &seed) == CORPUS_SEED_OK);
    expected = bar_in(&meter, &c_major, "C5:h E5:q G5:q");
    expected.key_source = MOTIVE_KEY_GIVEN;
    assert(motive_equal(&seed.motive, &expected));

    /* In A minor the first note keeps its degree (the tonic, A) and the intervals are the bar's. */
    PlanKey a_minor = key_of(9, PLAN_MODE_MINOR);
    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &meter, &a_minor, 1, &seed) == CORPUS_SEED_OK);
    assert(seed.motive.note_count == 3);
    assert(seed.motive.notes[0].midi_pitch % 12 == 9);
    assert(seed.motive.notes[1].midi_pitch - seed.motive.notes[0].midi_pitch == 4);
    assert(seed.motive.notes[2].midi_pitch - seed.motive.notes[1].midi_pitch == 3);
    assert(seed.motive.key.tonic_pitch_class == 9 && seed.motive.key.mode == PLAN_MODE_MINOR);

    /* Cut at the barline: a 6/8 bar (three quarters) takes the half note and the quarter, which ends on the barline;
       a 5/8 bar takes the half note and a quarter shortened to the eighth that is left. */
    PlanMeter six_eight = {6, 8, 0};
    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &six_eight, &c_major, 1, &seed) == CORPUS_SEED_OK);
    assert(seed.motive.note_count == 2);
    assert(plan_time_compare(seed.motive.length, (PlanTime){3, 4}) == 0);
    assert(plan_time_compare(seed.motive.notes[0].duration, (PlanTime){1, 2}) == 0);
    assert(plan_time_compare(seed.motive.notes[1].duration, (PlanTime){1, 4}) == 0);
    PlanMeter five_eight = {5, 8, 0};
    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &five_eight, &c_major, 1, &seed) == CORPUS_SEED_OK);
    assert(seed.motive.note_count == 2);
    assert(plan_time_compare(seed.motive.length, (PlanTime){5, 8}) == 0);
    assert(plan_time_compare(seed.motive.notes[1].offset, (PlanTime){1, 2}) == 0);
    assert(plan_time_compare(seed.motive.notes[1].duration, (PlanTime){1, 8}) == 0);

    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, "no-such-file.mid", &meter, &d_major, 1, &seed) ==
           CORPUS_SEED_ERR_QUERY);
    assert(corpus_seed_motive("no-such-directory", PASSAGE, &meter, &d_major, 1, &seed) == CORPUS_SEED_ERR_CORPUS);
    printf("the opening bar of Mozart 545 seeds the bar transposed, cut at the barline\n");
    return 0;
}

/* -------------------------------------------------------------------- same */

static int test_same(void) {
    PlanMeter meter = four_four();
    PlanKey d_major = key_of(2, PLAN_MODE_MAJOR);
    CorpusSeed best;
    assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &meter, &d_major, 0, &best) == CORPUS_SEED_OK);
    for (unsigned long seed = 1; seed <= 20; seed++) {
        CorpusSeed first, second;
        assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &meter, &d_major, seed, &first) == CORPUS_SEED_OK);
        assert(corpus_seed_motive(COMPOSER_SONATINAS_CORPUS, PASSAGE, &meter, &d_major, seed, &second) == CORPUS_SEED_OK);
        assert(strcmp(first.piece, second.piece) == 0 && first.start_measure == second.start_measure &&
               first.start_beat == second.start_beat && first.end_measure == second.end_measure &&
               first.score_numerator == second.score_numerator && first.score_denominator == second.score_denominator);
        assert(motive_equal(&first.motive, &second.motive));
        /* Within 1/10 of the best match: the exact one scores 1. */
        assert(10 * first.score_numerator >= 9 * first.score_denominator);
    }
    printf("the same seed picks the same match, and every pick is within 1/10 of the best\n");
    return 0;
}

/* ------------------------------------------------------------------ the cli */

static int run(const char *arguments) {
    char command[4096];
    snprintf(command, sizeof(command), "\"%s\" %s >seed_test_out.txt 2>seed_test_err.txt", COMPOSER_CLI_PATH, arguments);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static bool exists(const char *path) {
    return access(path, F_OK) == 0;
}

static char *slurp(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = calloc((size_t)size + 1, 1);
    assert(text && fread(text, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    return text;
}

static bool output_contains(const char *needle) {
    char *text = slurp("seed_test_out.txt");
    bool found = strstr(text, needle) != NULL;
    free(text);
    return found;
}

static bool error_contains(const char *needle) {
    char *text = slurp("seed_test_err.txt");
    bool found = strstr(text, needle) != NULL;
    free(text);
    return found;
}

/* Whether the plan has a SOURCE constraint on a node, and on which kind of node it is, and the piece it names. */
static bool source_of(const PiecePlan *plan, NodeId *node_out, char piece[PLAN_SOURCE_PIECE_MAX]) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        for (size_t c = 0; c < plan_node_constraint_count(plan, id); c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, c);
            if (constraint->kind == PLAN_CONSTRAINT_SOURCE) {
                *node_out = id;
                snprintf(piece, PLAN_SOURCE_PIECE_MAX, "%s", constraint->params.source.piece);
                return true;
            }
        }
    }
    return false;
}

static int test_plan(void) {
    remove("seed_plan.json");
    assert(run("plan --form rounded-binary --key \"D major\" --meter 4/4 --from-corpus \"" COMPOSER_SONATINAS_CORPUS
               "\" --like \"" PASSAGE "\" --seed 3 --out seed_plan.json") == 0);
    assert(output_contains("motive from Mozart 545.mid m."));
    assert(output_contains(", score "));
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file("seed_plan.json", &plan, &error) == PLAN_OK);
    NodeId node;
    char piece[PLAN_SOURCE_PIECE_MAX];
    assert(source_of(plan, &node, piece));
    assert(plan_node_kind(plan, node) == PLAN_NODE_MOTIVE);
    assert(strcmp(plan_node_name(plan, node), "m0") == 0);
    assert(strcmp(piece, "Mozart 545.mid") == 0);
    plan_destroy(plan);

    /* The text and the JSON output of the command name the source. */
    assert(run("plan --form binary --key \"D major\" --meter 4/4 --from-corpus \"" COMPOSER_SONATINAS_CORPUS
               "\" --like \"" PASSAGE "\" --seed 3 --out seed_plan.json --format json") == 0);
    assert(output_contains("\"motive_source\":{\"piece\":\"Mozart 545.mid\""));

    /* grow takes the seeded motive instead of an input file, and records the source too. */
    remove("seed_theme.json");
    assert(run("grow --from-corpus \"" COMPOSER_SONATINAS_CORPUS "\" --like \"" PASSAGE
               "\" --into sentence --key \"D major\" --meter 4/4 --seed 7 --out seed_theme.json") == 0);
    assert(output_contains("motive from Mozart 545.mid m."));
    assert(plan_read_json_file("seed_theme.json", &plan, &error) == PLAN_OK);
    assert(source_of(plan, &node, piece));
    assert(plan_node_kind(plan, node) == PLAN_NODE_MOTIVE && strcmp(piece, "Mozart 545.mid") == 0);
    plan_destroy(plan);
    assert(run("grow --from-corpus \"" COMPOSER_SONATINAS_CORPUS "\" --like \"" PASSAGE
               "\" --into sentence --key \"D major\" --meter 4/4 --seed 7 --format json") == 0);
    assert(output_contains("\"source\":{\"piece\":\"Mozart 545.mid\""));
    printf("plan and grow record the source on the motive node\n");
    return 0;
}

static int test_progression(void) {
    CorpusSeed seed;
    assert(corpus_seed_progression(COMPOSER_SONATINAS_CORPUS, "I IV V I", PLAN_MODE_MAJOR, &seed) == CORPUS_SEED_OK);
    assert(seed.numeral_count == 4);
    assert(strlen(seed.piece) > 0 && 2 * seed.score_numerator >= seed.score_denominator);
    for (size_t i = 0; i < seed.numeral_count; i++) {
        assert(strlen(seed.numerals[i]) > 0);
    }
    CorpusSeed minor;
    assert(corpus_seed_progression(COMPOSER_SONATINAS_CORPUS, "i iv V i", PLAN_MODE_MINOR, &minor) == CORPUS_SEED_OK);
    assert(corpus_seed_progression(COMPOSER_SONATINAS_CORPUS, "not numerals", PLAN_MODE_MAJOR, &seed) ==
           CORPUS_SEED_ERR_QUERY);

    /* Placed at B in a rounded binary form, the plan fills completely for seeds 1 to 5. */
    remove("seed_progression.json");
    assert(run("plan --form rounded-binary --key \"D major\" --meter 4/4 --from-corpus \"" COMPOSER_SONATINAS_CORPUS
               "\" --like-progression \"I IV V I\" --at B --seed 2 --out seed_progression.json") == 0);
    assert(output_contains("progression from "));
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_read_json_file("seed_progression.json", &plan, &error) == PLAN_OK);
    NodeId node;
    char piece[PLAN_SOURCE_PIECE_MAX];
    assert(source_of(plan, &node, piece) && plan_node_kind(plan, node) == PLAN_NODE_SECTION);
    plan_destroy(plan);
    for (int seed_number = 1; seed_number <= 5; seed_number++) {
        char arguments[256];
        snprintf(arguments, sizeof(arguments), "fill seed_progression.json --seed %d --out seed_filled.json", seed_number);
        remove("seed_filled.json");
        if (run(arguments) != 0) {
            fprintf(stderr, "fill seed %d failed:\n%s\n", seed_number, slurp("seed_test_err.txt"));
            return 1;
        }
        assert(exists("seed_filled.json"));
    }
    printf("a seeded progression placed at B fills completely for seeds 1 to 5\n");
    return 0;
}

static int test_no_match(void) {
    remove("seed_none.json");
    /* Nothing analyzed in the corpus: exit 2 with the message, nothing written. */
    assert(run("plan --form binary --key \"D major\" --meter 4/4 --from-corpus \"" COMPOSER_EMPTY_CORPUS
               "\" --like \"" PASSAGE "\" --out seed_none.json") == 2);
    assert(error_contains("no passage of the corpus"));
    assert(!exists("seed_none.json"));
    assert(run("plan --form binary --key \"D major\" --meter 4/4 --from-corpus \"" COMPOSER_EMPTY_CORPUS
               "\" --like-progression \"I IV V I\" --at B --out seed_none.json") == 2);
    assert(!exists("seed_none.json"));
    assert(run("grow --from-corpus \"" COMPOSER_EMPTY_CORPUS "\" --like \"" PASSAGE
               "\" --into sentence --key \"D major\" --meter 4/4 --out seed_none.json") == 2);
    assert(error_contains("no passage of the corpus"));
    assert(!exists("seed_none.json"));
    CorpusSeed seed;
    PlanMeter meter = four_four();
    PlanKey d_major = key_of(2, PLAN_MODE_MAJOR);
    assert(corpus_seed_motive(COMPOSER_EMPTY_CORPUS, PASSAGE, &meter, &d_major, 1, &seed) == CORPUS_SEED_ERR_NO_MATCH);
    assert(corpus_seed_progression(COMPOSER_EMPTY_CORPUS, "I IV V I", PLAN_MODE_MAJOR, &seed) == CORPUS_SEED_ERR_NO_MATCH);
    printf("a corpus with no match exits 2 and writes nothing\n");
    return 0;
}

static int test_cli(void) {
    const char *base = "plan --form binary --key \"D major\" --meter 4/4 --out seed_usage.json";
    char arguments[1024];
    snprintf(arguments, sizeof(arguments), "%s --like \"%s\"", base, PASSAGE);
    assert(run(arguments) == 1);
    assert(error_contains("--like needs --from-corpus"));
    snprintf(arguments, sizeof(arguments), "%s --like-progression \"I IV V I\" --at B", base);
    assert(run(arguments) == 1);
    snprintf(arguments, sizeof(arguments), "%s --from-corpus \"%s\"", base, COMPOSER_SONATINAS_CORPUS);
    assert(run(arguments) == 1);
    snprintf(arguments, sizeof(arguments), "%s --from-corpus \"%s\" --like \"%s\" --motive \"%s/motive_d_minor.txt\"", base,
             COMPOSER_SONATINAS_CORPUS, PASSAGE, COMPOSER_FIXTURE_DIR);
    assert(run(arguments) == 1);
    assert(error_contains("both give the motive"));
    snprintf(arguments, sizeof(arguments), "%s --from-corpus \"%s\" --like-progression \"I IV V I\"", base,
             COMPOSER_SONATINAS_CORPUS);
    assert(run(arguments) == 1); /* a given progression needs its --at */
    assert(!exists("seed_usage.json"));

    snprintf(arguments, sizeof(arguments), "grow --like \"%s\" --into sentence --key \"D major\" --meter 4/4", PASSAGE);
    assert(run(arguments) == 1);
    snprintf(arguments, sizeof(arguments), "grow --from-corpus \"%s\" --into sentence --key \"D major\" --meter 4/4",
             COMPOSER_SONATINAS_CORPUS);
    assert(run(arguments) == 1);
    snprintf(arguments, sizeof(arguments),
             "grow \"%s/motive_d_minor.txt\" --from-corpus \"%s\" --like \"%s\" --into sentence --key \"D major\" --meter 4/4",
             COMPOSER_FIXTURE_DIR, COMPOSER_SONATINAS_CORPUS, PASSAGE);
    assert(run(arguments) == 1);
    assert(error_contains("not both"));
    printf("the usage errors are refused\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_corpus_seed motive|same|plan|progression|no-match|cli\n");
        return 2;
    }
    if (strcmp(argv[1], "motive") == 0) {
        return test_motive();
    }
    if (strcmp(argv[1], "same") == 0) {
        return test_same();
    }
    if (strcmp(argv[1], "plan") == 0) {
        return test_plan();
    }
    if (strcmp(argv[1], "progression") == 0) {
        return test_progression();
    }
    if (strcmp(argv[1], "no-match") == 0) {
        return test_no_match();
    }
    if (strcmp(argv[1], "cli") == 0) {
        return test_cli();
    }
    fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
