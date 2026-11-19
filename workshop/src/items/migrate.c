#include "migrate.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include "base/knowledge.h"
#include "items/data_check.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/store.h"

#define PATH_BYTES 1024
#define BUILD_DIR ".migrate-1"
#define PRE_DIR ".pre-1"
#define COMPLETE_MARKER ".complete"

/* The top-level entries of layout 0 the migration moves aside, and the ones layout 1 puts in place. */
static const char *const kOldEntries[] = {"materials", "pieces", "subjects", "collections", "styles", "links"};
static const char *const kNewEntries[] = {WORKSHOP_DIR_PROJECTS, WORKSHOP_DIR_SUBJECTS, "library", "settings", WORKSHOP_DIR_CACHE};

typedef struct {
    bool dry;
    const char *root;
    char build[PATH_BYTES];
    UrsatzError *error;
    long projects;
    long items;
    long versions;
    long bodies_rewritten;
    long subjects;
    long pieces;
    long collections;
    long styles;
    long runs;
    long renders;
    long files;
    long long bytes;
    CommonNames known_projects;
} Run;

static bool in_list(const char *name, const char *const *list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

#define COUNT(list) (sizeof(list) / sizeof((list)[0]))

static UrsatzStatus fail(Run *run, UrsatzStatus status, const char *path, const char *format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    return ursatz_error_set(run->error, status, path, "%s", message);
}

static bool join(char *out, const char *a, const char *b) {
    return snprintf(out, PATH_BYTES, "%s/%s", a, b) < PATH_BYTES;
}

/* Makes the parent folder of path. */
static bool make_parent(const char *path) {
    char parent[PATH_BYTES];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (!slash) {
        return true;
    }
    *slash = '\0';
    return common_dir_make(parent, NULL) == URSATZ_OK;
}

/* --- building layout 1 ------------------------------------------------------------------------------------------------ */

/* Writes object (stamped as format, owned by the caller) as <build>/<relative>. */
static UrsatzStatus put_json(Run *run, const char *relative, const char *format, cJSON *object) {
    run->files++;
    if (run->dry) {
        return URSATZ_OK;
    }
    char path[PATH_BYTES];
    if (!join(path, run->build, relative) || !make_parent(path)) {
        return fail(run, URSATZ_ERR_IO, relative, "cannot make the folder of %s", relative);
    }
    UrsatzStatus written = workshop_format_write(path, format, object, run->error);
    return written;
}

static UrsatzStatus copy_file(Run *run, const char *source, const char *relative) {
    char *data = NULL;
    size_t size = 0;
    UrsatzStatus read = common_file_read(source, &data, &size, run->error);
    if (read != URSATZ_OK) {
        return read;
    }
    run->files++;
    run->bytes += (long long)size;
    UrsatzStatus written = URSATZ_OK;
    if (!run->dry) {
        char path[PATH_BYTES];
        if (!join(path, run->build, relative) || !make_parent(path)) {
            written = fail(run, URSATZ_ERR_IO, relative, "cannot make the folder of %s", relative);
        } else {
            written = common_file_write_atomic(path, data, size, run->error);
        }
    }
    free(data);
    return written;
}

/* Copies a folder with everything under it into <build>/<relative>, leaving out the entries skip names (top level only). */
static UrsatzStatus copy_tree(Run *run, const char *source, const char *relative, const char *const *skip, size_t skip_count) {
    CommonNames names = {0};
    UrsatzStatus status = common_dir_list(source, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char from[PATH_BYTES];
        char to[PATH_BYTES];
        if (in_list(names.items[i], skip, skip_count)) {
            continue;
        }
        if (!join(from, source, names.items[i]) || !join(to, relative, names.items[i])) {
            status = fail(run, URSATZ_ERR_IO, source, "a path under %s is too long", source);
            break;
        }
        struct stat info;
        if (lstat(from, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            status = copy_tree(run, from, to, NULL, 0);
        } else if (S_ISREG(info.st_mode)) {
            status = copy_file(run, from, to);
        }
    }
    common_names_free(&names);
    return status;
}

static UrsatzStatus read_object(Run *run, const char *path, cJSON **out) {
    UrsatzStatus status = common_json_read_file(path, out, run->error);
    if (status == URSATZ_OK && !cJSON_IsObject(*out)) {
        cJSON_Delete(*out);
        *out = NULL;
        return fail(run, URSATZ_ERR_FORMAT, path, "%s is not a JSON object", path);
    }
    return status;
}

static UrsatzStatus ensure_text(cJSON *object, const char *name, const char *fallback) {
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(object, name))) {
        cJSON_DeleteItemFromObjectCaseSensitive(object, name);
        if (!cJSON_AddStringToObject(object, name, fallback)) {
            return URSATZ_ERR_ALLOC;
        }
    }
    return URSATZ_OK;
}

static const char *id_from_file(const char *file, char *out, size_t size) {
    size_t length = strlen(file);
    if (length <= 5 || strcmp(file + length - 5, ".json") != 0 || length - 5 >= size) {
        return NULL;
    }
    memcpy(out, file, length - 5);
    out[length - 5] = '\0';
    return workshop_layout_id_ok(out) ? out : NULL;
}

