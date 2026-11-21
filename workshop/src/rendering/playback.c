#include "playback.h"
#include "base/last_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <common/files.h>
#include <common/status.h>

#include "items/ids.h"

UrsatzStatus workshop_playback_of_midi(const char *path, char **json) {
    if (!path || !json) {
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    *json = NULL;
    ImportedScore notes;
    if (analyzer_import_file(path, IMPORTED_FORMAT_MIDI, &notes, NULL) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_FORMAT, NULL);
    }
    AnalyzerPlayback playback;
    UrsatzStatus status = analyzer_playback_build(&notes, &playback);
    if (status == URSATZ_OK) {
        status = analyzer_playback_to_json(&playback, &notes, json);
        analyzer_playback_destroy(&playback);
    }
    imported_score_destroy(&notes);
    if (status == URSATZ_ERR_ALLOC) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    return status == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_FORMAT, NULL);
}

UrsatzStatus workshop_playback_of_midi_bytes(const uint8_t *bytes, size_t size, const char *tmp_dir,
                                                          char **json) {
    if (!bytes || size == 0 || !tmp_dir || !json) {
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    *json = NULL;
    char *name = workshop_new_id("render");
    char path[640];
    if (!name) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char leaf[160];
    int length = snprintf(leaf, sizeof(leaf), "%s.mid", name);
    free(name);
    if (length < 0 || length >= (int)sizeof(leaf) || !common_path_join(path, sizeof(path), tmp_dir, leaf) || !(common_dir_make(tmp_dir, NULL) == URSATZ_OK) ||
        !(common_file_write_atomic(path, bytes, size, NULL) == URSATZ_OK)) {
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }
    UrsatzStatus result = workshop_playback_of_midi(path, json);
    remove(path);
    return result;
}
