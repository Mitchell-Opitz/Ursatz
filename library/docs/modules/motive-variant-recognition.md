# Motive variant recognition (analysis, L8)

Files: `src/analysis/motive_variant_recognition.h/.c`. Tests: `tests/analysis/test_motive_variant_recognition.c`. Part of `analysis` (see `docs/modules/analysis.md`); includes `analysis/motive_relation.h`, `analysis/motive_sketch.h` and C headers. Shared by the Composer's variant recognition and the Analyzer.

Names the chain of motive relations (`docs/modules/motive-relations.md`) that turns one `MotiveSketch` into another, with the chain's distance and the dimensions it kept and changed. The inputs are plain sketches, the notes the analysis chain already produces (R-006: the recognizer reads no audio and no score of its own).

## API

- `motive_recognize_options_default(&options)`: `max_depth` 4 (1 to 6), `beam` 64 (1 to 256), `contour` false, `admit` and `admit_context` NULL.
- `motive_variant_recognize(source, target, options, &out)` returns `OK` (the search ran; read `out.found`), `INVALID` (a sketch fails `motive_sketch_validate`, an option is out of range, or `out` is NULL) or `NO_MEMORY` (the beam, about 4 KB a state, could not be allocated).
- `MotiveRecognition`: `found`, `steps` and `step_count` (a `MotiveRelationStep` chain), `distance`, and per dimension `kept`, `changed`, `similarity`.
- `motive_variant_compare(a, b, &dimensions)` and `motive_variant_nearness(&dimensions)` expose the per-dimension comparison on its own.

Equal sketches give one `exact` step and distance 0. Both sketches are compared in the settled form every relation's result has (its length is the end of its last note), so a replay of `steps` through `motive_relation_apply` equals the settled target.

## Distance

`cost / (cost + 4)`, an exact `ExactRational`, where cost is the sum of the chain's relation costs (R-142: exact 0; transpose, augment, diminish 1; change-meter 3; every other relation 2). A chain of two relations costing 1 and 2 is 3/7. No chain within the depth is distance 1.

## Dimensions (R-134)

`similarity` is 0 to 1, `kept` is true when it is 1 and `changed` when it is not.

| Dimension | Similarity |
|---|---|
| rhythm | the share of neighbouring note pairs whose inter-onset ratio and duration ratio match; kept means onsets and durations scale by one common factor |
| intervals | the share of steps that match: scale steps by each sketch's own scale, semitones without one |
| contour | the share of directions that match |
| harmony | the share of chords, by position, with the same degree, alteration, inversion and seventh (onsets are not compared) |

Shares are over the longer sketch. Rhythm, intervals and contour compare at the shift (up to 3 notes either way) that matches most, so a fragment, an added upbeat or an omitted note still shows what it kept; only the unshifted comparison can match every pair, so `kept` is exact. A dimension neither sketch has (pitches in a rhythm sketch, chords) is 1; one sketch having it and the other not is 0. `out.kept`, `changed` and `similarity` always compare the target with the source, found or not. The meter is not a dimension: a change of meter scales every time, so it keeps rhythm.

A harmony relation adds the best implied chord at 0 to a sketch without chords (`docs/modules/motive-relations.md`), so a caller that compares harmony gives both sketches their chords first.

## Search

Breadth first by depth up to `max_depth`.

