# Rondo forms

The built-in Andante and small rondo forms after Schoenberg, *Fundamentals of Musical Composition*, ch. XIX (R-407, R-515 to R-520, P52 B5). Back to the [docs index](README.md). The forms are data (`forms/andante-aba.json`, `forms/andante-abab.json`, `forms/small-rondo-ababa.json`, `forms/small-rondo-abaca.json`, read like any form, [forms.md](forms.md)); B1 to B4 added what they use: a size of up to 16 sections, the return variations `embellish` and `setting`, and a returning section in the tonic major.

## The classification

The forms are named by their parts, the way the chapter sorts them: the Andante form is ABA or ABAB, the small rondo ABABA or ABACA. A is the main theme and comes back in the tonic. B is the subordinate theme, in V (III in minor) the first time and in the tonic the second. C is the contrast, a section in a more remote region. R is the retransition home and T a transition. Each form ends in a coda, K. The large and sonata rondos are not here.

| Form | Parts | Bars | Tempo |
| --- | --- | --- | --- |
| `andante-aba` | A T B R A1 K | 8 + 8 + 8 + 4 + 8 + 8 = 44 | 72 |
| `andante-abab` | A T B R A1 T1 B1 K | 8 + 8 + 8 + 4 + 8 + 4 + 8 + 8 = 56 | 72 |
| `small-rondo-ababa` | A T B R A1 T1 B1 R1 A2 K | 8 + 8 + 8 + 4 + 8 + 4 + 8 + 4 + 8 + 8 = 68 | 88 |
| `small-rondo-abaca` | A T B R A1 C R1 A2 K | 8 + 8 + 8 + 4 + 8 + 8 + 4 + 8 + 8 = 64 | 88 |

Each has one size (the default), `closing`, `modulates` and no meters (any meter). The request needs no `modulate`. The regions are `subordinate` (V in major, III in minor) and, for the small rondo ABACA, `second` (vi or IV in major, VI or iv in minor).

