# Contrast distance

A contrasting idea that is far from the basic idea instead of a copy of its rhythm (R-475, R-476; Michelle's listening request 18). Back to the [docs index](README.md); the ideas and their choices are in [grow.md](grow.md).

Off by default (R-422). `GrowContrast` (`grow.h`) has `distance`, the weights of `GROW_CONTRAST_SIMILAR` and `GROW_CONTRAST_DISTANT` (all 0 reads as similar 1), and `new_material` (B2). It is carried in `GrowContext.contrast`, `GrowSetup.contrast` and `GrowRequest.contrast` (NULL or zero for the defaults), with the motif vocabulary a drawn rhythm uses in `GrowContext.motif` and `GrowSetup.motif` (`MotiveRandomPreferences`, NULL for the defaults). Nothing sets them from the preferences document yet (B6). With every weight 0 no random number is drawn and every plan is the plan without them.

## Similar and distant

A similar contrast is today's: a development of the source (an inversion or an interval change) whose contour differs, on the source's rhythm. A distant contrast keeps that contour change and takes its rhythm from the motif generator, so the idea differs in both. It applies to a period's contrasting idea, a consequent's contrasting idea and a sentence's fragment 1 (not cut, not diminished, not a slot's own motive); not to fragment 2, the cadential idea or the repetition.

## How a distant contrast is made

`grow_draw_distance` weighs the two (one random number, none when the distant weight is 0). For a distant one, `grow_make_distant` draws today's contrast step first, then up to 8 times:

1. A rhythm from `motive_random` on its own seeded generator: one bar or two (the idea's), the theme's key, mode and meter, no pickup, no plan, the request's motif settings or the defaults. It is refused when it fails, is longer than the idea's room, or has the same onsets and durations as the source over their common length.
2. The pitches of the developed source spread over the rhythm's notes (note j of k takes the developed note `(j(n-1) + (k-1)/2) / (k-1)`); a note equal to the one before moves one scale step toward the next different pitch.
3. It is refused when it keeps the source's contour, or cannot be moved by whole octaves into MIDI 58 to 84 (in the key's scale when the source is).

The first one whose last note lands on the cadence's tone is taken (the consequent's, as the similar contrast), else the first made. The result is the idea's own motive, `distant for m<k>`, a Realized material node restated exact, so the melody stage is as motive-driven as for any slot motive and fills nothing after it with the source's rhythm. The theme's developments name it `a distant variant` and its slot row has source `distant`. The derived motive nodes and the material take its bars.

## Fallbacks

- No rhythm passes, no pitches fit, or the draw for the idea fails: the similar contrast stands.
- A 4-bar 3/4 theme makes none: `motive_random` has no one-bar motive in 3/4, so a sentence's fragment 1 in 3/4 stays similar.

## Measure

`listen --what themes --count 60 --seed 500`, 8 bars, C major and C minor, 4/4 and 3/4, sentence and period, with the `none` and `uematsu-ff7` settings of the taste profile (R-481), read from each sample's MusicXML melody and `structure`. Off is the baseline; on has `distance` {0, 1} through a local build (not committed: B6 makes it a setting). Recorded, not asserted.

| Measure | Off | Distant |
|---|---|---|
| Themes composed (16 batches) | 33 to 56 of 60, 735 | 33 to 56 of 60, 736 |
| Periods: contrasting idea with the basic idea's rhythm over their common length | 370 of 370 | 0 of 371 |
| Periods: consequent's contrasting idea with the consequent basic idea's rhythm | 370 of 370 | 0 of 371 |
| Sentences: fragment 1 with the basic idea's rhythm | 271 of 365 (4/4 style none 24 of 105) | 148 of 365 (4/4 8 of 201; 3/4 140 of 164; 4/4 style none 5 of 105) |
| Final note over the theme's mean note value, per batch | periods 1.06 to 1.89, sentences 1.17 to 3.23 | periods 1.02 to 1.28, sentences 1.17 to 3.27 |
| Final note longer than the mean | periods 250 of 370, sentences 347 of 365 | periods 190 of 371, sentences 346 of 365 |
| Longest run of bars of 2 or more notes with one rhythm | 6 to 8 per batch; over 3 in 604 of 735 | 2 to 6; over 3 in 301 of 736 |
| Notes under a sixteenth | 6 | 6 |
| Leaps (5 semitones or more) per theme, per batch | 2.5 to 9.8; on a downbeat 1.9 to 5.0 | 2.5 to 9.5; on a downbeat 1.9 to 4.9 |
| Largest move of the mean pitch between two ideas | 6.7 to 12.1 | 6.6 to 13.7 |

The distant contrast does what it is for: no period contrast or consequent contrast keeps the basic idea's rhythm, and runs of one rhythm shorten. Its cost is that the consequent's final note is longer than the mean in fewer periods (190 of 371 against 250 of 370): the cadence's tone is chosen in the pitches, not the rhythm, so B3's closure setting has the ending to do. Sentences in 3/4 keep a similar fragment 1. The baseline figures here are read with the same reader as the distant ones and differ from the phase table by a few themes (sentences 271 against 265, runs over 3: 604 against 584).
