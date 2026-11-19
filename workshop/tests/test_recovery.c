/* Recovery of the data folder after a stop (P63 B8, R-643): hand-made leftovers of every kind, then workshop_open, which recovers them from the
 * journal before any service starts. A pending entry of a committed record is renamed into place (a file replacing the old one, a folder where
 * there was none); a pending entry of a record that was never committed is deleted; a removed entry of a committed record is deleted and one of
 * an uncommitted record is renamed back; a torn last journal line is cut off; an item header older than the last record that sets it is written
 * again (and one that a pending folder left out is made); stray temporaries and the old staging folders and uploads go; cache/index/ is built
 * again. The report is one line of counts. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>

#include <common/files.h>
#include <common/json.h>
#include <ursatz_workshop/workshop_api.h>

#include "items/journal.h"

static void write_text(const char *path, const char *text) {
    char parent[512];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        assert(common_dir_make(parent, NULL) == URSATZ_OK);
    }
    assert(common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK);
}

static char *read_text(const char *path) {
    char *data = NULL;
    return common_file_read(path, &data, NULL, NULL) == URSATZ_OK ? data : NULL;
}

static bool has(const char *path, const char *needle) {
    char *text = read_text(path);
    bool found = text && strstr(text, needle) != NULL;
    free(text);
    return found;
}

static const char kSubject[] =
    "{\"format\":\"ursatz-subject\",\"format_version\":1,\"id\":\"%s\",\"name\":\"%s\",\"kind\":\"idea\",\"note\":\"\",\"items\":[],\"pieces\":[],"
    "\"related\":[],\"created_at\":\"2026-01-01T00:00:00Z\",\"updated_at\":\"2026-01-01T00:00:00Z\"}";

static void write_subject(const char *path, const char *id, const char *name) {
    char text[1024];
    snprintf(text, sizeof(text), kSubject, id, name);
    write_text(path, text);
}

static void write_item(const char *path, const char *id, const char *name, int journal_seq) {
    char text[1024];
    snprintf(text, sizeof(text),
             "{\"format\":\"ursatz-item\",\"format_version\":1,\"id\":\"%s\",\"type\":\"motif\",\"name\":\"%s\",\"current_version\":1,"
             "\"created_at\":\"2026-01-01T00:00:00Z\",\"updated_at\":\"2026-01-01T00:00:00Z\",\"journal_seq\":%d}",
             id, name, journal_seq);
    write_text(path, text);
}

/* A journal line for a record with the given writes ("pending|final" pairs), removes ("removed|final") and header (NULL for none). */
static void append_record(const char *journal, int seq, const char **writes, const char **removes, const char *header) {
    char line[4096];
    int used = snprintf(line, sizeof(line), "{\"seq\":%d,\"id\":\"cmd-%016d\",\"command\":\"test\",\"actor\":\"michelle\",\"at\":\"2026-01-01T00:00:00Z\","
                                           "\"target\":\"\",\"base_version\":0,\"writes\":[", seq, seq);
    for (int i = 0; writes && writes[i]; i++) {
        char pending[400];
        char final_path[400];
        sscanf(writes[i], "%399[^|]|%399s", pending, final_path);
        used += snprintf(line + used, sizeof(line) - (size_t)used, "%s{\"pending\":\"%s\",\"final\":\"%s\"}", i ? "," : "", pending, final_path);
    }
    used += snprintf(line + used, sizeof(line) - (size_t)used, "],\"removes\":[");
    for (int i = 0; removes && removes[i]; i++) {
        char removed[400];
        char final_path[400];
        sscanf(removes[i], "%399[^|]|%399s", removed, final_path);
        used += snprintf(line + used, sizeof(line) - (size_t)used, "%s{\"removed\":\"%s\",\"final\":\"%s\"}", i ? "," : "", removed, final_path);
    }
    snprintf(line + used, sizeof(line) - (size_t)used, "],\"headers\":[%s]}", header ? header : "");
    assert(common_file_append_sync(journal, line, strlen(line), NULL) == URSATZ_OK);
    assert(common_file_append_sync(journal, "\n", 1, NULL) == URSATZ_OK);
}

