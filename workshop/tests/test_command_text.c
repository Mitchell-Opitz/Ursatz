#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_workshop/workshop_api.h>

#include "ursatz_test.h"

/*
 * Commands in Ursatz Text (R-665): the tool commands-parse on a tools workshop. Each rule with the line and the message it names, a text with
 * comments, a header, an object param and three commands, and the objects against hand-written JSON.
 */

static Workshop *workshop;

static UrsatzStatus parse(const char *text, cJSON **commands, UrsatzError *error) {
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "text", text);
    WorkshopReply reply;
    memset(error, 0, sizeof *error);
    UrsatzStatus status = workshop_tool(workshop, "commands-parse", params, NULL, &reply, error);
    cJSON_Delete(params);
    *commands = status == URSATZ_OK ? cJSON_DetachItemFromObjectCaseSensitive(reply.json, "commands") : NULL;
    workshop_reply_destroy(&reply);
    return status;
}

/* The commands of `text` equal the JSON `expected` (a list), member order ignored. */
static void expect_commands(const char *text, const char *expected, const char *what) {
    cJSON *commands = NULL;
    UrsatzError error;
    UrsatzStatus status = parse(text, &commands, &error);
    cJSON *want = cJSON_Parse(expected);
    char *got_text = commands ? cJSON_PrintUnformatted(commands) : NULL;
    CHECKF(status == URSATZ_OK && commands && cJSON_Compare(commands, want, true), "%s: %s\n got %s\n want %s", what, error.message, got_text ? got_text : "-", expected);
    free(got_text);
    cJSON_Delete(commands);
    cJSON_Delete(want);
}

static void expect_refusal(const char *text, const char *message, const char *what) {
    cJSON *commands = NULL;
    UrsatzError error;
    UrsatzStatus status = parse(text, &commands, &error);
    CHECKF(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0 && strcmp(error.message, message) == 0, "%s: wanted '%s', got %d '%s' at %s", what,
           message, (int)status, error.message, error.path);
    cJSON_Delete(commands);
}

