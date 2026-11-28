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

## The chord text's mode suffix

`<numeral>:<mode>` names the mode a chord is taken from (R-762, amends R-744): `IV:dorian` is the triad on degree 4 of dorian, `II:lydian` in C major is D major, `V7:mixolydian` a seventh of the mode. It is read wherever chord text is read (a theme's `chords`, a blueprint's, a section's, a motive's own harmony).

- The numeral is upper case with no `b`, `#` or color; its figure is a triad's (none, 6, 64) or a seventh's (7, 65, 43, 42). `bIV:dorian`, `IVadd9:dorian` and `iv:dorian` are refused with "a chord with a mode is written as an upper-case numeral with no b, # or color, as IV:dorian".
- The word after the colon is looked up as a material word of the knowledge snapshot (`knowledge_snapshot_material_named`: a key word, a name or an alias, so `ionian` is the major mode) and must be one the Composer writes keys in. Any other word is refused with "'<word>' is not a mode the Composer writes; write major, minor, dorian, phrygian, lydian, mixolydian or locrian after the colon"; without a snapshot, "a chord with a mode needs the knowledge snapshot".
- `request_read_chords` takes the triad on the degree in that mode (with a seventh figure, the mode's own seventh) and refuses it when it is diminished or augmented (`the triad on IV in lydian is not a major or minor chord`), a seventh the numeral grammar does not name, or the key's own mode (`minor is the key's own mode; write the numeral without it`). Otherwise it is read as the chord of that quality, which the harmony reader borrows from the nearest mode that holds it as for every borrowed chord, so the payload's `borrowed_from` names a mode that holds it (not always the one written, when another mode nearer the key holds the same chord).
- `MotiveSketchChord.mode` holds the material; `motive_chord_numeral` writes the suffix back with the material's key word (`IV:ionian` prints `IV:major`). A theme request whose motive has its own harmony with a suffix (`I IV:dorian@2`) composes; `MOTIVE_CHORD_NUMERAL_MAX` is 24 for the longer text.

## Planed chords

`michelle.pattern.planed-chords` (R-763, K11 accompaniment with `planing` true) writes a chord on every beat (`by_meter` gives 3/4 and 2/4 three and two). The first chord of the accompaniment span is voiced by `choose_voicing` as for any pattern; each later chord takes the same members in the same order from the bottom (root position stays root position, first inversion first inversion), in close position, its lowest note the pitch of that member nearest the first chord's lowest note. A block that leaves the beat's window or meets the melody falls back to `choose_voicing`. The random stream is drawn exactly as without `planing`, so the figure's other choices do not shift.

The lowest notes are the nearest pitch to the first block's, not a step from the block before (R-771): `IV I@1 II@2 V@3` in bar 4 of a C major sentence on seed 1 gives blocks starting on 57, 52, 53 and 59, each within six semitones of the first. The test `test_accompaniment_patterns` pins this case. Planing outside the mode (exact transposition) is Later.

## Measured

Without the grid, `IV I@1 II@2 V@3` in bar 4 of a sentence stuck in the bass stage on every seed; with the grid in `harmony_read_metre` and `harmony_read_cells` it stuck in the melody stage; with `melody_idea_read` too it composes. The test `beat_chords` composes six requests (a sentence of eight bars with one bar of given chords: 4/4 on beats and on half beats, 3/4 and 6/8) on seeds 1 to 3, each on its own seed with every given chord at its onset, and pins the grid of a span for each meter. The listen gate's MIDI and MusicXML, the piece sweep and the accompaniment golden equal `main`.

Not a beat-cell problem: a period with given chords in some bars of these motifs, and a 6/8 sentence with `IV` alone in bar 4, fail the cadence rule or the draw on `main` as well.
