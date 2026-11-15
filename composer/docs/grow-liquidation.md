# The liquidation section

A section of a form that takes the pattern apart over a dominant pedal and ends in the half cadence (R-494), and, laid out the other way, the retransition that leads back home (R-505). Back to the [docs index](README.md); the form side is in [forms.md](forms.md), the constraint in [plan-constraints.md](plan-constraints.md), and the other grow sections in [grow-harmony.md](grow-harmony.md).

## The section

A form section of role `liquidation` has 4 bars, the half cadence and no theme shape. grow-theme builds it with its own operator (`src/grow/grow_liquidate.c`), not as a theme: the section has a `liquidation` constraint and no `archetype` or `theme_pairing`. The reader's refusals are in [forms.md](forms.md#a-section-of-a-size).

The operator applies to a Constrained section with no children and a `liquidation` constraint, no `return`, `hold`, `archetype` or `theme_pairing`, a half cadence, 4 whole bars from a barline, one meter span over it and one key span (a liquidation never opens a key). A half bar holds 2 to 4 eighths, so it works in 2/4, 3/4, 6/8 and 4/4 (R-505). The same operator grows a retransition (below). Anything else it leaves to the other operators. A draw whose source motive has fewer than 2 notes or is longer than a bar offers no candidate.

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

The scale line and the broken chord are literal motives the operator makes (a copy of m0 with its notes and length set, no features or tone labels, and V for its own harmony), eighth notes: 4 in 4/4, 3 in 3/4 and 6/8, 2 in 2/4.

- The scale line: stepwise in the key's scale (the harmonic minor in minor), the direction drawn (down or up), its first note degree 2 or 4 (drawn) in the octave nearest the last note of `fragment 1`, within MIDI 58 to 84.
- The broken chord: ascending, the tones of V (degrees 5, 7, 2) from the degree 5 nearest the scale line's last note, and in four eighths (4/4) the root again an octave up (5, 7, 2, 5), so the last note is a tone of V (the seventh, degree 4, would end on a dissonance). In minor the seventh is raised.

A run outside the range is moved an octave; a draw that cannot be placed offers no candidate. The scale line is written into the plan as a melody span (Realized), because the melody stage moves a note that is not a tone of V to a chord tone and a scale line over a pedal is made of them; the broken chord is a target as the other ideas are, with the cadence on its span.

## The retransition

A section with the `retransition` function (and no `liquidation` constraint) is grown by the same operator in a second layout, for the part of a larger form that leads home from another region (R-505, [form-parts.md](form-parts.md)). The five ideas have the same spans (1, 1, 1, 1/2 and 1/2 bars), the same phrase, the same prolongation of V over the pedal and the same idea names, but each is made differently, and all five have a motive node in the material (5 bars: `MOTIVE_BARS` is 4 for a liquidation and 5 here):

| Idea | Length | What it is |
| --- | --- | --- |
| `basic idea` | 1 bar | the residue: m0 liquidated (`liquidate drop=<n>`, 1 to half the notes), a motive node `m1` |
| `repetition` | 1 bar | the residue one note further (`drop=<n>+1`, two notes kept at least, else drawn again), a motive node `m2` |
| `fragment 1` | 1 bar | a scale line of eighths filling the bar (4 in 2/4, 6 in 3/4 and 6/8, 8 in 4/4), a motive node `scale line`, written as it is |
| `fragment 2` | half a bar | the broken chord of V, a motive node `broken chord` |
| `cadential` | half a bar | the main theme head: m0's leading notes that fit half a bar (3 at most, 2 at least; their onsets and durations halved once when the first two do not fit), a motive node `main theme head`, written as it is, on the half cadence |

The scale line starts at degree 2 or 4 nearest the last note of `repetition`; the direction and the first degree are drawn as the liquidation's are. The head is written because a melody target is made again under the cadence's rules and would not keep m0's intervals, which are the hint of the main theme.

A retransition may open the home key from any region: the key before it is the region's, and the prolongation of V over the pedal is the arrival, so it has no pivot. It refuses a change of key at its half. The two layouts do not touch each other's draws: a liquidation draws as before in 3/4, 2/4 and 6/8, byte for byte.

Hard rules: every idea fits its span, and `repetition` has fewer notes than `basic idea`. Soft: the share of ideas that are shorter than the one before, as the liquidation's. The label is `retransition <drop> dropped, scale <down|up>`.

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
| C major 4/4 | 10 of 10 |

The 4/4 row is new (R-505). The retransition is measured in [form-parts.md](form-parts.md#the-retransition). Each composed run has the five ideas, V in the bass throughout, a last chord V or V7, a last melody note that is a tone of V, a `fragment 1` that asks for fewer notes than m0, a stepwise scale line and an ascending broken chord. With the A return `varied` the same form composes 7 of 10 in C major 2/4 whether or not the section is a liquidation (the other rows 10 of 10), so the fixture's A return is `exact`.

## Not here

A liquidation of 8 bars or with more ideas, one that opens a key and one not on a dominant pedal are Later (R-494). A retransition of 8 bars, or over another pedal, is Later (R-505). The scherzo form that uses the liquidation is B4 of P49.
