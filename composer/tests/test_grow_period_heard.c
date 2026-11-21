#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>

#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <plan/plan_state.h>
#include "support/test_composer.h"
#include "support/test_knowledge.h"

/*
 * The grown periods and hybrids, heard. The same runs as test_grow_period (four keys and meters, seeds 1 to 10) for
 * one pairing of a beginning and an ending: form_hear, the way standard-form's adapter hears real music, reads each
 * plan's lanes alone, and the theme it finds must be the form the pairing makes: a period for an antecedent and a
 * consequent, a hybrid with the requested beginning and ending for the rest. The shares are printed.
 *
 * Only themes of 8 bars are held to a floor. The adapter cuts a phrase into ideas of two measures, so a theme of 4 bars is
 * two ideas, which it reads as one part, never as a theme: those are printed, and documented in docs/grow.md.
 *
 * The shares are held to a baseline table (R-698): a pairing fails when the themes heard as requested fall below the
 * floor or the runs with no theme rise above the ceiling, and passes when a number holds or improves. The target,
 * every grown theme heard as the form it was grown as, is printed and does not gate.
 */

typedef struct {
    const char *name;
    int tonic;
    HarmonyMode mode;
    int beats;
    int unit;
    const char *motive;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, HARMONY_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"D minor 3/4", 2, HARMONY_MODE_MINOR, 3, 4, "F4:q A4:e G4:e E4:q"},
    {"G major 6/8", 7, HARMONY_MODE_MAJOR, 6, 8, "B4:q A4:e G4:e F#4:e D4:e"},
    {"F major 2/4", 5, HARMONY_MODE_MAJOR, 2, 4, "A4:q C5:e Bb4:e"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

typedef struct {
    size_t runs;
    size_t as_requested;
    size_t right_form_wrong_parts; /* a hybrid heard with another beginning or ending */
    size_t other_form;
    size_t nothing;
} Share;

static PhraseBeginning beginning_of(SectionBeginning beginning) {
    switch (beginning) {
    case SECTION_BEGINNING_ANTECEDENT:
        return PHRASE_BEGINNING_ANTECEDENT;
    case SECTION_BEGINNING_COMPOUND_BASIC_IDEA:
        return PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
    case SECTION_BEGINNING_PRESENTATION:
        return PHRASE_BEGINNING_PRESENTATION;
    }
    assert(0);
    return PHRASE_BEGINNING_COUNT;
}

static PhraseEnding ending_of(SectionEnding ending) {
    switch (ending) {
    case SECTION_ENDING_CONTINUATION:
        return PHRASE_ENDING_CONTINUATION;
    case SECTION_ENDING_CONSEQUENT:
        return PHRASE_ENDING_CONSEQUENT;
    case SECTION_ENDING_CADENTIAL:
        return PHRASE_ENDING_CADENTIAL;
    }
    assert(0);
    return PHRASE_ENDING_COUNT;
}

/* One line of heard.txt, written by test_grow_period for the same pairing: what grow_run heard when it grew the plan. */
typedef struct {
    char key[32];
    size_t count;
    int requested_form;
    int as_requested;
} Written;

#define WRITTEN_MAX 128

static size_t read_written(Written *written) {
    FILE *file = fopen("heard.txt", "rb");
    assert(file);
    size_t n = 0;
    while (n < WRITTEN_MAX &&
           fscanf(file, "%31s %zu %d %d", written[n].key, &written[n].count, &written[n].requested_form,
                  &written[n].as_requested) == 4) {
        n++;
    }
    assert(n == 80 && "four scenarios, two sizes, seeds 1 to 10");
    fclose(file);
    return n;
}

static void hear_one(size_t scenario, size_t bars, unsigned long seed, SectionBeginning beginning,
                     SectionEnding ending, const Written *written, size_t written_count, Share *share) {
    char key[32];
    char path[64];
    Plan *plan = NULL;
    FormHeard heard;
    snprintf(key, sizeof(key), "s%zu-%zu-%lu", scenario, bars, seed);
    snprintf(path, sizeof(path), "%s.plan.json", key);
    assert(test_read_plan_file(path, &plan, NULL) == PLAN_OK);
    const Written *line = NULL;
    for (size_t i = 0; i < written_count; i++) {
        if (strcmp(written[i].key, key) == 0) {
            line = &written[i];
        }
    }
    assert(line);

    PhraseLevelForm wanted;
    bool period = beginning == SECTION_BEGINNING_ANTECEDENT && ending == SECTION_ENDING_CONSEQUENT;
    bool sentence = beginning == SECTION_BEGINNING_PRESENTATION && ending == SECTION_ENDING_CONTINUATION;
    wanted = period ? PHRASE_LEVEL_FORM_PERIOD : (sentence ? PHRASE_LEVEL_FORM_SENTENCE : PHRASE_LEVEL_FORM_HYBRID);

    PlanSpan theme = plan_node_span(plan, test_find_node(plan, "theme"));
    assert(test_form_hear(plan, theme, &heard) == FORM_HEAR_OK);
    share->runs++;
    bool as_requested = false;
    if (heard.count == 0) {
        share->nothing++;
    } else if (heard.themes[0].form != (int)wanted) {
        share->other_form++;
    } else if (wanted == PHRASE_LEVEL_FORM_HYBRID && (heard.themes[0].beginning != (int)beginning_of(beginning) ||
                                                       heard.themes[0].ending != (int)ending_of(ending))) {
        share->right_form_wrong_parts++;
    } else {
        share->as_requested++;
        as_requested = true;
    }
    /* grow_run heard the same plan when test_grow_period grew it, and said the same. */
    assert(line->count == heard.count);
    assert(line->requested_form == (int)wanted);
    assert((line->as_requested == 1) == as_requested);
    form_heard_destroy(&heard);
    plan_destroy(plan);
}

/*
 * What the runs heard at 243bd316 (seeds 1 to 10, four scenarios, so 40 runs per size; the runs are deterministic), per
 * beginning and ending (the order of SectionBeginning and SectionEnding), 8 bars: as requested / a hybrid with other
 * parts / another form / no theme. The floor is the first number and the ceiling the last. A presentation and a
 * continuation is the sentence, heard by test_grow_sentence_heard. Raise a baseline when a change lifts it.
 *   presentation + consequent 1/28/10/1, presentation + cadential 0/29/9/2,
 *   antecedent + continuation 0/2/35/3, antecedent + consequent 32/0/3/5, antecedent + cadential 0/2/38/0,
 *   compound basic idea + continuation 32/0/0/8, + consequent 3/37/0/0, + cadential 0/37/0/3.
 * At 4 bars every pairing is 0/0/0/40: the adapter reads two ideas of two measures as one part.
 */
typedef struct {
    size_t floor_as_requested;
    size_t ceiling_nothing;
} Baseline;

static const Baseline BASELINES_8[3][3] = {
    {{0, 0}, {1, 1}, {0, 2}},
    {{0, 3}, {32, 5}, {0, 0}},
    {{32, 8}, {3, 0}, {0, 3}},
};
static const Baseline BASELINE_4 = {0, 40};

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: test_grow_period_heard <beginning 0-2> <ending 0-2>\n", stderr);
        return 2;
    }
    SectionBeginning beginning = (SectionBeginning)atoi(argv[1]);
    SectionEnding ending = (SectionEnding)atoi(argv[2]);
    assert(beginning >= SECTION_BEGINNING_PRESENTATION && beginning <= SECTION_BEGINNING_COMPOUND_BASIC_IDEA);
    assert(ending >= SECTION_ENDING_CONTINUATION && ending <= SECTION_ENDING_CADENTIAL);

    Written written[WRITTEN_MAX];
    size_t written_count = read_written(written);
    Share share[2];
    memset(share, 0, sizeof(share));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (unsigned long seed = 1; seed <= 10; seed++) {
            hear_one(s, 8, seed, beginning, ending, written, written_count, &share[0]);
            hear_one(s, 4, seed, beginning, ending, written, written_count, &share[1]);
        }
    }
    static const size_t BARS[2] = {8, 4};
    for (int i = 0; i < 2; i++) {
        printf("%zu bars: heard as requested %zu of %zu (%zu%%), a hybrid with other parts %zu, another form %zu, no "
               "theme %zu\n",
               BARS[i], share[i].as_requested, share[i].runs, share[i].as_requested * 100 / share[i].runs,
               share[i].right_form_wrong_parts, share[i].other_form, share[i].nothing);
    }
    const Baseline *baselines[2] = {&BASELINES_8[beginning][ending], &BASELINE_4};
    int failed = 0;
    for (int i = 0; i < 2; i++) {
        if (share[i].as_requested < baselines[i]->floor_as_requested) {
            printf("%zu bars: heard as requested %zu, below the baseline %zu\n", BARS[i], share[i].as_requested,
                   baselines[i]->floor_as_requested);
            failed = 1;
        }
        if (share[i].nothing > baselines[i]->ceiling_nothing) {
            printf("%zu bars: no theme %zu, above the baseline %zu\n", BARS[i], share[i].nothing,
                   baselines[i]->ceiling_nothing);
            failed = 1;
        }
    }
    puts("target: every grown theme heard as the form it was grown as (not gating)");
    if (failed) {
        puts("test_grow_period_heard: a number is worse than its baseline");
        return 1;
    }
    puts("test_grow_period_heard: ok");
    return 0;
}
