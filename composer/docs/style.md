# Styles

```sh
ursatz-composer plan --form binary --key C --meter 4/4 --style chorales.style.json --out piece.json
ursatz-composer fill piece.json --style-weight 2/1
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --style sonatinas.style.json
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --style chorales.style.json \
    --blend sonatinas.style.json --blend-weights harmony=1/4,form=1
```

Library: `<ursatz_composer/style.h>`. `ComposeConfig.style` and `.style_weight` (`compose.h`), `GrowConfig` the same.

A style is a StyleModel file the Ursatz Analyzer wrote from a corpus (`ursatz-analyzer --style-profile`, see the Analyzer's
`docs/style-profile.md`). The Composer reads it with the Analyzer's own reader (`composer_style_load`) and keeps, for each
category, how often each label was seen. A style is a count, not a rule (Laws 12, 13): it prefers one candidate over another
and never rejects one.

## Labels

A candidate is counted the way the Analyzer counts a corpus, with the labels of `style_labels.h`:

| Stage | Categories | From |
| --- | --- | --- |
| harmony | `chord-numeral`, `chord-progression-2`, `chord-progression-3` | the candidate's Roman numerals, root position (`V6/5` is `V7`); the chord written right before the target is the first element of the first pair |
| melody | `melodic-interval`, `contour`, `duration` | the candidate's notes; the note written before the target gives the first step |
| bass, texture | none | the candidate is neutral |

