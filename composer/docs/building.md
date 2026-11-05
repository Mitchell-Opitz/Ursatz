# Building and library layout

Build and test beyond the basic commands: the slow tests, the public header rule, the submodule pins and what the Composer takes from the library. Back to the [docs index](README.md).

cd Ursatz-Composer
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -LE slow

parallel) are left out of that command and of CI. Run them all, with plain `ctest --test-dir build -j8
--output-on-failure`, before pushing a branch.

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive`.


## Submodules

| Path | Pinned at |
| --- | --- |
| `external/ursatz` | Ursatz `main` at PR #195 (`d2b85ce`). |
| `external/ursatz-analyzer` | Ursatz-Analyzer `main` at PR #67 (`ee8a901`), version 0.6.0. |

cJSON 1.7.18 is vendored at `external/cjson` and added before the submodules, so the Analyzer reuses it. `scripts/check-pins.sh`
fails when a pin is not on its remote's `main` or when one repository is pinned at two commits; CI runs it after the
submodule checkout.

Theory comes from the frameworks (R-217): the Composer holds plan structure and composing decisions and asks
standard-meter for the accented beats, standard-counterpoint for the melodic moves, standard-tonality for the scales and
degrees, and standard-harmony for the phrase model and the resolutions. What has no framework owner (the harmonic rhythm
cells per beat, the answer to a leap, a chord's member degree) stays in the Composer and is listed in the phase notes.

Library headers are included by their staged public paths only: kernel headers as `<time/meter_map.h>`, framework headers
with their folder (`<standard-harmony/harmony_cadence_rules.h>`), Analyzer headers as `<ursatz_analyzer/...>`; Composer
headers keep their own paths. `scripts/check-library-includes.sh` (run after the build, CI runs it) fails on an Ursatz
header outside the staged public roots in `build/external/ursatz/public/manifest.txt`, an Analyzer header outside
`external/ursatz-analyzer/include/ursatz_analyzer/`, any `*_internal.h` of either, or a CMake file that reads the
`SOURCE_DIR` of an Ursatz or Analyzer target or a path under `external/ursatz/tests` or `external/ursatz-analyzer/src`.

## Library primitives

The Composer keeps no copy of what the library owns outside the style share ratios (`StyleRational`, one `wide_gcd`
in `src/style/style_internal.h`). There is no tick grid: every time is a library `Rational` in whole notes, and the
lanes of the plan (R-019) hold library values:

| Need | Library owner |
| --- | --- |
| Exact fractions and their gcd (motive scores, MIDI divisions, hashing, text) | `<core/rational.h>`: `Rational`, checked, an overflow fails the caller. |
| Pitch class names, spelling, octaves | `<pitch/pitch_arithmetic.h>`: `pitch_class_name`, `pitch_spelling_from_fifths`, `pitch_spelling_midi_number`. |
| Key names and signatures | `<standard-tonality/tonal_key.h>`: `standard_tonality_key_tonic_of_pitch_class`, `standard_tonality_key_fifths`, `standard_tonality_key_signature_of`. |
| Roman numerals of plain chords | `<standard-tonality/roman_numeral_grammar.h>`: parse reads every form, format writes the canonical one (`viio`, `viiø7`, `65`, `43`, `42`). Applied chords keep `harmony_applied_label`. |
| Plan time, spans and the positions inside an operator | `<core/rational.h>`: a `PlanSpan` is a `Rational` start and length; the operators work on `Rational` positions from the start of their target (`src/util/position.h` wraps the checked arithmetic), and `<time/time_rational.h>` bridges to `MusicalTime`. No meter is refused for a grid. |
| A motive | `<analysis/motive_sketch.h>`: `MotiveSketch`, which the Composer names `Motive` (`typedef MotiveSketch Motive`, `src/include/ursatz_composer/motive.h`). Its notes are `pitch` (`MOTIVE_SKETCH_PITCH_UNKNOWN` in a rhythm motive), `onset` and `duration`; its meter is the sketch's `bar` and `beat` and its key the sketch's scale and tonic spelling, read and set only through `motive_meter_of`, `motive_set_meter`, `motive_key_of` and `motive_set_key`. A motive keeps no per-note spelling and no hyperbar. |
| How a motive is made from another | `<analysis/motive_relation.h>`: `MotiveRelationStep`, a relation kind and up to 4 parameter values. It is the only transform spec: `motive_develop` takes it, a `PlanMotiveRef` holds it (`step`, or none for a plain reference to the source) and the plan file writes its text (`"step": "transpose tonal=1 by=2"`). The variant registry rows name their relation and describe their parameters through `motive_relation_param_info`; `docs/plan-constraints.md#motive-relation-steps` has the table. |
| Cadence type | `<semantics/cadence_type.h>`: `CadenceType`. The plan file and the command line name the five a plan holds PAC, IAC, HC, DC and PC (`plan_cadence_names` in `src/util/plan_util.c`). |
| Harmonic function | `<standard-harmony/harmonic_function.h>`: `HarmonicFunction`, with `PlanHarmony.has_function` false for chords loaded from an Analyzer result and for a plan file chord with no `function`. |
| Key | `<semantics/analysis_key.h>`: `AnalysisKey`, a spelled tonic and a `KeyStructureMode`. `plan_key_of_pitch_class` spells a pitch class with the library; `analysis_key_tonic_pitch_class` reads it back. |
| Pitch | A `PlanNote` is a MIDI number and an optional spelling (`spelled`, `letter`, `accidental`). MusicXML notes keep their notated spelling; the operators write unspelled notes and the renderer spells them in the key under them. |
| MIDI import | `<io/midi_importer.h>`: `midi_importer_import_complete` (notes, meter and key signatures in one parse). |
