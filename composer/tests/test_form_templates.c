#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>
#include "support/test_composer.h"
#include "support/test_knowledge.h"

/*
 * Form templates, every template and size, in C major 4/4 and A minor 3/4, with a motive invented and with the given
 * one (tests/fixtures/motive_d_minor.json; in 3/4 its last note ends on the barline). The minuet and the trio are in
 * 3/4 alone (R-487), so they run in C major and A minor 3/4.
 *
 *   structure <template>  the plan tiles the form with the table's sections in order (letters, bars, pairings,
 *                         cadences, the section each return names, the layers), and sizes not in the table are refused
 *   compose <template>    seeds 1 to 5: how many runs complete and how many theme sections form_hear runs on; neither
 *                         is below its baseline; the runs that do not complete are printed
 *   smoke <template>      the smallest size in C major 4/4 (3/4 for the minuet and the trio), invented: one of seeds 1 to 5
 *                         composes and is heard (quick)
 *   heard <template>      seeds 1 to 5: how many sections are heard as the form they ask for is printed with grow's
 *                         measured shares (docs/grow.md) and is not below its baseline
 */

#define SEEDS 5

typedef struct {
    const char *name;
    int tonic;
    HarmonyMode mode;
    int beats;
    bool given;
} Scenario;

static const Scenario SCENARIOS_ANY[] = {
    {"C major 4/4, invented", 0, HARMONY_MODE_MAJOR, 4, false},
    {"A minor 3/4, invented", 9, HARMONY_MODE_MINOR, 3, false},
    {"C major 4/4, given", 0, HARMONY_MODE_MAJOR, 4, true},
    {"A minor 3/4, given", 9, HARMONY_MODE_MINOR, 3, true},
};
/* The forms that list 3/4 as their only meter. */
static const Scenario SCENARIOS_THREE_FOUR[] = {
    {"C major 3/4, invented", 0, HARMONY_MODE_MAJOR, 3, false},
    {"A minor 3/4, invented", 9, HARMONY_MODE_MINOR, 3, false},
    {"C major 3/4, given", 0, HARMONY_MODE_MAJOR, 3, true},
    {"A minor 3/4, given", 9, HARMONY_MODE_MINOR, 3, true},
};
/* The scenarios of the template being run (set in main). */
static const Scenario *SCENARIOS = SCENARIOS_ANY;
static size_t SCENARIO_COUNT = sizeof(SCENARIOS_ANY) / sizeof(SCENARIOS_ANY[0]);

/* A size of a template: bars, or the count of variations or layers. */
typedef struct {
    size_t bars;
    size_t count;
} Size;

#define P SECTION_BEGINNING_PRESENTATION
#define AN SECTION_BEGINNING_ANTECEDENT
#define CBI SECTION_BEGINNING_COMPOUND_BASIC_IDEA
#define CONT SECTION_ENDING_CONTINUATION
#define CONS SECTION_ENDING_CONSEQUENT
#define CAD SECTION_ENDING_CADENTIAL
#define NONE FORM_SPEC_NO_RETURN
/* The sonata rondo's P and S sit on the region degree of the piece's mode: vi or iv, ii or v (R-520). */
#define DEVELOPMENT 1
#define SEQUENCE 2
/* The sonatina's E sits on IV in major and VI in minor, a major key (R-523). */
#define ELABORATION 3

/* What a section of the table must be. */
typedef struct {
    const char *letter;
    size_t bars;
    SectionBeginning beginning;
    SectionEnding ending;
    CadenceType cadence;
    size_t returns;
    unsigned layers;
    bool liquidation; /* a liquidation section: a LIQUIDATION constraint, no pairing and no archetype (R-494) */
    int key_offset;   /* the section's key above the home key, in semitones: the scherzo's planned keys (R-493) */
    bool parallel;    /* the section is in the home tonic's other mode (R-499): the figured variations' V3 */
    bool has_figure;  /* a figurate return with a FIGURE constraint (R-497): the figured variations' variations */
    SectionFigure figure;
    bool region;      /* the section ends in the form's region (V in major, III in minor): the main and subordinate themes (R-509) */
    int function;     /* a SECTION_FUNCTION constraint of this SectionFunctionKind (R-503), or -1 for none */
    bool lyric;       /* a lyric subordinate theme: m0 augmented (R-506) */
    bool tonic_major; /* the section is in the home tonic's major key (R-517): the rondos' B1 */
    bool second;      /* the section is in the second region (vi or IV in major, VI or iv in minor): the rondos' C (R-520) */
    int region_key;   /* the section is in the key of one degree of its region, in semitones above home (the sonata rondo's P and S, R-520; the sonatina's E, R-523), or -1 */
} Expected;

#define MAX_EXPECTED FORM_SPEC_MAX_SECTIONS

static const unsigned BASS = PLAN_LAYER_BIT(PLAN_LANE_BASS);
static const unsigned ACCOMPANIED = PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE);
static const unsigned WITH_MELODY =
    PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE) | PLAN_LAYER_BIT(PLAN_LANE_MELODY);

static void set(Expected *e, const char *letter, size_t bars, SectionBeginning beginning, SectionEnding ending,
                CadenceType cadence, size_t returns, unsigned layers) {
    e->letter = letter;
    e->bars = bars;
    e->beginning = beginning;
    e->ending = ending;
    e->cadence = cadence;
    e->returns = returns;
    e->layers = layers;
    e->liquidation = false;
    e->key_offset = 0;
    e->parallel = false;
    e->has_figure = false;
    memset(&e->figure, 0, sizeof(e->figure));
    e->region = false;
    e->function = -1;
    e->lyric = false;
    e->tonic_major = false;
    e->second = false;
    e->region_key = -1;
}

