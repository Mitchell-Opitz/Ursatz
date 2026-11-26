#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_workshop/workshop_api.h>

#include "ursatz_test.h"

/*
 * A blueprint in Ursatz Text (R-747): the tool blueprint-parse and the tool text on a tools workshop. The fixture text of a hand-B-like sentence reads
 * to the request JSON the Composer accepts and writes back to the same text; a text with every header and every field reads to the JSON written by
 * hand and writes back to itself; each refusal names its line and what it refused.
 */

static Workshop *workshop;

static char *fixture(const char *name) {
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", BLUEPRINT_FIXTURES, name);
    return test_must_read_file(path, NULL);
}

/* The request JSON of `text` through the tool blueprint-parse, a new tree; NULL with the error filled. */
static cJSON *parse(const char *text, UrsatzStatus *status, UrsatzError *error) {
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "text", text);
    WorkshopReply reply;
    memset(error, 0, sizeof *error);
    *status = workshop_tool(workshop, "blueprint-parse", params, NULL, &reply, error);
    cJSON_Delete(params);
    cJSON *request = *status == URSATZ_OK ? cJSON_DetachItemFromObjectCaseSensitive(reply.json, "request") : NULL;
    workshop_reply_destroy(&reply);
    return request;
}

/* The text of a request through the tool text (view full), a new string; NULL with the error filled. */
static char *write_text(const cJSON *request, UrsatzStatus *status, UrsatzError *error) {
    char *json = cJSON_PrintUnformatted(request);
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "text", json);
    cJSON_AddStringToObject(params, "view", "full");
    free(json);
    WorkshopReply reply;
    memset(error, 0, sizeof *error);
    *status = workshop_tool(workshop, "text", params, NULL, &reply, error);
    cJSON_Delete(params);
    char *text = NULL;
    if (*status == URSATZ_OK) {
        CHECK(strcmp(test_json_string(reply.json, "kind"), "blueprint") == 0);
        const char *printed = test_json_string(reply.json, "text");
        text = printed ? strdup(printed) : NULL;
    }
    workshop_reply_destroy(&reply);
    return text;
}

/* The Composer's verdict on the request as a theme (the entry validate). */
static UrsatzStatus theme_status(const cJSON *request, UrsatzError *error) {
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "entry", "validate");
    cJSON_AddStringToObject(params, "kind", "theme");
    cJSON_AddItemToObject(params, "request", cJSON_Duplicate(request, true));
    WorkshopReply reply;
    memset(error, 0, sizeof *error);
    UrsatzStatus status = workshop_tool(workshop, "composer", params, NULL, &reply, error);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
    return status;
}

static void expect_theme_valid(const cJSON *request, const char *what) {
    UrsatzError error;
    UrsatzStatus status = theme_status(request, &error);
    CHECKF(status == URSATZ_OK, "%s: the Composer refuses the request: %s at %s", what, error.message, error.path);
}

/* `text` reads to `expected` (JSON text, member order ignored) and the request writes back to `text`. */
static void expect_round_trip(const char *text, const char *expected, const char *what) {
    UrsatzStatus status;
    UrsatzError error;
    cJSON *request = parse(text, &status, &error);
    cJSON *want = cJSON_Parse(expected);
    char where[200] = "";
    CHECKF(request && want && test_json_equal(want, request, where, sizeof where), "%s: %s (the request differs at '%s')", what, error.message, where);
    if (request) {
        char *back = write_text(request, &status, &error);
        CHECKF(back && strcmp(back, text) == 0, "%s: the text does not write back as it was: %s\n%s", what, error.message, back ? back : "-");
        free(back);
    }
    cJSON_Delete(request);
    cJSON_Delete(want);
}

static void expect_refusal(const char *text, const char *message, const char *what) {
    UrsatzStatus status;
    UrsatzError error;
    cJSON *request = parse(text, &status, &error);
    CHECKF(!request && status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0 && strcmp(error.message, message) == 0,
           "%s: wanted '%s', got %d '%s' at %s", what, message, (int)status, error.message, error.path);
    cJSON_Delete(request);
}

/* A request JSON the text has no place for is refused when it is written. */
static void expect_write_refusal(const char *json, const char *message, const char *what) {
    UrsatzStatus status;
    UrsatzError error;
    cJSON *request = cJSON_Parse(json);
    char *text = write_text(request, &status, &error);
    CHECKF(!text && status == URSATZ_ERR_REQUEST && strcmp(error.message, message) == 0, "%s: wanted '%s', got %d '%s'", what, message, (int)status, error.message);
    free(text);
    cJSON_Delete(request);
}

