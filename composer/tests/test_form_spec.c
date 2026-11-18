#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

/* The old section fields the tests below read, as the description gives them: a key as the index of the region it names (or FORM_SPEC_HOME),
 * the motive as the step it makes from m0, and a return as the index of the section it returns. */
static int key_of(const FormSpec *form, const SectionRegion *region) {
    return form_spec_region_of(form, region);
}

static bool has_motive(const SectionDescription *section) {
    MotiveRelationStep step;
    return form_spec_section_motive(section, &step);
}

static MotiveRelationStep motive_of(const SectionDescription *section) {
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    (void)form_spec_section_motive(section, &step);
    return step;
}

static size_t returns_of(const FormSpecSize *size, const SectionDescription *section) {
    return form_spec_section_index(size, section->returns);
}

/*
 * The form documents (R-416, docs/forms.md).
 *
 *   test_form_spec            the reader and the built-in forms (fast):
 *                               every built-in parses and is what docs/forms.md says of it;
 *                               the figured variations (B4 of P50, R-500): sections, figures, the parallel V3, the meters;
 *                               a section's construction (B6 of P47): "auto", the ideas and extensions, the refusals;
 *                               a section's figure (B1 of P50, tests/fixtures/forms/figure-form.json): what it reads as, each refusal;
 *                               a section in the parallel mode (B3 of P50, tests/fixtures/forms/parallel-form.json): what it reads as, each refusal;
 *                               the parts of larger forms (B1 of P51, tests/fixtures/forms/parts-of-forms.json): the four roles, independent and lyric, each refusal, the plan's section functions;
 *                               a returning section in the tonic major (B3 of P52, R-517): what it reads as, each refusal;
 *                               a returning section in the home mode (B1 of P53, R-524): what it reads as, each refusal;
 *                               each refusal of the reader, with the path of the member at fault;
 *                               form_spec_size for every size, count and bad size of each built-in
 *   test_form_spec pedal      a form with a dominant pedal in its B (tests/fixtures/forms/pedal-form.json) composes: seeds
 *                             1 to 10 in C major 4/4, at least 9 compose, and in each the chords and the bass of B's
 *                             first half are all on the dominant
 *   test_form_spec leads      a form whose B leads into A return (tests/fixtures/forms/lead-form.json, R-486) composes: seeds
 *                             1 to 10 in C major and A minor 3/4, at least 9 compose in each, B ends on V or V7 and A return
 *                             opens on the tonic, and in at least 6 the connective is written (B's downbeat note kept, four
 *                             eighths in the scale, the last a step from A return's first note); a "sometimes" connective
 *                             is asked for on some seeds and not others. The reader's lead-in refusals are in the fast run
 *   test_form_spec modulates  a form that always modulates (tests/fixtures/forms/modulating-form.json, R-493) composes with
 *                             the request not asking to modulate: seeds 1 to 10 in C major and A minor 3/4, at least 9
 *                             compose in each, and in each P is in the key of V (v in minor) and S is home. The reader's
 *                             modulates and role tests are in the fast run
 *   test_form_spec meters     a form document that lists 3/4 and a tempo (tests/fixtures/forms/meter-form.json) builds in
 *                             3/4 and is refused in 4/4 (R-487); the reader's meters and tempo refusals are in the fast run
 *   test_form_spec custom     a form added as a document alone (tests/fixtures/forms/bar-form.json) composes: seeds 1
 *                             to 10 in C major 4/4 with an invented motive; no C code names the form. The same for
 *                             tests/fixtures/forms/uneven-form.json (a 4+6 period and a 4+6 sentence with an evaded
 *                             cadence, 20 bars, each section with a construction): 10 of 10 seeds compose
 */

/* ------------------------------------------------------------- helpers */

static FormSpec *builtin(const char *name) {
    FormSpec *form = NULL;
    assert(form_spec_builtin(name, &form) == FORM_SPEC_OK && form);
    return form;
}

/* The document of the built-in with the first occurrence of `from` replaced by `to`; the caller frees it. */
static char *replaced(const char *name, const char *from, const char *to) {
    const char *document = NULL;
    for (size_t i = 0; i < form_spec_builtin_count(); i++) {
        if (strcmp(form_spec_builtin_name(i), name) == 0) {
            document = form_spec_builtin_document(i);
        }
    }
    assert(document);
    const char *at = strstr(document, from);
    if (!at) {
        fprintf(stderr, "'%s' is not in the %s document\n", from, name);
        exit(2);
    }
    size_t head = (size_t)(at - document);
    size_t length = strlen(document) - strlen(from) + strlen(to);
    char *out = malloc(length + 1);
    assert(out);
    memcpy(out, document, head);
    memcpy(out + head, to, strlen(to));
    strcpy(out + head + strlen(to), at + strlen(from));
    return out;
}

/* A copy of `text` with the first `from` replaced by `to`. */
static char *replaced_in(const char *text, const char *from, const char *to) {
    const char *at = strstr(text, from);
    assert(at);
    size_t head = (size_t)(at - text);
    char *out = malloc(strlen(text) + strlen(to) + 1);
    assert(out);
    memcpy(out, text, head);
    strcpy(out + head, to);
    strcat(out, at + strlen(from));
    return out;
}

/* The text is refused as an invalid document, naming `path`. */
static void refused(const char *text, const char *path) {
    FormSpec *form = NULL;
    FormSpecError error;
    FormSpecResult result = form_spec_parse(text, &form, &error);
    if (result != FORM_SPEC_ERR_INVALID || form != NULL || strcmp(error.path, path) != 0 || error.message[0] == '\0') {
        fprintf(stderr, "expected a refusal at '%s', got result %d at '%s': %s\n%s\n", path, (int)result, error.path,
                error.message, text);
        exit(1);
    }
}

static void refused_edit(const char *name, const char *from, const char *to, const char *path) {
    char *text = replaced(name, from, to);
    refused(text, path);
    free(text);
}

/* A document of one size from `sizes` (the text of the "sizes" members) and `top` (more top-level members, with a leading comma). */
static void document(char *out, size_t cap, const char *top, const char *sizes) {
    snprintf(out, cap, "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\"%s, \"sizes\": [%s]}", top,
             sizes);
}

#define SECTION_A                                                                                                          \
    "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"presentation\", "                \
    "\"ending\": \"continuation\", \"cadence\": \"perfect-authentic\"}"

/* ------------------------------------------------------------- the built-in forms */

typedef struct {
    size_t bars;
    size_t count;
    bool is_default;
} SizeRow;

typedef struct {
    const char *name;
    bool closing;
    bool accompaniment_required;
    size_t regions;
    SizeRow sizes[8];
    size_t size_count;
    /* Sizes that are not in the form. */
    SizeRow bad[8];
    size_t bad_count;
    /* What the form says of its meters, tempo, repeats and parts (R-487); all zero for the forms that say none. */
    size_t meters;
    unsigned tempo;
    size_t repeats;
    size_t parts;
} FormRow;

