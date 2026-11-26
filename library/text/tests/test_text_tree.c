#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text/text_tree.h"
#include "ursatz_test.h"

/*
 * The tree form of Ursatz Text (R-662): every JSON object of the repository's fixtures, schemas, forms and knowledge prints and parses
 * back to an equal tree with the member order kept; the cases the rules name; every error with its line.
 */

/* Prints `object`, parses the text and checks the tree is equal with its member order. Returns the text (the caller frees) or NULL. */
static char *round_trip(const cJSON *object, const char *what) {
    char *text = text_tree_print(object, TEXT_KIND_JSON);
    if (!CHECKF(text != NULL, "%s: not printed", what)) {
        return NULL;
    }
    TextKind kind = TEXT_KIND_PLAN;
    TextError error = {0};
    cJSON *back = text_tree_parse(text, &kind, &error);
    if (!CHECKF(back != NULL, "%s: line %d: %s", what, error.line, error.message)) {
        free(text);
        return NULL;
    }
    CHECKF(kind == TEXT_KIND_JSON, "%s: kind", what);
    CHECKF(cJSON_Compare(object, back, true), "%s: the tree changed", what);
    char *a = cJSON_PrintUnformatted(object);
    char *b = cJSON_PrintUnformatted(back);
    CHECKF(a && b && strcmp(a, b) == 0, "%s: the member order changed", what);
    free(a);
    free(b);
    cJSON_Delete(back);
    return text;
}

static char *case_text(const char *json, const char *what) {
    cJSON *object = cJSON_Parse(json);
    CHECKF(object != NULL, "%s: bad case", what);
    char *text = object ? round_trip(object, what) : NULL;
    cJSON_Delete(object);
    return text;
}

static bool has(const char *text, const char *part) {
    return text && strstr(text, part) != NULL;
}

static void test_files(void) {
    size_t size = 0;
    char *list = test_must_read_file(TEXT_TREE_FILES, &size);
    int files = 0, round_tripped = 0, skipped = 0;
    for (char *line = strtok(list, "\n"); line; line = strtok(NULL, "\n")) {
        files++;
        cJSON *object = test_read_json(line);
        if (!cJSON_IsObject(object) || !object->child) {
            skipped++;
            cJSON_Delete(object);
            continue;
        }
        char *text = round_trip(object, line);
        if (text) {
            round_tripped++;
        }
        free(text);
        cJSON_Delete(object);
    }
    free(list);
    printf("text_tree: %d files, %d round-tripped, %d skipped\n", files, round_tripped, skipped);
    CHECKF(round_tripped >= 396, "only %d files round-tripped", round_tripped);
    CHECK(files == round_tripped + skipped);
}

/* Every kind round-trips through its name; an unknown or missing name is no kind and leaves the output alone. */
static void test_kinds(void) {
    for (int k = TEXT_KIND_JSON; k <= TEXT_KIND_BLUEPRINT; k++) {
        TextKind found = TEXT_KIND_JSON;
        CHECKF(text_kind_find(text_kind_name((TextKind)k), &found) && found == (TextKind)k, "kind %d round-trips through its name", k);
    }
    TextKind untouched = TEXT_KIND_PLAN;
    CHECK(!text_kind_find("no-such-kind", &untouched) && untouched == TEXT_KIND_PLAN);
    CHECK(!text_kind_find("", &untouched) && !text_kind_find(NULL, &untouched) && untouched == TEXT_KIND_PLAN);
    CHECK(text_kind_find("plan", NULL)); /* a NULL output is accepted */
}

