# The scherzo

The built-in `scherzo` and `scherzo-and-trio` forms after Schoenberg, *Fundamentals of Musical Composition*, ch. XVI (R-407, R-492 to R-496, P49 B4). Back to the [docs index](README.md). The forms are data (`forms/scherzo.json`, `forms/scherzo-and-trio.json`, read like any form, [forms.md](forms.md)); B1 to B3 added what they use: a form that always modulates, the roles `pattern`, `sequence`, `episode` and `codetta`, [the liquidation](grow-liquidation.md) and a part with its own bars.

## The forms

A scherzo is a ternary with a modulating middle. A is a sentence in the tonic. The middle section is a pattern (P) that opens another key, repeated in sequence at other pitch levels (S1, S2) and then taken apart (L, the liquidation) over a dominant pedal, which leads into A return, extended by an episode (E) and two codettas (C1, C2).

| Form | Sizes | What it is |
| --- | --- | --- |
| `scherzo` | **44**, 60 | 3/4, 2/4 and 6/8 (3/4 first) at 176 quarters a minute, `closing`, `modulates`, nine sections. `repeats` `[["A"], ["P", "S1", "S2", "L", "A return"]]`, as the minuet's, so it is performed A A P S1 S2 L A1 P S1 S2 L A1 E C1 C2: 15 rows, 76 bars at 44 and 108 at 60. |
| `scherzo-and-trio` | **44**, 60 | A compound form in 3/4 at 176: `Scherzo` (form `scherzo`), `Trio` (form `trio` at its own 24 bars, `"bars": 24`, key from the region `trio`, a motive of its own) and `Scherzo da capo` (again `Scherzo`, no repeats). It does not set `modulates`: each part decides. 168 bars performed at 44 (76 + 48 + 44), 216 at 60 (108 + 48 + 60), in 30 rows (15 + 6 + 9). |

The sections of the scherzo, and what each is for:

| Section | Role | Bars (44, 60) | Shape | What it is |
| --- | --- | --- | --- | --- |
| A | `theme` | 8, 16 | presentation + continuation, PAC, closed | The sentence in the tonic. At 60 it has the construction 4 + 4 + 2 + 2 + 4 written out. |
| P | `pattern` | 4 | antecedent + consequent | The pattern: it opens the key of the region `pattern` by pivot, V (minor v), and stays in it. |
| S1 | `sequence` | 4 | returns P, `exact` | The pattern again a step below in the key of the region `sequence`, IV (iv). |
| S2 | `sequence` | 4 | returns P, `exact` | The pattern again, at home. |
| L | `liquidation` | 4 | HC | The liquidation: the pattern halved, then reduced to a bar, a scale line and a broken chord over a dominant pedal, ending the half cadence (the upbeat chord). |
| A return | `return` | 8, 16 | returns A, `varied`, PAC, closed | The tonic again, begun on the tonic after L. |
| E | `episode` | 4 | antecedent + consequent | An episode in IV (iv): the extension of the return. |
| C1, C2 | `codetta` | 4 each | antecedent + cadential; compound basic idea + cadential | Two codettas, the short coda, ending the piece in a PAC. |

## The modulation plan

The key lane is the plan: home, V, IV, home (P, S1, S2), home (L, A return), IV (E), home (C1, C2). The return operator transposes a section by one amount when the key lane has the same shape and mode (`form_return.h`), so S1 and S2 are real sequences of P, not copies in the same key. The scherzo sets `modulates` (R-493), so the keys leave home with no `modulate` in the request, and a request that asks for `modulate` changes nothing. Each section's KEY constraint is the key it ends in.

A section that ends away from home closes in a PAC whatever its document says, so P, S1 and E have a PAC in the plan, and S2, which ends at home before the piece does, keeps the IAC it asks for (`piece_plan_cadences`). The document's IAC for P, S1 and E is not what the plan holds.

Sections as short as these leave little room: with P, S1 and S2 of 8 bars (A 16, size 72) 0 of 10 composed in C major 3/4, and with C1 and C2 of 8 bars (size 68) 0 of 10, so the sizes are 44 and 60 and the sections are 4 bars. A of 16 bars needs the explicit construction (`"auto"` composed 0 of 10 at 10, 12 and 16 bars).

## What the rows say (API 1.21)