/* Written out here so the test does not read it from the documents it checks. */
static const FormRow FORMS[] = {
    {"binary", true, false, 1, {{8, 0, false}, {16, 0, true}}, 2,
     {{4, 0, false}, {12, 0, false}, {20, 0, false}, {32, 0, false}, {16, 2, false}, {0, 1, false}}, 6, 0, 0, 0, 0},
    {"rounded-binary", true, false, 1, {{16, 0, true}, {24, 0, false}}, 2,
     {{8, 0, false}, {12, 0, false}, {32, 0, false}, {24, 1, false}}, 4, 0, 0, 0, 0},
    {"ternary", true, false, 1, {{12, 0, false}, {24, 0, true}}, 2,
     {{8, 0, false}, {16, 0, false}, {36, 0, false}, {0, 3, false}}, 4, 0, 0, 0, 0},
    {"variations", true, false, 1,
     {{16, 1, false}, {24, 2, false}, {32, 3, true}, {40, 4, false}, {48, 5, false}, {56, 6, false}, {64, 7, false}, {72, 8, false}},
     8, {{8, 0, false}, {12, 0, false}, {80, 0, false}, {0, 9, false}, {24, 3, false}, {20, 0, false}}, 6, 0, 0, 0, 0},
    {"loop", false, false, 0, {{8, 0, true}, {16, 0, false}}, 2,
     {{4, 0, false}, {12, 0, false}, {24, 0, false}, {8, 1, false}}, 4, 0, 0, 0, 0},
    {"layered", false, true, 0, {{16, 2, false}, {24, 3, false}, {32, 4, true}}, 3,
     {{8, 0, false}, {40, 0, false}, {0, 1, false}, {0, 5, false}, {24, 2, false}, {12, 0, false}}, 6, 0, 0, 0, 0},
    {"minuet", true, false, 1, {{12, 0, false}, {24, 0, true}}, 2,
     {{8, 0, false}, {16, 0, false}, {36, 0, false}, {0, 3, false}, {12, 1, false}}, 5, 1, 112, 2, 0},
    {"trio", true, false, 1, {{12, 0, false}, {24, 0, true}}, 2,
     {{8, 0, false}, {16, 0, false}, {36, 0, false}, {0, 3, false}, {12, 1, false}}, 5, 1, 112, 2, 0},
    {"minuet-and-trio", true, false, 1, {{12, 0, false}, {24, 0, true}}, 2,
     {{8, 0, false}, {16, 0, false}, {36, 0, false}, {0, 3, false}, {12, 1, false}}, 5, 1, 112, 0, 3},
    {"scherzo", true, false, 3, {{44, 0, true}, {60, 0, false}}, 2,
     {{8, 0, false}, {24, 0, false}, {48, 0, false}, {0, 3, false}, {44, 1, false}}, 5, 3, 176, 2, 0},
    {"scherzo-and-trio", true, false, 1, {{44, 0, true}, {60, 0, false}}, 2,
     {{8, 0, false}, {24, 0, false}, {48, 0, false}, {0, 3, false}, {44, 1, false}}, 5, 1, 176, 0, 3},
    {"figured-variations", true, false, 0, {{40, 3, false}, {56, 5, true}}, 2,
     {{8, 0, false}, {32, 0, false}, {48, 0, false}, {0, 1, false}, {0, 4, false}, {40, 5, false}}, 6, 4, 0, 0, 0},
    {"main-and-subordinate", true, false, 1, {{48, 0, false}, {57, 0, true}}, 2,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {57, 1, false}}, 5, 0, 0, 0, 0},
    {"andante-aba", true, false, 1, {{44, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {44, 1, false}}, 5, 0, 72, 0, 0},
    {"andante-abab", true, false, 1, {{56, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {44, 0, false}, {0, 1, false}, {56, 1, false}}, 5, 0, 72, 0, 0},
    {"small-rondo-ababa", true, false, 1, {{68, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {68, 1, false}}, 5, 0, 88, 0, 0},
    {"small-rondo-abaca", true, false, 2, {{64, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {64, 1, false}}, 5, 0, 88, 0, 0},
    {"large-rondo", true, false, 2, {{88, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {88, 1, false}}, 5, 0, 132, 0, 0},
    {"sonata-rondo", true, false, 3, {{88, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {88, 1, false}}, 5, 0, 132, 0, 0},
    {"sonatina", true, false, 4, {{92, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {88, 1, false}}, 5, 0, 132, 0, 0},
    {"sonatina-tonic-major", true, false, 4, {{92, 0, true}}, 1,
     {{8, 0, false}, {32, 0, false}, {56, 0, false}, {0, 1, false}, {88, 1, false}}, 5, 0, 132, 0, 0},
};
#define FORM_COUNT (sizeof(FORMS) / sizeof(FORMS[0]))

static void test_builtin_forms(void) {
    /* The forms that come in sizes, then the game loop, whose sections the request gives (P44 B5). */
    assert(form_spec_builtin_count() == FORM_COUNT + 1 && strcmp(form_spec_builtin_name(FORM_COUNT), "game-loop") == 0);
    for (size_t f = 0; f < FORM_COUNT; f++) {
        const FormRow *row = &FORMS[f];
        /* Registry order, and a document that is its file's name. */
        assert(strcmp(form_spec_builtin_name(f), row->name) == 0);
        assert(form_spec_builtin_document(f) != NULL);
        FormSpec *form = builtin(row->name);
        assert(strcmp(form->name, row->name) == 0 && strcmp(form->plan_name, row->name) == 0 && form->label[0] != '\0');
        assert(form->closing == row->closing && form->accompaniment_required == row->accompaniment_required);
        assert(form->region_count == row->regions && !form->request_sections && form->checks == FORM_CHECK_ALL);
        assert(form->size_count == row->size_count);
        assert(form->meter_count == row->meters && form->tempo == row->tempo && form->repeat_count == row->repeats &&
               form->part_count == row->parts);
        /* The first meter is 3/4 where a form lists some, and 2/4 for the figured variations (R-500). */
        const bool figured = strcmp(row->name, "figured-variations") == 0;
        assert(row->meters == 0 || (form->meters[0].beats_per_bar == (figured ? 2 : 3) && form->meters[0].beat_unit == 4));
        for (size_t z = 0; z < row->size_count; z++) {
            const FormSpecSize *size = &form->sizes[z];
            size_t sum = 0;
            assert(size->bars == row->sizes[z].bars && size->count == row->sizes[z].count &&
                   size->is_default == row->sizes[z].is_default);
            for (size_t i = 0; i < size->section_count; i++) {
                const SectionDescription *section = &size->sections[i];
                assert(section_role_is_theme(section->role) && section->bars > 0 && section->beats == 0);
                assert(section->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || section->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
                       section->cadence == CADENCE_TYPE_HALF);
                assert(section->returns[0] == '\0' || returns_of(size, section) < i);
                /* Only the B of the ternary, the minuet and the trio asks for a pedal (R-446, R-445, R-487). */
                bool b_pedal = (strcmp(row->name, "ternary") == 0 || strcmp(row->name, "minuet") == 0 ||
                                strcmp(row->name, "trio") == 0) && i == 1;
                assert(section->pedal == (b_pedal ? 5 : 0));
                sum += section->bars;
            }
            /* A compound form's sizes have bars and no sections: its parts have them. */
            assert(row->parts > 0 ? size->section_count == 0 : sum == size->bars);
        }
        form_spec_destroy(form);
    }
    assert(form_spec_builtin_name(FORM_COUNT + 1) == NULL && form_spec_builtin_document(FORM_COUNT + 1) == NULL);
    FormSpec *none = NULL;
    assert(form_spec_builtin("sonata", &none) == FORM_SPEC_ERR_UNKNOWN && none == NULL);
    assert(form_spec_builtin(NULL, &none) == FORM_SPEC_ERR_NULL);
}

/* What docs/forms.md says of each form's keys, closed sections, motives and layers. */
static void test_builtin_details(void) {
    FormSpec *form = builtin("binary");
    assert(strcmp(form->regions[0].name, "away") == 0 && form->regions[0].major_count == 1 && strcmp(form->regions[0].major[0], "omt.relation.dominant") == 0 &&
           form->regions[0].minor_count == 1 && strcmp(form->regions[0].minor[0], "omt.relation.mediant") == 0);
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *size = &form->sizes[z];
        assert(key_of(form, &size->sections[0].key_start) == FORM_SPEC_HOME && key_of(form, &size->sections[0].key_end) == 0);
        assert(key_of(form, &size->sections[1].key_start) == 0 && key_of(form, &size->sections[1].key_end) == FORM_SPEC_HOME);
        assert(!size->sections[0].closed && !size->sections[1].closed);
        /* B is m0 inverted in the key's steps: tonal, axis the default. */
        assert(!has_motive(&size->sections[0]) && has_motive(&size->sections[1]));
        assert(motive_of(&size->sections[1]).kind == MOTIVE_RELATION_INVERT && motive_of(&size->sections[1]).value[0] == -1 &&
               motive_of(&size->sections[1]).value[1] == 1);
    }
    form_spec_destroy(form);

    form = builtin("rounded-binary");
    const FormSpecSize *rounded = form_spec_size(form, 16, 0);
    assert(rounded && rounded->section_count == 3 && rounded->sections[0].closed && !rounded->sections[1].closed &&
           !rounded->sections[2].closed);
    assert(rounded->sections[1].cadence == CADENCE_TYPE_HALF && strcmp(rounded->sections[1].cadence_name, "half") == 0);
    assert(rounded->sections[1].bars == 4 && rounded->sections[2].bars == 4 && form_spec_size(form, 24, 0)->sections[1].bars == 8);
    assert(returns_of(rounded, &rounded->sections[2]) == 0 && rounded->sections[2].variation == SECTION_RETURN_EXACT &&
           strcmp(rounded->sections[2].letter, "A'") == 0 &&
           rounded->sections[1].pairing.beginning == SECTION_BEGINNING_COMPOUND_BASIC_IDEA);
    form_spec_destroy(form);

    form = builtin("ternary");
    assert(form->regions[0].major_count == 2 && strcmp(form->regions[0].major[0], "omt.relation.dominant") == 0 && strcmp(form->regions[0].major[1], "omt.relation.mediant") == 0 &&
           form->regions[0].minor_count == 2 && strcmp(form->regions[0].minor[0], "omt.relation.mediant") == 0 && strcmp(form->regions[0].minor[1], "omt.relation.dominant") == 0);
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *ternary = &form->sizes[z];
        assert(ternary->sections[0].closed && !ternary->sections[1].closed && !ternary->sections[2].closed);
        /* A ends in the away region, B starts there and returns home, ending on V (a half cadence) over a pedal. */
        assert(key_of(form, &ternary->sections[0].key_start) == FORM_SPEC_HOME && key_of(form, &ternary->sections[0].key_end) == 0);
        assert(key_of(form, &ternary->sections[1].key_start) == 0 && key_of(form, &ternary->sections[1].key_end) == FORM_SPEC_HOME);
        assert(ternary->sections[1].cadence == CADENCE_TYPE_HALF && ternary->sections[1].pedal == 5);
        assert(ternary->sections[2].variation == SECTION_RETURN_VARIED && returns_of(ternary, &ternary->sections[2]) == 0);
        assert(strcmp(ternary->sections[2].name, "A return") == 0 && strcmp(ternary->sections[2].letter, "A") == 0 &&
               ternary->sections[0].pairing.beginning == SECTION_BEGINNING_ANTECEDENT &&
               ternary->sections[0].pairing.ending == SECTION_ENDING_CONSEQUENT);
    }
    form_spec_destroy(form);

    /* Variations: the theme is index 0; with three or more the section at (count + 1) / 2 is wholly away. */
    form = builtin("variations");
    assert(form->regions[0].major_count == 1 && strcmp(form->regions[0].major[0], "omt.relation.submediant") == 0 && strcmp(form->regions[0].minor[0], "omt.relation.mediant") == 0);
    for (size_t count = 1; count <= 8; count++) {
        const FormSpecSize *size = form_spec_size(form, 0, count);
        assert(size && size->section_count == count + 1 && strcmp(size->sections[0].name, "theme") == 0);
        for (size_t i = 0; i < size->section_count; i++) {
            const SectionDescription *section = &size->sections[i];
            bool away = count >= 3 && i == (count + 1) / 2;
            assert(section->closed && key_of(form, &section->key_start) == (away ? 0 : FORM_SPEC_HOME) &&
                   key_of(form, &section->key_end) == (away ? 0 : FORM_SPEC_HOME));
            if (i > 0) {
                bool odd = i % 2 == 1;
                assert(returns_of(size, section) == 0 && section->variation == (odd ? SECTION_RETURN_ORNAMENT : SECTION_RETURN_REHARMONIZE));
                assert(has_motive(section) &&
                       motive_of(section).kind == (odd ? MOTIVE_RELATION_DIMINISH : MOTIVE_RELATION_INTERVAL_CHANGE));
            }
        }
    }
    form_spec_destroy(form);

    /* Layered: the masks build up from the bass, the last everything. */
    form = builtin("layered");
    const unsigned bass = PLAN_LAYER_BIT(PLAN_LANE_BASS);
    const unsigned inner = bass | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE);
    const unsigned melody = inner | PLAN_LAYER_BIT(PLAN_LANE_MELODY);
    const FormSpecSize *layered = form_spec_size(form, 0, 4);
    assert(layered->sections[0].layers == bass && layered->sections[1].layers == inner &&
           layered->sections[2].layers == melody && layered->sections[3].layers == PLAN_LAYERS_ALL);
    assert(layered->sections[0].returns[0] == '\0' && returns_of(layered, &layered->sections[3]) == 0);
    assert(form_spec_size(form, 0, 2)->sections[0].layers == bass && form_spec_size(form, 0, 2)->sections[1].layers == PLAN_LAYERS_ALL);
    form_spec_destroy(form);

    /* Loop: the first section ends in the half cadence that leads back; the 16-bar one opens with an IAC. */
    form = builtin("loop");
    assert(form_spec_size(form, 8, 0)->sections[0].cadence == CADENCE_TYPE_HALF);
    assert(form_spec_size(form, 16, 0)->sections[0].cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
           form_spec_size(form, 16, 0)->sections[1].cadence == CADENCE_TYPE_HALF);
    form_spec_destroy(form);

    /* Minuet and trio (R-487): ternary's keys and returns, B over a dominant pedal leading into A return, the groups
       A and B A return written out; the minuet's A and A return a sentence, the trio's a period. */
    for (int trio = 0; trio < 2; trio++) {
        form = builtin(trio ? "trio" : "minuet");
        assert(form->closing && form->region_count == 1 && strcmp(form->regions[0].name, "away") == 0 &&
               form->regions[0].major_count == 2 && strcmp(form->regions[0].major[0], "omt.relation.dominant") == 0 && strcmp(form->regions[0].major[1], "omt.relation.mediant") == 0 &&
               form->regions[0].minor_count == 2 && strcmp(form->regions[0].minor[0], "omt.relation.mediant") == 0 && strcmp(form->regions[0].minor[1], "omt.relation.dominant") == 0);
        assert(form->repeat_count == 2 && form->repeats[0].first == 0 && form->repeats[0].count == 1 &&
               form->repeats[1].first == 1 && form->repeats[1].count == 2);
        for (size_t z = 0; z < form->size_count; z++) {
            const FormSpecSize *size = &form->sizes[z];
            assert(size->section_count == 3 && size->bars == (z == 0 ? 12u : 24u));
            for (size_t i = 0; i < 3; i++) {
                assert(size->sections[i].bars == size->bars / 3);
            }
            assert(size->sections[0].closed && !size->sections[1].closed && !size->sections[2].closed);
            assert(key_of(form, &size->sections[0].key_start) == FORM_SPEC_HOME && key_of(form, &size->sections[0].key_end) == 0 &&
                   key_of(form, &size->sections[1].key_start) == 0 && key_of(form, &size->sections[1].key_end) == FORM_SPEC_HOME);
            assert(size->sections[0].cadence == CADENCE_TYPE_PERFECT_AUTHENTIC &&
                   size->sections[1].cadence == CADENCE_TYPE_HALF && size->sections[2].cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
            assert(has_motive(&size->sections[1]) && motive_of(&size->sections[1]).kind == MOTIVE_RELATION_INVERT &&
                   size->sections[1].pedal == 5 && size->sections[1].leads_in &&
                   size->sections[1].connective == SECTION_CONNECTIVE_SOMETIMES);
            assert(returns_of(size, &size->sections[2]) == 0 && size->sections[2].variation == SECTION_RETURN_VARIED &&
                   strcmp(size->sections[2].name, "A return") == 0 && !size->sections[2].leads_in);
            SectionBeginning beginning = trio ? SECTION_BEGINNING_ANTECEDENT : SECTION_BEGINNING_PRESENTATION;
            SectionEnding ending = trio ? SECTION_ENDING_CONSEQUENT : SECTION_ENDING_CONTINUATION;
            assert(size->sections[0].pairing.beginning == beginning && size->sections[0].pairing.ending == ending &&
                   size->sections[2].pairing.beginning == beginning && size->sections[2].pairing.ending == ending &&
                   size->sections[1].pairing.beginning == SECTION_BEGINNING_PRESENTATION &&
                   size->sections[1].pairing.ending == SECTION_ENDING_CONTINUATION);
        }
        form_spec_destroy(form);
    }

    /* The minuet and trio: the minuet, the trio in a region of its own with a motive of its own, the minuet da capo
       without repeats; its sizes carry bars alone. */
    form = builtin("minuet-and-trio");
    assert(form->closing && form->region_count == 1 && strcmp(form->regions[0].name, "trio") == 0 &&
           form->regions[0].major_count == 3 && strcmp(form->regions[0].major[0], "omt.relation.parallel") == 0 && strcmp(form->regions[0].major[1], "omt.relation.subdominant") == 0 &&
           strcmp(form->regions[0].major[2], "omt.relation.submediant") == 0 && form->regions[0].minor_count == 3 && strcmp(form->regions[0].minor[0], "omt.relation.parallel") == 0 &&
           strcmp(form->regions[0].minor[1], "omt.relation.mediant") == 0 && strcmp(form->regions[0].minor[2], "omt.relation.submediant") == 0);
    assert(form->part_count == 3 && form->repeat_count == 0 && !form->request_sections);
    assert(strcmp(form->parts[0].name, "Minuet") == 0 && strcmp(form->parts[0].form, "minuet") == 0 &&
           form->parts[0].key == FORM_SPEC_HOME && !form->parts[0].new_motive && form->parts[0].again == FORM_SPEC_NO_RETURN &&
           form->parts[0].repeats);
    assert(strcmp(form->parts[1].name, "Trio") == 0 && strcmp(form->parts[1].form, "trio") == 0 && form->parts[1].key == 0 &&
           form->parts[1].new_motive && form->parts[1].again == FORM_SPEC_NO_RETURN && form->parts[1].repeats);
    assert(strcmp(form->parts[2].name, "Minuet da capo") == 0 && form->parts[2].form[0] == '\0' && form->parts[2].again == 0 &&
           !form->parts[2].repeats);
    for (size_t z = 0; z < form->size_count; z++) {
        assert(form->sizes[z].section_count == 0);
    }
    assert(form_spec_performed(form));
    assert(form->parts[0].bars == 0 && form->parts[1].bars == 0 && form->parts[2].bars == 0);
    form_spec_destroy(form);

    /* The scherzo and trio: the scherzo, a trio of 24 bars in its own key with a motive of its own, the scherzo da capo
       without repeats, at 44 or 60 bars (R-495); it does not modulate always, each part decides. */
    form = builtin("scherzo-and-trio");
    assert(form->closing && !form->modulates && form->region_count == 1 && strcmp(form->regions[0].name, "trio") == 0 &&
           form->regions[0].major_count == 3 && form->regions[0].minor_count == 3);
    assert(form->part_count == 3 && form->repeat_count == 0 && form->meter_count == 1 && form->tempo == 176 && form_spec_performed(form));
    assert(strcmp(form->parts[0].name, "Scherzo") == 0 && strcmp(form->parts[0].form, "scherzo") == 0 &&
           form->parts[0].key == FORM_SPEC_HOME && !form->parts[0].new_motive && form->parts[0].bars == 0 &&
           form->parts[0].again == FORM_SPEC_NO_RETURN && form->parts[0].repeats);
    assert(strcmp(form->parts[1].name, "Trio") == 0 && strcmp(form->parts[1].form, "trio") == 0 && form->parts[1].key == 0 &&
           form->parts[1].new_motive && form->parts[1].bars == 24 && form->parts[1].again == FORM_SPEC_NO_RETURN &&
           form->parts[1].repeats);
    assert(strcmp(form->parts[2].name, "Scherzo da capo") == 0 && form->parts[2].form[0] == '\0' && form->parts[2].bars == 0 &&
           form->parts[2].again == 0 && !form->parts[2].repeats);
    for (size_t z = 0; z < form->size_count; z++) {
        assert(form->sizes[z].section_count == 0);
    }
    form_spec_destroy(form);

    /* The scherzo (R-492, R-493): a ternary with a modulating middle, nine sections at both sizes, three regions for the
       key of each pattern, its sequences and the episode, repeats A then P to A return. */
    form = builtin("scherzo");
    assert(form->closing && form->modulates && form->region_count == 3 && form->part_count == 0 && form->repeat_count == 2);
    assert(strcmp(form->regions[0].name, "pattern") == 0 && form->regions[0].major_count == 1 && strcmp(form->regions[0].major[0], "omt.relation.dominant") == 0 &&
           form->regions[0].minor_count == 1 && strcmp(form->regions[0].minor[0], "omt.relation.dominant") == 0);
    assert(strcmp(form->regions[1].name, "sequence") == 0 && form->regions[1].major_count == 1 && strcmp(form->regions[1].major[0], "omt.relation.subdominant") == 0 &&
           form->regions[1].minor_count == 1 && strcmp(form->regions[1].minor[0], "omt.relation.subdominant") == 0);
    assert(strcmp(form->regions[2].name, "episode") == 0 && form->regions[2].major_count == 1 && strcmp(form->regions[2].major[0], "omt.relation.subdominant") == 0 &&
           form->regions[2].minor_count == 1 && strcmp(form->regions[2].minor[0], "omt.relation.subdominant") == 0);
    assert(form->meters[0].beats_per_bar == 3 && form->meters[1].beats_per_bar == 2 && form->meters[2].beats_per_bar == 6 &&
           form->meters[2].beat_unit == 8);
    static const struct {
        const char *name;
        const char *letter;
        SectionRole role;
        size_t bars;
        SectionBeginning beginning;
        SectionEnding ending;
        CadenceType cadence;
        int key;
        size_t returns;
        bool closed;
    } SCHERZO[] = {
        {"A", "A", SECTION_ROLE_THEME, 8, SECTION_BEGINNING_PRESENTATION, SECTION_ENDING_CONTINUATION, CADENCE_TYPE_PERFECT_AUTHENTIC, FORM_SPEC_HOME, FORM_SPEC_NO_RETURN, true},
        {"P", "B", SECTION_ROLE_PATTERN, 4, SECTION_BEGINNING_ANTECEDENT, SECTION_ENDING_CONSEQUENT, CADENCE_TYPE_IMPERFECT_AUTHENTIC, 0, FORM_SPEC_NO_RETURN, false},
        {"S1", "B", SECTION_ROLE_SEQUENCE, 4, SECTION_BEGINNING_ANTECEDENT, SECTION_ENDING_CONSEQUENT, CADENCE_TYPE_IMPERFECT_AUTHENTIC, 1, 1, false},
        {"S2", "B", SECTION_ROLE_SEQUENCE, 4, SECTION_BEGINNING_ANTECEDENT, SECTION_ENDING_CONSEQUENT, CADENCE_TYPE_IMPERFECT_AUTHENTIC, FORM_SPEC_HOME, 1, false},
        {"L", "B", SECTION_ROLE_LIQUIDATION, 4, SECTION_BEGINNING_PRESENTATION, SECTION_ENDING_CONTINUATION, CADENCE_TYPE_HALF, FORM_SPEC_HOME, FORM_SPEC_NO_RETURN, false},
        {"A return", "A", SECTION_ROLE_RETURN, 8, SECTION_BEGINNING_PRESENTATION, SECTION_ENDING_CONTINUATION, CADENCE_TYPE_PERFECT_AUTHENTIC, FORM_SPEC_HOME, 0, true},
        {"E", "E", SECTION_ROLE_EPISODE, 4, SECTION_BEGINNING_ANTECEDENT, SECTION_ENDING_CONSEQUENT, CADENCE_TYPE_IMPERFECT_AUTHENTIC, 2, FORM_SPEC_NO_RETURN, false},
        {"C1", "C", SECTION_ROLE_CODETTA, 4, SECTION_BEGINNING_ANTECEDENT, SECTION_ENDING_CADENTIAL, CADENCE_TYPE_PERFECT_AUTHENTIC, FORM_SPEC_HOME, FORM_SPEC_NO_RETURN, true},
        {"C2", "C", SECTION_ROLE_CODETTA, 4, SECTION_BEGINNING_COMPOUND_BASIC_IDEA, SECTION_ENDING_CADENTIAL, CADENCE_TYPE_PERFECT_AUTHENTIC, FORM_SPEC_HOME, FORM_SPEC_NO_RETURN, false},
    };
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *size = &form->sizes[z];
        size_t long_theme = size->bars == 60 ? 16 : 8;
        assert(size->section_count == 9 && (size->bars == 44 || size->bars == 60));
        for (size_t i = 0; i < 9; i++) {
            const SectionDescription *section = &size->sections[i];
            size_t bars = (i == 0 || i == 5) ? long_theme : SCHERZO[i].bars;
            assert(strcmp(section->name, SCHERZO[i].name) == 0 && strcmp(section->letter, SCHERZO[i].letter) == 0);
            assert(section->role == SCHERZO[i].role && section->bars == bars && section->cadence == SCHERZO[i].cadence);
            assert(section->closed == SCHERZO[i].closed && returns_of(size, section) == SCHERZO[i].returns);
            assert(key_of(form, &section->key_start) == SCHERZO[i].key && key_of(form, &section->key_end) == SCHERZO[i].key);
            if (section->role != SECTION_ROLE_LIQUIDATION) {
                assert(section->pairing.beginning == SCHERZO[i].beginning && section->pairing.ending == SCHERZO[i].ending);
            }
            /* Only the two long themes of the larger size have a construction, and the return keeps the first's. */
            assert(section->has_construction == (size->bars == 60 && (i == 0 || i == 5)) && !section->construction_auto);
            assert(section->pedal == 0 && !section->leads_in && !has_motive(section));
        }
        assert(size->sections[2].variation == SECTION_RETURN_EXACT && size->sections[3].variation == SECTION_RETURN_EXACT &&
               size->sections[5].variation == SECTION_RETURN_VARIED);
    }
    assert(form_spec_size(form, 0, 0)->bars == 44 && form_spec_size(form, 60, 0)->sections[0].construction.idea_bars[0] != 0);
    form_spec_destroy(form);
}

/* forms/main-and-subordinate.json (R-509): a main theme, the transition, the subordinate theme (and at 57 bars the lyric one),
   a codetta, the retransition, the return of A and the coda; sizes 48 and 57, one region (V in major, III in minor). */
static void test_main_and_subordinate(void) {
    FormSpec *form = builtin("main-and-subordinate");
    assert(form->closing && form->modulates && form->region_count == 1 && form->tempo == 0 && form->meter_count == 0 &&
           form->part_count == 0 && form->repeat_count == 0);
    assert(strcmp(form->label, "Main and subordinate themes") == 0 && strcmp(form->regions[0].name, "subordinate") == 0);
    assert(form->regions[0].major_count == 1 && strcmp(form->regions[0].major[0], "omt.relation.dominant") == 0 && form->regions[0].minor_count == 1 &&
           strcmp(form->regions[0].minor[0], "omt.relation.mediant") == 0);
    typedef struct {
        const char *name;
        const char *letter;
        SectionRole role;
        size_t bars;
        SectionBeginning beginning;
        SectionEnding ending;
        CadenceType cadence;
        int key_start;
        int key_end;
        size_t returns;
        bool closed;
    } Row;
    const SectionBeginning PRES = SECTION_BEGINNING_PRESENTATION;
    const SectionBeginning ANT = SECTION_BEGINNING_ANTECEDENT;
    const SectionEnding CONTINUATION = SECTION_ENDING_CONTINUATION;
    const SectionEnding CONSEQUENT = SECTION_ENDING_CONSEQUENT;
    const CadenceType PAC = CADENCE_TYPE_PERFECT_AUTHENTIC;
    const size_t NO = FORM_SPEC_NO_RETURN;
    const Row A = {"A", "A", SECTION_ROLE_THEME, 8, PRES, CONTINUATION, PAC, FORM_SPEC_HOME, FORM_SPEC_HOME, NO, true};
    const Row T = {"T", "T", SECTION_ROLE_TRANSITION, 8, PRES, CONTINUATION, CADENCE_TYPE_HALF, FORM_SPEC_HOME, 0, NO, false};
    const Row S1_57 = {"S1", "B", SECTION_ROLE_SUBORDINATE, 9, PRES, CONTINUATION, PAC, 0, 0, NO, false};
    const Row S1_48 = {"S1", "B", SECTION_ROLE_SUBORDINATE, 8, PRES, CONTINUATION, PAC, 0, 0, NO, false};
    const Row S2 = {"S2", "B", SECTION_ROLE_SUBORDINATE, 8, ANT, CONSEQUENT, PAC, 0, 0, NO, false};
    const Row C = {"C", "C", SECTION_ROLE_CODETTA, 4, ANT, SECTION_ENDING_CADENTIAL, PAC, 0, 0, NO, false};
    const Row R = {"R", "R", SECTION_ROLE_RETRANSITION, 4, PRES, CONTINUATION, CADENCE_TYPE_HALF, FORM_SPEC_HOME, FORM_SPEC_HOME, NO, false};
    const Row AR = {"A return", "A", SECTION_ROLE_RETURN, 8, PRES, CONTINUATION, PAC, FORM_SPEC_HOME, FORM_SPEC_HOME, 0, true};
    const Row K = {"K", "K", SECTION_ROLE_CODA, 8, PRES, CONTINUATION, PAC, FORM_SPEC_HOME, FORM_SPEC_HOME, NO, true};
    const Row AT_57[] = {A, T, S1_57, S2, C, R, AR, K};
    const Row AT_48[] = {A, T, S1_48, C, R, AR, K};
    assert(form->size_count == 2 && form_spec_default_size(form)->bars == 57);
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *size = &form->sizes[z];
        const bool large = size->bars == 57;
        const Row *rows = large ? AT_57 : AT_48;
        assert(size->count == 0 && size->section_count == (large ? 8u : 7u) && (large || size->bars == 48));
        for (size_t i = 0; i < size->section_count; i++) {
            const SectionDescription *section = &size->sections[i];
            assert(strcmp(section->name, rows[i].name) == 0 && strcmp(section->letter, rows[i].letter) == 0);
            assert(section->role == rows[i].role && section->bars == rows[i].bars && section->cadence == rows[i].cadence);
            assert(key_of(form, &section->key_start) == rows[i].key_start && key_of(form, &section->key_end) == rows[i].key_end);
            assert(returns_of(size, section) == rows[i].returns && section->closed == rows[i].closed);
            /* The retransition and the coda have no pairing: they are made by operators (R-503). */
            if (section->role != SECTION_ROLE_RETRANSITION && section->role != SECTION_ROLE_CODA) {
                assert(section->pairing.beginning == rows[i].beginning && section->pairing.ending == rows[i].ending);
            }
            assert(section->pedal == 0 && !section->leads_in && !section->has_figure && section->mode != SECTION_MODE_PARALLEL);
            /* The transition is not independent, and only S2 is lyric (which reads its motive as m0 augmented). */
            const bool lyric = large && i == 3;
            assert(!section->function.independent && section->function.lyric == lyric);
            assert(has_motive(section) == (i == 2 || lyric));
            if (i == 2) {
                assert(motive_of(section).kind == MOTIVE_RELATION_INVERT && motive_of(section).value[0] == -1 && motive_of(section).value[1] == 1);
            }
            if (lyric) {
                assert(motive_of(section).kind == MOTIVE_RELATION_AUGMENT && motive_of(section).value[0] == 0);
            }
            /* Only S1 at 57 bars has a construction: nine bars with the fragment 2 repeated. */
            assert(section->has_construction == (large && i == 2) && !section->construction_auto);
        }
        assert(size->sections[size->section_count - 2].variation == SECTION_RETURN_EXACT);
    }
    const SectionConstruction *s1 = &form_spec_size(form, 57, 0)->sections[2].construction;
    assert(s1->extension_count == 1 && s1->extensions[0].kind == SECTION_EXTENSION_REPEAT);
    form_spec_destroy(form);
}

/* forms/figured-variations.json (R-500): the theme, the variations with their figures, the minore, the coda; sizes 40 and 56. */
static void test_figured_variations(void) {
    FormSpec *form = builtin("figured-variations");
    assert(form->closing && form->region_count == 0 && form->tempo == 0 && form->part_count == 0 && form->repeat_count == 0);
    assert(strcmp(form->label, "Figured variations") == 0 && form->meter_count == 4);
    static const struct {
        int beats_per_bar;
        int beat_unit;
    } METERS[] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    for (size_t m = 0; m < 4; m++) {
        assert(form->meters[m].beats_per_bar == METERS[m].beats_per_bar && form->meters[m].beat_unit == METERS[m].beat_unit);
    }
    typedef struct {
        SectionFigureKind kind;
        SectionFigureRhythm rhythm;
        bool reharmonize;
        unsigned change_at;
        bool climax;
        bool parallel;
    } FigureRow;
    static const FigureRow V1 = {SECTION_FIGURE_NEIGHBOR, SECTION_FIGURE_EIGHTHS, false, 0, false, false};
    static const FigureRow V2 = {SECTION_FIGURE_NEIGHBOR, SECTION_FIGURE_SIXTEENTHS, false, 0, false, false};
    static const FigureRow V3 = {SECTION_FIGURE_SCALE, SECTION_FIGURE_EIGHTHS, true, 0, false, true};
    static const FigureRow V4 = {SECTION_FIGURE_SCALE, SECTION_FIGURE_SIXTEENTHS, true, 3, false, false};
    static const FigureRow V5 = {SECTION_FIGURE_BROKEN_CHORD, SECTION_FIGURE_SIXTEENTHS, false, 0, true, false};
    const FigureRow *const at56[] = {&V1, &V2, &V3, &V4, &V5};
    const FigureRow *const at40[] = {&V1, &V3, &V5};
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *size = &form->sizes[z];
        const bool large = size->bars == 56;
        const size_t variations = large ? 5u : 3u;
        const FigureRow *const *figures = large ? at56 : at40;
        assert((large || size->bars == 40) && size->count == variations && size->is_default == large);
        assert(size->section_count == variations + 3);
        /* The theme: an eight-bar period, closed, home. */
        const SectionDescription *theme = &size->sections[0];
        assert(strcmp(theme->name, "theme") == 0 && strcmp(theme->letter, "T") == 0 && theme->role == SECTION_ROLE_THEME &&
               theme->bars == 8 && theme->closed && theme->returns[0] == '\0' && !theme->has_figure && theme->mode != SECTION_MODE_PARALLEL &&
               theme->pairing.beginning == SECTION_BEGINNING_ANTECEDENT && theme->pairing.ending == SECTION_ENDING_CONSEQUENT &&
               theme->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
        for (size_t v = 0; v < variations; v++) {
            const SectionDescription *section = &size->sections[v + 1];
            char letter[32];
            snprintf(letter, sizeof(letter), "V%zu", v + 1);
            assert(strcmp(section->letter, letter) == 0 && section->role == SECTION_ROLE_VARIATION && section->bars == 8 &&
                   section->closed && returns_of(size, section) == 0 && section->variation == SECTION_RETURN_FIGURATE &&
                   section->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC && !has_motive(section) && !section->has_construction);
            assert(section->pairing.beginning == SECTION_BEGINNING_ANTECEDENT && section->pairing.ending == SECTION_ENDING_CONSEQUENT);
            assert(section->has_figure && section->figure.kind == figures[v]->kind && section->figure.rhythm == figures[v]->rhythm &&
                   section->figure.reharmonize == figures[v]->reharmonize && section->figure.change_at == figures[v]->change_at &&
                   section->figure.climax == figures[v]->climax && (section->mode == SECTION_MODE_PARALLEL) == figures[v]->parallel);
            /* No keys: the parallel mode is not a key lane, and every other variation is home. */
            assert(key_of(form, &section->key_start) == FORM_SPEC_HOME && key_of(form, &section->key_end) == FORM_SPEC_HOME);
        }
        /* The coda: two codettas, the second a compound basic idea that is not closed. */
        const SectionDescription *c1 = &size->sections[variations + 1];
        const SectionDescription *c2 = &size->sections[variations + 2];
        assert(strcmp(c1->name, "C1") == 0 && strcmp(c1->letter, "C") == 0 && c1->role == SECTION_ROLE_CODETTA && c1->bars == 4 &&
               c1->closed && c1->pairing.beginning == SECTION_BEGINNING_ANTECEDENT && c1->pairing.ending == SECTION_ENDING_CADENTIAL);
        assert(strcmp(c2->name, "C2") == 0 && strcmp(c2->letter, "C") == 0 && c2->role == SECTION_ROLE_CODETTA && c2->bars == 4 &&
               !c2->closed && c2->pairing.beginning == SECTION_BEGINNING_COMPOUND_BASIC_IDEA &&
               c2->pairing.ending == SECTION_ENDING_CADENTIAL && c1->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC &&
               c2->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
        assert(!c1->has_figure && !c2->has_figure && c1->returns[0] == '\0' && c2->returns[0] == '\0');
    }
    assert(form_spec_size(form, 0, 0)->bars == 56 && form_spec_size(form, 0, 3)->bars == 40 && form_spec_size(form, 56, 0)->count == 5);
    /* Eight sections at 56 bars and six at 40, under the limit. */
    assert(form_spec_size(form, 56, 0)->section_count == 8 && form_spec_size(form, 40, 0)->section_count == 6 &&
           form_spec_size(form, 56, 0)->section_count < FORM_SPEC_MAX_SECTIONS);
    form_spec_destroy(form);
}

/* A plan made from each built-in is named for the form, and the form is found again from it. */
static void test_for_plan(void) {
    for (size_t f = 0; f < FORM_COUNT; f++) {
        /* A compound form has no plan of its own: each of its parts is a form that has (the performed plan is named for the
           form by piece_perform_compound, which test_piece_perform and test_api_piece read). */
        if (FORMS[f].parts > 0) {
            continue;
        }
        PieceRequest request;
        memset(&request, 0, sizeof(request));
        request.form = FORMS[f].name;
        request.key = test_key(0, "major");
        request.knowledge = test_knowledge();
        request.meter.beats_per_bar = FORMS[f].meters > 0 ? 3 : 4;
        request.meter.beat_unit = 4;
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        const PlanFormRecord *record = plan_form_record(plan);
        assert(record && strcmp(record->name, FORMS[f].name) == 0);
        plan_destroy(plan);
    }
    PiecePlan *plan = plan_create();
    assert(plan_form_record(plan) == NULL);
    plan_destroy(plan);
}

/* ------------------------------------------------------------- the reader's refusals */

static void test_refusals(void) {
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(NULL, &form, &error) == FORM_SPEC_ERR_NULL);
    assert(form_spec_parse("not json", &form, &error) == FORM_SPEC_ERR_PARSE && form == NULL);
    refused("[]", "");
    refused("{}", "/schema");

    /* Members: an unknown one at every level, a wrong type, a missing one. */
    refused_edit("rounded-binary", "\"closing\": true,", "\"closing\": true, \"bogus\": 1,", "/bogus");
    refused_edit("rounded-binary", "\"closing\": true,", "\"closing\": true, \"a/b~c\": 1,", "/a~1b~0c");
    refused_edit("rounded-binary", "{\"name\": \"away\",", "{\"bogus\": 1, \"name\": \"away\",", "/regions/0/bogus");
    refused_edit("rounded-binary", "\"keys\": {\"end\"", "\"keys\": {\"bogus\": 1, \"end\"", "/sizes/0/sections/0/keys/bogus");
    refused_edit("rounded-binary", "{\"bars\": 16, \"default\"", "{\"bogus\": 1, \"bars\": 16, \"default\"", "/sizes/0/bogus");
    refused_edit("rounded-binary", "{\"name\": \"A\", \"letter\": \"A\",", "{\"bogus\": 1, \"name\": \"A\", \"letter\": \"A\",",
                 "/sizes/0/sections/0/bogus");
    refused_edit("rounded-binary", "\"closing\": true", "\"closing\": \"yes\"", "/closing");
    refused_edit("rounded-binary", "\"label\": \"Rounded binary\",", "", "/label");
    refused_edit("rounded-binary", "\"label\": \"Rounded binary\"", "\"label\": 4", "/label");
    refused_edit("rounded-binary", "\"schema\": \"ursatz-form\"", "\"schema\": \"ursatz-plan\"", "/schema");
    refused_edit("rounded-binary", "\"version\": 2", "\"version\": 1", "/version");
    refused_edit("rounded-binary", "\"version\": 2", "\"version\": \"2\"", "/version");
    refused_edit("rounded-binary", "\"sizes\": [", "\"sizes\": 3, \"x\": [", "/x");
    refused_edit("rounded-binary", "\"letter\": \"A\", \"role\"", "\"role\"", "/sizes/0/sections/0/letter");
    refused_edit("rounded-binary", "\"bars\": 8, \"beginning\"", "\"bars\": \"8\", \"beginning\"", "/sizes/0/sections/0/bars");

    /* Names. */
    refused_edit("rounded-binary", "\"name\": \"rounded-binary\"", "\"name\": \"Rounded_binary\"", "/name");
    refused_edit("rounded-binary", "\"name\": \"rounded-binary\"", "\"name\": \"\"", "/name");
    refused_edit("rounded-binary", "\"name\": \"rounded-binary\"",
                 "\"name\": \"a-name-of-thirty-two-characters--\"", "/name");
    refused_edit("rounded-binary", "\"closing\": true", "\"plan_name\": \"No Spaces\", \"closing\": true", "/plan_name");

    /* The vocabularies: role, beginning, ending, cadence, region, variation, check, lane. */
    refused_edit("rounded-binary", "\"role\": \"theme\"", "\"role\": \"chorus\"", "/sizes/0/sections/0/role");
    refused_edit("rounded-binary", "\"beginning\": \"presentation\"", "\"beginning\": \"sentence\"", "/sizes/0/sections/0/beginning");
    refused_edit("rounded-binary", "\"ending\": \"continuation\"", "\"ending\": \"close\"", "/sizes/0/sections/0/ending");
    refused_edit("rounded-binary", "\"cadence\": \"perfect-authentic\"", "\"cadence\": \"full\"", "/sizes/0/sections/0/cadence");
    refused_edit("rounded-binary", "\"end\": \"away\"", "\"end\": \"nowhere\"", "/sizes/0/sections/0/keys/end");
    refused_edit("rounded-binary", "\"start\": \"away\"", "\"start\": 5", "/sizes/0/sections/1/keys/start");
    refused_edit("rounded-binary", "\"variation\": \"exact\"", "\"variation\": \"tweaked\"", "/sizes/0/sections/2/variation");
    refused_edit("rounded-binary", "\"closing\": true", "\"closing\": true, \"checks\": [\"sections\", \"bogus\"]", "/checks/1");
    refused_edit("rounded-binary", "\"closing\": true", "\"closing\": true, \"checks\": \"all\"", "/checks");
    refused_edit("layered", "[\"bass\", \"inner_voice\"]", "[\"bass\", \"guitar\"]", "/sizes/1/sections/1/layers/1");
    refused_edit("layered", "\"layers\": [\"bass\"]", "\"layers\": \"some\"", "/sizes/0/sections/0/layers");
    refused_edit("layered", "\"accompaniment\": \"required\"", "\"accompaniment\": \"always\"", "/accompaniment");
    refused_edit("rounded-binary", "\"motive\": \"invert tonal=1\"", "\"motive\": \"twist\"", "/sizes/0/sections/1/motive");
    /* A cadence of the vocabulary whose core is not a PAC, an IAC or an HC. */
    refused_edit("rounded-binary", "\"cadence\": \"perfect-authentic\"", "\"cadence\": \"plagal\"", "/sizes/0/sections/0/cadence");
    refused_edit("rounded-binary", "\"cadence\": \"perfect-authentic\"", "\"cadence\": \"evaded\"", "/sizes/0/sections/0/cadence");
    refused_edit("rounded-binary", "\"cadence\": \"perfect-authentic\"", "\"cadence\": \"abandoned\"", "/sizes/0/sections/0/cadence");

    /* Regions. */
    refused_edit("rounded-binary", "\"name\": \"away\"", "\"name\": \"home\"", "/regions/0/name");
    refused_edit("rounded-binary", "\"major\": [\"omt.relation.dominant\"]", "\"major\": []", "/regions/0/major");
    refused_edit("rounded-binary", "\"major\": [\"omt.relation.dominant\"]", "\"major\": [8]", "/regions/0/major/0");
    refused_edit("rounded-binary", "\"minor\": [\"omt.relation.mediant\"]", "\"minor\": [3.5]", "/regions/0/minor/0");
    refused_edit("rounded-binary", "\"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.mediant\"]", "\"major\": [\"omt.relation.dominant\"]", "/regions/0/minor");
    refused_edit("rounded-binary", "\"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.mediant\"]}]",
                 "\"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.mediant\"]}, {\"name\": \"away\", \"major\": [\"omt.relation.subdominant\"], \"minor\": [\"omt.relation.subdominant\"]}]",
                 "/regions/1/name");

    /* Lengths: bars and beats together or neither. */
    refused_edit("rounded-binary", "\"role\": \"theme\", \"bars\": 8,", "\"role\": \"theme\", \"bars\": 8, \"beats\": 32,",
                 "/sizes/0/sections/0");
    refused_edit("rounded-binary", "\"role\": \"theme\", \"bars\": 8,", "\"role\": \"theme\",", "/sizes/0/sections/0");
    refused_edit("rounded-binary", "\"role\": \"theme\", \"bars\": 8,", "\"role\": \"theme\", \"bars\": 0,", "/sizes/0/sections/0/bars");

    /* Names in a size, returns and variation. */
    refused_edit("rounded-binary", "{\"name\": \"B\", \"letter\": \"B\"", "{\"name\": \"A\", \"letter\": \"B\"", "/sizes/0/sections/1/name");
    refused_edit("rounded-binary", "\"returns\": \"A\"", "\"returns\": \"Z\"", "/sizes/0/sections/2/returns");
    refused_edit("rounded-binary", "\"returns\": \"A\"", "\"returns\": \"A'\"", "/sizes/0/sections/2/returns");
    refused_edit("rounded-binary", "\"closed\": true,", "\"closed\": true, \"returns\": \"B\",", "/sizes/0/sections/0/returns");
    refused_edit("rounded-binary", "\"closed\": true,", "\"closed\": true, \"variation\": \"ornament\",", "/sizes/0/sections/0/variation");

    /* Sizes: the sum of the bars, defaults, two of one size. */
    refused_edit("rounded-binary", "{\"bars\": 16, \"default\"", "{\"bars\": 20, \"default\"", "/sizes/0/bars");
    refused_edit("rounded-binary", "{\"bars\": 24, \"sections\"", "{\"bars\": 24, \"default\": true, \"sections\"", "/sizes");
    refused_edit("rounded-binary", "\"default\": true, ", "", "/sizes");
    char text[8192];
    char sizes[2048];
    document(text, sizeof(text), "", "");
    refused(text, "/sizes");
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}, {\"bars\": 8, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), "", sizes);
    refused(text, "/sizes/1");
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"count\": 2, \"default\": true, \"sections\": [" SECTION_A "]}, {\"bars\": 8, \"count\": 2, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), "", sizes);
    refused(text, "/sizes/1");
    /* The same bars with other counts are two sizes. */
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"count\": 1, \"default\": true, \"sections\": [" SECTION_A "]}, {\"bars\": 8, \"count\": 2, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), "", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    form_spec_destroy(form);

    /* A form whose sections the request gives has no sizes; one with sections other than "request" is refused. */
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), ", \"sections\": \"request\"", sizes);
    refused(text, "/sizes");
    document(text, sizeof(text), ", \"sections\": \"given\"", sizes);
    refused(text, "/sections");
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
             "\"roles\": [\"body\"], \"loop\": {\"roles\": [\"body\"]}, \"cadences\": {\"default\": \"half\"}}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->request_sections && form->size_count == 0);
    assert(form_spec_size(form, 0, 0) == NULL && form_spec_default_size(form) == NULL);
    form_spec_destroy(form);

    /* The limits: more regions, sizes and sections than the form holds. */
    char *big = malloc(1 << 17);
    char *list = malloc(1 << 16);
    assert(big && list);
    list[0] = '\0';
    for (int i = 0; i <= FORM_SPEC_MAX_REGIONS; i++) {
        char region[96];
        snprintf(region, sizeof(region), "%s{\"name\": \"r%d\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.mediant\"]}", i ? ", " : "", i);
        strcat(list, region);
    }
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");
    snprintf(text, sizeof(text), ", \"regions\": [%s]", list);
    document(big, 1 << 17, text, sizes);
    refused(big, "/regions");
    list[0] = '\0';
    for (int i = 0; i <= FORM_SPEC_MAX_SIZES; i++) {
        char size[400];
        snprintf(size, sizeof(size), "%s{\"bars\": %d, \"default\": %s, \"sections\": [{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", "
                                     "\"bars\": %d, \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"half\"}]}",
                 i ? ", " : "", 4 * (i + 1), i == 0 ? "true" : "false", 4 * (i + 1));
        strcat(list, size);
    }
    document(big, 1 << 17, "", list);
    refused(big, "/sizes");
    list[0] = '\0';
    for (int i = 0; i <= FORM_SPEC_MAX_SECTIONS; i++) {
        char section[256];
        snprintf(section, sizeof(section),
                 "%s{\"name\": \"S%d\", \"letter\": \"S\", \"role\": \"theme\", \"bars\": 4, \"beginning\": \"presentation\", "
                 "\"ending\": \"continuation\", \"cadence\": \"half\"}",
                 i ? ", " : "", i);
        strcat(list, section);
    }
    snprintf(text, sizeof(text), "{\"bars\": %d, \"default\": true, \"sections\": [%s]}", 4 * (FORM_SPEC_MAX_SECTIONS + 1), list);
    document(big, 1 << 17, "", text);
    refused(big, "/sizes/0/sections");
    /* Exactly the most sections the form holds parses (R-515). */
    list[0] = '\0';
    for (int i = 0; i < FORM_SPEC_MAX_SECTIONS; i++) {
        char section[256];
        snprintf(section, sizeof(section),
                 "%s{\"name\": \"S%d\", \"letter\": \"S\", \"role\": \"theme\", \"bars\": 4, \"beginning\": \"presentation\", "
                 "\"ending\": \"continuation\", \"cadence\": \"half\"}",
                 i ? ", " : "", i);
        strcat(list, section);
    }
    snprintf(text, sizeof(text), "{\"bars\": %d, \"default\": true, \"sections\": [%s]}", 4 * FORM_SPEC_MAX_SECTIONS, list);
    document(big, 1 << 17, "", text);
    assert(form_spec_parse(big, &form, &error) == FORM_SPEC_OK && form);
    assert(form->size_count == 1 && form->sizes[0].section_count == FORM_SPEC_MAX_SECTIONS && form->sizes[0].bars == 64);
    form_spec_destroy(form);
    free(big);
    free(list);
}

