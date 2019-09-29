# L6 shared notes

`context`, `semantics`, `relationship`, and `interpretation` are built together against Dependency & Layer Spec v2 §2-§3 as four mutually-restricted siblings. Rules on this page apply to all four; each module's own page covers only what is specific to it.

## No sideways imports

The four modules never import each other's headers. Every cross-module reference at this layer is an opaque `EntityID` (L0). `Relationship`'s `context_id`/`evidence_id`, `Interpretation`'s `context_id`/`evidence_id`, `Relationship`'s `predicate_name` (correlated to a `PredicateDefinition` by name string, not by object reference), and `Chord`/`Key`/`ScaleDegree`'s Sonority/Context/Scale reference (carried by their embedded `Interpretation`'s own `target_id`, per `docs/modules/interpretation.md`) all follow this pattern. Each module's architecture test allowlists only L0-L5 plus its own directory; none allowlist a sibling L6 module.

## Not-yet-built dependencies get an opaque placeholder

These follow the same pattern `Cadence`'s `optional_harmonic_context` established at L5. `Relationship`'s `uncertainty` field is an opaque, nullable `Value`, since it was built before `Uncertainty` existed. `Assertion`'s and `Interpretation`'s `author` fields are opaque agent-identifier strings, since `ProvenanceAgent` does not exist yet. `Observation`'s `timestamp` is an opaque, nullable `Value`; it is real-world wall-clock time, not a musical position, and no wall-clock timestamp type exists yet.

## Interpretation-family to Framework (Dependency Spec §7.1, closed)

The System Registry's "Depends On" column for `Interpretation`, `ScaleDegree`, `Chord`, and `Key` lists `Framework` (L7, `theory`) as a dependency. A literal import would violate the layer rule.

Per System Registry v3, the resolution is a reference type. `Interpretation` carries an optional `framework_reference` (`FrameworkReference`, `core`, L1) instead of a live `Framework` reference; the reference type lives at L1, the same reference-only pattern already used for `NoteEvent`'s `optional_voice` (`VoiceReference`) at L3. `ScaleDegree`, `Chord`, and `Key` (now built, `interpretation/chord.h`, `interpretation/key.h`, `interpretation/scale_degree.h`) each compose an `Interpretation` and require its `framework_reference` to be non-NULL at construction, rather than adding a second, direct `Framework` reference of their own. No `theory`/`rules` import exists anywhere in `interpretation` or the other three L6 modules; `interpretation`'s architecture test enforces this. Resolution to an actual `Framework` happens only at L7+. See `docs/modules/core.md` for `FrameworkReference` and `docs/modules/interpretation.md` for the field itself and the full `Chord`/`Key`/`ScaleDegree` shape. See `docs/modules/theory.md` for the L7 side, which was already compliant (`Framework` never held a reference back into `interpretation`).