| Section | Role | Bars | Shape | What it does |
| --- | --- | --- | --- | --- |
| A | `theme` | 8 | presentation + continuation, PAC, closed | The main theme in the tonic. |
| T | `transition` | 8 | presentation + continuation, HC, keys end `subordinate` | Modulates to the subordinate region ([form-parts.md](form-parts.md#the-transition)). |
| B | `subordinate` | 8 | presentation + continuation, PAC, in `subordinate`; motive `invert tonal=1` | The subordinate theme. |
| R, R1 | `retransition` | 4 | HC | Returns home over the dominant ([grow-liquidation.md](grow-liquidation.md#the-retransition)). |
| A1 | `return` | 8 | returns A, `embellish`, PAC, closed | A again at home, its eighth notes split into sixteenths where it has no longer note to ornament (R-516). |
| A2 | `return` | 8 | returns A, `setting` `arpeggiated`, PAC, closed | A again with its harmony, bass and melody and a new accompaniment in the arpeggiated pattern (R-518). |
| T1 | `transition` | 4 | presentation + continuation, HC | The returning B's transition: 4 bars at home, grown afresh (R-517). |
| B1 | `subordinate` | 8 | returns B, `exact`, `"mode": "major"` | B again, a copy of B in the tonic major (R-517). |
| C | `contrast` | 8 | antecedent + consequent, PAC, in `second`; motive `interval-change` | The contrast, in a remoter region than B's (ABACA only). |
| K | `coda` | 8 | PAC, closed | Three phrases closing on the tonic ([form-parts.md](form-parts.md#the-coda)). |

## A's returns and their variations

A comes back varied, and each time differently, so the form never repeats itself. A1 is `embellish` (the ornament melody; where the melody has no note longer than an eighth to split, its eighths are split into sixteenths): it keeps every melody note of A at its place and pitch and adds notes. A2 is `setting`: it keeps A's harmony, bass and melody and gives the accompaniment a new pattern, so its inner voice 1 has notes where A's has none. The piece's own accompaniment pattern, and alberti in a compound meter, are skipped for the next pattern, so the setting always differs from the source's ([piece.md](piece.md)). `reharmonize` and `reregister` are not used here (R-519).

## B in the tonic and its changed transition

B1 returns B exact in the tonic major (`"mode": "major"`, R-517): its key is the home tonic in major, so the home key in a major piece and the parallel major in a minor one, with no draw, and its melody is B's moved by the shortest step between the two keys (up 5 semitones from V in a major piece, down 3 from III to A major in A minor). The transition before it is changed: T1 is a non-modulating transition of 4 bars at home, grown afresh, in place of T's 8 bars to the region. In minor the music turns back to the minor at the section after B1, a change of mode on one tonic with no pivot (R-499).

B1 is not closed, so in a major piece, where the tonic major is the home key, a closing form plans its PAC as a half cadence (`piece_plan_cadences`); in a minor piece it is in A major, not the home key, and keeps its PAC. The tests expect both.

## The contrast

C of the small rondo ABACA is a theme of 8 bars (antecedent + consequent, PAC, motive `interval-change`) in the `second` region, which is a draw: vi or IV in a major piece (A minor or F major in C major), VI or iv in a minor one (F major or D minor in A minor). It starts and ends in its key, so it is one key throughout. R1 then returns home straight from it (below).

## The keys

| Form | Home (A, R, R1, A1, A2, K) | The region (T's second half, B) | Tonic major (T1 at home, B1) | Second region (C) |
| --- | --- | --- | --- | --- |
| `andante-aba` | A, R, A1, K | T from its half, B | none | none |
| `andante-abab` | A, R, A1, T1, K | T from its half, B | B1 | none |
| `small-rondo-ababa` | A, R, A1, T1, R1, A2, K | T from its half, B | B1 | none |
| `small-rondo-abaca` | A, R, A1, R1, A2, K | T from its half, B | none | C |

Each section's KEY constraint is the key it ends in. A retransition opens the home key straight from the key before it, over the dominant, with no pivot chord (R-505): that is a pivot only when the region is V, so `test_piece_keys` does not ask a retransition for one (the small rondo's C is in IV or vi, whose pivot chords V is not).

## What is heard and what is only composed

The themes are composed and heard back as the form they were asked for in a share of the runs (the table below, as for every form, R-408); the keys, the retransitions, the coda and the returns are composed to the plan and recorded in it, not heard as a form. The checks read the plan.

## What was measured

`tests/test_rondo_forms.c` (`test_rondo_forms_<form>`, `LABELS slow`): the eight rows of key and meter (C major and A minor in 4/4 and 3/4, G major and D minor in 2/4, C major and D minor in 6/8), seeds 1 to 10, each seed tried again from the next seeds, up to 16 times, as the piece entry does. At least 9 of 10 must compose in each row, and in every composed run: B is in V (III) from its first bar; R and R1 start at home; A1 keeps every melody note of A and has more; B1 is in the tonic major, its melody is B's moved by the key difference and the section after it starts at home; A2's harmony, bass and melody are A's and its inner voice 1 has notes where A's has none; C is in one of its region's keys throughout; K ends on the tonic.

| Form | Composed of 10 in each row | On the first seed alone, per row (C 4/4, a 4/4, C 3/4, a 3/4, G 2/4, d 2/4, C 6/8, d 6/8) | Runs a row | Seconds a seed |
| --- | --- | --- | --- | --- |
| `andante-aba` | 10 | 10, 10, 10, 10, 10, 9, 10, 10 | 10 to 11 | 0.3 to 0.5 |
| `andante-abab` | 10 | 10, 10, 10, 10, 10, 9, 10, 10 | 10 to 11 | 0.3 to 0.7 |
| `small-rondo-ababa` | 10 | 10, 10, 10, 10, 9, 9, 10, 10 | 10 to 11 | 0.3 to 0.8 |
| `small-rondo-abaca` | 10 | 10, 10, 10, 10, 8, 9, 10, 9 | 10 to 12 | 0.5 to 1.1 |

(Seconds are CPU seconds of a debug build with the four tests running side by side.)

`tests/test_form_templates.c` runs each form in C major 4/4 and A minor 3/4, invented and given motive, seeds 1 to 5: every run composes, and the baselines are the sections heard and heard as requested.

| Form | Runs complete | Sections heard | Heard as requested |
| --- | --- | --- | --- |
| `andante-aba` | 20 of 20 | 80 | 36 of 120 |
| `andante-abab` | 20 of 20 | 120 | 43 of 160 |
| `small-rondo-ababa` | 20 of 20 | 140 | 46 of 200 |
| `small-rondo-abaca` | 20 of 20 | 120 | 46 of 180 |

The plans are pinned in `tests/fixtures/forms/template-plans.txt` (`test_form_plans record` added only the four forms' lines).
