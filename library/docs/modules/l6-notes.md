# L6 shared notes

`context`, `semantics`, `relationship`, and `interpretation` are built together against Dependency & Layer Spec v2 §2-§3 as four mutually-restricted siblings. Rules on this page apply to all four; each module's own page covers only what is specific to it.

## No sideways imports

The four modules never import each other's headers. Every cross-module reference at this layer is an opaque `EntityID` (L0). `Relationship`'s `context_id`/`evidence_id`, `Interpretation`'s `context_id`/`evidence_id`, and `Relationship`'s `predicate_name` (correlated to a `PredicateDefinition` by name string, not by object reference) all follow this pattern. Each module's architecture test allowlists only L0-L5 plus its own directory; none allowlist a sibling L6 module.

## Not-yet-built dependencies get an opaque placeholder

These follow the same pattern `Cadence`'s `optional_harmonic_context` established at L5. `Relationship`'s `uncertainty` field is an opaque, nullable `Value`, since it was built before `Uncertainty` existed. `Assertion`'s and `Interpretation`'s `author` fields are opaque agent-identifier strings, since `ProvenanceAgent` does not exist yet. `Observation`'s `timestamp` is an opaque, nullable `Value`; it is real-world wall-clock time, not a musical position, and no wall-clock timestamp type exists yet.

## Interpretation-family to Framework (open issue)

The System Registry's "Depends On" column for `Interpretation`, `ScaleDegree`, `Chord`, and `Key` lists `Framework` (L7, `theory`) as a dependency. A literal import would violate the layer rule. `ScaleDegree`, `Chord`, and `Key` sit at Registry Phase 3/8 or 8, not 6, and are deferred whole, so only `Interpretation` itself needed resolving here. It has no resolved `framework` field. It is omitted entirely, not even an opaque placeholder, flagged as follow-up once `theory` exists to define what that field should hold. This is Dependency Spec Open Issue §7.1; it is not resolved by this note. See `docs/modules/theory.md` for the L7 side of the same issue.
