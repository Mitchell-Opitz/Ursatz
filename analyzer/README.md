# Ursatz-Analyzer

A small C11 command-line tool that turns a MIDI or MusicXML file into a Markdown report and a JSON analysis
document: the piece's time signature, measure count and pickup, its local keys over time (key
regions, with the home key, tonicizations, modulations and pivot chords), and the most likely chord
for each group of notes that start together, each located by measure and beat. It is a
thin consumer of the [Ursatz](https://github.com/Mitchell-Opitz/Ursatz) library and its
`common-practice-minimal` framework.

## Build

Requires CMake 3.20+, a C11 compiler, and SQLite3 (for Ursatz). Ursatz is a git submodule at
`external/ursatz`, pinned to Ursatz main at PR #112 (`6a0853b`).

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests:

- `test_analysis_pipeline`, `test_report_output`: end-to-end pipeline and report file.
- `test_corpus_key_estimation`: the home key of six reference pieces.
- `test_key_regions_clementi_36`, `test_key_regions_mozart_545`: the key regions and principal
  modulations of Clementi Sonatina Op. 36 No. 1 and Mozart K. 545 against hand-read regions. Both are
  marked `WILL_FAIL`: Ursatz's detection does not reach the target yet (see "Known limits").
- `test_key_regions_constructed`: a piece with no modulation, one with a tonicization only, one with a
  modulation, and a notated key signature the music does not fit.
- `test_long_labels`: labels past 31 characters render intact.
- `test_analysis_document`: the analysis document's sections, claims and values.
- `test_measure_locations`: time signature, measure count, pickup, meter changes, and the measure
  and beat of the chords covering chosen positions, for the five reference pieces and two
  constructed files (a pickup and a file with no time signature).
- `test_chord_coverage`: chord coverage of the five reference pieces. Field Nocturne No. 1 must be
  above 90% (marked `WILL_FAIL` until it is: see "Known limits"); the others must still produce chords. See "Chords and non-chord tones".
- `test_chord_details`: inversion, runner-up present and absent, and four non-chord-tone types and
  an unclassified one, on small constructed files.
- `test_cli_output_flags`: which files each output flag writes.
- `test_musicxml_midi_agreement`: the five reference pieces from their MusicXML versions
  (`tests/musicxml`) and from their MIDI files: key regions, measure count and meter must agree
  exactly, chords and outer-voice scale degrees to the tolerance stated in the test, and the
  measured agreement is printed.
- `test_musicxml_input`: format choice, notated key and meter, a pickup, a multi-part score, notated
  spelling (Db vs C#) and every error case.
- `test_cli_input_formats`: `--input-format`, format detection and the error exits of the CLI.
- `test_analysis_document_schema`: validates the documents for the corpus pieces against the
  schema. Optional: needs `python3` with the `jsonschema` module at configure time, and is
  reported as skipped otherwise.

## Usage

```sh
ursatz-analyzer [--json] [--markdown] [--input-format midi|musicxml] <input-file-path>
```

Writes two files next to the input file:

- `<stem>.report.md`: the Markdown report.
- `<stem>.analysis.json`: the analysis document. See
  [docs/analysis-document.md](docs/analysis-document.md) and the
  [schema](schema/analysis-document.v7.schema.json).

`--markdown` writes only the report, `--json` writes only the analysis document. No flag, or both,
writes both. `--input-format` forces the input format (see "Input formats"). Flags can come before or
after the path. Exits 1 with a message on an unknown flag, a
missing path, bad input, allocation failure, or write failure.

## Library

The pipeline builds as the `ursatz_analyzer` static library. The CLI and the tests link it.
Its public include directory is `include/`, and consumers include headers as
`<ursatz_analyzer/xxx.h>`. Everything under `src/` is private.

| Header | Purpose |
| --- | --- |
| `input_pipeline.h` | The common input interface: `input_pipeline_import_file` reads a MIDI or MusicXML file (chosen by `InputFormat`, or detected) into `ImportedNotes`. |
| `imported_notes.h` | `ImportedNotes`, what every input produces (notes, meter, key signatures, and an `ImportedSource` saying what the input states); `imported_notes_destroy` frees it. |
| `midi_pipeline.h` | The MIDI backend of the input interface. |
| `musicxml_pipeline.h` | The MusicXML backend of the input interface. |
| `analysis_pipeline.h` | `run_analysis_pipeline` turns `ImportedNotes` into an `AnalysisReport`. |
| `analysis_report.h` | `AnalysisReport` and its entries (segment span, chord readings, non-chord tones), key regions and key changes. |
| `analysis_document.h` | Builds the `AnalysisDocument` from an `AnalysisReport`, serializes it as JSON, and derives `<stem>.analysis.json`. |
| `markdown_report.h` | Renders an `AnalysisDocument` as the Markdown report, section by section. |
| `report_writer.h` | Derives the `<stem>.report.md` path and writes the file. |

Ownership: the caller owns the `AnalysisReport` and frees it with `analysis_report_destroy`,
which also frees every entry's labels. Labels are heap strings with no length limit; "no match"
marks a group or key that was not identified. It also frees each entry's `note_ids`. The caller
frees an `AnalysisDocument` with `analysis_document_destroy`; it keeps no reference to the notes
or the report, so those can be destroyed as soon as it is built. Strings returned by
`markdown_report_render`, `analysis_document_to_json`, `analysis_document_render`,
`derive_report_path` and `derive_analysis_document_path` are freed by the caller with `free`.

```cmake
add_subdirectory(external/ursatz-analyzer)
target_link_libraries(my_app PRIVATE ursatz_analyzer)
```

## Pipeline

`main.c` runs `input_pipeline` (MIDI or MusicXML import through Ursatz, both into `ImportedNotes`), then `analysis_pipeline`:

1. `segmentation`: cuts the notes into sonority segments with Ursatz's adaptive segmentation, on the
   beats of the file's meter, and builds each segment's members for chord identification.
2. `key_region_pipeline`: finds the local key regions with Ursatz's key-region detection.
3. `chord_pipeline`: identifies each segment's chord with Ursatz's weighted chord identification,
   with its runner-up and the segment's non-chord tones.
4. `outer_voice_pipeline`: extracts the soprano and bass of each segment with Ursatz's outer-voice
   extraction and names their scale degrees in the segment's local key.

`analysis_document` then builds the analysis document once. It is the single source of truth:
the JSON file is its serialization, and `markdown_report` renders the report from it, never from
the pipeline directly. `report_writer` writes each file.

## Input formats

The analyzer reads MIDI and MusicXML. Both go through the same pipeline: each input becomes the
same `ImportedNotes` (notes, a `MeterMap`, key signature events), so every later stage is
independent of the format, and the analyzer adds no parsing or music theory of its own. MusicXML is
read by Ursatz's MusicXMLImporter.

- **Supported files.** MIDI (`.mid`, `.midi`; SMF type 0 and 1) and uncompressed MusicXML
  `score-partwise` (`.musicxml`, `.xml`).
- **Choosing the format.** By extension first; when the extension says nothing, by the file's first
  bytes (an `MThd` header is MIDI, an XML document is MusicXML). `--input-format midi|musicxml` forces
  the format and overrides both. A file whose format cannot be told is an error that names the flag.
- **What is notated and what is inferred.** MusicXML states what MIDI does not: pitch spelling, the
  time and key signatures as written, the measures, a pickup, parts and voices. The report's Summary
  says which input was read and whether the meter, the key signature and the pitch spelling were
  *notated* (the score's own), *embedded in the file* (a MIDI event) or *inferred* (the input has none:
  4/4 is assumed, or the key is found from the notes alone). The same is in the analysis document's
  `metadata.input`. A key is always *detected* from the notes; a notated key signature is only a
  boundary for the detection (see "Key regions"), for either input.
- **Pitch spelling.** For a MusicXML score the chord roots and basses and the outer-voice pitch names
  follow the score's spelling (`Db` stays `Db`, `C#` stays `C#`). For MIDI, which has none, they are
  spelled by the pipeline's rule (flats in keys that read in flats). Key labels are always spelled by
  the rule. Where a transposing part's written pitch is not the sounding pitch, the sounding pitch is
  analyzed and the name falls back to the rule.
- **Several parts.** All parts are analyzed together as one texture: none is dropped, and the soprano
  and bass are the highest and lowest notes sounding in any part. Each note keeps its part, staff and
  voice (`ImportedSource` in `imported_notes.h`), and `metadata.input.parts` lists the parts with their
  note counts. Per-part and per-voice analysis is not done.
- **Tempo, dynamics, articulation.** Tempo and dynamics markings are imported and counted
  (`metadata.input.tempo`, `dynamics_count`) but not analyzed or reported beyond that. The importer
  does not expose articulation.
- **Errors.** Malformed XML, a root other than `score-partwise`, an unreadable value, a score with no
  notes, a compressed file and an unreadable file each stop with a message on stderr and exit 1, and
  no output file is written. Things the importer skips or rounds (a grace note, an unpitched note, a
  fractional alteration) are warnings on stderr and are counted in `metadata.input.warning_count`.
- **Not supported.** Compressed `.mxl` (the Ursatz importer reads uncompressed documents only; the
  analyzer reports it as unsupported rather than unzipping it: export the score as uncompressed
  MusicXML), `score-timewise`, grace notes, wedges (crescendo and diminuendo), additive meters
  (summed) and senza-misura. Repeats and endings are not unfolded: the score is analyzed once, in
  written order.
- **Schema.** The analysis document is schema version 6 (see
  [docs/analysis-document.md](docs/analysis-document.md)): `metadata.input` is new, and
  `meter.source` can be `"notated"`.

## Chords and non-chord tones

The analyzer adds no music theory of its own here: it renders what Ursatz's segmentation
(`sonority_builder_build_adaptive`) and weighted identification
(`common_practice_identify_weighted_sequence`) establish.

- **Segments.** A segment is a stretch of the piece with the same sounding notes over the same bass:
  the piece is cut at every beat of the meter and where the bass changes, and neighbours that sound
  the same pitch classes over the same bass are merged (not across a barline). A segment holds every
  note sounding in it, attacked or held over. A chord's `note_ids` are those notes.
- **Each chord** has a root, quality, bass note and inversion (0 root position, 1 first, 2 second,
  3 third), a duration in beats and the measure and beat where it starts and ends, and the library's
  confidence. The chord's `label` is the library's (a Roman numeral for a triad, the seventh quality
  for a seventh chord). Roots are spelled with flats in keys that read in flats.
- **Runner-up.** The library also reports the best-scoring chord on a different root, with its
  score. The analysis document always carries it. The report's Chords table shows it only when its
  score is within 0.10 of the best chord's (`RUNNER_UP_MARGIN_THRESHOLD` in
  `src/pipeline/markdown_report.c`): a close call is worth seeing, a clear win is not.
- **Non-chord tones.** Every note in a matched segment that is not a tone of the chord is listed in
  `non_chord_tones` and in the report's Non-chord tones section, with the library's name for the
  figure (`passing`, `neighbor`, `suspension`, `anticipation`, `appoggiatura`, `escape-tone`,
  `pedal`, ...), the note, where it sounds, and the chord it is non-chordal against. A tone the
  library cannot place in one of its figures is listed as `unclassified`, not dropped.
- **Chord coverage** is the fraction of the notes' sounding time, in all segments, that is a chord
  tone or a classified non-chord tone of an identified chord. `test_chord_coverage` measures it.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Outer voices

The analyzer adds no music theory of its own here: it renders what Ursatz's outer-voice extraction
(`outer_voice_extract`, `src/analysis/outer_voice.h`) and scale-degree labelling
(`common_practice_scale_degree_label`, `frameworks/common-practice-minimal/outer_scale_degrees.h`)
establish, in `src/pipeline/outer_voice_pipeline.c`.

- **Soprano and bass.** For each chord segment the soprano is the highest sounding note and the bass the
  lowest, attacked or held. Doubled pitches resolve to the note attacked most recently. The library
  smooths each line across short gaps and register leaps; a line it leaves empty (a rest, a bridged gap,
  or a note dropped by the register guard) is reported as a rest, never filled from another voice.
- **Scale degrees** are taken relative to the local key: the key of the region the chord lies in, so a
  note in a tonicized or modulated region is read against that region's key, not the home key. Degrees
  are the library's labels: `1` to `7`, with `b` or `#` for a chromatic alteration (`b7`, `#4`). In a
  minor region the diatonic degrees are the natural minor's, so the raised sixth and seventh read as
  `#6` and `#7`. A chord outside every key region has pitches but no degrees. Pitches are spelled with
  flats in keys that read in flats.
- **Uncertainty.** A degree is only as sure as the key it is read against, so each degree's
  `uncertainty` in the analysis document is the library's uncertainty for that key region (0 clearly
  this key, 1 another key fits as well or better).
