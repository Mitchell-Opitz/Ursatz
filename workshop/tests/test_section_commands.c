#include <stdarg.h>

#include "api_harness.h"

#include <ursatz_test.h>

#include "composition/sections.h"

/*
 * The section commands (R-666): the pure edits of a piece request on the game loop fixture (every command, every refusal at its path), then the
 * commands on a migrated copy of the layout 0 fixture folder: a version made, journaled with the actor, a stale base refused, the schema's limits, the
 * Composer's refusal at its path, an overlay on a form piece. Runs in its own working directory.
 */

#ifndef TEXT_LOOP_REQUEST
#error "TEXT_LOOP_REQUEST must name composer/tests/fixtures/piece/structure-only.json"
#endif
#ifndef TEXT_DATA_V0
#error "TEXT_DATA_V0 must name the layout 0 fixture folder"
#endif

#define LOOP_PIECE "pce-2c8d54834a9c499f"
#define FORM_PIECE "pce-0821ac7670440f33"

/* The ids of the sections of a request, as "a,b,c". */
static void ids_text(const cJSON *request, char *out, size_t size) {
    cJSON *ids = workshop_sections_ids(request);
    out[0] = '\0';
    const cJSON *id;
    cJSON_ArrayForEach(id, ids) {
        snprintf(out + strlen(out), size - strlen(out), "%s%s", out[0] ? "," : "", id->valuestring);
    }
    cJSON_Delete(ids);
}

static const cJSON *section_of(const cJSON *request, const char *id) {
    const cJSON *section;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        if (strcmp(str(section, "id"), id) == 0 || strcmp(str(section, "name"), id) == 0) {
            return section;
        }
    }
    return NULL;
}

/* Runs an edit: the status, with the edited request (NULL on a refusal), the note and the error. */
static UrsatzStatus edit(const char *command, const cJSON *request, const char *params_text, cJSON **edited, char *note, UrsatzError *error) {
    cJSON *params = cJSON_Parse(params_text);
    memset(error, 0, sizeof *error);
    UrsatzStatus status = workshop_sections_edit(command, request, params, edited, note, 200, error);
    cJSON_Delete(params);
    return status;
}

static void expect_ids(const cJSON *request, const char *wanted, const char *what) {
    char ids[200];
    ids_text(request, ids, sizeof ids);
    CHECKF(strcmp(ids, wanted) == 0, "%s: wanted %s, got %s", what, wanted, ids);
}

static void expect_refusal(const char *command, const cJSON *request, const char *params, UrsatzStatus status, const char *path, const char *message) {
    cJSON *edited = NULL;
    UrsatzError error;
    char note[200];
    UrsatzStatus got = edit(command, request, params, &edited, note, &error);
    CHECKF(got == status && edited == NULL && strcmp(error.path, path) == 0 && strcmp(error.message, message) == 0, "%s %s: wanted %d at %s '%s', got %d at %s '%s'",
           command, params, (int)status, path, message, (int)got, error.path, error.message);
    cJSON_Delete(edited);
}

