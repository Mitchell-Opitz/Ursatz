# Ursatz-Analyzer

A small C11 command-line tool that turns a MIDI file into a Markdown report: the piece's
global key, and the most likely chord for each group of notes that start together. It is a
thin consumer of the [Ursatz](https://github.com/Mitchell-Opitz/Ursatz) library and its
`common-practice-minimal` framework.

## Build

Requires CMake 3.20+, a C11 compiler, and SQLite3 (for Ursatz). Ursatz is a git submodule at
`external/ursatz`, pinned to Ursatz main at PR #97 (`090695c`).

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Four tests run: `test_analysis_pipeline`, `test_report_output`,
`test_corpus_key_estimation` (checks the reported key of six reference pieces), and
`test_long_labels` (labels past 31 characters render intact).

## Usage

```sh
ursatz-analyzer <midi-file-path>
```

Writes `<stem>.report.md` next to the input file. Exits 1 with a message on bad input,
allocation failure, or write failure.

## Library

The pipeline builds as the `ursatz_analyzer` static library. The CLI and the tests link it.
Its public include directory is `include/`, and consumers include headers as
`<ursatz_analyzer/xxx.h>`. Everything under `src/` is private.

| Header | Purpose |
| --- | --- |
| `midi_pipeline.h` | Imports a MIDI file into `ImportedNotes`; `imported_notes_destroy` frees it. |
| `analysis_pipeline.h` | `run_analysis_pipeline` turns `ImportedNotes` into an `AnalysisReport`. |
| `analysis_report.h` | `AnalysisReport` and its entries (onset, chord label, key label). |
| `markdown_report.h` | Renders an `AnalysisReport` as a Markdown string. |
| `report_writer.h` | Derives the `<stem>.report.md` path and writes the file. |

Ownership: the caller owns the `AnalysisReport` and frees it with `analysis_report_destroy`,
which also frees every entry's labels. Labels are heap strings with no length limit; "no match"
marks a group or key that was not identified. Strings returned by `markdown_report_render` and
`derive_report_path` are freed by the caller with `free`.

```cmake
add_subdirectory(external/ursatz-analyzer)
target_link_libraries(my_app PRIVATE ursatz_analyzer)
```

## Pipeline

`main.c` runs `midi_pipeline` (MIDI import through Ursatz), then `analysis_pipeline`:

1. `segmentation`: groups notes whose onsets are exactly equal.
2. `key_pipeline`: estimates one key for the whole piece.
3. `chord_pipeline`: per group, maps pitch classes to scale degrees and identifies a chord.

Then `markdown_report` renders and `report_writer` writes the file.

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
- Chord identification inherits Ursatz's matching. The pinned submodule (Ursatz PR #97) uses
  subset matching: one extra diatonic tone is tolerated and the first match wins
  (non-diatonic notes are dropped first). Two or more non-chord tones per beat still fail.
- SMPTE-division MIDI files are rejected.
