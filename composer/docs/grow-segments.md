# Segments in a theme

How a continuation and a cadential ending develop the motif through its segments (ch. VIII, P34). Back to the [docs index](README.md); the choices a theme draws are in [grow.md](grow.md#choices).

## The segments of a run

`GrowThemeSpec.segments` are the segments of the motive `m0` for one run: its `features` when the compose run's basic motive
(`GrowContext.basic`, set from `GrowConfig.basic`, whose notes must be `m0`'s) has any, else the proposal of
`motive_segments_propose` (rest, long note, bar line on beat 1; labels `a`, `a'`, `b`). A motif as long as the presentation uses
the segments that start inside its basic idea; a section's own motive (a `MOTIVE_REF` on the section) has proposals. Nothing is
written back to the motif. A segment is a motive of its own by `grow_segment_motive` (`src/grow/grow_segments.c`): its notes from
time 0, with the motif's meter and key, the base harmony chords that sound under it and the tone roles of its notes. A segment of
one note, and a segment that is the whole motif, is not a fragment.

## Fragments

Fragment 1 is a segment that fits its idea, drawn by `FRAGMENT_SEGMENT_WEIGHTS` (`src/grow/grow_ending.c`; balanced: a letter
other than `a` 2, `a` 1; the [development setting](grow-development.md) changes them). Fragment 2 is fragment 1's segment in sequence one step down in the key (`transpose tonal=1 by=-1`, weight 3) or the
next segment (`exact`, weight 1), the other when one does not fit (`FRAGMENT_2_WEIGHTS`). Each segment is a Realized motive node
`segment <label>` in the material, after the idea motives, laid out by its own bars; the fragment's motive node is made from it,
and a fragment shorter than its idea is grown as a shorter motif is ([grow.md](grow.md#how-a-motif-fills-its-idea)). The plan
records the segment on the fragment's motive node: `"segment": "b"`. With a motive of the continuation slot given, or when no
segment fits, the fragments are the leading notes of `m0`, with no segment.

Four settings, off by default, draw the fragments from every segment, cut them to half a bar or a beat, or diminish them: [grow-continuation.md](grow-continuation.md).

A presentation in sequence writes I and ii whole when the motif has no harmony of its own, and takes the repetition's chord from its motive when it has (R-348).

The motives stage writes a fragment as the step its ref names, through one function (`grow_develop_motive`, see [grow-development.md](grow-development.md#what-sounds)): a segment is `exact`, fragment 2 in sequence is the tonal `transpose` with its chords moved a step down, and the leading notes are a `fragment`. A fragment is no longer chosen among the liquidations by features the registry offers; the halving of the units is the liquidation below.

## Liquidation

With the `liquidate` continuation setting fragment 2 is also a liquidation, and the cadential idea goes a step further: [grow-continuation.md](grow-continuation.md).

The cadential idea of a continuation, and both ideas of a cadential ending, liquidate the segments: `liquidate-features` with
`steps 1` and `shorten 1`, applied to `m0` with its segments as its features, one more step each draw (at most as many as the
motif has segments) until the result has fewer notes than `m0`, fits its idea, stays in range and in the key, has no leap wider
than a fifth (or `m0`'s), its last note can take the cadence (moved to the nearest tone of the cadence, the rhythm kept, P33 B1),
and the residue keeps a segment (R-345, `residue_keeps` in `src/grow/grow_ending.c`): its first two notes have the durations of
a segment's first two, one to one or both halved (`shorten`), and its first interval is that segment's in scale steps, same
direction and size. The two ideas of a cadential ending take at least as many steps, the second as far as the first.

When no step count keeps a segment, the draw is widened (`around_head`): each segment in turn keeps its head (the whole
segment, then one note fewer, down to two) and the rest of `m0` is liquidated around it, every step count. When that finds
nothing either, as for a one-segment one-bar motif or the one-bar source of a 4-bar theme, where the rest after the head is
one note or a span the library cannot write as one note, the idea is a head residue (R-350, `head_residue`): a segment's first
two notes (whole, then halved) and a close on `m0`'s last pitch moved to the cadence's tone (else in the octave nearest the
head's second note), lasting `m0`'s last duration or else the head's second, the first that fits. No draw: the first accepted
wins. The label says `head of <segment>`. A residue keeps to the key's scale when `m0` does; a chromatic `m0`'s keeps to the
range. A motif whose segments are each one note has no residue, so no continuation or cadential ending is made from it.

The result is a Realized motive node `liquidated <n>` (the run's own: a regenerated section removes it with the rest it made,
and the material has room for ten bars per section, `GROW_MADE_NODES`) the idea restates exactly, and the plan records on the
idea's motive node the segment the residue keeps and the steps: `"segment": "a", "steps": 2` (a head residue has as many steps
as `m0` has segments). That node is the only source of a theme's cadential idea: the motive-forms restate it, and the P13
`motive-fragment` operator is not reached for it (it stays registered for pieces).

A draw whose residue cannot keep a segment fails, and the structure stage tries its next seed. No idea is liquidated by notes
any more: `liquidate_drop`, `cadential_drop` and the `liquidate` steps of R-321's fallback are gone from themes (R-330, R-345).

## In the result

Each item of the theme result's `developments` has the `segment` it is made from (or `null`) and a one-line `summary`
([explain.md](explain.md#the-summary-line-of-a-developed-idea)). The plan's two members are in
[plan-format.md](plan-format.md#motive-refs-the-segment-and-the-steps).