static UrsatzStatus migrate_projects(Run *run) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (!join(dir, run->root, "materials/projects") || !common_path_is_dir(dir)) {
        return URSATZ_OK;
    }
    UrsatzStatus status = common_dir_list(dir, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char id[128];
        char path[PATH_BYTES];
        char relative[PATH_BYTES];
        if (!id_from_file(names.items[i], id, sizeof(id)) || !join(path, dir, names.items[i])) {
            continue;
        }
        cJSON *project = NULL;
        status = read_object(run, path, &project);
        if (status != URSATZ_OK) {
            break;
        }
        cJSON_DeleteItemFromObjectCaseSensitive(project, "id");
        cJSON_AddStringToObject(project, "id", id);
        if (ensure_text(project, "name", id) != URSATZ_OK || ensure_text(project, "description", "") != URSATZ_OK ||
            ensure_text(project, "created_at", "1970-01-01T00:00:00Z") != URSATZ_OK ||
            ensure_text(project, "updated_at", "1970-01-01T00:00:00Z") != URSATZ_OK ||
            !workshop_layout_project_file(id, relative, sizeof(relative))) {
            cJSON_Delete(project);
            status = fail(run, URSATZ_ERR_ALLOC, path, "out of memory");
            break;
        }
        status = put_json(run, relative, WORKSHOP_FORMAT_PROJECT, project);
        cJSON_Delete(project);
        run->projects++;
        common_names_add(&run->known_projects, id);
    }
    common_names_free(&names);
    return status;
}

/* The lineage of a version: the version of another item a motif body says it derives from. */
static cJSON *lineage_of(const char *type, const cJSON *body) {
    cJSON *lineage = cJSON_CreateObject();
    const cJSON *derived =
        strcmp(type, "motif") == 0 ? cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived") : NULL;
    const char *source = common_json_string(derived, "source_item");
    long long version = 0;
    cJSON *from = NULL;
    if (source[0] != '\0' && common_json_long(derived, "source_version", &version) && version >= 1) {
        from = cJSON_CreateObject();
        if (from) {
            cJSON_AddStringToObject(from, "item_id", source);
            cJSON_AddNumberToObject(from, "version", (double)version);
        }
    } else {
        from = cJSON_CreateNull();
    }
    if (!lineage || !from) {
        cJSON_Delete(lineage);
        cJSON_Delete(from);
        return NULL;
    }
    cJSON_AddItemToObject(lineage, "derived_from", from);
    return lineage;
}

static bool type_ok(const char *type) {
    return strcmp(type, "motif") == 0 || strcmp(type, "theme") == 0 || strcmp(type, "piece") == 0;
}

static const char *body_format_of(const char *type) {
    return strcmp(type, "motif") == 0   ? WORKSHOP_FORMAT_MOTIF_BODY
           : strcmp(type, "theme") == 0 ? WORKSHOP_FORMAT_THEME_BODY
                                        : WORKSHOP_FORMAT_PIECE_BODY;
}

/* The render cache of a motif version (items/<id>/v<n>/render/) becomes cache/renders/<id>/v<n>/. Its render.json named the Composer
 * build by ursatz_commit; layout 1 names the composition key, which an old cache cannot say, so the cache is made again on first use. */
static UrsatzStatus migrate_render(Run *run, const char *render_dir, const char *id, int number) {
    char relative[PATH_BYTES];
    char path[PATH_BYTES];
    if (!common_path_is_dir(render_dir) || !workshop_layout_render_dir(id, number, relative, sizeof(relative))) {
        return URSATZ_OK;
    }
    cJSON *meta = NULL;
    if (!join(path, render_dir, "render.json") || read_object(run, path, &meta) != URSATZ_OK) {
        return URSATZ_OK; /* a cache without its key never matched */
    }
    cJSON *render = cJSON_CreateObject();
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(meta, "tempo");
    bool ok = render && cJSON_AddStringToObject(render, "api_version", common_json_string(meta, "api_version")) &&
              cJSON_AddStringToObject(render, "composer_version", common_json_string(meta, "composer_version")) &&
              cJSON_AddStringToObject(render, "composition_key", "") &&
              cJSON_AddStringToObject(render, "render_key", common_json_string(meta, "render_key")) &&
              cJSON_AddNumberToObject(render, "tempo", cJSON_IsNumber(tempo) ? tempo->valuedouble : 0);
    cJSON_Delete(meta);
    UrsatzStatus status = ok ? URSATZ_OK : fail(run, URSATZ_ERR_ALLOC, render_dir, "out of memory");
    static const char *const kFiles[] = {"score.musicxml", "motif.mid"};
    for (size_t i = 0; status == URSATZ_OK && i < COUNT(kFiles); i++) {
        char from[PATH_BYTES];
        char to[PATH_BYTES];
        if (join(from, render_dir, kFiles[i]) && common_path_exists(from) && join(to, relative, kFiles[i])) {
            status = copy_file(run, from, to);
        }
    }
    if (status == URSATZ_OK) {
        char to[PATH_BYTES];
        status = join(to, relative, "render.json") ? put_json(run, to, WORKSHOP_FORMAT_RENDER, render)
                                                   : fail(run, URSATZ_ERR_IO, relative, "a path is too long");
        run->renders++;
    }
    cJSON_Delete(render);
    return status;
}

