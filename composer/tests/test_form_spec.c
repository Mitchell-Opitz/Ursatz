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

/*
 * The form documents (R-416, docs/forms.md).
 *
 *   test_form_spec            the reader and the built-in forms (fast):
 *                               every built-in parses and is what docs/forms.md says of it;
 *                               a section's construction (B6 of P47): "auto", the ideas and extensions, the refusals;
 *                               each refusal of the reader, with the path of the member at fault;
 *                               form_spec_size for every size, count and bad size of each built-in
 *   test_form_spec pedal      a form with a dominant pedal in its B (tests/fixtures/forms/pedal-form.json) composes: seeds
 *                             1 to 10 in C major 4/4, at least 9 compose, and in each the chords and the bass of B's
 *                             first half are all on the dominant
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
    snprintf(out, cap, "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"t\", \"label\": \"T\"%s, \"sizes\": [%s]}", top,
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
} FormRow;

/* Written out here so the test does not read it from the documents it checks. */
static const FormRow FORMS[] = {
    {"binary", true, false, 1, {{8, 0, false}, {16, 0, true}}, 2,
     {{4, 0, false}, {12, 0, false}, {20, 0, false}, {32, 0, false}, {16, 2, false}, {0, 1, false}}, 6},
    {"rounded-binary", true, false, 1, {{16, 0, true}, {24, 0, false}}, 2,
     {{8, 0, false}, {12, 0, false}, {32, 0, false}, {24, 1, false}}, 4},
    {"ternary", true, false, 1, {{12, 0, false}, {24, 0, true}}, 2,
     {{8, 0, false}, {16, 0, false}, {36, 0, false}, {0, 3, false}}, 4},
    {"variations", true, false, 1,
     {{16, 1, false}, {24, 2, false}, {32, 3, true}, {40, 4, false}, {48, 5, false}, {56, 6, false}, {64, 7, false}, {72, 8, false}},
     8, {{8, 0, false}, {12, 0, false}, {80, 0, false}, {0, 9, false}, {24, 3, false}, {20, 0, false}}, 6},
    {"loop", false, false, 0, {{8, 0, true}, {16, 0, false}}, 2,
     {{4, 0, false}, {12, 0, false}, {24, 0, false}, {8, 1, false}}, 4},
    {"layered", false, true, 0, {{16, 2, false}, {24, 3, false}, {32, 4, true}}, 3,
     {{8, 0, false}, {40, 0, false}, {0, 1, false}, {0, 5, false}, {24, 2, false}, {12, 0, false}}, 6},
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
        for (size_t z = 0; z < row->size_count; z++) {
            const FormSpecSize *size = &form->sizes[z];
            size_t sum = 0;
            assert(size->bars == row->sizes[z].bars && size->count == row->sizes[z].count &&
                   size->is_default == row->sizes[z].is_default);
            for (size_t i = 0; i < size->section_count; i++) {
                const FormSectionSpec *section = &size->sections[i];
                assert(form_role_is_theme(section->role) && section->bars > 0 && section->beats == 0);
                assert(section->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || section->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
                       section->cadence == CADENCE_TYPE_HALF);
                assert(section->returns == FORM_SPEC_NO_RETURN || section->returns < i);
                /* Only the ternary's B asks for a pedal (R-446, R-445). */
                assert(section->pedal == (strcmp(row->name, "ternary") == 0 && i == 1 ? 5 : 0));
                sum += section->bars;
            }
            assert(sum == size->bars);
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
    assert(strcmp(form->regions[0].name, "away") == 0 && form->regions[0].major_count == 1 && form->regions[0].major[0] == 5 &&
           form->regions[0].minor_count == 1 && form->regions[0].minor[0] == 3);
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *size = &form->sizes[z];
        assert(size->sections[0].key_start == FORM_SPEC_HOME && size->sections[0].key_end == 0);
        assert(size->sections[1].key_start == 0 && size->sections[1].key_end == FORM_SPEC_HOME);
        assert(!size->sections[0].closed && !size->sections[1].closed);
        /* B is m0 inverted in the key's steps: tonal, axis the default. */
        assert(!size->sections[0].has_motive && size->sections[1].has_motive);
        assert(size->sections[1].motive.kind == MOTIVE_RELATION_INVERT && size->sections[1].motive.value[0] == -1 &&
               size->sections[1].motive.value[1] == 1);
    }
    form_spec_destroy(form);

    form = builtin("rounded-binary");
    const FormSpecSize *rounded = form_spec_size(form, 16, 0);
    assert(rounded && rounded->section_count == 3 && rounded->sections[0].closed && !rounded->sections[1].closed &&
           !rounded->sections[2].closed);
    assert(rounded->sections[1].cadence == CADENCE_TYPE_HALF && strcmp(rounded->sections[1].cadence_name, "half") == 0);
    assert(rounded->sections[1].bars == 4 && rounded->sections[2].bars == 4 && form_spec_size(form, 24, 0)->sections[1].bars == 8);
    assert(rounded->sections[2].returns == 0 && rounded->sections[2].variation == PLAN_RETURN_EXACT &&
           strcmp(rounded->sections[2].letter, "A'") == 0 &&
           rounded->sections[1].pairing.beginning == PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA);
    form_spec_destroy(form);

    form = builtin("ternary");
    assert(form->regions[0].major_count == 2 && form->regions[0].major[0] == 5 && form->regions[0].major[1] == 3 &&
           form->regions[0].minor_count == 2 && form->regions[0].minor[0] == 3 && form->regions[0].minor[1] == 5);
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *ternary = &form->sizes[z];
        assert(ternary->sections[0].closed && !ternary->sections[1].closed && !ternary->sections[2].closed);
        /* A ends in the away region, B starts there and returns home, ending on V (a half cadence) over a pedal. */
        assert(ternary->sections[0].key_start == FORM_SPEC_HOME && ternary->sections[0].key_end == 0);
        assert(ternary->sections[1].key_start == 0 && ternary->sections[1].key_end == FORM_SPEC_HOME);
        assert(ternary->sections[1].cadence == CADENCE_TYPE_HALF && ternary->sections[1].pedal == 5);
        assert(ternary->sections[2].variation == PLAN_RETURN_VARIED && ternary->sections[2].returns == 0);
        assert(strcmp(ternary->sections[2].name, "A return") == 0 && strcmp(ternary->sections[2].letter, "A") == 0 &&
               ternary->sections[0].pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
               ternary->sections[0].pairing.ending == PLAN_THEME_ENDING_CONSEQUENT);
    }
    form_spec_destroy(form);

    /* Variations: the theme is index 0; with three or more the section at (count + 1) / 2 is wholly away. */
    form = builtin("variations");
    assert(form->regions[0].major_count == 1 && form->regions[0].major[0] == 6 && form->regions[0].minor[0] == 3);
    for (size_t count = 1; count <= 8; count++) {
        const FormSpecSize *size = form_spec_size(form, 0, count);
        assert(size && size->section_count == count + 1 && strcmp(size->sections[0].name, "theme") == 0);
        for (size_t i = 0; i < size->section_count; i++) {
            const FormSectionSpec *section = &size->sections[i];
            bool away = count >= 3 && i == (count + 1) / 2;
            assert(section->closed && section->key_start == (away ? 0 : FORM_SPEC_HOME) &&
                   section->key_end == (away ? 0 : FORM_SPEC_HOME));
            if (i > 0) {
                bool odd = i % 2 == 1;
                assert(section->returns == 0 && section->variation == (odd ? PLAN_RETURN_ORNAMENT : PLAN_RETURN_REHARMONIZE));
                assert(section->has_motive &&
                       section->motive.kind == (odd ? MOTIVE_RELATION_DIMINISH : MOTIVE_RELATION_INTERVAL_CHANGE));
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
    assert(layered->sections[0].returns == FORM_SPEC_NO_RETURN && layered->sections[3].returns == 0);
    assert(form_spec_size(form, 0, 2)->sections[0].layers == bass && form_spec_size(form, 0, 2)->sections[1].layers == PLAN_LAYERS_ALL);
    form_spec_destroy(form);

    /* Loop: the first section ends in the half cadence that leads back; the 16-bar one opens with an IAC. */
    form = builtin("loop");
    assert(form_spec_size(form, 8, 0)->sections[0].cadence == CADENCE_TYPE_HALF);
    assert(form_spec_size(form, 16, 0)->sections[0].cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
           form_spec_size(form, 16, 0)->sections[1].cadence == CADENCE_TYPE_HALF);
    form_spec_destroy(form);
}

/* A plan made from each built-in is named for the form, and the form is found again from it. */
static void test_for_plan(void) {
    for (size_t f = 0; f < FORM_COUNT; f++) {
        PieceRequest request;
        memset(&request, 0, sizeof(request));
        request.form = FORMS[f].name;
        request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
        request.meter.beats_per_bar = 4;
        request.meter.beat_unit = 4;
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        FormSpec *found = NULL;
        assert(form_spec_builtin_for_plan(plan, &found) == FORM_SPEC_OK && strcmp(found->name, FORMS[f].name) == 0);
        form_spec_destroy(found);
        plan_destroy(plan);
    }
    PiecePlan *plan = plan_create();
    FormSpec *found = NULL;
    assert(form_spec_builtin_for_plan(plan, &found) == FORM_SPEC_ERR_UNKNOWN && found == NULL);
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
    refused_edit("rounded-binary", "\"version\": 1", "\"version\": 2", "/version");
    refused_edit("rounded-binary", "\"version\": 1", "\"version\": \"1\"", "/version");
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
    refused_edit("rounded-binary", "\"major\": [5]", "\"major\": []", "/regions/0/major");
    refused_edit("rounded-binary", "\"major\": [5]", "\"major\": [8]", "/regions/0/major/0");
    refused_edit("rounded-binary", "\"minor\": [3]", "\"minor\": [3.5]", "/regions/0/minor/0");
    refused_edit("rounded-binary", "\"major\": [5], \"minor\": [3]", "\"major\": [5]", "/regions/0/minor");
    refused_edit("rounded-binary", "\"regions\": [{\"name\": \"away\", \"major\": [5], \"minor\": [3]}]",
                 "\"regions\": [{\"name\": \"away\", \"major\": [5], \"minor\": [3]}, {\"name\": \"away\", \"major\": [4], \"minor\": [4]}]",
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
    refused_edit("rounded-binary", "\"closed\": true,", "\"closed\": true, \"variation\": \"exact\",", "/sizes/0/sections/0/variation");

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
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
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
        snprintf(region, sizeof(region), "%s{\"name\": \"r%d\", \"major\": [5], \"minor\": [3]}", i ? ", " : "", i);
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
    free(big);
    free(list);
}

/* ------------------------------------------------------------- the game loop */

/* A form whose sections the request gives, with the members the test varies (roles, loop roles, cadences). */
static void loop_document(char *out, size_t cap, const char *roles, const char *loop, const char *cadences, const char *rest) {
    snprintf(out, cap,
             "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
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
    assert(form->role_count == 4 && form->roles[0] == FORM_ROLE_INTRO && form->roles[1] == FORM_ROLE_BODY &&
           form->roles[2] == FORM_ROLE_TURNAROUND && form->roles[3] == FORM_ROLE_ENDING);
    assert(form->loop_role_count == 2 && form->loop_roles[0] == FORM_ROLE_BODY && form->loop_roles[1] == FORM_ROLE_TURNAROUND);
    assert(form_spec_role_in_loop(form, FORM_ROLE_BODY) && !form_spec_role_in_loop(form, FORM_ROLE_INTRO) &&
           form_spec_has_role(form, FORM_ROLE_ENDING) && !form_spec_has_role(form, FORM_ROLE_THEME));
    assert(form->default_cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
           strcmp(form->default_cadence_name, "imperfect-authentic") == 0 && form->last_cadence_count == 3);
    assert(form->last_cadences[0].role_count == 1 && form->last_cadences[0].roles[0] == FORM_ROLE_ENDING &&
           form->last_cadences[0].cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(form->last_cadences[1].role_count == 2 && form->last_cadences[1].roles[0] == FORM_ROLE_BODY &&
           form->last_cadences[1].roles[1] == FORM_ROLE_TURNAROUND && form->last_cadences[1].cadence == CADENCE_TYPE_HALF);
    assert(form->last_cadences[2].role_count == 1 && form->last_cadences[2].roles[0] == FORM_ROLE_INTRO &&
           form->last_cadences[2].cadence == CADENCE_TYPE_HALF);
    assert(form->has_default_ending && strcmp(form->default_ending.id, "end") == 0 &&
           strcmp(form->default_ending.letter, "E") == 0 && form->default_ending.role == FORM_ROLE_ENDING &&
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
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
             "\"roles\": " ALL_ROLES ", \"cadences\": " CADENCES "}");
    refused(text, "/loop");
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\", "
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

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(2);
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)length + 1);
    size_t read = text ? fread(text, 1, (size_t)length, file) : 0;
    assert(text && read == (size_t)length);
    text[length] = '\0';
    fclose(file);
    return text;
}

/* Composes the form's piece with the seed; the plan is returned (the caller destroys it), the result in *ran. */
static PiecePlan *compose_custom(const FormSpec *form, unsigned long seed, ComposeOutcome *outcome, PieceResult *ran) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    PiecePlan *plan = NULL;
    PieceResult built = form_build_plan(form, &request, &plan);
    assert(built == PIECE_OK && plan);
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
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
    char *text = read_file(path);
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
                if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
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
    char *again_text = read_file(path);
    FormSpecResult parsed = form_spec_parse(again_text, &sections_only, NULL);
    assert(parsed == FORM_SPEC_OK);
    free(again_text);
    sections_only->checks = FORM_CHECK_SECTIONS;
    ComposeOutcome outcome;
    PieceResult ran;
    PiecePlan *plan = compose_custom(sections_only, first, &outcome, &ran);
    assert(ran == PIECE_OK);
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
    char *text = read_file(path);
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
                if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
                    sections[count++] = child;
                }
            }
            assert(count == 2 && outcome.section_count == 2);
            for (size_t i = 0; i < 2; i++) {
                const PlanConstraint *construction = plan_node_find_constraint(plan, sections[i], PLAN_CONSTRAINT_CONSTRUCTION);
                assert(construction && construction->params.construction.reason == PLAN_CONSTRUCTION_GIVEN);
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
    const FormSectionSpec *section = &form->sizes[0].sections[0];
    assert(section->has_construction && !section->construction_auto && section->bars == 6 &&
           section->construction.reason == PLAN_CONSTRUCTION_GIVEN && section->construction.extension_count == 0 &&
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
    assert(section->construction.extension_count == 1 && section->construction.extensions[0].kind == PLAN_EXTENSION_REPEAT &&
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
    assert(harmony_read_lane(plan, half, KEY_STRUCTURE_MODE_MAJOR, chords, &read, &metre) && read >= 1 && read <= 16);
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
    char *text = read_file(path);
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
                if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
                    sections[count++] = child;
                }
            }
            assert(count == 3 && plan_node_find_constraint(plan, sections[1], PLAN_CONSTRAINT_PEDAL));
            assert(!plan_node_find_constraint(plan, sections[0], PLAN_CONSTRAINT_PEDAL));
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

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "pedal") == 0) {
        return test_pedal_form();
    }
    if (argc == 2 && strcmp(argv[1], "custom") == 0) {
        return test_custom();
    }
    if (argc != 1) {
        fprintf(stderr, "usage: test_form_spec [custom|pedal]\n");
        return 2;
    }
    test_builtin_forms();
    test_builtin_details();
    test_for_plan();
    test_refusals();
    test_game_loop();
    test_default_cadences();
    test_sizes();
    test_pedal();
    test_construction();
    printf("form documents: %zu built-in forms and the game loop, the reader's refusals and the sizes are as docs/forms.md says\n", FORM_COUNT);
    return 0;
}
