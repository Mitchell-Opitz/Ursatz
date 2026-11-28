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

## The palette

`composer_api_palette` (R-765) answers `{"key": "C minor", "with": "dorian"}` with the triads of the key's material and of `with` on the same tonic: `materials` (each `{material, word, chords}`, a chord `{degree, numeral, name, quality, text, shared, color}`), `shared` (the numerals of the key's material that both hold) and `color` (for each material, the numerals of its chords that carry a pitch class the other lacks). The numeral is the Open Music Theory spelling against the major scale of the tonic (i, bIII, vio, #ivo); the name is spelled from the tonic's letter (Cm, Eb, Adim). The library side is `modal_palette` (`library/theory/tonality`): a triad is shared when the other material holds a triad of the same root and quality.

`text` is the chord text the Composer reads for the chord (`composer_chord_text_of`): the first of the upper-case numeral (against the key's own degrees, `III`), the numeral that names the quality (`v`, `iv`, `iio`) and the numeral with its mode, `IV:dorian`, that `request_read_chords` reads in the key as exactly that chord. In C minor with dorian the shared chords are `I`, `III`, `v` and `VII`, the F major chord `IV:dorian`; in C major with lydian D major is `II:lydian`. A chord none of the texts reads has no `text`. The schemas are `palette.request.schema.json` and `palette.response.schema.json`; the workshop's `knowledge.palette` ([knowledge.md](../../workshop/docs/knowledge.md)) answers the same document.

## The progression request

A progression request with `palette` (R-766; the CLI's `--palette <file>`) writes the chords itself: a plan of Given chords, a bass, and with `accompaniment` the chords sounded with no melody.

- `key`, `meter`, `bars` (1 to 64), `seed`, `suggest` (1 to 8) and the outputs as before; `cadence` is `none`, `PAC` or `HC`; `schema`, `sequence` and `embellish` are refused ("a palette writes its own chords; leave out schema, sequence and embellish"). `accompaniment` is `{pattern, voicing?}` as a theme's (a pattern name or a K11 accompaniment id, a K3 voicing id for a block).
- `palette` is closed: `with` (a mode on the key's tonic, not the key's own), `chords_per_bar` (1, or a meter's 2, or its beats: 4/4 1, 2, 4; 3/4 1, 3; 2/4 1, 2; 6/8 1, 2), `spans` (up to 16 `{bars: [first, last], chords: shared|color|first|second|any, schema, loop}`, bars from 1, none overlapping; `shared`, `color` and `second` need `with`; `schema` is a K5 progression id, `loop` defaults to the schema's own and a schema that does not loop must fit its span), `chords` (`[{bar, text}]`, the chord text of a theme's chords, kept as written, starting on a unit) and `brightness` and `tension` (`{shape: rise|fall|hold|peak|step, at}`, `at` from 1 and required for `peak` and `step`). A refusal names its path (`/palette/spans/0/chords`).

### The operator

`harmony_palette_draw` (`composer/src/harmony/harmony_palette.c`) gives one chord for each unit. The pools are the triads of the two modes (`modal_palette`) without diminished and augmented chords and without the mode's avoided degrees: `first`, `second`, `shared` (both hold), `color` (the second's chords with a tone the first lacks) and `any`. Given chords stand; a schema's stages are placed from a rotation drawn among its `rotations`, each stage its units, repeated with `loop` and cut at the span's end. 64 candidates are drawn from a stream seeded with the request's seed plus 13 plus 7919 times the suggestion, each open unit uniform from its pool and redrawn up to 16 times when its root is a tritone from the one before; the cadence fixes the last chords (a PAC the tonic after the approach chord, an HC the approach chord).

The cost of a candidate, the least wins and the earlier on a tie: 10 for each pair of neighbouring bars against a shape (`rise` counts the drops, `fall` the rises, `hold` the changes, `peak` the drops up to `at` and the rises after it; `step` counts every pair of a bar before `at` and a bar at or after it whose earlier value is not below the later), the bar's value being the mean over its units of the chord's brightness (over the tonic, no mode) or tension (Hindemith's rank, [tension.md](../../library/docs/modules/tension.md)) as root-position tones from C4; 2 for each pair of equal neighbouring open units; plus the Series 2 rank of each neighbouring root interval. Suggestion k is the least-cost candidate of its own draw that differs from the suggestions before it.

### The response

Each result has `heard` (null with cadence `none`), `chords` (a row for each chord), `measures` (`[{bar, tension, brightness}]`, rational strings, measured on the written bass and inner-voice notes) and `harmony` (`{key, material, with?, meter, bars, chords: [{bar, text}], stages: [{chord, units}]}`: the texts as a theme's `chords` reads them, neighbouring equal units of a bar joined, and the stages as OMT numerals with neighbouring equal units joined). `list_schemas` adds `progression_schemas` (`{id, name, loop, materials}`). The bass stage runs as for any progression; a chord that borrows is not re-checked by `harmony_check_bass`, which reads the chords without the snapshot.

A given chord in a modal key reads as the material's stacked triad: `IV` in C dorian is F major, `VII` in C mixolydian B flat major.

## Measured

Without the grid, `IV I@1 II@2 V@3` in bar 4 of a sentence stuck in the bass stage on every seed; with the grid in `harmony_read_metre` and `harmony_read_cells` it stuck in the melody stage; with `melody_idea_read` too it composes. The test `beat_chords` composes six requests (a sentence of eight bars with one bar of given chords: 4/4 on beats and on half beats, 3/4 and 6/8) on seeds 1 to 3, each on its own seed with every given chord at its onset, and pins the grid of a span for each meter. The listen gate's MIDI and MusicXML, the piece sweep and the accompaniment golden equal `main`.

Not a beat-cell problem: a period with given chords in some bars of these motifs, and a 6/8 sentence with `IV` alone in bar 4, fail the cadence rule or the draw on `main` as well.

### The sweep

`test_harmony_lab_sweep` (slow; its smoke test runs seed 1 of every case) runs the lab's promises over seeds 1 to 10 and pins what it measured, 2026-11-28:

| Case | Measured |
|---|---|
| her plan (`fixtures/harmony/sad-dorian-lift.json`) | composes on 10 of 10; bars 1 to 12 are among i, III, v and VII and bar 13 holds A natural on all 10; bar 13 is brighter than every bar before it on 1 (it is usually `ii`, as bright as the `v` of bar 8) |
| each of the fifteen OMT VII schemas, looped over 8 bars (a schema that does not loop: its own length, the rest `any`) | 150 of 150 write their stages in order |
| C major, 8 bars, tension `rise` | composes on 10 of 10, the bar tension falling at most once |
| the six sentences of R-761 | 60 of 60 compose with every given chord in the plan at its onset |
| a doo-wop plan (`IV V I VI IV V V I`, PAC) less bar 2, given to a sentence | 10 of 10 compose with every given chord at its onset |

The doo-wop plan is the lab's, so the rotation is the one it drew; a sentence refuses a given `V` in bar 1 or a `VI` in bar 5 against its PAC, which is why bar 2 is the one left out. The sweep takes 92 s on 3 cores, the sentences of R-761 most of it.