static UrsatzStatus migrate_item(Run *run, const char *items_dir, const char *id) {
    char dir[PATH_BYTES];
    char path[PATH_BYTES];
    if (!join(dir, items_dir, id) || !join(path, dir, "item.json") || !common_path_exists(path)) {
        return URSATZ_OK; /* a folder without item.json is not an item */
    }
    cJSON *item = NULL;
    UrsatzStatus status = read_object(run, path, &item);
    if (status != URSATZ_OK) {
        return status;
    }
    const char *type = common_json_string(item, "type");
    const char *project = common_json_string(item, "project_id");
    long long current = 0;
    if (!type_ok(type) || !workshop_layout_id_ok(project) || !common_json_long(item, "current_version", &current) ||
        current < 1) {
        cJSON_Delete(item);
        return fail(run, URSATZ_ERR_FORMAT, path, "%s is not an item of the GUI's store (type, project_id or current_version)", path);
    }
    if (!common_names_contains(&run->known_projects, project)) {
        /* an item names a project that has no file: the project is made, so the item has a place */
        char relative[PATH_BYTES];
        cJSON *made = cJSON_CreateObject();
        if (!made || !cJSON_AddStringToObject(made, "id", project) || !cJSON_AddStringToObject(made, "name", "Recovered") ||
            !cJSON_AddStringToObject(made, "description", "Made by the migration: its items named a project that had no file.") ||
            !cJSON_AddStringToObject(made, "created_at", "1970-01-01T00:00:00Z") ||
            !cJSON_AddStringToObject(made, "updated_at", "1970-01-01T00:00:00Z") ||
            !workshop_layout_project_file(project, relative, sizeof(relative))) {
            cJSON_Delete(made);
            cJSON_Delete(item);
            return fail(run, URSATZ_ERR_ALLOC, path, "out of memory");
        }
        status = put_json(run, relative, WORKSHOP_FORMAT_PROJECT, made);
        cJSON_Delete(made);
        run->projects++;
        common_names_add(&run->known_projects, project);
        if (status != URSATZ_OK) {
            cJSON_Delete(item);
            return status;
        }
    }

    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(item, "versions");
    for (int number = 1; status == URSATZ_OK && number <= (int)current; number++) {
        char body_path[PATH_BYTES];
        char version_dir[PATH_BYTES];
        char relative[PATH_BYTES];
        char name[64];
        snprintf(name, sizeof(name), "v%d.json", number);
        if (snprintf(version_dir, sizeof(version_dir), "%s/v%d", dir, number) >= (int)sizeof(version_dir) || !join(body_path, dir, name) ||
            !common_path_exists(body_path)) {
            continue;
        }
        cJSON *body = NULL;
        status = read_object(run, body_path, &body);
        if (status != URSATZ_OK) {
            break;
        }
        char *before = cJSON_PrintUnformatted(body);
        workshop_material_body_migrate(body);
        char *after = cJSON_PrintUnformatted(body);
        if (before && after && strcmp(before, after) != 0) {
            run->bodies_rewritten++;
        }
        free(before);
        free(after);
        const char *note = "";
        const char *created = common_json_string(item, "created_at");
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, versions) {
            long long entry_number = 0;
            if (common_json_long(entry, "number", &entry_number) && entry_number == number) {
                note = common_json_string(entry, "note");
                created = common_json_string(entry, "created_at");
            }
        }
        cJSON *envelope = cJSON_CreateObject();
        cJSON *lineage = lineage_of(type, body);
        bool ok = envelope && lineage && cJSON_AddStringToObject(envelope, "id", id) && cJSON_AddStringToObject(envelope, "kind", type) &&
                  cJSON_AddNumberToObject(envelope, "version", number) && cJSON_AddStringToObject(envelope, "note", note) &&
                  cJSON_AddItemToObject(envelope, "lineage", lineage) && cJSON_AddStringToObject(envelope, "actor", "michelle") &&
                  cJSON_AddStringToObject(envelope, "created_at", created);
        if (!ok) {
            if (envelope && !cJSON_GetObjectItemCaseSensitive(envelope, "lineage")) {
                cJSON_Delete(lineage);
            }
            cJSON_Delete(envelope);
            cJSON_Delete(body);
            status = fail(run, URSATZ_ERR_ALLOC, body_path, "out of memory");
            break;
        }
        if (workshop_layout_version_file(project, id, number, WORKSHOP_FILE_BODY, relative, sizeof(relative))) {
            status = put_json(run, relative, body_format_of(type), body);
        }
        /* the composed version's files first, the envelope last: a version exists once it has one */
        if (status == URSATZ_OK && common_path_is_dir(version_dir)) {
            char target[PATH_BYTES];
            static const char *const kSkip[] = {"render"};
            if (workshop_layout_version_dir(project, id, number, target, sizeof(target))) {
                status = copy_tree(run, version_dir, target, kSkip, COUNT(kSkip));
            }
            char render_dir[PATH_BYTES];
            if (status == URSATZ_OK && join(render_dir, version_dir, "render")) {
                status = migrate_render(run, render_dir, id, number);
            }
        }
        if (status == URSATZ_OK && workshop_layout_version_file(project, id, number, WORKSHOP_FILE_ENVELOPE, relative, sizeof(relative))) {
            status = put_json(run, relative, WORKSHOP_FORMAT_ITEM_VERSION, envelope);
        }
        cJSON_Delete(envelope);
        cJSON_Delete(body);
        run->versions++;
    }

    char marks[PATH_BYTES];
    if (status == URSATZ_OK && join(marks, dir, WORKSHOP_FILE_OP_CHECKS) && common_path_exists(marks)) {
        cJSON *checks = NULL;
        char relative[PATH_BYTES];
        status = read_object(run, marks, &checks);
        if (status == URSATZ_OK && workshop_layout_item_file(project, id, WORKSHOP_FILE_OP_CHECKS, relative, sizeof(relative))) {
            if (!cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(checks, "checks"))) {
                cJSON_DeleteItemFromObjectCaseSensitive(checks, "checks");
                cJSON_AddObjectToObject(checks, "checks");
            }
            status = put_json(run, relative, WORKSHOP_FORMAT_OP_CHECKS, checks);
        }
        cJSON_Delete(checks);
    }
    if (status == URSATZ_OK) {
        /* the header last: a folder without it is not an item */
        char relative[PATH_BYTES];
        cJSON *header = cJSON_CreateObject();
        bool ok = header && cJSON_AddStringToObject(header, "id", id) && cJSON_AddStringToObject(header, "type", type) &&
                  cJSON_AddStringToObject(header, "name", common_json_string(item, "name")) &&
                  cJSON_AddNumberToObject(header, "current_version", (double)current) &&
                  cJSON_AddStringToObject(header, "created_at", common_json_string(item, "created_at")) &&
                  cJSON_AddStringToObject(header, "updated_at", common_json_string(item, "updated_at")) &&
                  workshop_layout_item_file(project, id, WORKSHOP_FILE_ITEM, relative, sizeof(relative));
        status = ok ? put_json(run, relative, WORKSHOP_FORMAT_ITEM, header) : fail(run, URSATZ_ERR_ALLOC, path, "out of memory");
        cJSON_Delete(header);
        run->items++;
    }
    cJSON_Delete(item);
    return status;
}

