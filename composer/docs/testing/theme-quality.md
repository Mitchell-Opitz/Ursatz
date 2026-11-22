# Theme quality: the settings that follow the formal functions

Measured 2026-11-22 for P67 (R-543, gate 2). The four settings of the formal functions turn each idea's harmony, surface and contrast by the function the knowledge entries (K7 and K8) give it: `development.functions.harmony`, `development.functions.surface`, `development.functions.contrast` and `melody.cadence_long`. Every one is off by default, so every plan and every composed file is as it was without them. See [grow-harmony.md](../grow-harmony.md), [grow-continuation.md](../grow-continuation.md), [grow-contrast.md](../grow-contrast.md) and [melody-fill.md](../melody-fill.md).

## The sweep test

`test_theme_functions_sweep` (slow, about 140 s; `test_theme_functions_sweep_smoke` runs seed 1 of every case) grows the four harmony fixtures (`continuation/s77-chords.json`, `continuation/u51-chords.json`, `rates/two-bar-3-4.json` and `motifs/motive-d-minor-arpeggio.json`) as 8-bar sentences and periods, development `balanced`, seeds 1 to 10, through `api_grow_run` with all of `functions` and `melody.cadence_long` on. It asserts:

- at least 8 of 10 seeds compose in each case (R-317); measured, 10 of 10 in all eight;
- a predominant between the last tonic before the final dominant and that dominant, in the response's `chords` of every composed theme;
- the last melody note starts on the theme's last downbeat and ends at its end, and an antecedent's contrasting idea ends the same way;
- in every period, both contrasting ideas at least 1/2 unlike the basic idea in rhythm and in contour (the response's `contrasts`);
- in every 4/4 sentence, two chords in each fragment bar.

`test_theme_functions` holds the cases of each setting alone (harmony: 8 of 8 seeds in six cases; surface; the held cadence; the contrast target).

## The theme measure

`ursatz listen --what themes` batches (20 themes each from seed 500, sentence and period, major and minor, 4/4 and 3/4, taste none), measured by `theme_props.py`, with the four settings on against the baseline (`edc4b5dc`):

| Measure | Baseline | Four settings on |
|---|---|---|
| Themes composed (8 batches) | 126 of 160 | 121 of 160 (every batch at 84% to 106% of its baseline count) |
| Predominant before the final dominant | 24 of 126 | 121 of 121 |
| Final note longer than the mean | 18 of 126 | 121 of 121 |
| Contrasting idea with the basic idea's rhythm over their common length | 64 of 64 | 0 of 59 |
| Consequent's contrasting idea with the consequent basic idea's rhythm | 64 of 64 | 0 of 59 |
| Contrasting idea's mean rhythm and contour distance from the basic idea | 0.00 and 0.58 | 0.97 and 0.98 |
| Final note over the theme's mean note value | 1.77 | 3.21 |
| Chords in a 4/4 sentence's fragment bar | 1.00 | 2.00 |

The listening round of gate 3 (`Listen Ursatz`: themes rated at 3.5 or more, the contrast heard) follows the merge.

## What the contrasts measure

The response's `contrasts` measure the motives the ideas are made from, which is what the draw holds to the target, not the melody the melody stage writes from them: with every setting on, the stage moves notes to the chords and holds the last note, and one contrasting idea of 80 (the 3/4 motif, seed 3) had a contour distance of 0.2 between its melody and the basic idea's while its motive's was above 1/2.
