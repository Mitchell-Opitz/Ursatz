#include "playback_service.h"
#include "last_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <common/files.h>
#include <common/status.h>

#include "id_service.h"

UrsatzStatus ursatz_gui_playback_of_midi(const char *path, char **json) {
    if (!path || !json) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
    }
    *json = NULL;
    ImportedScore notes;
    if (analyzer_import_file(path, IMPORTED_FORMAT_MIDI, &notes, NULL) != URSATZ_OK) {
        return ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL);
    }
    AnalyzerPlayback playback;
    UrsatzStatus status = analyzer_playback_build(&notes, &playback);
    if (status == URSATZ_OK) {
        status = analyzer_playback_to_json(&playback, &notes, json);
        analyzer_playback_destroy(&playback);
    }
    imported_score_destroy(&notes);
    if (status == URSATZ_ERR_ALLOC) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    return status == URSATZ_OK ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL);
}

UrsatzStatus ursatz_gui_playback_of_midi_bytes(const uint8_t *bytes, size_t size, const char *tmp_dir,
                                                          char **json) {
    if (!bytes || size == 0 || !tmp_dir || !json) {
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
    }
    *json = NULL;
    char *name = ursatz_gui_new_id("render");
    char path[640];
    if (!name) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    int length = snprintf(path, sizeof(path), "%s/%s.mid", tmp_dir, name);
    free(name);
    if (length < 0 || length >= (int)sizeof(path) || !(common_dir_make(tmp_dir, NULL) == URSATZ_OK) ||
        !(common_file_write_atomic(path, bytes, size, NULL) == URSATZ_OK)) {
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }
    UrsatzStatus result = ursatz_gui_playback_of_midi(path, json);
    remove(path);
    return result;
}