static UrsatzStatus migrate_items(Run *run) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (!join(dir, run->root, "materials/items") || !common_path_is_dir(dir)) {
        return URSATZ_OK;
    }
    UrsatzStatus status = common_dir_list(dir, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        if (workshop_layout_id_ok(names.items[i])) {
            status = migrate_item(run, dir, names.items[i]);
        }
    }
    common_names_free(&names);
    return status;
}

/* One JSON file per entry of a folder (subjects/, collections/) becomes the same file stamped as format under <relative_dir>. */
static UrsatzStatus migrate_json_folder(Run *run, const char *folder, const char *relative_dir, const char *format, long *count) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (!join(dir, run->root, folder) || !common_path_is_dir(dir)) {
        return URSATZ_OK;
    }
    UrsatzStatus status = common_dir_list(dir, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char id[128];
        char path[PATH_BYTES];
        char relative[PATH_BYTES];
        if (!id_from_file(names.items[i], id, sizeof(id)) || !join(path, dir, names.items[i]) ||
            snprintf(relative, sizeof(relative), "%s/%s", relative_dir, names.items[i]) >= (int)sizeof(relative)) {
            continue;
        }
        cJSON *object = NULL;
        status = read_object(run, path, &object);
        if (status != URSATZ_OK) {
            break;
        }
        cJSON_DeleteItemFromObjectCaseSensitive(object, "id");
        cJSON_AddStringToObject(object, "id", id);
        status = put_json(run, relative, format, object);
        cJSON_Delete(object);
        (*count)++;
    }
    common_names_free(&names);
    return status;
}

