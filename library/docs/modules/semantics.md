# semantics

Layer L6 (`music-semantics`). Path `src/semantics/`, tests `tests/semantics/`. One of four L6 siblings; see `docs/modules/l6-notes.md` for rules shared across all four.

## Provides

`Interval`, `Sonority`, `RhythmicPattern`.

## Notes

- `Interval` and `Sonority` do not depend on `PitchClass`. `PitchClass`, `PitchRealization`, and `TuningSystem` are Phase 3 Deliverable D and are not built yet. `Interval` computes nothing itself, since `PitchIdentity` is an opaque token with no numeric semantics; each of `Interval`'s three dimensions (directed, undirected, chromatic distance) is supplied by the caller as an independently-optional field, never defaulted to zero when unknown. Diatonic distance (the fourth TDD §67 dimension) is not represented at all, since it needs `PitchClass`.
- `Sonority` is not a `Chord`. `sonority.h` carries no chord-identification logic (root, quality, inversion); that is `Chord` (Phase 8, depends on `Framework`, deferred).
- `RhythmicPattern` has no pitch fields. `rhythmic_pattern.h` takes no dependency on pitch headers; a rhythmic query never needs pitch information.

## Deferred

`Scale` (Registry non-normative, reference concept only), `Contour` (Registry non-normative, descriptive examples only), `PitchClass`/`PitchRealization`/`TuningSystem` (Phase 3 Deliverable D, not yet spec'd).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `context`, `relationship`, or `interpretation`.
