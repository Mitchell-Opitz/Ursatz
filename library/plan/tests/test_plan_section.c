#include <stdio.h>
#include <string.h>

#include "plan/section.h"
#include "plan/section_json.h"
#include "ursatz_test.h"

/*
 * The section description: its vocabulary, each member read and written for the uses that hold it and refused for the others,
 * a round trip of descriptions per use that together set every member, every rule of section_description_check refused with
 * its message and path, and the member table against the schema fragment. The documents written go to the build directory for
 * plan_section_schema.
 */

static const char *const USE_NAMES[] = {"form", "plan", "request", "overlay"};
static const SectionUse USES[] = {SECTION_USE_FORM, SECTION_USE_PLAN, SECTION_USE_REQUEST, SECTION_USE_OVERLAY};
#define USE_COUNT 4

static size_t use_index(SectionUse use) {
    for (size_t i = 0; i < USE_COUNT; i++) {
        if (USES[i] == use) {
            return i;
        }
    }
    return 0;
}

/* ------------------------------------------------------------- the vocabulary */

static void test_vocabulary(void) {
    int theme_like = 0;
    int operators = 0;
    SectionRole role;
    for (int r = 0; r < SECTION_ROLE_COUNT; r++) {
        CHECK(section_role_name((SectionRole)r) != NULL && section_role_label((SectionRole)r) != NULL);
        CHECK(section_role_find(section_role_name((SectionRole)r), &role) && role == (SectionRole)r);
        theme_like += section_role_is_theme((SectionRole)r) ? 1 : 0;
        operators += section_role_is_operator((SectionRole)r) ? 1 : 0;
    }
    CHECK(section_role_name((SectionRole)SECTION_ROLE_COUNT) == NULL && section_role_name((SectionRole)-1) == NULL);
    CHECK(!section_role_find("nonsense", &role) && !section_role_find(NULL, &role));
    CHECK(theme_like == 15 && operators == 3);
    CHECK(SECTION_ROLE_FORM_COUNT == 19 && SECTION_ROLE_COUNT == 21);
    CHECK(strcmp(section_role_name(SECTION_ROLE_BODY), "body") == 0 && strcmp(section_role_label(SECTION_ROLE_BODY), "Loop body") == 0);
    CHECK(strcmp(section_role_label(SECTION_ROLE_SUBORDINATE), "Subordinate theme") == 0);
    CHECK(strcmp(section_role_name(SECTION_ROLE_MATERIAL), "material") == 0 && strcmp(section_role_label(SECTION_ROLE_SEAM), "Seam") == 0);

    SectionBeginning beginning;
    SectionEnding ending;
    SectionIdea idea;
    SectionExtensionKind extension;
    SectionConstructionReason reason;
    SectionReturnVariation variation;
    SectionSetting setting;
    SectionModeShift mode;
    SectionConnective connective;
    SectionFigureKind kind;
    SectionFigureRhythm rhythm;
    SectionFunctionKind function;
    SectionLane lane;
    CHECK(section_beginning_find("compound_basic_idea", &beginning) && beginning == SECTION_BEGINNING_COMPOUND_BASIC_IDEA);
    CHECK(section_ending_find("cadential", &ending) && ending == SECTION_ENDING_CADENTIAL);
    CHECK(section_idea_find("cadential 2", &idea) && idea == SECTION_IDEA_CADENTIAL_2);
    CHECK(strcmp(section_idea_name(SECTION_IDEA_CONSEQUENT_CONTRASTING), "consequent contrasting idea") == 0);
    CHECK(section_extension_kind_find("evaded", &extension) && extension == SECTION_EXTENSION_EVADED);
    CHECK(section_construction_reason_find("length", &reason) && reason == SECTION_CONSTRUCTION_FROM_LENGTH);
    CHECK(section_return_variation_find("setting", &variation) && variation == SECTION_RETURN_SETTING);
    CHECK(strcmp(section_return_variation_name(SECTION_RETURN_FIGURATE), "figurate") == 0);
    CHECK(section_setting_find("oom-pah", &setting) && setting == SECTION_SETTING_OOM_PAH);
    CHECK(section_setting_name(SECTION_SETTING_NONE) == NULL && !section_setting_find("none", &setting));
    CHECK(section_mode_find("major", &mode) && mode == SECTION_MODE_TONIC_MAJOR && section_mode_name(SECTION_MODE_NONE) == NULL);
    CHECK(section_connective_find("always", &connective) && connective == SECTION_CONNECTIVE_ALWAYS);
    CHECK(section_figure_kind_find("broken-chord", &kind) && kind == SECTION_FIGURE_BROKEN_CHORD);
    CHECK(section_figure_rhythm_find("sixteenths", &rhythm) && rhythm == SECTION_FIGURE_SIXTEENTHS);
    CHECK(section_function_kind_find("coda", &function) && function == SECTION_FUNCTION_CODA);
    CHECK(section_lane_find("inner_voice", &lane) && lane == SECTION_LANE_INNER_VOICE);
    CHECK(section_lane_name((SectionLane)SECTION_LANE_COUNT) == NULL && SECTION_LAYERS_ALL == 63u);
    CHECK(SECTION_LAYER_BIT(SECTION_LANE_BASS) == 8u);

    SectionDescription d;
    memset(&d, 0xAB, sizeof(d));
    section_description_init(&d);
    CHECK(d.name[0] == '\0' && !d.has_role && d.variation == SECTION_RETURN_EXACT && d.key_start.kind == SECTION_REGION_NONE &&
          d.key_end.anchor == SECTION_ANCHOR_HOME && d.harmony.kind == SECTION_HARMONY_NONE && d.pedal == 0);
}

/* ------------------------------------------------------------- reading */

static cJSON *parse(const char *text) {
    cJSON *json = cJSON_Parse(text);
    CHECKF(json != NULL, "the test document parses: %s", text);
    return json;
}

/* Reads a document; true on success. */
static bool read_doc(SectionUse use, const char *text, SectionDescription *out, SectionError *error) {
    cJSON *json = parse(text);
    bool ok = json && section_from_json(json, use, NULL, 0, out, error);
    cJSON_Delete(json);
    return ok;
}