- **Report layout.** The soprano and bass go in their own Outer voices table, one row per chord, and not
  as columns of the Chords table, which already has nine. A chord's span is one segment, so each line has
  one note per chord; the same note appears in consecutive rows when it is held across them.
- **Crossing voices and texture.** The soprano is whatever is highest, whatever voice plays it: an inner
  voice that rises above the melody becomes the soprano for as long as it is highest. Where the melody
  repeats a note, the soprano is the note attacked most recently. When one note sounds, soprano and bass
  are that note.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Meter and measure numbers

The meter comes from the file's time signature events, read by Ursatz's `MeterMap`. The report's
Meter section gives the time signature, the number of measures, a pickup if there is one, and any
later change of signature. The analysis document's `meter` section carries the same, and every
claim in the document has a `location` with the measure and beat where it starts and ends. The
report writes these as `m. N, beat B`.

- The first full measure is measure 1. A pickup (anacrusis) is measure 0, so the music after it
  starts at measure 1. A piece with no pickup has no measure 0. A pickup is found from what the file
  says (a partial first measure ended by a later time signature, or a short leading signature such
  as 1/8 before 12/8), never from the notes: a leading rest is a rest, not a pickup.
- A beat is the signature's beat: a quarter note in 4/4, 3/4 and 2/4, a dotted quarter in 6/8 and
  12/8. Beats count from 1. A fractional beat is shown as a decimal (`beat 2.5`); the document
  keeps the exact fraction.
