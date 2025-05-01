# Styles

```sh
ursatz-composer plan --form binary --key C --meter 4/4 --style chorales.style.json --out piece.json
ursatz-composer fill piece.json --style-weight 2/1
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --style sonatinas.style.json
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