static void expect_refused(SectionUse use, const char *text, const char *path, const char *message) {
    SectionDescription d;
    SectionError error;
    memset(&error, 0, sizeof(error));
    bool ok = read_doc(use, text, &d, &error);
    CHECKF(!ok, "%s refused: %s", USE_NAMES[use_index(use)], text);
    CHECKF(strcmp(error.path, path) == 0, "path of %s: expected '%s', got '%s'", text, path, error.path);
    CHECKF(strstr(error.message, message) != NULL, "message of %s: expected '%s', got '%s'", text, message, error.message);
}

static bool covered[USE_COUNT][64];

static void mark_covered(SectionUse use, const cJSON *written) {
    for (const cJSON *child = written->child; child; child = child->next) {
        for (size_t i = 0; i < section_json_member_count(); i++) {
            if (strcmp(section_json_member_name(i), child->string) == 0) {
                covered[use_index(use)][i] = true;
            }
        }
    }
}

static void write_file(SectionUse use, int number, const cJSON *json) {
    char name[96];
    char temporary[128];
    snprintf(name, sizeof(name), "plan_section_written_%s_%d.json", USE_NAMES[use_index(use)], number);
    snprintf(temporary, sizeof(temporary), "%s.tmp", name);
    char *text = cJSON_Print(json);
    CHECK(text != NULL);
    if (text) {
        FILE *file = fopen(temporary, "wb");
        CHECKF(file != NULL, "writes %s", temporary);
        if (file) {
            fputs(text, file);
            fputc('\n', file);
            fclose(file);
            CHECKF(rename(temporary, name) == 0, "renames %s", temporary);
        }
    }
    cJSON_free(text);
}

/* A document in the table's order and the writer's form: read, written back equal, and read and written once more. */
static void round_trip(SectionUse use, int number, const char *text) {
    SectionDescription d;
    SectionDescription again;
    SectionError error;
    char where[96];
    memset(&error, 0, sizeof(error));
    if (!CHECKF(read_doc(use, text, &d, &error), "%s reads %s: %s %s", USE_NAMES[use_index(use)], text, error.path, error.message)) {
        return;
    }
    cJSON *expected = parse(text);
    cJSON *written = section_to_json(&d, use);
    if (CHECK(written != NULL && expected != NULL)) {
        CHECKF(test_json_equal(expected, written, where, sizeof(where)), "%s round trip of %s differs at %s",
               USE_NAMES[use_index(use)], text, where);
        mark_covered(use, written);
        CHECKF(section_from_json(written, use, NULL, 0, &again, &error), "the written %s reads again", text);
        cJSON *twice = section_to_json(&again, use);
        CHECK(twice != NULL && test_json_equal(written, twice, where, sizeof(where)));
        cJSON_Delete(twice);
        write_file(use, number, written);
    }
    cJSON_Delete(written);
    cJSON_Delete(expected);
}

