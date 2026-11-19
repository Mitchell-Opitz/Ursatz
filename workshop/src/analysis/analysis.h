#ifndef WORKSHOP_ANALYSIS_ANALYSIS_H
#define WORKSHOP_ANALYSIS_ANALYSIS_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include "items/library_pieces.h"


/*
 * Shared analysis service: import, analyze and store a piece.
 *
 * This is the ONLY place outside collection_service.c allowed to call
 * into Ursatz or the Analyzer. Every
 * analysis goes through the Analyzer library (analyzer_import_file,
 * analyzer_analyze, analyzer_document_build); the GUI keeps no
 * analysis code of its own (R-084).
 *
 * A piece is a directory pieces_dir/<id>/ holding:
 *   source.mid | source.musicxml | source.xml | source.mxl   the upload, unchanged
 *   analysis.json                                            the Analyzer's document
 *   meta.json                                                filename, format, uploaded_at,
 *                                                            analyzer_version, document_schema_version,
 *                                                            ursatz_commit,
 *                                                            composer, source, tags (R-103; absent = unset),
 *                                                            notation (R-110; absent = no override)
 * pieces_dir is created if it does not already exist.
 */

typedef struct {
    char *id;
    char *filename;    /* upload name, or "" if none was given */
    char *format;      /* "midi" or "musicxml" */
    char *uploaded_at; /* ISO 8601 UTC */
    bool has_notation;   /* true when the piece has a notation override */
    WorkshopNotation notation;
    const char *analysis_status; /* workshop_analysis_status_name of the stored document; static text */
    char *document_json; /* the Analyzer's analysis document, as stored */
    char **note_xml_ids; /* note_xml_ids[i]: the <note xml:id> the score export writes for note i */
    size_t note_count;
} WorkshopPieceDetail;

/*
 * Stores data[0..size) (a MIDI or MusicXML file) unchanged as the source of
 * a new piece, imports it through the Analyzer library, and saves the
 * Analyzer's analysis document next to it, all under pieces_dir/<id>/.
 * original_filename (may be NULL or "") only picks the extension of a
 * stored MusicXML source and is recorded as the piece's display name.
 *
 * On URSATZ_OK, *out_id is a newly allocated, caller-owned id.
 * URSATZ_ERR_REQUEST if the bytes are neither MIDI
 * nor MusicXML; URSATZ_ERR_FORMAT if the import or the
 * analysis failed. On any error the piece directory is removed.
 */
UrsatzStatus workshop_analysis_import_and_save(const char *pieces_dir, const unsigned char *data,
                                                              size_t size, const char *original_filename,
                                                              char **out_id);

/*
 * Loads one piece: its meta.json fields, its stored analysis document, and
 * the xml:id the score export writes for each note of the source, in the
 * order of the document's note_ids (R-085).
 *
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists. On
 * URSATZ_OK, *out_detail is newly allocated and caller-owned
 * (free with workshop_piece_detail_destroy).
 */
UrsatzStatus workshop_analysis_get_piece(const char *pieces_dir, const char *id,
                                                       WorkshopPieceDetail **out_detail);

void workshop_piece_detail_destroy(WorkshopPieceDetail *detail);

/*
 * The members of an analysis document named in names[0..count), as one JSON object (a name the document lacks is
 * left out). When out_names_json is not NULL it receives a JSON array of every top-level member name of the whole
 * document. Both outputs are caller-owned (free()). False when the text is not a JSON object or on allocation
 * failure.
 */
bool workshop_document_subset(const char *document_json, const char *const *names, size_t count,
                                char **out_json, char **out_names_json);

/*
 * Reads a piece's stored analysis.json under the piece lock and returns only the named sections
 * (workshop_document_subset) without importing the source. With names NULL it returns the whole stored document
 * text unchanged (count is ignored). URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 */
UrsatzStatus workshop_analysis_get_document(const char *pieces_dir, const char *id,
                                                          const char *const *names, size_t count, char **out_json);

/*
 * Deletes a piece's directory and every file in it.
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 */
UrsatzStatus workshop_analysis_delete_piece(const char *pieces_dir, const char *id);

