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
    URSATZ_GUI_ANALYSIS_ERR_ALLOC,
    URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND
} UrsatzGuiAnalysisResult;

/*
 * One persisted Chord or Key Interpretation, as returned by
 * ursatz_gui_analysis_get_piece. produced_by (author/method) is the only
 * analyzer/importer identity Ursatz's Interpretation actually carries and
 * persists today -- NOT a full Provenance-object reference. Ursatz's
 * ProvenanceReference on these Interpretations is always
 * PROVENANCE_REFERENCE_ABSENT (see key_estimation.c/chord_identification.c),
 * and no resolvable Provenance record type exists upstream yet to point
 * it at even if it were present.
 */
typedef struct {
    char *interpretation_id;
    char *type;              /* "key" or "chord" */
    char *label;              /* e.g. "D major", "V7" */
    char *produced_by_author; /* Interpretation's author field */
    char *produced_by_method; /* Interpretation's method field */
} UrsatzGuiInterpretationSummary;

typedef struct {
    char *id;
    size_t note_event_count;
    UrsatzGuiInterpretationSummary *interpretations;
    size_t interpretation_count;
} UrsatzGuiPieceDetail;

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

/*
 * Loads the detail view for one saved piece: its NoteEvent count plus
 * every Chord/Key Interpretation produced for it (via the
 * ursatz-gui-owned interpretation index recorded at analysis time -- see
 * analysis_service.c), each with its produced_by author/method.
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists
 * under pieces_dir. On URSATZ_GUI_ANALYSIS_OK, *out_detail is newly
 * allocated and caller-owned (free with ursatz_gui_piece_detail_destroy).
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece(const char *pieces_dir, const char *id,
                                                       UrsatzGuiPieceDetail **out_detail);

void ursatz_gui_piece_detail_destroy(UrsatzGuiPieceDetail *detail);

#endif /* URSATZ_GUI_ANALYSIS_SERVICE_H */
