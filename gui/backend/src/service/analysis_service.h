#ifndef URSATZ_GUI_ANALYSIS_SERVICE_H
#define URSATZ_GUI_ANALYSIS_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <export/musicxml_export.h>

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
 * against or call ursatz-analyzer. This includes global key
 * estimation's precedence order (a MIDI key signature meta-event's
 * tonic, then the bass pitch class of the piece's closing and opening
 * chord, then a brute-force pitch-class scan as a last resort) and its
 * flat/sharp spelling rules -- kept in sync by hand, not by any
 * mechanism that enforces it, so a future fix to ursatz-analyzer's
 * key_pipeline.c needs a matching port here too.
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
    URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND,
    URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT
} UrsatzGuiAnalysisResult;

/*
 * One persisted Chord or Key Interpretation, as returned by
 * ursatz_gui_analysis_get_piece. produced_by (author/method) is the only
 * analyzer/importer identity Ursatz's Interpretation actually carries and
 * persists today -- NOT a full Provenance-object reference.
 *
 * provenance_state reads the Interpretation's existing ProvenanceReference
 * (interpretation_provenance()/provenance_reference_state()) as "present"
 * or "absent". No analyzer upstream (in Ursatz or ursatz-analyzer) calls
 * provenance_reference_create_present() today, so "absent" is the only
 * value real output produces -- that reflects the current state of the
 * upstream analyzers, not a limitation of this field, and no resolvable
 * Provenance record type exists yet to point a "present" reference at.
 */
/*
 * highlighted_note_ids: the MusicXML xml:id of every NoteEvent this
 * claim's target resolves to, empty for types where no resolution
 * exists yet. For type "phrase" the target is directly a NoteEvent id
 * (resolved on the fly from the Interpretation's own target_id). For
 * "cadence"/"motif" the target is a transient Structure/Occurrence that
 * is never itself persisted (see kNoteTargetTableName in
 * analysis_service.c) -- its event ids are captured at analysis time,
 * while that object is still alive, and looked up here by
 * interpretation_id. Relationship-shaped types (period/sentence/form/
 * thematic_return) and key/chord are not resolved this pass; this array
 * is empty for those, not an error.
 */
typedef struct {
    char *interpretation_id;
    char *type;              /* "key" or "chord" */
    char *label;              /* e.g. "D major", "V7" */
    char *produced_by_author; /* Interpretation's author field */
    char *produced_by_method; /* Interpretation's method field */
    char *provenance_state;   /* "present" or "absent" */
    char **highlighted_note_ids;
    size_t highlighted_note_id_count;
} UrsatzGuiInterpretationSummary;

/*
 * One persisted Relationship, as returned by ursatz_gui_analysis_get_piece.
 * Period/Sentence/Form Structure/Thematic Return Detection's output is
 * Relationship-shaped (a source/predicate/target link between two other
 * ids), not a simple label like key/chord -- see analysis_service.c's
 * kRelationshipTableName for how/why this module persists Relationships
 * itself (Ursatz's persistence layer has no Relationship store).
 */
typedef struct {
    char *relationship_id;
    char *source_id;
    char *predicate_name;
    char *target_id;
} UrsatzGuiRelationshipSummary;

typedef struct {
    char *id;
    char *filename; /* original upload filename, or "" if none was given */
    size_t note_event_count;
    UrsatzGuiInterpretationSummary *interpretations;
    size_t interpretation_count;
    UrsatzGuiRelationshipSummary *relationships;
    size_t relationship_count;
} UrsatzGuiPieceDetail;

/*
 * One entry in ursatz_gui_analysis_list's result: a saved piece's id
 * plus the original filename it was uploaded as ("" if none was given).
 */
typedef struct {
    char *id;
    char *filename;
} UrsatzGuiPieceSummary;

/*
 * Stores data[0..size) (a MIDI or MusicXML file) unchanged as the source of
 * a new piece, imports it through the Analyzer library, and saves the
 * Analyzer's analysis document next to it, all under pieces_dir/<id>/.
 * original_filename (may be NULL or "") only picks the extension of a
 * stored MusicXML source and is recorded as the piece's display name.
 *
 * On URSATZ_GUI_ANALYSIS_OK, *out_id is a newly allocated, caller-owned id.
 * URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT if the bytes are neither MIDI
 * nor MusicXML; URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED if the import or the
 * analysis failed. On any error the piece directory is removed.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_import_and_save(const char *pieces_dir, const unsigned char *data,
                                                              size_t size, const char *original_filename,
                                                              char **out_id);

/*
 * Lists saved pieces found in pieces_dir (filenames minus ".db"), each
 * with its persisted original filename. On URSATZ_GUI_ANALYSIS_OK,
 * *out_pieces is a newly allocated array of *out_count
 * UrsatzGuiPieceSummary entries (caller frees each entry's id/filename,
 * then the array). A missing or empty pieces_dir yields *out_count ==
 * 0, *out_pieces == NULL, still URSATZ_GUI_ANALYSIS_OK.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_list(const char *pieces_dir, UrsatzGuiPieceSummary **out_pieces,
                                                  size_t *out_count);

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

/*
 * Deletes a saved piece's entire SQLite file under pieces_dir, removing
 * its NoteEvents, Interpretations, and the ursatz-gui-owned
 * interpretation index/meta/relationship tables in one step -- all of
 * it lives in that one per-piece file (see analysis_service.c's
 * kIndexTableName/kMetaTableName/kRelationshipTableName), so there is
 * no separate per-table cascade to perform.
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists
 * under pieces_dir.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_delete_piece(const char *pieces_dir, const char *id);

/*
 * Updates the persisted original_filename (see kMetaTableName) for the
 * saved piece at id -- the display name shown in the Library view. Does
 * not touch NoteEvents, Interpretations, or the interpretation index.
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists
 * under pieces_dir. Caller is responsible for rejecting an empty
 * new_filename before calling; this function stores whatever it is given.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_rename_piece(const char *pieces_dir, const char *id,
                                                          const char *new_filename);

/*
 * Opens the saved piece's PersistenceHandle via the same lookup path
 * ursatz_gui_analysis_get_piece uses (pieces_dir/id.db), then exports
 * it to MusicXML via Ursatz's musicxml_export_write. On
 * URSATZ_GUI_ANALYSIS_OK, *out_xml is a newly allocated, caller-owned,
 * NUL-terminated MusicXML document (free() it), and *out_cosmetic_meter
 * reports whether the export used a synthetic 4/4 grid rather than an
 * authored meter (see musicxml_export.h -- true today for every piece,
 * since Meter/MeterMap is not populated anywhere in Ursatz yet).
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists
 * under pieces_dir. URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED if
 * musicxml_export_write itself fails (e.g. a NoteEvent missing pitch
 * spelling, or overlapping notes -- see musicxml_export.h); in that case
 * *out_export_error is set to the specific MusicXmlExportResult
 * musicxml_export_write returned, so callers can surface it instead of
 * just the generic PERSISTENCE_FAILED. Left as MUSICXML_EXPORT_OK for
 * every other outcome, including success.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_cosmetic_meter,
                                                                MusicXmlExportResult *out_export_error);

#endif /* URSATZ_GUI_ANALYSIS_SERVICE_H */
