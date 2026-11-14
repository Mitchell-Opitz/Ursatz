# Main and subordinate themes

The built-in `main-and-subordinate` form after Schoenberg, *Fundamentals of Musical Composition*, ch. XVIII (R-407, R-503 to R-509, P51 B7). Back to the [docs index](README.md). The form is data (`forms/main-and-subordinate.json`, read like any form, [forms.md](forms.md)); B1 to B6 added what it uses: the four parts as roles ([form-parts.md](form-parts.md)), the liquidating transition, the retransition and the coda.

## The form

A main theme in the tonic, a transition that modulates to the key of the subordinate group, the subordinate theme in that key (at the default size a loose sentence and then a lyric period), a codetta that closes the group, a retransition that returns home, the main theme again and a coda.

| Size | Sections | Bars |
| --- | --- | --- |
| **57** (default) | A, T, S1, S2, C, R, A return, K | 8 + 8 + 9 + 8 + 4 + 4 + 8 + 8 |
| 48 | A, T, S1, C, R, A return, K | 8 + 8 + 8 + 4 + 4 + 8 + 8 |

`closing`, `modulates`, any meter, no tempo, one region `subordinate`: V in major, III in minor. The request needs no `modulate`, and a request that asks for it changes nothing.

| Section | Role | Bars | Shape | What it does |
| --- | --- | --- | --- | --- |
| A | `theme` | 8 | presentation + continuation, PAC, closed | The main theme in the tonic. |
| T | `transition` | 8 | presentation + continuation, HC, keys end `subordinate` | Takes the main theme's motive, liquidates it and modulates: its second half is in the region's key and it ends on that key's V, the upbeat chord ([form-parts.md](form-parts.md#the-transition)). |
| S1 | `subordinate` | 9, 8 | presentation + continuation, PAC, in `subordinate`; motive `invert tonal=1` | The subordinate theme: loose in construction. At 57 bars its ideas are 2, 2, 1, 1 and 2 bars with `fragment 2` repeated (R-506); at 48 it is an ordinary 8-bar sentence. |
| S2 | `subordinate`, `lyric` | 8 | antecedent + consequent, PAC, in `subordinate` | A lyric period whose motive is m0 augmented: longer, singing notes (57 bars only). |
| C | `codetta` | 4 | antecedent + cadential, PAC, in `subordinate` | Closes the subordinate group. |
| R | `retransition` | 4 | HC | Returns home over the dominant, ending on the main theme's head ([grow-liquidation.md](grow-liquidation.md#the-retransition)). |
| A return | `return` | 8 | returns A, `exact`, PAC, closed | The main theme again, as it was. |
| K | `coda` | 8 | PAC, closed | Three phrases of 4, 2 and 2 bars, each closing on the tonic ([form-parts.md](form-parts.md#the-coda)). |

## The keys and the cadences

The key lane is home (A), home then the region (T, from its half), the region (S1, S2, C), home (R, A return, K). Each section's KEY constraint is the key it ends in. The return home is at the retransition's start, as in every form that modulates ([form-parts.md](form-parts.md#the-retransition)).

A closing form gives a section that ends in another key a PAC (`piece_plan_cadences`), and `piece_check_cadences` asks for an authentic cadence there. The transition is the exception (R-503, R-512): it ends on the new key's V in a half cadence, not in a PAC, so both leave a transition's cadence as its document says. S1 and the codetta close the region with a PAC, so the modulation is confirmed by authentic cadences heard inside the new key, as `piece_check_keys` asks.

## Composed, not heard

The transition's liquidation, the retransition's pedal and head, the coda's phrases and the key lane are composed to the plan and recorded in it; the hearing does not find them (R-408). A retransition and a coda are not heard as a form, so their rows have `heard` null. What is heard is the themes: A, S1, S2, the codetta and A return, and the Analyzer pairs A return with A (the return heard, below) in most runs, not every one. The checks of a piece read the plan.

## Later (R-509)

- A varied return of A: with `varied` the stand-in composed 6 to 10 of 10 (G major 2/4 stuck at returns, structure and melody), so the return is exact; P52 builds varied returns.
- A minor-mode form with the subordinate group in v.
- A subordinate group in a region other than V and III, and a second transition.

## Measured

Seeds 1 to 10 in the eight rows of key and meter, `modulate` on, an invented motive, each seed tried up to 16 times from the next seeds as the piece entry does (`PIECE_SEED_TRIES`). Composed is the number of seeds that composed. Checks is the number of those that pass every check of the plan: T's second half is in the region (a key span starts at its half), S1, S2 and the codetta are in the region's key throughout, R starts in the home key, and K's last melody note is the tonic. Return heard is the number of those in which the Analyzer's thematic return detection pairs A return with A (not asserted: the detection needs a stretch of six events at a similarity of 0.8 and misses some exact returns, as the ternary's returns heard 9 of 10). Seconds is CPU time a seed. A row under 9 of 10 composed is a blocker.

| Size, row | Composed | Checks | Return heard | Seconds a seed |
| --- | --- | --- | --- | --- |
| 57, C major 4/4 | 10 of 10 | 10 | 9 | 1.2 |
| 57, A minor 4/4 | 10 of 10 | 10 | 9 | 1.4 |
| 57, C major 3/4 | 10 of 10 | 10 | 10 | 0.7 |
| 57, A minor 3/4 | 10 of 10 | 10 | 9 | 0.8 |
| 57, G major 2/4 | 10 of 10 | 10 | 8 | 0.7 |
| 57, D minor 2/4 | 10 of 10 | 10 | 10 | 0.6 |
| 57, C major 6/8 | 10 of 10 | 10 | 9 | 0.5 |
| 57, D minor 6/8 | 10 of 10 | 10 | 10 | 0.6 |
| 48, C major 4/4 | 10 of 10 | 10 | 8 | 0.8 |
| 48, A minor 4/4 | 10 of 10 | 10 | 9 | 0.9 |
| 48, C major 3/4 | 10 of 10 | 10 | 10 | 0.4 |
| 48, A minor 3/4 | 10 of 10 | 10 | 9 | 0.6 |
| 48, G major 2/4 | 10 of 10 | 10 | 8 | 0.7 |
| 48, D minor 2/4 | 10 of 10 | 10 | 10 | 0.4 |
| 48, C major 6/8 | 10 of 10 | 10 | 10 | 0.5 |
| 48, D minor 6/8 | 10 of 10 | 10 | 10 | 0.7 |

Every seed composed on its first try: 160 of 160 runs, all 160 pass the checks, the return is heard in 148 (93%). The tests are `test_form_templates` (the plan's structure, a quick compose run, and the compose and heard sweeps in C major 4/4 and A minor 3/4, invented and given: 40 of 40 runs complete, 220 sections heard, 58 heard as requested) and `test_piece_keys` (20 of 20 pieces pass the key and cadence checks).