static void test_pure(void) {
    char *text = test_must_read_file(TEXT_LOOP_REQUEST, NULL);
    cJSON *loop = cJSON_Parse(text);
    free(text);
    char ids[200];
    ids_text(loop, ids, sizeof ids);
    check(strcmp(ids, "intro,a,b,t") == 0 || ids[0] != '\0', "the loop fixture has sections");
    char first[40], second[40];
    snprintf(first, sizeof first, "%s", cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(loop, "sections"), 0)->child ? str(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(loop, "sections"), 0), "id") : "");
    snprintf(second, sizeof second, "%s", str(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(loop, "sections"), 1), "id"));
    cJSON *edited = NULL;
    UrsatzError error;
    char note[200];
    char params[400];

    /* insert after and before */
    snprintf(params, sizeof params, "{\"id\":\"T0\",\"after\":\"%s\",\"bars\":4,\"role\":\"body\",\"letter\":\"T\",\"function\":\"transition\",\"pickup\":\"1/2\",\"cadence\":\"HC\",\"fill\":\"empty\"}", first);
    check(edit("section.insert", loop, params, &edited, note, &error) == URSATZ_OK, "section.insert after a section");
    char wanted[200];
    snprintf(wanted, sizeof wanted, "%s,T0,%s", first, ids + strlen(first) + 1);
    expect_ids(edited, wanted, "insert after");
    const cJSON *inserted = section_of(edited, "T0");
    char *written = inserted ? cJSON_PrintUnformatted(inserted) : NULL;
    check(written && strcmp(written, "{\"id\":\"T0\",\"role\":\"body\",\"letter\":\"T\",\"bars\":4,\"pickup\":\"1/2\",\"cadence\":\"HC\",\"function\":\"transition\",\"fill\":{\"kind\":\"empty\"}}") == 0,
          "the inserted section is written in the order of the request");
    free(written);
    check(strcmp(note, "insert section T0 after ") != 0 && strncmp(note, "insert section T0 after ", 24) == 0, "the default note names the insert");
    cJSON *again = NULL;
    snprintf(params, sizeof params, "{\"id\":\"U\",\"before\":\"%s\",\"beats\":6,\"note\":\"my note\"}", first);
    check(edit("section.insert", edited, params, &again, note, &error) == URSATZ_OK && strcmp(note, "my note") == 0, "section.insert before, with a note");
    snprintf(wanted, sizeof wanted, "U,%s,T0,%s", first, ids + strlen(first) + 1);
    expect_ids(again, wanted, "insert before");
    check(section_of(again, "U") && cJSON_IsNumber(member(section_of(again, "U"), "beats")) && !member(section_of(again, "U"), "bars") &&
              strcmp(str(section_of(again, "U"), "role"), "body") == 0,
          "beats instead of bars; the role defaults to body");
    cJSON_Delete(again);
    check(member(loop, "sections") && cJSON_GetArraySize(member(loop, "sections")) == cJSON_GetArraySize(member(edited, "sections")) - 1,
          "an edit leaves the request it was given as it was");

    /* resize: the unit given, the other removed */
    cJSON *resized = NULL;
    snprintf(params, sizeof params, "{\"id\":\"T0\",\"beats\":12}");
    check(edit("section.resize", edited, params, &resized, note, &error) == URSATZ_OK && strcmp(note, "resize section T0 to 12 beats") == 0, "section.resize");
    check(cJSON_IsNumber(member(section_of(resized, "T0"), "beats")) && !member(section_of(resized, "T0"), "bars"), "resize writes the unit given and removes the other");
    cJSON_Delete(resized);

    /* move */
    cJSON *moved = NULL;
    snprintf(params, sizeof params, "{\"id\":\"T0\",\"before\":\"%s\"}", first);
    check(edit("section.move", edited, params, &moved, note, &error) == URSATZ_OK, "section.move before the first section");
    snprintf(wanted, sizeof wanted, "T0,%s,%s", first, ids + strlen(first) + 1);
    expect_ids(moved, wanted, "move");
    cJSON_Delete(moved);
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"after\":\"T0\"}", first);
    check(edit("section.move", edited, params, &moved, note, &error) == URSATZ_OK, "section.move after another");
    snprintf(wanted, sizeof wanted, "T0,%s,%s", first, ids + strlen(first) + 1);
    expect_ids(moved, wanted, "move after");
    cJSON_Delete(moved);

    /* delete down to the last section */
    cJSON *deleted = NULL;
    check(edit("section.delete", edited, "{\"id\":\"T0\"}", &deleted, note, &error) == URSATZ_OK && strcmp(note, "delete section T0") == 0, "section.delete");
    expect_ids(deleted, ids, "delete");
    cJSON_Delete(deleted);

    /* set: the members, the words that take one away, a section that says part keeps it */
    cJSON *set = NULL;
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"role\":\"ending\",\"letter\":\"Z\",\"function\":\"coda\",\"cadence\":\"PAC\",\"pickup\":\"1\",\"fill\":\"motif\","
                                    "\"region\":{\"relation\":\"omt.relation.dominant\",\"anchor\":\"home\"},\"harmony\":{\"progression\":\"I V\"}}", first);
    check(edit("section.set", loop, params, &set, note, &error) == URSATZ_OK && strncmp(note, "set role, letter, function, cadence, pickup, fill, region, harmony of section ", 78) == 0,
          "section.set of every member");
    const cJSON *s = section_of(set, first);
    check(s && strcmp(str(s, "part"), "ending") == 0 && !member(s, "role") && strcmp(str(s, "letter"), "Z") == 0 && strcmp(str(s, "function"), "coda") == 0 &&
              strcmp(str(s, "cadence"), "PAC") == 0 && strcmp(str(s, "pickup"), "1") == 0 && strcmp(str(member(s, "fill"), "kind"), "motif") == 0 &&
              strcmp(str(member(s, "region"), "relation"), "omt.relation.dominant") == 0 && strcmp(str(member(s, "harmony"), "progression"), "I V") == 0,
          "set writes each member (a section that says part keeps saying it)");
    cJSON *cleared = NULL;
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"function\":\"none\",\"cadence\":\"none\",\"region\":\"home\",\"harmony\":\"none\"}", first);
    check(edit("section.set", set, params, &cleared, note, &error) == URSATZ_OK, "set with the words none and home");
    s = section_of(cleared, first);
    check(s && !member(s, "function") && !member(s, "cadence") && !member(s, "region") && !member(s, "harmony"), "none and home take the member away");
    cJSON_Delete(cleared);
    cJSON_Delete(set);
    cJSON_Delete(edited);

    /* every refusal at its path */
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"after\":\"%s\",\"bars\":2}", first, first);
    char taken[80];
    snprintf(taken, sizeof taken, "section '%s' exists", first);
    expect_refusal("section.insert", loop, params, URSATZ_ERR_CONFLICT, "/params/id", taken);
    snprintf(params, sizeof params, "{\"id\":\"zz\",\"after\":\"nope\",\"bars\":2}");
    expect_refusal("section.insert", loop, params, URSATZ_ERR_NOT_FOUND, "/params/after", "unknown section 'nope'");
    snprintf(params, sizeof params, "{\"id\":\"zz\",\"before\":\"nope\",\"bars\":2}");
    expect_refusal("section.insert", loop, params, URSATZ_ERR_NOT_FOUND, "/params/before", "unknown section 'nope'");
    expect_refusal("section.delete", loop, "{\"id\":\"nope\"}", URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section 'nope'");
    expect_refusal("section.move", loop, "{\"id\":\"nope\",\"after\":\"nope\"}", URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section 'nope'");
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"after\":\"nope\"}", first);
    expect_refusal("section.move", loop, params, URSATZ_ERR_NOT_FOUND, "/params/after", "unknown section 'nope'");
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"before\":\"%s\"}", first, first);
    expect_refusal("section.move", loop, params, URSATZ_ERR_REQUEST, "/params/before", "a section cannot move next to itself");
    expect_refusal("section.resize", loop, "{\"id\":\"nope\",\"bars\":2}", URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section 'nope'");
    expect_refusal("section.set", loop, "{\"id\":\"nope\",\"letter\":\"A\"}", URSATZ_ERR_NOT_FOUND, "/params/id", "unknown section 'nope'");
    cJSON *single = cJSON_Parse("{\"schema_version\":1,\"sections\":[{\"id\":\"only\",\"part\":\"body\",\"beats\":8}]}");
    expect_refusal("section.delete", single, "{\"id\":\"only\"}", URSATZ_ERR_REQUEST, "/params/id", "a piece keeps at least one section");
    cJSON_Delete(single);

    /* a form piece takes its sections from its form: only the overlay members of set */
    cJSON *form = cJSON_Parse("{\"schema_version\":2,\"form\":\"ternary\",\"sections\":[{\"name\":\"A\",\"region\":\"home\"}]}");
    expect_refusal("section.insert", form, "{\"id\":\"x\",\"after\":\"A\",\"bars\":2}", URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    expect_refusal("section.delete", form, "{\"id\":\"A\"}", URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    expect_refusal("section.move", form, "{\"id\":\"A\",\"after\":\"A\"}", URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    expect_refusal("section.resize", form, "{\"id\":\"A\",\"bars\":2}", URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    expect_refusal("section.set", form, "{\"id\":\"A\",\"letter\":\"Q\"}", URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    expect_refusal("section.set", form, "{\"id\":\"A\",\"note\":\"x\"}", URSATZ_ERR_REQUEST, "/target", "a form piece takes its sections from its form");
    cJSON *overlay = NULL;
    check(edit("section.set", form, "{\"id\":\"A\",\"region\":{\"relation\":\"omt.relation.dominant\",\"anchor\":\"home\"},\"harmony\":{\"progression\":\"I V\"}}", &overlay, note, &error) == URSATZ_OK,
          "section.set of region and harmony on a form piece");
    check(cJSON_GetArraySize(member(overlay, "sections")) == 1 && strcmp(str(member(section_of(overlay, "A"), "region"), "relation"), "omt.relation.dominant") == 0 &&
              strcmp(str(member(section_of(overlay, "A"), "harmony"), "progression"), "I V") == 0,
          "the entry with that name takes them");
    cJSON *made = NULL;
    check(edit("section.set", overlay, "{\"id\":\"B\",\"fill\":\"empty\"}", &made, note, &error) == URSATZ_OK && cJSON_GetArraySize(member(made, "sections")) == 2 &&
              strcmp(str(section_of(made, "B"), "name"), "B") == 0 && strcmp(str(member(section_of(made, "B"), "fill"), "kind"), "empty") == 0,
          "an entry is made when the form's section has none");
    cJSON_Delete(made);
    cJSON_Delete(overlay);
    cJSON_Delete(form);
    cJSON_Delete(loop);
    expect_refusal("section.insert", NULL, "{}", URSATZ_ERR_REQUEST, "/target", "the piece has no request to edit");
}

static void sh(const char *format, ...) __attribute__((format(printf, 1, 2)));

static void sh(const char *format, ...) {
    char command_text[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(command_text, sizeof(command_text), format, args);
    va_end(args);
    if (system(command_text) != 0) {
        fprintf(stderr, "command failed: %s\n", command_text);
    }
}

static bool open_copy(void) {
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd)) || strstr(cwd, "/workshop/tests/") == NULL) {
        return false;
    }
    sh("rm -rf ./* ./.[!.]* && cp -r '%s'/. . && rm -f .pre-overhaul-checked.lock", TEXT_DATA_V0);
    UrsatzError error = {{0}, {0}};
    cJSON *report = NULL;
    WorkshopMigrateOptions options = {false, NULL};
    if (workshop_migrate(".", &options, &report, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot migrate: %s\n", error.message);
        return false;
    }
    cJSON_Delete(report);
    sh("rm -rf ./*-backup-*");
    WorkshopConfig flags = {.data_dir = ".", .config_path = NULL, .port = 0, .http_threads = 2, .job_workers = 2};
    WorkshopConfig resolved;
    char storage[4096];
    if (workshop_config_resolve(&flags, &resolved, storage, sizeof(storage), &error) != URSATZ_OK ||
        workshop_open(&resolved, &g_workshop, &error) != URSATZ_OK || workshop_start(g_workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the workshop: %s\n", error.message);
        return false;
    }
    return true;
}

static void version_ids(const char *id, int version, char *out, size_t size) {
    char params[200];
    snprintf(params, sizeof params, "{\"id\":\"%s\",\"version\":%d}", id, version);
    WorkshopReply reply;
    UrsatzError error;
    out[0] = '\0';
    if (query("item.version", params, &reply, &error) == URSATZ_OK) {
        ids_text(member(member(reply.json, "body"), "request"), out, size);
    }
    workshop_reply_destroy(&reply);
}

static void test_commands(void) {
    WorkshopReply reply;
    UrsatzError error;
    char ids[200];
    version_ids(LOOP_PIECE, 2, ids, sizeof ids);
    check(strcmp(ids, "s0") == 0, "the fixture piece has the section s0");

    check(command("section.insert", LOOP_PIECE, "{\"id\":\"T0\",\"after\":\"s0\",\"bars\":4,\"role\":\"body\",\"letter\":\"T\",\"function\":\"transition\"}", 2, &reply, &error) == URSATZ_OK &&
              num(reply.json, "version") == 3 && cJSON_GetArraySize(member(reply.json, "sections")) == 2 && strcmp(str(cJSON_GetArrayItem(member(reply.json, "sections"), 1), "x"), "") == 0,
          "section.insert makes version 3");
    char *sections = cJSON_PrintUnformatted(member(reply.json, "sections"));
    check(sections && strcmp(sections, "[\"s0\",\"T0\"]") == 0, "the reply lists the sections in order");
    free(sections);
    workshop_reply_destroy(&reply);
    version_ids(LOOP_PIECE, 3, ids, sizeof ids);
    check(strcmp(ids, "s0,T0") == 0, "version 3 has the sections s0 and T0");
    version_ids(LOOP_PIECE, 2, ids, sizeof ids);
    check(strcmp(ids, "s0") == 0, "version 2 is as it was");
    cJSON *record = last_record();
    check(strcmp(str(record, "command"), "section.insert") == 0 && strcmp(str(record, "actor"), "test") == 0 && strcmp(str(record, "target"), LOOP_PIECE) == 0 &&
              num(record, "base_version") == 2,
          "it is journaled with the actor and the base version");
    cJSON_Delete(record);
    WorkshopReply version;
    check(query("item.version", "{\"id\":\"" LOOP_PIECE "\",\"version\":3}", &version, &error) == URSATZ_OK && strcmp(str(version.json, "actor"), "test") == 0 &&
              !member(member(version.json, "body"), "result") && member(member(version.json, "body"), "request") != NULL,
          "the new version is a piece body with no result, made by the actor");
    workshop_reply_destroy(&version);
    check(query("item.get", "{\"id\":\"" LOOP_PIECE "\"}", &version, &error) == URSATZ_OK &&
              strcmp(str(cJSON_GetArrayItem(member(member(version.json, "item"), "versions"), 2), "note"), "insert section T0 after s0") == 0,
          "its note says what changed");
    workshop_reply_destroy(&version);

    /* a stale base is refused with the current version */
    check(command("section.insert", LOOP_PIECE, "{\"id\":\"U\",\"after\":\"s0\",\"bars\":2}", 2, &reply, &error) == URSATZ_ERR_CONFLICT && strcmp(error.path, "/base_version") == 0 &&
              num(reply.json, "current_version") == 3,
          "a stale base version is refused with current_version");
    workshop_reply_destroy(&reply);

    /* the schema's limits and the Composer's refusal at its path */
    check(command("section.insert", LOOP_PIECE, "{\"id\":\"U\",\"after\":\"s0\",\"bars\":0}", 3, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/bars") == 0 &&
              strcmp(error.message, "bars must be from 1 to 128") == 0,
          "a section with bars 0 is refused by the schema");
    workshop_reply_destroy(&reply);
    check(command("section.insert", LOOP_PIECE, "{\"id\":\"U\",\"after\":\"s0\",\"bars\":2,\"beats\":8}", 3, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/bars") == 0 &&
              strcmp(error.message, "give exactly one of bars, beats") == 0,
          "bars and beats together are refused");
    workshop_reply_destroy(&reply);
    check(command("section.insert", LOOP_PIECE, "{\"id\":\"U\",\"bars\":2}", 3, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/after") == 0, "after or before is needed");
    workshop_reply_destroy(&reply);
    check(command("section.insert", LOOP_PIECE, "{\"id\":\"s0\",\"after\":\"s0\",\"bars\":2}", 3, &reply, &error) == URSATZ_ERR_CONFLICT && strcmp(error.path, "/params/id") == 0,
          "an id taken is a conflict");
    workshop_reply_destroy(&reply);
    check(command("section.insert", LOOP_PIECE, "{\"id\":\"U\",\"after\":\"nope\",\"bars\":2}", 3, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/after") == 0,
          "an unknown section is not found at its param");
    workshop_reply_destroy(&reply);
    check(command("section.set", LOOP_PIECE, "{\"id\":\"s0\",\"region\":{\"relation\":\"omt.relation.nope\",\"anchor\":\"home\"}}", 3, &reply, &error) == URSATZ_ERR_REQUEST &&
              strcmp(error.path, "/request/sections/0/region") == 0,
          "a region the Composer refuses keeps its path into the request");
    workshop_reply_destroy(&reply);
    version_ids(LOOP_PIECE, 3, ids, sizeof ids);
    check(strcmp(ids, "s0,T0") == 0, "a refusal makes no version");
    check(command("section.delete", "mot-4ff8d7b3cd1fb440", "{\"id\":\"s0\"}", 1, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/target") == 0 &&
              strcmp(error.message, "section commands change a piece") == 0,
          "section commands change a piece");
    workshop_reply_destroy(&reply);
    check(command("section.move", LOOP_PIECE, "{\"id\":\"T0\",\"before\":\"s0\"}", 3, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 4, "section.move makes version 4");
    workshop_reply_destroy(&reply);
    version_ids(LOOP_PIECE, 4, ids, sizeof ids);
    check(strcmp(ids, "T0,s0") == 0, "T0 now comes before s0");
    check(command("section.resize", LOOP_PIECE, "{\"id\":\"T0\",\"beats\":8}", 4, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 5, "section.resize makes version 5");
    workshop_reply_destroy(&reply);
    check(command("section.set", LOOP_PIECE, "{\"id\":\"s0\",\"letter\":\"A\",\"cadence\":\"PAC\"}", 5, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 6, "section.set makes version 6");
    workshop_reply_destroy(&reply);
    check(command("section.delete", LOOP_PIECE, "{\"id\":\"T0\"}", 6, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 7, "section.delete makes version 7");
    workshop_reply_destroy(&reply);
    version_ids(LOOP_PIECE, 7, ids, sizeof ids);
    check(strcmp(ids, "s0") == 0, "version 7 has the section s0 again");

    /* a form piece: set of region writes an overlay; the others are refused */
    check(command("section.insert", FORM_PIECE, "{\"id\":\"x\",\"after\":\"A\",\"bars\":2}", 2, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/target") == 0 &&
              strcmp(error.message, "a form piece takes its sections from its form") == 0,
          "section.insert on a form piece is refused");
    workshop_reply_destroy(&reply);
    check(command("section.set", FORM_PIECE, "{\"id\":\"B\",\"region\":{\"relation\":\"omt.relation.subdominant\",\"anchor\":\"A\"}}", 2, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 3,
          "section.set of a region on a form piece makes a version");
    workshop_reply_destroy(&reply);
    check(query("item.version", "{\"id\":\"" FORM_PIECE "\",\"version\":3}", &reply, &error) == URSATZ_OK, "the form piece's new version reads");
    const cJSON *overlay = section_of(member(member(reply.json, "body"), "request"), "B");
    check(overlay && strcmp(str(member(overlay, "region"), "relation"), "omt.relation.subdominant") == 0, "its overlay entry for B has the region");
    workshop_reply_destroy(&reply);
}

int main(void) {
    test_pure();
    if (!open_copy()) {
        return 1;
    }
    test_commands();
    harness_close();
    return test_finish("workshop_section_commands");
}
