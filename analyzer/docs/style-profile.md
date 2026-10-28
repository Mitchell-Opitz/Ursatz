# Style profiles

```sh
ursatz-analyzer --style-profile <corpus output dir> --name <style name> [--out <file>]
ursatz-analyzer --compare-styles <a.json> <b.json> [--without <category>]... [--format text|json]
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
| rhythm | `tempo` | piece that declares a tempo | the opening tempo in quarter notes per minute as a band of 20: `<40`, `40-59`, ..., `200-219`, `220+`; a value on a boundary goes to the upper band, 140 is `140-159` | `metadata.input.tempo` (`declared`, `quarters_per_minute`) |
| rhythm | `note-density` | phrase | attacks per beat of the whole phrase to the nearest 1/2, halves up, as a reduced fraction: `0`, `1/2`, `1`, `3/2`, ..., `4+` for 4 or more | `rhythm[]` (density, line `all`) `attacks_per_beat.mean` |
| rhythm | `meter` | time signature (the opening one and each change) | `3/4` | `meter.time_signature`, `meter.changes[]` |
| rhythm | `syncopation-per-phrase` | phrase | soprano syncopations in the phrase, `4+` for four or more | `rhythm[]` (syncopation, soprano) and `phrases[].measures` |
| melody | `melodic-interval` | pair of soprano notes | directed semitones: `+2`, `-5`, `0`; `+13+` and `-13+` beyond an octave | `outer_voices.soprano[].pitch.midi` |
| melody | `contour` | the same pair | `up`, `down`, `same` | the same |
| melody | `phrase-range` | phrase | soprano range in semitones | `melody[].range.semitones` |
| melody | `motive-degrees` | motive family with features | the prototype's degrees against the tonic's major scale, `1 b6 5` | `motives[].features.degrees` |
| melody | `motive-intervals` | motive family with features | the semitones with signs, `+8 -1`, `0` unsigned | `motives[].features.intervals` |
| melody | `motive-rhythm` | motive family with features | the durations, `1/8 1/8 1/4` | `motives[].features.rhythm` |
| melody | `motive-nct-figure` | non-chord tone on a motive note (each note once per piece) | the previous note's degree, the note's degree and type joined by `:`, the next note's degree, `1 b6:appoggiatura 5`; `-` for a missing neighbour | `motives[].occurrences[].degrees`, `non_chord_tones[]` |
| form | `theme-form` | theme | `sentence`, `period`, `hybrid`... | `themes[].label` |
| form | `phrase-length` | phrase | length in measures | `phrases[].length` |
| form | `section-letters` | piece | the form plan's letters joined by spaces, `A B A` | `form.sections[].label` |
| form | `mode` | piece | `major` or `minor`, the home key's mode | `keys.home_key.label` |

Rules the counts follow:

- `tempo` is the opening tempo only: a MusicXML score's first metronome mark or a MIDI file's set-tempo event at tick 0
  (see [analysis-document.md](analysis-document.md)). A piece with no declared tempo adds nothing to it. Tempo changes
  within a piece are not counted.

- A note held across several chord segments is one soprano note. A rest bridged inside the line does not break it.
- A chord with no numeral, or a change of key region, ends a run of numerals. Within a run a numeral that repeats is
  counted as a chord but not as a progression step.
- The total of a category is the sum of its counts.
- Motive families are not a category by their ids, which belong to one piece. Their `features` make them comparable:
  a family is described by its degrees, intervals and rhythm, so two pieces that share a figure share a label. A
  family without `features` adds nothing to the four motive categories.
- `motive-nct-figure` reads every occurrence of every family, so it counts non-chord tones on motive notes of any
  occurrence. A note that several occurrences share counts once, in the first occurrence that names it. A tone the
  library left unclassified has no figure. The degrees of an occurrence's notes (`occurrences[].degrees`) use the key
  region each note starts in.

## Label builders

The Composer labels its own material with the same functions the profile uses, all in `style_labels.h`:
`style_label_tempo` and `style_label_density` (the bands above), `style_label_degree` (`1 b2 2 b3 3 4 #4 5 b6 6 b7 7`
by the semitones above the tonic), `style_label_join` (the `motive-degrees` and `motive-rhythm` labels),
`style_label_motive_intervals` (the `motive-intervals` label; no octave cap) and `style_label_nct_figure` (the
`motive-nct-figure` label).