static void test_scalars(void) {
    char *text = case_text("{\"a\":\"12\",\"b\":\"1/2\",\"c\":\"1/2..3/4\",\"d\":\"null\",\"e\":\"true\",\"f\":\"a..b\",\"g\":\"plain\","
                           "\"h\":\"with space\",\"i\":\"say \\\"hi\\\"\",\"j\":\"\",\"k\":\"x\\ny\",\"l\":\"é\"}",
                           "strings");
    CHECK(has(text, "a \"12\"") && has(text, "b \"1/2\"") && has(text, "c \"1/2..3/4\"") && has(text, "d \"null\""));
    CHECK(has(text, "e \"true\"") && has(text, "f \"a..b\"") && has(text, "g plain") && has(text, "h \"with space\""));
    CHECK(has(text, "j \"\""));
    free(text);

    text = case_text("{\"a\":null,\"b\":true,\"c\":false,\"d\":0,\"e\":-3,\"f\":1.5,\"g\":1e+20,\"h\":0.1,\"i\":-0.25}", "scalars");
    CHECK(has(text, "a null") && has(text, "b true") && has(text, "c false") && has(text, "e -3") && has(text, "f 1.5"));
    free(text);

    text = case_text("{\"r\":{\"num\":1,\"den\":2},\"s\":{\"start\":{\"num\":0,\"den\":1},\"end\":{\"num\":3,\"den\":4}},"
                     "\"n\":{\"num\":-5,\"den\":3}}",
                     "rationals");
    CHECK(has(text, "r 1/2") && has(text, "s 0/1..3/4") && has(text, "n -5/3"));
    free(text);

    /* an object that is not exactly a rational stays an object */
    text = case_text("{\"a\":{\"num\":1,\"den\":0},\"b\":{\"den\":1,\"num\":2},\"c\":{\"num\":1.5,\"den\":2},\"d\":{\"num\":1,\"den\":2,\"x\":3},"
                     "\"e\":{\"start\":{\"num\":1,\"den\":2},\"end\":3}}",
                     "not rationals");
    CHECK(has(text, "a num 1 den 0") && has(text, "b den 1 num 2") && has(text, "c num 1.5 den 2"));
    CHECK(has(text, "d num 1 den 2 x 3"));
    free(text);
}

static void test_shapes(void) {
    char *text = case_text("{\"rows\":[{\"id\":\"a\",\"pos\":{\"x\":1,\"y\":2},\"t\":[1,2]},{\"id\":\"b\",\"pos\":{\"x\":3,\"y\":4},\"t\":[]}]}",
                           "table with an object column");
    CHECK(has(text, "rows | id pos.x pos.y t") && has(text, "  a 1 2 [1 2]") && has(text, "  b 3 4 []"));
    free(text);

    /* rows that differ in their members are items, not a table */
    text = case_text("{\"rows\":[{\"id\":\"a\"},{\"id\":\"b\",\"n\":1}],\"one\":[{\"id\":\"a\"}]}", "no table");
    CHECK(!has(text, "|"));
    free(text);

    text = case_text("{\"m\":[[1,2],[3,[4,5]],[]],\"n\":[[[]]]}", "arrays of arrays");
    free(text);

    text = case_text("{\"a\":{},\"b\":[],\"c\":[{},[],1,{\"x\":{}}],\"d\":{\"p\":1,\"q\":{},\"r\":[]}}", "empty values");
    CHECK(has(text, "a {}") && has(text, "b []") && has(text, "- {}") && has(text, "- []"));
    free(text);

    text = case_text("{\"a b\":1,\"1x\":2,\"null\":3,\"\":4,\"-\":5,\"q\\\"r\":6,\"ok-1\":7,\"$x\":8}", "keys");
    CHECK(has(text, "\"a b\" 1") && has(text, "\"1x\" 2") && has(text, "\"null\" 3") && has(text, "\"\" 4") && has(text, "ok-1 7") &&
          has(text, "$x 8"));
    free(text);

    text = case_text("{\"o\":{\"a\":1,\"b\":\"x\",\"c\":{\"d\":2,\"e\":[{\"f\":1,\"g\":{\"h\":2}}]}},\"l\":[{\"a\":1,\"b\":{\"c\":1}},{\"x\":[1]},"
                     "{\"y\":{\"z\":[1,{\"k\":1}]}}]}",
                     "nesting");
    free(text);

    /* an array item that is an object whose first member is not a scalar */
    text = case_text("{\"l\":[{\"o\":{\"a\":1,\"b\":{\"c\":2}},\"n\":1},{\"p\":[],\"q\":{}}]}", "items");
    free(text);

    /* an object member with leading scalars and members below */
    text = case_text("{\"v\":{\"a\":1,\"b\":2,\"c\":{\"d\":3},\"e\":4}}", "leading");
    CHECK(has(text, "v a 1 b 2") && has(text, "  c d 3") && has(text, "  e 4"));
    free(text);

    cJSON *not_object = cJSON_CreateArray();
    CHECK(text_tree_print(not_object, TEXT_KIND_JSON) == NULL && text_tree_print(NULL, TEXT_KIND_JSON) == NULL);
    cJSON_Delete(not_object);
}