static void test_round_trips(void) {
    /* forms: every member a form holds is in one of these */
    static const char *const FORMS[] = {
        "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":\"consequent\","
        "\"cadence\":\"perfect-authentic\",\"closed\":true,\"keys\":{\"start\":\"home\",\"end\":\"dominant\"},"
        "\"motive\":\"invert tonal=1\",\"layers\":[\"bass\",\"melody\"],\"construction\":{\"ideas\":{\"basic idea\":2,"
        "\"contrasting idea\":2,\"consequent basic idea\":2,\"consequent contrasting idea\":1},\"extensions\":[{\"kind\":"
        "\"repeat\",\"idea\":\"basic idea\",\"bars\":1,\"varied\":true}]}}",
        "{\"name\":\"b\",\"letter\":\"B\",\"role\":\"theme\",\"bars\":6,\"beginning\":\"presentation\",\"ending\":"
        "\"continuation\",\"cadence\":\"half\",\"layers\":\"all\",\"pedal\":\"dominant\",\"leads_in\":true,\"connective\":"
        "\"always\",\"construction\":\"auto\"}",
        "{\"name\":\"c\",\"letter\":\"C\",\"role\":\"episode\",\"beats\":8,\"beginning\":\"presentation\",\"ending\":"
        "\"continuation\",\"cadence\":\"imperfect-authentic\"}",
        "{\"name\":\"a2\",\"letter\":\"A\",\"role\":\"return\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":"
        "\"consequent\",\"cadence\":\"perfect-authentic\",\"returns\":\"a\",\"variation\":\"setting\",\"setting\":\"alberti\"}",
        "{\"name\":\"a3\",\"letter\":\"A\",\"role\":\"variation\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":"
        "\"consequent\",\"cadence\":\"perfect-authentic\",\"returns\":\"a\",\"variation\":\"figurate\",\"mode\":\"parallel\","
        "\"figure\":{\"kind\":\"scale\",\"rhythm\":\"sixteenths\",\"change_at\":2,\"climax\":true}}",
        "{\"name\":\"a4\",\"letter\":\"A\",\"role\":\"variation\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":"
        "\"consequent\",\"cadence\":\"perfect-authentic\",\"returns\":\"a\",\"variation\":\"figurate\",\"figure\":{\"kind\":"
        "\"neighbor\",\"rhythm\":\"eighths\",\"harmony\":\"reharmonized\"}}",
        "{\"name\":\"a5\",\"letter\":\"A\",\"role\":\"return\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":"
        "\"consequent\",\"cadence\":\"perfect-authentic\",\"returns\":\"a\",\"variation\":\"reharmonize\",\"mode\":\"home\"}",
        "{\"name\":\"a6\",\"letter\":\"A\",\"role\":\"return\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":"
        "\"consequent\",\"cadence\":\"perfect-authentic\",\"returns\":\"a\",\"mode\":\"major\"}",
        "{\"name\":\"l\",\"letter\":\"L\",\"role\":\"liquidation\",\"bars\":4,\"cadence\":\"half\"}",
        "{\"name\":\"t\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":4,\"beginning\":\"presentation\",\"ending\":"
        "\"continuation\",\"cadence\":\"half\",\"independent\":true}",
        "{\"name\":\"s\",\"letter\":\"S\",\"role\":\"subordinate\",\"bars\":8,\"beginning\":\"antecedent\",\"ending\":"
        "\"consequent\",\"cadence\":\"perfect-authentic\",\"lyric\":true}",
    };
    static const char *const PLANS[] = {
        "{\"id\":\"p1\",\"letter\":\"A\",\"role\":\"theme\",\"beginning\":\"presentation\",\"ending\":\"continuation\","
        "\"cadence\":\"half\",\"first_cadence\":\"imperfect-authentic\",\"closed\":true,\"keys\":{\"start\":\"home\",\"end\":"
        "{\"tonic\":\"G\",\"material\":\"major\"}},\"pedal\":\"dominant\",\"leads_in\":true,\"connective\":\"sometimes\","
        "\"connective_written\":true,\"construction\":{\"ideas\":{\"basic idea\":2,\"repetition\":2},\"reason\":\"motive\"},"
        "\"function\":\"subordinate\",\"free_phrase\":true,\"harmony\":{\"progression\":\"I V I\",\"cells\":4}}",
        "{\"id\":\"p2\",\"role\":\"subordinate\",\"beginning\":\"antecedent\",\"ending\":\"consequent\",\"cadence\":"
        "\"deceptive\",\"function\":\"subordinate\",\"lyric\":true,\"harmony\":{\"schema\":\"romanesca\"}}",
        "{\"id\":\"p3\",\"role\":\"variation\",\"returns\":\"p1\",\"variation\":\"figurate\",\"mode\":\"parallel\","
        "\"layers\":[\"bass\",\"melody\",\"inner_voice\"],\"figure\":{\"kind\":\"broken-chord\",\"rhythm\":\"eighths\"}}",
        "{\"id\":\"p4\",\"role\":\"transition\",\"keys\":{\"start\":\"first\",\"end\":{\"relation\":\"dominant\",\"anchor\":"
        "\"previous\"}},\"layers\":\"all\",\"function\":\"transition\",\"independent\":true,\"harmony\":{\"chords\":[{\"bar\":1,"
        "\"text\":\"I\"},{\"bar\":3,\"text\":\"V7\"}]}}",
        "{\"id\":\"p5\",\"role\":\"return\",\"keys\":{\"end\":{\"relation\":\"mediant\",\"anchor\":\"p1\"}},\"returns\":\"p1\","
        "\"variation\":\"setting\",\"setting\":\"oom-pah\"}",
    };
    static const char *const REQUESTS[] = {
        "{\"id\":\"intro\",\"letter\":\"I\",\"role\":\"intro\",\"bars\":4,\"pickup\":\"1/2\",\"cadence\":\"half\",\"region\":"
        "\"home\",\"harmony\":{\"progression\":\"I IV V\"}}",
        "{\"id\":\"body\",\"role\":\"body\",\"beats\":8,\"pickup\":1,\"cadence\":\"deceptive\",\"region\":"
        "{\"tonic\":\"C\",\"material\":\"major\"},\"function\":\"transition\",\"harmony\":{\"schema\":\"pachelbel\"}}",
        "{\"id\":\"end\",\"role\":\"ending\",\"bars\":8,\"region\":{\"relation\":\"dominant\"},\"function\":\"coda\","
        "\"harmony\":{\"chords\":[{\"bar\":1,\"text\":\"I\"},{\"bar\":2,\"text\":\"V\"}]}}",
    };
    static const char *const OVERLAYS[] = {
        "{\"name\":\"o\",\"region\":{\"relation\":\"mediant\",\"anchor\":\"previous\"},\"harmony\":{\"progression\":\"vi IV\"}}",
        "{\"name\":\"q\",\"region\":{\"relation\":\"subdominant\",\"anchor\":\"a\"}}",
    };
    for (size_t i = 0; i < sizeof(FORMS) / sizeof(FORMS[0]); i++) {
        round_trip(SECTION_USE_FORM, (int)i + 1, FORMS[i]);
    }
    for (size_t i = 0; i < sizeof(PLANS) / sizeof(PLANS[0]); i++) {
        round_trip(SECTION_USE_PLAN, (int)i + 1, PLANS[i]);
    }
    for (size_t i = 0; i < sizeof(REQUESTS) / sizeof(REQUESTS[0]); i++) {
        round_trip(SECTION_USE_REQUEST, (int)i + 1, REQUESTS[i]);
    }
    for (size_t i = 0; i < sizeof(OVERLAYS) / sizeof(OVERLAYS[0]); i++) {
        round_trip(SECTION_USE_OVERLAY, (int)i + 1, OVERLAYS[i]);
    }
    /* every member of a use is set in one of its documents */
    for (size_t u = 0; u < USE_COUNT; u++) {
        for (size_t i = 0; i < section_json_member_count(); i++) {
            if (section_json_member_uses(i) & (unsigned)USES[u]) {
                CHECKF(covered[u][i], "%s documents set \"%s\"", USE_NAMES[u], section_json_member_name(i));
            } else {
                CHECKF(!covered[u][i], "%s documents do not set \"%s\"", USE_NAMES[u], section_json_member_name(i));
            }
        }
    }
}

/* A cadence in any case reads as its core; a kind name is kept as written. */
static void test_cadence_spellings(void) {
    SectionDescription d;
    SectionError error;
    CHECK(read_doc(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"cadence\":\"pAc\"}", &d, &error));
    CHECK(d.has_cadence && d.cadence == CADENCE_TYPE_PERFECT_AUTHENTIC && d.cadence_name[0] == '\0');
    cJSON *json = section_to_json(&d, SECTION_USE_REQUEST);
    CHECK(json && strcmp(test_json_string(json, "cadence"), "perfect-authentic") == 0);
    cJSON_Delete(json);
    const char *const SPELLINGS[] = {"IAC", "hc", "Dc", "PC"};
    const CadenceType CORES[] = {CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_HALF, CADENCE_TYPE_DECEPTIVE,
                                 CADENCE_TYPE_PLAGAL};
    for (size_t i = 0; i < 4; i++) {
        char text[128];
        snprintf(text, sizeof(text), "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"cadence\":\"%s\"}", SPELLINGS[i]);
        CHECK(read_doc(SECTION_USE_REQUEST, text, &d, &error) && d.cadence == CORES[i]);
    }
    CHECK(read_doc(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"cadence\":\"medial-caesura\"}", &d, &error));
    CHECK(d.cadence == CADENCE_TYPE_HALF && strcmp(d.cadence_name, "medial-caesura") == 0);
    json = section_to_json(&d, SECTION_USE_REQUEST);
    CHECK(json && strcmp(test_json_string(json, "cadence"), "medial-caesura") == 0);
    cJSON_Delete(json);
}