int main(void) {
    UrsatzError error;
    if (workshop_tools_open(&workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the tools workshop: %s\n", error.message);
        return 1;
    }

    /* a command with no target, params by their types */
    expect_commands("project.create name demo description \"a first project\"\n",
                    "[{\"command\":\"project.create\",\"params\":{\"name\":\"demo\",\"description\":\"a first project\"}}]", "a string and a quoted string");
    expect_commands("project.create name 123\n", "[{\"command\":\"project.create\",\"params\":{\"name\":\"123\"}}]", "a number for a string is its text");
    expect_commands("project.create name 3/4\n", "[{\"command\":\"project.create\",\"params\":{\"name\":\"3/4\"}}]", "a rational for a string is its text");
    expect_commands("subject.create name hero kind character note \"a person\"\n",
                    "[{\"command\":\"subject.create\",\"params\":{\"name\":\"hero\",\"kind\":\"character\",\"note\":\"a person\"}}]", "an enum word");
    expect_commands("item.compose item:pce-1@v2 seed 7 update_uses true\n",
                    "[{\"command\":\"item.compose\",\"target\":\"pce-1\",\"base_version\":2,\"params\":{\"seed\":7,\"update_uses\":true}}]", "an integer and a boolean, a target with its version");
    expect_commands("collection.add-pieces collection:col-1 piece_ids [a b \"c d\"]\n",
                    "[{\"command\":\"collection.add-pieces\",\"target\":\"col-1\",\"params\":{\"piece_ids\":[\"a\",\"b\",\"c d\"]}}]", "a list of words");
    expect_commands("library.update piece:piece-1 tags [x 5]\n", "[{\"command\":\"library.update\",\"target\":\"piece-1\",\"params\":{\"tags\":[\"x\",\"5\"]}}]",
                    "a number in a list of strings is its text");
    expect_commands("library.import filename song.mid file /tmp/song.mid\n",
                    "[{\"command\":\"library.import\",\"params\":{\"filename\":\"song.mid\"},\"file\":\"/tmp/song.mid\"}]", "a command that takes a file");
    expect_commands("job.cancel job:job-1\n", "[{\"command\":\"job.cancel\",\"target\":\"job-1\",\"params\":{}}]", "no params");
    expect_commands("project.update project:prj-1 name x\n", "[{\"command\":\"project.update\",\"target\":\"prj-1\",\"params\":{\"name\":\"x\"}}]",
                    "a target that takes no version");

    /* an object param below the command, in the tree form; comments, blank lines and the header */
    const char *file =
        "# edit the piece\n"
        "ursatz-text 1 commands\n"
        "\n"
        "item.save item:mot-1@v1 note \"Shorter\"\n"
        "  body uses []\n"
        "    motif role melodic notes \"C4:e D4:e\"\n"
        "    extra\n"
        "      - 1\n"
        "      - k v\n"
        "# between two commands\n"
        "project.create name demo\n"
        "item.compose item:pce-1@v2 seed 1\n";
    expect_commands(file,
                    "[{\"command\":\"item.save\",\"target\":\"mot-1\",\"base_version\":1,\"params\":{\"note\":\"Shorter\",\"body\":{\"uses\":[],"
                    "\"motif\":{\"role\":\"melodic\",\"notes\":\"C4:e D4:e\"},\"extra\":[1,{\"k\":\"v\"}]}}},"
                    "{\"command\":\"project.create\",\"params\":{\"name\":\"demo\"}},"
                    "{\"command\":\"item.compose\",\"target\":\"pce-1\",\"base_version\":2,\"params\":{\"seed\":1}}]",
                    "a file of three commands with an object param, comments and a header");
    expect_commands("style.build name Mine\n  filter tag t\n", "[{\"command\":\"style.build\",\"params\":{\"name\":\"Mine\",\"filter\":{\"tag\":\"t\"}}}]",
                    "an object param written as a member line");
    expect_commands("", "[]", "no commands");
    expect_commands("# only a comment\n", "[]", "only a comment");

    /* every refusal names its line */
    expect_refusal("project.create name a\nitem.nope item:x@v1\n", "line 2: unknown command 'item.nope'", "an unknown command");
    expect_refusal("item.compose project:x@v1\n", "line 1: item.compose works on item:<id>", "a target of another kind");
    expect_refusal("item.compose seed 1\n", "line 1: item.compose works on item:<id>", "a missing target");
    expect_refusal("item.compose item:x seed 1\n", "line 1: give the version you read: item:<id>@v<n>", "a missing version");
    expect_refusal("item.compose item:x@vx seed 1\n", "line 1: '@vx' is not a version: write item:<id>@v<n>", "a version that is not a number");
    expect_refusal("project.create nme demo\n", "line 1: project.create takes no param 'nme'", "an unknown param");
    expect_refusal("project.create name\n", "line 1: name needs a value: a string", "a missing value");
    expect_refusal("item.compose item:x@v1 seed one\n", "line 1: seed must be a whole number", "a value of the wrong unit");
    expect_refusal("item.compose item:x@v1 update_uses 1\n", "line 1: update_uses must be true or false", "a number for a boolean");
    expect_refusal("subject.create name a kind [x]\n", "line 1: kind must be one of its words", "a list for an enum");
    expect_refusal("project.create name a name b\n", "line 1: name is given twice", "a param given twice");
    expect_refusal("project.create name a\n  name b\n", "line 2: name is given twice", "a param given on a line below too");
    expect_refusal("project.create name a\n  nope b\n", "line 2: project.create takes no param 'nope'", "an unknown param below");
    expect_refusal("item.save item:x@v1\n  body 5\n", "line 2: body must be an object (write its members below the command)", "an object param with a scalar");
    expect_refusal("project.create name \"a\n", "line 1: unterminated string", "an unterminated string");
    expect_refusal("collection.add-pieces collection:c piece_ids [a b\n", "line 1: unterminated [", "an unterminated list");
    expect_refusal("project.create name a\n\tdescription b\n", "line 2: indent with two spaces, not a tab", "a tab");
    expect_refusal("project.create name a\n    description b\n", "line 2: unexpected indent", "an indent of four");
    expect_refusal("  project.create name a\n", "line 1: unexpected indent: a command starts at the left margin", "a command that starts indented");
    expect_refusal("ursatz-text 1 plan\nproject.create name a\n", "line 1: the header must be ursatz-text 1 commands", "another header");
    expect_refusal("library.import filename x.mid file\n", "line 1: file needs the path of the file", "a file with no path");
    expect_refusal("project.create name a file x\n", "line 1: project.create takes no param 'file'", "a file for a command that takes none");

    /* the tool's own refusals */
    cJSON *params = cJSON_CreateObject();
    WorkshopReply reply;
    memset(&error, 0, sizeof error);
    CHECK(workshop_tool(workshop, "commands-parse", params, NULL, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
    workshop_close(workshop);
    return test_finish("workshop_command_text");
}
