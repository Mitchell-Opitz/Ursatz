# The harmony lab

The Composer side of the harmony lab (P70, R-758 to R-770). Back to the [docs index](README.md). The first section is the reading grid; the later branches of P70 add the rest.

## The reading grid

Given chords may start on any beat or half beat, up to four a bar (R-761, API 4.10; the rules are in [theme-request.md](theme-request.md#chords)). The cells the Composer draws harmony on stay the accent cells (R-590: two a bar in 4/4 and 6/8, one in 2/4 and 3/4), but a span that holds a Given chord off them is read on a finer grid, so the harmony, bass and melody stages see the chord edges as cell edges.

`harmony_metre_grid` (`src/harmony/harmony_request.c`) does it. It looks at the Given harmony spans of the span being read: when a start or end strictly inside the span is not a whole number of the current cell from the start of the meter span, the grid moves one level finer, and the first level that holds every such edge wins:

| Meter | Accent cells | Beats | Divisions |
|---|---|---|---|
| 4/4 | 2 | 4 | 8 |
| 3/4 | 1 | 3 | 6 |
| 6/8 | 2 | 6 | 6 |

Other meters keep their cells. A span with no such edge keeps the accent cells, so a request without chords off the cells composes as it did. The three readers call it right after they compute the cells: `harmony_read_metre`, `harmony_read_cells` (the skeleton, schema and sequence operators) and `melody_idea_read`. `harmony_count_cells` allows eight cells a bar. The motif's own harmony (two a bar, R-195) and the chord templates of K8 keep the accent cells, and the cadence rule (every given chord in a cadence bar must allow the cadence) is unchanged.

## Measured

Without the grid, `IV I@1 II@2 V@3` in bar 4 of a sentence stuck in the bass stage on every seed; with the grid in `harmony_read_metre` and `harmony_read_cells` it stuck in the melody stage; with `melody_idea_read` too it composes. The test `beat_chords` composes six requests (a sentence of eight bars with one bar of given chords: 4/4 on beats and on half beats, 3/4 and 6/8) on seeds 1 to 3, each on its own seed with every given chord at its onset, and pins the grid of a span for each meter. The listen gate's MIDI and MusicXML, the piece sweep and the accompaniment golden equal `main`.

Not a beat-cell problem: a period with given chords in some bars of these motifs, and a 6/8 sentence with `IV` alone in bar 4, fail the cadence rule or the draw on `main` as well.
