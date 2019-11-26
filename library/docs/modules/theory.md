# theory

Layer L7 (`music-theory`). Path `src/theory/`, tests `tests/theory/`. Sibling of `rules`; unlike L6, `rules` depends directly on `theory`, but `theory` never depends back on `rules`.

## Provides

`Framework`, `Heuristic`, `Tendency`, `StatisticalModel`, `StyleModel`.

## Notes

- `Framework` holds no reference back into `interpretation`, and carries no forward reference to its own siblings (`Rule`, `Constraint`, `Preference`, `Heuristic`, `Tendency`, `StatisticalModel`, `StyleModel`) either. It is the thing an L6+ consumer resolves a reference against, never the thing that reaches back out. Dependency Spec Open Issue §7.1 is now closed. `Interpretation` carries an optional `FrameworkReference` (`core`, L1) rather than a live reference to this module; see `docs/modules/l6-notes.md`.
- `Tendency`'s frequency is not a Rule/Constraint verdict. `TendencyResult` is a disjoint enum from `RuleResult` and `ConstraintResult`; no function converts a `Tendency` into either, and no function in `rule.h`/`constraint.h` accepts a `Tendency`. An observed frequency (TDD §99) never automatically becomes a must/must-not/valid/invalid claim.
- `StatisticalModel`'s output cannot be silently promoted to a `Rule`. `theory/statistical_model.h` does not include `rules/rule.h`, and no function anywhere converts a `StatisticalModel` into a `Rule`. `training_corpus` is a plain string label (TDD §100), never a corpus object reference.
- `Heuristic` exposes no evaluate/pass-fail API at all. `theory/heuristic.h` is identity plus a label only (TDD §98); nothing on the type could be evaluated as true or false in the first place.
- `StyleModel` is shape only, same as `Tendency` and `StatisticalModel`. `style_features` is an opaque `Value`, `training_corpus` is a plain label string, and there is no fitting or comparison logic. Construction/shape tests only.

## Theory to Corpus open issue (Dependency Spec §7.2)

The System Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` as depending on `Corpus` (L9, not yet implemented). This module does not implement that dependency; the spec is explicit that no implementation may establish it until the resolution is recorded upstream. `Tendency`, `StatisticalModel`, and `StyleModel` each accept corpus-derived statistics only as an opaque, already-computed `Value` supplied by the caller. No file under `src/theory/` or `src/rules/` includes a corpus header. `tests/theory/test_architecture_theory.c` and `tests/rules/test_architecture_rules.c` enforce this by allowlisting only L0-L6 directories.

## Deferred

Species-counterpoint frameworks (TDD Phase 10) and common-practice harmony (Roman numeral analysis, harmonic function, voice leading, cadence, tonicization, modulation, TDD Phase 11) are built as extensions on top of `Framework`/`Rule`/`Constraint` after this module, never as changes to the L7 kernel itself.

The first such extension, `common-practice-minimal-v1`, is now built at `frameworks/common-practice-minimal/` (physically outside `src/`, its own architecture-gate test at the same L0-L6-plus-`Framework`/`Rule`/`Constraint` dependency ceiling `Framework` itself carries). It covers only the seven diatonic triads and major/natural-minor keys; see that module's own `README.md` for its exact scope and exclusions. This establishes the `frameworks/<name>/` location as the pattern future extensions (jazz harmony, species counterpoint, etc.) are expected to follow.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), and all four L6 modules only.