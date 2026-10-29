# theory

Layer L7 (`music-theory`). Path `src/theory/`, tests `tests/theory/`. Sibling of `rules`; unlike L6, `rules` depends directly on `theory`, but `theory` never depends back on `rules`.

## Provides

`Framework`, `Heuristic`, `Tendency`, `StatisticalModel`, `StyleModel`.

## Notes

- `Framework` holds no reference back into `interpretation`, and carries no forward reference to its own siblings (`Rule`, `Constraint`, `Preference`, `Heuristic`, `Tendency`, `StatisticalModel`, `StyleModel`) either. It is the thing an L6+ consumer resolves a reference against, never the thing that reaches back out. Dependency Spec Open Issue §7.1 is now closed. `Interpretation` carries an optional `FrameworkReference` (`core`, L1) rather than a live reference to this module; see `docs/modules/l6-notes.md`.
- `Tendency`'s frequency is not a Rule/Constraint verdict. `TendencyResult` is a disjoint enum from `RuleResult` and `ConstraintResult`; no function converts a `Tendency` into either, and no function in `rule.h`/`constraint.h` accepts a `Tendency`. An observed frequency (TDD §99) never automatically becomes a must/must-not/valid/invalid claim.
- `StatisticalModel`'s output cannot be silently promoted to a `Rule`. `theory/statistical_model.h` does not include `rules/rule.h`, and no function anywhere converts a `StatisticalModel` into a `Rule`. `training_corpus` is a plain string label (TDD §100), never a corpus object reference.
- `Heuristic` exposes no evaluate/pass-fail API at all. `theory/heuristic.h` is identity plus a label only (TDD §98); nothing on the type could be evaluated as true or false in the first place.
- `StyleModel` keeps `style_features` as an opaque `Value` and `training_corpus` as a plain label string; its shape is unchanged. Corpus style statistics fill it (below). There is still no fitting: every number is a count or an exact rational.
- **Framework singletons (`theory/framework_singleton.h`).** Each framework accessor (`standard_form_framework()` and its 16 siblings) returns `framework_singleton_get(&g_singleton)` with a static `FrameworkSingleton` declared `FRAMEWORK_SINGLETON_INIT("<framework>-v1")`. The first call from any thread creates the `Framework` (the string is both its entity id and its name) under `ursatz_once`; every caller gets the same pointer, and a failed creation stays NULL for good. The Framework lives as long as the process.

## Theory to Corpus open issue (Dependency Spec §7.2)

The System Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` as depending on `Corpus` (L9, `src/corpus/`). This module does not implement that dependency; the spec is explicit that no implementation may establish it until the resolution is recorded upstream. `Tendency`, `StatisticalModel`, and `StyleModel` each accept corpus-derived statistics only as an opaque, already-computed `Value` supplied by the caller. No file under `src/theory/` or `src/rules/` includes a corpus header. `tests/theory/test_architecture_theory.c` and `tests/rules/test_architecture_rules.c` enforce this by allowlisting only L0-L6 directories.


## Frameworks

Concrete theory lives in Framework extension modules under `frameworks/<name>/`, built on
`Framework`/`Rule`/`Constraint` without changing this module. Seventeen exist, from
`common-practice-minimal` (the one the analyzers use) through species counterpoint
(`standard-counterpoint`) and common-practice harmony (`standard-harmony`). The catalog, the
shared conventions and the build note live in `frameworks/README.md`; each framework's own
`README.md` holds its scope.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), and all four L6 modules only.

## Style statistics and blending

Corpus statistics reach `theory/` as plain values, never as a corpus type (the Theory -> Corpus open issue above stays open: `theory/` has no dependency on `corpus_stats/` or `corpus/`).

- `style_features.h`: the one format `style_features` is given, `style-tendencies-v1`: text, one statistic per line (`category`, `label`, `count`, `total`, tab-separated), sorted by category then label, no duplicates. `style_features_encode`/`_decode` pack and read it back. Counts and totals are unsigned integers; a frequency is `count/total`.
- `style_model_build.h`: `style_model_from_statistics` builds a `StyleModel` from an array of `StyleStatistic`, validating each (non-empty names without tab or newline, `total > 0`, `count <= total`, no duplicates). `style_model_tendencies` reads the model back as `Tendency` objects (label, frequency `count/total` unreduced, and the category and training corpus as observed data), so each statistic is observed behavior and nothing stronger.
- `style_model_blend.h`: `style_model_blend` blends two models with an exact weight `w` in [0, 1] (`(1 - w) * a + w * b` per category and label, a missing label counting as 0, results reduced). Weight 0 gives A and weight 1 gives B as reduced fractions; a label whose blended frequency is 0 is left out. A zero denominator, a weight above 1, an input that is not statistics, and an overflow are explicit results, never wraparound. The caller supplies the result's id, name, training-corpus label and version. After a blend, a statistic's count and total are the numerator and denominator of the blended frequency, not a literal tally.
- `style_fraction.h`: the small exact-fraction helper these use (reduce, add, multiply with overflow checks); `theory/` keeps its own copy because it cannot depend on `rules/`.
- **No path to a Rule.** None of these files (nor `tendency.h`) names a `Rule` or `Constraint` or includes anything from `rules/`; `tests/theory/test_style_model_not_a_rule.c` checks it, and a statistic can never exceed 1. Statistics are non-authoritative.
