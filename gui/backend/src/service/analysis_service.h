#ifndef URSATZ_GUI_ANALYSIS_SERVICE_H
#define URSATZ_GUI_ANALYSIS_SERVICE_H

#include <stdbool.h>
#include <stddef.h>


/*
 * Shared analysis service: import, analyze and store a piece.
 *
 * This is the ONLY place outside persistence_service.c and
 * collection_service.c allowed to call into Ursatz or the Analyzer. Every
 * analysis goes through the Analyzer library (input_pipeline_import_file,
 * run_analysis_pipeline, analysis_document_build); the GUI keeps no
 * analysis code of its own (R-084).
 *
 * A piece is a directory pieces_dir/<id>/ holding:
 *   source.mid | source.musicxml | source.xml | source.mxl   the upload, unchanged
 *   analysis.json                                            the Analyzer's document
 *   meta.json                                                filename, format, uploaded_at,
 *                                                            analyzer_version, document_schema_version,
 *                                                            composer, source, tags (R-103; absent = unset),
 *                                                            notation (R-110; absent = no override)
 * pieces_dir is created if it does not already exist.
 */

typedef enum {
    URSATZ_GUI_ANALYSIS_OK = 0,
    URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED,
    URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED,
    URSATZ_GUI_ANALYSIS_ERR_ALLOC,
    URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND,
    URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT,
    URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA
} UrsatzGuiAnalysisResult;

/*
 * A piece's notation override (R-110): how its notes are to be read as notation.
 * Each member is the text the Analyzer's notation_reading_parse_* reads, or ""
 * for "not set": meter "3/4", value_scale "2" (1/4, 1/2, 1, 2 or 4), pickup
 * "1/4" (whole notes, after scaling).
 */
typedef struct {
    char meter[16];
    char value_scale[8];
    char pickup[16];
} UrsatzGuiNotation;

typedef struct {
    char *id;
    char *filename;    /* upload name, or "" if none was given */
    char *format;      /* "midi" or "musicxml" */
    char *uploaded_at; /* ISO 8601 UTC */
    bool has_notation;   /* true when the piece has a notation override */
    UrsatzGuiNotation notation;
    char *document_json; /* the Analyzer's analysis document, as stored */
    char **note_xml_ids; /* note_xml_ids[i]: the <note xml:id> the score export writes for note i */
    size_t note_count;
} UrsatzGuiPieceDetail;

