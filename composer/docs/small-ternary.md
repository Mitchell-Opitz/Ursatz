# The small ternary form

The built-in `ternary` form after Schoenberg, *Fundamentals of Musical Composition*, ch. XIII (R-445). Back to the [docs index](README.md). The form is data: `forms/ternary.json`, read like any other form document ([forms.md](forms.md)). Sizes are 12 and 24 bars (default 24), a third each; uneven lengths come later.

## The form

| Part | What it is |
| --- | --- |
| A | A period (antecedent + consequent) in the tonic, ending on a PAC. It states the tonic and is closed. With `modulate`, it ends in the `away` region. |
| B | A presentation and a continuation that ends on V with a half cadence, the upbeat chord that leads back to A. Its motive is m0 inverted (a near motive-form). |
| A return | A1: A returned `varied`. |

`away` is V or iii in major and III or v in minor.

Without `modulate` every section is in the home key, and B is written over a dominant pedal: the presentation's two ideas prolong V over the dominant in the bass ([grow-harmony.md](grow-harmony.md)), then the continuation leads to the half cadence. A1 keeps A's melody contour and rhythm and adds at most one passing or neighbor note a bar, changes the harmony by insertion and substitution, and changes the cadence (the chords of its last quarter differ from A's). The return step retries until it does, in the first half of its attempts.

With `modulate` (two seeds in three) A ends in the `away` region and B starts there. B has no pedal, since it does not start in the home key. Its continuation pivots home and ends on V. A1 then has a different key lane from A's, so it cannot repeat A: it is composed anew from A's shape and motive (a reconstruction, as returns with another key lane were before).

## Composed, not heard

The pedal and the region are composed and recorded in the plan. The hearing does not find them (R-408). The cadence heard at the end of B is the last one the recognizer finds in B, and with `modulate` B is heard from the away key, so its V of the tonic is not always heard as a half cadence there.

## Later

- Other regions (IV, vi) for A's ending and B's start.
- A connective figure before A1.
- A1 shorter or longer than A (uneven lengths).
- A tonic pedal and pedals under other beginnings.

## Measured

Seeds 1 to 10, invented motive, up to 16 attempts each (the piece entry's loop: the plan is built again with each attempt's seed). Attempts is the total over the 10 seeds, so 10 means the first attempt composed every one. B's last cadence is the last cadence heard in B, counted when it is a half cadence. Returns heard counts the runs where A1 is heard as A's return. A1 cadence differs counts the runs where a chord in A1's last quarter differs from A's.

| Ternary, size, key, meter | Composed | Attempts | B's last cadence HC | Returns heard | A1 cadence differs |
| --- | --- | --- | --- | --- | --- |
| 24, C major 4/4 | 10 of 10 | 10 | 10 | 9 of 10 | 8 of 10 |
| 24, A minor 3/4 | 10 of 10 | 11 | 10 | 8 of 10 | 9 of 10 |
| 24, D minor 4/4 | 10 of 10 | 10 | 10 | 6 of 10 | 8 of 10 |
| 24, G major 6/8 | 10 of 10 | 10 | 10 | 5 of 10 | 6 of 10 |
| 12, C major 4/4 | 10 of 10 | 10 | 9 | 0 of 10 | 0 of 10 |
| 24, C major 4/4, modulate | 10 of 10 | 10 | 4 | 6 of 10 | 10 of 10 |
| 24, A minor 3/4, modulate | 10 of 10 | 25 | 10 | 10 of 10 | 10 of 10 |
| 24, D minor 4/4, modulate | 10 of 10 | 25 | 10 | 4 of 10 | 6 of 10 |
| 24, G major 6/8, modulate | 10 of 10 | 10 | 5 | 7 of 10 | 8 of 10 |
| 12, C major 4/4, modulate | 10 of 10 | 10 | 2 | 0 of 10 | 3 of 10 |

At 12 bars A1 is 4 bars and its last quarter is one bar, and the hearing finds no return in a piece that short; the same held for the old ternary.

The regression baselines for the form are in `tests/test_form_templates.c` (`BASELINES`) and `tests/test_return_heard.c`, and the plans are pinned by `tests/fixtures/forms/template-plans.txt`.