- A file with no time signature event gets the library's default 4/4. The report and the document
  mark it as inferred, an assumption and not a fact about the piece. A MusicXML score's time
  signatures are its own `<time>` elements and are marked as notated.
- The measure and beat are the file's: the analyzer does not check them against a printed score.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Key regions, tonicizations and modulations

The analyzer adds no music theory of its own here: it renders what Ursatz's key-region detection
(`key_regions_estimate`, `frameworks/common-practice-minimal/key_regions.h`) establishes, in
`src/pipeline/key_region_pipeline.c`.

- **Key regions.** The piece is cut into windows of four measures, each scored against the 24 major and
  minor keys, and a path through the windows that charges for each key change gives the local key over
  time. Regions run from barline to barline. Each has a key, a span located by measure and beat, the
  measures it covers, and the library's uncertainty (0 clearly this key, 1 another key fits as well or
  better). Key labels use flat spelling (`Eb`, `Db`) when the tonic needs fewer flats than sharps,
  for every region. Chord roots and basses are spelled the same way, following the region.
- **Home key.** The library gives no determination of its own, so the home key is the key with the
  greatest total time over all the regions in it, a tie going to the key that appears first. The
  opening and closing regions are reported as well. The home key is not necessarily either of them.
- **Notated key signatures.** The file's key signature events are input metadata
  (`metadata.key_signatures`) and a boundary for the library: the region that starts at one is
  restricted to the signature's major key and its relative minor. They never decide a key on their
  own. Where the detected keys do not fit a notated signature, the document's `notes` and the report
  say so; neither side is preferred.