static UrsatzStatus migrate_pieces(Run *run) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (!join(dir, run->root, "pieces") || !common_path_is_dir(dir)) {
        return URSATZ_OK;
    }
    UrsatzStatus status = common_dir_list(dir, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char piece_dir[PATH_BYTES];
        char meta_path[PATH_BYTES];
        char relative[PATH_BYTES];
        if (!workshop_layout_id_ok(names.items[i]) || !join(piece_dir, dir, names.items[i]) || !common_path_is_dir(piece_dir) ||
            !join(meta_path, piece_dir, "meta.json") || !common_path_exists(meta_path) ||
            snprintf(relative, sizeof(relative), WORKSHOP_DIR_PIECES "/%s", names.items[i]) >= (int)sizeof(relative)) {
            continue; /* anything that is not a folder with a meta.json is left out, as the Library list left it out */
        }
        cJSON *meta = NULL;
        status = read_object(run, meta_path, &meta);
        if (status != URSATZ_OK) {
            break;
        }
        /* the source's format is "format" in the GUI's file and source_format in ours; the analysis key is producer_key */
        cJSON *source_format = cJSON_DetachItemFromObjectCaseSensitive(meta, "format");
        cJSON *key = cJSON_DetachItemFromObjectCaseSensitive(meta, "ursatz_commit");
        if (source_format) {
            cJSON_AddItemToObject(meta, "source_format", source_format);
        }
        if (key) {
            cJSON_AddItemToObject(meta, "producer_key", key);
        }
        char file[PATH_BYTES];
        static const char *const kFiles[] = {"source.mid", "source.musicxml", "source.xml", "source.mxl", "analysis.json"};
        for (size_t f = 0; status == URSATZ_OK && f < COUNT(kFiles); f++) {
            char from[PATH_BYTES];
            if (join(from, piece_dir, kFiles[f]) && common_path_exists(from) &&
                snprintf(file, sizeof(file), "%s/%s", relative, kFiles[f]) < (int)sizeof(file)) {
                status = copy_file(run, from, file);
            }
        }
        if (status == URSATZ_OK && snprintf(file, sizeof(file), "%s/" WORKSHOP_FILE_PIECE, relative) < (int)sizeof(file)) {
            status = put_json(run, file, WORKSHOP_FORMAT_LIBRARY_PIECE, meta);
            run->pieces++;
        }
        cJSON_Delete(meta);
    }
    common_names_free(&names);
    return status;
}

static UrsatzStatus migrate_styles(Run *run) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (!join(dir, run->root, "styles") || !common_path_is_dir(dir)) {
        return URSATZ_OK;
    }
    UrsatzStatus status = common_dir_list(dir, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char style_dir[PATH_BYTES];
        char meta_path[PATH_BYTES];
        char relative[PATH_BYTES];
        if (!workshop_layout_id_ok(names.items[i]) || !join(style_dir, dir, names.items[i]) || !common_path_is_dir(style_dir) ||
            !join(meta_path, style_dir, "meta.json") || !common_path_exists(meta_path) ||
            !workshop_layout_style_dir(names.items[i], relative, sizeof(relative))) {
            continue;
        }
        cJSON *meta = NULL;
        status = read_object(run, meta_path, &meta);
        if (status != URSATZ_OK) {
            break;
        }
        char model_from[PATH_BYTES];
        char target[PATH_BYTES];
        if (join(model_from, style_dir, WORKSHOP_FILE_STYLE_MODEL) && common_path_exists(model_from) &&
            snprintf(target, sizeof(target), "%s/" WORKSHOP_FILE_STYLE_MODEL, relative) < (int)sizeof(target)) {
            status = copy_file(run, model_from, target);
        }
        if (status == URSATZ_OK && snprintf(target, sizeof(target), "%s/" WORKSHOP_FILE_STYLE, relative) < (int)sizeof(target)) {
            cJSON_DeleteItemFromObjectCaseSensitive(meta, "id");
            cJSON_AddStringToObject(meta, "id", names.items[i]);
            status = put_json(run, target, WORKSHOP_FORMAT_STYLE, meta);
            run->styles++;
        }
        cJSON_Delete(meta);
    }
    common_names_free(&names);
    return status;
}

static UrsatzStatus migrate_links(Run *run) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    char cache[PATH_BYTES];
    UrsatzStatus status = URSATZ_OK;
    if (join(cache, run->root, "links/motive-link-cache.json") && common_path_exists(cache)) {
        status = copy_file(run, cache, WORKSHOP_FILE_LINK_CACHE);
    }
    if (status != URSATZ_OK || !join(dir, run->root, "links/runs") || !common_path_is_dir(dir)) {
        return status;
    }
    status = common_dir_list(dir, &names, run->error);
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char path[PATH_BYTES];
        char relative[PATH_BYTES];
        if (!workshop_layout_id_ok(names.items[i]) || snprintf(path, sizeof(path), "%s/%s/run.json", dir, names.items[i]) >= (int)sizeof(path) ||
            !common_path_exists(path) || !workshop_layout_link_file(names.items[i], relative, sizeof(relative))) {
            continue;
        }
        cJSON *record = NULL;
        status = read_object(run, path, &record);
        if (status != URSATZ_OK) {
            break;
        }
        cJSON_DeleteItemFromObjectCaseSensitive(record, "id");
        cJSON_AddStringToObject(record, "id", names.items[i]);
        if (ensure_text(record, "status", "failed") != URSATZ_OK || ensure_text(record, "created_at", "1970-01-01T00:00:00Z") != URSATZ_OK ||
            !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(record, "scope"))) {
            cJSON_DeleteItemFromObjectCaseSensitive(record, "scope");
            cJSON_AddObjectToObject(record, "scope");
        }
        status = put_json(run, relative, WORKSHOP_FORMAT_LINK_RUN, record);
        cJSON_Delete(record);
        run->runs++;
    }
    common_names_free(&names);
    return status;
}

