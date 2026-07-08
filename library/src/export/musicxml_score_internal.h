#ifndef URSATZ_EXPORT_MUSICXML_SCORE_INTERNAL_H
#define URSATZ_EXPORT_MUSICXML_SCORE_INTERNAL_H

#include "export/musicxml_export.h"

/*
 * Internal to music-export. The same as musicxml_export_write_score, except that an overlap inside one voice, chords
 * included, fails with MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES instead of being written. musicxml_export_write (the
 * persistence path) uses it so that path keeps its documented behaviour.
 */
MusicXmlExportResult musicxml_export_write_score_strict(const MusicXmlExportScore *score, char **out_xml);

#endif