1. At each state every relation but `exact` gives its proposals: at most 64 parameter sets. A relation that needs pitches (or a scale, for the harmony relations) gives none for a rhythm sketch (or one without a scale), so a rhythm sketch takes only the relations that touch time and a sketch without a scale takes no tonal parameters (`transpose` and `invert` stay in semitones).
2. Proposals are inferred from the state and the target first: transpose from the first and last note's difference in semitones and in steps and the target's first degree; invert from the axis that maps first to first; interval-change from the single differing interval or the common difference; fragment and liquidate from the target's note count; augment and diminish from the target's length over the state's; rhythm-shift from the pickup; change-meter from the target's meter; add-interval from the first note that differs; condense from the length saved; repeat-feature and shift-feature from each feature; the harmony relations from the target's chords. Add-interval also tries, for a target with one note more, every interval whose note the target orders like the state without it, with every steps from -7 to 7 (an added lowest or highest note of a contour form shifts every rank, so the first difference names only interval 1). Repeat-feature tries every feature with times 1 to 2 and steps -7 to 7. Then the usual values of a small range follow (the first and the longest fragments, one to three notes liquidated, a step or two of transposition, each interval or chord in turn), because a state in the middle of a chain is not near enough to the target for the target to show the parameters.
3. Every proposal is applied. A refused result is dropped. A result equal to the target is a solution at this depth: of the solutions of one depth the lowest cost wins, then the first in relation order (the kinds compared along the chain). The search ends at the depth of the first solution.
4. Other results become the next depth's states: a state equal to one already kept at any depth is dropped (the cheaper chain stays), and of the rest the best `beam` are kept (except at depth 1, below) by nearness to the target (the mean of the four similarities; ties go to the earlier relation). The nearest state each relation reached at this depth is always kept when the beam is at least 56, because a relation the nearness cannot see (a change of meter) would otherwise never survive the beam. Deterministic.

When no chain is found, `found` is false, `distance` is 1 and `steps` hold the chain to the closest state any depth reached (`step_count` 0 when the source was the closest).

## Contour mode (R-148)

With `options.contour` the source and the target are first normalized to their contour (`motive_sketch_contour_normalize`: dense ranks over the distinct pitches, each note a quarter), and every state is normalized right after its relation applies; a state whose normalization fails is dropped. `found` then means replaying the steps with `motive_relation_apply` and a normalization after each step gives the target. Without it a target the normalization moved (an inversion or an added interval of a contour form) is out of reach of exact pitches. A sketch with unknown pitches is `INVALID` in contour mode.

## Depth 2 is exhaustive (R-149)

The states one relation from the source are never cut by the beam: level 1 holds every distinct candidate (relations x 64, the proposer cap). The beam cuts from level 2 on. So every chain of two relations whose parameters the proposers give is found at any beam, even 1. A depth-2 miss costs about 770 ms, a depth-3 miss about 1.0 s and a depth-4 miss about 1.3 s (default build, six-note fixture; the cap of 64 makes level 1 four times larger than it was at 16).

## Proposed parameters

Ratios are proposed inside the range a composer writes (terms 1 to 3), inferred from the target where it can be, plus the usual ratios (2:1, 3:2, 3:1 and their inverses for augment and diminish), because a later step can hide the factor in the length. Condense infers its run from the notes whose durations changed, and tries single notes first.

## Limits

- The search is a beam search guided by nearness, not exhaustive. A chain whose first steps look no nearer to the target than the beam's other states can be missed, then a longer or costlier chain, or none, is returned. In the seeded test (the six-note fixture, every relation but exact and counter-line) every chain of two relations is found and about four in five of three or four; a larger `beam` finds more and costs more.
- A target is reachable only through the relations: a feature, role or chord that no relation produces is not found.
- Notes: at most 64, chords 8, features 8 (the sketch's limits). Depth at most 6.
- Parameters are proposed only from the list in step 2, so a chain that needs another value (a note repeated four times in the middle of a long sketch beyond the first 64 proposals) can be missed.

### Admit filter

`options.admit(step, context)` (R-152) is called for every proposal at every depth before it is applied; a step it refuses is skipped, so `found` means a chain of admitted steps. A caller that replays chains through its own operation table passes the steps that table can take. Without it the search ends at the first chain equal in library terms, which may hold a step the table cannot replay (a chromatic transpose outside its range, a rhythm-shift ratio outside its terms, liquidate or interval-change on a contour form), and the chain of the same depth the table can replay never comes back. NULL admits every step.


## Cost

Measured in the default build (no optimization) on the six-note fixture of the tests: one relation about 1 ms, a chain of three relations about 180 ms, a pair with no chain (a full depth-4 search) about 500 ms. Time grows with the beam and with the number of notes. The Composer's round trip (2043 variants on five fixtures, with an admit function) takes about 17 s in the default build and recovers all of them.