## The style model file

`schema/style-model.v1.schema.json`:

```json
{
  "schema": "ursatz-style-model",
  "schema_version": 1,
  "style_name": "sonatinas",
  "training_corpus": "<the corpus run's input folder>",
  "model_version": "0.3.0",
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

## Leaving categories out

`style_model_file_read_without(path, categories, count, &model, &pieces)` reads a model file and drops the named
categories before the model is built, so the model holds no statistics of them (`style_model_file_read` is this with
none). An unknown category name is `STYLE_MODEL_FILE_ERR_ARGUMENT`. `--compare-styles` takes `--without <category>`,
repeatable, and drops the categories from both models; an unknown name is exit code 1. The Composer reads styles
without the four motive categories until it uses them as preferences. To measure a fragment against such a model, use
`analyzer_fragment_style_distance_without` with the same list ([composer-api.md](composer-api.md)); the plain call
counts each category the model lacks as distance 1.

## Distance

`--compare-styles` (and `style_profile_compare`) gives, per category, the total variation distance between the two
models' label frequencies: half the sum over labels of |count_a/total_a - count_b/total_b|. It is an exact fraction in
lowest terms, held in 128 bits. A category only one model has is distance 1. A dimension's distance is the mean of its
categories' distances, and the overall distance is the mean of the dimensions that have a category in either model. A
category distance that does not fit in 128 bits is `STYLE_COMPARE_ERR_OVERFLOW`, never a wrapped value. A mean over
categories (a dimension's or the overall distance) is exact while it fits in 128 bits; with the motive categories, whose
labels are many, the exact mean of a real corpus needs more, and it is then the nearest fraction over 2^64
(`dimension_exact` and `overall_exact` in `StyleComparison` say which). Each category also lists each model's three most
frequent labels.

The text output is `overall`, then each dimension with its categories indented, each as `0.4703 (n/d)`. The JSON output
has `overall`, `dimensions` and `categories`, each distance as `{"fraction": "n/d", "value": 0.470300}`. Exit codes: 0,
1 usage, 2 an unreadable model or an overflow.

## Measured distances

The ten Bach chorales of the accuracy corpus (`chorales`) against Clementi Op. 36 No. 1 and Mozart K. 545
(`sonatinas`), printed by `test_style_profile`:

| Dimension | Distance |
| --- | --- |
| harmony | 0.4657 |
| rhythm | 0.6604 |
| melody | 0.7375 |
| form | 0.4450 |
| overall | 0.5771 |

| Category | Distance |
| --- | --- |
| `cadence-type` | 0.4324 |
| `chord-numeral` | 0.2407 |
| `chord-progression-2` | 0.5678 |
| `chord-progression-3` | 0.8472 |
| `harmonic-rhythm` | 0.2403 |
| `duration` | 0.7998 |
| `tempo` | 1.0000 |
| `note-density` | 0.6935 |
| `meter` | 0.4000 |
| `syncopation-per-phrase` | 0.4088 |
| `melodic-interval` | 0.4108 |
| `contour` | 0.0353 |
| `phrase-range` | 0.7816 |
| `motive-degrees` | 0.9953 |
| `motive-intervals` | 0.9860 |
| `motive-rhythm` | 1.0000 |
| `motive-nct-figure` | 0.9533 |
| `theme-form` | 0.0524 |
| `phrase-length` | 0.4276 |
| `section-letters` | 0.9000 |
| `mode` | 0.4000 |

The motive categories are far from each other because a figure of 3 to 8 notes rarely repeats between two
composers: nearly every label is seen in one corpus only. They are best read against pieces that share material.

The distance is symmetric. A model is at distance 0 from itself in every category.
