# Sonata-allegro at sonatina size

The built-in sonatina forms: a first-movement sonata form after Schoenberg, *Fundamentals of Musical Composition*, ch. XX, at sonatina size (R-407, R-523, R-524, P53 B2). Back to the [docs index](README.md). The forms are data (`forms/sonatina.json`, `forms/sonatina-tonic-major.json`, read like any form, [forms.md](forms.md)); the role `elaboration` is new (a theme-like role with no rules of its own, as `contrast`).

## The shape

A sonata-allegro has three parts: the exposition (the main theme, the transition, the subordinate theme in V or III, the codettas), the elaboration (the development: a working-out of the exposition's material in other regions, ending on the retransition) and the recapitulation (the exposition again, the subordinate group in the tonic). Both forms are 92 bars, tempo 132, `closing`, `modulates`, any meter, one size.

| Part | Section | Role | Bars | Shape | Key |
| --- | --- | --- | --- | --- | --- |
| Exposition | A | `theme` | 8 | presentation + continuation, PAC, closed | home |
| | T | `transition` | 8 | presentation + continuation, HC | ends in `subordinate` |
| | B | `subordinate` | 8 | presentation + continuation, PAC; motive `invert tonal=1` | `subordinate` (V major, III in minor) |
| | C1 | `codetta` | 4 | antecedent + cadential, PAC | `subordinate` |
| | C2 | `codetta` | 4 | compound basic idea + cadential, PAC | `subordinate` |
| Elaboration | E | `elaboration` | 8 | presentation + continuation, PAC; returns B `embellish` | `elaboration` (IV in major, VI in minor; major) |
| | P | `pattern` | 4 | antecedent + consequent, IAC (planned as a PAC: it ends away from home) | `development` (vi in major, iv in minor) |
| | S | `sequence` | 4 | antecedent + consequent; returns P `exact` | `sequence` (ii in major, v in minor) |
| | R | `retransition` | 4 | HC | home |
| Recapitulation | A1 | `return` | 8 | returns A `exact`, closed | home |
| | T1 | `transition` | 8 | presentation + continuation, HC | home |
| | B1 | `subordinate` | 8 | returns B | home (`sonatina`) or the tonic major (`sonatina-tonic-major`) |
| | C3 | `codetta` | 4 | returns C1 | as B1 |
| | C4 | `codetta` | 4 | compound basic idea + cadential | home |
| | K | `coda` | 8 | PAC, closed | home |

The exposition is 32 bars, the elaboration 20, the recapitulation 40. The elaboration works in falling segments: E is 8 bars, P and S 4 each, then the retransition's 4 bars as segments of 1, 1, 1, 1/2 and 1/2 bar, as the chapter has it for a development that shortens its units as it nears the return. E is B taken to IV (VI) and embellished; P and S are the sonata rondo's pattern and its sequence, so the sequence follows its pattern in another key ([rondo.md](rondo.md)).

## The two recapitulations

The subordinate group (B1 and C3) returns in the tonic, and in a minor piece it must say in which mode:

| Form | B1, C3 | In a major piece | In a minor piece |
| --- | --- | --- | --- |
| `sonatina` | `reharmonize`, `"mode": "home"` (R-524) | the home key, B and C1 moved from V by the tonic step | the home minor: B and C1 (in III) are moved by the tonic step, mapped degree by degree into the minor and reharmonized. This is the plan of Beethoven, Op. 2/1 and Mozart, K. 421, whose first movements bring the second group back in the tonic minor |
| `sonatina-tonic-major` | `exact`, `"mode": "major"` (R-517) | as `sonatina` | the tonic major (A major in A minor): B and C1 moved from III; C4 and K turn back to minor, as in Beethoven, Op. 18/4 |

B1, C3 and C4 close in the home key and are not closed, so a major piece plans them a half cadence (`piece_cadences.c`: a section that ends at home before the last one and is not `closed` has no PAC); K carries the piece's last PAC. In the tonic major of a minor piece B1 and C3 end away from the home key and keep their PAC (as the rondos' B1, [rondo.md](rondo.md)).

## What the rows and the forms entry say

The forms entry (API 1.25, R-525) gives B1 and C3 of `sonatina` `"mode": "home"`, B1 and C3 of `sonatina-tonic-major` `"mode": "major"`, and E the role `elaboration` (label `Elaboration`). A row whose copy crosses modes names it: in a minor piece B1 and C3 of `sonatina` have `changes.mode` `"minor"` and `text` with the part `mode: mapped into minor` before the harmony part; in a major piece the copy keeps the mode and `changes.mode` is `null`. The tonic major recapitulation's rows say `null` too: that copy is exact in the keys of its own mode.

## What is heard and what is only composed

The themes (A, B, E's embellished B, the codettas) are composed and heard back as the form they were asked for in a share of the runs (the baselines below, as for every form, R-408); the keys, the retransition, the coda and the returns are composed to the plan and recorded in it, not heard as a form. The checks read the plan.

## What was measured

`tests/test_form_families.c` (`test_sonata_forms_<form>`, slow, one test per form) composes each form in eight rows of key and meter, seeds 1 to 10, each seed tried again from the next seeds, up to 16 times, as the piece entry does (`PIECE_SEED_TRIES`). Every row composes 10 of 10. Each composed run is checked: B, C1 and C2 in the subordinate key throughout; E in the elaboration key throughout, B's melody moved by the key difference at its onsets with notes added; P and S in the development and sequence keys, S's melody P's moved; R, A1, T1 and C4 at home; B1 and C3 with the onsets and durations of B and C1 and the pitches moved (and, in `sonatina` in a minor piece, mapped), in the home key or the tonic major; K ending on the tonic.

| Row | `sonatina`: first seed / tries / s a seed | `sonatina-tonic-major` |
| --- | --- | --- |
| C major 4/4 | 7 / 14 / 5.0 | 9 / 11 / 2.1 |
| A minor 4/4 | 6 / 20 / 8.0 | 10 / 10 / 2.7 |
| C major 3/4 | 10 / 10 / 1.4 | 10 / 10 / 1.1 |
| A minor 3/4 | 9 / 11 / 1.5 | 10 / 10 / 1.5 |
| G major 2/4 | 8 / 12 / 2.0 | 9 / 11 / 1.2 |
| D minor 2/4 | 9 / 11 / 1.4 | 10 / 10 / 0.9 |
| C major 6/8 | 3 / 28 / 6.4 | 9 / 11 / 1.5 |
| D minor 6/8 | 3 / 22 / 4.5 | 7 / 14 / 1.9 |

Seconds are the CPU time of a seed over its tries, four tests side by side on 4 cores. The template sweep (`test_form_templates compose`, seeds 1 to 5, C major 4/4 and A minor 3/4, invented and given motive) completes 13 of 20 runs for `sonatina` and 15 of 20 for `sonatina-tonic-major`: the given motive in C major 4/4 gets stuck in all 5 runs of both, the invented one in 2 of 5 for `sonatina`; the A minor 3/4 runs complete 10 of 10. The sections heard as requested are 46 of 300 (`sonatina`) and 51 of 300 (`sonatina-tonic-major`). `test_piece_keys` sweeps both (seeds 1 to 10, C major and A minor) and every run passes every key and cadence check.
