# interpretation

Layer L6 (`music-interpretation`). Path `src/interpretation/`, tests `tests/interpretation/`. One of four L6 siblings; see `docs/modules/l6-notes.md` for rules shared across all four, including the now-closed Interpretation-to-Framework open issue.

## Provides

`Uncertainty`, `Evidence`, `Assertion`, `Observation`, `Hypothesis`, `Interpretation`.

## Notes

- `Uncertainty` is a small tagged/variant type, not a scalar. TDD §84-86 names eight distinct uncertainty shapes; this module implements three (`UNCERTAINTY_KIND_UNKNOWN`, `_SCALAR_CONFIDENCE`, `_CANDIDATE_SET`) and documents the rest (interval estimate, probability distribution, credible interval, qualitative uncertainty) as future `UncertaintyKind` variants. Confidence and candidate weights are exact rational fractions, never float or double.
- `Evidence`'s strength, relevance, and reliability never collapse to one score. Each is its own rational-fraction field; there is no single "evidence score" anywhere in `evidence.h`.
- `Hypothesis` supports multiple candidates by construction. `hypothesis_create` takes a `HypothesisCandidate` array (`candidate_count >= 1`, uncapped), not a single value plus a confidence score. Multiple `Hypothesis` instances may reference the same `Observation` id without conflict.
- `Interpretation` never mutates its target and never enforces uniqueness on it. `target_id` is an opaque `EntityID` reference; no accessor resolves it to a mutable object. Multiple competing `Interpretation`s pointing at the same `target_id` are always valid to construct (TDD §89).
- `Interpretation` carries an optional `framework_reference` (`FrameworkReference`, `core`), closing Dependency Spec Open Issue §7.1. It is optional, not required, because `ScaleDegree`/`Chord`/`Key`, the framework-relative interpretation types, are still deferred (see below), and every existing `Interpretation` producer (the eight `analysis` analyzers) constructs framework-neutral claims. `Interpretation` owns the `FrameworkReference` wrapper itself, the same as `context_id`/`evidence_id`; it does not own or resolve the `Framework` the reference points to. No `theory`/`rules` header is imported anywhere in this module to support it.

## Deferred

`ScaleDegree` (depends on `Framework`, Phase 3/8), `Chord` and `Key` (Phase 8, depend on `Framework`), `Annotation` and `ScholarlyAnnotation` (soft "shall eventually support" requirement, post-kernel).

Out of scope entirely, not missing. `Framework`, `Rule`, `Constraint`, `Preference`, `Heuristic`, `Tendency`, `StatisticalModel` (all L7); any actual harmonic/theoretical analysis (chord ID, key estimation, Roman numeral analysis, cadence detection, Phase 8 `analysis`); and full `ProvenanceAgent`/`ProvenanceActivity`/`Derivation` objects (Phase 6 cross-cutting Provenance module).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `context`, `semantics`, or `relationship`.