/* A pickup is a whole number or a fraction. */
static void test_pickups(void) {
    SectionDescription d;
    SectionError error;
    CHECK(read_doc(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":\"3/2\"}", &d, &error));
    CHECK(d.has_pickup && d.pickup.num == 3 && d.pickup.den == 2);
    CHECK(read_doc(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":\"2/4\"}", &d, &error));
    CHECK(d.pickup.num == 1 && d.pickup.den == 2);
    CHECK(read_doc(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":0}", &d, &error));
    CHECK(d.has_pickup && d.pickup.num == 0 && d.pickup.den == 1);
    expect_refused(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":\"-1\"}", "/pickup",
                   "whole number or a fraction");
    expect_refused(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":\"1/0\"}", "/pickup",
                   "whole number or a fraction");
    expect_refused(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":1.5}", "/pickup",
                   "whole number");
    expect_refused(SECTION_USE_REQUEST, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"pickup\":true}", "/pickup",
                   "whole number or a fraction");
}

/* ------------------------------------------------------------- uses */

/* A member given to a use that does not hold it is refused with its path; an unknown one too; the caller's own are skipped. */
static void test_members_by_use(void) {
    for (size_t i = 0; i < section_json_member_count(); i++) {
        const char *name = section_json_member_name(i);
        for (size_t u = 0; u < USE_COUNT; u++) {
            if (section_json_member_uses(i) & (unsigned)USES[u]) {
                continue;
            }
            char text[96];
            char path[96];
            char message[96];
            snprintf(text, sizeof(text), "{\"%s\":1}", name);
            snprintf(path, sizeof(path), "/%s", name);
            snprintf(message, sizeof(message), "\"%s\" is not given here", name);
            expect_refused(USES[u], text, path, message);
        }
    }
    CHECK(section_json_member_count() == 31 && section_json_member_name(31) == NULL && section_json_member_uses(31) == 0);
    expect_refused(SECTION_USE_FORM, "{\"nonsense\":1}", "/nonsense", "unknown member \"nonsense\"");
    expect_refused(SECTION_USE_FORM, "{\"a/b~c\":1}", "/a~1b~0c", "unknown member");
    expect_refused(SECTION_USE_PLAN, "[]", "", "must be an object");
    expect_refused(SECTION_USE_PLAN, "{\"id\":\"a\",\"id\":\"b\"}", "/id", "member given twice");

    /* the caller's own members are not read and not refused */
    static const char *const SKIP[] = {"fill", "part"};
    cJSON *json = parse("{\"id\":\"a\",\"fill\":{\"x\":1},\"part\":3}");
    SectionDescription d;
    SectionError error;
    CHECK(section_from_json(json, SECTION_USE_PLAN, SKIP, 2, &d, &error) && strcmp(d.id, "a") == 0);
    CHECK(!section_from_json(json, SECTION_USE_PLAN, SKIP, 1, &d, &error) && strcmp(error.path, "/part") == 0);
    cJSON_Delete(json);
}

/* A description with every member set is written, for each use, with exactly the members that use holds. */
static void test_writer_by_use(void) {
    SectionDescription d;
    section_description_init(&d);
    snprintf(d.name, sizeof(d.name), "n");
    snprintf(d.id, sizeof(d.id), "i");
    snprintf(d.letter, sizeof(d.letter), "L");
    d.has_role = true;
    d.role = SECTION_ROLE_THEME;
    d.bars = 4;
    d.beats = 4;
    d.has_pickup = true;
    d.pickup.num = 1;
    d.pickup.den = 2;
    d.has_pairing = true;
    d.has_cadence = true;
    d.has_first_cadence = true;
    d.closed = true;
    d.key_start.kind = SECTION_REGION_HOME;
    d.key_end.kind = SECTION_REGION_HOME;
    snprintf(d.motive, sizeof(d.motive), "m");
    snprintf(d.returns, sizeof(d.returns), "r");
    d.variation = SECTION_RETURN_ORNAMENT;
    d.setting = SECTION_SETTING_BLOCK;
    d.mode = SECTION_MODE_HOME;
    d.layers = SECTION_LAYERS_ALL;
    d.pedal = 5;
    d.leads_in = true;
    d.connective = SECTION_CONNECTIVE_ALWAYS;
    d.connective_written = true;
    d.has_construction = true;
    d.has_figure = true;
    d.has_function = true;
    d.function.independent = true;
    d.function.lyric = true;
    d.free_phrase = true;
    d.harmony.kind = SECTION_HARMONY_SCHEMA;
    snprintf(d.harmony.schema, sizeof(d.harmony.schema), "s");
    for (size_t u = 0; u < USE_COUNT; u++) {
        cJSON *json = section_to_json(&d, USES[u]);
        if (!CHECK(json != NULL)) {
            continue;
        }
        size_t written = 0;
        for (size_t i = 0; i < section_json_member_count(); i++) {
            bool holds = (section_json_member_uses(i) & (unsigned)USES[u]) != 0;
            const char *name = section_json_member_name(i);
            CHECKF((test_json_member(json, name) != NULL) == holds, "%s writes \"%s\" only when it holds it", USE_NAMES[u], name);
            written += holds ? 1 : 0;
        }
        CHECK((size_t)cJSON_GetArraySize(json) == written);
        cJSON_Delete(json);
    }
}

/* ------------------------------------------------------------- the rules */

typedef struct {
    SectionUse use;
    const char *document;
    const char *path;
    const char *message;
} Refusal;

#define F SECTION_USE_FORM
#define P SECTION_USE_PLAN
#define R SECTION_USE_REQUEST
#define O SECTION_USE_OVERLAY

/* A form section's members from here on, for the refusals below. */
#define BASE "\"letter\":\"A\",\"bars\":8,\"cadence\":\"half\""
#define PAIR "\"beginning\":\"presentation\",\"ending\":\"continuation\""
#define PERIOD "\"beginning\":\"antecedent\",\"ending\":\"consequent\""
#define THEME "{\"name\":\"a\",\"role\":\"theme\"," BASE "," PAIR
#define RETURN_OF "{\"name\":\"b\",\"role\":\"return\"," BASE "," PAIR ",\"returns\":\"a\""

