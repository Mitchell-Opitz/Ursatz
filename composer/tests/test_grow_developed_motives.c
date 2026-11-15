#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/variant_registry.h>

#include "api/api_grow.h"
#include "grow/grow_internal.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Developed motives as concrete notes and chords (B1 of P36): over seeds 1 to 10 of the Test Motif as an 8-bar sentence in
 * C major 6/8 and the 2-bar 3/4 A minor motif as an 8-bar period, every motive node made from another by a step of its ref
 * has notes and a MOTIVE_TONES with a chord, and its notes are what the step makes: exact is the source's pitches and
 * rhythm; a tonal transposition by s moves every pitch and every chord s steps of the scale; a dominant form opens on V;
 * a segment fragment has the segment's rhythm; a contrasting idea is the ref's step applied to the source.
 */

typedef struct {
    const char *file;
    const char *key;
    const char *meter;
    GrowInto into;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
} Case;

static const Case CASES[] = {
    {COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", "C major", "6/8", GROW_INTO_SENTENCE,
     PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION},
    {COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", "A minor", "3/4", GROW_INTO_PERIOD, PLAN_THEME_BEGINNING_ANTECEDENT,
     PLAN_THEME_ENDING_CONSEQUENT}};

static Motive load_motive(const char *path) {
    char *text = test_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

/* The MOTIVE_REF a motive node is made by, to another node; NULL for a node that has none (m0) or refers to itself. */
static const PlanMotiveRef *node_ref(const PiecePlan *plan, NodeId node) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive != node) {
            return &constraint->params.motive;
        }
    }
    return NULL;
}

static const PlanMotiveTones *node_tones(const PiecePlan *plan, NodeId node) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_MOTIVE_TONES);
    return constraint ? &constraint->params.motive_tones : NULL;
}

static const PlanNotes *node_notes(const PiecePlan *plan, NodeId node) {
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, node, &span, 1) >= 1);
    return &plan_lane_span_payload(plan, span)->notes;
}

/* The scale degree (1 to 7) a chord numeral names. */
static int degree_of(const char *numeral) {
    while (*numeral == '#' || *numeral == 'b') {
        numeral++;
    }
    char lower[8];
    size_t n = 0;
    while (numeral[n] && strchr("IVivx", numeral[n]) && n < 7) {
        lower[n] = (char)tolower((unsigned char)numeral[n]);
        n++;
    }
    lower[n] = '\0';
    static const char *const NAMES[] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    assert(!"a chord numeral");
    return 0;
}

/* The scale step of a pitch in the key, counting the octaves. */
static int step_of(int midi, const Motive *key) {
    static const int MAJOR[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const int MINOR[12] = {0, 0, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6};
    int relative = midi - key->tonic_pc;
    int octave = relative >= 0 ? relative / 12 : -((11 - relative) / 12);
    int within = relative - octave * 12;
    return octave * 7 + (key->minor ? MINOR : MAJOR)[within];
}

static bool same_rhythm(const PlanNote *a, const PlanNote *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(a[i].offset, b[i].offset) != 0 || rational_cmp(a[i].duration, b[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/* The name of the idea whose melody asks for the motive node, else "". */
static const char *idea_of(const PiecePlan *plan, NodeId motive) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_node_kind(plan, id) != PLAN_NODE_IDEA) {
            continue;
        }
        LaneSpanId spans[16];
        size_t count = plan_node_lane_spans(plan, id, spans, 16);
        for (size_t s = 0; s < count && s < 16; s++) {
            for (size_t c = 0; plan_lane_span_lane(plan, spans[s]) == PLAN_LANE_MELODY &&
                               c < plan_lane_span_constraint_count(plan, spans[s]);
                 c++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, spans[s], c);
                if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == motive) {
                    return plan_node_name(plan, id);
                }
            }
        }
    }
    return "";
}