static const char *const kEveryField =
    "ursatz-text 1 blueprint\n"
    "key D \"natural minor\"\n"
    "meter 6/8\n"
    "into period\n"
    "bars 4\n"
    "cadence PAC\n"
    "first_cadence HC\n"
    "development theme\n"
    "tempo 120\n"
    "accompaniment block voicing satb\n"
    "climax 3 3/2 A5\n"
    "basic 1:q 3:e 5:h pickup 1/2\n"
    "1: chords i ; melody D5:q. E5:e F5:q. r:q.\n"
    "2: chords V7 ; rhythm x:q. x:e x:q. x:e ; anchor 0 A4 ; anchor 3/2 C5 ; event 3 passing\n"
    "3: chords i ; job running ; bass D3:q. A3:q.\n"
    "4:\n";

static const char *const kEveryFieldJson =
    "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"into\":\"period\",\"key\":\"D\",\"material\":\"natural minor\",\"meter\":\"6/8\","
    "\"bars\":4,\"cadence\":\"PAC\",\"first_cadence\":\"HC\",\"development\":\"theme\",\"tempo\":120,"
    "\"motives\":[{\"id\":\"A\",\"form\":\"full\",\"degrees\":\"1:q 3:e 5:h\",\"pickup\":\"1/2\",\"harmony\":null,\"features\":[],\"role\":\"melodic\"}],"
    "\"slots\":{\"basic\":\"A\"},"
    "\"blueprint\":{\"bars\":["
    "{\"chords\":\"i\",\"melody\":\"D5:q. E5:e F5:q. r:q.\"},"
    "{\"chords\":\"V7\",\"rhythm\":\"x:q. x:e x:q. x:e\",\"anchors\":[{\"at\":\"0\",\"pitch\":\"A4\"},{\"at\":\"3/2\",\"pitch\":\"C5\"}],"
    "\"events\":[{\"at\":\"3\",\"type\":\"passing\"}]},"
    "{\"chords\":\"i\",\"job\":\"running\",\"bass\":\"D3:q. A3:q.\"},"
    "{}],"
    "\"climax\":{\"bar\":2,\"at\":\"3/2\",\"pitch\":\"A5\"},"
    "\"accompaniment\":{\"pattern\":\"block\",\"voicing\":\"satb\"}}}";

static void test_fixture(void) {
    char *text = fixture("blueprint-sentence.txt");
    char *json = fixture("blueprint-sentence.json");
    expect_round_trip(text, json, "the hand-B-like sentence");
    UrsatzStatus status;
    UrsatzError error;
    cJSON *request = parse(text, &status, &error);
    if (CHECKF(request != NULL, "the fixture text does not read: %s", error.message)) {
        expect_theme_valid(request, "the fixture text");
        CHECK(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "blueprint"), "bars")) == 8);
    }
    cJSON_Delete(request);
    /* the JSON file writes the same text as the text file holds, and the text reads to the JSON file's request */
    cJSON *from_file = cJSON_Parse(json);
    char *written = write_text(from_file, &status, &error);
    CHECKF(written && strcmp(written, text) == 0, "the fixture JSON does not write the fixture text: %s", error.message);
    free(written);
    cJSON_Delete(from_file);
    free(text);
    free(json);
}