static const Refusal REFUSALS[] = {
    /* a form needs its members */
    {F, "{\"letter\":\"A\",\"role\":\"theme\",\"bars\":8," PAIR ",\"cadence\":\"half\"}", "/name", "required member is missing"},
    {F, "{\"name\":\"a\",\"role\":\"theme\",\"bars\":8," PAIR ",\"cadence\":\"half\"}", "/letter", "required member is missing"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"bars\":8," PAIR ",\"cadence\":\"half\"}", "/role", "required member is missing"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8," PAIR "}", "/cadence", "required member is missing"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\"," PAIR ",\"cadence\":\"half\"}", "", "give bars or beats"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"beats\":8," PAIR ",\"cadence\":\"half\"}", "",
     "give bars or beats, not both"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"material\",\"bars\":8," PAIR ",\"cadence\":\"half\"}", "/role",
     "\"material\" is not a role of a form"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"beginning\":\"presentation\",\"cadence\":\"half\"}",
     "/ending", "required member is missing"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"cadence\":\"half\"}", "/beginning",
     "required member is missing"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"liquidation\",\"bars\":4," PAIR ",\"cadence\":\"half\"}", "/beginning",
     "a liquidation has no beginning or ending"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8," PAIR ",\"cadence\":\"nonsense\"}", "/cadence",
     "unknown cadence \"nonsense\""},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"nonsense\",\"bars\":8," PAIR ",\"cadence\":\"half\"}", "/role",
     "unknown role \"nonsense\""},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":0," PAIR ",\"cadence\":\"half\"}", "/bars",
     "must be from 1 to 4096"},
    {F, THEME ",\"layers\":[\"meter\"]}", "/layers/0", "unknown lane \"meter\" (bass, inner_voice, melody)"},
    {F, THEME ",\"keys\":{\"end\":{\"tonic\":\"C\",\"material\":\"major\"}}}", "/keys/end",
     "a form's keys are home or a region of the form"},
    {F, THEME ",\"keys\":{\"middle\":\"home\"}}", "/keys/middle", "unknown member"},
    /* the operator roles and the transition */
    {F, "{\"name\":\"l\",\"letter\":\"L\",\"role\":\"liquidation\",\"bars\":4,\"cadence\":\"half\",\"construction\":\"auto\"}",
     "/construction", "a liquidation has no construction"},
    {F, "{\"name\":\"l\",\"letter\":\"L\",\"role\":\"liquidation\",\"bars\":4,\"cadence\":\"half\",\"motive\":\"x\"}", "/motive",
     "a liquidation has no motive"},
    {F, "{\"name\":\"l\",\"letter\":\"L\",\"role\":\"liquidation\",\"beats\":4,\"cadence\":\"half\"}", "/beats",
     "a liquidation has no beats"},
    {F, "{\"name\":\"l\",\"letter\":\"L\",\"role\":\"liquidation\",\"bars\":4,\"cadence\":\"half\",\"pedal\":\"dominant\"}",
     "/pedal", "a liquidation has no pedal"},
    {F, "{\"name\":\"r\",\"letter\":\"R\",\"role\":\"retransition\",\"bars\":4,\"cadence\":\"half\",\"mode\":\"home\"}", "/mode",
     "a retransition has no mode"},
    {F, "{\"name\":\"c\",\"letter\":\"C\",\"role\":\"coda\",\"bars\":8,\"cadence\":\"perfect-authentic\",\"keys\":{\"end\":"
        "\"home\"}}", "/keys", "a coda has no keys"},
    {F, "{\"name\":\"c\",\"letter\":\"C\",\"role\":\"coda\",\"bars\":8,\"cadence\":\"perfect-authentic\",\"layers\":\"all\"}",
     "/layers", "a coda has no layers"},
    {F, "{\"name\":\"t\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":4," PAIR ",\"cadence\":\"half\",\"returns\":\"a\"}",
     "/returns", "a transition has no returns"},
    {F, "{\"name\":\"t\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":4," PAIR ",\"cadence\":\"half\",\"pedal\":"
        "\"dominant\"}", "/pedal", "a transition has no pedal"},
    /* independent and lyric */
    {F, THEME ",\"independent\":true}", "/independent", "only a transition is independent"},
    {F, THEME ",\"lyric\":true}", "/lyric", "only a subordinate theme is lyric"},
    {F, "{\"name\":\"t\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":4," PAIR ",\"cadence\":\"half\",\"independent\":true,"
        "\"motive\":\"x\"}", "/motive", "an independent transition has its own motive"},
    {F, "{\"name\":\"s\",\"letter\":\"S\",\"role\":\"subordinate\",\"bars\":8," PERIOD ",\"cadence\":\"half\",\"lyric\":true,"
        "\"motive\":\"x\"}", "/motive", "a lyric theme's motive is m0 augmented"},
    {F, "{\"name\":\"s\",\"letter\":\"S\",\"role\":\"subordinate\",\"bars\":8," PERIOD ",\"cadence\":\"half\",\"lyric\":true,"
        "\"returns\":\"a\"}", "/returns", "a lyric theme returns nothing"},
    {F, "{\"name\":\"s\",\"letter\":\"S\",\"role\":\"subordinate\",\"bars\":8," PAIR ",\"cadence\":\"half\",\"lyric\":true}",
     "/beginning", "a lyric theme is a period"},
    /* constructions */
    {F, "{\"name\":\"i\",\"letter\":\"I\",\"role\":\"intro\",\"bars\":4," PAIR ",\"cadence\":\"half\",\"construction\":\"auto\"}",
     "/construction", "only a theme-like section has a construction"},
    {F, "{\"name\":\"a\",\"letter\":\"A\",\"role\":\"theme\",\"beats\":8," PAIR ",\"cadence\":\"half\",\"construction\":\"auto\"}",
     "/construction", "a construction needs bars, not beats"},
    {F, THEME ",\"construction\":{}}", "/construction/ideas", "required member is missing"},
    {F, THEME ",\"construction\":{\"ideas\":{\"nonsense\":1}}}", "/construction/ideas/nonsense", "unknown idea"},
    {F, THEME ",\"construction\":{\"ideas\":{\"basic idea\":17}}}", "/construction/ideas/basic idea",
     "must be a whole number of bars from 0 to 16"},
    {F, THEME ",\"construction\":{\"ideas\":{},\"extensions\":[{\"kind\":\"repeat\"},{\"kind\":\"repeat\"},{\"kind\":\"repeat\"},"
        "{\"kind\":\"repeat\"}]}}", "/construction/extensions", "at most 3 extensions"},
    {F, THEME ",\"construction\":{\"ideas\":{},\"extensions\":[{\"kind\":\"repeat\"}]}}", "/construction/extensions/0/idea",
     "a repeat needs the idea it follows"},
    {F, THEME ",\"construction\":{\"ideas\":{},\"extensions\":[{\"kind\":\"stretch\",\"idea\":\"nonsense\"}]}}",
     "/construction/extensions/0/idea", "must be the name of an idea"},
    {F, THEME ",\"construction\":{\"ideas\":{},\"extensions\":[{\"kind\":\"nonsense\"}]}}", "/construction/extensions/0/kind",
     "must be repeat, stretch, addition or evaded"},
    {F, THEME ",\"construction\":{\"ideas\":{},\"reason\":\"given\"}}", "/construction/reason", "\"reason\" is not given here"},
    {P, "{\"construction\":\"auto\"}", "/construction", "\"auto\" is for forms"},
    /* returns, variation, setting */
    {F, THEME ",\"variation\":\"ornament\"}", "/variation", "a variation needs returns"},
    {F, THEME ",\"setting\":\"block\"}", "/setting", "a setting needs returns"},
    {F, RETURN_OF ",\"setting\":\"block\"}", "/setting", "only a setting return has a setting"},
    {F, RETURN_OF ",\"variation\":\"setting\"}", "/variation", "a setting return names its setting"},
    {F, RETURN_OF ",\"variation\":\"nonsense\"}", "/variation", "unknown variation \"nonsense\""},
    /* the mode */
    {F, THEME ",\"mode\":\"home\"}", "/mode", "the home mode is for a returning section"},
    {F, RETURN_OF ",\"mode\":\"home\"}", "/mode", "a return in the home mode is reharmonized"},
    {F, RETURN_OF ",\"variation\":\"reharmonize\",\"mode\":\"home\",\"keys\":{\"end\":\"home\"}}", "/mode",
     "a section in the home mode has no keys"},
    {F, RETURN_OF ",\"mode\":\"parallel\"}", "/mode", "the parallel mode needs a figurate variation"},
    {F, THEME ",\"mode\":\"major\"}", "/mode", "the tonic major is for a returning section"},
    {F, RETURN_OF ",\"variation\":\"figurate\",\"mode\":\"major\",\"figure\":{\"kind\":\"scale\",\"rhythm\":\"eighths\"}}",
     "/mode", "a figurate variation takes the parallel mode, not the tonic major"},
    {F, RETURN_OF ",\"mode\":\"major\",\"keys\":{\"end\":\"home\"}}", "/mode", "a section in the tonic major has no keys"},
    {F, RETURN_OF ",\"mode\":\"nonsense\"}", "/mode", "unknown mode \"nonsense\""},
    /* the figure */
    {F, RETURN_OF ",\"variation\":\"figurate\"}", "/variation", "a figurate variation needs a figure"},
    {F, RETURN_OF ",\"figure\":{\"kind\":\"scale\",\"rhythm\":\"eighths\"}}", "/figure", "a figure needs the variation figurate"},
    {F, RETURN_OF ",\"variation\":\"figurate\",\"mode\":\"parallel\",\"figure\":{\"kind\":\"scale\",\"rhythm\":\"eighths\","
        "\"harmony\":\"kept\"}}", "/figure/harmony", "a variation in the other mode is reharmonized"},
    {F, RETURN_OF ",\"variation\":\"figurate\",\"figure\":{\"kind\":\"scale\",\"rhythm\":\"eighths\",\"change_at\":2}}",
     "/figure/change_at", "a root change needs the harmony reharmonized"},
    {F, RETURN_OF ",\"variation\":\"figurate\",\"figure\":{\"kind\":\"scale\",\"rhythm\":\"eighths\",\"harmony\":"
        "\"reharmonized\",\"change_at\":8}}", "/figure/change_at", "change_at must be a bar of the section before its last"},
    {F, RETURN_OF ",\"variation\":\"figurate\",\"figure\":{\"kind\":\"scale\"}}", "/figure/rhythm", "required member is missing"},
    {F, RETURN_OF ",\"variation\":\"figurate\",\"figure\":{\"kind\":\"scale\",\"rhythm\":\"eighths\",\"extra\":1}}",
     "/figure/extra", "unknown member"},
    /* the pedal, the lead-in and the connective */
    {F, "{\"name\":\"a\",\"role\":\"theme\"," BASE "," PERIOD ",\"pedal\":\"dominant\"}", "/pedal",
     "a pedal needs a presentation beginning"},
    {F, THEME ",\"pedal\":\"tonic\"}", "/pedal", "unknown pedal \"tonic\""},
    {F, THEME ",\"connective\":\"always\"}", "/connective", "a connective needs leads_in"},
    {F, RETURN_OF ",\"leads_in\":true}", "/leads_in", "a section that returns another cannot lead in"},
    /* a plan section */
    {P, "{\"letter\":\"A\",\"name\":\"a\"}", "/name", "\"name\" is not given here"},
    {P, "{\"motive\":\"x\"}", "/motive", "\"motive\" is not given here"},
    {P, "{\"beginning\":\"presentation\"}", "/ending", "required member is missing"},
    {P, "{\"role\":\"theme\",\"pedal\":\"dominant\"}", "/pedal", "a pedal needs a presentation beginning"},
    {P, "{\"construction\":{\"ideas\":{},\"reason\":\"nonsense\"}}", "/construction/reason", "unknown reason \"nonsense\""},
    {F, "{\"name\":\"i\",\"letter\":\"I\",\"role\":\"intro\",\"bars\":4," PAIR ",\"cadence\":\"half\",\"construction\":{\"ideas\":{}}}", "/construction", "only a theme-like section has a construction"},
    {P, "{\"function\":\"nonsense\"}", "/function", "unknown function \"nonsense\""},
    {P, "{\"independent\":true}", "/independent", "only a transition is independent"},
    {P, "{\"role\":\"theme\",\"function\":\"transition\",\"independent\":true,\"motive\":\"x\"}", "/motive",
     "\"motive\" is not given here"},
    /* a request */
    {R, "{\"role\":\"intro\",\"bars\":2}", "/id", "required member is missing"},
    {R, "{\"id\":\"i\",\"bars\":2}", "/role", "required member is missing"},
    {R, "{\"id\":\"i\",\"role\":\"theme\",\"bars\":2}", "/role", "a request names intro, body, turnaround or ending"},
    {R, "{\"id\":\"i\",\"role\":\"intro\"}", "", "give bars or beats"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"beats\":2}", "", "give bars or beats, not both"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"function\":\"retransition\"}", "/function",
     "a request takes transition or coda"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"region\":\"dominant\"}", "/region",
     "must be \"home\", {\"tonic\", \"material\"} or {\"relation\", \"anchor\"}"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"region\":{\"tonic\":\"C\"}}", "/region/material", "required member is missing"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"region\":{\"tonic\":\"Cbbbb\",\"material\":\"major\"}}", "/region/tonic",
     "must be 1 to 3 characters"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"region\":{\"relation\":\"dominant\",\"extra\":1}}", "/region/extra",
     "unknown member"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"region\":{}}", "/region",
     "a key needs a tonic and a material, a relation needs its name"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{}}", "/harmony", "give a progression, a schema or chords"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"schema\":\"s\",\"progression\":\"I\"}}", "/harmony",
     "give a progression, a schema or chords"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"schema\":\"s\",\"cells\":2}}", "/harmony/cells",
     "cells go with a progression"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"progression\":\"I\",\"cells\":17}}", "/harmony/cells",
     "must be from 0 to 16"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"chords\":[{\"bar\":1,\"text\":\"I\"},{\"bar\":1,\"text\":\"V\"}]}}",
     "/harmony/chords/1/bar", "bar 1 is given twice"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"chords\":[]}}", "/harmony/chords", "must hold 1 to 16 bars"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"chords\":[{\"bar\":1}]}}", "/harmony/chords/0/text",
     "required member is missing"},
    {R, "{\"id\":\"i\",\"role\":\"intro\",\"bars\":2,\"harmony\":{\"progression\":\"\"}}", "/harmony/progression",
     "must be 1 to 255 characters"},
    /* an overlay */
    {O, "{\"region\":\"home\"}", "/name", "required member is missing"},
    {O, "{\"name\":\"o\",\"letter\":\"A\"}", "/letter", "\"letter\" is not given here"},
    {O, "{\"name\":\"o\",\"region\":{\"relation\":\"x\",\"anchor\":\"\"}}", "/region/anchor", "must be 1 to 31 characters"},
};

