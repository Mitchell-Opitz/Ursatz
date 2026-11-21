# theory

[Docs index](../README.md): the Ursatz docs by topic.

Layer L7 (`music-theory`). Path `theory/style/`, tests `tests/theory/`.

## Provides

`StyleModel`, and the framework names (`theory/vocabulary/framework_names.h`).

## Notes

- A framework is a name, not an object: the ten strings the analysis document's claims carry in their `framework` member are constants in `theory/vocabulary/framework_names.h` (R-687). The `Framework` type, its singleton helper and the 11 accessors were deleted (P66b B2).
- `StyleModel` keeps `style_features` as an opaque `Value` and `training_corpus` as a plain label string; its shape is unchanged. Corpus style statistics fill it (below). There is still no fitting: every number is a count or an exact rational.

## Theory to Corpus open issue (Dependency Spec §7.2)

The System Registry lists `StyleModel` as depending on `Corpus` (L9, `src/corpus/`). This module does not implement that dependency; the spec is explicit that no implementation may establish it until the resolution is recorded upstream. `StyleModel` accepts corpus-derived statistics only as an opaque, already-computed `Value` supplied by the caller. No file under `theory/style/` includes a corpus header. The layer table enforces this by allowing only L0-L6 directories.


## Frameworks

Concrete theory lives in the theory areas of `library/theory/` (one per former framework), built on
the rule table without changing this module. Eleven exist, from
`common-practice-minimal` (the one the analyzers use) through species counterpoint
(`standard-counterpoint`) and common-practice harmony (`standard-harmony`). The catalog, the
shared conventions and the build note live in `theory/README.md`; each framework's own
`README.md` holds its scope.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), `structure` (L5), and all four L6 modules only.

## Style statistics and blending

Corpus statistics reach `theory/` as plain values, never as a corpus type (the Theory -> Corpus open issue above stays open: `theory/` has no dependency on any storage or corpus module).

- `style_features.h`: the one format `style_features` is given, `style-tendencies-v1`: text, one statistic per line (`category`, `label`, `count`, `total`, tab-separated), sorted by category then label, no duplicates. `style_features_encode`/`_decode` pack and read it back. Counts and totals are unsigned integers; a frequency is `count/total`.
- `style_model_build.h`: `style_model_from_statistics` builds a `StyleModel` from an array of `StyleStatistic`, validating each (non-empty names without tab or newline, `total > 0`, `count <= total`, no duplicates). The statistics read back with `style_features_decode`.
- `style_model_blend.h`: `style_model_blend` blends two models with an exact weight `w` in [0, 1] (`(1 - w) * a + w * b` per category and label, a missing label counting as 0, results reduced). Weight 0 gives A and weight 1 gives B as reduced fractions; a label whose blended frequency is 0 is left out. A zero denominator, a weight above 1, an input that is not statistics, and an overflow are explicit results, never wraparound. The caller supplies the result's id, name, training-corpus label and version. After a blend, a statistic's count and total are the numerator and denominator of the blended frequency, not a literal tally.
- `style_fraction.h`: the small exact-fraction helper these use (add, multiply with overflow checks); `theory/` keeps its own exact-rational arithmetic because it cannot depend on `rules/`.
- **No path to a rule.** None of these files includes anything from `rules/`; `theory/tests/style/test_style_model_not_a_rule.c` checks it, and a statistic can never exceed 1. Statistics are non-authoritative.