static void test_reading(void) {
    expect_round_trip(kEveryField, kEveryFieldJson, "every header and field");
    /* comments and blank lines are ignored, the headers come in any order, white space inside a field is one space, a header value may be quoted */
    UrsatzStatus status;
    UrsatzError error;
    cJSON *request = parse("# a sentence\nursatz-text 1 blueprint\n\nmeter 4/4\nkey \"C\"   major\n# the bars\n1:   chords I  ;  rhythm  x:w  \n", &status, &error);
    cJSON *want = cJSON_Parse("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"key\":\"C\",\"material\":\"major\",\"meter\":\"4/4\","
                              "\"blueprint\":{\"bars\":[{\"chords\":\"I\",\"rhythm\":\"x:w\"}]}}");
    char where[200] = "";
    CHECKF(request && test_json_equal(want, request, where, sizeof where), "a text with comments and spaces: %s (differs at '%s')", error.message, where);
    cJSON_Delete(request);
    cJSON_Delete(want);
    /* a key with no material is a key alone */
    request = parse("ursatz-text 1 blueprint\nkey \"D minor\"\nclimax 1 2 G5\n1: chords I\n", &status, &error);
    CHECKF(request && strcmp(test_json_string(request, "key"), "D minor") == 0 && !cJSON_GetObjectItemCaseSensitive(request, "material"), "a key with no material");
    cJSON_Delete(request);
    /* a text with a basic motive is a request the Composer accepts */
    request = parse("ursatz-text 1 blueprint\nkey C major\nmeter 4/4\ninto sentence\nbars 8\ncadence PAC\nbasic 1:q 3:e 5:e 1':h\n"
                    "1: chords I\n2: chords V7\n3: chords V7\n4: chords I\n5: chords VI II@2\n6: chords I6 IV@2\n7: chords II6 V7@2\n8: chords I\n",
                    &status, &error);
    if (CHECKF(request != NULL, "a text with a basic motive: %s", error.message)) {
        expect_theme_valid(request, "a text with a basic motive");
    }
    cJSON_Delete(request);
    /* the Composer's check is not vacuous: a blueprint of one bar in an 8-bar theme is refused */
    request = parse("ursatz-text 1 blueprint\nkey C major\nmeter 4/4\ninto sentence\nbars 8\n1: chords I\n", &status, &error);
    if (CHECKF(request != NULL, "a one-bar blueprint text: %s", error.message)) {
        UrsatzStatus verdict = theme_status(request, &error);
        CHECKF(verdict == URSATZ_ERR_REQUEST && strstr(error.message, "the blueprint has 1 bars; the theme has 8") != NULL, "a one-bar blueprint: %d '%s'", (int)verdict,
               error.message);
    }
    cJSON_Delete(request);
}