The Analyzer counts the soprano note at the start of each chord, the Composer counts every note of a candidate, so the two
agree exactly where each note starts a chord (the test's hand-written progression) and only roughly in a running melody.

## Closeness

The closeness of a candidate is in [0, 1], exact: the mean over its labels of the label's share in its category divided by
the category's largest share. That is the label's count over the largest count of its category, so the style's most common
label scores 1 and a label it never saw 0. A label whose category the style does not have is left out; a candidate with no
labels is neutral, 1/1. A fraction that does not fit in 64 bits is rounded to 30 bits.

## Weight

The harmony and melody stage checkers add `style_weight * closeness` to the score of every candidate they accept
(`style_weight` is a fraction, 1/1 when not set). Rejection is unchanged, and with no style the checkers are the ones there were
before: the sentence of the tests is byte for byte what `main` composed.

The scores the fill loop compares have to stay below 2^32, so a sum that would not is scaled down, keeping its order.

## Distance

After a complete run with a style the whole plan is rendered, analyzed in memory and compared with the style by the Analyzer
(`analyzer_fragment_style_distance`, `composer_style_measure`). That measure is independent of the closeness the Composer
scores with. It is `ComposeOutcome.style_distance` (0 is the style, 1 is nothing alike), per dimension and overall, and is
printed by `fill`, `grow` (text and JSON). A category the plan has no data for is left out of the means.

## Measured

Sentences in C major 4/4 grown from `E4:q G4:e F4:e D4:q C4:q`, seeds 1 to 20, weight 1/1, mean distance (`tests/test_style.c`
`distance`, label `slow`, with `smoke` on seeds 1 and 2):

| Style | Harmony styled | Harmony unstyled | Melody styled | Melody unstyled |
| --- | --- | --- | --- | --- |
| chorales | 0.6875 | 0.7245 | 0.4756 | 0.4944 |
| sonatinas | 0.6733 | 0.6609 | 0.4519 | 0.4550 |

A style is a preference among the one to ten candidates an operator proposes for a target, so it moves the result a little.
Under `chorales` both dimensions are closer. Under `sonatinas` (two pieces) melody is a little closer and harmony is not: a
known limit, the styled mean is held to its baseline (R-075). A larger weight did not help (1 to 64 measured). The Analyzer
also hears the Composer's chords through the notes only (`ii7` where `ii` was written), which blurs the harmony labels.

## Blending

```sh
ursatz-composer plan --form binary --key C --meter 4/4 --style chorales.style.json --blend sonatinas.style.json \
    --blend-weight 1/4 --blend-weights harmony=3/4,form=1 --out piece.json
ursatz-composer fill piece.json
```

`composer_style_blend(a, b, weights, &out)` mixes two styles with a weight for each dimension (`ComposerStyleWeights`, in the
Analyzer's `StyleDimension` order: harmony, rhythm, melody, form). A weight is the second style's share, a fraction from 0 (only
the first) to 1 (only the second). For each dimension the statistics of that dimension's categories (`style_category_dimension`)
of each style become a model, the two are blended with Ursatz `style_model_blend`, frequency = (1 - w) * a + w * b, exact,
and the four results are merged. The blended frequencies come as reduced fractions with their own denominators, so each
category is then brought to one total, as in a file the Analyzer wrote: its labels' frequencies over their common denominator,
or over 2^30 when that is too large. A weight of 0 gives the first style's frequencies in a dimension and 1 the second's; a
label only the other style has is left out at weight 0 and kept at weight 1. The result is a style like any other: named
`<a> + <b>`, its training corpus both joined with `;`, its pieces the sum. A blend can be blended again.

On the command line `--blend <file>` goes with `--style` (a usage error without it), `--blend-weight <n/d>` sets every
dimension (default 1/2) and `--blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>` sets some; the others take
`--blend-weight`. A weight above 1, a dimension that is not one or is named twice, and a weight without `--blend` are usage
errors. `plan` keeps the blend in the plan's `style` constraint (the second file and the four weights), so `fill` blends the
same two files from where it is run. On `fill`, `--blend` replaces the plan's blend, `--blend-weight` and `--blend-weights`
replace its weights, and `--style` alone drops it. The weight of the preference itself, `--style-weight`, is the same for a
blend as for one style.

### Form

The form dimension is used by the structure stage: when a theme section does not fix its form, the stage's checker adds
`style_weight * closeness` over each candidate's `theme-form` and `phrase-length` labels. A section made from a template
fixes it (every candidate has the section's theme pairing), so the form weight has no effect there: the same labels would
be added to every candidate. Today every theme the Composer grows is of that kind, and the form weight shapes the blend and
the distance that is measured, not the notes.

### Measured

Sentences in C major 4/4 as above, seeds 1 to 20, `chorales` blended with `sonatinas`, weight 1/1, the weight on harmony
alone (`tests/test_style_blend.c` `harmony`, label `slow`, with `smoke` on seeds 1 and 2). Mean harmony distance (the
Analyzer's):

| Harmony weight | From chorales | From sonatinas |
| --- | --- | --- |
| 0 | 0.6875 | 0.6580 |
| 1/2 | 0.7027 | 0.6820 |
| 1 | 0.7088 | 0.6687 |

Weight 0 is the `chorales` run above. The distance from `chorales` rises with the weight, as the weight moves the preference
away from it. The distance from `sonatinas` does not fall steadily: the same known limit as above, held to its baseline
(R-075). A weight on melody alone (1, harmony 0) moved the harmony distances by 0.008 and 0.010: the Analyzer hears the chords
through the melody, so a melody weight reaches the harmony it measures a little. The test holds this within 0.02.

## Seeding from a corpus

A style says what a corpus is like; seeding starts a piece from a passage of it. The corpus is the output directory of
`ursatz-analyzer --corpus <folder> --out <dir>`, the same one the Analyzer's `--similar` searches. The search is the
Analyzer's (`similar_query_from_file`, `similar_material_find`, see its `docs/similar-material.md`) and reads only the
analysis documents (R-079).

```sh
ursatz-composer grow --from-corpus corpus-out --like passage.mid --into sentence --key "D major" --meter 4/4 --seed 3 --out theme.json
ursatz-composer plan --form rounded-binary --key "D major" --meter 4/4 --from-corpus corpus-out --like passage.mid \
    --like-progression "I IV V I" --at B --out piece.json
```

**A motive.** `--like <file>` (a MIDI or MusicXML passage, with `--from-corpus <dir>`) replaces the motive input of `grow`
and `--motive` of `plan`. `corpus_seed_motive(dir, passage, meter, key, seed, &out)` (`<ursatz_composer/corpus_seed.h>`)
takes the best ten matches scoring 1/2 or more, keeps those within 1/10 of the best and picks one with the seeded generator
(`fill_rng`), so the same seed gives the same choice. From the start of the matched passage it takes the notes that start in
one bar of the meter, the last cut at the barline, and moves them by one interval so the first note keeps its scale degree in
the requested key, the source key being the key its analysis heard there (the key of the chord at the passage). In the same
mode that is a plain transposition (Mozart 545's opening bar, C major, seeds D5 half, F#5 quarter, A5 quarter in D major);
from major to minor the first note takes the same degree of the other scale and the others keep their intervals. A match with
fewer than two notes in the bar is passed over for the next. The result is a `Motive` in the requested key (`MOTIVE_KEY_GIVEN`).

**A progression.** `--like-progression "<numerals>"` goes after `--from-corpus` like a `--given-progression` and takes the
same `--at` and `--cells`: the chords placed are those of the passage of the corpus that best matches the numerals
(`corpus_seed_progression`). The Analyzer writes a passage in the notation of its own key (`i iv V i` in a minor key) and the
plan reads numerals in the mode of the key they are placed in, so the passage is the best of the best hundred whose every
chord the plan can place under the requested key's mode. There is no seed: the best match is the first the Analyzer ranks.

**What is reported.** The text output names the source piece, its measures and the score (`motive from Mozart 545.mid m.1
b.1 - m.2 b.1, score 1/1`; `progression from ...`), the JSON output has `source` (grow), `motive_source` (plan) and `from`
(a progression), and the plan JSON records it in a `source` constraint: on the motive node `m0` for a motive, on the section
for a progression (`docs/plan-format.md`). Nothing reads the note when the plan is filled.

A corpus with no passage scoring 1/2 or more, or none analyzed, exits 2 with `no passage of the corpus ... is similar to ...`
and nothing is written. A passage that is not a MIDI or MusicXML file, or numerals the Analyzer cannot read, is a usage
error; a directory that is not a corpus output is exit 2. Only one bar of melody is taken: for more, cut the passage into a
file and give it to `plan --given-melody`.

## The test styles

`tests/fixtures/styles/chorales.style.json` (ten chorales) and `sonatinas.style.json` (Clementi 36, Mozart 545) were made by
the Analyzer over its test corpora. From a folder holding `chorales/` (the ten `external/ursatz/tests/accuracy/corpus/*/score.musicxml`,
each renamed `<folder>.musicxml`) and `sonatinas/` (the two files in `external/ursatz/tests/fixtures/framework-v1-corpus`):

```sh
for c in chorales sonatinas; do
  ursatz-analyzer --corpus $c --out out-$c
  ursatz-analyzer --style-profile out-$c --name $c --out $c.style.json
done
```
