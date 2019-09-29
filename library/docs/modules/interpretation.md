# interpretation

Layer L6 (`music-interpretation`). Path `src/interpretation/`, tests `tests/interpretation/`. One of four L6 siblings; see `docs/modules/l6-notes.md` for rules shared across all four, including the now-closed Interpretation-to-Framework open issue.

## Provides

`Uncertainty`, `Evidence`, `Assertion`, `Observation`, `Hypothesis`, `Interpretation`, `Chord`, `Key`, `ScaleDegree`.

## Notes

- `Interpretation` never mutates its target and never enforces uniqueness on it. `target_id` is an opaque `EntityID` reference; no accessor resolves it to a mutable object. Multiple competing `Interpretation`s pointing at the same `target_id` are always valid to construct (TDD §89).
- `Interpretation` carries an optional `framework_reference` (`FrameworkReference`, `core`), closing Dependency Spec Open Issue §7.1. It is optional, not required, because most existing `Interpretation` producers (the eight `analysis` analyzers) construct framework-neutral claims. `Interpretation` owns the `FrameworkReference` wrapper itself, the same as `context_id`/`evidence_id`; it does not own or resolve the `Framework` the reference points to. No `theory`/`rules` header is imported anywhere in this module to support it.
- **The Interpretation-family shape (`Chord`, `Key`, `ScaleDegree`), the pattern for future Interpretation-family types to follow:** each of these three composes and owns a base `Interpretation` directly -- the same relationship `structure/motif_occurrence.h`'s `MotifOccurrence` has to `core/occurrence.h`'s `Occurrence` (the next type built directly on a base type embeds it rather than duplicating its fields). This gives each type three parts:
  1. **The underlying fact.** Carried by the embedded `Interpretation`'s own `target_id` -- no separate `sonority_id`/`context_id`/`scale_id` field is added. `Chord`'s `target_id` is a Sonority's `EntityID` (`semantics`); `Key`'s is a Context's `EntityID` (`context`); `ScaleDegree`'s is a Scale's `EntityID` (`semantics`). None of these three files import `semantics/` or `context/` headers to do this -- see the "No sideways imports" rule below, which the Sonority/Context/Scale reference must still honor.
  2. **A `FrameworkReference`**, also carried by the embedded `Interpretation` -- but required at these three constructors specifically (`chord_create`/`key_create`/`scale_degree_create` all reject a `NULL` `interpretation_framework_reference()`), unlike the base `Interpretation` type where the field stays optional. `Chord`/`Key`/`ScaleDegree` are precisely the Registry's framework-relative interpretation types; the base type stays permissive because most other producers are framework-neutral.
  3. **An opaque, non-enumerated `label` string** naming what the framework calls this thing (e.g. `"V7"`, `"D major"`, `"V"`) -- own field, new on each of these three types, not present on the base `Interpretation`. Same opaque-token convention already used for `Articulation.kind`, `Dynamics.marking`, `Instrument`'s illustrative properties, and `SourceVariant`'s descriptor: no concrete theory framework exists yet to own real chord-name/key-name/degree-name vocabulary, and hard-coding one now would prejudge which framework's vocabulary is authoritative.
  `ScaleDegree` additionally owns a `pitch` (`PitchIdentity`, `pitch`, L2) by value -- the specific pitch within the referenced Scale this degree names. It is owned, not referenced by `EntityID`, since `PitchIdentity` is an opaque token, not an Entity with its own identity.

## Deferred

`Annotation` and `ScholarlyAnnotation` (soft "shall eventually support" requirement, post-kernel, Registry non-normative -- same low-priority treatment already given to L5's example-only items).

Out of scope entirely, not missing. `Framework`, `Rule`, `Constraint`, `Preference`, `Heuristic`, `Tendency`, `StatisticalModel` (all L7); any actual harmonic/theoretical analysis (chord ID, key estimation, Roman numeral analysis, cadence detection, Phase 8 `analysis`); and full `ProvenanceAgent`/`ProvenanceActivity`/`Derivation` objects (Phase 6 cross-cutting Provenance module).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), `events` (L3), `containers` (L4), and `structure` (L5) only. Never imports `context`, `semantics`, or `relationship`.