int main(void) {
    char root[64];
    snprintf(root, sizeof(root), "recovery_%d", (int)getpid());
    assert(common_dir_make(root, NULL) == URSATZ_OK);

    /* a layout 1 folder: the marker, a project, a motif with an old header, two subjects, a collection, a piece */
    char path[512];
    snprintf(path, sizeof(path), "%s/data-folder.json", root);
    write_text(path, "{\"format\":\"ursatz-data-folder\",\"format_version\":1,\"created_at\":\"2026-01-01T00:00:00Z\"}");
    snprintf(path, sizeof(path), "%s/projects/prj-1/project.json", root);
    write_text(path, "{\"format\":\"ursatz-project\",\"format_version\":1,\"id\":\"prj-1\",\"name\":\"One\",\"description\":\"\","
                     "\"created_at\":\"2026-01-01T00:00:00Z\",\"updated_at\":\"2026-01-01T00:00:00Z\"}");
    snprintf(path, sizeof(path), "%s/projects/prj-1/items/mot-old/item.json", root);
    write_item(path, "mot-old", "Old name", 1);
    snprintf(path, sizeof(path), "%s/subjects/sub-keep.json", root);
    write_subject(path, "sub-keep", "Keep");
    snprintf(path, sizeof(path), "%s/subjects/.removed-9-sub-restore.json", root);
    write_subject(path, "sub-restore", "Restore");
    snprintf(path, sizeof(path), "%s/subjects/.removed-2-sub-gone.json", root);
    write_subject(path, "sub-gone", "Gone");

    /* committed record 1: a pending subject file whose final is absent */
    snprintf(path, sizeof(path), "%s/subjects/.pending-1-sub-new.json", root);
    write_subject(path, "sub-new", "New");
    /* committed record 2: a pending file replacing the old one, and a removed entry */
    snprintf(path, sizeof(path), "%s/subjects/.pending-2-sub-keep.json", root);
    write_subject(path, "sub-keep", "Kept and changed");
    /* committed record 3: a pending folder (a Library piece) */
    snprintf(path, sizeof(path), "%s/library/pieces/.pending-3-piece-x/piece.json", root);
    write_text(path, "{\"format\":\"ursatz-library-piece\",\"format_version\":1,\"filename\":\"x.mid\",\"source_format\":\"midi\","
                     "\"uploaded_at\":\"2026-01-01T00:00:00Z\",\"analyzer_version\":\"0\",\"document_schema_version\":13}");
    snprintf(path, sizeof(path), "%s/library/pieces/.pending-3-piece-x/source.mid", root);
    write_text(path, "MThd");
    /* committed record 4: an item header newer than the file's, and a folder whose header was never written */
    snprintf(path, sizeof(path), "%s/projects/prj-1/items/mot-made/v1/body.json", root);
    write_text(path, "{}");
    /* uncommitted: a pending entry of seq 9 */
    snprintf(path, sizeof(path), "%s/subjects/.pending-9-sub-never.json", root);
    write_subject(path, "sub-never", "Never");
    snprintf(path, sizeof(path), "%s/library/pieces/.pending-9-piece-never/piece.json", root);
    write_text(path, "{}");
    /* stray temporaries, an old staging folder and an upload */
    snprintf(path, sizeof(path), "%s/subjects/sub-keep.json.tmp-4242", root);
    write_text(path, "half");
    snprintf(path, sizeof(path), "%s/projects/prj-1/items/mot-old/v3.stage-1-4242/request.json", root);
    write_text(path, "{}");
    snprintf(path, sizeof(path), "%s/library/pieces/piece-y/source.upload", root);
    write_text(path, "upload");

    /* the journal: four committed records and a torn fifth */
    char journal[512];
    snprintf(journal, sizeof(journal), "%s/%s", root, WORKSHOP_JOURNAL_FILE);
    write_text(journal, "{\"format\":\"ursatz-journal\",\"format_version\":1}\n");
    const char *writes1[] = {"subjects/.pending-1-sub-new.json|subjects/sub-new.json", NULL};
    append_record(journal, 1, writes1, NULL, NULL);
    const char *writes2[] = {"subjects/.pending-2-sub-keep.json|subjects/sub-keep.json", NULL};
    const char *removes2[] = {"subjects/.removed-2-sub-gone.json|subjects/sub-gone.json", NULL};
    append_record(journal, 2, writes2, removes2, NULL);
    const char *writes3[] = {"library/pieces/.pending-3-piece-x|library/pieces/piece-x", NULL};
    append_record(journal, 3, writes3, NULL, NULL);
    append_record(journal, 4, NULL, NULL,
                  "{\"id\":\"mot-old\",\"type\":\"motif\",\"name\":\"Newer name\",\"current_version\":1,\"created_at\":\"2026-01-01T00:00:00Z\","
                  "\"updated_at\":\"2026-01-02T00:00:00Z\",\"project_id\":\"prj-1\"},"
                  "{\"id\":\"mot-made\",\"type\":\"motif\",\"name\":\"Made\",\"current_version\":1,\"created_at\":\"2026-01-02T00:00:00Z\","
                  "\"updated_at\":\"2026-01-02T00:00:00Z\",\"project_id\":\"prj-1\"}");
    const char torn[] = "{\"seq\":5,\"id\":\"cmd-torn\",\"command\":\"te";
    assert(common_file_append_sync(journal, torn, sizeof(torn) - 1, NULL) == URSATZ_OK);

    /* open: the recovery runs before anything else */
    WorkshopConfig config = {root, NULL, 9301, 2, 2};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    assert(workshop_settings_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop);
    const char *summary = workshop_recovery_summary(workshop);
    printf("%s\n", summary);
    assert(strstr(summary, "recovered the data folder") != NULL);
    workshop_close(workshop);

    char p[512];
    /* committed pending entries are in place */
    snprintf(p, sizeof(p), "%s/subjects/sub-new.json", root);
    assert(common_path_exists(p) && has(p, "\"New\""));
    snprintf(p, sizeof(p), "%s/subjects/sub-keep.json", root);
    assert(has(p, "Kept and changed"));
    snprintf(p, sizeof(p), "%s/library/pieces/piece-x/piece.json", root);
    assert(common_path_exists(p));
    snprintf(p, sizeof(p), "%s/library/pieces/piece-x/source.mid", root);
    assert(common_path_exists(p));
    /* a removed entry of a committed record is gone, one of an uncommitted record is back */
    snprintf(p, sizeof(p), "%s/subjects/sub-gone.json", root);
    assert(!common_path_exists(p));
    snprintf(p, sizeof(p), "%s/subjects/sub-restore.json", root);
    assert(common_path_exists(p) && has(p, "\"Restore\""));
    /* uncommitted pending entries are deleted */
    snprintf(p, sizeof(p), "%s/subjects/sub-never.json", root);
    assert(!common_path_exists(p));
    snprintf(p, sizeof(p), "%s/library/pieces/piece-never", root);
    assert(!common_path_exists(p));
    /* headers: the old one is written again with the record's seq, the missing one is made */
    snprintf(p, sizeof(p), "%s/projects/prj-1/items/mot-old/item.json", root);
    assert(has(p, "Newer name") && has(p, "\"journal_seq\":\t4"));
    snprintf(p, sizeof(p), "%s/projects/prj-1/items/mot-made/item.json", root);
    assert(common_path_exists(p) && has(p, "\"Made\"") && has(p, "ursatz-item"));
    /* the torn line is gone and the journal is whole */
    WorkshopJournal read;
    assert(workshop_journal_read(journal, &read, &error) == URSATZ_OK && read.count == 4 && read.last_seq == 4 && !read.torn);
    workshop_journal_free(&read);
    /* temporaries, staging folders and uploads are deleted */
    snprintf(p, sizeof(p), "%s/subjects/sub-keep.json.tmp-4242", root);
    assert(!common_path_exists(p));
    snprintf(p, sizeof(p), "%s/projects/prj-1/items/mot-old/v3.stage-1-4242", root);
    assert(!common_path_exists(p));
    snprintf(p, sizeof(p), "%s/library/pieces/piece-y/source.upload", root);
    assert(!common_path_exists(p));
    /* the index is built again */
    snprintf(p, sizeof(p), "%s/cache/index/journal-seq", root);
    assert(has(p, "4"));

    /* a second open finds nothing to recover */
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop);
    assert(workshop_recovery_summary(workshop)[0] == '\0');
    workshop_close(workshop);

    char command[600];
    snprintf(command, sizeof(command), "rm -rf '%s'", root);
    if (system(command) != 0) {
        return 1;
    }
    printf("workshop_recovery: ok\n");
    return 0;
}
