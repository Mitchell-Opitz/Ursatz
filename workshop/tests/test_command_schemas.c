#include "api_harness.h"

#include <ursatz_test.h>

#include "coordinator/command_schemas.h"
#include "items/store.h"

/*
 * The commands as schemas (R-664): every schema has a row of the dispatch table and every row a schema; the check of a command's target and params,
 * one refusal of each kind at its path (on a synthetic schema for the kinds no command of today uses, and through workshop_command for the rest);
 * the actor of a command in the version envelope; the actor and the lineage in the versions an item lists. Runs in its own working directory.
 */

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static void test_parity(void) {
    size_t rows = 0;
    const char *const *names = workshop_command_names(&rows);
    check(rows == 43, "the dispatch table has 43 commands");
    size_t schemas = 0;
    for (size_t f = 0; f < workshop_command_family_count(); f++) {
        const cJSON *command;
        cJSON_ArrayForEach(command, member(workshop_command_family_at(f), "commands")) {
            schemas++;
            bool found = false;
            for (size_t i = 0; i < rows; i++) {
                found = found || strcmp(names[i], str(command, "name")) == 0;
            }
            check_quiet(found, "a schema names a command of the table");
            const char *dot = strchr(str(command, "name"), '.');
            check_quiet(dot && strncmp(str(command, "name"), str(workshop_command_family_at(f), "family"), (size_t)(dot - str(command, "name"))) == 0,
                        "a command's family is its name before the dot");
        }
    }
    check(schemas == rows, "every command of the table has a schema and every schema a command");
    for (size_t i = 0; i < rows; i++) {
        check_quiet(workshop_command_schema(names[i]) != NULL, "every command of the table has a schema");
    }
    check(workshop_command_schema("item.nope") == NULL, "an unknown name has no schema");
}