static void test_scalar_print(void) {
    cJSON *v = cJSON_Parse("[1,\"a b\",{\"num\":1,\"den\":2},null,[]]");
    char *s = text_scalar_print(v->child);
    CHECK(s && strcmp(s, "1") == 0);
    free(s);
    s = text_scalar_print(v);
    CHECK(s == NULL); /* an array that holds an array that holds... is inline only when every item is a scalar */
    cJSON_Delete(v);
    v = cJSON_Parse("[1,\"a b\",{\"num\":1,\"den\":2},null,true]");
    s = text_scalar_print(v);
    CHECK(s && strcmp(s, "[1 \"a b\" 1/2 null true]") == 0);
    free(s);
    cJSON_Delete(v);
    v = cJSON_Parse("{\"a\":1}");
    CHECK(text_scalar_print(v) == NULL);
    cJSON_Delete(v);
    v = cJSON_Parse("{}");
    CHECK(text_scalar_print(v) == NULL);
    cJSON_Delete(v);
}

static void expect_error(const char *text, int line, const char *part) {
    TextKind kind;
    TextError error = {-1, ""};
    cJSON *tree = text_tree_parse(text, &kind, &error);
    CHECKF(tree == NULL, "parsed: %s", text);
    CHECKF(error.line == line && strstr(error.message, part) != NULL, "wanted line %d '%s', got line %d '%s' for: %s", line, part, error.line,
           error.message, text);
    cJSON_Delete(tree);
}

static void test_errors(void) {
    expect_error("", 1, "the first line must be ursatz-text 1 <kind>");
    expect_error("a 1\n", 1, "the first line must be ursatz-text 1 <kind>");
    expect_error("ursatz-text 1\na 1\n", 1, "the first line must be ursatz-text 1 <kind>");
    expect_error("ursatz-text x json\n", 1, "the first line must be ursatz-text 1 <kind>");
    expect_error("# c\n\nursatz-text 2 json\na 1\n", 3, "grammar 2 is not known");
    expect_error("ursatz-text 1 nothing\n", 1, "kind 'nothing' is not known");
    expect_error("ursatz-text 1 json\n\ta 1\n", 2, "indent with two spaces");
    expect_error("ursatz-text 1 json\n a 1\n", 2, "indent with two spaces");
    expect_error("ursatz-text 1 json\na \"abc\n", 2, "unterminated string");
    expect_error("ursatz-text 1 json\na [1 2\n", 2, "unterminated [");
    expect_error("ursatz-text 1 json\na 1 2 3\n", 2, "a member takes one value or pairs of key and value");
    expect_error("ursatz-text 1 json\na 1 b\n", 2, "a key is a name or a quoted string");
    expect_error("ursatz-text 1 json\na x 1 y\n", 2, "a member takes one value or pairs of key and value");
    expect_error("ursatz-text 1 json\nt | a b\n  1 2\n  3\n", 4, "a table row has 1 cells, its header has 2");
    expect_error("ursatz-text 1 json\na 1\nb 2\na 3\n", 4, "duplicate member 'a'");
    expect_error("ursatz-text 1 json\na x 1\n  x 2\n", 3, "duplicate member 'x'");
    expect_error("ursatz-text 1 json\na\n", 2, "expected an indented block");
    expect_error("ursatz-text 1 json\n- 1\n", 2, "members, not list items");
    expect_error("ursatz-text 1 json\na 1\n    b 2\n", 3, "unexpected indent");
    expect_error("ursatz-text 1 json\na 1/0\n", 2, "denominator");
    expect_error("ursatz-text 1 json\na 1..2\n", 2, "not a span");
    expect_error("ursatz-text 1 json\na ]\n", 2, "unexpected ]");
    expect_error("ursatz-text 1 json\nl\n  - 1\n  k 2\n", 4, "list item");
    expect_error("ursatz-text 1 json\n1 2\n", 2, "a key is a name or a quoted string");
}