/*
 * The PATCH of a piece as one hold of its lock: the metadata change above, then, when the update changes
 * the notation override, workshop_analysis_reanalyze; if that fails the old override is put back, and a source
 * that no longer imports gives URSATZ_ERR_REQUEST with message "invalid notation".
 */
UrsatzStatus workshop_analysis_update_piece(const char *pieces_dir, const char *id,
                                                          const WorkshopMetaUpdate *update, UrsatzError *error);

/*
 * Runs the Analyzer again on the piece's stored source and rewrites
 * analysis.json, then the analyzer_version and document_schema_version in
 * meta.json (filename, format and uploaded_at stay). The piece is unchanged
 * unless the new analysis succeeded.
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 * URSATZ_ERR_FORMAT if the stored source no longer
 * imports or the analysis fails.
 */
UrsatzStatus workshop_analysis_reanalyze(const char *pieces_dir, const char *id);

/*
 * Makes a piece's stored analysis current, under the piece lock. A meta.json without ursatz_commit gets it from the
 * stored analysis.json's metadata.producer_key (written back). When analyzer_document_status of analysis.json is
 * not CURRENT (another schema, Analyzer version or Ursatz commit, or unreadable), the piece is analyzed again as in
 * workshop_analysis_reanalyze. OK when the stored analysis is current afterwards; otherwise the failure of the new
 * analysis, with the stored files unchanged. URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 */
UrsatzStatus workshop_analysis_ensure_current(const char *pieces_dir, const char *id);

/*
 * Imports the piece's stored source and exports it to MusicXML with Ursatz's
 * analyzer_score_export_musicxml: the source's parts and staves, its meter
 * (the MIDI time signatures or the score's own) and its key signatures in
 * part 0; tempo markings and dynamics are left out. On
 * URSATZ_OK, *out_xml is a newly allocated, caller-owned,
 * NUL-terminated MusicXML document (free() it) and *out_meter_defaulted
 * says the source gave no meter, so the barlines come from a default 4/4.
 *
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 * URSATZ_ERR_FORMAT if the stored source no longer
 * imports. URSATZ_ERR_REQUEST if the Analyzer refuses the import,
 * URSATZ_ERR_INTERNAL if the export itself fails. The title (the filename without its
 * extension) and the composer are added to the score here: the Analyzer
 * leaves both out.
 */
UrsatzStatus workshop_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted);

/*
 * Whether the piece's source gave no meter, so its score has barlines from a default 4/4: the answer of
 * analyzer_imported_meter_defaulted on the cached import (import_cache.h), without exporting a score. Same errors
 * as workshop_analysis_get_piece_playback.
 */
UrsatzStatus workshop_analysis_get_piece_meter_defaulted(const char *pieces_dir, const char *id,
                                                                       bool *out_meter_defaulted);

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
 * On URSATZ_OK, *out_json is newly allocated and caller-owned
 * (free() it). URSATZ_ERR_NOT_FOUND if no piece with this id
 * exists; URSATZ_ERR_FORMAT if the stored source no
 * longer imports; URSATZ_ERR_ALLOC if the JSON cannot be built.
 */
UrsatzStatus workshop_analysis_get_piece_playback(const char *pieces_dir, const char *id,
                                                                char **out_json);

/*
 * The piece's text export (the Analyzer's analyzer_text_export_piece, docs/text-export.md), under the piece lock: the
 * cached import, the stored analysis.json (made current first, as workshop_analysis_get_piece does) and the title
 * from meta.json (the filename without its extension). first_measure and last_measure are an inclusive range of
 * measure numbers as written in the score; 0 and 0 export the whole piece. The GUI builds no line of the text.
 *
 * On URSATZ_OK, *out_text is newly allocated and caller-owned (free() it).
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists. URSATZ_ERR_FORMAT if the
 * stored source no longer imports. URSATZ_ERR_REQUEST if the Analyzer refused the export (an empty range, or a stored
 * document it will not read); URSATZ_ERR_INTERNAL if the export failed.
 */
UrsatzStatus workshop_analysis_get_piece_export(const char *pieces_dir, const char *id,
                                                              int first_measure, int last_measure, char **out_text);

#endif /* WORKSHOP_ANALYSIS_ANALYSIS_H */
