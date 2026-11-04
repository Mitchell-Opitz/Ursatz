#ifndef URSATZ_GUI_PLAYBACK_SERVICE_H
#define URSATZ_GUI_PLAYBACK_SERVICE_H

#include <stddef.h>
#include <stdint.h>

/* Playback of a MIDI file the Composer wrote (R-275). The Analyzer reads the file and builds the playback; the GUI
 * keeps no MIDI reader of its own. The JSON is analyzer_playback_to_json's: {"ok":true,"tempo_source","tempo","parts",
 * "end","notes"}. */
typedef enum {
    URSATZ_GUI_PLAYBACK_OK = 0,
    URSATZ_GUI_PLAYBACK_ERR_ARGUMENT,
    URSATZ_GUI_PLAYBACK_ERR_IMPORT, /* the file is not a MIDI file the Analyzer can read, or has no tempo map */
    URSATZ_GUI_PLAYBACK_ERR_IO,     /* the temporary file could not be written */
    URSATZ_GUI_PLAYBACK_ERR_ALLOC
} UrsatzGuiPlaybackResult;

/* The playback of the MIDI file at path. On OK *json is a string the caller frees; otherwise it is NULL. */
UrsatzGuiPlaybackResult ursatz_gui_playback_of_midi(const char *path, char **json);

/*
 * As ursatz_gui_playback_of_midi for MIDI bytes held in memory (an unsaved render). The Analyzer imports by path, so
 * the bytes are written to "<tmp_dir>/<random>.mid", imported, and the file deleted. tmp_dir is created when absent.
 */
UrsatzGuiPlaybackResult ursatz_gui_playback_of_midi_bytes(const uint8_t *bytes, size_t size, const char *tmp_dir,
                                                          char **json);

#endif /* URSATZ_GUI_PLAYBACK_SERVICE_H */
