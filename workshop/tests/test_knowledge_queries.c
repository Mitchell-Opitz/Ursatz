/* The knowledge queries (R-765): knowledge.find, knowledge.show, knowledge.check and knowledge.palette on a workshop with a data folder. Runs in its own
 * working directory. */

#include "api_harness.h"

#include <assert.h>

#include <common/files.h>

#include <ursatz_test.h>

#ifndef URSATZ_USER_KNOWLEDGE_FIXTURE
#error "URSATZ_USER_KNOWLEDGE_FIXTURE must name her voicings fixture"
#endif

static bool entry_listed(const cJSON *reply, const char *id) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(reply, "entries")) {
        if (strcmp(str(entry, "id"), id) == 0) {
            return true;
        }
    }
    return false;
}

static void test_find(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(query("knowledge.find", "{\"words\": \"mario\"}", &reply, &error) == URSATZ_OK && entry_listed(reply.json, "omt.schema.aeolian-cadence") &&
              !cJSON_IsTrue(member(reply.json, "more")),
          "find mario gives the aeolian cadence (an alias holds the word)");
    workshop_reply_destroy(&reply);
    check(query("knowledge.find", "{\"words\": \"shuttle\", \"kind\": \"schema\"}", &reply, &error) == URSATZ_OK &&
              cJSON_GetArraySize(member(reply.json, "entries")) == 4 && entry_listed(reply.json, "omt.schema.dorian-shuttle"),
          "find shuttle among the schemas gives four");
    const cJSON *first = cJSON_GetArrayItem(member(reply.json, "entries"), 0);
    check(strcmp(str(first, "kind"), "schema") == 0 && strcmp(str(first, "subtype"), "progression") == 0 && !cJSON_IsTrue(member(first, "user")) &&
              str(first, "status")[0] != '\0',
          "an entry row has its kind, subtype, status and user flag");
    workshop_reply_destroy(&reply);
    check(query("knowledge.find", "{\"words\": \"DORIAN shuttle\"}", &reply, &error) == URSATZ_OK && entry_listed(reply.json, "omt.schema.dorian-shuttle"),
          "every word must be held, case ignored");
    workshop_reply_destroy(&reply);
    check(query("knowledge.find", "{\"words\": \"\"}", &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "entries")) == 50 &&
              cJSON_IsTrue(member(reply.json, "more")),
          "no words match every entry, the first 50 and more");
    workshop_reply_destroy(&reply);
    check(query("knowledge.find", "{\"words\": \"x\", \"kind\": \"nothing\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "an unknown kind is not found");
    check(query("knowledge.find", "{}", &reply, &error) == URSATZ_ERR_REQUEST, "words are required");
}

static void test_show(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(query("knowledge.show", "{\"id\": \"omt.vocabulary.cadential\", \"view\": \"json\"}", &reply, &error) == URSATZ_OK &&
              strcmp(str(member(reply.json, "entry"), "id"), "omt.vocabulary.cadential") == 0 && strcmp(str(member(reply.json, "entry"), "kind"), "vocabulary") == 0,
          "show the json view: the entry as loaded");
    workshop_reply_destroy(&reply);
    check(query("knowledge.show", "{\"id\": \"omt.vocabulary.cadential\", \"view\": \"text\"}", &reply, &error) == URSATZ_OK &&
              strstr(str(reply.json, "text"), "ursatz-text 1 knowledge-entry") != NULL && strstr(str(reply.json, "text"), "id omt.vocabulary.cadential") != NULL,
          "show the text view: Ursatz Text of the entry");
    workshop_reply_destroy(&reply);
    check(query("knowledge.show", "{\"id\": \"omt.vocabulary.cadential\"}", &reply, &error) == URSATZ_OK && member(reply.json, "entry") != NULL,
          "the default view is json");
    workshop_reply_destroy(&reply);
    check(query("knowledge.show", "{\"id\": \"omt.vocabulary.nothing\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/id") == 0,
          "an unknown id is not found");
    check(query("knowledge.show", "{\"id\": \"omt.vocabulary.cadential\", \"view\": \"pdf\"}", &reply, &error) == URSATZ_ERR_REQUEST, "an unknown view is refused");
}

static void test_check_file(void) {
    WorkshopReply reply;
    UrsatzError error;
    char *fixture = NULL;
    assert(common_file_read(URSATZ_USER_KNOWLEDGE_FIXTURE, &fixture, NULL, &error) == URSATZ_OK);
    char params[8192];
    snprintf(params, sizeof(params), "{\"file\": %s}", fixture);
    check(query("knowledge.check", params, &reply, &error) == URSATZ_OK && cJSON_IsTrue(member(reply.json, "ok")) && num(reply.json, "entries") == 2,
          "her voicings document loads: ok, 2 entries");
    workshop_reply_destroy(&reply);
    free(fixture);
    static const char BROKEN[] =
        "{\"file\": {\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[{\"id\":\"user.voicing.broken\","
        "\"kind\":\"voicing\",\"name\":\"broken\",\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0}],"
        "\"fits\":[\"omt.chord-type.no-such-type\"]},\"citations\":[],\"status\":\"active\",\"version\":1}]}}";
    check(query("knowledge.check", BROKEN, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/file") == 0 &&
              strstr(error.message, "user.voicing.broken") != NULL && strstr(error.message, "content.members") != NULL,
          "a broken entry is refused at /file with the loader's message");
    check(query("knowledge.check", "{\"file\": []}", &reply, &error) == URSATZ_ERR_REQUEST, "the file is an object");
    check(query("knowledge.check", "{\"file\": {\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[{\"id\":\"omt.material.major\","
                                   "\"kind\":\"voicing\",\"name\":\"x\"}]}}", &reply, &error) == URSATZ_ERR_REQUEST,
          "an id of the built-in namespace is refused");
}

static void test_palette(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(query("knowledge.palette", "{\"key\": \"C minor\", \"with\": \"dorian\"}", &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "materials")) == 2 &&
              cJSON_GetArraySize(member(reply.json, "shared")) == 4 && strcmp(str(reply.json, "key"), "C minor") == 0,
          "palette answers the Composer's document");
    workshop_reply_destroy(&reply);
    check(query("knowledge.palette", "{\"key\": \"C minor\", \"with\": \"minor\"}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/with") == 0,
          "palette refuses the key's own mode at /with");
    check(query("knowledge.palette", "{}", &reply, &error) == URSATZ_ERR_REQUEST, "palette needs a key");
}

int main(void) {
    if (!harness_open(18996)) {
        return 1;
    }
    test_find();
    test_show();
    test_check_file();
    test_palette();
    harness_close();
    return test_finish("workshop_knowledge_queries");
}