static void check_node(const PiecePlan *plan, NodeId node, const PlanMotiveRef *ref, const Motive *basic,
                       const AnalysisKey *key, const PlanMeter *meter, size_t *seen) {
    const PlanNotes *notes = node_notes(plan, node);
    const PlanNotes *source = node_notes(plan, ref->motive);
    const PlanMotiveTones *tones = node_tones(plan, node);
    assert(notes->note_count > 0);
    assert(tones && tones->chord_count >= 1 && tones->label_count == notes->note_count);
    const PlanMotiveTones *source_tones = node_tones(plan, ref->motive);
    MotiveRelationKind kind = ref->has_step ? ref->step.kind : MOTIVE_RELATION_EXACT;

    if (ref->segment[0] != '\0' && ref->steps > 0) {
        /* A liquidated residue names the segment it keeps: its first two durations are the segment's, or both halved (R-345). */
        bool found = false;
        for (size_t f = 0; f < basic->feature_count; f++) {
            if (strcmp(basic->features[f].label, ref->segment) != 0) {
                continue;
            }
            size_t first = basic->features[f].first;
            assert(notes->note_count >= 2);
            bool equal = true;
            bool halved = true;
            for (size_t n = 0; n < 2; n++) {
                Rational doubled = notes->notes[n].duration;
                doubled.num *= 2;
                equal = equal && rational_cmp(notes->notes[n].duration, basic->notes[first + n].duration) == 0;
                halved = halved && rational_cmp(doubled, basic->notes[first + n].duration) == 0;
            }
            assert(equal || halved);
            found = true;
        }
        assert(found);
    } else if (ref->segment[0] != '\0') {
        /* A segment fragment has the segment's rhythm: the notes of m0 from the segment's first to its last. */
        bool found = false;
        for (size_t f = 0; f < basic->feature_count; f++) {
            if (strcmp(basic->features[f].label, ref->segment) != 0) {
                continue;
            }
            size_t first = basic->features[f].first;
            size_t last = basic->features[f].last;
            assert(notes->note_count == last - first + 1);
            for (size_t n = 0; n < notes->note_count; n++) {
                Rational start;
                assert(rational_sub(basic->notes[first + n].onset, basic->notes[first].onset, &start) == RATIONAL_OK);
                assert(rational_cmp(notes->notes[n].offset, start) == 0);
                assert(rational_cmp(notes->notes[n].duration, basic->notes[first + n].duration) == 0);
            }
            found = true;
        }
        assert(found);
        seen[4]++;
    }

    switch (kind) {
    case MOTIVE_RELATION_EXACT:
        assert(notes->note_count == source->note_count && same_rhythm(notes->notes, source->notes, notes->note_count));
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(notes->notes[n].midi == source->notes[n].midi);
        }
        seen[0]++;
        break;
    case MOTIVE_RELATION_TRANSPOSE:
        assert(notes->note_count == source->note_count && same_rhythm(notes->notes, source->notes, notes->note_count));
        if (ref->step.value[0] != 0) {
            long by = ref->step.value[1];
            for (size_t n = 0; n < notes->note_count; n++) {
                assert(step_of(notes->notes[n].midi, basic) - step_of(source->notes[n].midi, basic) == by);
            }
            if (source_tones && source_tones->chord_count == tones->chord_count) {
                for (size_t c = 0; c < tones->chord_count; c++) {
                    int expected = (int)(((degree_of(source_tones->chords[c].numeral) - 1 + by) % 7 + 7) % 7) + 1;
                    assert(degree_of(tones->chords[c].numeral) == expected);
                }
            }
            seen[1]++;
        }
        break;
    case MOTIVE_RELATION_DOMINANT_FORM:
        assert(degree_of(tones->chords[0].numeral) == 5);
        seen[2]++;
        break;
    default:
        break;
    }

    /* A contrasting idea is the ref's own step applied to its source. */
    const char *idea = idea_of(plan, node);
    if (strstr(idea, "contrasting") != NULL) {
        Motive from;
        Motive made;
        VariantChain chain;
        assert(grow_read_motive(plan, ref->motive, meter, key, &from));
        assert(grow_develop_chain(ref, &chain));
        assert(variant_apply_chain(&from, &chain, GROW_DEVELOP_SEED, &made, NULL) == VARIANT_OK);
        assert(made.note_count == notes->note_count);
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(motive_pitch(&made, n) == notes->notes[n].midi);
        }
        seen[3]++;
    }
}

int main(void) {
    size_t seen[5] = {0, 0, 0, 0, 0}; /* exact, tonal transposition, dominant form, contrast, segment */
    size_t nodes = 0;
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        Motive motive = load_motive(CASES[c].file);
        AnalysisKey key = motive_key(&motive);
        PlanMeter meter = motive_meter(&motive);
        for (unsigned long seed = 1; seed <= 10; seed++) {
            GrowBriefText text;
            memset(&text, 0, sizeof(text));
            text.key = CASES[c].key;
            text.meter = CASES[c].meter;
            text.cadence = "PAC";
            GrowSetup setup;
            memset(&setup, 0, sizeof(setup));
            setup.text = &text;
            setup.into = CASES[c].into;
            setup.pairing.beginning = CASES[c].beginning;
            setup.pairing.ending = CASES[c].ending;
            setup.key = key;
            setup.meter = meter;
            setup.bars = 8;
            setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
            setup.first_cadence = CADENCE_TYPE_HALF;
            setup.motive = &motive;
            setup.suggest = 1;
            setup.development = GROW_DEVELOP_BALANCED;
            setup.seed = seed;
            GrowRun run;
            ComposerError error;
            memset(&error, 0, sizeof(error));
            ComposerStatus status = api_grow_run(&setup, &run, &error);
            if (status != COMPOSER_OK || run.made != 1) {
                api_grow_run_destroy(&run);
                continue;
            }
            const PiecePlan *plan = run.themes[0].plan;
            for (NodeId id = 1; id <= plan_node_count(plan); id++) {
                const PlanMotiveRef *ref = plan_node_kind(plan, id) == PLAN_NODE_MOTIVE ? node_ref(plan, id) : NULL;
                if (ref && plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED && idea_of(plan, id)[0] != '\0') {
                    check_node(plan, id, ref, &motive, &key, &meter, seen);
                    nodes++;
                }
            }
            api_grow_run_destroy(&run);
        }
    }
    assert(nodes > 0 && seen[0] > 0 && seen[1] > 0 && seen[3] > 0 && seen[4] > 0);
    printf("test_grow_developed_motives: ok (%zu nodes: %zu exact, %zu tonal, %zu dominant, %zu contrast, %zu segment)\n",
           nodes, seen[0], seen[1], seen[2], seen[3], seen[4]);
    return 0;
}
