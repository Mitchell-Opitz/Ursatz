#include "playback_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/playback.h>

#include <common/files.h>
#include <common/status.h>

#include "id_service.h"

UrsatzGuiPlaybackResult ursatz_gui_playback_of_midi(const char *path, char **json) {
    if (!path || !json) {
        return URSATZ_GUI_PLAYBACK_ERR_ARGUMENT;
    }
    *json = NULL;
    ImportedScore notes;
    if (analyzer_import_file(path, IMPORTED_FORMAT_MIDI, &notes, NULL, 0) != ANALYZER_OK) {
        return URSATZ_GUI_PLAYBACK_ERR_IMPORT;
    }
    AnalyzerPlayback playback;
    AnalyzerStatus status = analyzer_playback_build(&notes, &playback);
    if (status == ANALYZER_OK) {
        status = analyzer_playback_to_json(&playback, &notes, json);
        analyzer_playback_destroy(&playback);
    }
    imported_score_destroy(&notes);
    if (status == ANALYZER_ERR_ALLOC) {
        return URSATZ_GUI_PLAYBACK_ERR_ALLOC;
    }
    return status == ANALYZER_OK ? URSATZ_GUI_PLAYBACK_OK : URSATZ_GUI_PLAYBACK_ERR_IMPORT;
}

UrsatzGuiPlaybackResult ursatz_gui_playback_of_midi_bytes(const uint8_t *bytes, size_t size, const char *tmp_dir,
                                                          char **json) {
    if (!bytes || size == 0 || !tmp_dir || !json) {
        return URSATZ_GUI_PLAYBACK_ERR_ARGUMENT;
    }
    *json = NULL;
    char *name = ursatz_gui_new_id("render");
    char path[640];
    if (!name) {
        return URSATZ_GUI_PLAYBACK_ERR_ALLOC;
    }
    int length = snprintf(path, sizeof(path), "%s/%s.mid", tmp_dir, name);
    free(name);
    if (length < 0 || length >= (int)sizeof(path) || !(common_dir_make(tmp_dir, NULL) == URSATZ_OK) ||
        !(common_file_write_atomic(path, bytes, size, NULL) == URSATZ_OK)) {
        return URSATZ_GUI_PLAYBACK_ERR_IO;
    }
    UrsatzGuiPlaybackResult result = ursatz_gui_playback_of_midi(path, json);
    remove(path);
    return result;
}