/* ------------------------------------------------------------- meters and tempo (R-487) */

static void test_meters_and_tempo(void) {
    char sizes[512];
    char text[1024];
    FormSpec *form = NULL;
    FormSpecError error;
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");

    document(text, sizeof(text), ", \"meters\": [\"3/4\"], \"tempo\": 112", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form);
    assert(form->meter_count == 1 && form->meters[0].beats_per_bar == 3 && form->meters[0].beat_unit == 4 && form->tempo == 112);
    PlanMeter three_four = {3, 4, 0};
    PlanMeter four_four = {4, 4, 0};
    assert(form_spec_meter_allowed(form, &three_four) && !form_spec_meter_allowed(form, &four_four));
    form_spec_destroy(form);

    document(text, sizeof(text), ", \"meters\": [\"3/4\", \"6/8\"]", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->meter_count == 2 && form->tempo == 0);
    PlanMeter six_eight = {6, 8, 0};
    assert(form_spec_meter_allowed(form, &six_eight) && form_spec_meter_allowed(form, &three_four));
    form_spec_destroy(form);

    document(text, sizeof(text), ", \"meters\": [\"5/4\"]", sizes);
    refused(text, "/meters/0");
    document(text, sizeof(text), ", \"meters\": [\"3/4\", \"3/4\"]", sizes);
    refused(text, "/meters/1");
    document(text, sizeof(text), ", \"meters\": []", sizes);
    refused(text, "/meters");
    document(text, sizeof(text), ", \"meters\": [\"2/4\", \"3/4\", \"4/4\", \"6/8\", \"3/4\"]", sizes);
    refused(text, "/meters");
    document(text, sizeof(text), ", \"meters\": [3]", sizes);
    refused(text, "/meters/0");
    document(text, sizeof(text), ", \"meters\": \"3/4\"", sizes);
    refused(text, "/meters");
    document(text, sizeof(text), ", \"tempo\": 20", sizes);
    refused(text, "/tempo");
    document(text, sizeof(text), ", \"tempo\": 301", sizes);
    refused(text, "/tempo");
    document(text, sizeof(text), ", \"tempo\": 112.5", sizes);
    refused(text, "/tempo");

    /* A form whose sections the request gives has neither. */
    refused_edit("game-loop", "\"closing\"", "\"meters\": [\"4/4\"], \"closing\"", "/meters");
    refused_edit("game-loop", "\"closing\"", "\"tempo\": 120, \"closing\"", "/tempo");

    /* The minuet, the trio and the minuet and trio are in 3/4 at 112 and allow no other meter; the scherzo is at 176 in 3/4,
       2/4 and 6/8 and the scherzo and trio in 3/4 only; the Andante forms are at 72, the small rondos at 88 and the large and sonata rondos at 132 with no meters; every other built-in lists no meters, gives no
       tempo and allows any meter (R-487, R-492, R-520). */
    for (size_t i = 0; i < form_spec_builtin_count(); i++) {
        FormSpec *builtin_form = builtin(form_spec_builtin_name(i));
        bool dance = strcmp(builtin_form->name, "minuet") == 0 || strcmp(builtin_form->name, "trio") == 0 ||
                     strcmp(builtin_form->name, "minuet-and-trio") == 0;
        bool scherzo = strcmp(builtin_form->name, "scherzo") == 0;
        bool scherzo_trio = strcmp(builtin_form->name, "scherzo-and-trio") == 0;
        size_t meters = dance || scherzo_trio ? 1u : scherzo ? 3u : strcmp(builtin_form->name, "figured-variations") == 0 ? 4u : 0u;
        unsigned tempo = dance ? 112u : (scherzo || scherzo_trio) ? 176u : strncmp(builtin_form->name, "andante-", 8) == 0 ? 72u
                         : strncmp(builtin_form->name, "small-rondo-", 12) == 0 ? 88u
                         : (strcmp(builtin_form->name, "large-rondo") == 0 || strcmp(builtin_form->name, "sonata-rondo") == 0 ||
                            strncmp(builtin_form->name, "sonatina", 8) == 0) ? 132u : 0u;
        assert(builtin_form->meter_count == meters && builtin_form->tempo == tempo);
        assert(form_spec_meter_allowed(builtin_form, &three_four));
        assert(form_spec_meter_allowed(builtin_form, &six_eight) == !(dance || scherzo_trio));
        form_spec_destroy(builtin_form);
    }
}