static void test_refusals(void) {
    for (size_t i = 0; i < sizeof(REFUSALS) / sizeof(REFUSALS[0]); i++) {
        expect_refused(REFUSALS[i].use, REFUSALS[i].document, REFUSALS[i].path, REFUSALS[i].message);
    }
    /* a plan's layers are any lane */
    SectionDescription d;
    SectionError error;
    CHECK(read_doc(SECTION_USE_PLAN, "{\"layers\":[\"harmony\",\"key\"]}", &d, &error) &&
          d.layers == (SECTION_LAYER_BIT(SECTION_LANE_HARMONY) | SECTION_LAYER_BIT(SECTION_LANE_KEY)));
}

/* The rules a document cannot reach: a description built in code. */
static void expect_check(SectionUse use, const SectionDescription *d, bool ok, const char *path, const char *message) {
    SectionError error;
    memset(&error, 0, sizeof(error));
    bool result = section_description_check(d, use, &error);
    CHECKF(result == ok, "check of %s expects %s (%s %s)", path, ok ? "ok" : "refused", error.path, error.message);
    if (!ok && !result) {
        CHECKF(strcmp(error.path, path) == 0 && strstr(error.message, message) != NULL, "check: expected '%s' '%s', got '%s' '%s'",
               path, message, error.path, error.message);
    }
    CHECK(section_description_check(d, use, NULL) == ok);
}

