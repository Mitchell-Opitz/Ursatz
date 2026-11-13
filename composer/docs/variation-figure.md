# The motive of variation

A variation that keeps the theme's course and gives each variation a figure of its own (R-497, R-498). Back to the [docs index](README.md); the form side is in [forms.md](forms.md), the constraint in [plan-constraints.md](plan-constraints.md), and the return it builds on in [piece.md](piece.md).

## The return

A section with a `return` of variation `figurate` and a `figure` constraint is a return of the theme (R-070): the copy of its phrases, ideas, harmony, bass and inner voices, so the course of events, the number and order of segments and their proportions are the theme's. form-return makes the copy and keeps the melody as it is (when the figure's harmony is `reharmonized` it reopens the harmony as a reharmonization does, and the melody must fit the chords the harmony operators write). compose_run then writes the figure, after the harmony is final (`form_figure_write`, `src/piece/form_figure.c`). A figurate section whose keys differ from the theme's is not a copy: it is grown afresh.

## The skeleton

One principal tone per cell, a tone of the chord over the cell (`form_figure_skeleton`). The cell is a beat: a quarter in 2/4, 3/4 and 4/4, a dotted quarter in 6/8. (The harmony operators' cell is longer in 2/4, 3/4 and 4/4: a chord covers whole cells of the figure.) The theme's melody is read moved by the key difference, and per cell the tone is

- the note sounding at the cell's start, when it is a tone of the chord over the cell;
- else the first note that starts inside the cell and is a chord tone;
- else the chord tone nearest the note sounding at the cell's start (the lower of two equally near).

Ornaments, passing tones and suspensions fall away, and the rhythm becomes regular.

## The figures

Each cell not kept holds N equal notes, the first the skeleton tone: N is the cell over an eighth or a sixteenth (2 or 4 in 2/4, 3/4 and 4/4; 3 or 6 in 6/8). The kind is the figure's:

| Kind | The cell |
| --- | --- |
| `neighbor` | the odd positions are the tone's neighbor in the key's scale, above or below; the first side is drawn once for the section and the sides alternate from one cell to the next. A neighbor more than two semitones away (the augmented second) takes the other side. |
| `scale` | stepwise in the scale from the tone toward the next cell's first note (the next skeleton tone, or the first note of a kept cell), turning back once it is reached; toward the same tone, up or down as drawn once for the section, half of the notes away and the rest back. |
| `broken-chord` | the tones of the chord over the cell from the tone, up or down as drawn once for the section, past the octave as needed; a tone outside the range turns the line back. |

The scale is the key's major or natural minor; in minor, over a chord that holds the leading tone (V, vii), the harmonic minor. A cell with a note outside the melody range is made again the other way (the other side, the other direction); if that is outside too, the write fails and the returns are made again from the plan as it was, with other seeds, up to 12 times.

## The kept cadence bars

The last bar of each half (the last bar of the section and the last bar of its first half) keeps the theme's notes: the figure gives way at the cadences. A note that sounds into a kept cell from a cell before it starts again at the cell, and a note ends where the next cell that is not kept begins.

## The reharmonized figure and the root change

`"harmony": "reharmonized"` puts the figure over a copy whose chords the harmony operators rewrite, as `reharmonize` does. `change_at` asks that a chord that starts inside that bar (from 1) is not a chord of the theme at the same place (`form_return_bar_changed`, Brahms's third bar); in the first half of the attempts a run is made again until it is so, then the requirement is dropped. A reharmonized return needs at least one chord that differs, in the same half of the attempts.

## The climax

With `climax` the whole melody of the section, kept cells too, is moved an octave up when every note stays inside the range, else left as it is.

## The check

`form_figure_check` holds a section to the above: every cell not kept holds N equal notes from the skeleton tone (an octave up for a raised climax) and keeps to its kind (neighbor: even positions the tone, odd ones a step of 1 or 2 semitones; scale: every interval a step of 1 or 2 semitones, 3 for the augmented second of the harmonic minor; broken chord: every note a tone of the chord over the cell), and every kept cell holds the theme's notes, moved.

## The hearing

The return's relation is heard on the skeletons: for a section with a figure and the section it returns, both melodies are read reduced to one note per cell (the note sounding at the cell's start, lasting the cell), and the matcher hears that pair on them, so what is heard is the skeleton's course (`return_hear`). Every other pair is read as before. Figurate returns are composed, not heard: the count of those the matcher finds is recorded below and not asserted (R-408).

## Measured

`tests/test_form_figure.c` composes [figure-form.json](../tests/fixtures/forms/figure-form.json) (a theme and three variations: neighbor tones in eighths; a scale in sixteenths over a reharmonized copy with the root changed at bar 3; a broken chord in sixteenths, the climax; 32 bars), invented motive, seeds 1 to 10, at P50 B2 (22 s in all).

| Key, meter | Composed | V3 raised | V2 with bar 3 changed | Figurate returns heard |
| --- | --- | --- | --- | --- |
| C major 2/4 | 10 of 10 | 6 | 0 | 30 of 30 |
| A minor 2/4 | 10 of 10 | 3 | 1 | 27 of 30 |
| C major 3/4 | 10 of 10 | 3 | 0 | 30 of 30 |
| C major 4/4 | 10 of 10 | 1 | 0 | 30 of 30 |
| C major 6/8 | 10 of 10 | 3 | 0 | 30 of 30 |

In each composed plan every figure keeps to its kind, every phrase and idea of a variation has the theme's name and length at the same offset, V1's chords are the theme's, V2's melody fits its chords, every cell start holds a tone of its chord, and the plan round-trips through plan JSON. V3 is raised when its figure and the theme's melody leave room in the range. The root change at bar 3 is asked for in the first half of the attempts and rarely survives to the plan: it is recorded, not asserted.
