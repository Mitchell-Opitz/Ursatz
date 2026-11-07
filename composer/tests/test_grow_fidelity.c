#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>

#include "api/api_grow.h"
#include "grow/grow_internal.h"
#include "support/test_support.h"
#include "util/plan_util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * An idea's fidelity read from the plan (B1 of P37, R-354): the Test Motif as an 8-bar sentence and the two-bar 3/4 motif as
 * an 8-bar period, seeds 1 to 3. Every idea with a role has a source and realized notes; the basic idea's realized notes
 * are m0's rhythm and, within R-328's bend, its pitches (R-326); an idea whose ref names a segment has that segment for its
 * source. Each idea's role, score and heard are printed, for B2's measurement; they are not asserted.
 */

#define SEEDS 3
/* How far a realized note of the basic idea may be from m0's (R-328: a clash moves to the nearest chord tone, two semitones in a triad). */
#define BEND_LIMIT 2

typedef struct {
    const char *name;
    const char *selector; /* the argument that runs only this case */
    const char *file;
    const char *key;
    const char *meter;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
} Case;

static const Case CASES[] = {
    {"test-motif sentence", "sentence", COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", "C major", "6/8", PLAN_THEME_BEGINNING_PRESENTATION,
     PLAN_THEME_ENDING_CONTINUATION},
    {"two-bar-3-4 period", "period", COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", "A minor", "3/4", PLAN_THEME_BEGINNING_ANTECEDENT,
     PLAN_THEME_ENDING_CONSEQUENT}};

static const char *const ROLE_NAMES[] = {"none", "presentation", "continuation", "cadential", "contrast"};

static Motive load_motive(const char *path) {
    char *text = test_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

static int pitch_class(int midi) {
    return ((midi % 12) + 12) % 12;
}

static int distance(int a, int b) {
    return a > b ? a - b : b - a;
}

/* Whether two pitches are the same pitch class or, with the bend, within BEND_LIMIT semitones of one. */
static bool same_or_bent(int a, int b) {
    int up = pitch_class(a - b);
    return up <= BEND_LIMIT || 12 - up <= BEND_LIMIT;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    size_t scored = 0;
    size_t from_segments = 0;
    size_t run_cases = 0;
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        /* Recognizing is searched, and the two cases together run past the 10 s limit: ctest runs each on its own. */
        if (argc > 1 && strcmp(argv[1], CASES[c].selector) != 0) {
            continue;
        }
        run_cases++;
        Motive motive = load_motive(CASES[c].file);
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            GrowBriefText text;
            memset(&text, 0, sizeof(text));
            text.key = CASES[c].key;
            text.meter = CASES[c].meter;
            text.cadence = "PAC";
            GrowSetup setup;
            memset(&setup, 0, sizeof(setup));
            setup.text = &text;
            setup.into = CASES[c].beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? GROW_INTO_PERIOD : GROW_INTO_HYBRID;
            setup.pairing.beginning = CASES[c].beginning;
            setup.pairing.ending = CASES[c].ending;
            setup.key = motive_key(&motive);
            setup.meter = motive_meter(&motive);
            setup.bars = 8;
            setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
            setup.first_cadence = CADENCE_TYPE_HALF;
            setup.motive = &motive;
            setup.suggest = 1;
            setup.seed = seed;
            GrowRun run;
            ComposerError error;
            memset(&error, 0, sizeof(error));
            assert(api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1);
            const PiecePlan *plan = run.themes[0].plan;
            NodeId source_node = grow_find_source(plan);
            assert(source_node != PLAN_NODE_NONE);
            Motive m0;
            assert(grow_read_motive(plan, source_node, &setup.meter, &setup.key, &m0));

            for (NodeId idea = 1; idea <= plan_node_count(plan); idea++) {
                LaneSpanId span;
                const char *name = plan_node_name(plan, idea);
                GrowIdeaRole role = plan_node_kind(plan, idea) == PLAN_NODE_IDEA ? grow_idea_role(name) : GROW_ROLE_NONE;
                if (role == GROW_ROLE_NONE || !plan_melody_span_of(plan, idea, &span)) {
                    continue;
                }
                Motive source;
                Motive realized;
                PlanMotiveRef ref;
                bool from_segment = false;
                MotiveFidelity fidelity;
                memset(&fidelity, 0, sizeof(fidelity));
                bool read = grow_idea_fidelity_source(plan, span, &motive, &source, &ref, &from_segment) &&
                            grow_idea_realized(plan, span, &realized) &&
                            motive_fidelity(&source, &realized, &ref, from_segment, &fidelity);
                if (!read) {
                    fprintf(stderr, "%s seed %lu: idea \"%s\" has no source or no realized notes\n", CASES[c].name, seed, name);
                }
                assert(read);
                scored++;
                printf("%-20s seed %lu  %-27s %-12s score %2lu/%-2lu  %s\n", CASES[c].name, seed, name, ROLE_NAMES[role],
                       fidelity.score.numerator, fidelity.score.denominator, fidelity.heard);

                /* A ref that names a segment makes the segment its source, and only then. */
                assert(from_segment == (ref.segment[0] != '\0'));
                if (from_segment) {
                    assert(source.note_count < m0.note_count);
                    from_segments++;
                }
                /* The basic idea is m0: its rhythm exactly, its pitches but for R-328's bend (R-326). */
                if (strcmp(name, "basic idea") == 0) {
                    assert(realized.note_count == m0.note_count);
                    for (size_t n = 0; n < m0.note_count; n++) {
                        assert(rational_cmp(realized.notes[n].onset, m0.notes[n].onset) == 0);
                        assert(rational_cmp(realized.notes[n].duration, m0.notes[n].duration) == 0);
                        if (!same_or_bent(realized.notes[n].pitch, m0.notes[n].pitch)) {
                            fprintf(stderr, "%s seed %lu: basic idea note %zu is %d, m0 has %d\n", CASES[c].name, seed, n + 1,
                                    realized.notes[n].pitch, m0.notes[n].pitch);
                        }
                        assert(same_or_bent(realized.notes[n].pitch, m0.notes[n].pitch));
                        assert(distance(realized.notes[n].pitch, m0.notes[n].pitch) <= BEND_LIMIT ||
                               pitch_class(realized.notes[n].pitch) == pitch_class(m0.notes[n].pitch));
                    }
                    assert(!from_segment);
                }
            }
            api_grow_run_destroy(&run);
        }
    }
    /* Every case has ideas to score; the sentence's fragments come from segments. */
    assert(run_cases > 0 && scored >= 4 * SEEDS * run_cases);
    assert(from_segments > 0 || (argc > 1 && strcmp(argv[1], "sentence") != 0));
    return 0;
}
