#ifndef URSATZ_ANALYZER_PLAYBACK_H
#define URSATZ_ANALYZER_PLAYBACK_H

#include <stddef.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/imported_notes.h>

#include "core/rational.h"

/*
 * Playback timing of an import: the tempo regions and the notes as a player needs them. It is what the GUI asked the
 * library for before the Analyzer owned it (R-218), with the same values: the tempo source names, the tempo rounded to
 * thousandths of a quarter note per minute, the MIDI number clamped to 0 to 127.
 *
 * tempo_source is "file" (the MIDI file's own set-tempo events), "score" (a MusicXML score's declared tempo and
 * metronome marks) or "default" (the file or score gives none, so 120 quarter notes per minute from 0). It points to a
 * static string. tempo is in time order, never empty, the first region at 0; a MusicXML metronome mark at the position of
 * an earlier region replaces it. at is in whole notes. When a notation reading with a value scale was applied, the
 * tempo is multiplied by the scale (a scale of 2 doubles it) so the piece sounds as fast as before; a MIDI file's region
 * positions are scaled too, a score's marks were scaled with its notes.
 *
 * notes are in the order of the ImportedNotes, index being that order. onset and length are in whole notes, midi is
 * 0 to 127, part is the ImportedNotes stream (the part index). end is the latest onset plus length, 0 for no notes.
 */
typedef struct {
    Rational at;
    double quarters_per_minute;
} AnalyzerTempoRegion;

typedef struct {
    size_t index;
    Rational onset;
    Rational length;
    int midi;
    size_t part;
} AnalyzerPlaybackNote;

typedef struct {
    const char *tempo_source;
    AnalyzerTempoRegion *tempo;
    size_t tempo_count;
    AnalyzerPlaybackNote *notes; /* NULL when note_count is 0 */
    size_t note_count;
    Rational end;
} AnalyzerPlayback;

/*
 * Builds the playback of an import (input_pipeline_import_file or a pipeline import: a MIDI import carries its tempo
 * map, B4). ANALYZER_OK fills *out, which the caller releases with analyzer_playback_destroy; any other status leaves it
 * zeroed. ANALYZER_ERR_ARGUMENT for a NULL argument or a MIDI import with no tempo map; ANALYZER_ERR_LIBRARY for a tempo
 * region the library cannot read or a time that does not fit; ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_playback_build(const ImportedNotes *notes, AnalyzerPlayback *out);

/* Releases a playback and zeroes it. NULL is a no-op. */
void analyzer_playback_destroy(AnalyzerPlayback *playback);

/*
 * The playback as JSON text for a player: {"ok":true,"tempo_source":s,"tempo":[{"at":[num,den],"qpm":q}],
 * "parts":[{"index":i,"name":s,"program":0}],"end":[num,den],"notes":[{"i":i,"on":[num,den],"len":[num,den],
 * "pitch":m,"part":p}]}. Positions are [numerator, denominator] pairs in whole notes. parts lists each part that has a
 * note, by index; the name is the MusicXML part name, empty for a MIDI file. notes is the ImportedNotes the playback was
 * built from. On ANALYZER_OK *out is a string the caller frees with free(); otherwise *out is NULL.
 */
AnalyzerStatus analyzer_playback_to_json(const AnalyzerPlayback *playback, const ImportedNotes *notes, char **out);

#endif