/* tests/fixtures/forms/meter-form.json: a document that lists 3/4 and a tempo; its plan builds in 3/4 and not in 4/4. */
static int test_meters(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/meter-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    free(text);
    assert(form->meter_count == 1 && form->meters[0].beats_per_bar == 3 && form->meters[0].beat_unit == 4 && form->tempo == 112);
    PieceRequest request;
    PiecePlan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    assert(form_build_plan(form, &request, &plan) == PIECE_ERR_BAD_REQUEST && plan == NULL);
    request.meter.beats_per_bar = 3;
    assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
    plan_destroy(plan);
    form_spec_destroy(form);
    printf("meter-form: refused in 4/4, built in 3/4\n");
    return 0;
}

/* ------------------------------------------------------------- the game loop */

/* A form whose sections the request gives, with the members the test varies (roles, loop roles, cadences). */
static void loop_document(char *out, size_t cap, const char *roles, const char *loop, const char *cadences, const char *rest) {
    snprintf(out, cap,
             "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
             "\"roles\": %s, \"loop\": {\"roles\": %s}, \"cadences\": %s%s}",
             roles, loop, cadences, rest);
}

#define ALL_ROLES "[\"intro\", \"body\", \"turnaround\", \"ending\"]"
#define LOOP_ROLES "[\"body\", \"turnaround\"]"
#define CADENCES "{\"default\": \"imperfect-authentic\", \"last\": [{\"roles\": [\"ending\"], \"cadence\": \"perfect-authentic\"}]}"

/* forms/game-loop.json (R-419): what docs/forms.md says of it, then the refusals of its own members. */
static void test_game_loop(void) {
    FormSpec *form = builtin("game-loop");
    assert(strcmp(form->name, "game-loop") == 0 && strcmp(form->plan_name, "structure") == 0 && form->label[0] != '\0');
    assert(form->request_sections && form->size_count == 0 && !form->closing && form->region_count == 0 &&
           form->checks == FORM_CHECK_ALL && !form->accompaniment_required);
    assert(form->role_count == 4 && form->roles[0] == SECTION_ROLE_INTRO && form->roles[1] == SECTION_ROLE_BODY &&
           form->roles[2] == SECTION_ROLE_TURNAROUND && form->roles[3] == SECTION_ROLE_ENDING);
    assert(form->loop_role_count == 2 && form->loop_roles[0] == SECTION_ROLE_BODY && form->loop_roles[1] == SECTION_ROLE_TURNAROUND);
    assert(form_spec_role_in_loop(form, SECTION_ROLE_BODY) && !form_spec_role_in_loop(form, SECTION_ROLE_INTRO) &&
           form_spec_has_role(form, SECTION_ROLE_ENDING) && !form_spec_has_role(form, SECTION_ROLE_THEME));
    assert(form->default_cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
           strcmp(form->default_cadence_name, "imperfect-authentic") == 0 && form->last_cadence_count == 3);
    assert(form->last_cadences[0].role_count == 1 && form->last_cadences[0].roles[0] == SECTION_ROLE_ENDING &&
           form->last_cadences[0].cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(form->last_cadences[1].role_count == 2 && form->last_cadences[1].roles[0] == SECTION_ROLE_BODY &&
           form->last_cadences[1].roles[1] == SECTION_ROLE_TURNAROUND && form->last_cadences[1].cadence == CADENCE_TYPE_HALF);
    assert(form->last_cadences[2].role_count == 1 && form->last_cadences[2].roles[0] == SECTION_ROLE_INTRO &&
           form->last_cadences[2].cadence == CADENCE_TYPE_HALF);
    assert(form->has_default_ending && strcmp(form->default_ending.id, "end") == 0 &&
           strcmp(form->default_ending.letter, "E") == 0 && form->default_ending.role == SECTION_ROLE_ENDING &&
           form->default_ending.bars == 2 && form->default_ending.cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    form_spec_destroy(form);

    char text[1024];
    FormSpec *parsed = NULL;
    FormSpecError error;
    /* A valid document of the members. */
    loop_document(text, sizeof(text), ALL_ROLES, LOOP_ROLES, CADENCES, "");
    assert(form_spec_parse(text, &parsed, &error) == FORM_SPEC_OK && parsed->request_sections && !parsed->has_default_ending);
    form_spec_destroy(parsed);

    /* roles: an unknown role, a role of the sized forms, roles out of order or twice, no body. */
    loop_document(text, sizeof(text), "[\"intro\", \"chorus\"]", LOOP_ROLES, CADENCES, "");
    refused(text, "/roles/1");
    loop_document(text, sizeof(text), "[\"body\", \"theme\"]", "[\"body\"]", CADENCES, "");
    refused(text, "/roles/1");
    loop_document(text, sizeof(text), "[\"body\", \"intro\"]", "[\"body\"]", CADENCES, "");
    refused(text, "/roles/1");
    loop_document(text, sizeof(text), "[\"intro\", \"body\", \"body\"]", "[\"body\"]", CADENCES, "");
    refused(text, "/roles/2");
    loop_document(text, sizeof(text), "[\"intro\", \"ending\"]", "[\"ending\"]", CADENCES, "");
    refused(text, "/roles");
    loop_document(text, sizeof(text), "[]", LOOP_ROLES, CADENCES, "");
    refused(text, "/roles");
    loop_document(text, sizeof(text), "\"body\"", LOOP_ROLES, CADENCES, "");
    refused(text, "/roles");
    /* loop: a role the form does not have. */
    loop_document(text, sizeof(text), "[\"intro\", \"body\"]", LOOP_ROLES, CADENCES, "");
    refused(text, "/loop/roles/1");
    loop_document(text, sizeof(text), ALL_ROLES, "[\"body\", \"coda\"]", CADENCES, "");
    refused(text, "/loop/roles/1");
    /* cadences: a rule for a role the form does not have, a cadence that is not PAC, IAC or HC, an unknown one. */
    loop_document(text, sizeof(text), "[\"body\"]", "[\"body\"]",
                  "{\"default\": \"half\", \"last\": [{\"roles\": [\"ending\"], \"cadence\": \"half\"}]}", "");
    refused(text, "/cadences/last/0/roles/0");
    loop_document(text, sizeof(text), ALL_ROLES, LOOP_ROLES, "{\"default\": \"plagal\"}", "");
    refused(text, "/cadences/default");
    loop_document(text, sizeof(text), ALL_ROLES, LOOP_ROLES,
                  "{\"default\": \"half\", \"last\": [{\"roles\": [\"body\"], \"cadence\": \"sonata\"}]}", "");
    refused(text, "/cadences/last/0/cadence");
    loop_document(text, sizeof(text), ALL_ROLES, LOOP_ROLES, "{\"default\": \"half\", \"order\": 1}", "");
    refused(text, "/cadences/order");
    /* default_ending: only the ending role. */
    loop_document(text, sizeof(text), ALL_ROLES, LOOP_ROLES, CADENCES,
                  ", \"default_ending\": {\"id\": \"end\", \"letter\": \"E\", \"role\": \"body\", \"bars\": 2, \"cadence\": \"half\"}");
    refused(text, "/default_ending/role");
    loop_document(text, sizeof(text), ALL_ROLES, LOOP_ROLES, CADENCES,
                  ", \"default_ending\": {\"id\": \"end\", \"letter\": \"E\", \"role\": \"ending\", \"cadence\": \"half\"}");
    refused(text, "/default_ending/bars");
    /* The members of a request-section form are missing, or given to a form that has sizes. */
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
             "\"roles\": " ALL_ROLES ", \"cadences\": " CADENCES "}");
    refused(text, "/loop");
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
             "\"roles\": " ALL_ROLES ", \"loop\": {\"roles\": " LOOP_ROLES "}}");
    refused(text, "/cadences");
    char sizes[512];
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), ", \"roles\": [\"body\"]", sizes);
    refused(text, "/roles");
    document(text, sizeof(text), ", \"loop\": {\"roles\": [\"body\"]}", sizes);
    refused(text, "/loop");
}

/*
 * The cadences the game loop's document gives a section that names none, for layouts of parts: what the cadences of
 * R-169 were, written out here so the test does not read them from the document it checks.
 */
typedef struct {
    const char *layout;
    PiecePart parts[6];
    CadenceType cadences[6];
    size_t count;
} CadenceRow;

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define IAC CADENCE_TYPE_IMPERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

static const CadenceRow CADENCE_ROWS[] = {
    {"intro, body, body, turnaround, ending",
     {PIECE_PART_INTRO, PIECE_PART_BODY, PIECE_PART_BODY, PIECE_PART_TURNAROUND, PIECE_PART_ENDING},
     {HC, IAC, IAC, HC, PAC}, 5},
    {"body", {PIECE_PART_BODY}, {HC}, 1},
    {"intro, body", {PIECE_PART_INTRO, PIECE_PART_BODY}, {HC, HC}, 2},
    {"body, body, ending", {PIECE_PART_BODY, PIECE_PART_BODY, PIECE_PART_ENDING}, {IAC, HC, PAC}, 3},
    {"body, turnaround, turnaround",
     {PIECE_PART_BODY, PIECE_PART_TURNAROUND, PIECE_PART_TURNAROUND},
     {IAC, IAC, HC}, 3},
    {"intro, intro, body, ending, ending",
     {PIECE_PART_INTRO, PIECE_PART_INTRO, PIECE_PART_BODY, PIECE_PART_ENDING, PIECE_PART_ENDING},
     {IAC, HC, HC, IAC, PAC}, 5},
};

static void test_default_cadences(void) {
    FormSpec *form = builtin("game-loop");
    for (size_t r = 0; r < sizeof(CADENCE_ROWS) / sizeof(CADENCE_ROWS[0]); r++) {
        const CadenceRow *row = &CADENCE_ROWS[r];
        PieceSection sections[6];
        PieceRequest request;
        memset(sections, 0, sizeof(sections));
        memset(&request, 0, sizeof(request));
        request.form = "game-loop";
        request.sections = sections;
        request.section_count = row->count;
        for (size_t i = 0; i < row->count; i++) {
            sections[i].part = row->parts[i];
        }
        piece_default_cadences(form, &request);
        for (size_t i = 0; i < row->count; i++) {
            if (!sections[i].has_cadence || sections[i].cadence != row->cadences[i]) {
                fprintf(stderr, "layout %s: section %zu has cadence %d, expected %d\n", row->layout, i, (int)sections[i].cadence,
                        (int)row->cadences[i]);
                exit(1);
            }
        }
    }
    /* A section with a cadence of its own keeps it, and the rest are still defaulted around it. */
    PieceSection sections[3];
    PieceRequest request;
    memset(sections, 0, sizeof(sections));
    memset(&request, 0, sizeof(request));
    request.sections = sections;
    request.section_count = 3;
    sections[0].part = PIECE_PART_BODY;
    sections[1].part = PIECE_PART_BODY;
    sections[1].has_cadence = true;
    sections[1].cadence = CADENCE_TYPE_PLAGAL;
    sections[2].part = PIECE_PART_ENDING;
    piece_default_cadences(form, &request);
    assert(sections[0].cadence == IAC && sections[1].cadence == CADENCE_TYPE_PLAGAL && sections[2].cadence == PAC);
    form_spec_destroy(form);
}

/* ------------------------------------------------------------- form_spec_size */

static void test_sizes(void) {
    for (size_t f = 0; f < FORM_COUNT; f++) {
        const FormRow *row = &FORMS[f];
        FormSpec *form = builtin(row->name);
        const FormSpecSize *chosen = form_spec_size(form, 0, 0);
        assert(chosen && chosen->is_default && form_spec_default_size(form) == chosen);
        for (size_t z = 0; z < row->size_count; z++) {
            const SizeRow *size = &row->sizes[z];
            const FormSpecSize *by_bars = form_spec_size(form, size->bars, 0);
            assert(by_bars && by_bars->bars == size->bars && by_bars->count == size->count);
            if (size->count > 0) {
                /* A count picks the size with it, and bars then must match. */
                assert(form_spec_size(form, 0, size->count) == by_bars && form_spec_size(form, size->bars, size->count) == by_bars);
                assert(form_spec_size(form, size->bars + 8, size->count) == NULL);
            } else {
                assert(form_spec_size(form, size->bars, 1) == NULL);
            }
        }
        for (size_t b = 0; b < row->bad_count; b++) {
            if (form_spec_size(form, row->bad[b].bars, row->bad[b].count) != NULL) {
                fprintf(stderr, "%s has a size of %zu bars, count %zu\n", row->name, row->bad[b].bars, row->bad[b].count);
                exit(1);
            }
        }
        form_spec_destroy(form);
    }
    assert(form_spec_size(NULL, 0, 0) == NULL && form_spec_default_size(NULL) == NULL);
}

/* ------------------------------------------------------------- a form added as a document alone */

#define CUSTOM_SEEDS 10

/* Composes the form's piece with the seed; the plan is returned (the caller destroys it), the result in *ran. */
static PiecePlan *compose_custom(const FormSpec *form, unsigned long seed, ComposeOutcome *outcome, PieceResult *ran) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    PiecePlan *plan = NULL;
    PieceResult built = form_build_plan(form, &request, &plan);
    assert(built == PIECE_OK && plan);
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.form = form;
    *ran = compose_run(plan, &config, outcome);
    return plan;
}

/* Every melody span in the section is written by the Composer, and there is at least one. */
static bool section_written(const PiecePlan *plan, NodeId section) {
    LaneSpanId ids[16];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, section), ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_state(plan, ids[i]) != PLAN_STATE_REALIZED) {
            return false;
        }
    }
    return found > 0;
}

static int test_uneven_form(void);

static int test_custom(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/bar-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    free(text);
    assert(strcmp(form->name, "bar-form") == 0 && form->size_count == 1 && form->sizes[0].bars == 24 &&
           form->sizes[0].section_count == 3 && form->checks == FORM_CHECK_ALL);
    /* The form is not one of the built-in ones: only its document says it exists. */
    FormSpec *builtin_none = NULL;
    assert(form_spec_builtin("bar-form", &builtin_none) == FORM_SPEC_ERR_UNKNOWN);

    unsigned long first = 0;
    char *first_json = NULL;
    for (unsigned long seed = 1; seed <= CUSTOM_SEEDS && !first; seed++) {
        ComposeOutcome outcome;
        PieceResult ran;
        PiecePlan *plan = compose_custom(form, seed, &outcome, &ran);
        if (ran == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = 0;
            NodeId root = plan_root(plan);
            for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
                NodeId child = plan_node_child_at(plan, root, i);
                if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
                    sections[count++] = child;
                }
            }
            assert(count == 3 && outcome.section_count == 3);
            for (size_t i = 0; i < 3; i++) {
                assert(outcome.sections[i].section == sections[i] && section_written(plan, sections[i]));
            }
            assert(outcome.returns_planned == 1);
            first = seed;
            first_json = plan_to_json(plan, NULL);
            assert(first_json);
        } else {
            printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    if (!first) {
        fprintf(stderr, "bar-form: no seed of 1 to %d completes\n", CUSTOM_SEEDS);
        return 1;
    }

    /* The same seed with the form naming one check: what is written does not change, the returns are not heard. */
    FormSpec *sections_only = NULL;
    char *again_text = test_must_read_file(path, NULL);
    FormSpecResult parsed = form_spec_parse(again_text, &sections_only, NULL);
    assert(parsed == FORM_SPEC_OK);
    free(again_text);
    sections_only->checks = FORM_CHECK_SECTIONS;
    ComposeOutcome outcome;
    PieceResult ran;
    PiecePlan *plan = compose_custom(sections_only, first, &outcome, &ran);
    assert(ran == PIECE_OK);
    /* The plan records the checks of the form it was built from; the written notes are the same. */
    PlanFormRecord recorded = *plan_form_record(plan);
    assert(recorded.checks == FORM_CHECK_SECTIONS);
    recorded.checks = FORM_CHECK_ALL;
    assert(plan_set_form_record(plan, &recorded) == PLAN_OK);
    char *again = plan_to_json(plan, NULL);
    assert(again && strcmp(again, first_json) == 0);
    assert(outcome.returns_planned == 0 && outcome.section_count == 3);
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    free(again);
    free(first_json);
    form_spec_destroy(sections_only);
    form_spec_destroy(form);
    printf("bar-form: composed with seed %lu (first of 1 to %d); with only the sections check the plan is the same and no return is heard\n",
           first, CUSTOM_SEEDS);
    return test_uneven_form();
}

/* ------------------------------------------------------------- a form with uneven sections */

/* The seeds of 1 to 10 that compose uneven-form.json, measured: a 4+6 period and a 4+6 sentence with an evaded cadence. */
#define UNEVEN_COMPOSED 10

static int test_uneven_form(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/uneven-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    free(text);
    assert(strcmp(form->name, "uneven-form") == 0 && form->sizes[0].bars == 20 && form->sizes[0].section_count == 2 &&
           form->sizes[0].sections[0].has_construction && form->sizes[0].sections[0].bars == 10 &&
           form->sizes[0].sections[1].has_construction && form->sizes[0].sections[1].bars == 10);

    int composed = 0;
    for (unsigned long seed = 1; seed <= CUSTOM_SEEDS; seed++) {
        ComposeOutcome outcome;
        PieceResult ran;
        PiecePlan *plan = compose_custom(form, seed, &outcome, &ran);
        if (ran == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = 0;
            NodeId root = plan_root(plan);
            for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
                NodeId child = plan_node_child_at(plan, root, i);
                if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
                    sections[count++] = child;
                }
            }
            assert(count == 2 && outcome.section_count == 2);
            for (size_t i = 0; i < 2; i++) {
                const SectionDescription *construction = plan_node_section(plan, sections[i]);
                assert(construction->has_construction && construction->construction.reason == SECTION_CONSTRUCTION_GIVEN);
                assert(section_written(plan, sections[i]) && outcome.sections[i].construction_text[0] != '\0');
            }
            composed++;
        } else {
            printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    form_spec_destroy(form);
    if (composed != UNEVEN_COMPOSED) {
        fprintf(stderr, "uneven-form: %d of %d seeds compose, %d expected\n", composed, CUSTOM_SEEDS, UNEVEN_COMPOSED);
        return 1;
    }
    printf("uneven-form: %d of %d seeds compose, each section with its construction\n", composed, CUSTOM_SEEDS);
    return 0;
}

/* The section member pedal (R-446): "dominant" under a presentation, nothing else. */
static void test_pedal(void) {
    char text[4096];
    char sizes[2048];
    FormSpec *form = NULL;
    FormSpecError error;
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), "", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[0].pedal == 0);
    form_spec_destroy(form);

    snprintf(sizes, sizeof(sizes),
             "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");
    char *with = replaced_in(sizes, "\"cadence\"", "\"pedal\": \"dominant\", \"cadence\"");
    document(text, sizeof(text), "", with);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[0].pedal == 5);
    form_spec_destroy(form);
    free(with);

    with = replaced_in(sizes, "\"cadence\"", "\"pedal\": \"tonic\", \"cadence\"");
    document(text, sizeof(text), "", with);
    refused(text, "/sizes/0/sections/0/pedal");
    free(with);
    with = replaced_in(sizes, "\"cadence\"", "\"pedal\": 5, \"cadence\"");
    document(text, sizeof(text), "", with);
    refused(text, "/sizes/0/sections/0/pedal");
    free(with);

    /* An antecedent has no presentation to put over a pedal. */
    with = replaced_in(sizes, "\"cadence\"", "\"pedal\": \"dominant\", \"cadence\"");
    char *antecedent = replaced_in(with, "\"presentation\"", "\"antecedent\"");
    char *consequent = replaced_in(antecedent, "\"continuation\"", "\"consequent\"");
    document(text, sizeof(text), "", consequent);
    refused(text, "/sizes/0/sections/0/pedal");
    free(with);
    free(antecedent);
    free(consequent);
}

/* ------------------------------------------------------------- a section's construction */

/* The text of one size of `bars` bars with one theme section A of `bars` bars and the given `construction` member text
   (with its leading comma, or ""). */
static void construction_document(char *out, size_t cap, size_t bars, const char *role, const char *construction) {
    char sizes[2048];
    snprintf(sizes, sizeof(sizes),
             "{\"bars\": %zu, \"default\": true, \"sections\": [{\"name\": \"A\", \"letter\": \"A\", \"role\": \"%s\", "
             "\"bars\": %zu, \"beginning\": \"presentation\", \"ending\": \"continuation\", "
             "\"cadence\": \"perfect-authentic\"%s}]}",
             bars, role, bars, construction);
    document(out, cap, "", sizes);
}

#define SIX_BAR_SENTENCE                                                                                                   \
    ", \"construction\": {\"ideas\": {\"basic idea\": 2, \"repetition\": 1, \"fragment 1\": 1, \"fragment 2\": 1, "         \
    "\"cadential\": 1}}"

