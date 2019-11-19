# semantics

Layer L6 (`music-semantics`). Path `src/semantics/`, tests `tests/semantics/`. One of four L6 siblings; see `docs/modules/l6-notes.md` for rules shared across all four.

## Provides

`Interval`, `Sonority`, `RhythmicPattern`, `Scale`, `Contour`.

## Notes

- `Interval` and `Sonority` do not depend on `PitchClass`. `PitchClass`, `PitchRealization`, and `TuningSystem` now exist (`pitch`, L2 -- see `docs/modules/pitch.md`), but neither of those two types consumes them: `Interval` computes nothing itself, since `PitchIdentity` is an opaque token with no numeric semantics; each of `Interval`'s three dimensions (directed, undirected, chromatic distance) is supplied by the caller as an independently-optional field, never defaulted to zero when unknown. Diatonic distance (the fourth TDD §67 dimension) is not represented at all; wiring it up would need `PitchClass`, which neither type has done. `Scale`, below, is the first type in this module to consume `pitch/pitch_class.h` and `pitch/tuning_system.h`.
- `Sonority` is not a `Chord`. `sonority.h` carries no chord-identification logic (root, quality, inversion); that is `Chord` (`interpretation`, L6, built on top of `Interpretation`).
- `RhythmicPattern` has no pitch fields. `rhythmic_pattern.h` takes no dependency on pitch headers; a rhythmic query never needs pitch information.
- `Scale` is a Value (no `EntityID` of its own, same as `Sonority`): an ordered collection of `PitchClass` (owned, `pitch_class_count > 0`) under a `TuningSystem`, referenced by `EntityID` rather than embedded -- the same reference-only pattern `Sonority`'s `voice_ids` already uses. `Scale` has no `FrameworkReference`: an ordered `PitchClass` collection under a `TuningSystem` does not require an interpretive claim to exist. Contrast `ScaleDegree` (`interpretation/scale_degree.h`), which references a `Scale` and does carry one.
- `Contour` is a Value: a relative pitch-change sequence (`ContourDirection`: up/down/same) over a `PitchIdentity` sequence. Directions are caller-supplied, not computed -- the same principle `Interval` already establishes, since `PitchIdentity` carries no numeric semantics this module could derive a direction from. `Contour` has no `FrameworkReference`: an observed pitch-direction sequence does not require an interpretive claim to exist.

## Deferred

Nothing at this layer -- `Scale` and `Contour` were the last two `semantics` gaps and are now built above.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `context`, `relationship`, or `interpretation`.