# Building and checking the Analyzer

Build requirements, the test list and the checks CI runs beyond the configure, build and test commands. Back to the [docs index](README.md).


Library headers are included only by their staged public paths (`time/meter_map.h`, `standard-harmony/analysis/analysis_cadence.h`,
`common-practice-minimal/key_regions.h`), never by a framework's source directory. `URSATZ_LEGACY_INCLUDE_DIRS` is off
(Ursatz's default), so the build sees only the staged public headers. `scripts/check-library-includes.sh` checks this
(see [Library include check](#library-include-check)). Tests read their input from `tests/fixtures/`, not from the submodule (see `tests/fixtures/README.md`).

The Analyzer holds no arithmetic of its own on times, pitches, key signatures or numerals. Exact times are `Rational`
(`core/rational.h`, with the `time/time_rational.h` bridges); an overflow fails the stage that met it, and a document
build that meets one returns no document. Pitch names, letters, pitch classes and the line of fifths come from
`pitch/pitch_arithmetic.h`; key signature counts and key tonics from `standard-tonality/tonal_key.h`; Roman numerals are
read, formatted and resolved to a root by `standard-tonality/roman_numeral_grammar.h`. Degree semitones come from `standard-tonality/tonal_key.h`
(`standard_tonality_degree_semitone`), a root name's pitch class from `pitch_spelling_parse_name` and `pitch_class_of_letter`, a
MIDI pitch class from `pitch_class_of_midi`, a doubling read from `standard-voicing/doubling_reading.h`, and rhythm density's
phrase span and onset snap from `time/score_time.h` (`score_time_span`, `score_time_snap`). Time comparisons use `rational_cmp`. `StyleRational` and the meter
check's own fraction stay until the library owns them.

## Tests

Tests:

- `test_analysis_pipeline`, `test_report_output`: end-to-end pipeline and report file.
- `test_analysis_result`: the claims of each area on the five reference pieces, targets and confidences, cancel and
  partial stages, and the document and report of the result against the snapshots (`smoke` runs Bach 114, also under
  valgrind).
- `test_corpus_key_estimation`: the home key of six reference pieces.
- `test_key_regions_clementi_36`, `test_key_regions_mozart_545`: the key regions and principal
  modulations of Clementi Sonatina Op. 36 No. 1 and Mozart K. 545 against hand-read regions. Both are
  marked `WILL_FAIL`: Ursatz's detection does not reach the target yet (see [known limits](known-limits.md)).
- `test_key_regions_constructed`: a piece with no modulation, one with an excursion whose cadence makes it a
  weak modulation, one with a modulation, and a notated key signature the music does not fit.
- `test_cadences`: the cadence claims of the five reference pieces (locations, note ids, types, regions), the
  cadence-confirmed key changes, Bach 114's two perfect authentic cadences and Clementi 36's counts.
- `test_harmonic_function`: the function of every labelled chord of Bach 114 and Mozart 545 (V and vii°
  dominant, ii and IV strong and iii and vi weak predominants), the six-four types, the prolongations, and a
  cadential six-four in Mozart 545.
- `test_progressions`: the progression finder on hand-built chord lists (held chords, key and unlabelled breaks,
  containment, the six-chord limit) and the progressions of Bach 114 and Mozart 545. `test_progressions_bach_114_top`
  asserts that Bach 114's most common progression ends on I; it is marked `WILL_FAIL` (see [known limits](known-limits.md)).
- `test_themes`: a constructed sentence, period and compound period (`tests/musicxml`), field by field (form, part
  names, measures, cadences, halves), and on the five reference pieces the structure of the themes and expansions
  (order, parts inside the theme, cadences in their part, expansions inside their phrase).
- `test_form_plan`: the form tree of the five reference pieces and the three constructed ones: every phrase in
  exactly one section, the sections following each other and closed only by an authentic cadence, the tree's levels
  named `form`, `section`, `phrase`, `subphrase` and `idea`, the letters following from the returns, and the
  section and return counts and letters as the library gives them (see "Form plan").
- `test_long_labels`: labels past 31 characters render intact.
- `test_report_snapshots` and `test_document_snapshots`: the Markdown report (MIDI and MusicXML) and the analysis
  document (MIDI, without `analyzer_version`, `ursatz_commit` and `input`) of the five reference pieces, byte for byte
  against `tests/snapshots/<piece>.report.md` and `<piece>.analysis.json`. Run a test with `UPDATE_SNAPSHOTS=1` (or
  `URSATZ_UPDATE_SNAPSHOTS=1`) to rewrite its files; a claim that moves is recaptured in its own commit.
- `test_analysis_document`: the analysis document's sections, claims and values.
- `test_measure_locations`: time signature, measure count, pickup, meter changes, and the measure
  and beat of the chords covering chosen positions, for the five reference pieces and two
  constructed files (a pickup and a file with no time signature).
- `test_chord_coverage`: chord coverage of the five reference pieces. Field Nocturne No. 1 must be
  above 90% (it reads 92.7%); the others must still produce chords. See "Chords and non-chord tones".
- `test_chord_details`: inversion, runner-up present and absent, and four non-chord-tone types and
  an unclassified one, on small constructed files.
- `test_roman_numerals`: hand-read numerals on constructed pieces (a major and a minor progression,
  inversions and seventh-chord figures, an applied chord, a borrowed chord, a modulation) and the
  chord vocabulary's invariants on those and the five reference pieces: counts add up to the
  chords with a numeral, shares to one, entries sorted. See "Roman numerals and the chord
  vocabulary".
- `test_cli_output_flags`: which files each output flag writes.
- `test_musicxml_midi_agreement`: the five reference pieces from their MusicXML versions
  (`tests/musicxml`) and from their MIDI files: key regions, measure count and meter must agree
  exactly, chords and outer-voice scale degrees to the tolerance stated in the test, and the
  measured agreement is printed.
- `test_musicxml_input`: format choice, notated key and meter, a pickup, a multi-part score, notated
  spelling (Db vs C#) and every error case.
- `test_cli_input_formats`: `--input-format`, format detection and the error exits of the CLI.
- `test_notation_reading`: the reading parsers, scaling that keeps note ids, the meter map a reading sets, a failed
  reading leaving the notes unchanged, and Bach 114 read in 3/4 at scale 2 (64 measures, its cadence arrivals on beat 1).
  `test_cli_notation_reading` checks the flags, the document and its schema.
- `test_counterpoint_check`: the first and second species Fux exercises (`tests/counterpoint`, one valid and one
  with a parallel fifth each), the document and the Markdown of a check, a shape error, and input that is not two
  voices. `test_cli_counterpoint_check` checks the flags and the exit codes.
- `test_corpus`: corpus mode on the `reference` and `mixed` corpora CMake builds in the build tree: the index
  against `tests/snapshots/corpus-index.reference.json`, each document byte for byte equal to a single-file run, and
  the resume rules. `test_cli_corpus` checks the flags and exit codes. `test_corpus_index_schema` validates the index
  against the schema (see below).
- `test_style_profile`: the style labels on hand cases, the profile of the `reference` corpus against
  `tests/snapshots/style-model.reference.json`, the model file round trip, the distances between the `chorales` and
  `sonatinas` corpora CMake builds in the build tree, and the refusals. `test_cli_style_profile` checks the flags and exit
  codes. `test_style_model_schema` validates the model file against the schema (see below).
- `test_chord_sequence`: the joined chord sequence of a result and of its document on the five reference pieces (equal, and each chord a run of document entries that no neighbour continues), a held chord, a key region change, an unlabelled entry and the refusals. `test_chord_sequence_smoke` (Bach 114) runs in CI.
- `test_playback`, `test_score_export`: the playback JSON and the MusicXML score of the five reference pieces and `pickup.musicxml`, `field-5.musicxml` against `tests/snapshots/gui/`, a value scale of 2, the numbers behind the JSON and the refusals. The `_smoke` runs (Bach 114, pickup) and valgrind runs are quick.
- `test_similar_material`: the similar material search over the `reference` corpus: the opening of Mozart 545 as a
  motive, the same motive moved up, `I IV V I`, the results against `tests/snapshots/similar.reference.txt`, and the
  refusals. `test_cli_similar` checks the flags, the output and the exit codes, and validates the JSON against the
  schema (see below).
- `test_motive_links`: the library over the four pieces of `tests/links`: a link across keys that replays from one
  prototype sketch to the other, a family in no link, the groups, the cache (a second run compares nothing, adding a piece
  compares only its pairs, another analyzer version or other options empty it) and the scope's refusals, also under
  valgrind. `test_cli_link_motives` checks the flags, the text and JSON output, the cache and the exit codes, and
  validates both JSON files against their schemas (see below).
- `test_staleness`: each `AnalyzerDocumentStatus`, from keys and from snapshot documents with one key changed, also under
  valgrind. `test_corpus` also resumes across a changed Ursatz commit.
- `test_version`: the public version header returns the macros, the version of `project()` and a non-empty Ursatz
  commit.
- `test_analysis_document_schema`: validates the documents for the corpus pieces against the
  schema. Optional: needs `python3` with the `jsonschema` module at configure time, and is
  reported as skipped otherwise.

## Library include check

`scripts/check-library-includes.sh <build dir>` runs after the build and fails when `src/` or `include/` includes an
Ursatz header that is not under a staged public root of the build, names a `*_internal.h` of Ursatz, when a CMake
file reads `SOURCE_DIR` of an Ursatz target or a path under `external/ursatz/tests`, or when a public header other than
`result_chain.h` includes a chain or framework header. It prints each offending line. CI runs it after the build.
`test_consumer_isolation` builds the targets of `tests/consumer`: one linking only `ursatz_analyzer` compiles every public
header but `result_chain.h`, and cannot include a chain or framework header.

## File scope state check

`scripts/check-statics.sh` fails when a file in `src/` declares a file-scope `static` variable that is not `const`
(a pointer to const data counts as mutable; write `static const T *const name`). It prints each file and line. State a
stage needs is passed through its caller's context. CI runs it after the library include check.

## Theory the analyzer takes from the library

The analyzer holds no theory of its own (R-217). The rules below are Ursatz's, and the stages only collect evidence,
call them and render the result:

- Meter: `standard_meter_check_changes` and `standard_meter_check_meter` (the meter check and the harmonic rhythm's
  changes), `standard_meter_find_tuplets` (tuplets and duplets, with their meter type and level).
- Tonality: `common_practice_home_key_region`, `common_practice_ranked_chord_quality_name` and
  `common_practice_ranked_chord_inversion` (an inversion is -1 for a bass that is no chord tone),
  `standard_tonality_triad_quality_semitones` and `standard_tonality_seventh_quality_semitones`,
  `standard_tonality_chromatic_degree_label` (`style_label_degree` wraps it), `common_practice_scale_degree_of`, and
  `standard_tonality_key_tonic_of_pitch_class` (a minor key's tonic is spelled through its relative major, so C minor
  reads in flats and D# minor in sharps).
- Kernel: `outer_voice_attack_soprano` (the soprano the motives are read over; a note of unknown pitch is skipped),
  `progression_patterns_find` and `progression_pattern_label` (repeated progressions), `motive_link_candidates` and
  `motive_link_groups` (the motive links; the analyzer orders the groups, largest first).

## Version header and pin check

`<ursatz_analyzer/version.h>` is generated at configure time. It defines `URSATZ_ANALYZER_VERSION` and
`URSATZ_ANALYZER_URSATZ_COMMIT` and declares `ursatz_analyzer_version()` and `ursatz_analyzer_ursatz_commit()`, so a
consumer reads the version from the library instead of parsing `CMakeLists.txt`.

The version is 0.6.0. Every analysis stored by an earlier version is stale by version.

`scripts/check-pins.sh` fails when a submodule pin is not on its remote's `main`, or when one repository is pinned at
two different commits anywhere in the tree. CI runs it right after the submodule checkout.