- **Tonicizations and modulations.** The library classes each change between neighbouring regions: a
  region shorter than four measures that returns to the previous key is a tonicization; a longer one
  that returns is a weak modulation; one that does not return is a modulation. No cadence is looked
  for, so "modulation" means a lasting new key region. The analyzer reports the library's
  classification exactly and applies no rule of its own.
- **Pivot chords.** For a modulation, the library names the last segment before the new key whose
  chord is diatonic in both keys, within two measures. The analysis document gives the chord by root,
  quality, bass and inversion (no Roman numeral) with the two keys. A modulation with none is direct.

Roman numerals, applied-chord notation and harmonic function are out of scope here.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Known limits

- Key regions use fixed four-measure windows, so a boundary can land a measure or two from where a
  phrase would put it and a modulation shorter than about half a window is smoothed away. A stretch of
  four or more measures built on the subdominant, dominant or supertonic can read as its own key.
  Measured here: Clementi Op. 36 No. 1 reads as annotated except measure 20 (the dominant of C minor
  reads as G major). Mozart K. 545 (the file holds all three movements) comes out as 42 regions, and 50
  measures inside the cores of the annotated first-movement regions read in a neighbouring key (the
  subdominant as F major, the dominant or supertonic as G major, A minor, E minor or D minor); the
  expositions' C-to-G modulation at measure 13 is found, the recapitulation's return to C at measures
  79 and 124 is not. Bach 114 is read as G major, then D major from measure 14 to the end (the earlier one-key estimate
  was G major). A region read in a key the music is not in also reads its chords in that key.
  The profile fits chromatic music less well. A key signature is trusted as a boundary: many exporters
  write a default C major at tick 0 whatever the key.
- Pivot chords are read in the old key's major collection (a minor old key reads in its relative
  major), and a chord that needs a minor key's raised sixth or seventh is not diatonic in the new key.
- Chord identification is Ursatz's weighted identification, read in each segment's key region. Its
  vocabulary is the diatonic triads and sevenths of a major key: in a minor region, or when no key is
  found, no chord is identified. Read in local keys, Field Nocturne No. 1 has chord coverage of 86.6%,
  below the 90% it reached when read in one key: its regions include an F minor one, which gets no
  chords, and a Bb major one (uncertainty 0.97) that the detection is unsure of. Chromatic, applied and fully diminished chords are not candidates,
  so a segment built on one is matched to the nearest diatonic chord or not at all.
- Non-chord tones are classified only where the library can: its passing, neighbor, suspension,
  anticipation, appoggiatura, escape-tone and pedal rules. Many tones come back unclassified.
- SMPTE-division MIDI files are rejected.
- Compressed `.mxl` MusicXML is not read: the Ursatz importer takes uncompressed documents only.
- MusicXML repeats and endings are not unfolded, and the pickup is found only when the first measure
  is marked `implicit="yes"`.
- Measure and beat follow the file's time signature events. A file that declares the wrong meter
  (for example, a minuet marked 4/4) is reported as declared.
