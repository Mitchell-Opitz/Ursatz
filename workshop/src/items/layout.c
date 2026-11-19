#include "layout.h"

#include <stdio.h>
#include <string.h>

#define ID_MAX_BYTES 64

bool workshop_layout_id_ok(const char *id) {
    if (!id || id[0] == '\0' || strlen(id) > ID_MAX_BYTES) {
        return false;
    }
    for (const char *c = id; *c; c++) {
        bool ok = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

static bool fits(int written, size_t size) {
    return written >= 0 && (size_t)written < size;
}

bool workshop_layout_project_dir(const char *project, char *out, size_t size) {
    return workshop_layout_id_ok(project) && fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s", project), size);
}

bool workshop_layout_project_file(const char *project, char *out, size_t size) {
    return workshop_layout_id_ok(project) && fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s/" WORKSHOP_FILE_PROJECT, project), size);
}

bool workshop_layout_items_dir(const char *project, char *out, size_t size) {
    return workshop_layout_id_ok(project) && fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s/items", project), size);
}

bool workshop_layout_item_dir(const char *project, const char *item, char *out, size_t size) {
    return workshop_layout_id_ok(project) && workshop_layout_id_ok(item) &&
           fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s/items/%s", project, item), size);
}

bool workshop_layout_item_file(const char *project, const char *item, const char *name, char *out, size_t size) {
    return workshop_layout_id_ok(project) && workshop_layout_id_ok(item) && name &&
           fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s/items/%s/%s", project, item, name), size);
}

bool workshop_layout_version_dir(const char *project, const char *item, int version, char *out, size_t size) {
    return workshop_layout_id_ok(project) && workshop_layout_id_ok(item) && version >= 1 &&
           fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s/items/%s/v%d", project, item, version), size);
}

bool workshop_layout_version_file(const char *project, const char *item, int version, const char *name, char *out, size_t size) {
    return workshop_layout_id_ok(project) && workshop_layout_id_ok(item) && version >= 1 && name &&
           fits(snprintf(out, size, WORKSHOP_DIR_PROJECTS "/%s/items/%s/v%d/%s", project, item, version, name), size);
}

bool workshop_layout_subject_file(const char *subject, char *out, size_t size) {
    return workshop_layout_id_ok(subject) && fits(snprintf(out, size, WORKSHOP_DIR_SUBJECTS "/%s.json", subject), size);
}

bool workshop_layout_collection_file(const char *collection, char *out, size_t size) {
    return workshop_layout_id_ok(collection) && fits(snprintf(out, size, WORKSHOP_DIR_COLLECTIONS "/%s.json", collection), size);
}

bool workshop_layout_link_dir(const char *run, char *out, size_t size) {
    return workshop_layout_id_ok(run) && fits(snprintf(out, size, WORKSHOP_DIR_LINKS "/%s", run), size);
}

bool workshop_layout_link_file(const char *run, char *out, size_t size) {
    return workshop_layout_id_ok(run) && fits(snprintf(out, size, WORKSHOP_DIR_LINKS "/%s/" WORKSHOP_FILE_RUN, run), size);
}

bool workshop_layout_style_dir(const char *style, char *out, size_t size) {
    return workshop_layout_id_ok(style) && fits(snprintf(out, size, WORKSHOP_DIR_STYLES "/%s", style), size);
}

bool workshop_layout_render_dir(const char *item, int version, char *out, size_t size) {
    return workshop_layout_id_ok(item) && version >= 1 && fits(snprintf(out, size, WORKSHOP_DIR_RENDERS "/%s/v%d", item, version), size);
}

bool workshop_layout_render_tmp_dir(char *out, size_t size) {
    return fits(snprintf(out, size, WORKSHOP_DIR_CACHE "/tmp"), size);
}
