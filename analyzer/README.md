# Ursatz-Analyzer

A small C11 command-line tool that turns a MIDI file into a Markdown report and a JSON analysis
document: the piece's time signature, measure count and pickup, its global key, and the most
likely chord for each group of notes that start together, each located by measure and beat. It is a
thin consumer of the [Ursatz](https://github.com/Mitchell-Opitz/Ursatz) library and its
`common-practice-minimal` framework.

## Build

Requires CMake 3.20+, a C11 compiler, and SQLite3 (for Ursatz). Ursatz is a git submodule at
`external/ursatz`, pinned to Ursatz main at PR #106 (`d6610cd`).

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests:

- `test_analysis_pipeline`, `test_report_output`: end-to-end pipeline and report file.
- `test_corpus_key_estimation`: the reported key of six reference pieces.
- `test_long_labels`: labels past 31 characters render intact.
- `test_analysis_document`: the analysis document's sections, claims and values.
- `test_measure_locations`: time signature, measure count, pickup, meter changes, and the measure
  and beat of chosen chords, for the five reference pieces and two constructed files (a pickup and
  a file with no time signature).
- `test_cli_output_flags`: which files each output flag writes.
- `test_analysis_document_schema`: validates the documents for the corpus pieces against the
  schema. Optional: needs `python3` with the `jsonschema` module at configure time, and is
  reported as skipped otherwise.

## Usage

```sh
ursatz-analyzer [--json] [--markdown] <midi-file-path>
```

Writes two files next to the input file:

- `<stem>.report.md`: the Markdown report.
- `<stem>.analysis.json`: the analysis document. See
  [docs/analysis-document.md](docs/analysis-document.md) and the
  [schema](schema/analysis-document.v2.schema.json).

`--markdown` writes only the report, `--json` writes only the analysis document. No flag, or both,
writes both. Flags can come before or after the path. Exits 1 with a message on an unknown flag, a
missing path, bad input, allocation failure, or write failure.

## Library

The pipeline builds as the `ursatz_analyzer` static library. The CLI and the tests link it.
Its public include directory is `include/`, and consumers include headers as
`<ursatz_analyzer/xxx.h>`. Everything under `src/` is private.

| Header | Purpose |
| --- | --- |
| `midi_pipeline.h` | Imports a MIDI file into `ImportedNotes`; `imported_notes_destroy` frees it. |
| `analysis_pipeline.h` | `run_analysis_pipeline` turns `ImportedNotes` into an `AnalysisReport`. |
| `analysis_report.h` | `AnalysisReport` and its entries (onset, chord label, key label). |
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

`main.c` runs `midi_pipeline` (MIDI import through Ursatz), then `analysis_pipeline`:

1. `segmentation`: groups notes whose onsets are exactly equal.
2. `key_pipeline`: estimates one key for the whole piece.
3. `chord_pipeline`: per group, maps pitch classes to scale degrees and identifies a chord.

`analysis_document` then builds the analysis document once. It is the single source of truth:
the JSON file is its serialization, and `markdown_report` renders the report from it, never from
the pipeline directly. `report_writer` writes each file.

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
  mark it as inferred, an assumption and not a fact about the piece.
- The measure and beat are the file's: the analyzer does not check them against a printed score.

See [docs/analysis-document.md](docs/analysis-document.md) for the fields.

## Key estimation

`estimate_global_key` (`src/pipeline/key_pipeline.c`) stops at the first source that succeeds:

1. **MIDI key signature meta-event** (`0xFF 0x59`, read by Ursatz's `midi_key_signature_scan`).
   Its sharps/flats count and major/minor flag are decoded through a circle-of-fifths table.
   It wins silently, even if the music later modulates.
2. **Closing chord, then opening chord.** The bass pitch class of the last onset group is tried
   as the tonic first, then the first group's. The closing chord is the more reliable one,
   since a piece can open on a pickup or a dominant.
3. **Brute-force scan.** Every pitch class 0 to 11 is tried in order against `KeyEstimation`'s
   major and natural-minor collections; the first match wins.

Key labels use flat spelling (`Eb`, `Db`) when the key signature has flats, or, for a key found
by steps 2 or 3, when the tonic needs fewer flats than sharps. Chord and note spelling elsewhere
in the report always uses sharps.

## Known limits

- One key for the whole piece; no modulation tracking.
- Sonority grouping needs exactly equal onsets, so a held or staggered note lands in its own
  group.
- Chord identification inherits Ursatz's matching. The pinned submodule (Ursatz PR #106) uses
  subset matching: one extra diatonic tone is tolerated and the first match wins
  (non-diatonic notes are dropped first). Two or more non-chord tones per beat still fail.
- SMPTE-division MIDI files are rejected.
- Measure and beat follow the file's time signature events. A file that declares the wrong meter
  (for example, a minuet marked 4/4) is reported as declared.