/* --- the backup ------------------------------------------------------------------------------------------------------- */

typedef struct {
    long files;
    long long bytes;
} TreeStats;

static void tree_stats(const char *path, TreeStats *stats) {
    CommonNames names = {0};
    if (common_dir_list(path, &names, NULL) != URSATZ_OK) {
        return;
    }
    for (size_t i = 0; i < names.count; i++) {
        char child[PATH_BYTES];
        struct stat info;
        if (!join(child, path, names.items[i]) || lstat(child, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            tree_stats(child, stats);
        } else if (S_ISREG(info.st_mode)) {
            stats->files++;
            stats->bytes += (long long)info.st_size;
        }
    }
    common_names_free(&names);
}

static UrsatzStatus backup_tree(const char *from, const char *to, UrsatzError *error) {
    UrsatzStatus status = common_dir_make(to, error);
    CommonNames names = {0};
    if (status == URSATZ_OK) {
        status = common_dir_list(from, &names, error);
    }
    for (size_t i = 0; status == URSATZ_OK && i < names.count; i++) {
        char source[PATH_BYTES];
        char target[PATH_BYTES];
        struct stat info;
        if (!join(source, from, names.items[i]) || !join(target, to, names.items[i]) || lstat(source, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            status = backup_tree(source, target, error);
        } else if (S_ISREG(info.st_mode)) {
            char *data = NULL;
            size_t size = 0;
            status = common_file_read(source, &data, &size, error);
            if (status == URSATZ_OK) {
                status = common_file_write_atomic(target, data, size, error);
            }
            free(data);
        }
    }
    common_names_free(&names);
    return status;
}

/* --- the swap ---------------------------------------------------------------------------------------------------------- */

static bool exists_at(const char *root, const char *name) {
    char path[PATH_BYTES];
    return join(path, root, name) && common_path_exists(path);
}

static bool entry_in(const char *root, const char *folder, const char *name) {
    char path[PATH_BYTES];
    return snprintf(path, sizeof(path), "%s/%s/%s", root, folder, name) < (int)sizeof(path) && common_path_exists(path);
}

/*
 * Moves the old entries into .pre-1/ and the new ones into place, then writes data-folder.json. It can be run again from any stop: an
 * old entry moves when it is at the top and not in .pre-1 yet (and, for subjects/, which both layouts name, only while the new one is
 * still in .migrate-1), a new entry moves when it is still in .migrate-1 and its place is empty.
 */
static UrsatzStatus swap_in(Run *run, bool migrated) {
    char pre[PATH_BYTES];
    if (!join(pre, run->root, PRE_DIR) || common_dir_make(pre, run->error) != URSATZ_OK) {
        return fail(run, URSATZ_ERR_IO, PRE_DIR, "cannot make %s", PRE_DIR);
    }
    for (size_t i = 0; i < COUNT(kOldEntries); i++) {
        const char *name = kOldEntries[i];
        bool shared = in_list(name, kNewEntries, COUNT(kNewEntries));
        if (!exists_at(run->root, name) || entry_in(run->root, PRE_DIR, name) || (shared && !entry_in(run->root, BUILD_DIR, name))) {
            continue;
        }
        char from[PATH_BYTES];
        char to[PATH_BYTES];
        if (!join(from, run->root, name) || !join(to, pre, name) || rename(from, to) != 0) {
            return fail(run, URSATZ_ERR_IO, name, "cannot move %s into %s", name, PRE_DIR);
        }
    }
    for (size_t i = 0; i < COUNT(kNewEntries); i++) {
        const char *name = kNewEntries[i];
        if (!entry_in(run->root, BUILD_DIR, name) || exists_at(run->root, name)) {
            continue;
        }
        char from[PATH_BYTES];
        char to[PATH_BYTES];
        if (snprintf(from, sizeof(from), "%s/%s/%s", run->root, BUILD_DIR, name) >= (int)sizeof(from) || !join(to, run->root, name) ||
            rename(from, to) != 0) {
            return fail(run, URSATZ_ERR_IO, name, "cannot move %s into place", name);
        }
    }
    cJSON *marker = cJSON_CreateObject();
    char now[32];
    common_utc_now(now);
    char path[PATH_BYTES];
    bool ok = marker && cJSON_AddStringToObject(marker, "created_at", now) && (!migrated || cJSON_AddNumberToObject(marker, "migrated_from", 0)) &&
              join(path, run->root, WORKSHOP_FILE_DATA_FOLDER);
    UrsatzStatus status = ok ? workshop_format_write(path, WORKSHOP_FORMAT_DATA_FOLDER, marker, run->error) : fail(run, URSATZ_ERR_ALLOC, path, "out of memory");
    cJSON_Delete(marker);
    if (status == URSATZ_OK) {
        char build[PATH_BYTES];
        if (join(build, run->root, BUILD_DIR)) {
            common_dir_remove(build, NULL);
        }
    }
    return status;
}

/* --- the report -------------------------------------------------------------------------------------------------------- */

static cJSON *report_of(const Run *run, bool changed, int from_layout, const char *backup, bool moves) {
    cJSON *report = cJSON_CreateObject();
    cJSON *counts = report ? cJSON_AddObjectToObject(report, "counts") : NULL;
    cJSON *list = report ? cJSON_AddArrayToObject(report, "moves") : NULL;
    if (!report || !counts || !list) {
        cJSON_Delete(report);
        return NULL;
    }
    cJSON_AddBoolToObject(report, "dry_run", run->dry);
    cJSON_AddStringToObject(report, "data_dir", run->root);
    cJSON_AddNumberToObject(report, "from_layout", from_layout);
    cJSON_AddNumberToObject(report, "to_layout", WORKSHOP_LAYOUT_VERSION);
    cJSON_AddBoolToObject(report, "changed", changed);
    if (backup) {
        cJSON_AddStringToObject(report, "backup", backup);
    } else {
        cJSON_AddNullToObject(report, "backup");
    }
    cJSON_AddNumberToObject(counts, "projects", (double)run->projects);
    cJSON_AddNumberToObject(counts, "items", (double)run->items);
    cJSON_AddNumberToObject(counts, "versions", (double)run->versions);
    cJSON_AddNumberToObject(counts, "bodies_rewritten", (double)run->bodies_rewritten);
    cJSON_AddNumberToObject(counts, "subjects", (double)run->subjects);
    cJSON_AddNumberToObject(counts, "library_pieces", (double)run->pieces);
    cJSON_AddNumberToObject(counts, "collections", (double)run->collections);
    cJSON_AddNumberToObject(counts, "styles", (double)run->styles);
    cJSON_AddNumberToObject(counts, "link_runs", (double)run->runs);
    cJSON_AddNumberToObject(counts, "render_caches", (double)run->renders);
    cJSON_AddNumberToObject(counts, "files", (double)run->files);
    cJSON_AddNumberToObject(counts, "bytes_copied", (double)run->bytes);
    for (size_t i = 0; moves && i < COUNT(kOldEntries); i++) {
        if (!exists_at(run->root, kOldEntries[i])) {
            continue;
        }
        cJSON *move = cJSON_CreateObject();
        char to[PATH_BYTES];
        snprintf(to, sizeof(to), PRE_DIR "/%s", kOldEntries[i]);
        if (move && cJSON_AddStringToObject(move, "from", kOldEntries[i]) && cJSON_AddStringToObject(move, "to", to)) {
            cJSON_AddItemToArray(list, move);
        } else {
            cJSON_Delete(move);
        }
    }
    return report;
}

/* --- the migration ----------------------------------------------------------------------------------------------------- */

static bool has_old_entries(const char *root) {
    for (size_t i = 0; i < COUNT(kOldEntries); i++) {
        if (exists_at(root, kOldEntries[i])) {
            return true;
        }
    }
    return false;
}

bool workshop_data_folder_has_old_entries(const char *data_dir) {
    return has_old_entries(data_dir);
}

UrsatzStatus workshop_data_folder_layout(const char *data_dir, int *layout, UrsatzError *error) {
    char path[PATH_BYTES];
    *layout = 0;
    if (!join(path, data_dir, WORKSHOP_FILE_DATA_FOLDER) || !common_path_exists(path)) {
        return URSATZ_OK;
    }
    cJSON *marker = NULL;
    UrsatzStatus status = workshop_format_read(path, WORKSHOP_FORMAT_DATA_FOLDER, true, &marker, error);
    if (status == URSATZ_OK) {
        *layout = WORKSHOP_LAYOUT_VERSION;
        cJSON_Delete(marker);
    }
    return status;
}

static void backup_name(const char *root, char *out, size_t size) {
    time_t now = time(NULL);
    struct tm parts;
    gmtime_r(&now, &parts);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &parts);
    char trimmed[PATH_BYTES];
    snprintf(trimmed, sizeof(trimmed), "%s", root);
    size_t length = strlen(trimmed);
    while (length > 1 && trimmed[length - 1] == '/') {
        trimmed[--length] = '\0';
    }
    snprintf(out, size, "%s-backup-%s", trimmed, stamp);
}

static UrsatzStatus migrate_run(const char *data_dir, const WorkshopMigrateOptions *options, cJSON **report, UrsatzError *error) {
    if (report) {
        *report = NULL;
    }
    Run run;
    memset(&run, 0, sizeof(run));
    run.dry = options && options->dry_run;
    run.error = error;
    char root[PATH_BYTES];
    if (!data_dir || data_dir[0] == '\0') {
        return fail(&run, URSATZ_ERR_REQUEST, "", "no data folder is named");
    }
    if (common_path_exists(data_dir) && !realpath(data_dir, root)) {
        return fail(&run, URSATZ_ERR_IO, data_dir, "cannot resolve %s", data_dir);
    }
    if (!common_path_exists(data_dir)) {
        snprintf(root, sizeof(root), "%s", data_dir);
    }
    run.root = root;
    if (!join(run.build, root, BUILD_DIR)) {
        return fail(&run, URSATZ_ERR_REQUEST, data_dir, "the data folder path is too long");
    }

    int layout = 0;
    UrsatzStatus status = workshop_data_folder_layout(root, &layout, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (layout == WORKSHOP_LAYOUT_VERSION) {
        if (report) {
            *report = report_of(&run, false, WORKSHOP_LAYOUT_VERSION, NULL, false);
        }
        return URSATZ_OK;
    }

    bool finishing = common_path_is_dir(run.build) && entry_in(root, BUILD_DIR, COMPLETE_MARKER);
    if (!has_old_entries(root) && !finishing) {
        /* an empty or absent folder is made layout 1 */
        if (!run.dry) {
            if (common_dir_make(root, error) != URSATZ_OK) {
                return URSATZ_ERR_IO;
            }
            status = swap_in(&run, false);
            if (status != URSATZ_OK) {
                return status;
            }
        }
        if (report) {
            *report = report_of(&run, !run.dry, 0, NULL, false);
        }
        return URSATZ_OK;
    }

    char backup[PATH_BYTES] = "";
    if (!finishing) {
        char message[2048];
        if (!workshop_data_check_only(root, message, sizeof(message))) {
            return ursatz_error_set(error, URSATZ_ERR_FORMAT, root, "%s", message);
        }
        if (!run.dry) {
            if (common_path_is_dir(run.build)) {
                common_dir_remove(run.build, NULL); /* an interrupted run that never got to its last moves */
            }
            /* The backup is the first change: the whole folder is copied and compared before anything is built. */
            if (options && options->backup_dir && options->backup_dir[0] != '\0') {
                snprintf(backup, sizeof(backup), "%s", options->backup_dir);
            } else {
                backup_name(root, backup, sizeof(backup));
            }
            if (common_path_exists(backup)) {
                return fail(&run, URSATZ_ERR_CONFLICT, backup, "the backup folder %s exists already: choose another with --backup", backup);
            }
            TreeStats before = {0, 0};
            TreeStats after = {0, 0};
            tree_stats(root, &before);
            status = backup_tree(root, backup, error);
            if (status == URSATZ_OK) {
                tree_stats(backup, &after);
                if (after.files != before.files || after.bytes != before.bytes) {
                    status = fail(&run, URSATZ_ERR_IO, backup,
                                  "the backup %s does not match the data folder (%ld files and %lld bytes against %ld and %lld): nothing was changed",
                                  backup, after.files, after.bytes, before.files, before.bytes);
                }
            }
            if (status != URSATZ_OK) {
                return status;
            }
        }
        status = migrate_projects(&run);
        status = status == URSATZ_OK ? migrate_items(&run) : status;
        status = status == URSATZ_OK ? migrate_json_folder(&run, "subjects", WORKSHOP_DIR_SUBJECTS, WORKSHOP_FORMAT_SUBJECT, &run.subjects) : status;
        status = status == URSATZ_OK ? migrate_pieces(&run) : status;
        status = status == URSATZ_OK ? migrate_json_folder(&run, "collections", WORKSHOP_DIR_COLLECTIONS, WORKSHOP_FORMAT_COLLECTION, &run.collections) : status;
        status = status == URSATZ_OK ? migrate_styles(&run) : status;
        status = status == URSATZ_OK ? migrate_links(&run) : status;
        if (status == URSATZ_OK && !run.dry) {
            char complete[PATH_BYTES];
            status = join(complete, run.build, COMPLETE_MARKER) ? common_file_write_atomic(complete, "complete\n", 9, error) : URSATZ_ERR_IO;
        }
        if (run.dry || status != URSATZ_OK) {
            if (status != URSATZ_OK && !run.dry) {
                common_dir_remove(run.build, NULL);
            }
            if (status == URSATZ_OK && report) {
                *report = report_of(&run, false, 0, NULL, true);
            }
            common_names_free(&run.known_projects);
            return status;
        }
    }

    cJSON *summary = report ? report_of(&run, true, 0, backup[0] ? backup : NULL, true) : NULL;
    status = swap_in(&run, true);
    common_names_free(&run.known_projects);
    if (status != URSATZ_OK) {
        cJSON_Delete(summary);
        return status;
    }
    if (report) {
        *report = summary;
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_migrate(const char *data_dir, const WorkshopMigrateOptions *options, cJSON **report, UrsatzError *error) {
    /* The body migrations name materials by the Composer's vocabulary, which needs the knowledge a workshop builds when it opens. */
    bool had_knowledge = workshop_knowledge_snapshot() != NULL;
    if (!had_knowledge && !workshop_knowledge_init()) {
        return ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the knowledge cannot be read");
    }
    UrsatzStatus status = migrate_run(data_dir, options, report, error);
    if (!had_knowledge) {
        workshop_knowledge_shutdown();
    }
    return status;
}