Each piece row has `key`, the planned key of the section: the key lane's span under its start, as "G major", so P, S1 and S2 of a scherzo in C major are G major, F major and C major whatever the seed (a performed row takes its composed section's, the trio's rows the trio's key). It is the plan's key, not the heard one: `key_heard` is what the Analyzer hears, and it often differs (the last column of the tables). The forms entry serves `modulates` (true for the scherzo, so `settings.modulate` is false and the choice is not offered) and the part `bars` (24 for the Trio of the scherzo and trio, `null` for the others).

## Composed, not heard

The sequences, the pedal, the upbeat chord and the episode are composed and recorded in the plan. The keys are planned, not heard (R-408): the Analyzer hears the planned key under P, S1 and E in a share of the pieces only (the last column of the tables). The liquidation is not heard as a form (its row has `heard` null). The checks of a piece read the plan: P's key is the key of V (minor v), S1's and E's of IV (iv), S2, L and A return's the home key; L's last chord is V or V7 and A return's first is I or i.

## Later (R-496)

- The pattern's other orders (R-492): the sequences here return P by one transposition of the key lane.
- A liquidated transition between the trio and the scherzo's return (P51).
- An accented accompaniment, a rhythmic scherzo pattern.
- The trio of a scherzo repeated twice, the rondo-like enlargement (P52).
- A coda longer than two codettas (P51).
- Regenerate and variations of a performed form (refused at `/request/form`), and an artificial dominant upbeat chord (R-488).

## Measured

Seeds 1 to 10 through the CLI `piece --seed`, an invented motive, no `modulate` in the request, up to 16 seeds each (the piece entry tries the next seeds when a run is stuck). Composed is the number of requests that composed. In every composed plan P's key is the key of V (minor v), S1's of IV (iv), S2's, L's and A return's home, E's of IV (iv); L's last chord is V or V7 and A return's first is I or i; the performed bars and rows are as above. P, S1 and E heard counts the first P, S1 and E rows that the Analyzer hears in their planned key (not asserted). Seconds is the CPU time of a piece.

| Form, size, key, meter | Composed | Seconds a piece | P, S1, E heard in their planned key |
| --- | --- | --- | --- |
| scherzo 44, C major 3/4 | 10 of 10 | 2.1 | 5, 4, 5 of 10 |
| scherzo 44, A minor 3/4 | 10 of 10 | 3.4 | 2, 0, 0 |
| scherzo 44, G major 3/4 | 10 of 10 | 3.4 | 4, 4, 3 |
| scherzo 44, D minor 3/4 | 10 of 10 | 2.8 | 2, 0, 0 |
| scherzo 44, C major 2/4 | 10 of 10 | 2.3 | 6, 3, 4 |
| scherzo 44, C major 6/8 | 10 of 10 | 4.1 | 10, 9, 6 |
| scherzo 60, C major 3/4 | 10 of 10 | 5.4 | 9, 4, 2 |
| scherzo 60, A minor 3/4 | 10 of 10 | 3.8 | 3, 0, 2 |
| scherzo 60, G major 3/4 | 10 of 10 | 3.8 | 5, 3, 4 |
| scherzo 60, D minor 3/4 | 10 of 10 | 4.3 | 4, 1, 0 |
| scherzo 60, A minor 2/4 | 10 of 10 | 8.7 | 9, 0, 0 |
| scherzo 60, A minor 6/8 | 9 of 10 | 37.2 | 0, 9, 9 (of 9) |
| scherzo and trio 44, C major 3/4 | 10 of 10 | 3.0 | 5, 4, 5 |
| scherzo and trio 44, A minor 3/4 | 10 of 10 | 4.0 | 2, 0, 0 |
| scherzo and trio 60, C major 3/4 | 10 of 10 | 6.1 | 9, 4, 2 |
| scherzo and trio 60, A minor 3/4 | 10 of 10 | 4.5 | 3, 0, 2 |

The one request that did not compose is the scherzo of 60 bars in A minor 6/8 at seed 1: no seed of 1 to 16 composed it (the last was stuck at the bass stage), and the 9 that did take 37 seconds a piece on average because of the 16 tries of that one. The scherzo is 9 of 10 or better in every row, the lowest the aim asks for.

The form tests: `test_form_spec` (the table, both forms, the roles and keys of the nine sections), `test_form_plans` (24 new plan hashes, the scherzo in 3/4 at both sizes, no existing line moved), `test_form_templates scherzo` (structure, a quick compose, and the sweeps `compose` 34 of 40 runs, 272 sections heard, and `heard` 28 sections, both `slow`), `test_piece_keys` (every key check of a plan and of a composed piece without the request to modulate), `test_api_forms` and `test_api_piece` (the forms entry, a scherzo and a scherzo and trio request written for `test_api_schema`).