static void test_checks_in_code(void) {
    SectionDescription d;
    section_description_init(&d);
    expect_check(SECTION_USE_PLAN, &d, true, "", "");

    d.pedal = 7;
    expect_check(SECTION_USE_PLAN, &d, false, "/pedal", "a pedal is dominant (5)");
    d.pedal = 0;

    d.has_cadence = true;
    d.cadence = CADENCE_TYPE_UNKNOWN;
    expect_check(SECTION_USE_PLAN, &d, false, "/cadence", "unknown cadence");
    d.cadence = CADENCE_TYPE_DECEPTIVE;
    expect_check(SECTION_USE_PLAN, &d, true, "", "");
    d.has_first_cadence = true;
    d.first_cadence = CADENCE_TYPE_COUNT;
    expect_check(SECTION_USE_PLAN, &d, false, "/first_cadence", "unknown cadence");
    d.has_first_cadence = false;

    d.layers = 1u << 9;
    expect_check(SECTION_USE_PLAN, &d, false, "/layers", "unknown lane");
    d.layers = 0;

    d.key_end.kind = SECTION_REGION_RELATION;
    expect_check(SECTION_USE_PLAN, &d, false, "/keys/end", "a relation needs its name");
    snprintf(d.key_end.relation, sizeof(d.key_end.relation), "dominant");
    d.key_end.anchor = SECTION_ANCHOR_SECTION;
    expect_check(SECTION_USE_PLAN, &d, false, "/keys/end/anchor", "an anchor that is a section names it");
    d.key_end.anchor = SECTION_ANCHOR_PREVIOUS;
    snprintf(d.key_end.anchor_section, sizeof(d.key_end.anchor_section), "a");
    expect_check(SECTION_USE_PLAN, &d, false, "/keys/end/anchor", "only an anchor that is a section has a section name");
    d.key_end.anchor_section[0] = '\0';
    expect_check(SECTION_USE_PLAN, &d, true, "", "");
    d.key_end.kind = SECTION_REGION_KEY;
    expect_check(SECTION_USE_PLAN, &d, false, "/keys/end", "a key needs a tonic and a material");
    d.key_end.kind = SECTION_REGION_FORM;
    expect_check(SECTION_USE_PLAN, &d, false, "/keys/end", "a form region needs its name");
    d.key_end.kind = (SectionRegionKind)9;
    expect_check(SECTION_USE_PLAN, &d, false, "/keys/end", "unknown region");
    d.key_end.kind = SECTION_REGION_NONE;

    d.harmony.kind = SECTION_HARMONY_PROGRESSION;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony/progression", "a progression has numerals");
    snprintf(d.harmony.numerals, sizeof(d.harmony.numerals), "I");
    d.harmony.cells = 17;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony/cells", "must be from 0 to 16");
    d.harmony.cells = 16;
    expect_check(SECTION_USE_PLAN, &d, true, "", "");
    d.harmony.kind = SECTION_HARMONY_SCHEMA;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony/schema", "a schema has a name");
    d.harmony.kind = SECTION_HARMONY_CHORDS;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony/chords", "must hold 1 to 16 bars");
    d.harmony.chord_count = 1;
    d.harmony.chords[0].bar = 0;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony/chords/0/bar", "must be from 1 to 4096");
    d.harmony.chords[0].bar = 1;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony/chords/0/text", "a chord has its text");
    d.harmony.kind = (SectionHarmonyKind)9;
    expect_check(SECTION_USE_PLAN, &d, false, "/harmony", "unknown harmony");
    d.harmony.kind = SECTION_HARMONY_NONE;

    d.has_construction = true;
    d.construction.idea_bars[SECTION_IDEA_BASIC] = 17;
    expect_check(SECTION_USE_PLAN, &d, false, "/construction/ideas/basic idea", "from 0 to 16");
    d.construction.idea_bars[SECTION_IDEA_BASIC] = 2;
    d.construction.extension_count = 4;
    expect_check(SECTION_USE_PLAN, &d, false, "/construction/extensions", "at most 3 extensions");
    d.construction.extension_count = 1;
    d.construction.extensions[0].bars = 5;
    expect_check(SECTION_USE_PLAN, &d, false, "/construction/extensions/0", "an extension is a kind, an idea and 0 to 4 bars");
    d.construction.extensions[0].bars = 1;
    d.construction.reason = SECTION_CONSTRUCTION_FROM_LENGTH;
    expect_check(SECTION_USE_PLAN, &d, true, "", "");
    d.has_construction = false;

    d.has_pickup = true;
    d.pickup.num = -1;
    d.pickup.den = 2;
    expect_check(SECTION_USE_PLAN, &d, true, "", ""); /* a plan holds no pickup member, the codec never sets one */
    d.has_pickup = false;

    /* a request's pickup is not negative */
    SectionDescription request;
    section_description_init(&request);
    snprintf(request.id, sizeof(request.id), "i");
    request.has_role = true;
    request.role = SECTION_ROLE_INTRO;
    request.bars = 2;
    expect_check(SECTION_USE_REQUEST, &request, true, "", "");
    request.has_pickup = true;
    request.pickup.num = -1;
    request.pickup.den = 2;
    expect_check(SECTION_USE_REQUEST, &request, false, "/pickup", "a pickup is not negative");
    request.has_pickup = false;
    request.key_end.kind = SECTION_REGION_FORM;
    snprintf(request.key_end.form_region, sizeof(request.key_end.form_region), "x");
    expect_check(SECTION_USE_REQUEST, &request, false, "/region", "a request names home, a tonic and material, or a relation");
    request.key_end.kind = SECTION_REGION_NONE;
    request.closed = true;
    expect_check(SECTION_USE_REQUEST, &request, false, "/closed", "\"closed\" is not given here");

    /* a form's function follows its role */
    SectionDescription form;
    section_description_init(&form);
    snprintf(form.name, sizeof(form.name), "t");
    snprintf(form.letter, sizeof(form.letter), "T");
    form.has_role = true;
    form.role = SECTION_ROLE_TRANSITION;
    form.bars = 4;
    form.has_pairing = true;
    form.has_cadence = true;
    form.cadence = CADENCE_TYPE_HALF;
    form.has_function = true;
    form.function.function = SECTION_FUNCTION_CODA;
    expect_check(SECTION_USE_FORM, &form, false, "/function", "the function of a transition is transition");
    form.function.function = SECTION_FUNCTION_TRANSITION;
    expect_check(SECTION_USE_FORM, &form, true, "", "");
    form.role = SECTION_ROLE_COUNT - 1;
    expect_check(SECTION_USE_FORM, &form, false, "/role", "\"seam\" is not a role of a form");
}