static void test_refusals(void) {
    expect_refusal("", "the first line must be ursatz-text 1 blueprint", "an empty text");
    expect_refusal("key C major\nursatz-text 1 blueprint\n1: chords I\n", "line 1: the first line must be ursatz-text 1 blueprint", "a line before the header");
    expect_refusal("# a comment\n1: chords I\nursatz-text 1 blueprint\n", "line 2: the first line must be ursatz-text 1 blueprint", "a bar before the header");
    expect_refusal("ursatz-text 1 commands\n1: chords I\n", "line 1: the first line must be ursatz-text 1 blueprint", "another kind");
    expect_refusal("ursatz-text 1 blueprint\n", "the blueprint has no bar line: write one line a bar, 1: chords I ; rhythm x:w", "no bar");
    expect_refusal("ursatz-text 1 blueprint\nkey C major\n", "the blueprint has no bar line: write one line a bar, 1: chords I ; rhythm x:w", "headers only");
    expect_refusal("ursatz-text 1 blueprint\nfrob 3\n1: chords I\n", "line 2: unknown header 'frob'", "an unknown header");
    expect_refusal("ursatz-text 1 blueprint\nkey C\nkey D\n1: chords I\n", "line 3: key is given twice", "a header twice");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I\nkey C\n", "line 3: 'key' comes after the first bar: the header lines come before the bars", "a header after a bar");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I\nfrob\n", "line 3: 'frob' comes after the first bar: the header lines come before the bars", "a word after a bar");
    expect_refusal("ursatz-text 1 blueprint\n2: chords I\n", "line 2: bar 2 is out of order: the next bar is 1", "a first bar that is not 1");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I\n3: chords V\n", "line 3: bar 3 is out of order: the next bar is 2", "a bar skipped");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I\n1: chords V\n", "line 3: bar 1 is out of order: the next bar is 2", "a bar twice");
    expect_refusal("ursatz-text 1 blueprint\n1 chords I\n", "line 2: a bar line starts with its number and a colon: 1: chords I", "a bar with no colon");
    expect_refusal("ursatz-text 1 blueprint\n1: frob x\n", "line 2: bar 1: unknown field 'frob' (the fields are chords, melody, rhythm, job, anchor, event, bass and inner)",
                   "an unknown field");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I ; chords V\n", "line 2: bar 1: chords is given twice", "a field twice");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I ;\n", "line 2: bar 1 has an empty field: fields are split by one ';'", "an empty field");
    expect_refusal("ursatz-text 1 blueprint\n1: chords\n", "line 2: bar 1: chords needs its text", "a field with no text");
    expect_refusal("ursatz-text 1 blueprint\n1: inner\n", "line 2: bar 1: inner needs its notes", "an inner voice with no notes");
    expect_refusal("ursatz-text 1 blueprint\n1: anchor 0\n", "line 2: bar 1: anchor takes a position in beats and a pitch: anchor 0 D5", "an anchor with no pitch");
    expect_refusal("ursatz-text 1 blueprint\n1: event 0 passing x\n", "line 2: bar 1: event takes a position in beats and a type: event 0 appoggiatura", "an event with three words");
    expect_refusal("ursatz-text 1 blueprint\n1: anchor x D5\n", "line 2: bar 1: anchor position 'x' is not beats like 2 or 3/2", "an anchor at a word");
    expect_refusal("ursatz-text 1 blueprint\nbars eight\n1: chords I\n", "line 2: bars is a whole number: bars 8", "bars as a word");
    expect_refusal("ursatz-text 1 blueprint\nbars 0\n1: chords I\n", "line 2: bars is a whole number: bars 8", "bars 0");
    expect_refusal("ursatz-text 1 blueprint\ntempo fast\n1: chords I\n", "line 2: tempo is a number of quarter notes a minute: tempo 92", "tempo as a word");
    expect_refusal("ursatz-text 1 blueprint\nkey\n1: chords I\n", "line 2: key needs a tonic and the material: key C major", "a key with no tonic");
    expect_refusal("ursatz-text 1 blueprint\nmeter 4/4 3/4\n1: chords I\n", "line 2: meter takes one value", "a meter with two values");
    expect_refusal("ursatz-text 1 blueprint\ninto\n1: chords I\n", "line 2: into takes one value", "an into with no value");
    expect_refusal("ursatz-text 1 blueprint\naccompaniment block satb\n1: chords I\n", "line 2: accompaniment takes a pattern and optionally voicing <id>: accompaniment block voicing satb",
                   "an accompaniment with a stray word");
    expect_refusal("ursatz-text 1 blueprint\nclimax 0 2 A5\n1: chords I\n", "line 2: climax takes a bar (from 1), a position in beats and a pitch: climax 6 2 A5", "a climax in bar 0");
    expect_refusal("ursatz-text 1 blueprint\nclimax 2 x A5\n1: chords I\n", "line 2: climax position 'x' is not beats like 2 or 3/2", "a climax at a word");
    expect_refusal("ursatz-text 1 blueprint\nbasic\n1: chords I\n", "line 2: basic needs the degrees of its motive, like 1:q 3:e 5:h", "a basic with no degrees");
    expect_refusal("ursatz-text 1 blueprint\nbasic 1:q 3:e pickup x\n1: chords I\n", "line 2: pickup takes beats like 1 or 1/2", "a pickup at a word");
    expect_refusal("ursatz-text 1 blueprint\nkey \"C\n1: chords I\n", "line 2: unterminated string", "an unterminated string");
    expect_refusal("ursatz-text 1 blueprint\n  1: chords I\n", "line 2: unexpected indent: a header or a bar starts at the left margin", "an indented bar");
    expect_refusal("ursatz-text 1 blueprint\n1: chords I\n\t2: chords V\n", "line 3: indent with two spaces, not a tab", "a tab");

    /* the tool's own refusal */
    cJSON *params = cJSON_CreateObject();
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    CHECK(workshop_tool(workshop, "blueprint-parse", params, NULL, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
}

static void test_writing(void) {
    const char *const bars = "\"blueprint\":{\"bars\":[{\"chords\":\"I\"}]}";
    char json[600];
    snprintf(json, sizeof json, "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"style\":\"/x.json\",%s}", bars);
    expect_write_refusal(json, "the request member 'style' has no place in a blueprint text: write the request as JSON", "a style");
    snprintf(json, sizeof json, "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"slots\":{\"basic\":\"A\"},%s}", bars);
    expect_write_refusal(json, "motives and slots hold more than the motive of slots.basic: write the request as JSON", "a slot with no motive");
    snprintf(json, sizeof json,
             "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"motives\":[{\"id\":\"A\",\"form\":\"full\",\"degrees\":\"1:q 3:e\",\"features\":[{\"label\":\"a\"}]}],"
             "\"slots\":{\"basic\":\"A\"},%s}",
             bars);
    expect_write_refusal(json, "the motive of slots.basic holds more than degrees and a pickup: write the request as JSON", "a motive with features");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[{\"chords\":\"I\",\"melody2\":\"C5:w\"}]}}",
                         "bar 1: 'melody2' has no place in a blueprint text: write the request as JSON", "a bar member");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[{\"melody\":\"C5:q; D5:q\"}]}}",
                         "bar 1: melody is not a text of one line without ';'", "a ';' in a note list");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[{\"anchors\":[{\"at\":\"0\",\"pitch\":\"C5\",\"x\":1}]}]}}",
                         "bar 1 anchor: 'x' has no place in a blueprint text: write the request as JSON", "an anchor member");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[{\"anchors\":[{\"at\":\"x\",\"pitch\":\"C5\"}]}]}}",
                         "bar 1 anchor: at is not a position in beats like 2 or 3/2", "an anchor at a word");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[],\"climax\":{\"bar\":0,\"at\":\"0\"}}}",
                         "the climax needs a bar from 0, an at and a pitch", "a climax with no pitch");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[]}}", "the blueprint has no bars", "no bars");
    expect_write_refusal("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"blueprint\":{\"bars\":[{}],\"melody\":1}}",
                         "the blueprint member 'melody' has no place in a blueprint text: write the request as JSON", "a blueprint member");
    /* a request of another shape stays JSON: its text is the tree */
    UrsatzStatus status;
    UrsatzError error;
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "text", "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"into\":\"sentence\"}");
    WorkshopReply reply;
    memset(&error, 0, sizeof error);
    status = workshop_tool(workshop, "text", params, NULL, &reply, &error);
    CHECKF(status == URSATZ_OK && strcmp(test_json_string(reply.json, "kind"), "json") == 0, "a request with no blueprint stays json: %s", error.message);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
}

/* The views of a blueprint: json is the request, full its text, any other view is refused. */
static void test_views(void) {
    char *text = fixture("blueprint-sentence.txt");
    char *json = fixture("blueprint-sentence.json");
    cJSON *want = cJSON_Parse(json);
    const char *inputs[2] = {text, json};
    for (int i = 0; i < 2; i++) {
        cJSON *params = cJSON_CreateObject();
        cJSON_AddStringToObject(params, "text", inputs[i]);
        cJSON_AddStringToObject(params, "view", "json");
        WorkshopReply reply;
        UrsatzError error;
        memset(&error, 0, sizeof error);
        UrsatzStatus status = workshop_tool(workshop, "text", params, NULL, &reply, &error);
        CHECKF(status == URSATZ_OK && strcmp(test_json_string(reply.json, "kind"), "blueprint") == 0, "view json of the %s: %s", i ? "JSON" : "text", error.message);
        cJSON *got = cJSON_Parse(test_json_string(reply.json, "json"));
        char where[200] = "";
        CHECKF(got && test_json_equal(want, got, where, sizeof where), "view json of the %s differs at '%s'", i ? "JSON" : "text", where);
        cJSON_Delete(got);
        workshop_reply_destroy(&reply);
        cJSON_ReplaceItemInObjectCaseSensitive(params, "view", cJSON_CreateString("summary"));
        memset(&error, 0, sizeof error);
        status = workshop_tool(workshop, "text", params, NULL, &reply, &error);
        CHECKF(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/view") == 0 &&
                   strcmp(error.message, "a blueprint shows the views json (the theme request) and full (its text), not summary") == 0,
               "view summary of a blueprint: %d '%s' at %s", (int)status, error.message, error.path);
        workshop_reply_destroy(&reply);
        cJSON_Delete(params);
    }
    /* a bad text names its line through the tool text, at /params/text */
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "text", "ursatz-text 1 blueprint\n1: chords I\n3: chords V\n");
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_tool(workshop, "text", params, NULL, &reply, &error);
    CHECKF(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0 && strcmp(error.message, "line 3: bar 3 is out of order: the next bar is 2") == 0,
           "a bad blueprint text through the tool text: %d '%s' at %s", (int)status, error.message, error.path);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
    cJSON_Delete(want);
    free(text);
    free(json);
}

int main(void) {
    UrsatzError error;
    if (workshop_tools_open(&workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the tools workshop: %s\n", error.message);
        return 1;
    }
    test_fixture();
    test_reading();
    test_refusals();
    test_writing();
    test_views();
    workshop_close(workshop);
    return test_finish("workshop_blueprint_text");
}