/* The section member construction (R-468): "auto" or the bars of each idea, 3 to 16 bars, and a return keeping its source's. */
static void test_construction(void) {
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;

    /* The object: read as the Composer's construction, reason given. */
    construction_document(text, sizeof(text), 6, "theme", SIX_BAR_SENTENCE);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    const SectionDescription *section = &form->sizes[0].sections[0];
    assert(section->has_construction && !section->construction_auto && section->bars == 6 &&
           section->construction.reason == SECTION_CONSTRUCTION_GIVEN && section->construction.extension_count == 0 &&
           section->construction.idea_bars[GROW_IDEA_BASIC] == 2 && section->construction.idea_bars[GROW_IDEA_REPETITION] == 1 &&
           section->construction.idea_bars[GROW_IDEA_CADENTIAL] == 1 && section->construction.idea_bars[GROW_IDEA_CONTRASTING] == 0);
    form_spec_destroy(form);

    /* With an extension: its bars count toward the section's (an evaded cadence of 0 bars is the last idea's own). */
    construction_document(text, sizeof(text), 7, "theme",
                          ", \"construction\": {\"ideas\": {\"basic idea\": 2, \"repetition\": 1, \"fragment 1\": 1, "
                          "\"fragment 2\": 1, \"cadential\": 1}, \"extensions\": [{\"kind\": \"repeat\", \"idea\": \"fragment 2\", "
                          "\"bars\": 1, \"varied\": true}]}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    section = &form->sizes[0].sections[0];
    assert(section->construction.extension_count == 1 && section->construction.extensions[0].kind == SECTION_EXTENSION_REPEAT &&
           section->construction.extensions[0].idea == GROW_IDEA_FRAGMENT_2 && section->construction.extensions[0].bars == 1 &&
           section->construction.extensions[0].varied);
    form_spec_destroy(form);

    /* "auto": chosen when the form is built, so there is nothing to add up. */
    construction_document(text, sizeof(text), 9, "theme", ", \"construction\": \"auto\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    section = &form->sizes[0].sections[0];
    assert(section->has_construction && section->construction_auto && section->bars == 9);
    form_spec_destroy(form);

    /* 4 and 8 bars need none; no other length of a theme-like section does without. */
    construction_document(text, sizeof(text), 4, "theme", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && !form->sizes[0].sections[0].has_construction);
    form_spec_destroy(form);
    construction_document(text, sizeof(text), 6, "theme", "");
    refused(text, "/sizes/0/sections/0/bars");
    construction_document(text, sizeof(text), 12, "theme", "");
    refused(text, "/sizes/0/sections/0/bars");

    /* A construction is 3 to 16 bars, of a theme-like section of whole bars. */
    construction_document(text, sizeof(text), 2, "theme", ", \"construction\": \"auto\"");
    refused(text, "/sizes/0/sections/0/bars");
    construction_document(text, sizeof(text), 17, "theme", ", \"construction\": \"auto\"");
    refused(text, "/sizes/0/sections/0/bars");
    construction_document(text, sizeof(text), 9, "body", ", \"construction\": \"auto\"");
    refused(text, "/sizes/0/sections/0/construction");

    /* What is wrong in the member, at its own path. */
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": \"chosen\"");
    refused(text, "/sizes/0/sections/0/construction");
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": 6");
    refused(text, "/sizes/0/sections/0/construction");
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": {}");
    refused(text, "/sizes/0/sections/0/construction/ideas");
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": {\"ideas\": {\"basic idea\": 6}, \"bars\": 6}");
    refused(text, "/sizes/0/sections/0/construction/bars");
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": {\"ideas\": {\"theme\": 6}}");
    refused(text, "/sizes/0/sections/0/construction/ideas/theme");
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": {\"ideas\": {\"basic idea\": 2.5}}");
    refused(text, "/sizes/0/sections/0/construction/ideas/basic idea");
    construction_document(text, sizeof(text), 6, "theme", ", \"construction\": {\"ideas\": {\"basic idea\": 2, \"repetition\": 2}}");
    refused(text, "/sizes/0/sections/0/construction");
    construction_document(text, sizeof(text), 7, "theme",
                          ", \"construction\": {\"ideas\": {\"basic idea\": 7}, \"extensions\": [{\"kind\": \"slide\"}]}");
    refused(text, "/sizes/0/sections/0/construction/extensions/0/kind");
    construction_document(text, sizeof(text), 7, "theme",
                          ", \"construction\": {\"ideas\": {\"basic idea\": 7}, \"extensions\": [{\"kind\": \"repeat\"}]}");
    refused(text, "/sizes/0/sections/0/construction/extensions/0/idea");
    construction_document(text, sizeof(text), 7, "theme",
                          ", \"construction\": {\"ideas\": {\"basic idea\": 7}, \"extensions\": [{\"kind\": \"evaded\", \"bars\": 5}]}");
    refused(text, "/sizes/0/sections/0/construction/extensions/0/bars");

    /* A return copies its source, so it keeps the source's construction: the same, or none on both. */
    char sizes[3072];
    const char *first = "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 9, \"beginning\": \"presentation\", "
                        "\"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"construction\": \"auto\"}";
    const char *same = "{\"name\": \"A'\", \"letter\": \"A'\", \"role\": \"return\", \"bars\": 9, \"beginning\": \"presentation\", "
                       "\"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"returns\": \"A\", \"construction\": \"auto\"}";
    const char *none = "{\"name\": \"A'\", \"letter\": \"A'\", \"role\": \"return\", \"bars\": 8, \"beginning\": \"presentation\", "
                       "\"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"returns\": \"A\"}";
    snprintf(sizes, sizeof(sizes), "{\"bars\": 18, \"default\": true, \"sections\": [%s, %s]}", first, same);
    document(text, sizeof(text), "", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].construction_auto);
    form_spec_destroy(form);
    snprintf(sizes, sizeof(sizes), "{\"bars\": 17, \"default\": true, \"sections\": [%s, %s]}", first, none);
    document(text, sizeof(text), "", sizes);
    refused(text, "/sizes/0/sections/1/construction");
    FormSpecResult result = form_spec_parse(text, &form, &error);
    assert(result == FORM_SPEC_ERR_INVALID && strcmp(error.message, "a return keeps the construction of A") == 0);
}

/* ------------------------------------------------------------- a form with a dominant pedal */

#define PEDAL_SEEDS 10
#define PEDAL_COMPOSED_LEAST 9

/* In B's first half every chord is V, its seventh or the neighbor six-four, and every bass note is the dominant. */
static void check_pedal_half(const PiecePlan *plan, NodeId section) {
    PlanSpan half = plan_node_span(plan, section);
    assert(rational_div_int(half.length, 2, &half.length) == RATIONAL_OK);
    PhraseChord chords[16];
    PhraseMetre metre;
    size_t read = 0;
    assert(harmony_read_lane(plan, half, HARMONY_MODE_MAJOR, chords, &read, &metre) && read >= 1 && read <= 16);
    for (size_t i = 0; i < read; i++) {
        const HarmonyChord *chord = &chords[i].chord;
        assert(!chords[i].applied && !chord->leading_tone);
        bool dominant = chord->degree == 5 && chord->inversion == 0;
        bool six_four = chord->degree == 1 && chord->inversion == 2 && !chord->seventh;
        assert(dominant || six_four);
    }
    LaneSpanId bass[32];
    size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, half, bass, 32);
    assert(found >= 1 && found <= 32);
    for (size_t i = 0; i < found; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, bass[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(notes->notes[n].midi % 12 == 7);
        }
    }
}

static int test_pedal_form(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/pedal-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    free(text);
    assert(strcmp(form->name, "pedal-form") == 0 && form->sizes[0].section_count == 3 &&
           form->sizes[0].sections[0].pedal == 0 && form->sizes[0].sections[1].pedal == 5 &&
           form->sizes[0].sections[2].pedal == 0);
    FormSpec *builtin_none = NULL;
    assert(form_spec_builtin("pedal-form", &builtin_none) == FORM_SPEC_ERR_UNKNOWN);

    int composed = 0;
    for (unsigned long seed = 1; seed <= PEDAL_SEEDS; seed++) {
        ComposeOutcome outcome;
        PieceResult ran;
        PiecePlan *plan = compose_custom(form, seed, &outcome, &ran);
        if (ran == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = 0;
            NodeId root = plan_root(plan);
            for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
                NodeId child = plan_node_child_at(plan, root, i);
                if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
                    sections[count++] = child;
                }
            }
            assert(count == 3 && plan_node_section(plan, sections[1])->pedal == 5);
            assert(plan_node_section(plan, sections[0])->pedal == 0);
            check_pedal_half(plan, sections[1]);
            composed++;
        } else {
            printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    form_spec_destroy(form);
    if (composed < PEDAL_COMPOSED_LEAST) {
        fprintf(stderr, "pedal-form: %d of %d seeds compose, at least %d expected\n", composed, PEDAL_SEEDS, PEDAL_COMPOSED_LEAST);
        return 1;
    }
    printf("pedal-form: %d of %d seeds compose, B's first half on the dominant in each\n", composed, PEDAL_SEEDS);
    return 0;
}

/* ------------------------------------------------------------- the pattern roles and a form that always modulates (R-493) */

static void test_modulating_reader(void) {
    static const struct {
        const char *role;
        size_t bars;
        const char *extra;
    } ROLES_OK[] = {{"pattern", 4, ""}, {"sequence", 4, ""}, {"episode", 8, ""}, {"codetta", 6, ", \"construction\": \"auto\""}};
    char text[4096];
    char sizes[2048];
    FormSpec *form = NULL;
    FormSpecError error;
    for (size_t i = 0; i < sizeof(ROLES_OK) / sizeof(ROLES_OK[0]); i++) {
        SectionRole role;
        assert(section_role_find(ROLES_OK[i].role, &role) && section_role_is_theme(role));
        assert(strcmp(section_role_name(role), ROLES_OK[i].role) == 0 && section_role_label(role) != NULL);
        snprintf(sizes, sizeof(sizes),
                 "{\"bars\": %zu, \"default\": true, \"sections\": [{\"name\": \"A\", \"letter\": \"A\", \"role\": \"%s\", "
                 "\"bars\": %zu, \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": "
                 "\"perfect-authentic\"%s}]}",
                 ROLES_OK[i].bars, ROLES_OK[i].role, ROLES_OK[i].bars, ROLES_OK[i].extra);
        document(text, sizeof(text), "", sizes);
        assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[0].role == role);
        form_spec_destroy(form);
    }
    /* A codetta of 6 bars needs a construction. */
    snprintf(sizes, sizeof(sizes),
             "{\"bars\": 6, \"default\": true, \"sections\": [{\"name\": \"A\", \"letter\": \"A\", \"role\": \"codetta\", "
             "\"bars\": 6, \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\"}]}");
    document(text, sizeof(text), "", sizes);
    refused(text, "/sizes/0/sections/0/bars");

    /* modulates: true parses with regions, false is the default, a non-boolean and the two refusals. */
    snprintf(sizes, sizeof(sizes), "{\"bars\": 8, \"default\": true, \"sections\": [" SECTION_A "]}");
    document(text, sizeof(text), ", \"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.dominant\"]}], \"modulates\": true", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->modulates);
    form_spec_destroy(form);
    document(text, sizeof(text), ", \"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.dominant\"]}], \"modulates\": false", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && !form->modulates);
    form_spec_destroy(form);
    document(text, sizeof(text), ", \"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.dominant\"]}], \"modulates\": 1", sizes);
    refused(text, "/modulates");
    document(text, sizeof(text), ", \"modulates\": true", sizes);
    refused(text, "/modulates");
    refused_edit("game-loop", "\"closing\"", "\"modulates\": true, \"closing\"", "/modulates");
    /* Only the scherzo, the main and subordinate themes, the Andante forms and the rondos modulate always; the scherzo and trio leaves it to its parts (R-492,
       R-495, R-509). */
    for (size_t f = 0; f < form_spec_builtin_count(); f++) {
        FormSpec *built = builtin(form_spec_builtin_name(f));
        assert(built->modulates == (strcmp(built->name, "scherzo") == 0 || strcmp(built->name, "main-and-subordinate") == 0 ||
                                    strncmp(built->name, "andante-", 8) == 0 || strncmp(built->name, "small-rondo-", 12) == 0 ||
                                    strcmp(built->name, "large-rondo") == 0 || strcmp(built->name, "sonata-rondo") == 0 ||
                                    strncmp(built->name, "sonatina", 8) == 0));
        form_spec_destroy(built);
    }
}

/* ------------------------------------------------------------- the figure of a figurate return (R-497) */

/* A size of a theme and a variation of 8 bars each; `variation` and `figure` are the variation's members (with a leading comma). */
static void figure_document(char *out, size_t cap, const char *variation, const char *figure) {
    snprintf(out, cap,
             "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sizes\": [{\"bars\": 16, "
             "\"default\": true, \"sections\": ["
             "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true}, "
             "{\"name\": \"V1\", \"letter\": \"V1\", \"role\": \"variation\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true, \"returns\": \"T\"%s%s}]}]}",
             variation, figure);
}

#define FIGURATE ", \"variation\": \"figurate\""
#define FIGURE_PATH "/sizes/0/sections/1/figure"

/* tests/fixtures/forms/figure-form.json: three figurate variations; every other form has no figure; each refusal at its path. */
static void test_figure_reader(void) {
    char path[1024];
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    snprintf(path, sizeof(path), "%s/forms/figure-form.json", COMPOSER_FIXTURE_DIR);
    char *fixture = test_must_read_file(path, NULL);
    assert(form_spec_parse(fixture, &form, &error) == FORM_SPEC_OK && form);
    free(fixture);
    assert(strcmp(form->name, "figure-form") == 0 && form->size_count == 1 && form->sizes[0].bars == 32 &&
           form->sizes[0].count == 3 && form->sizes[0].section_count == 4);
    const SectionDescription *sections = form->sizes[0].sections;
    assert(!sections[0].has_figure && sections[0].returns[0] == '\0');
    for (size_t i = 1; i < 4; i++) {
        assert(sections[i].has_figure && returns_of(&form->sizes[0], &sections[i]) == 0 && sections[i].variation == SECTION_RETURN_FIGURATE);
    }
    assert(sections[1].figure.kind == SECTION_FIGURE_NEIGHBOR && sections[1].figure.rhythm == SECTION_FIGURE_EIGHTHS &&
           !sections[1].figure.reharmonize && sections[1].figure.change_at == 0 && !sections[1].figure.climax);
    assert(sections[2].figure.kind == SECTION_FIGURE_SCALE && sections[2].figure.rhythm == SECTION_FIGURE_SIXTEENTHS &&
           sections[2].figure.reharmonize && sections[2].figure.change_at == 3 && !sections[2].figure.climax);
    assert(sections[3].figure.kind == SECTION_FIGURE_BROKEN_CHORD && sections[3].figure.rhythm == SECTION_FIGURE_SIXTEENTHS &&
           !sections[3].figure.reharmonize && sections[3].figure.change_at == 0 && sections[3].figure.climax);
    form_spec_destroy(form);

    /* The names the constraint, the form and the plan share. */
    SectionFigureKind kind;
    SectionFigureRhythm rhythm;
    assert(section_figure_kind_find("broken-chord", &kind) && kind == SECTION_FIGURE_BROKEN_CHORD &&
           strcmp(section_figure_kind_name(SECTION_FIGURE_SCALE), "scale") == 0 && !section_figure_kind_find("run", &kind));
    assert(section_figure_rhythm_find("sixteenths", &rhythm) && rhythm == SECTION_FIGURE_SIXTEENTHS &&
           strcmp(section_figure_rhythm_name(SECTION_FIGURE_EIGHTHS), "eighths") == 0 && !section_figure_rhythm_find("triplets", &rhythm));
    assert(section_figure_kind_name((SectionFigureKind)9) == NULL && section_figure_rhythm_name((SectionFigureRhythm)9) == NULL);

    /* A figure with only its two required members reads with the defaults. */
    figure_document(text, sizeof(text), FIGURATE, ", \"figure\": {\"kind\": \"neighbor\", \"rhythm\": \"eighths\"}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].has_figure &&
           !form->sizes[0].sections[1].figure.reharmonize && form->sizes[0].sections[1].figure.change_at == 0 &&
           !form->sizes[0].sections[1].figure.climax);
    form_spec_destroy(form);
    /* The last bar the root change may be at is the one before the section's last. */
    figure_document(text, sizeof(text), FIGURATE,
                    ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"reharmonized\", \"change_at\": 7}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].figure.change_at == 7);
    form_spec_destroy(form);

    static const struct {
        const char *variation;
        const char *figure;
        const char *path;
    } REFUSED[] = {
        /* a figure needs the variation figurate; a figurate variation needs a figure */
        {"", ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}", FIGURE_PATH},
        {", \"variation\": \"ornament\"", ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}", FIGURE_PATH},
        {FIGURATE, "", "/sizes/0/sections/1/variation"},
        /* a root change needs the harmony reharmonized, and a bar of the section before its last */
        {FIGURATE, ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"change_at\": 3}", FIGURE_PATH "/change_at"},
        {FIGURATE,
         ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"kept\", \"change_at\": 3}",
         FIGURE_PATH "/change_at"},
        {FIGURATE,
         ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"reharmonized\", \"change_at\": 0}",
         FIGURE_PATH "/change_at"},
        {FIGURATE,
         ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"reharmonized\", \"change_at\": 8}",
         FIGURE_PATH "/change_at"},
        {FIGURATE,
         ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"reharmonized\", \"change_at\": 2.5}",
         FIGURE_PATH "/change_at"},
        /* the members of the figure */
        {FIGURATE, ", \"figure\": []", FIGURE_PATH},
        {FIGURATE, ", \"figure\": {\"rhythm\": \"eighths\"}", FIGURE_PATH "/kind"},
        {FIGURATE, ", \"figure\": {\"kind\": \"scale\"}", FIGURE_PATH "/rhythm"},
        {FIGURATE, ", \"figure\": {\"kind\": \"run\", \"rhythm\": \"eighths\"}", FIGURE_PATH "/kind"},
        {FIGURATE, ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"triplets\"}", FIGURE_PATH "/rhythm"},
        {FIGURATE, ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"changed\"}", FIGURE_PATH "/harmony"},
        {FIGURATE, ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"climax\": 1}", FIGURE_PATH "/climax"},
        {FIGURATE, ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"shape\": \"x\"}", FIGURE_PATH "/shape"},
    };
    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        figure_document(text, sizeof(text), REFUSED[i].variation, REFUSED[i].figure);
        refused(text, REFUSED[i].path);
    }

    /* The parallel mode (R-499): tests/fixtures/forms/parallel-form.json; each refusal at its path. */
    snprintf(path, sizeof(path), "%s/forms/parallel-form.json", COMPOSER_FIXTURE_DIR);
    fixture = test_must_read_file(path, NULL);
    assert(form_spec_parse(fixture, &form, &error) == FORM_SPEC_OK && form->sizes[0].section_count == 3);
    free(fixture);
    assert(form->sizes[0].sections[1].mode == SECTION_MODE_PARALLEL && form->sizes[0].sections[1].figure.reharmonize &&
           form->sizes[0].sections[1].has_figure && form->sizes[0].sections[0].mode != SECTION_MODE_PARALLEL &&
           form->sizes[0].sections[2].mode != SECTION_MODE_PARALLEL);
    form_spec_destroy(form);
    figure_document(text, sizeof(text), FIGURATE ", \"mode\": \"parallel\"",
                    ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"reharmonized\"}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].mode == SECTION_MODE_PARALLEL);
    form_spec_destroy(form);
    static const struct {
        const char *variation;
        const char *figure;
        const char *path;
    } PARALLEL_REFUSED[] = {
        /* the parallel mode needs a figurate variation, has no keys, and reharmonizes */
        {", \"mode\": \"parallel\"", "", "/sizes/0/sections/1/mode"},
        {", \"variation\": \"ornament\", \"mode\": \"parallel\"", "", "/sizes/0/sections/1/mode"},
        {FIGURATE ", \"mode\": \"relative\"", ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}",
         "/sizes/0/sections/1/mode"},
        {FIGURATE ", \"mode\": \"parallel\", \"keys\": {\"start\": \"home\"}",
         ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}", "/sizes/0/sections/1/mode"},
        {FIGURATE ", \"mode\": \"parallel\"",
         ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"kept\"}",
         FIGURE_PATH "/harmony"},
    };
    for (size_t i = 0; i < sizeof(PARALLEL_REFUSED) / sizeof(PARALLEL_REFUSED[0]); i++) {
        figure_document(text, sizeof(text), PARALLEL_REFUSED[i].variation, PARALLEL_REFUSED[i].figure);
        refused(text, PARALLEL_REFUSED[i].path);
    }

    /* A section given in beats has no bars to hold a figure. */
    snprintf(text, sizeof(text),
             "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sizes\": [{\"bars\": 16, "
             "\"default\": true, \"sections\": ["
             "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true}, "
             "{\"name\": \"V1\", \"letter\": \"V1\", \"role\": \"variation\", \"beats\": 32, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true, \"returns\": \"T\"%s"
             ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}}]}]}",
             FIGURATE);
    refused(text, FIGURE_PATH);

    /* No built-in form has a figure but the figured variations (B4 of P50). */
    for (size_t i = 0; i < form_spec_builtin_count(); i++) {
        FormSpec *built = builtin(form_spec_builtin_name(i));
        const bool figured = strcmp(built->name, "figured-variations") == 0;
        for (size_t z = 0; z < built->size_count; z++) {
            for (size_t k = 0; k < built->sizes[z].section_count; k++) {
                const SectionDescription *section = &built->sizes[z].sections[k];
                assert(section->has_figure == (figured && section->role == SECTION_ROLE_VARIATION) &&
                       (section->variation == SECTION_RETURN_FIGURATE) == section->has_figure);
            }
        }
        form_spec_destroy(built);
    }
}