static void test_lines_and_units(void) {
    TextLines lines;
    TextError error = {0};
    CHECK(text_lines_split("\n# comment\nursatz-text 1 json\r\n\n  a 1  \n    # deeper comment\n  \nb [x y]", &lines, &error));
    CHECK(lines.count == 3);
    CHECK(lines.lines[0].indent == 0 && lines.lines[0].number == 3 && strcmp(lines.lines[0].text, "ursatz-text 1 json") == 0);
    CHECK(lines.lines[1].indent == 2 && lines.lines[1].number == 5 && strcmp(lines.lines[1].text, "a 1") == 0);
    CHECK(lines.lines[2].indent == 0 && lines.lines[2].number == 8 && strcmp(lines.lines[2].text, "b [x y]") == 0);
    text_lines_free(&lines);
    CHECK(lines.count == 0 && lines.lines == NULL);
    CHECK(!text_lines_split("a\n\tb\n", &lines, &error) && error.line == 2);
    CHECK(!text_lines_split("a\n   b\n", &lines, &error) && error.line == 2 && strstr(error.message, "two spaces"));

    cJSON *units = text_units_parse("a 1/2 0/1..1/1 [1 [2 \"x y\"]] {} \"q\" null 2.5 -3", 7, &error);
    CHECK(units && cJSON_GetArraySize(units) == 9);
    char *printed = units ? cJSON_PrintUnformatted(units) : NULL;
    CHECK(printed && strcmp(printed, "[\"a\",{\"num\":1,\"den\":2},{\"start\":{\"num\":0,\"den\":1},\"end\":{\"num\":1,\"den\":1}},"
                                     "[1,[2,\"x y\"]],{},\"q\",null,2.5,-3]") == 0);
    free(printed);
    cJSON_Delete(units);
    CHECK(text_units_parse("a \"b", 7, &error) == NULL && error.line == 7);
    CHECK(text_units_parse("a [b", 8, &error) == NULL && error.line == 8);

    /* a block of list items at an indent */
    CHECK(text_lines_split("- a\n- b 1\n  c 2\n-\n  - 1\n", &lines, &error));
    size_t pos = 0;
    cJSON *block = text_block_parse(&lines, &pos, 0, &error);
    char *json = block ? cJSON_PrintUnformatted(block) : NULL;
    CHECK(json && strcmp(json, "[\"a\",{\"b\":1,\"c\":2},[1]]") == 0);
    CHECK(pos == 5);
    free(json);
    cJSON_Delete(block);
    text_lines_free(&lines);

    /* no line at that indent gives an empty object */
    CHECK(text_lines_split("a 1\n", &lines, &error));
    pos = 1;
    block = text_block_parse(&lines, &pos, 2, &error);
    CHECK(cJSON_IsObject(block) && block->child == NULL);
    cJSON_Delete(block);
    text_lines_free(&lines);
}

int main(void) {
    test_files();
    test_kinds();
    test_scalars();
    test_shapes();
    test_scalar_print();
    test_errors();
    test_lines_and_units();
    return test_finish("text_tree");
}
