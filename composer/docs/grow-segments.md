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
next segment (`exact`, weight 1), the other when one does not fit (`FRAGMENT_2_WEIGHTS`). Each segment is a Given motive node
`segment <label>` in the material, after the idea motives, laid out by its own bars; the fragment's motive node is made from it,
and a fragment shorter than its idea is grown as a shorter motif is ([grow.md](grow.md#how-a-motif-fills-its-idea)). The plan
records the segment on the fragment's motive node: `"segment": "b"`. With a motive of the continuation slot given, or when no
segment fits, the fragments are the leading notes of `m0`, with no segment.

## Liquidation

The cadential idea of a continuation, and both ideas of a cadential ending, liquidate the segments: `liquidate-features` with
`steps 1` and `shorten 1`, applied to `m0` with its segments as its features, one more step each draw (at most as many as the
motif has segments) until the result has fewer notes than `m0`, fits its idea, stays in range and in the key, has no leap wider
than a fifth (or `m0`'s) and its last note can take the cadence: the idea that ends the theme has that note moved to the nearest
tone of the cadence, the rhythm kept (P33 B1). The two ideas of a cadential ending take at least as many steps, the second as
far as the first. The result is a Given motive node `liquidated <n>` the idea restates exactly, and the plan records the number
of steps on the idea's motive node: `"steps": 2`. When no number of steps meets that, the idea is `liquidated` by notes as before
(`liquidate_drop`, `cadential_drop`), with no steps.

## In the result

Each item of the theme result's `developments` has the `segment` it is made from (or `null`) and a one-line `summary`
([explain.md](explain.md#the-summary-line-of-a-developed-idea)). The plan's two members are in
[plan-format.md](plan-format.md#motive-refs-the-segment-and-the-steps).