/* ------------------------------------------------------------- the tonic major of a return (R-517) */

/* A returning section may be in the home tonic's major key: it reads as tonic_major, not parallel; each refusal at /mode. */
static void test_tonic_major_reader(void) {
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    const char *const MAJOR = ", \"variation\": \"exact\", \"mode\": \"major\"";
    figure_document(text, sizeof(text), MAJOR, "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].section_count == 2);
    assert(form->sizes[0].sections[1].mode == SECTION_MODE_TONIC_MAJOR && form->sizes[0].sections[1].mode != SECTION_MODE_PARALLEL &&
           form->sizes[0].sections[0].mode != SECTION_MODE_TONIC_MAJOR);
    form_spec_destroy(form);
    /* "parallel" reads as before and is not the tonic major. */
    figure_document(text, sizeof(text), FIGURATE ", \"mode\": \"parallel\"",
                    ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\", \"harmony\": \"reharmonized\"}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].mode == SECTION_MODE_PARALLEL &&
           form->sizes[0].sections[1].mode != SECTION_MODE_TONIC_MAJOR);
    form_spec_destroy(form);
    /* a section in the tonic major has no keys, is a returning section, and is not figurate */
    figure_document(text, sizeof(text), ", \"variation\": \"exact\", \"mode\": \"major\", \"keys\": {\"start\": \"home\"}", "");
    refused(text, "/sizes/0/sections/1/mode");
    snprintf(text, sizeof(text),
             "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sizes\": [{\"bars\": 16, "
             "\"default\": true, \"sections\": ["
             "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true, \"mode\": \"major\"}, "
             "{\"name\": \"V1\", \"letter\": \"V1\", \"role\": \"variation\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true, \"returns\": \"T\"}]}]}");
    refused(text, "/sizes/0/sections/0/mode");
    figure_document(text, sizeof(text), FIGURATE ", \"mode\": \"major\"",
                    ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}");
    refused(text, "/sizes/0/sections/1/mode");
    /* a mode that is neither */
    figure_document(text, sizeof(text), ", \"variation\": \"exact\", \"mode\": \"minor\"", "");
    refused(text, "/sizes/0/sections/1/mode");
    /* the refusals name the messages of R-517 */
    figure_document(text, sizeof(text), ", \"variation\": \"exact\", \"mode\": \"major\", \"keys\": {\"start\": \"home\"}", "");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK && strcmp(error.message, "a section in the tonic major has no keys") == 0);
    figure_document(text, sizeof(text), FIGURATE ", \"mode\": \"major\"", ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK &&
           strcmp(error.message, "a figurate variation takes the parallel mode, not the tonic major") == 0);
}

/* A returning section may be at home in the home mode (R-524): it reads as home_mode, with no other mode; each refusal at /mode. */
static void test_home_mode_reader(void) {
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    figure_document(text, sizeof(text), ", \"variation\": \"reharmonize\", \"mode\": \"home\"", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].section_count == 2);
    assert(form->sizes[0].sections[1].mode == SECTION_MODE_HOME && form->sizes[0].sections[1].mode != SECTION_MODE_PARALLEL &&
           form->sizes[0].sections[1].mode != SECTION_MODE_TONIC_MAJOR && form->sizes[0].sections[0].mode != SECTION_MODE_HOME);
    form_spec_destroy(form);
    /* a section in the home mode has no keys */
    figure_document(text, sizeof(text), ", \"variation\": \"reharmonize\", \"mode\": \"home\", \"keys\": {\"start\": \"home\"}", "");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK && strcmp(error.path, "/sizes/0/sections/1/mode") == 0 &&
           strcmp(error.message, "a section in the home mode has no keys") == 0);
    /* it is a returning section */
    snprintf(text, sizeof(text),
             "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"sizes\": [{\"bars\": 16, "
             "\"default\": true, \"sections\": ["
             "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true, \"mode\": \"home\"}, "
             "{\"name\": \"V1\", \"letter\": \"V1\", \"role\": \"variation\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true, \"returns\": \"T\"}]}]}");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK && strcmp(error.path, "/sizes/0/sections/0/mode") == 0 &&
           strcmp(error.message, "the home mode is for a returning section") == 0);
    /* and it is reharmonized */
    figure_document(text, sizeof(text), ", \"variation\": \"exact\", \"mode\": \"home\"", "");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK && strcmp(error.path, "/sizes/0/sections/1/mode") == 0 &&
           strcmp(error.message, "a return in the home mode is reharmonized") == 0);
    figure_document(text, sizeof(text), FIGURATE ", \"mode\": \"home\"", ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK && strcmp(error.path, "/sizes/0/sections/1/mode") == 0 &&
           strcmp(error.message, "a return in the home mode is reharmonized") == 0);
}

/* A setting return (R-518) names its pattern: it reads as the section's setting; each refusal at its path. */
static void test_setting_reader(void) {
    static const struct {
        const char *name;
        PlanAccompanimentPattern pattern;
    } PATTERNS[] = {{"block", PLAN_ACCOMPANIMENT_BLOCK},
                    {"arpeggiated", PLAN_ACCOMPANIMENT_ARPEGGIATED},
                    {"alberti", PLAN_ACCOMPANIMENT_ALBERTI},
                    {"oom-pah", PLAN_ACCOMPANIMENT_OOM_PAH}};
    char member[128];
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    for (size_t i = 0; i < sizeof(PATTERNS) / sizeof(PATTERNS[0]); i++) {
        snprintf(member, sizeof(member), ", \"setting\": \"%s\"", PATTERNS[i].name);
        figure_document(text, sizeof(text), ", \"variation\": \"setting\"", member);
        assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].variation == SECTION_RETURN_SETTING &&
               (PlanAccompanimentPattern)form->sizes[0].sections[1].setting == PATTERNS[i].pattern && form->sizes[0].sections[0].setting == 0);
        form_spec_destroy(form);
    }
    /* an unknown setting, a setting return without one, and a setting with any other variation */
    figure_document(text, sizeof(text), ", \"variation\": \"setting\"", ", \"setting\": \"waltz\"");
    refused(text, "/sizes/0/sections/1/setting");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK && strcmp(error.message, "unknown setting \"waltz\"") == 0);
    figure_document(text, sizeof(text), ", \"variation\": \"setting\"", ", \"setting\": \"motive\"");
    refused(text, "/sizes/0/sections/1/setting");
    figure_document(text, sizeof(text), ", \"variation\": \"setting\"", "");
    refused(text, "/sizes/0/sections/1/variation");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK &&
           strcmp(error.message, "a setting return names its setting") == 0);
    figure_document(text, sizeof(text), ", \"variation\": \"exact\"", ", \"setting\": \"block\"");
    refused(text, "/sizes/0/sections/1/setting");
    assert(form_spec_parse(text, &form, &error) != FORM_SPEC_OK &&
           strcmp(error.message, "only a setting return has a setting") == 0);
    figure_document(text, sizeof(text), "", ", \"setting\": \"block\"");
    refused(text, "/sizes/0/sections/1/setting");
}

/* ------------------------------------------------------------- the liquidation section (R-494) */

static void test_liquidation_reader(void) {
    static const char *const SECTION =
        "{\"name\": \"L\", \"letter\": \"L\", \"role\": \"liquidation\", \"bars\": 4, \"cadence\": \"half\"%s}";
    static const struct {
        const char *extra;
        const char *path;
    } REFUSED[] = {{", \"beginning\": \"presentation\"", "/sizes/0/sections/0/beginning"},
                   {", \"ending\": \"continuation\"", "/sizes/0/sections/0/ending"},
                   {", \"construction\": \"auto\"", "/sizes/0/sections/0/construction"},
                   {", \"motive\": \"invert tonal=1\"", "/sizes/0/sections/0/motive"},
                   {", \"pedal\": \"dominant\"", "/sizes/0/sections/0/pedal"},
                   {", \"layers\": \"all\"", "/sizes/0/sections/0/layers"},
                   {", \"leads_in\": true", "/sizes/0/sections/0/leads_in"},
                   {", \"returns\": \"L\"", "/sizes/0/sections/0/returns"},
                   {", \"beats\": 12", "/sizes/0/sections/0/beats"},
                   {", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}", "/sizes/0/sections/0/figure"}};
    char text[4096];
    char sizes[2048];
    char section[1024];
    FormSpec *form = NULL;
    FormSpecError error;
    SectionRole role;
    assert(section_role_find("liquidation", &role) && role == SECTION_ROLE_LIQUIDATION && section_role_is_theme(role));
    assert(strcmp(section_role_name(role), "liquidation") == 0 && strcmp(section_role_label(role), "Liquidation") == 0);

    snprintf(section, sizeof(section), SECTION, "");
    snprintf(sizes, sizeof(sizes), "{\"bars\": 4, \"default\": true, \"sections\": [%s]}", section);
    document(text, sizeof(text), "", sizes);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    const SectionDescription *spec = &form->sizes[0].sections[0];
    assert(spec->role == SECTION_ROLE_LIQUIDATION && spec->bars == 4 && spec->pairing.beginning == 0 && spec->pairing.ending == 0 &&
           spec->returns[0] == '\0' && !spec->has_construction && spec->pedal == 0);
    form_spec_destroy(form);

    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        snprintf(section, sizeof(section), SECTION, REFUSED[i].extra);
        snprintf(sizes, sizeof(sizes), "{\"bars\": 4, \"default\": true, \"sections\": [%s]}", section);
        document(text, sizeof(text), "", sizes);
        refused(text, REFUSED[i].path);
    }
    /* Only 4 bars, and only the half cadence. */
    snprintf(sizes, sizeof(sizes),
             "{\"bars\": 8, \"default\": true, \"sections\": [{\"name\": \"L\", \"letter\": \"L\", \"role\": \"liquidation\", "
             "\"bars\": 8, \"cadence\": \"half\"}]}");
    document(text, sizeof(text), "", sizes);
    refused(text, "/sizes/0/sections/0/bars");
    snprintf(sizes, sizeof(sizes),
             "{\"bars\": 4, \"default\": true, \"sections\": [{\"name\": \"L\", \"letter\": \"L\", \"role\": \"liquidation\", "
             "\"bars\": 4, \"cadence\": \"perfect-authentic\"}]}");
    document(text, sizeof(text), "", sizes);
    refused(text, "/sizes/0/sections/0/cadence");
    /* Only the scherzo has one, the fifth of its nine sections at both sizes. */
    for (size_t f = 0; f < form_spec_builtin_count(); f++) {
        FormSpec *built = builtin(form_spec_builtin_name(f));
        bool scherzo = strcmp(built->name, "scherzo") == 0;
        for (size_t z = 0; z < built->size_count; z++) {
            for (size_t k = 0; k < built->sizes[z].section_count; k++) {
                assert((built->sizes[z].sections[k].role == SECTION_ROLE_LIQUIDATION) == (scherzo && k == 4));
            }
        }
        form_spec_destroy(built);
    }
}

/* ------------------------------------------------------------- the parts of larger forms (R-503) */

/* A document of one size whose one section X is `role` of `bars` bars with `extra` members after them (a leading comma). */
static void part_document(char *out, size_t cap, const char *role, size_t bars, const char *extra) {
    char sizes[2048];
    snprintf(sizes, sizeof(sizes),
             "{\"bars\": %zu, \"default\": true, \"sections\": [{\"name\": \"X\", \"letter\": \"X\", \"role\": \"%s\", "
             "\"bars\": %zu%s}]}",
             bars, role, bars, extra);
    document(out, cap, ", \"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.mediant\"]}], \"modulates\": true", sizes);
}

/* The elaboration role (R-523): read with its label, theme-like, with no rules of its own. */
static void test_elaboration_role(void) {
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    SectionRole role;
    assert(section_role_find("elaboration", &role) && role == SECTION_ROLE_ELABORATION && section_role_is_theme(role));
    assert(strcmp(section_role_name(role), "elaboration") == 0 && strcmp(section_role_label(role), "Elaboration") == 0);
    part_document(text, sizeof(text), "elaboration", 8,
                  ", \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->sizes[0].sections[0].role == SECTION_ROLE_ELABORATION);
    form_spec_destroy(form);
}

/* tests/fixtures/forms/parts-of-forms.json: the four roles read with their members; each refusal of the reader at its path; the
   plan the form builds carries the section function of each part and no archetype on a retransition or a coda. */
static void test_parts_reader(void) {
#define TRANSITION_OK ", \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"half\""
#define SUBORDINATE_OK ", \"beginning\": \"antecedent\", \"ending\": \"consequent\", \"cadence\": \"perfect-authentic\""
    static const struct {
        const char *role;
        const char *label;
    } ROLES_NEW[] = {{"transition", "Transition"},
                     {"retransition", "Retransition"},
                     {"subordinate", "Subordinate theme"},
                     {"coda", "Coda"}};
    static const struct {
        const char *role;
        size_t bars;
        const char *extra;
        const char *path;
    } REFUSED[] = {
        /* a retransition: no beginning or ending, none of the members of a theme, 4 bars, a half cadence */
        {"retransition", 4, ", \"cadence\": \"half\", \"beginning\": \"presentation\"", "/sizes/0/sections/0/beginning"},
        {"retransition", 4, ", \"cadence\": \"half\", \"ending\": \"continuation\"", "/sizes/0/sections/0/ending"},
        {"retransition", 4, ", \"cadence\": \"half\", \"construction\": \"auto\"", "/sizes/0/sections/0/construction"},
        {"retransition", 4, ", \"cadence\": \"half\", \"motive\": \"invert tonal=1\"", "/sizes/0/sections/0/motive"},
        {"retransition", 4, ", \"cadence\": \"half\", \"returns\": \"X\"", "/sizes/0/sections/0/returns"},
        {"retransition", 4, ", \"cadence\": \"half\", \"variation\": \"ornament\"", "/sizes/0/sections/0/variation"},
        {"retransition", 4, ", \"cadence\": \"half\", \"pedal\": \"dominant\"", "/sizes/0/sections/0/pedal"},
        {"retransition", 4, ", \"cadence\": \"half\", \"leads_in\": true", "/sizes/0/sections/0/leads_in"},
        {"retransition", 4, ", \"cadence\": \"half\", \"connective\": \"always\"", "/sizes/0/sections/0/connective"},
        {"retransition", 4, ", \"cadence\": \"half\", \"layers\": \"all\"", "/sizes/0/sections/0/layers"},
        {"retransition", 4, ", \"cadence\": \"half\", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}", "/sizes/0/sections/0/figure"},
        {"retransition", 4, ", \"cadence\": \"half\", \"mode\": \"parallel\"", "/sizes/0/sections/0/mode"},
        {"retransition", 4, ", \"cadence\": \"half\", \"keys\": {\"end\": \"away\"}", "/sizes/0/sections/0/keys"},
        {"retransition", 4, ", \"cadence\": \"half\", \"independent\": true", "/sizes/0/sections/0/independent"},
        {"retransition", 4, ", \"cadence\": \"half\", \"lyric\": true", "/sizes/0/sections/0/lyric"},
        {"retransition", 8, ", \"cadence\": \"half\"", "/sizes/0/sections/0/bars"},
        {"retransition", 4, ", \"cadence\": \"perfect-authentic\"", "/sizes/0/sections/0/cadence"},
        /* a coda: the same, 8 bars, a perfect authentic cadence */
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"beginning\": \"presentation\"", "/sizes/0/sections/0/beginning"},
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"ending\": \"continuation\"", "/sizes/0/sections/0/ending"},
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"construction\": \"auto\"", "/sizes/0/sections/0/construction"},
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"motive\": \"invert tonal=1\"", "/sizes/0/sections/0/motive"},
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"keys\": {\"end\": \"away\"}", "/sizes/0/sections/0/keys"},
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"independent\": true", "/sizes/0/sections/0/independent"},
        {"coda", 8, ", \"cadence\": \"perfect-authentic\", \"lyric\": true", "/sizes/0/sections/0/lyric"},
        {"coda", 4, ", \"cadence\": \"perfect-authentic\"", "/sizes/0/sections/0/bars"},
        {"coda", 8, ", \"cadence\": \"half\"", "/sizes/0/sections/0/cadence"},
        /* a transition: a presentation or a compound basic idea, a continuation, a half cadence */
        {"transition", 8, ", \"beginning\": \"antecedent\", \"ending\": \"continuation\", \"cadence\": \"half\"", "/sizes/0/sections/0/beginning"},
        {"transition", 8, ", \"beginning\": \"presentation\", \"ending\": \"consequent\", \"cadence\": \"half\"", "/sizes/0/sections/0/ending"},
        {"transition", 8, ", \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\"", "/sizes/0/sections/0/cadence"},
        {"transition", 8, TRANSITION_OK ", \"returns\": \"X\"", "/sizes/0/sections/0/returns"},
        {"transition", 8, TRANSITION_OK ", \"variation\": \"ornament\"", "/sizes/0/sections/0/variation"},
        {"transition", 8, TRANSITION_OK ", \"figure\": {\"kind\": \"scale\", \"rhythm\": \"eighths\"}", "/sizes/0/sections/0/figure"},
        {"transition", 8, TRANSITION_OK ", \"mode\": \"parallel\"", "/sizes/0/sections/0/mode"},
        {"transition", 8, TRANSITION_OK ", \"pedal\": \"dominant\"", "/sizes/0/sections/0/pedal"},
        {"transition", 4, TRANSITION_OK ", \"keys\": {\"end\": \"away\"}", "/sizes/0/sections/0/bars"},
        {"transition", 8, TRANSITION_OK ", \"independent\": true, \"motive\": \"invert tonal=1\"", "/sizes/0/sections/0/motive"},
        {"transition", 8, TRANSITION_OK ", \"independent\": 1", "/sizes/0/sections/0/independent"},
        {"transition", 8, TRANSITION_OK ", \"lyric\": true", "/sizes/0/sections/0/lyric"},
        /* only a transition is independent, only a subordinate theme is lyric */
        {"theme", 8, SUBORDINATE_OK ", \"independent\": true", "/sizes/0/sections/0/independent"},
        {"theme", 8, SUBORDINATE_OK ", \"lyric\": true", "/sizes/0/sections/0/lyric"},
        {"subordinate", 8, SUBORDINATE_OK ", \"independent\": true", "/sizes/0/sections/0/independent"},
        /* a lyric theme: no motive, no returns, a period */
        {"subordinate", 8, SUBORDINATE_OK ", \"lyric\": true, \"motive\": \"invert tonal=1\"", "/sizes/0/sections/0/motive"},
        {"subordinate", 8, SUBORDINATE_OK ", \"lyric\": true, \"returns\": \"X\"", "/sizes/0/sections/0/returns"},
        {"subordinate", 8, ", \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"half\", \"lyric\": true", "/sizes/0/sections/0/beginning"},
        {"subordinate", 8, ", \"beginning\": \"antecedent\", \"ending\": \"cadential\", \"cadence\": \"half\", \"lyric\": true", "/sizes/0/sections/0/beginning"},
    };
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;

    /* The roles: a theme-like row each, with its name and label. */
    for (size_t i = 0; i < 4; i++) {
        SectionRole role;
        assert(section_role_find(ROLES_NEW[i].role, &role) && section_role_is_theme(role));
        assert(strcmp(section_role_name(role), ROLES_NEW[i].role) == 0 && strcmp(section_role_label(role), ROLES_NEW[i].label) == 0);
    }
    assert(SECTION_ROLE_FORM_COUNT == 19);

    /* One section of each new role parses with its members set. */
    part_document(text, sizeof(text), "transition", 8, TRANSITION_OK ", \"independent\": true");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->sizes[0].sections[0].role == SECTION_ROLE_TRANSITION && form->sizes[0].sections[0].function.independent &&
           !form->sizes[0].sections[0].function.lyric && !has_motive(&form->sizes[0].sections[0]));
    form_spec_destroy(form);
    part_document(text, sizeof(text), "transition", 4,
                  ", \"beginning\": \"compound_basic_idea\", \"ending\": \"continuation\", \"cadence\": \"half\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && !form->sizes[0].sections[0].function.independent &&
           form->sizes[0].sections[0].pairing.beginning == SECTION_BEGINNING_COMPOUND_BASIC_IDEA);
    form_spec_destroy(form);
    part_document(text, sizeof(text), "transition", 8, TRANSITION_OK ", \"keys\": {\"end\": \"away\"}");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && key_of(form, &form->sizes[0].sections[0].key_end) == 0);
    form_spec_destroy(form);
    part_document(text, sizeof(text), "retransition", 4, ", \"cadence\": \"half\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->sizes[0].sections[0].role == SECTION_ROLE_RETRANSITION && form->sizes[0].sections[0].bars == 4 &&
           form->sizes[0].sections[0].pairing.beginning == 0 && form->sizes[0].sections[0].pairing.ending == 0 &&
           form->sizes[0].sections[0].cadence == CADENCE_TYPE_HALF);
    form_spec_destroy(form);
    part_document(text, sizeof(text), "subordinate", 8, SUBORDINATE_OK ", \"lyric\": true");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->sizes[0].sections[0].role == SECTION_ROLE_SUBORDINATE && form->sizes[0].sections[0].function.lyric &&
           has_motive(&form->sizes[0].sections[0]) && motive_of(&form->sizes[0].sections[0]).kind == MOTIVE_RELATION_AUGMENT &&
           motive_of(&form->sizes[0].sections[0]).value[0] == 0);
    form_spec_destroy(form);
    part_document(text, sizeof(text), "subordinate", 8, ", \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"half\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && !form->sizes[0].sections[0].function.lyric &&
           !has_motive(&form->sizes[0].sections[0]));
    form_spec_destroy(form);
    part_document(text, sizeof(text), "coda", 8, ", \"cadence\": \"perfect-authentic\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->sizes[0].sections[0].role == SECTION_ROLE_CODA && form->sizes[0].sections[0].bars == 8 &&
           form->sizes[0].sections[0].cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    form_spec_destroy(form);

    /* Each refusal at its path. */
    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        part_document(text, sizeof(text), REFUSED[i].role, REFUSED[i].bars, REFUSED[i].extra);
        refused(text, REFUSED[i].path);
    }

    /* The form of the four parts: 48 bars in C major 4/4; T, S, R and K carry their function, R and K have no archetype. */
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/parts-of-forms.json", COMPOSER_FIXTURE_DIR);
    char *fixture = test_must_read_file(path, NULL);
    assert(form_spec_parse(fixture, &form, &error) == FORM_SPEC_OK && form);
    free(fixture);
    assert(strcmp(form->name, "parts-of-forms") == 0 && form->size_count == 1 && form->sizes[0].bars == 48 &&
           form->sizes[0].section_count == 7 && form->modulates);
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    PiecePlan *plan = NULL;
    assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
    static const struct {
        const char *name;
        SectionFunctionKind function;
        bool independent;
        bool lyric;
        bool archetype;
    } PARTS[] = {{"T", SECTION_FUNCTION_TRANSITION, false, false, true},
                 {"S", SECTION_FUNCTION_SUBORDINATE, false, true, true},
                 {"R", SECTION_FUNCTION_RETRANSITION, false, false, false},
                 {"K", SECTION_FUNCTION_CODA, false, false, false}};
    size_t found = 0;
    size_t with_function = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, plan_root(plan)); i++) {
        NodeId child = plan_node_child_at(plan, plan_root(plan), i);
        const char *name = plan_node_name(plan, child);
        const SectionDescription *function = plan_node_kind(plan, child) == PLAN_NODE_SECTION && plan_node_section(plan, child)->has_function
                                                 ? plan_node_section(plan, child)
                                                 : NULL;
        with_function += function != NULL;
        for (size_t p = 0; p < sizeof(PARTS) / sizeof(PARTS[0]); p++) {
            if (plan_node_kind(plan, child) != PLAN_NODE_SECTION || !name || strcmp(name, PARTS[p].name) != 0) {
                continue;
            }
            found++;
            assert(function && function->function.function == PARTS[p].function &&
                   function->function.independent == PARTS[p].independent && function->function.lyric == PARTS[p].lyric);
            assert(plan_node_section(plan, child)->has_pairing == PARTS[p].archetype);
            const PlanConstraint *motive = plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MOTIVE_REF);
            if (PARTS[p].lyric) {
                assert(motive && motive->params.motive.has_step && motive->params.motive.step.kind == MOTIVE_RELATION_AUGMENT &&
                       motive->params.motive.step.value[0] == 0);
            } else {
                assert(!motive);
            }
        }
    }
    assert(found == 4 && with_function == 4);
    plan_destroy(plan);
    form_spec_destroy(form);
