#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece.h>

/*
 * The form documents (R-416, docs/forms.md).
 *
 *   test_form_spec            the reader and the built-in forms (fast):
 *                               every built-in parses and is what docs/forms.md says of it;
 *                               each refusal of the reader, with the path of the member at fault;
 *                               form_spec_size for every size, count and bad size of each built-in
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
    assert(form_spec_builtin_count() == FORM_COUNT);
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
                sum += section->bars;
            }
            assert(sum == size->bars);
        }
        form_spec_destroy(form);
    }
    assert(form_spec_builtin_name(FORM_COUNT) == NULL && form_spec_builtin_document(FORM_COUNT) == NULL);
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
    assert(form->regions[0].major_count == 3 && form->regions[0].major[0] == 4 && form->regions[0].major[1] == 6 &&
           form->regions[0].major[2] == 5 && form->regions[0].minor_count == 3 && form->regions[0].minor[0] == 3 &&
           form->regions[0].minor[1] == 4 && form->regions[0].minor[2] == 6);
    const FormSpecSize *ternary = form_spec_size(form, 24, 0);
    assert(ternary->sections[0].closed && !ternary->sections[1].closed && !ternary->sections[2].closed);
    assert(ternary->sections[1].key_start == FORM_SPEC_HOME && ternary->sections[1].key_end == 0);
    assert(strcmp(ternary->sections[2].name, "A return") == 0 && strcmp(ternary->sections[2].letter, "A") == 0 &&
           ternary->sections[0].pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
           ternary->sections[0].pairing.ending == PLAN_THEME_ENDING_CONSEQUENT);
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
    snprintf(text, sizeof(text), "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"t\", \"label\": \"T\", \"sections\": \"request\"}");
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

int main(void) {
    test_builtin_forms();
    test_builtin_details();
    test_for_plan();
    test_refusals();
    test_sizes();
    printf("form documents: %zu built-in forms, the reader's refusals and the sizes are as docs/forms.md says\n", FORM_COUNT);
    return 0;
}
