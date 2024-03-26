#ifndef URSATZ_GUI_ANALYSIS_SERVICE_H
#define URSATZ_GUI_ANALYSIS_SERVICE_H

#include <stddef.h>

/*
 * Shared analysis service: import + analyze + persist a MIDI piece.
 *
 * This is the ONLY place outside persistence_service.c allowed to call
 * into Ursatz. It wraps Ursatz's MIDIImporter, ChordIdentification and
 * KeyEstimation (both run against the common-practice-minimal
 * framework), and the NoteEvent/Interpretation persistence functions
 * into operations the Analyzer tool's route code can call directly,
 * per the boundary tool.h/README describe.
 *
 * The Sonority-grouping (by onset) and scale-degree/tonic-relative
 * pitch-class computation this module does before calling
 * ChordIdentification/KeyEstimation mirrors ursatz-analyzer's pipeline
 * (src/pipeline/segmentation.c, key_pipeline.c, chord_pipeline.c),
 * used as a reference call sequence only -- this module does not link
 * against or call ursatz-analyzer.
 *
 * Ursatz's persistence has no "piece" concept (persistence.h): each
 * piece is its own SQLite file, id == filename (without extension).
 * pieces_dir is created if it does not already exist.
 */

typedef enum {
    URSATZ_GUI_ANALYSIS_OK = 0,
    URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED,
    URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED,
    URSATZ_GUI_ANALYSIS_ERR_ALLOC
} UrsatzGuiAnalysisResult;

/*
 * Imports midi_data[0..midi_size) via Ursatz's MIDIImporter, estimates
 * a single global key and identifies a chord for each onset-grouped
 * sonority against the common-practice-minimal framework, and saves
 * the resulting NoteEvent sequence plus every produced Interpretation
 * into a new SQLite file under pieces_dir.
 *
 * On URSATZ_GUI_ANALYSIS_OK, *out_id is a newly allocated, caller-owned
 * id string (the saved piece's filename, without directory or ".db").
 * A piece with no recognizable key still saves successfully (its
 * NoteEvents only, no Interpretations).
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_import_and_save(const char *pieces_dir, const unsigned char *midi_data,
                                                              size_t midi_size, char **out_id);

/*
 * Lists saved piece ids found in pieces_dir (filenames minus ".db").
 * On URSATZ_GUI_ANALYSIS_OK, *out_ids is a newly allocated array of
 * *out_count newly allocated id strings (caller frees each string,
 * then the array). A missing or empty pieces_dir yields *out_count ==
 * 0, *out_ids == NULL, still URSATZ_GUI_ANALYSIS_OK.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_list(const char *pieces_dir, char ***out_ids, size_t *out_count);

#endif /* URSATZ_GUI_ANALYSIS_SERVICE_H */
