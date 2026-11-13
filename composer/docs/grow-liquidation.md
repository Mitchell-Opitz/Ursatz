# The liquidation section

A section of a form that takes the pattern apart over a dominant pedal and ends in the half cadence (R-494). Back to the [docs index](README.md); the form side is in [forms.md](forms.md), the constraint in [plan-constraints.md](plan-constraints.md), and the other grow sections in [grow-harmony.md](grow-harmony.md).

## The section

A form section of role `liquidation` has 4 bars, the half cadence and no theme shape. grow-theme builds it with its own operator (`src/grow/grow_liquidate.c`), not as a theme: the section has a `liquidation` constraint and no `archetype` or `theme_pairing`. The reader's refusals are in [forms.md](forms.md#a-section-of-a-size).

The operator applies to a Constrained section with no children and a `liquidation` constraint, no `return`, `hold`, `archetype` or `theme_pairing`, a half cadence, 4 whole bars from a barline, one meter span over it and one key span (a liquidation never opens a key). Anything else it leaves to the other operators. A draw whose source motive has fewer than 2 notes or is longer than a bar offers no candidate.

## The layout

One `phrase` node over the section, with the cadence, holds five ideas named as the motives stage scores a theme's ideas:

| Idea | Length | What it is |
| --- | --- | --- |
| `basic idea` | 1 bar | m0, exact |
| `repetition` | 1 bar | m0 moved one step down the key's scale (`transpose tonal=1 by=-1`), a motive node `m1` |
| `fragment 1` | 1 bar | m0 liquidated (`liquidate drop=<n>`, 1 to half the notes), a motive node `m2` |
| `fragment 2` | half a bar | the scale line, a motive node `scale line` |
| `cadential` | half a bar | the broken chord, a motive node `broken chord` |

The four motive nodes go in the material section, a bar each, after the motives already there.

## The pedal

The harmony is one prolongation of V over the whole phrase with the pedal (`harmony_shape` `prolongation`, `starts_on` and `ends_on` `dominant`, `pedal` true), as a presentation under a pedal is written (R-446). Every chord is V (or V7) with the dominant's pitch class in the bass, and the section ends on V.

## The two made motives

The scale line and the broken chord are literal motives the operator makes (a copy of m0 with its notes and length set, no features or tone labels, and V for its own harmony), eighth notes: 3 in 3/4 and 6/8, 2 in 2/4.

- The scale line: stepwise in the key's scale (the harmonic minor in minor), the direction drawn (down or up), its first note degree 2 or 4 (drawn) in the octave nearest the last note of `fragment 1`, within MIDI 58 to 84.
- The broken chord: ascending, the tones of V (degrees 5, 7, 2) from the degree 5 nearest the scale line's last note; the last note is degree 2 or 7, a tone of the half cadence. In minor the seventh is raised.

A run outside the range is moved an octave; a draw that cannot be placed offers no candidate. The scale line is written into the plan as a melody span (Realized), because the melody stage moves a note that is not a tone of V to a chord tone and a scale line over a pedal is made of them; the broken chord is a target as the other ideas are, with the cadence on its span.

## The candidates and the rules

Up to `GROW_DEFAULT_CANDIDATES` distinct draws (the drop, the direction and the first degree), none repeating another's liquidated idea and runs; the label is `liquidation <drop> dropped, scale <down|up>`.

Hard rules: every idea fits its span, and `fragment 1` has fewer notes than m0. Soft: the share of the four ideas after the first that are shorter than the one before, by span, or by notes when the spans are as long.

## Measured

`test_grow_liquidate` composes `tests/fixtures/forms/liquidation-form.json` (A 8 presentation and continuation PAC, S 4 antecedent and consequent IAC, L 4 liquidation, A return 8 exact; 24 bars) with an invented motive, seeds 1 to 10:

| Key and meter | Composed |
| --- | --- |
| C major 3/4 | 10 of 10 |
| A minor 3/4 | 10 of 10 |
| C major 2/4 | 10 of 10 |
| C major 6/8 | 10 of 10 |

Each composed run has the five ideas, V in the bass throughout, a last chord V or V7, a last melody note that is a tone of V, a `fragment 1` that asks for fewer notes than m0, a stepwise scale line and an ascending broken chord. With the A return `varied` the same form composes 7 of 10 in C major 2/4 whether or not the section is a liquidation (the other rows 10 of 10), so the fixture's A return is `exact`.

## Not here

A liquidation of 8 bars or with more ideas, one that opens a key and one not on a dominant pedal are Later (R-494). The scherzo form that uses it is B4 of P49.