/* A retransition or a coda is an operator section like the liquidation: no pairing and no archetype (R-503). */
static bool is_operator(const Expected *e) {
    return e->liquidation || e->function == SECTION_FUNCTION_RETRANSITION || e->function == SECTION_FUNCTION_CODA;
}

#define IS(name) (strcmp(kind, name) == 0)

/* The table, written out here so the test does not read it from the code it checks. */
static size_t expected_sections(const char *kind, Size size, Expected *out) {
    static char letters[MAX_EXPECTED][PLAN_SECTION_LETTER_MAX];
    size_t n = 0;
    if (IS("binary")) {
        size_t half = size.bars / 2;
        set(&out[n++], "A", half, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", half, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
    } else if (IS("rounded-binary")) {
        size_t second = size.bars == 16 ? 4 : 8;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", second, CBI, CONT, CADENCE_TYPE_HALF, NONE, 0);
        set(&out[n++], "A'", second, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
    } else if (IS("ternary")) {
        size_t third = size.bars / 3;
        set(&out[n++], "A", third, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", third, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        set(&out[n++], "A", third, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
    } else if (IS("variations")) {
        set(&out[n++], "T", 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        for (size_t k = 1; k <= size.count; k++) {
            letters[n][0] = 'V';
            letters[n][1] = (char)('0' + k);
            letters[n][2] = '\0';
            set(&out[n], letters[n], 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
            n++;
        }
    } else if (IS("loop")) {
        if (size.bars == 8) {
            set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        } else {
            set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_IMPERFECT_AUTHENTIC, NONE, 0);
            set(&out[n++], "B", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        }
    } else if (IS("minuet") || IS("trio")) {
        /* The ternary's A, B, A, with B over a pedal leading into A return: the minuet's A a sentence, the trio's a period. */
        size_t third = size.bars / 3;
        SectionBeginning beginning = IS("trio") ? AN : P;
        SectionEnding ending = IS("trio") ? CONS : CONT;
        set(&out[n++], "A", third, beginning, ending, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", third, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        set(&out[n++], "A", third, beginning, ending, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
    } else if (IS("scherzo")) {
        /* A, then P and its sequences S1 (IV) and S2 (home), the liquidation, A return, the episode in IV and two codettas.
           The planned keys of the pattern, S1 and the episode are V, IV and IV: 7, 5 and 5 semitones above home. A section
           that ends away from home closes with a PAC whatever its document says (piece_plan_cadences), so the plan has a
           PAC for P, S1 and E, and the document's IAC for S2 (home, not closed). */
        size_t theme = size.bars == 60 ? 16 : 8;
        set(&out[n++], "A", theme, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n], "B", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].key_offset = 7;
        set(&out[n], "B", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, 1, 0);
        out[n++].key_offset = 5;
        set(&out[n++], "B", 4, AN, CONS, CADENCE_TYPE_IMPERFECT_AUTHENTIC, 1, 0);
        set(&out[n], "B", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].liquidation = true;
        set(&out[n++], "A", theme, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
        set(&out[n], "E", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].key_offset = 5;
        set(&out[n++], "C", 4, AN, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "C", 4, CBI, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
    } else if (IS("figured-variations")) {
        /* The theme, then variations that return it with a figure each (R-500): V1 and V2 neighbor tones in eighths and
           sixteenths, V3 a scale line in eighths in the parallel mode, V4 a scale line in sixteenths over a reharmonized
           copy with the root changed at bar 3, V5 a broken chord in sixteenths at the climax (the three of the size of 40
           are V1, V3 as the parallel one and V5); then the two codettas. */
        static const SectionFigure NEIGHBOR_8 = {SECTION_FIGURE_NEIGHBOR, SECTION_FIGURE_EIGHTHS, false, 0, false};
        static const SectionFigure NEIGHBOR_16 = {SECTION_FIGURE_NEIGHBOR, SECTION_FIGURE_SIXTEENTHS, false, 0, false};
        static const SectionFigure SCALE_8_PARALLEL = {SECTION_FIGURE_SCALE, SECTION_FIGURE_EIGHTHS, true, 0, false};
        static const SectionFigure SCALE_16_CHANGED = {SECTION_FIGURE_SCALE, SECTION_FIGURE_SIXTEENTHS, true, 3, false};
        static const SectionFigure BROKEN_16_CLIMAX = {SECTION_FIGURE_BROKEN_CHORD, SECTION_FIGURE_SIXTEENTHS, false, 0, true};
        const SectionFigure *const AT_56[] = {&NEIGHBOR_8, &NEIGHBOR_16, &SCALE_8_PARALLEL, &SCALE_16_CHANGED, &BROKEN_16_CLIMAX};
        const SectionFigure *const AT_40[] = {&NEIGHBOR_8, &SCALE_8_PARALLEL, &BROKEN_16_CLIMAX};
        const SectionFigure *const *figures = size.count == 5 ? AT_56 : AT_40;
        set(&out[n++], "T", 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        for (size_t k = 1; k <= size.count; k++) {
            letters[n][0] = 'V';
            letters[n][1] = (char)('0' + k);
            letters[n][2] = '\0';
            set(&out[n], letters[n], 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
            out[n].has_figure = true;
            out[n].figure = *figures[k - 1];
            out[n].parallel = figures[k - 1] == &SCALE_8_PARALLEL;
            n++;
        }
        set(&out[n++], "C", 4, AN, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "C", 4, CBI, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
    } else if (IS("main-and-subordinate")) {
        /* A main theme, the transition to the subordinate region (V, or III in minor; the KEY constraint is the key a section
           ends in), the subordinate theme (S1, and at 57 bars S2 the lyric one), the codetta, the retransition home, the
           return of A and the coda (R-509). The retransition and the coda are operator sections. */
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n], "T", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_TRANSITION;
        set(&out[n], "B", size.bars == 57 ? 9 : 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        if (size.bars == 57) {
            set(&out[n], "B", 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
            out[n].region = true;
            out[n].function = SECTION_FUNCTION_SUBORDINATE;
            out[n++].lyric = true;
        }
        set(&out[n], "C", 4, AN, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].region = true;
        set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_RETRANSITION;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
        set(&out[n], "K", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].function = SECTION_FUNCTION_CODA;
    } else if (IS("andante-aba") || IS("andante-abab") || IS("small-rondo-ababa") || IS("small-rondo-abaca")) {
        /* The Andante and small rondo forms (P52 B5, R-520): A, the transition T to the subordinate region (V, or III in
           minor), B there, the retransition R home, A1 (A returned as `embellish`), then per form: the coda K; T1, B1
           (B returned in the tonic major), K; T1, B1, R1, A2 (A returned as a `setting`), K; or C in the second region,
           R1, A2, K. The retransitions and the coda are operator sections. */
        const bool abab = IS("andante-abab") || IS("small-rondo-ababa");
        const bool rondo = IS("small-rondo-ababa") || IS("small-rondo-abaca");
        size_t a = n;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n], "T", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_TRANSITION;
        size_t b = n;
        set(&out[n], "B", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_RETRANSITION;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, a, 0);
        if (abab) {
            set(&out[n], "T", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
            out[n++].function = SECTION_FUNCTION_TRANSITION;
            set(&out[n], "B", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, b, 0);
            out[n].tonic_major = true;
            out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        } else if (IS("small-rondo-abaca")) {
            set(&out[n], "C", 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
            out[n++].second = true;
        }
        if (rondo) {
            set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
            out[n++].function = SECTION_FUNCTION_RETRANSITION;
            set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, a, 0);
        }
        set(&out[n], "K", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].function = SECTION_FUNCTION_CODA;
    } else if (IS("large-rondo") || IS("sonata-rondo")) {
        /* The large rondo and sonata rondo (P52 B6, R-520): A, T, B, R, A1 (`embellish`), then C in the trio (large) or P and S
           (sonata: P in the development, S its sequence in another key), R1, A2 (reharmonized), T1, B1 (B in the tonic major),
           R2, A3 (A returned as a `setting`) and K. P and S end away from home, so they plan a PAC. */
        const bool sonata = IS("sonata-rondo");
        size_t a = n;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n], "T", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_TRANSITION;
        size_t b = n;
        set(&out[n], "B", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_RETRANSITION;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, a, 0);
        if (sonata) {
            size_t pattern = n;
            set(&out[n], "C", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
            out[n++].region_key = DEVELOPMENT;
            set(&out[n], "C", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, pattern, 0);
            out[n++].region_key = SEQUENCE;
        } else {
            set(&out[n], "C", 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
            out[n++].second = true;
        }
        set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_RETRANSITION;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, a, 0);
        set(&out[n], "T", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_TRANSITION;
        set(&out[n], "B", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, b, 0);
        out[n].tonic_major = true;
        out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_RETRANSITION;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, a, 0);
        set(&out[n], "K", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].function = SECTION_FUNCTION_CODA;
    } else if (IS("sonatina") || IS("sonatina-tonic-major")) {
        /* The sonatina forms (P53 B2, R-523): the exposition A, T, B and the codettas C1 and C2 in the subordinate region; the
           elaboration E (B returned as `embellish` in IV or VI) with P and S as in the sonata rondo and the retransition R; the
           recapitulation A1 (A returned), T1, B1 and C3 (B and C1 returned: at home in `sonatina`, in the tonic major in
           `sonatina-tonic-major`), C4 and the coda K. B1, C3 and C4 close in the home key and are not closed, so a major piece
           plans them a half cadence; in the tonic major of a minor piece B1 and C3 keep their PAC. P and S plan a PAC. */
        const bool major_recap = IS("sonatina-tonic-major");
        size_t a = n;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n], "T", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_TRANSITION;
        size_t b = n;
        set(&out[n], "B", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n].region = true;
        out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        size_t c1 = n;
        set(&out[n], "C", 4, AN, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].region = true;
        set(&out[n], "C", 4, CBI, CAD, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].region = true;
        set(&out[n], "B", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, b, 0);
        out[n++].region_key = ELABORATION;
        size_t pattern = n;
        set(&out[n], "D", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].region_key = DEVELOPMENT;
        set(&out[n], "D", 4, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, pattern, 0);
        out[n++].region_key = SEQUENCE;
        set(&out[n], "R", 4, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_RETRANSITION;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, a, 0);
        set(&out[n], "T", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        out[n++].function = SECTION_FUNCTION_TRANSITION;
        set(&out[n], "B", 8, P, CONT, major_recap ? CADENCE_TYPE_PERFECT_AUTHENTIC : CADENCE_TYPE_HALF, b, 0);
        out[n].tonic_major = major_recap;
        out[n++].function = SECTION_FUNCTION_SUBORDINATE;
        set(&out[n], "C", 4, AN, CAD, major_recap ? CADENCE_TYPE_PERFECT_AUTHENTIC : CADENCE_TYPE_HALF, c1, 0);
        out[n++].tonic_major = major_recap;
        set(&out[n], "C", 4, CBI, CAD, CADENCE_TYPE_HALF, NONE, 0);
        n++;
        set(&out[n], "K", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        out[n++].function = SECTION_FUNCTION_CODA;
    } else if (IS("layered")) {
        static const unsigned *const BUILD_UP[3] = {&BASS, &ACCOMPANIED, &WITH_MELODY};
        for (size_t k = 1; k <= size.count; k++) {
            letters[n][0] = 'L';
            letters[n][1] = (char)('0' + k);
            letters[n][2] = '\0';
            unsigned layers = k == size.count ? PLAN_LAYERS_ALL : *BUILD_UP[k - 1];
            set(&out[n], letters[n], 8, P, CONT, CADENCE_TYPE_HALF, k == 1 ? NONE : 0, layers);
            n++;
        }
    }
    return n;
}

/* Every size of each template: bars for the fixed ones, the count for variations and layered. */
static size_t sizes_of(const char *kind, Size *out) {
    size_t n = 0;
    if (IS("binary")) {
        out[n++] = (Size){16, 0};
        out[n++] = (Size){8, 0};
    } else if (IS("rounded-binary")) {
        out[n++] = (Size){16, 0};
        out[n++] = (Size){24, 0};
    } else if (IS("ternary") || IS("minuet") || IS("trio")) {
        out[n++] = (Size){24, 0};
        out[n++] = (Size){12, 0};
    } else if (IS("variations")) {
        for (size_t count = 1; count <= 8; count++) {
            out[n++] = (Size){8 * (1 + count), count};
        }
    } else if (IS("loop")) {
        out[n++] = (Size){8, 0};
        out[n++] = (Size){16, 0};
    } else if (IS("layered")) {
        for (size_t count = 2; count <= 4; count++) {
            out[n++] = (Size){8 * count, count};
        }
    } else if (IS("scherzo")) {
        out[n++] = (Size){44, 0};
        out[n++] = (Size){60, 0};
    } else if (IS("figured-variations")) {
        out[n++] = (Size){56, 5};
        out[n++] = (Size){40, 3};
    } else if (IS("main-and-subordinate")) {
        out[n++] = (Size){57, 0};
        out[n++] = (Size){48, 0};
    } else if (IS("andante-aba")) {
        out[n++] = (Size){44, 0};
    } else if (IS("andante-abab")) {
        out[n++] = (Size){56, 0};
    } else if (IS("small-rondo-ababa")) {
        out[n++] = (Size){68, 0};
    } else if (IS("small-rondo-abaca")) {
        out[n++] = (Size){64, 0};
    } else if (IS("large-rondo") || IS("sonata-rondo")) {
        out[n++] = (Size){88, 0};
    } else if (IS("sonatina") || IS("sonatina-tonic-major")) {
        out[n++] = (Size){92, 0};
    }
    return n;
}

/*
 * The sizes the compose and heard sweeps run: every size, except that variations run 1, 2 and 5 variations (16, 24 and
 * 48 bars). Past that a run only repeats what the shortest ones show, and its stuck runs make the sweep take minutes.
 */
static size_t sweep_sizes_of(const char *kind, Size *out) {
    if (!IS("variations")) {
        return sizes_of(kind, out);
    }
    static const size_t COUNTS[] = {1, 2, 5};
    size_t n = 0;
    for (size_t i = 0; i < sizeof(COUNTS) / sizeof(COUNTS[0]); i++) {
        out[n++] = (Size){8 * (1 + COUNTS[i]), COUNTS[i]};
    }
    return n;
}

/* ------------------------------------------------------------- the plan */

static Motive scenario_motive(const Scenario *scenario) {
    Motive motive;
    MotiveError error;
    char path[1024];
    snprintf(path, sizeof(path), "%s/motive_d_minor.json", COMPOSER_FIXTURE_DIR);
    if (test_load_motive_file(path, MOTIVE_FORMAT_JSON, NULL, &motive, &error) != MOTIVE_OK) {
        fprintf(stderr, "cannot load %s: %s\n", path, error.message);
        exit(2);
    }
    motive_set_meter(&motive, (PlanMeter){scenario->beats, 4, 0});
    SpelledKey motive_key_value = test_key_mode(scenario->tonic, scenario->mode);
    motive_set_key(&motive, &motive_key_value, MOTIVE_SKETCH_KEY_GIVEN);
    if (scenario->beats == 3) {
        /* The half note at beat 3 ends on the barline as a quarter. */
        MotiveSketchNote *last = &motive.notes[motive.note_count - 1];
        assert(rational_make(1, 4, &last->duration) == RATIONAL_OK);
        assert(rational_make(3, 4, &motive.length) == RATIONAL_OK);
    }
    return motive;
}

static PieceRequest scenario_request(const Scenario *scenario, const char *kind, Size size,
                                     const Motive *motive) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = kind;
    request.key = test_key_mode(scenario->tonic, scenario->mode);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = 4;
    request.bars = size.bars;
    request.count = size.count;
    request.motive = scenario->given ? motive : NULL;
    return request;
}

static const PlanConstraint *find(const PiecePlan *plan, NodeId id, PlanConstraintKind kind, size_t nth) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind && nth-- == 0) {
            return constraint;
        }
    }
    return NULL;
}

static long bars_of(Rational time, int beats) {
    /* time is in whole notes; a bar is beats quarters. */
    long quarters_num = time.num * 4;
    assert(quarters_num % (long)time.den == 0);
    long quarters = quarters_num / (long)time.den;
    assert(quarters % beats == 0);
    return quarters / beats;
}

static void check_structure(const PiecePlan *plan, const Scenario *scenario, const char *kind, Size size) {
    Expected expected[MAX_EXPECTED];
    size_t count = expected_sections(kind, size, expected);
    NodeId form = plan_root(plan);
    assert(plan_node_child_count(plan, form) == count + 1);
    NodeId sections[MAX_EXPECTED];
    long at = 0;
    for (size_t i = 0; i < count; i++) {
        NodeId section = plan_node_child_at(plan, form, i);
        sections[i] = section;
        PlanSpan span = plan_node_span(plan, section);
        assert(plan_node_kind(plan, section) == PLAN_NODE_SECTION);
        assert(plan_node_state(plan, section) == PLAN_STATE_CONSTRAINED);
        assert(bars_of(span.start, scenario->beats) == at);
        assert(bars_of(span.length, scenario->beats) == (long)expected[i].bars);
        at += (long)expected[i].bars;

        const SectionDescription *described = plan_node_section(plan, section);
        const PlanConstraint *key = find(plan, section, PLAN_CONSTRAINT_KEY, 0);
        const bool liquidation = described->has_role && described->role == SECTION_ROLE_LIQUIDATION;
        assert(described->letter[0] != '\0' && strcmp(described->letter, expected[i].letter) == 0);
        /* The part of a larger form a section is, and only that (R-503). */
        assert(described->has_function == (expected[i].function >= 0));
        if (described->has_function) {
            assert((int)described->function.function == expected[i].function &&
                   described->function.lyric == expected[i].lyric && !described->function.independent);
        }
        if (is_operator(&expected[i])) {
            assert(liquidation == expected[i].liquidation);
            assert(!described->has_pairing);
        } else {
            assert(!liquidation && described->has_pairing && described->pairing.beginning == expected[i].beginning &&
                   described->pairing.ending == expected[i].ending);
        }
        /* B1 is not closed: in a major piece the tonic major is the home key, and a closing form plans a PAC there that is not
           closed as a half cadence; in a minor piece it is the parallel major and keeps its PAC (R-517, piece_cadences.c). */
        const CadenceType want = expected[i].tonic_major && scenario->mode == HARMONY_MODE_MAJOR ? CADENCE_TYPE_HALF
                                                                                                       : expected[i].cadence;
        assert(described->has_cadence && described->cadence == want);
        /* An antecedent's weak cadence comes second. */
        assert((expected[i].beginning == AN) == described->has_first_cadence);
        assert(!described->has_first_cadence || described->first_cadence == CADENCE_TYPE_HALF);
        /* A section in the parallel mode is on the home tonic in the other mode. */
        /* The region of the main and subordinate themes is V in major and III in minor, which is a major key. */
        const HarmonyMode mode = expected[i].parallel || (expected[i].region && scenario->mode == HARMONY_MODE_MINOR) ? (scenario->mode == HARMONY_MODE_MAJOR ? HARMONY_MODE_MINOR
                                                                                                          : HARMONY_MODE_MAJOR)
                                                           : scenario->mode;
        const int region_offset = scenario->mode == HARMONY_MODE_MAJOR ? 7 : 3; /* V in major, III in minor */
        const int offset = expected[i].region ? region_offset : expected[i].key_offset;
        if (expected[i].tonic_major) {
            /* The home tonic's major key: the home key in a major piece, the parallel major in a minor one (R-517). */
            assert(key && spelled_key_tonic_pitch_class(&key->params.key) == scenario->tonic &&
                   harmony_key_mode(&key->params.key) == HARMONY_MODE_MAJOR);
        } else if (expected[i].second) {
            /* One of the second region's keys: vi or IV in major, VI or iv in minor (R-520). */
            const int pc = spelled_key_tonic_pitch_class(&key->params.key);
            const bool major = scenario->mode == HARMONY_MODE_MAJOR;
            assert(key && ((pc == (scenario->tonic + (major ? 9 : 8)) % 12 && harmony_key_mode(&key->params.key) == (major ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR)) ||
                           (pc == (scenario->tonic + 5) % 12 && harmony_key_mode(&key->params.key) == scenario->mode)));
        } else if (expected[i].region_key >= 0) {
            /* The development is vi in major and iv in minor; the sequence is ii in major and v in minor; all minor keys (R-520). The
               elaboration is IV in major and VI in minor, both major keys (R-523). */
            const bool major = scenario->mode == HARMONY_MODE_MAJOR;
            const bool elaboration = expected[i].region_key == ELABORATION;
            const int up = elaboration ? (major ? 5 : 8) : expected[i].region_key == DEVELOPMENT ? (major ? 9 : 5) : (major ? 2 : 7);
            assert(key && spelled_key_tonic_pitch_class(&key->params.key) == (scenario->tonic + up) % 12 &&
                   harmony_key_mode(&key->params.key) == (elaboration ? HARMONY_MODE_MAJOR : HARMONY_MODE_MINOR));
        } else {
            assert(key && spelled_key_tonic_pitch_class(&key->params.key) == (scenario->tonic + offset) % 12 &&
                   harmony_key_mode(&key->params.key) == mode);
        }
        /* A figure rides on a figurate return and only there (R-497). */
        const SectionFigure *figure = described->has_figure ? &described->figure : NULL;
        assert((figure != NULL) == expected[i].has_figure);
        if (figure) {
            assert(described->returns[0] != '\0' && described->variation == SECTION_RETURN_FIGURATE);
            assert(figure->kind == expected[i].figure.kind && figure->rhythm == expected[i].figure.rhythm &&
                   figure->reharmonize == expected[i].figure.reharmonize && figure->change_at == expected[i].figure.change_at &&
                   figure->climax == expected[i].figure.climax);
        }
        if (expected[i].returns == NONE) {
            assert(described->returns[0] == '\0');
        } else {
            assert(plan_section_named(plan, described->returns) == sections[expected[i].returns]);
        }
        assert(described->layers == expected[i].layers);
        /* A layered form carries an accompaniment in every layer. */
        assert(IS("layered") == (find(plan, section, PLAN_CONSTRAINT_ACCOMPANIMENT, 0) != NULL));
        /* A section made from m0 other than as m0 itself names m0; a returning one takes its source's motive. */
        const PlanConstraint *motive = find(plan, section, PLAN_CONSTRAINT_MOTIVE_REF, 0);
        if (expected[i].returns != NONE && !IS("variations")) {
            const PlanConstraint *source = find(plan, sections[expected[i].returns], PLAN_CONSTRAINT_MOTIVE_REF, 0);
            assert((motive == NULL) == (source == NULL));
        }
        if (motive) {
            const char *name = plan_node_name(plan, motive->params.motive.motive);
            assert(name && strcmp(name, "m0") == 0);
        }
        if (expected[i].lyric) {
            /* A lyric theme is m0 augmented (R-506): the open ratio the builder fills. */
            assert(motive && motive->params.motive.has_step && motive->params.motive.step.kind == MOTIVE_RELATION_AUGMENT);
        } else if (strcmp(expected[i].letter, "B") == 0 && !IS("scherzo")) {
            assert(motive && motive->params.motive.has_step && motive->params.motive.step.kind == MOTIVE_RELATION_INVERT &&
                   motive->params.motive.step.value[1] != 0);
        }
        if (expected[i].letter[0] == 'V' && !IS("figured-variations")) {
            int k = atoi(expected[i].letter + 1);
            assert(motive && motive->params.motive.has_step &&
                   motive->params.motive.step.kind ==
                       (k % 2 == 1 ? MOTIVE_RELATION_DIMINISH : MOTIVE_RELATION_INTERVAL_CHANGE));
        }
    }
    /* The material, after the sections: m0 Given, or open with a motive to invent. */
    NodeId material = plan_node_child_at(plan, form, count);
    assert(plan_section_has_role(plan, material, SECTION_ROLE_MATERIAL));
    assert(bars_of(plan_node_span(plan, material).start, scenario->beats) == at);
    NodeId m0 = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, "m0") == 0) {
            m0 = id;
        }
    }
    assert(m0 != PLAN_NODE_NONE);
    LaneSpanId melody;
    assert(plan_node_lane_spans(plan, m0, &melody, 1) == 1);
    if (scenario->given) {
        assert(plan_lane_span_state(plan, melody) == PLAN_STATE_GIVEN);
    } else {
        assert(plan_lane_span_state(plan, melody) == PLAN_STATE_CONSTRAINED);
        assert(plan_lane_span_constraint_at(plan, melody, 0)->kind == PLAN_CONSTRAINT_INVENT_MOTIVE);
    }
}

/* Sizes not in the table, and counts for the templates that take none, are refused. */
static void check_refused(const char *kind, const Scenario *scenario, const Motive *motive) {
    static const Size FIXED_BAD[] = {{4, 0}, {12, 0}, {20, 0}, {32, 0}, {16, 2}};
    static const Size VARIATIONS_BAD[] = {{8, 0}, {12, 0}, {80, 0}, {0, 9}, {24, 3}, {20, 0}};
    static const Size LAYERED_BAD[] = {{8, 0}, {40, 0}, {0, 1}, {0, 5}, {24, 2}, {12, 0}};
    static const Size FIGURED_BAD[] = {{8, 0}, {32, 0}, {48, 0}, {0, 1}, {0, 4}, {40, 5}, {56, 3}};
    const Size *bad = IS("variations") ? VARIATIONS_BAD : IS("layered") ? LAYERED_BAD : IS("figured-variations") ? FIGURED_BAD : FIXED_BAD;
    size_t count = IS("variations")          ? sizeof(VARIATIONS_BAD) / sizeof(Size)
                   : IS("layered")           ? sizeof(LAYERED_BAD) / sizeof(Size)
                   : IS("figured-variations") ? sizeof(FIGURED_BAD) / sizeof(Size)
                                              : sizeof(FIXED_BAD) / sizeof(Size);
    for (size_t i = 0; i < count; i++) {
        Size size = bad[i];
        bool in_table = false;
        Size good[16];
        for (size_t g = 0; g < sizes_of(kind, good); g++) {
            in_table = in_table || (good[g].bars == size.bars && (size.count == 0 || good[g].count == size.count));
        }
        if (in_table) {
            continue;
        }
        PieceRequest request = scenario_request(scenario, kind, size, motive);
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_ERR_BAD_SIZE && plan == NULL);
    }
}

static void test_structure(const char *kind) {
    Size sizes[16];
    size_t size_count = sizes_of(kind, sizes);
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        Motive motive = scenario_motive(&SCENARIOS[s]);
        for (size_t z = 0; z < size_count; z++) {
            PieceRequest request = scenario_request(&SCENARIOS[s], kind, sizes[z], &motive);
            PiecePlan *plan = NULL;
            PieceResult built = piece_build_plan(&request, &plan);
            if (built != PIECE_OK) {
                fprintf(stderr, "%s %zu bars, %s: %s\n", kind, sizes[z].bars, SCENARIOS[s].name,
                        piece_result_name(built));
            }
            assert(built == PIECE_OK);
            check_structure(plan, &SCENARIOS[s], kind, sizes[z]);
            plan_destroy(plan);
            /* The default size and count are a size in the table. */
            request.bars = 0;
            request.count = 0;
            assert(piece_build_plan(&request, &plan) == PIECE_OK);
            plan_destroy(plan);
        }
        check_refused(kind, &SCENARIOS[s], &motive);
    }
    printf("%s: structure of %zu sizes in %zu scenarios as the table says\n", kind, size_count,
           SCENARIO_COUNT);
}

/* ------------------------------------------------------------ composing */

/* What the grow tests measured, heard as requested out of 40 (docs/grow.md), for the themes the templates use. */
static unsigned grow_measured(SectionBeginning beginning, SectionEnding ending, size_t bars) {
    if (bars != 8) {
        return 0; /* every theme of 4 bars: no theme heard */
    }
    if (beginning == P && ending == CONT) {
        return 26;
    }
    if (beginning == CBI && ending == CONT) {
        return 39;
    }
    if (beginning == AN && ending == CONT) {
        return 30;
    }
    return 0; /* a period: 0 of 40 */
}

typedef struct {
    size_t runs;
    size_t completed;
    size_t sections;
    size_t heard;            /* form_hear ran on the section */
    size_t as_requested;
    unsigned long expected;  /* sum over the sections of grow's measured share, in fortieths */
} Tally;

static void compose_all(const char *kind, Tally *total) {
    Size sizes[16];
    size_t size_count = sweep_sizes_of(kind, sizes);
    memset(total, 0, sizeof(*total));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        Motive motive = scenario_motive(&SCENARIOS[s]);
        for (size_t z = 0; z < size_count; z++) {
            Expected expected[MAX_EXPECTED];
            size_t count = expected_sections(kind, sizes[z], expected);
            Tally tally;
            memset(&tally, 0, sizeof(tally));
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PieceRequest request = scenario_request(&SCENARIOS[s], kind, sizes[z], &motive);
                PiecePlan *plan = NULL;
                assert(piece_build_plan(&request, &plan) == PIECE_OK);
                ComposeConfig config;
                ComposeOutcome outcome;
                memset(&config, 0, sizeof(config));
                config.knowledge = test_knowledge();
                config.seed = seed;
                PieceResult ran = compose_run(plan, &config, &outcome);
                tally.runs++;
                tally.sections += count;
                for (size_t i = 0; i < count; i++) {
                    tally.expected += grow_measured(expected[i].beginning, expected[i].ending, expected[i].bars);
                }
                if (ran == PIECE_OK) {
                    tally.completed++;
                    assert(outcome.section_count == count);
                    for (size_t i = 0; i < count; i++) {
                        tally.heard += outcome.sections[i].heard_result == FORM_HEAR_OK;
                        tally.as_requested += outcome.sections[i].heard_as_requested;
                    }
                } else {
                    assert(ran == PIECE_ERR_STUCK);
                    printf("  %s %zu bars, %s, seed %lu: stuck at %s\n", kind, sizes[z].bars,
                           SCENARIOS[s].name, seed, compose_stage_name(outcome.stuck_stage));
                }
                compose_outcome_destroy(&outcome);
                plan_destroy(plan);
            }
            printf("%s %zu bars, %s: %zu of %zu runs complete, %zu of %zu sections heard as requested\n",
                   kind, sizes[z].bars, SCENARIOS[s].name, tally.completed, tally.runs,
                   tally.as_requested, tally.sections);
            total->runs += tally.runs;
            total->completed += tally.completed;
            total->sections += tally.sections;
            total->heard += tally.heard;
            total->as_requested += tally.as_requested;
            total->expected += tally.expected;
        }
    }
}

/*
 * What the sweeps measured when this was written (seeds 1 to 5, the four scenarios, the sizes of sweep_sizes_of; the
 * runs are deterministic, so the numbers repeat). The tests fail when a number falls below its baseline and pass when
 * it holds or rises; raise a baseline when a change lifts it. The causes of the stuck runs and of the shares being
 * below grow's are in docs/piece.md. The rounded binary one was 28, 84, 20 until the presentation drew its repetition by
 * weight and a sequence wrote its chords whole (P34 B1); it was 25, 75, 19 until P44 B6 gave minor half cadences chords
 * and heard key changes inside a section in their new key (R-418), and is 40, 120, 58. The ternary one was 30, 90, 35
 * until it became the small ternary (R-445: B over a pedal ending on V, A1 varied), and is 39, 117, 51. The minuet and the
 * trio (P48 B5, R-487) run in C major and A minor 3/4 and were measured at 35, 105, 30 and 36, 108, 45: the given D minor
 * motive in A minor 24 bars is the weak scenario (1 of 5 and 2 of 5 compose, stuck at the returns and the seam stages), the
 * invented one composes 5 of 5; the 12-bar forms have sections of 4 bars, so none is heard as requested (as for the ternary).
 * The scherzo (P49 B4, R-492) runs in C major and A minor 3/4 at 44 and 60 bars and was measured at 34, 272, 28: the
 * invented motive gets stuck at the returns stage 5 times and at structure once, the given one composes 20 of 20; the
 * 60-bar sizes have themes of 16 bars and 4-bar sections, so none is heard as requested, and the liquidation is not heard.
 * The figured variations (P50 B4, R-500) run in C major 4/4 and A minor 3/4 at 56 and 40 bars and were measured at 39, 272, 126:
 * the invented motive in C major 4/4 at 56 bars is stuck at the returns stage once (4 of 5), the rest compose 35 of 35. The
 * variations are heard as returns, not as the theme they copy, so the shares are well under the period's; the sweeps in the
 * parallel mode and the figure are in docs/figured-variations.md.
 * The main and subordinate themes (P51 B7, R-509) run in C major 4/4 and A minor 3/4 at 57 and 48 bars and were measured at
 * 40, 220, 58: all 40 runs compose. The retransition and the coda are composed and recorded, not heard as a form (R-503), so 80
 * of the 300 sections are not heard; the rows of key and meter are in docs/main-and-subordinate.md.
 * The Andante and small rondo forms (P52 B5, R-520) run in C major 4/4 and A minor 3/4 at one size each (44, 56, 68 and 64
 * bars) and were measured at 20, 80, 36 (ABA), 20, 120, 43 (ABAB), 20, 140, 46 (ABABA) and 20, 120, 46 (ABACA): every run
 * composes. The retransitions and the coda are composed and recorded, not heard as a form (R-503); the rows of key and meter
 * are in docs/rondo.md.
 * The large rondo and sonata rondo (P52 B6, R-520) run in the same two scenarios at 88 bars and were measured at 20, 180, 116 (large) and 15, 150, 75 (sonata): the given motive in C major 4/4 is stuck 5 times in the
 * sonata rondo (motives, melody and texture), the other 15 runs compose; the invented one composes 10 of 10. The retransitions and the coda are composed and
 * recorded, not heard as a form (R-503).
 * The sonatina forms (P53 B2, R-523) run in the same two scenarios at 92 bars and were measured at 13, 169, 46 (sonatina) and 15, 195, 51 (sonatina-tonic-major): the given
 * motive in C major 4/4 composes 0 of 5 in both (stuck), the invented one 3 of 5 (sonatina) and 5 of 5 (tonic major); the other two scenarios compose 5 of 5. The
 * retransition and the coda are composed and recorded, not heard as a form (R-503).
 */
typedef struct {
    const char *kind;
    size_t completed;    /* runs that complete */
    size_t heard;        /* sections form_hear ran on */
    size_t as_requested; /* sections heard as the form they ask for */
} Baseline;

/* Re-recorded at P61 B6 (the Composer hears on harmonic spans, R-618), sections heard as requested: minuet 30 to 29, trio 45 to 41,
   scherzo 28 to 26, figured-variations 126 to 123, large-rondo 116 to 110, sonatina-tonic-major 51 to 48. */
static const Baseline BASELINES[] = {
    {"binary", 30, 60, 20},     {"rounded-binary", 40, 120, 58},
    {"ternary", 39, 117, 51},    {"variations", 60, 220, 184},
    {"loop", 40, 60, 32},       {"layered", 60, 180, 74},
    {"minuet", 35, 105, 29},     {"trio", 36, 108, 41},
    {"scherzo", 34, 272, 26},
    {"figured-variations", 39, 272, 123},
    {"main-and-subordinate", 40, 220, 58},
    {"andante-aba", 20, 80, 36},
    {"andante-abab", 20, 120, 43},
    {"small-rondo-ababa", 20, 140, 46},
    {"small-rondo-abaca", 20, 120, 46},
    {"large-rondo", 20, 180, 110},
    {"sonata-rondo", 15, 150, 75},
    {"sonatina", 13, 169, 46},
    {"sonatina-tonic-major", 15, 195, 48},
};

static const Baseline *baseline_of(const char *kind) {
    for (size_t i = 0; i < sizeof(BASELINES) / sizeof(BASELINES[0]); i++) {
        if (strcmp(BASELINES[i].kind, kind) == 0) {
            return &BASELINES[i];
        }
    }
    assert(!"a baseline for every template");
    return NULL;
}

static int test_compose(const char *kind) {
    Tally total;
    compose_all(kind, &total);
    const Baseline *baseline = baseline_of(kind);
    printf("%s: %zu of %zu runs complete (baseline %zu); %zu sections heard (baseline %zu)\n", kind,
           total.completed, total.runs, baseline->completed, total.heard, baseline->heard);
    return total.completed >= baseline->completed && total.heard >= baseline->heard ? 0 : 1;
}

static int test_heard(const char *kind) {
    Tally total;
    compose_all(kind, &total);
    const Baseline *baseline = baseline_of(kind);
    /* As many as grow's shares give for the same themes: expected / 40, rounded down. For comparison only. */
    size_t grow_floor = (size_t)(total.expected / 40);
    printf("%s: %zu of %zu sections heard as requested (%.0f%%; baseline %zu); grow's measured shares give %zu\n",
           kind, total.as_requested, total.sections,
           total.sections ? 100.0 * (double)total.as_requested / (double)total.sections : 0.0, baseline->as_requested,
           grow_floor);
    return total.as_requested >= baseline->as_requested ? 0 : 1;
}

/*
 * A quick check that the compose run works for the template: the smallest size in C major 4/4 with an invented motive,
 * the first of seeds 1 to SEEDS that completes (some seeds get stuck, see docs/piece.md), every section heard.
 */
static int test_smoke(const char *kind) {
    Size sizes[16];
    size_t size_count = sizes_of(kind, sizes);
    Size smallest = sizes[0];
    for (size_t i = 1; i < size_count; i++) {
        if (sizes[i].bars < smallest.bars) {
            smallest = sizes[i];
        }
    }
    Motive motive = scenario_motive(&SCENARIOS[0]);
    Expected expected[MAX_EXPECTED];
    size_t count = expected_sections(kind, smallest, expected);
    /* A liquidation, a retransition and a coda are composed and recorded, not heard as a form (R-494, R-503). */
    size_t to_hear = count;
    for (size_t i = 0; i < count; i++) {
        to_hear -= is_operator(&expected[i]) ? 1 : 0;
    }
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PieceRequest request = scenario_request(&SCENARIOS[0], kind, smallest, &motive);
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        PieceResult ran = compose_run(plan, &config, &outcome);
        size_t heard = 0;
        if (ran == PIECE_OK) {
            assert(outcome.section_count == count);
            for (size_t i = 0; i < count; i++) {
                heard += outcome.sections[i].heard_result == FORM_HEAR_OK;
            }
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
        if (ran == PIECE_OK && heard == to_hear) {
            printf("%s: %zu bars composed with seed %lu, %zu sections heard\n", kind,
                   smallest.bars, seed, heard);
            return 0;
        }
    }
    printf("%s: no seed of 1 to %d composed %zu bars\n", kind, SEEDS, smallest.bars);
    return 1;
}

int main(int argc, char **argv) {
    const char *kind = NULL;
    for (size_t form = 0; argc == 3 && form < form_spec_builtin_count(); form++) {
        if (strcmp(argv[2], form_spec_builtin_name(form)) == 0) {
            kind = form_spec_builtin_name(form);
        }
    }
    if (!kind) {
        fprintf(stderr, "usage: test_form_templates structure|compose|heard|smoke <template>\n");
        return 2;
    }
    if (IS("minuet") || IS("trio") || IS("scherzo")) {
        SCENARIOS = SCENARIOS_THREE_FOUR;
        SCENARIO_COUNT = sizeof(SCENARIOS_THREE_FOUR) / sizeof(SCENARIOS_THREE_FOUR[0]);
    }
    if (strcmp(argv[1], "structure") == 0) {
        test_structure(kind);
        return 0;
    }
    if (strcmp(argv[1], "compose") == 0) {
        return test_compose(kind);
    }
    if (strcmp(argv[1], "heard") == 0) {
        return test_heard(kind);
    }
    if (strcmp(argv[1], "smoke") == 0) {
        return test_smoke(kind);
    }
    fprintf(stderr, "usage: test_form_templates structure|compose|heard|smoke <template>\n");
    return 2;
}