/* The check on a hand-written schema, for the kinds no command of today uses. */
static void test_check_kinds(void) {
    cJSON *schema = cJSON_Parse(
        "{\"name\":\"demo.run\",\"target\":\"item\",\"params\":["
        "{\"name\":\"word\",\"type\":\"string\",\"required\":true,\"max_length\":4,\"help\":\"h\"},"
        "{\"name\":\"count\",\"type\":\"integer\",\"required\":false,\"minimum\":1,\"maximum\":8,\"help\":\"h\"},"
        "{\"name\":\"floor\",\"type\":\"integer\",\"required\":false,\"minimum\":2,\"help\":\"h\"},"
        "{\"name\":\"flag\",\"type\":\"boolean\",\"required\":false,\"help\":\"h\"},"
        "{\"name\":\"pick\",\"type\":\"enum\",\"required\":false,\"values\":[\"a\",\"b\",\"c\"],\"help\":\"h\"},"
        "{\"name\":\"pickup\",\"type\":\"rational\",\"required\":false,\"help\":\"h\"},"
        "{\"name\":\"extra\",\"type\":\"object\",\"required\":false,\"help\":\"h\"},"
        "{\"name\":\"ids\",\"type\":\"array\",\"required\":false,\"items\":\"integer\",\"help\":\"h\"},"
        "{\"name\":\"after\",\"type\":\"string\",\"required\":false,\"help\":\"h\"},"
        "{\"name\":\"before\",\"type\":\"string\",\"required\":false,\"help\":\"h\"}],"
        "\"one_of\":[[\"after\",\"before\"]]}");
    UrsatzError error;
    struct {
        const char *target;
        const char *params;
        const char *path;
        const char *message;
    } cases[] = {
        {"", "{\"word\":\"ab\",\"after\":\"x\"}", "/target", "demo.run needs a target"},
        {"item:x", "{\"after\":\"x\"}", "/params/word", "word is required"},
        {"item:x", "{\"word\":null,\"after\":\"x\"}", "/params/word", "word is required"},
        {"item:x", "{\"word\":5,\"after\":\"x\"}", "/params/word", "word must be a string"},
        {"item:x", "{\"word\":\"abcde\",\"after\":\"x\"}", "/params/word", "word is longer than 4 characters"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"count\":0}", "/params/count", "count must be from 1 to 8"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"count\":9}", "/params/count", "count must be from 1 to 8"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"count\":1.5}", "/params/count", "count must be a whole number"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"count\":\"2\"}", "/params/count", "count must be a whole number"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"floor\":1}", "/params/floor", "floor must be 2 or more"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"flag\":\"yes\"}", "/params/flag", "flag must be true or false"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"pick\":\"d\"}", "/params/pick", "pick must be one of a, b, c"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"pickup\":\"1/0\"}", "/params/pickup", "pickup must be a rational like 3/4"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"pickup\":\"x\"}", "/params/pickup", "pickup must be a rational like 3/4"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"extra\":[]}", "/params/extra", "extra must be an object"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"ids\":{}}", "/params/ids", "ids must be a list"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"ids\":[1,\"b\"]}", "/params/ids/1", "ids must be a list of whole numbers"},
        {"item:x", "{\"word\":\"ab\"}", "/params/after", "give exactly one of after, before"},
        {"item:x", "{\"word\":\"ab\",\"after\":\"x\",\"before\":\"y\"}", "/params/after", "give exactly one of after, before"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        cJSON *params = cJSON_Parse(cases[i].params);
        memset(&error, 0, sizeof error);
        UrsatzStatus status = workshop_command_check(schema, cases[i].target, params, &error);
        check(status == URSATZ_ERR_REQUEST && strcmp(error.path, cases[i].path) == 0 && strcmp(error.message, cases[i].message) == 0, cases[i].message);
        cJSON_Delete(params);
    }
    cJSON *good = cJSON_Parse("{\"word\":\"abcd\",\"count\":8,\"floor\":2,\"flag\":true,\"pick\":\"c\",\"pickup\":\"-3/4\",\"extra\":{},\"ids\":[1,2],\"before\":\"y\","
                              "\"undeclared\":[1]}");
    memset(&error, 0, sizeof error);
    check(workshop_command_check(schema, "item:x", good, &error) == URSATZ_OK, "a command that meets its schema is accepted (an undeclared member passes)");
    cJSON_Delete(good);
    cJSON *whole = cJSON_Parse("{\"word\":\"ab\",\"pickup\":3,\"after\":\"x\"}");
    check(workshop_command_check(schema, "item:x", whole, &error) == URSATZ_OK, "a whole number is a rational");
    cJSON_Delete(whole);
    cJSON_Delete(schema);
    cJSON *none = cJSON_Parse("{\"name\":\"demo.make\",\"target\":null,\"params\":[],\"one_of\":[]}");
    memset(&error, 0, sizeof error);
    cJSON *empty = cJSON_CreateObject();
    check(workshop_command_check(none, "item:x", empty, &error) == URSATZ_ERR_REQUEST && strcmp(error.message, "demo.make takes no target") == 0 &&
              strcmp(error.path, "/target") == 0,
          "a command with no target refuses one");
    check(workshop_command_check(none, "", empty, &error) == URSATZ_OK, "and takes none");
    cJSON_Delete(empty);
    cJSON_Delete(none);
}

/* The same checks through workshop_command on the schemas of today. */
static void test_through_command(void) {
    WorkshopReply reply;
    UrsatzError error;
    struct {
        const char *name;
        const char *target;
        const char *params;
        long base;
        const char *path;
        const char *message;
    } cases[] = {
        {"project.update", NULL, "{\"name\":\"x\"}", -1, "/target", "project.update needs a target"},
        {"project.create", "prj-x", "{\"name\":\"x\"}", -1, "/target", "project.create takes no target"},
        {"project.create", NULL, "{}", -1, "/params/name", "name is required"},
        {"project.create", NULL, "{\"name\":5}", -1, "/params/name", "name must be a string"},
        {"item.regenerate", "mot-x", "{\"version\":\"x\",\"section_id\":\"a\"}", 1, "/params/version", "version must be a whole number"},
        {"item.regenerate", "mot-x", "{\"version\":1}", 1, "/params/section_id", "section_id is required"},
        {"item.regenerate", "mot-x", "{\"version\":1,\"section_id\":\"a\",\"seed\":-1}", 1, "/params/seed", "seed must be from 0 to 4294967295"},
        {"item.compose", "mot-x", "{\"update_uses\":\"yes\"}", 1, "/params/update_uses", "update_uses must be true or false"},
        {"item.variations", "mot-x", "{\"version\":1,\"section_id\":\"a\",\"lane\":\"melody\",\"count\":9}", -1, "/params/count", "count must be from 1 to 8"},
        {"subject.create", NULL, "{\"name\":\"A\",\"kind\":\"robot\"}", -1, "/params/kind", "kind must be one of character, place, idea, note"},
        {"collection.add-pieces", "col-x", "{\"piece_ids\":[1]}", -1, "/params/piece_ids/0", "piece_ids must be a list of strings"},
        {"item.save", "mot-x", "{\"body\":\"x\"}", 1, "/params/body", "body must be an object"},
        {"style.build", NULL, "{}", -1, "/params/filter", "give exactly one of filter, collection_id, piece_ids, brief"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        UrsatzStatus status = command(cases[i].name, cases[i].target, cases[i].params, cases[i].base, &reply, &error);
        check(status == URSATZ_ERR_REQUEST && strcmp(error.path, cases[i].path) == 0 && strcmp(error.message, cases[i].message) == 0, cases[i].message);
        workshop_reply_destroy(&reply);
    }
    /* a params member no schema declares passes until the GUI sends exactly the schemas' params (B9) */
    check(command("project.create", NULL, "{\"name\":\"Quiet\",\"undeclared\":1}", -1, &reply, &error) == URSATZ_OK, "an undeclared member passes");
    workshop_reply_destroy(&reply);
}

static void test_actor(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(command("project.create", NULL, "{\"name\":\"Work\"}", -1, &reply, &error) == URSATZ_OK, "a project is made");
    char project[64];
    snprintf(project, sizeof project, "%s", str(member(reply.json, "project"), "id"));
    workshop_reply_destroy(&reply);
    char params[2000];
    snprintf(params, sizeof params, "{\"type\":\"motif\",\"name\":\"Source\",\"project_id\":\"%s\",\"body\":%s}", project, kMotifBody);
    check(command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK, "a motif is made");
    char motif[64];
    snprintf(motif, sizeof motif, "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof params, "{\"body\":%s,\"note\":\"second\"}", kMotifBody);
    check(command("item.save", motif, params, 1, &reply, &error) == URSATZ_OK, "item.save writes the next version");
    workshop_reply_destroy(&reply);
    cJSON *last = last_record();
    check(strcmp(str(last, "command"), "item.save") == 0 && strcmp(str(last, "actor"), "test") == 0, "item.save is journaled with the test's actor");
    cJSON_Delete(last);
    cJSON *envelope = NULL;
    check(workshop_material_version_envelope(motif, 2, &envelope) == URSATZ_OK && strcmp(str(envelope, "actor"), "test") == 0,
          "the version's envelope records the command's actor");
    cJSON_Delete(envelope);
    check(command_as("claude", "item.save", motif, params, 2, NULL, &reply, &error) == URSATZ_OK, "a command of another actor saves a version");
    workshop_reply_destroy(&reply);
    check(workshop_material_version_envelope(motif, 3, &envelope) == URSATZ_OK && strcmp(str(envelope, "actor"), "claude") == 0,
          "and the envelope names that actor");
    cJSON_Delete(envelope);

    snprintf(params, sizeof params, "{\"id\":\"%s\"}", motif);
    check(query("item.get", params, &reply, &error) == URSATZ_OK, "item.get answers");
    const cJSON *versions = member(member(reply.json, "item"), "versions");
    check(cJSON_GetArraySize(versions) == 3 && strcmp(str(cJSON_GetArrayItem(versions, 1), "actor"), "test") == 0 &&
              strcmp(str(cJSON_GetArrayItem(versions, 2), "actor"), "claude") == 0 && member(cJSON_GetArrayItem(versions, 0), "lineage") != NULL,
          "item.get's versions carry actor and lineage");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"version\":3}", motif);
    check(query("item.version", params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "actor"), "claude") == 0 && member(reply.json, "lineage") != NULL,
          "item.version carries the actor and the lineage of its version");
    workshop_reply_destroy(&reply);
}

int main(void) {
    if (!harness_open(18200)) {
        return 1;
    }
    test_parity();
    test_check_kinds();
    test_through_command();
    test_actor();
    harness_close();
    return test_finish("workshop_command_schemas");
}