#undef TRANSITION_OK
#undef SUBORDINATE_OK
}

/* ------------------------------------------------------------- a section that leads into the next (R-486) */

#define LEAD_SEEDS 10
#define LEAD_COMPOSED_LEAST 9
#define LEAD_WRITTEN_LEAST 6

/* Section `name` with `extra` members after the cadence (with a leading comma, or ""), 8 bars. */
static void lead_section(char *out, size_t cap, const char *name, const char *extra) {
    snprintf(out, cap,
             "{\"name\": \"%s\", \"letter\": \"%s\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"presentation\", "
             "\"ending\": \"continuation\", \"cadence\": \"perfect-authentic\"%s}",
             name, name, extra);
}

/* A document of three 8-bar sections A, B, C with `a`, `b` and `c` the members each has after its cadence. */
static void lead_document(char *out, size_t cap, const char *a, const char *b, const char *c) {
    char sections[3][400];
    char sizes[1400];
    lead_section(sections[0], sizeof(sections[0]), "A", a);
    lead_section(sections[1], sizeof(sections[1]), "B", b);
    lead_section(sections[2], sizeof(sections[2]), "C", c);
    snprintf(sizes, sizeof(sizes), "{\"bars\": 24, \"default\": true, \"sections\": [%s, %s, %s]}", sections[0], sections[1],
             sections[2]);
    document(out, cap, "", sizes);
}

/* The member repeats (R-485): what it reads as, and each refusal with the path of the member at fault. */
static void test_repeats(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/repeat-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    const char *original = "\"repeats\": [[\"A\"], [\"B\", \"A'\"]]";
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form_spec_performed(form) && form->repeat_count == 2);
    assert(form->repeats[0].first == 0 && form->repeats[0].count == 1 && form->repeats[1].first == 1 &&
           form->repeats[1].count == 2);
    form_spec_destroy(form);
    char *plain = replaced_in(text, original, "\"checks\": [\"sections\"]");
    assert(form_spec_parse(plain, &form, &error) == FORM_SPEC_OK && !form_spec_performed(form) && form->repeat_count == 0);
    form_spec_destroy(form);
    free(plain);
    /* Groups may be single, adjacent and cover every section. */
    char *all = replaced_in(text, original, "\"repeats\": [[\"A\", \"B\", \"A'\"]]");
    assert(form_spec_parse(all, &form, &error) == FORM_SPEC_OK && form->repeat_count == 1 && form->repeats[0].count == 3);
    form_spec_destroy(form);
    free(all);
    static const struct {
        const char *repeats;
        const char *path;
    } REFUSED[] = {
        {"\"repeats\": []", "/repeats"},
        {"\"repeats\": \"A\"", "/repeats"},
        {"\"repeats\": [[]]", "/repeats/0"},
        {"\"repeats\": [\"A\"]", "/repeats/0"},
        {"\"repeats\": [[\"A\"], [\"C\"]]", "/repeats/1/0"},
        {"\"repeats\": [[1]]", "/repeats/0/0"},
        {"\"repeats\": [[\"A\", \"A'\"]]", "/repeats/0/1"},
        {"\"repeats\": [[\"B\", \"A\"]]", "/repeats/0/1"},
        {"\"repeats\": [[\"A\", \"B\"], [\"B\"]]", "/repeats/1/0"},
        {"\"repeats\": [[\"B\"], [\"A\"]]", "/repeats/1/0"},
    };
    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        char *document = replaced_in(text, original, REFUSED[i].repeats);
        refused(document, REFUSED[i].path);
        free(document);
    }
    /* A name every size must have: a second size with no A' refuses the group that names it. */
    char *sizes = replaced_in(text, "]}\n  ]", "]},\n    {\"bars\": 8, \"sections\": [\n"
        "      {\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 4, \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\"},\n"
        "      {\"name\": \"B\", \"letter\": \"B\", \"role\": \"contrast\", \"bars\": 4, \"beginning\": \"presentation\", \"ending\": \"continuation\", \"cadence\": \"half\"}\n"
        "    ]}\n  ]");
    refused(sizes, "/repeats/1/1");
    free(sizes);
    /* A form whose sections the request gives has no repeats. */
    refused("{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"x\", \"label\": \"X\", \"sections\": \"request\", "
            "\"repeats\": [[\"A\"]]}",
            "/repeats");
    free(text);
}

/* The member parts (R-485): what it reads as, and each refusal with the path of the member at fault. */
static void test_parts(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/parts-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form_spec_performed(form) && form->part_count == 3);
    assert(form->repeat_count == 0 && form->size_count == 1 && form->sizes[0].bars == 16 && form->sizes[0].section_count == 0);
    assert(strcmp(form->parts[0].name, "First") == 0 && strcmp(form->parts[0].form, "binary") == 0 &&
           form->parts[0].key == FORM_SPEC_HOME && !form->parts[0].new_motive && form->parts[0].again == FORM_SPEC_NO_RETURN &&
           form->parts[0].repeats);
    assert(strcmp(form->parts[1].name, "Second") == 0 && form->parts[1].key == 0 && form->parts[1].new_motive &&
           form->parts[1].repeats);
    assert(strcmp(form->parts[2].name, "First again") == 0 && form->parts[2].form[0] == '\0' && form->parts[2].again == 0 &&
           !form->parts[2].repeats);
    form_spec_destroy(form);
    /* repeats says whether a part's repeats are written out, an again part's too. */
    char *flags = replaced_in(text, "{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"binary\", \"repeats\": false}");
    assert(form_spec_parse(flags, &form, &error) == FORM_SPEC_OK && !form->parts[0].repeats);
    form_spec_destroy(form);
    free(flags);
    flags = replaced_in(text, "\"again\": \"First\"}", "\"again\": \"First\", \"repeats\": true}");
    assert(form_spec_parse(flags, &form, &error) == FORM_SPEC_OK && form->parts[2].repeats);
    form_spec_destroy(form);
    free(flags);
    /* A part of the piece's own motive and the home key may name them. */
    flags = replaced_in(text, "\"motive\": \"new\"", "\"motive\": \"piece\", \"key\": \"other\"");
    refused(flags, "/parts/1/key");
    free(flags);
    flags = replaced_in(text, "\"key\": \"other\", \"motive\": \"new\"", "\"key\": \"home\", \"motive\": \"piece\"");
    assert(form_spec_parse(flags, &form, &error) == FORM_SPEC_OK && form->parts[1].key == FORM_SPEC_HOME && !form->parts[1].new_motive);
    form_spec_destroy(form);
    free(flags);
    static const struct {
        const char *from;
        const char *to;
        const char *path;
    } REFUSED[] = {
        {"\"sizes\": [{\"bars\": 16, \"default\": true}]", "\"sizes\": [{\"bars\": 16, \"default\": true, \"sections\": []}]", "/sizes/0/sections"},
        {"\"sizes\": [{\"bars\": 16, \"default\": true}]", "\"sizes\": [{\"bars\": 16, \"default\": true, \"count\": 2}]", "/sizes/0/count"},
        {"\"sizes\": [{\"bars\": 16, \"default\": true}]", "\"sizes\": [{\"bars\": 16, \"default\": true}, {\"bars\": 16}]", "/sizes/1"},
        {"\"parts\": [", "\"repeats\": [[\"A\"]], \"parts\": [", "/repeats"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"binary\", \"flavor\": 1}", "/parts/0/flavor"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"form\": \"binary\"}", "/parts/0/name"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\"}", "/parts/0/form"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"Binary\"}", "/parts/0/form"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"nothing\"}", "/parts/0/form"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"parts-form\"}", "/parts/0/form"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"game-loop\"}", "/parts/0/form"},
        {"{\"name\": \"First\", \"form\": \"binary\"}", "{\"name\": \"First\", \"form\": \"binary\", \"key\": \"away\"}", "/parts/0/key"},
        {"\"motive\": \"new\"", "\"motive\": \"old\"", "/parts/1/motive"},
        {"\"motive\": \"new\"", "\"motive\": 1", "/parts/1/motive"},
        {"\"name\": \"Second\"", "\"name\": \"First\"", "/parts/1/name"},
        {"\"again\": \"First\"", "\"again\": \"Third\"", "/parts/2/again"},
        {"\"again\": \"First\"", "\"again\": \"First again\"", "/parts/2/again"},
        {"\"again\": \"First\"", "\"again\": 1", "/parts/2/again"},
        {"\"again\": \"First\"", "\"again\": \"First\", \"form\": \"binary\"", "/parts/2/form"},
        {"\"again\": \"First\"", "\"again\": \"First\", \"key\": \"home\"", "/parts/2/key"},
        {"\"again\": \"First\"", "\"again\": \"First\", \"motive\": \"new\"", "/parts/2/motive"},
        {"\"again\": \"First\"}", "\"again\": \"First\"}, {\"name\": \"Third again\", \"again\": \"First again\"}", "/parts/3/again"},
        {"\"again\": \"First\"}", "\"again\": \"First\"}, {\"name\": \"Fourth\", \"again\": \"First\"}, {\"name\": \"Fifth\", \"again\": \"First\"}", "/parts"},
        {"\"bars\": 16", "\"bars\": 100", "/parts/0/form"},
        {"\"bars\": 16", "\"bars\": 0", "/sizes/0/bars"},
    };
    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        char *document = replaced_in(text, REFUSED[i].from, REFUSED[i].to);
        refused(document, REFUSED[i].path);
        free(document);
    }
    refused("{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"x\", \"label\": \"X\", \"parts\": [], \"sizes\": [{\"bars\": 16, \"default\": true}]}",
            "/parts");
    refused("{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"x\", \"label\": \"X\", \"sections\": \"request\", "
            "\"parts\": [{\"name\": \"A\", \"form\": \"binary\"}]}",
            "/parts");
    free(text);
}

/* The part member bars (R-495): the size of the part's form it is composed at, and each refusal with its path. */
static void test_part_bars(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/part-bars-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->part_count == 3 && form->size_count == 2);
    assert(form->sizes[0].bars == 24 && form->sizes[1].bars == 12);
    assert(form->parts[0].bars == 0 && form->parts[1].bars == 12 && form->parts[2].bars == 0);
    form_spec_destroy(form);
    static const struct {
        const char *from;
        const char *to;
        const char *path;
    } REFUSED[] = {
        {"\"again\": \"First\"", "\"again\": \"First\", \"bars\": 12", "/parts/2/bars"},
        {"\"bars\": 12}", "\"bars\": 10}", "/parts/1/bars"},
        {"\"bars\": 12}", "\"bars\": 0}", "/parts/1/bars"},
        {"\"bars\": 12}", "\"bars\": 12.5}", "/parts/1/bars"},
        {"\"bars\": 12}", "\"bars\": \"12\"}", "/parts/1/bars"},
    };
    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        char *document = replaced_in(text, REFUSED[i].from, REFUSED[i].to);
        refused(document, REFUSED[i].path);
        free(document);
    }
    /* A part with bars needs no size of the compound's bars in its form: trio has no size of 100 bars. */
    const char *own =
        "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"x\", \"label\": \"X\", "
        "\"parts\": [{\"name\": \"Trio\", \"form\": \"trio\", \"bars\": 12}], \"sizes\": [{\"bars\": 100, \"default\": true}]}";
    assert(form_spec_parse(own, &form, &error) == FORM_SPEC_OK && form->parts[0].bars == 12 && form->sizes[0].bars == 100);
    form_spec_destroy(form);
    /* Without bars it does: the same compound with no bars is refused at the part's form. */
    char *without = replaced_in(own, ", \"bars\": 12}", "}");
    refused(without, "/parts/0/form");
    free(without);
    free(text);
}

/* The members lead_in and connective: what they read as, and each refusal with the path of the member at fault. */
static void test_lead_ins(void) {
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    lead_document(text, sizeof(text), "", ", \"leads_in\": true, \"connective\": \"always\"", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form);
    assert(!form->sizes[0].sections[0].leads_in && form->sizes[0].sections[0].connective == SECTION_CONNECTIVE_NONE);
    assert(form->sizes[0].sections[1].leads_in && form->sizes[0].sections[1].connective == SECTION_CONNECTIVE_ALWAYS);
    assert(!form->sizes[0].sections[2].leads_in);
    form_spec_destroy(form);
    lead_document(text, sizeof(text), "", ", \"leads_in\": true, \"connective\": \"sometimes\"", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].connective == SECTION_CONNECTIVE_SOMETIMES);
    form_spec_destroy(form);
    lead_document(text, sizeof(text), "", ", \"leads_in\": true", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].leads_in &&
           form->sizes[0].sections[1].connective == SECTION_CONNECTIVE_NONE);
    form_spec_destroy(form);
    lead_document(text, sizeof(text), ", \"leads_in\": true", "", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[0].leads_in);
    form_spec_destroy(form);
    lead_document(text, sizeof(text), "", ", \"leads_in\": false", "");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && !form->sizes[0].sections[1].leads_in);
    form_spec_destroy(form);

    /* The last section of a size has no section to lead into. */
    lead_document(text, sizeof(text), "", "", ", \"leads_in\": true");
    refused(text, "/sizes/0/sections/2/leads_in");
    /* Two in a row: the first is the one refused. */
    lead_document(text, sizeof(text), ", \"leads_in\": true", ", \"leads_in\": true", "");
    refused(text, "/sizes/0/sections/0/leads_in");
    /* A section that returns one. */
    lead_document(text, sizeof(text), "", ", \"returns\": \"A\", \"leads_in\": true", "");
    refused(text, "/sizes/0/sections/1/leads_in");
    /* A section another section returns. */
    lead_document(text, sizeof(text), "", ", \"leads_in\": true", ", \"returns\": \"B\"");
    refused(text, "/sizes/0/sections/1/leads_in");
    /* A connective needs leads_in, and is one of two words. */
    lead_document(text, sizeof(text), "", ", \"connective\": \"always\"", "");
    refused(text, "/sizes/0/sections/1/connective");
    lead_document(text, sizeof(text), "", ", \"leads_in\": false, \"connective\": \"sometimes\"", "");
    refused(text, "/sizes/0/sections/1/connective");
    lead_document(text, sizeof(text), "", ", \"leads_in\": true, \"connective\": \"never\"", "");
    refused(text, "/sizes/0/sections/1/connective");
    lead_document(text, sizeof(text), "", ", \"leads_in\": true, \"connective\": true", "");
    refused(text, "/sizes/0/sections/1/connective");
    lead_document(text, sizeof(text), "", ", \"leads_in\": \"yes\"", "");
    refused(text, "/sizes/0/sections/1/leads_in");

    /* Only the B of the minuet and the trio leads in, with a connective on some seeds (R-486, R-487). */
    for (size_t i = 0; i < form_spec_builtin_count(); i++) {
        FormSpec *built = builtin(form_spec_builtin_name(i));
        bool dance = strcmp(built->name, "minuet") == 0 || strcmp(built->name, "trio") == 0;
        for (size_t s = 0; s < built->size_count; s++) {
            for (size_t k = 0; k < built->sizes[s].section_count; k++) {
                bool lead = dance && k == 1;
                assert(built->sizes[s].sections[k].leads_in == lead &&
                       built->sizes[s].sections[k].connective == (lead ? SECTION_CONNECTIVE_SOMETIMES : SECTION_CONNECTIVE_NONE));
            }
        }
        form_spec_destroy(built);
    }
}

static size_t piece_sections(const PiecePlan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
            sections[count++] = child;
        }
    }
    return count;
}

/* The numeral of the chord sounding at `time`; "" when none is written. */
static const char *chord_at(const PiecePlan *plan, Rational time) {
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    PlanSpan range = {time, {1, 4096}};
    if (plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, &id, 1) == 0) {
        return "";
    }
    return plan_lane_span_payload(plan, id)->harmony.roman_numeral;
}

static Rational add_time(Rational a, Rational b) {
    Rational out;
    assert(rational_add(a, b, &out) == RATIONAL_OK);
    return out;
}

/* The notes of the melody that start in [from, to), as (pitch, onset, duration), in order; returns how many. */
typedef struct {
    int midi;
    Rational onset;
    Rational duration;
} HeardNote;

static size_t melody_notes(const PiecePlan *plan, Rational from, Rational to, HeardNote *out, size_t cap) {
    LaneSpanId ids[16];
    PlanSpan range = {from, {0, 1}};
    assert(rational_sub(to, from, &range.length) == RATIONAL_OK);
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, range, ids, 16);
    size_t count = 0;
    for (size_t s = 0; s < found && s < 16; s++) {
        Rational origin = plan_lane_span_span(plan, ids[s]).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            Rational onset = add_time(origin, notes->notes[i].offset);
            if (rational_cmp(onset, from) >= 0 && rational_cmp(onset, to) < 0 && count < cap) {
                out[count].midi = notes->notes[i].midi;
                out[count].onset = onset;
                out[count].duration = notes->notes[i].duration;
                count++;
            }
        }
    }
    return count;
}

static bool pitch_in_scale(int tonic, bool is_minor, int pitch) {
    static const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR[7] = {0, 2, 3, 5, 7, 8, 11};
    int pitch_class = ((pitch - tonic) % 12 + 12) % 12;
    for (int i = 0; i < 7; i++) {
        if ((is_minor ? MINOR : MAJOR)[i] == pitch_class) {
            return true;
        }
    }
    return false;
}

static PiecePlan *compose_lead(const FormSpec *form, unsigned long seed, int pitch_class, HarmonyMode mode,
                               ComposeOutcome *outcome, PieceResult *ran) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = test_key_mode(pitch_class, mode);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 3;
    request.meter.beat_unit = 4;
    request.seed = seed;
    PiecePlan *plan = NULL;
    assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.form = form;
    *ran = compose_run(plan, &config, outcome);
    return plan;
}

