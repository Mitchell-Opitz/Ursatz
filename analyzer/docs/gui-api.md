# GUI API

What the GUI asks the library for today, as public Analyzer headers (R-218): import with tempo, playback timing and
score export. Each gives the output the GUI's own code gave at its pin (`src/service/analysis_service.c`). The GUI
switches onto them in P27. Storage of pieces, titles and composers stays a GUI edge.

## Import with tempo

No new entry. `input_pipeline_import_file` (`<ursatz_analyzer/input_pipeline.h>`) fills `ImportedNotes.tempo_map` from
the one library parse of a MIDI file: its set-tempo events, or one defaulted region when it has none. A MusicXML import
has no tempo map; its tempo is in `ImportedSource` (`tempo_declared`, `initial_quarters_per_minute`, `tempo_markings`).

## Playback timing

`<ursatz_analyzer/playback.h>`

```c
ImportedNotes notes;
AnalyzerPlayback playback;
char *json = NULL;
if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_OK &&
    analyzer_playback_build(&notes, &playback) == ANALYZER_OK) {
    if (analyzer_playback_to_json(&playback, &notes, &json) == ANALYZER_OK) {
        /* send json, then free(json) */
    }
    analyzer_playback_destroy(&playback);
}
imported_notes_destroy(&notes);
```

| Field | Meaning |
| --- | --- |
| `tempo_source` | `"file"` (the MIDI file's set-tempo events), `"score"` (a MusicXML tempo and metronome marks), `"default"` (none given: 120 from 0). |
| `tempo` | `AnalyzerTempoRegion` list, in time order, the first at 0. `quarters_per_minute` is rounded to thousandths. |
| `notes` | One `AnalyzerPlaybackNote` per imported note: `index`, `onset` and `length` in whole notes, `midi` clamped to 0 to 127, `part`. |
| `end` | The latest onset plus length. |

A MusicXML metronome mark at the position of an earlier region replaces it. When a notation reading with a value scale
was applied (`notation_reading_apply`), every region's tempo is multiplied by the scale (a scale of 2 doubles it) so the
piece sounds as fast as before; a MIDI file's region positions are scaled too, a score's marks were scaled with its
notes.

The JSON is `{"ok":true,"tempo_source":s,"tempo":[{"at":[n,d],"qpm":q}],"parts":[{"index":i,"name":s,"program":0}],
"end":[n,d],"notes":[{"i":i,"on":[n,d],"len":[n,d],"pitch":m,"part":p}]}`. `parts` lists the parts that have a note; the
name is the MusicXML part name, empty for a MIDI file.

| `AnalyzerStatus` | Meaning |
| --- | --- |
| `ANALYZER_ERR_ARGUMENT` | A NULL argument, or a MIDI import with no tempo map. |
| `ANALYZER_ERR_LIBRARY` | A tempo region the library cannot read, or a time that does not fit. |
| `ANALYZER_ERR_ALLOC` | Out of memory. |

## Score export

`<ursatz_analyzer/score_export.h>`: `analyzer_score_export_musicxml(&notes, &xml)` writes the imported notes as MusicXML
3.1 (free with `free()`): parts with their General MIDI programs, staves, the source's own meter, key signatures in
part 0, times quantized to notated values. Tempo markings, dynamics, the title and the composer are not written; the
GUI keeps the last two with the piece. `ANALYZER_ERR_LIBRARY` when the library cannot write the score.

## Tests

`test_playback` and `test_score_export` compare the five reference pieces (MIDI) and `tests/musicxml/pickup.musicxml`
and `field-5.musicxml` against `tests/snapshots/gui/<stem>.playback.json` and `.musicxml`
(`UPDATE_SNAPSHOTS=1` rewrites them). Their `_smoke` runs (Bach 114 and pickup) and valgrind runs are quick.
