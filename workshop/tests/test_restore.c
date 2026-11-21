#include <dirent.h>
#include <stdarg.h>

#include "api_harness.h"

#include <sys/stat.h>

#include <ursatz_test.h>

#include "items/store.h"

/*
 * Restore a version (R-667): on a migrated copy of the layout 0 fixture folder, version 2 of a composed piece restored after another edit makes the next
 * version with its files and its body (the paths of its result moved to the new folder), an envelope that says restored_from and the actor, and a
 * version that regenerates; the current version, a version that is not there and a stale base are refused.
 */

#ifndef TEXT_DATA_V0
#error "TEXT_DATA_V0 must name the layout 0 fixture folder"
#endif

#define PIECE "pce-2c8d54834a9c499f"

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

/* The path of a version folder: projects/<project>/items/<id>/v<n>, found by looking for the item's folder. */
static bool version_folder(int version, char *out, size_t size) {
    char command_text[400];
    snprintf(command_text, sizeof command_text, "ls -d projects/*/items/" PIECE " > restore_item_dir.txt 2>/dev/null");
    if (system(command_text) != 0) {
        return false;
    }
    char *dir = test_read_file("restore_item_dir.txt", NULL);
    if (!dir) {
        return false;
    }
    dir[strcspn(dir, "\n")] = '\0';
    snprintf(out, size, "%s/v%d", dir, version);
    free(dir);
    return true;
}

static int file_names(const char *folder, char names[64][256]) {
    int count = 0;
    DIR *dir = opendir(folder);
    struct dirent *entry;
    while (dir && (entry = readdir(dir)) != NULL && count < 64) {
        if (entry->d_name[0] != '.') {
            snprintf(names[count++], 256, "%s", entry->d_name);
        }
    }
    if (dir) {
        closedir(dir);
    }
    return count;
}

static bool same_bytes(const char *a, const char *b) {
    size_t size_a = 0, size_b = 0;
    char *data_a = test_read_file(a, &size_a);
    char *data_b = test_read_file(b, &size_b);
    bool same = data_a && data_b && size_a == size_b && memcmp(data_a, data_b, size_a) == 0;
    free(data_a);
    free(data_b);
    return same;
}

