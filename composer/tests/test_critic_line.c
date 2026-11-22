#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/critic.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/render.h>

#include "api/api_grow.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The melody line the critic judges (R-719). A motive with a pickup (the motive member of `ursatz listen --what motifs --count 1
 * --seed 70 --settings r8-a.json`, pickup 1/4) has its first note in bar 0 and the rest in bars 1 and 2, positions from each bar's
 * start; one without a pickup starts at bar 1; a rhythm motive sounds on its tonic. A period grown through api_grow_run has the two pairs of
 * ideas, by the names grow gives a period's ideas with development.functions.contrast off and by the contrast constraints with it on.
 */

static Motive load_motive(const char *path) {
    char *text = test_must_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(motive_from_json(text, test_motive_options(), &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

static void check_note(const CriticLine *line, size_t index, int bar, long long position_num, long long position_den) {
    assert(line->notes[index].bar == bar);
    assert(line->notes[index].position.num == position_num && line->notes[index].position.den == position_den);
}

static void test_motive_with_a_pickup(void) {
    Motive motive = load_motive(COMPOSER_FIXTURE_DIR "/critic/motif-pickup.json");
    CriticLine line;
    assert(critic_line_from_motive(&motive, &line) == CRITIC_OK);
    assert(line.count == 8 && line.pair_count == 0);
    assert(line.beat.num == 1 && line.beat.den == 4);
    int bars[8] = {0, 1, 1, 1, 1, 1, 2, 2};
    for (size_t i = 0; i < 8; i++) {
        assert(line.notes[i].bar == bars[i]);
        assert(line.notes[i].pitch == motive_pitch(&motive, i));
        assert(rational_equals(line.notes[i].onset, motive.notes[i].onset) && rational_equals(line.notes[i].duration, motive.notes[i].duration));
    }
    check_note(&line, 0, 0, 0, 1);
    check_note(&line, 1, 1, 0, 1);
    check_note(&line, 2, 1, 3, 8);
    check_note(&line, 5, 1, 3, 4);
    check_note(&line, 6, 2, 0, 1);
    check_note(&line, 7, 2, 1, 2);
    critic_line_destroy(&line);
    assert(line.notes == NULL && line.count == 0);
    critic_line_destroy(&line);
}

static void test_motive_without_a_pickup(void) {
    Motive motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77-chords.json");
    CriticLine line;
    assert(critic_line_from_motive(&motive, &line) == CRITIC_OK);
    assert(line.count == motive.note_count && line.notes[0].bar == 1 && line.notes[0].position.num == 0);
    for (size_t i = 1; i < line.count; i++) {
        assert(line.notes[i].bar >= line.notes[i - 1].bar);
    }
    critic_line_destroy(&line);
    Motive empty = motive;
    empty.note_count = 0;
    assert(critic_line_from_motive(&empty, &line) == CRITIC_ERR_EMPTY);
    assert(critic_line_from_motive(NULL, &line) == CRITIC_ERR_NULL && critic_line_from_motive(&motive, NULL) == CRITIC_ERR_NULL);
    Motive rhythm = motive;
    for (size_t i = 0; i < rhythm.note_count; i++) {
        rhythm.notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
    }
    /* a rhythm motive sounds on its tonic, as its render does */
    assert(critic_line_from_motive(&rhythm, &line) == CRITIC_OK && line.count == rhythm.note_count);
    for (size_t i = 0; i < line.count; i++) {
        assert(line.notes[i].pitch == 60 + (rhythm.has_scale ? rhythm.tonic_pc : 0));
    }
    critic_line_destroy(&line);
}

/* The 8-bar period of the motive and a seed through api_grow_run; false when the seed makes none. */
static bool period_of(const Motive *motive, unsigned long seed, const GrowFunctions *functions, GrowBriefText *text, GrowSetup *setup,
                      GrowRun *run) {
    memset(text, 0, sizeof(*text));
    text->key = "C major";
    text->meter = "4/4";
    text->cadence = "PAC";
    text->into = "period";
    memset(setup, 0, sizeof(*setup));
    setup->knowledge = test_knowledge();
    setup->text = text;
    setup->into = GROW_INTO_PERIOD;
    setup->pairing.beginning = SECTION_BEGINNING_ANTECEDENT;
    setup->pairing.ending = SECTION_ENDING_CONSEQUENT;
    setup->key = motive_key(motive);
    setup->meter = motive_meter(motive);
    setup->bars = 8;
    setup->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup->first_cadence = CADENCE_TYPE_HALF;
    setup->motive = motive;
    setup->functions = functions;
    setup->suggest = 1;
    setup->seed = seed;
    setup->tempo_from = "none";
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    if (api_grow_run(setup, run, &error) != URSATZ_OK || run->made != 1) {
        api_grow_run_destroy(run);
        return false;
    }
    return true;
}

static size_t contrast_constraints(const Plan *plan) {
    size_t count = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        count += plan_has_node(plan, id) && plan_node_find_constraint(plan, id, PLAN_CONSTRAINT_CONTRAST) != NULL ? 1 : 0;
    }
    return count;
}

/* The line of the first period a seed from 1 to 6 makes, with the constraint count of its plan; false when none is made. */
static bool period_line(const Motive *motive, const GrowFunctions *functions, CriticLine *line, size_t *constraints) {
    for (unsigned long seed = 1; seed <= 6; seed++) {
        GrowBriefText text;
        GrowSetup setup;
        GrowRun run;
        if (!period_of(motive, seed, functions, &text, &setup, &run)) {
            continue;
        }
        const Plan *plan = run.themes[0].plan;
        RenderedPlan rendered;
        memset(&rendered, 0, sizeof(rendered));
        assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
        assert(critic_line_from_render(plan, &rendered, line) == CRITIC_OK);
        *constraints = contrast_constraints(plan);
        rendered_plan_destroy(&rendered);
        api_grow_run_destroy(&run);
        return true;
    }
    return false;
}

static void check_pairs(const CriticLine *line) {
    assert(line->pair_count == 2);
    assert(strcmp(line->pairs[0].idea, "contrasting idea") == 0 && strcmp(line->pairs[0].against, "basic idea") == 0);
    assert(strcmp(line->pairs[1].idea, "consequent contrasting idea") == 0 && strcmp(line->pairs[1].against, "consequent basic idea") == 0);
    for (size_t i = 0; i < line->pair_count; i++) {
        const CriticIdeaPair *pair = &line->pairs[i];
        assert(pair->idea_count > 0 && pair->against_count > 0);
        assert(pair->idea_first + pair->idea_count <= line->count && pair->against_first + pair->against_count <= line->count);
        assert(pair->idea_length.num > 0 && pair->against_length.num > 0);
        /* the notes of an idea start inside its span */
        for (size_t n = 0; n < pair->idea_count; n++) {
            Rational onset = line->notes[pair->idea_first + n].onset;
            Rational end;
            assert(rational_add(pair->idea_start, pair->idea_length, &end) == RATIONAL_OK);
            assert(rational_cmp(onset, pair->idea_start) >= 0 && rational_cmp(onset, end) < 0);
        }
    }
}

static void test_period_pairs(void) {
    Motive motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77-chords.json");
    GrowFunctions off;
    GrowFunctions on;
    memset(&off, 0, sizeof(off));
    memset(&on, 0, sizeof(on));
    on.contrast = true;
    CriticLine line;
    size_t constraints = 0;
    assert(period_line(&motive, &off, &line, &constraints));
    assert(constraints == 0);
    check_pairs(&line);
    assert(line.count > 8 && line.notes[0].bar == 1);
    critic_line_destroy(&line);
    assert(period_line(&motive, &on, &line, &constraints));
    assert(constraints == 2);
    check_pairs(&line);
    critic_line_destroy(&line);
}

int main(void) {
    test_motive_with_a_pickup();
    test_motive_without_a_pickup();
    test_period_pairs();
    printf("test_critic_line: ok\n");
    return 0;
}