/*
 * One key of the lead-in sweep: seeds 1 to 10 of lead-form.json in 3/4. Each run that composes has B ending on V or V7
 * and A return opening on the tonic; where the connective was written B's last bar keeps its downbeat note (the same
 * seed composed without the connective has it too) as one beat, the rest of the bar is four eighths in the scale of the
 * key, and the last of them is a step from A return's first note. Returns how many composed and how many were written.
 */
static void lead_sweep(const char *text, int pitch_class, HarmonyMode mode, int *composed, int *written) {
    FormSpec *form = NULL;
    FormSpec *plain_form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    char *without = replaced_in(text, ", \"connective\": \"always\"", "");
    assert(form_spec_parse(without, &plain_form, &error) == FORM_SPEC_OK);
    free(without);
    *composed = 0;
    *written = 0;
    bool is_minor = mode == HARMONY_MODE_MINOR;
    for (unsigned long seed = 1; seed <= LEAD_SEEDS; seed++) {
        ComposeOutcome outcome;
        ComposeOutcome plain_outcome;
        PieceResult ran;
        PieceResult plain_ran;
        PiecePlan *plan = compose_lead(form, seed, pitch_class, mode, &outcome, &ran);
        PiecePlan *plain = compose_lead(plain_form, seed, pitch_class, mode, &plain_outcome, &plain_ran);
        if (ran == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            assert(piece_sections(plan, sections) == 3);
            PlanSpan b = plan_node_span(plan, sections[1]);
            Rational b_end = add_time(b.start, b.length);
            Rational tick = {1, 4096};
            Rational last_tick;
            assert(rational_sub(b_end, tick, &last_tick) == RATIONAL_OK);
            const char *last = chord_at(plan, last_tick);
            assert(strcmp(last, "V") == 0 || strcmp(last, "V7") == 0);
            const char *first = chord_at(plan, b_end);
            assert(strcmp(first, "I") == 0 || strcmp(first, "i") == 0);
            /* B is held and asks for the connective; the flag says whether it was written. */
            assert(plan_node_section(plan, sections[1])->leads_in);
            const SectionDescription *constraint = plan_node_section(plan, sections[1]);
            assert(constraint->connective != SECTION_CONNECTIVE_NONE);
            assert(plan_node_section(plan, sections[0])->connective == SECTION_CONNECTIVE_NONE &&
                   plan_node_section(plan, sections[2])->connective == SECTION_CONNECTIVE_NONE);
            if (constraint->connective_written) {
                Rational bar = {3, 4};
                Rational beat = {1, 4};
                Rational eighth = {1, 8};
                Rational bar_start;
                assert(rational_sub(b_end, bar, &bar_start) == RATIONAL_OK);
                HeardNote bar_notes[16];
                size_t count = melody_notes(plan, bar_start, b_end, bar_notes, 16);
                assert(count == 5 && rational_cmp(bar_notes[0].onset, bar_start) == 0 &&
                       rational_cmp(bar_notes[0].duration, beat) == 0);
                /* The same seed without the connective keeps the downbeat note. */
                if (plain_ran == PIECE_OK) {
                    HeardNote plain_notes[16];
                    size_t plain_count = melody_notes(plain, bar_start, b_end, plain_notes, 16);
                    assert(plain_count >= 1 && plain_notes[0].midi == bar_notes[0].midi);
                }
                for (size_t i = 1; i < count; i++) {
                    Rational offset = add_time(bar_start, add_time(beat, (Rational){(long long)(i - 1), 8}));
                    assert(rational_cmp(bar_notes[i].onset, offset) == 0 && rational_cmp(bar_notes[i].duration, eighth) == 0);
                    assert(pitch_in_scale(pitch_class, is_minor, bar_notes[i].midi));
                }
                /* The last note is a step from A return's first note. */
                HeardNote next[4];
                size_t next_count = melody_notes(plan, b_end, add_time(b_end, beat), next, 4);
                assert(next_count >= 1);
                int target = next[0].midi;
                for (size_t i = 1; i < next_count; i++) {
                    target = rational_cmp(next[i].onset, next[0].onset) < 0 ? next[i].midi : target;
                }
                int step = bar_notes[count - 1].midi - target;
                assert(step == 1 || step == 2 || step == -1 || step == -2);
                (*written)++;
            }
            (*composed)++;
        } else {
            printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
        }
        compose_outcome_destroy(&outcome);
        compose_outcome_destroy(&plain_outcome);
        plan_destroy(plan);
        plan_destroy(plain);
    }
    form_spec_destroy(form);
    form_spec_destroy(plain_form);
}

/* A "sometimes" connective is on B for some seeds and not others, and the same seed always gives the same plan. */
static void lead_sometimes(const char *text) {
    char *sometimes = replaced_in(text, "\"always\"", "\"sometimes\"");
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(sometimes, &form, &error) == FORM_SPEC_OK && form->sizes[0].sections[1].connective == SECTION_CONNECTIVE_SOMETIMES);
    free(sometimes);
    int with = 0;
    int without = 0;
    for (unsigned long seed = 1; seed <= LEAD_SEEDS; seed++) {
        bool has[2] = {false, false};
        for (int pass = 0; pass < 2; pass++) {
            PieceRequest request;
            memset(&request, 0, sizeof(request));
            request.form = form->name;
            request.key = test_key(0, "major");
            request.knowledge = test_knowledge();
            request.meter.beats_per_bar = 3;
            request.meter.beat_unit = 4;
            request.seed = seed;
            PiecePlan *plan = NULL;
            assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            assert(piece_sections(plan, sections) == 3);
            assert(plan_node_section(plan, sections[1])->leads_in);
            const SectionDescription *constraint = plan_node_section(plan, sections[1]);
            assert(constraint->connective == SECTION_CONNECTIVE_NONE || !constraint->connective_written);
            has[pass] = constraint->connective != SECTION_CONNECTIVE_NONE;
            plan_destroy(plan);
        }
        assert(has[0] == has[1]);
        with += has[0] ? 1 : 0;
        without += has[0] ? 0 : 1;
    }
    assert(with > 0 && without > 0);
    form_spec_destroy(form);
    printf("lead-form with \"sometimes\": a connective asked for on %d of %d seeds\n", with, LEAD_SEEDS);
}

static int test_leads(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/lead-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    assert(strcmp(form->name, "lead-form") == 0 && form->sizes[0].section_count == 3 && !form->sizes[0].sections[0].leads_in &&
           form->sizes[0].sections[1].leads_in && form->sizes[0].sections[1].connective == SECTION_CONNECTIVE_ALWAYS &&
           !form->sizes[0].sections[2].leads_in);
    form_spec_destroy(form);
    static const struct {
        const char *name;
        int pitch_class;
        HarmonyMode mode;
    } KEYS[] = {{"C major", 0, HARMONY_MODE_MAJOR}, {"A minor", 9, HARMONY_MODE_MINOR}};
    int status = 0;
    for (size_t k = 0; k < sizeof(KEYS) / sizeof(KEYS[0]); k++) {
        int composed;
        int written;
        lead_sweep(text, KEYS[k].pitch_class, KEYS[k].mode, &composed, &written);
        printf("lead-form in %s 3/4: %d of %d seeds compose, connective written in %d\n", KEYS[k].name, composed, LEAD_SEEDS,
               written);
        if (composed < LEAD_COMPOSED_LEAST || written < LEAD_WRITTEN_LEAST) {
            fprintf(stderr, "lead-form in %s: %d composed (at least %d expected), %d written (at least %d expected)\n",
                    KEYS[k].name, composed, LEAD_COMPOSED_LEAST, written, LEAD_WRITTEN_LEAST);
            status = 1;
        }
    }
    lead_sometimes(text);
    free(text);
    return status;
}

/* ------------------------------------------------------------- the compose run of a form that always modulates */

#define MODULATES_SEEDS 10
#define MODULATES_COMPOSED_LEAST 9

/* The key on the key lane at the start of section `section`. */
static AnalysisKey key_at_start(const PiecePlan *plan, NodeId section) {
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    PlanSpan range = {plan_node_span(plan, section).start, {1, 4096}};
    assert(plan_lane_find(plan, PLAN_LANE_KEY, 0, range, &id, 1) == 1);
    return plan_lane_span_payload(plan, id)->key;
}

static int test_modulates(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/modulating-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    free(text);
    assert(strcmp(form->name, "modulating-form") == 0 && form->modulates && form->sizes[0].section_count == 4 &&
           form->sizes[0].sections[1].role == SECTION_ROLE_PATTERN && form->sizes[0].sections[2].role == SECTION_ROLE_SEQUENCE);
    static const struct {
        const char *name;
        int pitch_class;
        HarmonyMode mode;
    } KEYS[] = {{"C major", 0, HARMONY_MODE_MAJOR}, {"A minor", 9, HARMONY_MODE_MINOR}};
    int status = 0;
    for (size_t k = 0; k < sizeof(KEYS) / sizeof(KEYS[0]); k++) {
        int composed = 0;
        for (unsigned long seed = 1; seed <= MODULATES_SEEDS; seed++) {
            ComposeOutcome outcome;
            PieceResult ran;
            /* The request does not ask to modulate (compose_lead leaves it false). */
            PiecePlan *plan = compose_lead(form, seed, KEYS[k].pitch_class, KEYS[k].mode, &outcome, &ran);
            if (ran == PIECE_OK) {
                NodeId sections[FORM_SPEC_MAX_SECTIONS];
                assert(piece_sections(plan, sections) == 4);
                AnalysisKey home = test_key_mode(KEYS[k].pitch_class, KEYS[k].mode);
                AnalysisKey fifth = test_key_mode(KEYS[k].pitch_class + 7, KEYS[k].mode);
                AnalysisKey p = key_at_start(plan, sections[1]);
                AnalysisKey s = key_at_start(plan, sections[2]);
                assert(analysis_key_equals(&p, &fifth));
                assert(analysis_key_equals(&s, &home));
                composed++;
            } else {
                printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
            }
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
        printf("modulating-form in %s 3/4: %d of %d seeds compose, P in the fifth key and S home in each\n", KEYS[k].name,
               composed, MODULATES_SEEDS);
        if (composed < MODULATES_COMPOSED_LEAST) {
            fprintf(stderr, "modulating-form in %s: %d composed, at least %d expected\n", KEYS[k].name, composed,
                    MODULATES_COMPOSED_LEAST);
            status = 1;
        }
    }
    form_spec_destroy(form);
    return status;
}

/* ------------------------------------------------------------- the section rules of the Composer (R-605, R-606) */

/* The text is refused naming `path` with a message that holds `message`. */
static void refused_with(const char *text, const char *path, const char *message) {
    FormSpec *form = NULL;
    FormSpecError error;
    FormSpecResult result = form_spec_parse(text, &form, &error);
    if (result != FORM_SPEC_ERR_INVALID || form != NULL || strcmp(error.path, path) != 0 || strstr(error.message, message) == NULL) {
        fprintf(stderr, "expected a refusal at '%s' with '%s', got result %d at '%s': %s\n%s\n", path, message, (int)result,
                error.path, error.message, text);
        exit(1);
    }
}

/* A size of two sections: the first A, a theme of 8 bars, then `second` (its members, with the name B). */
static void two_sections(char *out, size_t cap, const char *region, const char *second) {
    snprintf(out, cap,
             "{\"schema\": \"ursatz-form\", \"version\": 2, \"name\": \"t\", \"label\": \"T\", \"regions\": [%s], \"sizes\": "
             "[{\"bars\": 16, \"default\": true, \"sections\": ["
             "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"antecedent\", "
             "\"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"closed\": true}, "
             "{\"name\": \"B\", \"letter\": \"B\", %s}]}]}",
             region, second);
}

#define AWAY "{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.mediant\"]}"
#define PAIR_PRESENTATION "\"beginning\": \"presentation\", \"ending\": \"continuation\""
#define THEME_B "\"role\": \"theme\", \"bars\": 8, " PAIR_PRESENTATION ", \"cadence\": \"half\""

/* Regions list relations; a section is read by the library codec, then checked here; each refusal names its path and says why. */
static void test_section_rules(void) {
    FormSpec *form = NULL;
    FormSpecError error;
    char text[4096];
    /* the regions: relation ids of the snapshot */
    two_sections(text, sizeof(text), AWAY, THEME_B);
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->region_count == 1 && strcmp(form->regions[0].major[0], "omt.relation.dominant") == 0 &&
           strcmp(form->regions[0].minor[0], "omt.relation.mediant") == 0);
    form_spec_destroy(form);
    two_sections(text, sizeof(text), "{\"name\": \"away\", \"major\": [5], \"minor\": [\"omt.relation.mediant\"]}", THEME_B);
    refused_with(text, "/regions/0/major/0", "must be a relation id");
    two_sections(text, sizeof(text), "{\"name\": \"away\", \"major\": [], \"minor\": [\"omt.relation.mediant\"]}", THEME_B);
    refused_with(text, "/regions/0/major", "must hold 1 to 8 relations");
    /* a cadence abbreviation reads as its kind's name */
    two_sections(text, sizeof(text), "", "\"role\": \"theme\", \"bars\": 8, " PAIR_PRESENTATION ", \"cadence\": \"HC\"");
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK);
    assert(form->sizes[0].sections[1].cadence == CADENCE_TYPE_HALF && strcmp(form->sizes[0].sections[1].cadence_name, "half") == 0);
    form_spec_destroy(form);

    /* the library's rules, through the codec, at their path under the section */
    two_sections(text, sizeof(text), "", THEME_B ", \"returns\": \"A\", \"variation\": \"setting\"");
    refused_with(text, "/sizes/0/sections/1/variation", "a setting return names its setting");
    two_sections(text, sizeof(text), "", THEME_B ", \"nonsense\": 1");
    refused_with(text, "/sizes/0/sections/1/nonsense", "unknown member");
    two_sections(text, sizeof(text), "", THEME_B ", \"harmony\": {\"schema\": \"x\"}");
    refused_with(text, "/sizes/0/sections/1/harmony", "is not given here");

    /* the rules of the Composer */
    two_sections(text, sizeof(text), "", THEME_B);
    char *repeated = replaced_in(text, "{\"name\": \"B\", \"letter\": \"B\"", "{\"name\": \"A\", \"letter\": \"B\"");
    refused_with(repeated, "/sizes/0/sections/1/name", "section name \"A\" repeated in the size");
    free(repeated);
    two_sections(text, sizeof(text), "", THEME_B ", \"name\": \"A\"");
    refused_with(text, "/sizes/0/sections/1/name", "member given twice");
    two_sections(text, sizeof(text), AWAY, "\"role\": \"theme\", \"bars\": 8, " PAIR_PRESENTATION ", \"cadence\": \"half\", \"keys\": {\"end\": \"nowhere\"}");
    refused_with(text, "/sizes/0/sections/1/keys/end", "unknown region \"nowhere\"");
    two_sections(text, sizeof(text), "", THEME_B ", \"returns\": \"Z\"");
    refused_with(text, "/sizes/0/sections/1/returns", "\"Z\" is not an earlier section of the size");
    two_sections(text, sizeof(text), "", THEME_B ", \"returns\": \"B\"");
    refused_with(text, "/sizes/0/sections/1/returns", "\"B\" is not an earlier section of the size");
    two_sections(text, sizeof(text), "", THEME_B ", \"returns\": \"A\", \"construction\": \"auto\"");
    refused_with(text, "/sizes/0/sections/1/construction", "a return keeps the construction of A");
    two_sections(text, sizeof(text), "", THEME_B ", \"motive\": \"nonsense\"");
    refused_with(text, "/sizes/0/sections/1/motive", "not a motive relation: \"nonsense\"");
    two_sections(text, sizeof(text), "", "\"role\": \"liquidation\", \"bars\": 8, \"cadence\": \"half\"");
    refused_with(text, "/sizes/0/sections/1/bars", "a liquidation is 4 bars");
    two_sections(text, sizeof(text), "", "\"role\": \"retransition\", \"bars\": 8, \"cadence\": \"half\"");
    refused_with(text, "/sizes/0/sections/1/bars", "a retransition is 4 bars");
    two_sections(text, sizeof(text), "", "\"role\": \"coda\", \"bars\": 4, \"cadence\": \"perfect-authentic\"");
    refused_with(text, "/sizes/0/sections/1/bars", "a coda is 8 bars");
    two_sections(text, sizeof(text), "", "\"role\": \"liquidation\", \"bars\": 4, \"cadence\": \"perfect-authentic\"");
    refused_with(text, "/sizes/0/sections/1/cadence", "a liquidation ends in a half cadence");
    two_sections(text, sizeof(text), "", "\"role\": \"retransition\", \"bars\": 4, \"cadence\": \"perfect-authentic\"");
    refused_with(text, "/sizes/0/sections/1/cadence", "a retransition ends in a half cadence");
    two_sections(text, sizeof(text), "", "\"role\": \"transition\", \"bars\": 4, " PAIR_PRESENTATION ", \"cadence\": \"perfect-authentic\"");
    refused_with(text, "/sizes/0/sections/1/cadence", "a transition ends in a half cadence");
    two_sections(text, sizeof(text), "", "\"role\": \"coda\", \"bars\": 8, \"cadence\": \"half\"");
    refused_with(text, "/sizes/0/sections/1/cadence", "a coda ends in a perfect authentic cadence");
    two_sections(text, sizeof(text), "", "\"role\": \"theme\", \"bars\": 8, " PAIR_PRESENTATION ", \"cadence\": \"deceptive\"");
    refused_with(text, "/sizes/0/sections/1/cadence", "a section ends in a perfect authentic, imperfect authentic or half cadence");
    two_sections(text, sizeof(text), "", "\"role\": \"transition\", \"bars\": 4, \"beginning\": \"antecedent\", \"ending\": \"continuation\", \"cadence\": \"half\"");
    refused_with(text, "/sizes/0/sections/1/beginning", "a transition begins with a presentation or a compound basic idea");
    two_sections(text, sizeof(text), "", "\"role\": \"transition\", \"bars\": 4, \"beginning\": \"presentation\", \"ending\": \"consequent\", \"cadence\": \"half\"");
    refused_with(text, "/sizes/0/sections/1/ending", "a transition ends with a continuation");
    two_sections(text, sizeof(text), AWAY, "\"role\": \"transition\", \"bars\": 4, " PAIR_PRESENTATION ", \"cadence\": \"half\", \"keys\": {\"end\": \"away\"}");
    refused_with(text, "/sizes/0/sections/1/bars", "a transition that modulates is 8 bars");
    two_sections(text, sizeof(text), "", "\"role\": \"theme\", \"bars\": 6, " PAIR_PRESENTATION ", \"cadence\": \"half\"");
    refused_with(text, "/sizes/0/sections/1/bars", "a theme-like section of 6 bars needs a construction; only 4 or 8 bars have none");
    two_sections(text, sizeof(text), "", "\"role\": \"theme\", \"bars\": 20, " PAIR_PRESENTATION ", \"cadence\": \"half\", \"construction\": \"auto\"");
    refused_with(text, "/sizes/0/sections/1/bars", "a section with a construction is 3 to 16 bars");
    two_sections(text, sizeof(text), "", "\"role\": \"theme\", \"bars\": 6, " PAIR_PRESENTATION ", \"cadence\": \"half\", \"construction\": {\"ideas\": {\"basic idea\": 2}}");
    refused_with(text, "/sizes/0/sections/1/construction", "the construction adds up to 2 bars but the section is 6");
    two_sections(text, sizeof(text), "", "\"role\": \"theme\", \"bars\": 8, " PAIR_PRESENTATION ", \"cadence\": \"half\", \"leads_in\": true");
    /* a section that leads in is the last of the size: refused with the size's rules */
    refused_with(text, "/sizes/0/sections/1/leads_in", "the last section of a size has no section to lead into");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "modulates") == 0) {
        return test_modulates();
    }
    if (argc == 2 && strcmp(argv[1], "leads") == 0) {
        return test_leads();
    }
    if (argc == 2 && strcmp(argv[1], "pedal") == 0) {
        return test_pedal_form();
    }
    if (argc == 2 && strcmp(argv[1], "custom") == 0) {
        return test_custom();
    }
    if (argc == 2 && strcmp(argv[1], "meters") == 0) {
        return test_meters();
    }
    if (argc != 1) {
        fprintf(stderr, "usage: test_form_spec [custom|leads|meters|modulates|pedal]\n");
        return 2;
    }
    test_builtin_forms();
    test_builtin_details();
    test_figured_variations();
    test_main_and_subordinate();
    test_for_plan();
    test_refusals();
    test_game_loop();
    test_default_cadences();
    test_meters_and_tempo();
    test_sizes();
    test_pedal();
    test_lead_ins();
    test_repeats();
    test_parts();
    test_part_bars();
    test_construction();
    test_modulating_reader();
    test_liquidation_reader();
    test_figure_reader();
    test_tonic_major_reader();
    test_home_mode_reader();
    test_setting_reader();
    test_parts_reader();
    test_elaboration_role();
    test_section_rules();
    printf("form documents: %zu built-in forms and the game loop, the reader's refusals and the sizes are as docs/forms.md says\n", FORM_COUNT);
    return 0;
}
