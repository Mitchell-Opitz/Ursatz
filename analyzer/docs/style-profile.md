# Style profiles

```sh
ursatz-analyzer --style-profile <corpus output dir> --name <style name> [--out <file>]
ursatz-analyzer --compare-styles <a.json> <b.json> [--format text|json]
```

Library: `include/ursatz_analyzer/style_profile.h` (profile, model file, comparison) and `style_labels.h` (the labels).

A style profile counts harmony, rhythm, melody and form labels over the analysis documents of a corpus run (see
[corpus.md](corpus.md)). It reads only the documents, never the scores. A profile says how often something was seen;
it is not a rule.

## Categories

Every category is a `StyleStatistic` category string. Labels come from `style_labels.c`, so the Composer labels its own
material the same way. Only analysis documents of schema version 9 are read.

| Dimension | Category | One label per | Label | Source in the document |
| --- | --- | --- | --- | --- |
| harmony | `chord-numeral` | labelled chord | root-position numeral (`V7` for `V6/5`, `I` for `I6/4`) | `chords[].roman_numeral.base_numeral` |
| harmony | `chord-progression-2` | pair of numerals | `ii-V7` | consecutive numerals in one key region |
| harmony | `chord-progression-3` | triple of numerals | `ii-V7-I` | the same |
| harmony | `cadence-type` | cadence | `perfect-authentic` | `cadences[].type` |
| harmony | `harmonic-rhythm` | measure | chord changes in the measure, `3` | `harmonic_rhythm.per_measure[].changes` |
| rhythm | `duration` | soprano note | reduced fraction of a whole note, `1/8` | `outer_voices.soprano[].duration` |
| rhythm | `meter` | time signature (the opening one and each change) | `3/4` | `meter.time_signature`, `meter.changes[]` |
| rhythm | `syncopation-per-phrase` | phrase | soprano syncopations in the phrase, `4+` for four or more | `rhythm[]` (syncopation, soprano) and `phrases[].measures` |
| melody | `melodic-interval` | pair of soprano notes | directed semitones: `+2`, `-5`, `0`; `+13+` and `-13+` beyond an octave | `outer_voices.soprano[].pitch.midi` |
| melody | `contour` | the same pair | `up`, `down`, `same` | the same |
| melody | `phrase-range` | phrase | soprano range in semitones | `melody[].range.semitones` |
| form | `theme-form` | theme | `sentence`, `period`, `hybrid`... | `themes[].label` |
| form | `phrase-length` | phrase | length in measures | `phrases[].length` |
| form | `section-letters` | piece | the form plan's letters joined by spaces, `A B A` | `form.sections[].label` |
| form | `mode` | piece | `major` or `minor`, the home key's mode | `keys.home_key.label` |

Rules the counts follow:

- A note held across several chord segments is one soprano note. A rest bridged inside the line does not break it.
- A chord with no numeral, or a change of key region, ends a run of numerals. Within a run a numeral that repeats is
  counted as a chord but not as a progression step.
- The total of a category is the sum of its counts.
- Motive families are not a category: their ids belong to one piece and cannot be compared across pieces.

## The style model file

`schema/style-model.v1.schema.json`:

```json
{
  "schema": "ursatz-style-model",
  "schema_version": 1,
  "style_name": "sonatinas",
  "training_corpus": "<the corpus run's input folder>",
  "model_version": "0.1.0",
  "pieces": 2,
  "statistics": [
    { "dimension": "harmony", "category": "cadence-type", "label": "half", "count": 34, "total": 92 }
  ]
}
```

Statistics are sorted by category, then label, and the same profile always writes the same bytes. `--out` defaults to
`<corpus output dir>/style-model.json`. The reader (`style_model_file_read`) rejects another schema or version, an
unknown dimension or category, a category under the wrong dimension, and a total that is not the sum of the counts.
It builds a Ursatz `StyleModel` through `style_model_from_statistics`. The library has no id generator, so the id is
`style-model-` and the 16 hex digits of the FNV-1a 64 hash of the file's bytes: the same file always gives the same id.

## Distance

`--compare-styles` (and `style_profile_compare`) gives, per category, the total variation distance between the two
models' label frequencies: half the sum over labels of |count_a/total_a - count_b/total_b|. It is an exact fraction in
lowest terms, held in 128 bits. A category only one model has is distance 1. A dimension's distance is the mean of its
categories' distances, and the overall distance is the mean of the dimensions that have a category in either model. A
result that does not fit in 128 bits is `STYLE_COMPARE_ERR_OVERFLOW`, never a wrapped value. Each category also lists
each model's three most frequent labels.

The text output is `overall`, then each dimension with its categories indented, each as `0.4703 (n/d)`. The JSON output
has `overall`, `dimensions` and `categories`, each distance as `{"fraction": "n/d", "value": 0.470300}`. Exit codes: 0,
1 usage, 2 an unreadable model or an overflow.

## Measured distances

The ten Bach chorales of the accuracy corpus (`chorales`) against Clementi Op. 36 No. 1 and Mozart K. 545
(`sonatinas`), printed by `test_style_profile`:

| Dimension | Distance |
| --- | --- |
| harmony | 0.4661 |
| rhythm | 0.5362 |
| melody | 0.4039 |
| form | 0.4751 |
| overall | 0.4703 |

| Category | Distance |
| --- | --- |
| `cadence-type` | 0.4343 |
| `chord-numeral` | 0.2407 |
| `chord-progression-2` | 0.5678 |
| `chord-progression-3` | 0.8472 |
| `harmonic-rhythm` | 0.2403 |
| `duration` | 0.7998 |
| `meter` | 0.4000 |
| `syncopation-per-phrase` | 0.4088 |
| `melodic-interval` | 0.4108 |
| `contour` | 0.0353 |
| `phrase-range` | 0.7654 |
| `theme-form` | 0.1000 |
| `phrase-length` | 0.4005 |
| `section-letters` | 1.0000 |
| `mode` | 0.4000 |

The distance is symmetric. A model is at distance 0 from itself in every category.
