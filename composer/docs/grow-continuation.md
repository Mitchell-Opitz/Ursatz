# An accelerating continuation

Six settings that make a sentence's continuation speed up instead of restating the presentation, and let a motif stand as it is (R-452, R-462 to R-466; Michelle's listening requests 13 to 16). Back to the [docs index](README.md); the fragments and the liquidation they change are in [grow-segments.md](grow-segments.md), the weights they leave alone in [grow-development.md](grow-development.md).

Off by default (R-422). Five are members of `GrowContinuation` (`grow.h`), carried in `GrowContext.continuation` and `GrowSetup.continuation` (NULL for the defaults) and read by `grow_ending.c` when any is set (`continuation_on`); `basic_as_written` is `GrowContext.basic_as_written` and `GrowSetup.basic_as_written`. A piece's theme sections take none (R-411). They are set from C only until the preferences document carries them (L3 B6), so no API output moved. With none set, or with `fragment_lengths` at {1, 0, 0}, every composed output is byte-identical (R-413) and no extra random number is drawn.

## The settings

| Member | What it does |
|---|---|
| `every_segment` | Fragment 1 is any segment that fits a bar (weight 1 each, not the development's weights). Fragment 2 is the first segment after it, round to the first, and fragment 1's own segment last, that fits: never the same segment unless it is the only one. Fragment 2 is exact, not the sequence. |
| `fragment_lengths` | The weights of a fragment being its whole segment, half a bar of it, or a beat of it. All 0 reads as {1, 0, 0}. |
| `diminish` | A fragment that is a whole segment is that segment in diminution 1:2 (a `diminish` step), half its length. |
| `liquidate` | Fragment 2 is a liquidation by segments (a residue that keeps a segment's rhythm and head interval), and the cadential idea liquidates at least as far, from fragment 2's step count. |
| `faster_harmony` | An 8-bar sentence's continuation has a chord a cell: one target an idea, each of at most one cell and none across a barline. Fragment 1 and fragment 2 prolong the tonic to the dominant (two cells: I, then V, V6, V65 or V43; the motive's tonic is held on cell 0 and its chord on cell 1 is freed by the anchor, R-344). The cadential idea is the cadential progression with the cadence (a predominant, V, then I on the last bar). |
| `basic_as_written` (`GrowContext`) | A motif as long as the basic idea, with chords of its own, is the basic idea as it is, over its own chords. |

A fragment cut to half a bar or a beat is the segment's leading notes that start before that point, at least 2 and fewer than all, written as one `fragment` leading step on the segment. A beat is `1/beat_unit`, or the dotted beat (`3/beat_unit`) in 6/8 and 12/8. A ref holds one step, so a cut or diminished fragment 2 is the next segment (or the same one), never the sequence one step down. With `every_segment` and `liquidate` together, fragment 1 is a segment other than the one fragment 2's residue keeps. A fragment shorter than its idea is grown as a shorter motif is ([grow.md](grow.md#how-a-motif-fills-its-idea)), so a half-bar cut fills its bar with its rhythm twice.

## Faster harmony and the basic idea as written

`faster_harmony` applies to a continuation with no key change in an 8-bar sentence whose bar has two cells or more (4/4 and 6/8). In 2/4 and 3/4 a bar is one cell, so the setting changes nothing and the plan equals the plan without it (R-465). The presentation's chords are drawn as before; they can differ from the plan without the setting because the chord search is judged over the whole theme.

`basic_as_written` is a setting because it is not safe for every request. Without it the basic idea is a prolongation target anchored to the tonic (R-344) and the melody stage bends the motif to the chord found (R-328), so a motif with chords of its own is not heard as written; with a sequence repetition the guard in `build_presentation` reads the source motif's chords, which are not always the compose run's (R-466, the R-348 sequence-guard bug, recorded and not fixed because fixing it moves default outputs). With the setting, and a basic motive with chords as long as the basic idea (not a part-length motif, a section motive, a given first half or a pedal), the basic idea's melody is one Realized span of m0's notes (`grow_add_melody_written`), so the melody stage leaves it and the harmony, bass and fidelity read it (the basic idea measures exact). Its harmony target ends on any chord (the motif's last chord stands), and the sequence guard reads the compose run's basic motive (`basic_has_chords`). The repetition is unchanged. A motif with no chords is not changed by the setting.

## Fallbacks

- A beat with under 2 notes falls back to half a bar, then to the whole segment.
- A cut or a diminution the library refuses, or that does not fit its idea, is the whole segment, undiminished. The library refuses the 1:2 diminution of a one-bar segment in 3/4 (a beat and a half), so in 3/4 `diminish` keeps the whole segment.
- With no segment that fits, or with a continuation-slot motive given, the fragments are the leading notes of m0 as before.
- A liquidation with no residue that keeps a segment fails the draw, as the cadential idea's does (R-345).

## Fidelity

A fragment by notes departs from its source whether the source is m0 or a segment (`grow_ref_departs`, R-464): a cut is ranked by fidelity and never refused, and its `minimum` in the Development list is 0, as a liquidation's. No default plan holds a `fragment` ref with a segment, so no default output moved; a hand-written plan with one is now ranked, not refused. `judge_continuation` (`grow_check.c`) measures a liquidated fragment against m0, as it measures a segment.

## What was measured

`tests/test_grow_continuation.c` (two motifs, `tests/fixtures/continuation/s77.json` in C major and `u51.json` in C minor, each two bars of 4/4; 8-bar sentences, seeds 1 to 8):

| Setting | Result |
|---|---|
| none, or `fragment_lengths` {1, 0, 0} | plan JSON equal to a context with no continuation set |
| `every_segment` | fragment 1 and fragment 2 name different segments in 8 of 8 seeds on both; without it both name the same segment in 8 of 8 |
| `fragment_lengths` {0, 1, 0} | every fragment is a leading cut of 2 notes or more and fewer than its segment, ending by half a bar |
| `fragment_lengths` {0, 0, 1} | s77's segment b cuts to its first two eighths (a quarter); every other segment falls back to half a bar |
| `diminish` | every fragment is `diminish` and half a bar long; melody notes a bar in bars 5 and 6 against bars 1 and 2: 9.00 against 4.50 (s77), 6.00 against 3.00 (u51) |
| `liquidate` | fragment 2 has 1 step or more and names a segment; the cadential idea's steps are at least fragment 2's |
| `faster_harmony` | a chord onset on beats 1 and 3 of bars 5, 6 and 7 and no two neighbors among those six the same chord, and more chord changes in the continuation than in the presentation, in 8 of 8 seeds on both; in 3/4 (`rates/two-bar-3-4.json`, A minor) the plan equals the plan without it |
| `basic_as_written` | the basic idea's melody notes equal m0's (offsets, durations, pitches) in 16 of 16 seeds on both, against 0 of 16 without (`s77-chords.json` and `u51-chords.json`, the same motifs with `"harmony"` of `I V@4 I@7` and `I IV@4 V@6`); a motif with no chords gives plans equal to the setting off |
| each alone and all together | 16 of 16 start seeds compose through `api_grow_run` on both motifs |

The prototype of R-463 measured the same on the two listening motifs s77 and u51: every setting composed 16 of 16, `every_segment` used segment b in 16 of 16 (9 and 10 without), and `diminish` took s77's continuation from 4.67 to 8.00 notes a bar. On `listen --what themes --count 60 --seed 500` with every setting on, the taste profile `none` batch in C minor 4/4 composed 47 of 60 against 54 at the defaults and every other batch was equal (R-467): recorded, not asserted.