/* ------------------------------------------------------------- the schema fragment */

/* The member table and the properties of $defs.section in the fragment name the same members. */
static void test_table_matches_schema(void) {
    cJSON *schema = test_read_json(PLAN_SECTION_SCHEMA);
    if (!CHECKF(schema != NULL, "reads %s", PLAN_SECTION_SCHEMA)) {
        return;
    }
    const cJSON *defs = test_json_member(schema, "$defs");
    const cJSON *section = test_json_member(defs, "section");
    const cJSON *properties = test_json_member(section, "properties");
    if (CHECK(properties != NULL)) {
        CHECKF((size_t)cJSON_GetArraySize(properties) == section_json_member_count(), "the schema has %d members, the table %zu",
               cJSON_GetArraySize(properties), section_json_member_count());
        for (size_t i = 0; i < section_json_member_count(); i++) {
            CHECKF(test_json_member(properties, section_json_member_name(i)) != NULL, "the schema has \"%s\"",
                   section_json_member_name(i));
        }
        for (const cJSON *member = properties->child; member; member = member->next) {
            bool found = false;
            for (size_t i = 0; i < section_json_member_count(); i++) {
                found = found || strcmp(section_json_member_name(i), member->string) == 0;
            }
            CHECKF(found, "the table has \"%s\"", member->string);
        }
    }
    static const char *const DEFINITIONS[] = {"form_section", "plan_section", "request_section", "overlay", "region",
                                              "harmony",      "construction", "figure",          "cadence", "layers"};
    for (size_t i = 0; i < sizeof(DEFINITIONS) / sizeof(DEFINITIONS[0]); i++) {
        CHECKF(test_json_member(defs, DEFINITIONS[i]) != NULL, "the schema defines %s", DEFINITIONS[i]);
    }
    cJSON_Delete(schema);
}

/* The weight is the number of constraints the members used to be (B3 of P60). */
static void test_weight(void) {
    SectionDescription d;
    section_description_init(&d);
    CHECK(section_description_weight(&d) == 0);
    d.has_pairing = true; /* an archetype and a pairing */
    d.has_cadence = true;
    d.has_first_cadence = true;
    snprintf(d.letter, sizeof(d.letter), "A");
    CHECK(section_description_weight(&d) == 5);
    d.has_role = true;
    d.role = SECTION_ROLE_BODY; /* a game loop part */
    CHECK(section_description_weight(&d) == 6);
    d.role = SECTION_ROLE_THEME; /* a role that was no constraint */
    CHECK(section_description_weight(&d) == 5);
    d.leads_in = true;
    d.pedal = 5;
    d.connective = SECTION_CONNECTIVE_ALWAYS;
    snprintf(d.returns, sizeof(d.returns), "a");
    CHECK(section_description_weight(&d) == 9);
}

int main(void) {
    test_vocabulary();
    test_round_trips();
    test_cadence_spellings();
    test_pickups();
    test_members_by_use();
    test_writer_by_use();
    test_refusals();
    test_checks_in_code();
    test_table_matches_schema();
    test_weight();
    return test_finish("plan_section");
}