/* One entry in ursatz_gui_analysis_list's result. */
typedef struct {
    char *id;
    char *filename;
    char *format;
    char *uploaded_at;
    char *composer;    /* "" when unset */
    char *source;      /* "" when unset */
    char **tags;       /* tag_count normalized tags in insertion order; NULL when none */
    size_t tag_count;
    bool has_notation; /* true when the piece has a notation override */
    UrsatzGuiNotation notation;
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
 * Lists the pieces in pieces_dir: every pieces_dir/<id>/ directory with a
 * readable meta.json, newest upload first, then by id. Anything else in
 * pieces_dir (old "<id>.db" pieces, stray files) is ignored. On
 * URSATZ_GUI_ANALYSIS_OK, *out_pieces is a newly allocated array of
 * *out_count entries (caller frees each entry's strings, then the array).
 * A missing or empty pieces_dir yields *out_count == 0, *out_pieces == NULL.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_list(const char *pieces_dir, UrsatzGuiPieceSummary **out_pieces,
                                                  size_t *out_count);

/*
 * Loads one piece: its meta.json fields, its stored analysis document, and
 * the xml:id the score export writes for each note of the source, in the
 * order of the document's note_ids (R-085).
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists. On
 * URSATZ_GUI_ANALYSIS_OK, *out_detail is newly allocated and caller-owned
 * (free with ursatz_gui_piece_detail_destroy).
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece(const char *pieces_dir, const char *id,
                                                       UrsatzGuiPieceDetail **out_detail);

void ursatz_gui_piece_detail_destroy(UrsatzGuiPieceDetail *detail);

/*
 * Deletes a piece's directory and every file in it.
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_delete_piece(const char *pieces_dir, const char *id);

/*
 * Changes a piece's metadata in its meta.json, written once. The source and
 * the analysis are untouched. NULL string fields are left unchanged.
 *
 *   filename            display name, stored as given (the caller rejects an empty one)
 *   composer, source    normalized (ursatz_gui_meta_normalize_value); "" clears
 *   has_tags, tags      replace all tags (each normalized with ursatz_gui_meta_normalize_tag)
 *   add_tags            then added (a duplicate is skipped)
 *   remove_tags         then removed (absent is not an error)
 *   has_notation        replace the notation override with *notation, or clear it
 *                       when notation is NULL
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists.
 * URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA if a value is too long, a tag is
 * invalid, the piece would hold more than 32 tags or a notation member is not
 * one the Analyzer reads; nothing is written and message (may be NULL)
 * receives "composer is too long", "source is too long", "invalid tag",
 * "too many tags" or "invalid notation".
 */
typedef struct {
    const char *filename;
    const char *composer;
    const char *source;
    bool has_tags;
    const char *const *tags;
    size_t tag_count;
    const char *const *add_tags;
    size_t add_tag_count;
    const char *const *remove_tags;
    size_t remove_tag_count;
    bool has_notation;
    const UrsatzGuiNotation *notation;
} UrsatzGuiMetaUpdate;

UrsatzGuiAnalysisResult ursatz_gui_analysis_update_metadata(const char *pieces_dir, const char *id,
                                                             const UrsatzGuiMetaUpdate *update, char *message,
                                                             size_t message_size);

/*
 * Reads the piece's notation override from its meta.json: *out_has is false
 * (and *out is zeroed) when it has none.
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_get_notation(const char *pieces_dir, const char *id, bool *out_has,
                                                          UrsatzGuiNotation *out);

/*
 * Rewrites the filename in the piece's meta.json (the display name shown in
 * the Library view): ursatz_gui_analysis_update_metadata with only filename.
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists. The
 * caller rejects an empty new_filename; this stores whatever it is given.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_rename_piece(const char *pieces_dir, const char *id,
                                                          const char *new_filename);

/*
 * Runs the Analyzer again on the piece's stored source and rewrites
 * analysis.json, then the analyzer_version and document_schema_version in
 * meta.json (filename, format and uploaded_at stay). The piece is unchanged
 * unless the new analysis succeeded.
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists.
 * URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED if the stored source no longer
 * imports or the analysis fails.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_reanalyze(const char *pieces_dir, const char *id);

/*
 * Imports the piece's stored source and exports it to MusicXML with Ursatz's
 * analyzer_score_export_musicxml: the source's parts and staves, its meter
 * (the MIDI time signatures or the score's own) and its key signatures in
 * part 0; tempo markings and dynamics are left out. On
 * URSATZ_GUI_ANALYSIS_OK, *out_xml is a newly allocated, caller-owned,
 * NUL-terminated MusicXML document (free() it) and *out_meter_defaulted
 * says the source gave no meter, so the barlines come from a default 4/4.
 *
 * URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id exists.
 * URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED if the stored source no longer
 * imports. URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED if the export itself
 * fails; message is then the name of the AnalyzerStatus it returned and is
 * empty for every other outcome. The title (the filename without its
 * extension) and the composer are added to the score here: the Analyzer
 * leaves both out.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted,
                                                                char message[256]);

/*
 * Imports the piece's stored source with the same Analyzer call the score
 * export uses (so note i here is note i of the document's note_ids and of
 * note_xml_ids) and writes what a browser needs to play it and follow it:
 *
 *   {"ok": true,
 *    "tempo_source": "file" | "score" | "default",
 *    "tempo": [{"at": [0, 1], "qpm": 120}],
 *    "parts": [{"index": 0, "name": "", "program": 0}],
 *    "end": [num, den],
 *    "notes": [{"i": 0, "on": [0, 1], "len": [1, 8], "pitch": 60, "part": 0}]}
 *
 * Positions and lengths are whole notes from the piece start as [num, den]
 * in lowest terms. pitch is the MIDI note number. part is the note's stream
 * (MIDI track chunk or MusicXML part index); parts lists each distinct one,
 * ascending, with the MusicXML part name ("" for MIDI) and program 0 (a
 * NoteEvent carries no program). tempo is strictly ascending by "at", qpm
 * in quarter notes per minute to 3 decimals: a MIDI file's set-tempo
 * regions, or a MusicXML score's declared tempo and metronome marks, or 120
 * when the source gives none. end is the largest on + len, [0, 1] without
 * notes.
 *
 * On URSATZ_GUI_ANALYSIS_OK, *out_json is newly allocated and caller-owned
 * (free() it). URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND if no piece with this id
 * exists; URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED if the stored source no
 * longer imports; URSATZ_GUI_ANALYSIS_ERR_ALLOC if the JSON cannot be built.
 */
UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_playback(const char *pieces_dir, const char *id,
                                                                char **out_json);

#endif /* URSATZ_GUI_ANALYSIS_SERVICE_H */
