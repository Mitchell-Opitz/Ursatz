# Motive link (analysis, L8)

Files: `src/analysis/motive_link.h/.c`, with the step text in `src/analysis/motive_relation.c` and the fingerprint in `src/analysis/motive_sketch.c`. Tests: `tests/analysis/test_motive_link.c` and `tests/analysis/test_motive_link_groups.c` (each with a valgrind run). Part of `analysis` (see `docs/modules/analysis.md`); includes `analysis/motive_variant_recognition.h` and `analysis/motive_sketch.h`.

A link claims that one motive is a chain of motive relations (`docs/modules/motive-relations.md`) from another, at most `max_depth` steps long. It is built on the variant recognizer (`docs/modules/motive-variant-recognition.md`) with every relation allowed; callers that need a link to be writable in a composer's registry check that themselves.

## API

- `motive_link_options_default(&options)`: `max_depth` 2 (1 to 6), `beam` 64 (1 to 256), `min_nearness` 0.5 (0 to 1), `contour` false.
- `motive_link_nearness(a, b)`: `motive_variant_nearness` of `motive_variant_compare(a, b)`. 0 to 1 and symmetric. Callers with many motives rank candidates by it.
- `motive_link_pair(a, b, options, &link)` returns `OK`, `INVALID` (a sketch fails `motive_sketch_validate`, an option is out of range, or `out` is NULL) or `NO_MEMORY`. `MotiveLink`: `gated`, `linked`, `reversed`, `nearness` and `recognition` (the chain found, or the a to b miss; zeroed when gated).

- `motive_link_candidates(sketches, groups, count, per_sketch, min_nearness, &pairs, &pair_count)`: for each sketch its `per_sketch` nearest others in a different group (`groups[i]`, the piece) whose nearness is at least `min_nearness`, nearest first, equal nearness by index, as unordered `MotiveLinkPair`s (`low < high`) sorted and unique. `*pairs` is owned (free), NULL for none. `INVALID` for `per_sketch` 0, `min_nearness` outside 0 to 1 or a NULL argument.
- `motive_link_groups(count, pairs, linked, pair_count, &group_of, &group_count)`: the connected components of the pairs whose `linked[p]` is true. Groups are numbered from 0 in the order of their lowest member; `group_of[i]` is `(size_t)-1` for a sketch no linked pair touches. The Analyzer ordered its groups largest first; a caller that needs that order sorts the groups itself.

## Choosing and grouping

`motive_link_candidates` and `motive_link_groups` (`analysis/motive_link.h`) choose the pairs of sketches in different pieces worth comparing (the nearest few above the gate) and the groups the linked pairs join. They take plain arrays and carry no document fields. See API above for the calls.

## The gate

A pair whose nearness is below `min_nearness` is `gated`: nothing is searched and `linked` is false. The gate exists because a miss is expensive (below). Motives that share neither rhythm, intervals nor contour sit near 0.25 (harmony is 1 when neither has chords), so the default 0.5 refuses only pairs that share almost nothing.

## Both directions

A chain is directional. The pair is recognized from a to b; when that finds nothing, from b to a. `reversed` is true when the chain found runs from b to a. A fragment of a motive is reached from the whole by one `fragment` step, but the whole is not reached from the fragment at depth 2, so a pair like (fragment, whole) is only found the second way. Replaying `recognition.steps` through `motive_relation_apply` starts from a (`reversed` false) or from b (`reversed` true).

## Fingerprints

`motive_sketch_fingerprint(sketch)` is FNV-1a 64 over a canonical byte form of every field: the notes (pitch, onset, duration), the scale (tonic and mode, only with a scale), bar, beat, pickup, length, the chords, the features (label, first, last) and the role. Every 64-bit value goes in as 8 bytes, least significant first, so the value is the same on every platform. Only the used entries of the arrays count. Equal sketches (`motive_sketch_equal`) give equal fingerprints. A cache keys a pair on the two fingerprints.

## Step text

`motive_relation_step_text(step, buffer, size)` writes the relation name, then `name=value` for each parameter, a ratio as `n:d`: `transpose tonal=1 by=-2`, `augment ratio=3:2`, `exact`. A parameter at its default (0, the invert axis -1) is left out. `motive_relation_step_parse(text, &step)` is the inverse; parameters not given take the default, and an unknown relation or parameter, a parameter given twice or a value out of form is refused. `motive_relation_param_name(kind, index)` gives the names (the header comment of `motive_relation.h` lists them).

## Measured cost

Default build (no build type), five-note fixture `C4:q E4:e G4:e F4:q E4:q`, one run each:

| Pair | Time |
|---|---|
| gated (three whole-note-pair notes, nearness below 0.5) | 0.009 ms |
| linked (transposed by 2 steps, augmented 2:1; found in 2 steps from a to b) | 39.6 ms |
| two-way miss (same rhythm, scrambled pitches, nearness 0.62; a to b and b to a at depth 2) | 326.4 ms |

A one-way depth-2 miss on the six-note fixture of the recognizer costs about 770 ms (`docs/modules/motive-variant-recognition.md`), so a two-way miss on a longer motive costs about twice that. A gated pair costs microseconds, which is why the gate and the cap on candidates per motive decide how long a run takes.