/* Replaces every occurrence of from by to in text (a new string). */
static char *replaced(const char *text, const char *from, const char *to) {
    char *out = malloc(strlen(text) * 2 + strlen(to) * 64 + 16);
    char *w = out;
    size_t n = strlen(from);
    for (const char *p = text; *p;) {
        if (strncmp(p, from, n) == 0) {
            w += sprintf(w, "%s", to);
            p += n;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return out;
}

int main(void) {
    if (!open_copy()) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;

    /* version 3 is an edit, version 4 a composed one (its result names the files of its own folder), version 5 another edit */
    check(command("section.set", PIECE, "{\"id\":\"s0\",\"cadence\":\"PAC\"}", 2, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 3, "an edit makes version 3");
    workshop_reply_destroy(&reply);
    check(command("item.compose", PIECE, "{\"seed\":1}", 3, &reply, &error) == URSATZ_OK, "item.compose is accepted");
    cJSON *composed = finish_job(&reply);
    check(composed && num(composed, "http_status") == 200, "and makes version 4");
    cJSON_Delete(composed);
    workshop_reply_destroy(&reply);
    check(command("section.set", PIECE, "{\"id\":\"s0\",\"cadence\":\"HC\"}", 4, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 5, "another edit makes version 5");
    workshop_reply_destroy(&reply);

    check(command("item.restore", PIECE, "{\"version\":4}", 5, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 6 && num(reply.json, "restored_from") == 4,
          "item.restore of version 4 makes version 6");
    workshop_reply_destroy(&reply);
    cJSON *record = last_record();
    check(strcmp(str(record, "command"), "item.restore") == 0 && strcmp(str(record, "actor"), "test") == 0 && strcmp(str(record, "target"), PIECE) == 0 &&
              num(record, "base_version") == 5,
          "it is journaled with the actor and the base version");
    cJSON_Delete(record);

    /* the files: every file of version 4 but its envelope and body is in version 6, byte for byte, and nothing else */
    char v4[400], v6[400];
    check(version_folder(4, v4, sizeof v4) && version_folder(6, v6, sizeof v6), "the version folders are found");
    char names[64][256];
    int count = file_names(v4, names);
    char after[64][256];
    int count_after = file_names(v6, after);
    check(count == count_after && count >= 8, "the new version holds the files of the old one and no other");
    int compared = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], "envelope.json") == 0 || strcmp(names[i], "body.json") == 0) {
            continue;
        }
        char a[700], b[700];
        snprintf(a, sizeof a, "%s/%s", v4, names[i]);
        snprintf(b, sizeof b, "%s/%s", v6, names[i]);
        struct stat info;
        if (stat(a, &info) == 0 && S_ISREG(info.st_mode)) {
            check_quiet(same_bytes(a, b), names[i]);
            compared++;
        } else {
            check_quiet(stat(b, &info) == 0 && S_ISDIR(info.st_mode), names[i]);
        }
    }
    check(compared >= 5, "the files of the old version are in the new one, byte for byte");

    /* the body: the old one with the folder of its result moved */
    char path_a[800], path_b[800];
    snprintf(path_a, sizeof path_a, "%s/body.json", v4);
    snprintf(path_b, sizeof path_b, "%s/body.json", v6);
    char *body_a = test_read_file(path_a, NULL);
    char *body_b = test_read_file(path_b, NULL);
    char abs_v4[900], abs_v6[900];
    char *cwd = getcwd(NULL, 0);
    snprintf(abs_v4, sizeof abs_v4, "%s/%s", cwd, v4);
    snprintf(abs_v6, sizeof abs_v6, "%s/%s", cwd, v6);
    free(cwd);
    char *moved = body_a ? replaced(body_a, abs_v4, abs_v6) : NULL;
    check(moved && body_b && strcmp(moved, body_b) == 0, "the new body is the old one, its result paths in the new folder");
    check(body_a && strstr(body_a, abs_v4) != NULL && body_b && strstr(body_b, abs_v4) == NULL && strstr(body_b, abs_v6) != NULL,
          "the old body names the old folder and the new one only its own");
    free(moved);
    free(body_a);
    free(body_b);

    /* the envelope */
    cJSON *envelope = NULL;
    check(workshop_material_version_envelope(PIECE, 6, &envelope) == URSATZ_OK, "the envelope reads");
    check(strcmp(str(envelope, "actor"), "test") == 0 && strcmp(str(envelope, "note"), "restore v4") == 0 && num(member(envelope, "lineage"), "restored_from") == 4,
          "it says restored_from, the actor and the default note");
    cJSON_Delete(envelope);

    /* a note of its own */
    check(command("item.restore", PIECE, "{\"version\":2,\"note\":\"back to the start\"}", 6, &reply, &error) == URSATZ_OK && num(reply.json, "version") == 7, "item.restore of version 2 with a note");
    workshop_reply_destroy(&reply);

    /* the restored version composes and regenerates */
    check(command("item.regenerate", PIECE, "{\"version\":6,\"section_id\":\"s0\"}", 7, &reply, &error) == URSATZ_OK, "item.regenerate of the restored version is accepted");
    cJSON *job = finish_job(&reply);
    check(job && num(job, "http_status") == 200, "and finishes: the plan and the request are there");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);

    /* the refusals */
    check(command("item.restore", PIECE, "{\"version\":8}", 8, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/version") == 0 &&
              strcmp(error.message, "version 8 is the current one") == 0,
          "the current version is refused at /params/version");
    workshop_reply_destroy(&reply);
    check(command("item.restore", PIECE, "{\"version\":99}", 8, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/version") == 0 &&
              strcmp(error.message, "no version 99") == 0,
          "a version that is not there is not found at /params/version");
    workshop_reply_destroy(&reply);
    check(command("item.restore", PIECE, "{\"version\":2}", 4, &reply, &error) == URSATZ_ERR_CONFLICT && strcmp(error.path, "/base_version") == 0 && num(reply.json, "current_version") == 8,
          "a stale base is refused with the current version");
    workshop_reply_destroy(&reply);
    check(command("item.restore", PIECE, "{}", 8, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/version") == 0 && strcmp(error.message, "version is required") == 0,
          "the schema asks for the version");
    workshop_reply_destroy(&reply);
    check(command("item.restore", "mot-nope", "{\"version\":1}", 1, &reply, &error) == URSATZ_ERR_NOT_FOUND || error.message[0] != '\0', "an item that is not there is refused");
    workshop_reply_destroy(&reply);
    harness_close();
    return test_finish("workshop_restore");
}
