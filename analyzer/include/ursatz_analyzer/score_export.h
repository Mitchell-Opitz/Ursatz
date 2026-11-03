#ifndef URSATZ_ANALYZER_SCORE_EXPORT_H
#define URSATZ_ANALYZER_SCORE_EXPORT_H

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/imported_notes.h>

/*
 * The score of an import as MusicXML (R-218), written the way the GUI wrote it before the Analyzer owned it: from the
 * imported notes, their parts, staves and General MIDI programs, the source's own meter, and its key signatures in
 * part 0, with times quantized to notated values (score_quantize_options_default). Tempo markings and dynamics are left
 * out, and so are the title and composer: the caller keeps those with the piece and they are not in an import.
 *
 * On ANALYZER_OK *xml is a MusicXML 3.1 string the caller frees with free(); otherwise *xml is NULL.
 * ANALYZER_ERR_ARGUMENT for a NULL argument or an import with a note count but no events; ANALYZER_ERR_LIBRARY when the
 * library cannot write the score (overlapping or unwritable times, a missing spelling); ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_score_export_musicxml(const ImportedNotes *notes, char **xml);

/*
 * The base xml:id of each note of an import in the MusicXML analyzer_score_export_musicxml writes (R-273), in
 * ImportedNotes order: the library's musicxml_export_note_xml_id of the note's EntityID value, cut to 63 characters as
 * the export cuts it. A tied note's segments carry this id with the suffixes the export adds (_a to _z, then _26 and
 * on), so a caller matches an id as the base or the base plus a suffix.
 *
 * On ANALYZER_OK *ids is an array of *count strings (NULL and 0 for an import with no notes) that the caller frees with
 * analyzer_score_note_xml_ids_destroy; otherwise *ids is NULL and *count is 0. ANALYZER_ERR_ARGUMENT for a NULL argument
 * or an import with a note count but no events; ANALYZER_ERR_LIBRARY when an id cannot be formed; ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_score_note_xml_ids(const ImportedNotes *notes, char ***ids, size_t *count);

/* Frees what analyzer_score_note_xml_ids gave. NULL is allowed. */
void analyzer_score_note_xml_ids_destroy(char **ids, size_t count);

/*
 * True when the import's meter was not read from the source: its meter map is missing or defaulted to 4/4
 * (meter_map_is_defaulted), as for a MIDI file with no time signature. False for a NULL argument.
 */
bool analyzer_imported_meter_defaulted(const ImportedNotes *notes);

#endif
