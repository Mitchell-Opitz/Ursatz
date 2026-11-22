# Contrast distance and new material

A contrasting idea that is far from the basic idea instead of a copy of its rhythm, or made of new material (R-475 to R-477; Michelle's listening request 18). Back to the [docs index](README.md); the ideas and their choices are in [grow.md](grow.md).

Off by default (R-422). `GrowContrast` (`grow.h`) has `distance`, the weights of `GROW_CONTRAST_SIMILAR` and `GROW_CONTRAST_DISTANT` (all 0 reads as similar 1), and `new_material`, the weight of new material (see below). It is carried in `GrowContext.contrast`, `GrowSetup.contrast` and `GrowRequest.contrast` (NULL or zero for the defaults), with the motif vocabulary a drawn rhythm uses in `GrowContext.motif` and `GrowSetup.motif` (`MotiveRandomPreferences`, NULL for the defaults). Nothing sets them from the preferences document yet (B6). With every weight 0 no random number is drawn and every plan is the plan without them.

## Similar and distant

A similar contrast is today's: a development of the source (an inversion or an interval change) whose contour differs, on the source's rhythm. A distant contrast keeps that contour change and takes its rhythm from the motif generator, so the idea differs in both. It applies to a period's contrasting idea, a consequent's contrasting idea and a sentence's fragment 1 (not cut, not diminished, not a slot's own motive); not to fragment 2, the cadential idea or the repetition.

## How a distant contrast is made

`grow_draw_contrast_kind` weighs similar and distant (one random number, none when the distant weight is 0 and new material is not drawn). For a distant one, `grow_make_distant` draws today's contrast step first, then up to 8 times:

1. A rhythm from `motive_random` on its own seeded generator: one bar or two (the idea's), the theme's key, mode and meter, no pickup, no plan, the request's motif settings or the defaults. It is refused when it fails, is longer than the idea's room, or has the same onsets and durations as the source over their common length.
2. The pitches of the developed source spread over the rhythm's notes (note j of k takes the developed note `(j(n-1) + (k-1)/2) / (k-1)`); a note equal to the one before moves one scale step toward the next different pitch.
3. It is refused when it keeps the source's contour, or cannot be moved by whole octaves into MIDI 58 to 84 (in the key's scale when the source is).

The first one whose last note lands on the cadence's tone is taken (the consequent's, as the similar contrast), else the first made. The result is the idea's own motive, `distant for m<k>`, a Realized material node restated exact, so the melody stage is as motive-driven as for any slot motive and fills nothing after it with the source's rhythm. The theme's developments name it `a distant variant` and its slot row has source `distant`. The derived motive nodes and the material take its bars.

## New material

`new_material` weighs a contrasting idea made of new material against the similar and distant weights: `grow_draw_contrast_kind(request, rng, new_allowed)` draws one of three kinds, with total = similar + distant + (new_allowed ? new_material : 0). It is allowed for the first contrasting idea of a theme with a contrasting slot (a period's antecedent or a compound basic idea) when no motive is given for the slot or the idea; a sentence has no contrasting slot and none is made. When only similar has weight no random number is drawn; with both distance weights 0 and `new_material` above 0, similar has no weight, so the draw is new material or, when none is made, similar.

`grow_make_new_material` draws up to 8 times from `motive_random` as a distant rhythm is drawn (the idea's one or two bars, the key, the meter, no pickup, the request's motif settings or the defaults). A motive is refused when it is longer than the idea's room, keeps the basic idea's contour, or cannot be moved by whole octaves into MIDI 58 to 84. The first whose last note lands on the cadence's tone is taken (the antecedent's has no wanted tone), else the first made.

The motive is the contrasting slot's source, recorded as the Realized material node `new material` (`GrowThemeSpec.has_new_material`, `GrowBuild.new_node`). The first contrasting idea is that motive as it is (the first idea of a slot, exact, as a given slot motive); the consequent's contrasting idea is made from it by today's contrast step, similar or distant by the weights, and is never the antecedent's. The theme's developments read `contrasting idea = new material, exact` and `consequent contrasting idea = new material, inverted ...`; the slot rows made from it have source `new`. The plan's material is sized for it (`GrowRequest.contrast.new_material`: a motive of up to two bars, as a distant one) and the derived motive nodes take its bars.

## Contrast targets

With `development.functions.contrast` on (R-710) and a knowledge snapshot, `knowledge_snapshot_theme_type_of` of the pairing gives the idea's formal function (`omt.function.contrasting-idea`: contrast against the basic idea, rhythm and contour by 1/2). A contrasting idea with no motive of its own, of an antecedent, a compound basic idea or a consequent, is then drawn by `grow_make_distant` (unless `contrast.new_material` draws new material), and a made motive is kept only when `dimension_contrast` of it against m0 reaches every amount of the function (`grow_contrast_met`; the notes from the start, the motive's length, a beat of 1/`beat_unit`, 3/8 in 6/8 and 12/8), within its 8 tries. When none does the structure candidate is dropped. The idea node gets a `contrast` constraint ([plan-constraints.md](plan-constraints.md)) with `against` `basic idea` (`consequent basic idea` for the consequent), and the theme response gives `contrasts` ([theme-request.md](theme-request.md#in-the-api)), measured on the motives. A target's motive is drawn no longer than the material is sized for (one bar when the motif is one bar: a longer one raises the bars every motive of the theme takes). Off, nothing is drawn or written for it.

## Fallbacks

- No rhythm passes, no pitches fit, or the draw for the idea fails: the similar contrast stands. Failed new material is the similar contrast, never a distant one.
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

### New material (B2)

The same batches, periods only (a sentence has no contrasting slot and is unchanged), 8 batches off and 8 on with `new_material` 1 and distance {0, 0} through a local build (not committed: B6 makes it a setting).

| Measure | Off | New material |
|---|---|---|
| Themes composed (8 batches) | 33 to 56 of 60, 370 | 33 to 56 of 60, 371 |
| Contrasting idea with the basic idea's rhythm over their common length | 370 of 370 | 24 of 371 |
| Consequent's contrasting idea with the consequent basic idea's rhythm | 370 of 370 | 23 of 371 |
| Final note over the theme's mean note value, per batch | 1.06 to 1.89 | 1.05 to 1.29 |
| Final note longer than the mean | 250 of 370 | 179 of 371 |
| Longest run of bars of 2 or more notes with one rhythm | 8 per batch; over 3 in 303 of 370 | 2 to 8; over 3 in 24 of 371 |
| Notes under a sixteenth | 0 | 0 |
| Leaps (5 semitones or more) per theme, per batch | 2.5 to 6.8; on a downbeat 1.9 to 4.0 | 4.9 to 8.8; on a downbeat 2.0 to 4.6 |
| Largest move of the mean pitch between two ideas | 6.7 to 8.9 | 5.9 to 8.1 |

New material removes the shared rhythm of the contrasting ideas and the long runs of one rhythm, as a distant contrast does, and the contrast is a motive of its own rather than a variant. A new motive is not refused for sharing the basic idea's rhythm, so 24 of 371 still do. Its costs: the final note is longer than the mean in fewer periods (179 of 371, B3 has the ending to do) and the themes leap more (4.9 to 8.8 per theme against 2.5 to 6.8), a motive of its own having no contour kept from the basic idea; B5 has the leaps.
