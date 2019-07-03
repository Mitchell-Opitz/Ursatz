# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/`
with its tests mirrored under `tests/<module>/`.

## Layers

- **L0** - no dependencies on any other module. Currently: `identity`.
- **L1+** - may depend only on lower layers, never sideways or upward.
  Currently: `core` (music-core), which depends on `identity` only.
- **L2** - may depend only on L0/L1, never sideways or upward. Currently:
  `time` (music-time) and `pitch` (music-pitch), each depending on
  `identity` and `core` only. `time` and `pitch` are siblings and do not
  depend on each other.
- **L3** - may depend only on L0/L1/L2, never sideways or upward.
  Currently: `events` (music-events), depending on `identity`, `core`,
  `time`, and `pitch`. `events` MUST NOT depend on `music-containers`
  (L4) -- where a lower object needs to reference Voice (L4), it uses
  `VoiceReference` (L1, `music-core`) instead of a resolved Voice
  object or a `music-containers` import.
- **L4** - may depend only on L0/L1/L2/L3, never sideways or upward.
  Currently: `containers` (music-containers), depending on `identity`,
  `core`, `time`, `pitch`, and `events`. `containers` MUST NOT depend on
  anything L5+.
- **L5** - may depend only on L0/L1/L2/L3/L4, never sideways or upward.
  Currently: `structure` (music-structure), depending on `identity`,
  `core`, `time`, `pitch`, `events`, and `containers`. `structure` MUST
  NOT depend on anything L6+.
- **L6** - may depend only on L0/L1/L2/L3/L4/L5, never sideways or
  upward. Currently: `context` (music-context), `semantics`
  (music-semantics), `relationship` (music-relationship), and
  `interpretation` (music-interpretation), each depending on
  `identity`, `core`, `time`, `pitch`, `events`, `containers`, and
  `structure` only. All four MUST NOT depend on `music-theory` or
  `music-rules` (L7) or anything above -- AND, being siblings at the
  same layer, MUST NOT depend on each other either (no
  `context`<->`semantics`<->`relationship`<->`interpretation` imports
  in any direction). Cross-module references at L6 go through opaque
  `EntityID` (L0) only, never a direct header dependency on a sibling
  L6 module.
- **L7** - may depend only on L0/L1/L2/L3/L4/L5/L6, never sideways or
  upward. Currently: `theory` (music-theory: `Framework`, `Heuristic`,
  `Tendency`, `StatisticalModel`, `StyleModel`) and `rules`
  (music-rules: `Rule`, `Constraint`, `Preference`), each depending on
  `identity`, `core`, `time`, `pitch`, `events`, `containers`,
  `structure`, `context`, `semantics`, `relationship`, and
  `interpretation` only. Both MUST NOT depend on `music-corpus` (L9)
  or `music-analysis`/`music-generation`/`music-transform` (L8) --
  see the OPEN ISSUE in the L7 module notes below. Unlike L6,
  `theory` and `rules` are not mutually sideways-restricted:
  `rules` depends directly on `theory` (`Rule`, `Constraint`, and
  `Preference` each hold a direct, unowned `Framework` reference), but
  `theory` never depends back on `rules`.
- **L8** - may depend only on L0/L1/L2/L3/L4/L5/L6/L7, never sideways
  or upward. Currently: `transform` (music-transform: `Transformation`
  and its eleven concrete subtypes), `analysis` (music-analysis:
  `Analyzer`, `AnalysisResult`, `FeatureExtraction`, and eight concrete
  analyzers), and `generation` (music-generation: `Intent` through
  `GenerationRecord`), each depending on `identity`, `core`, `time`,
  `pitch`, `events`, `containers`, `structure`, `context`, `semantics`,
  `relationship`, `interpretation`, `theory`, and `rules` only. All
  three MUST NOT depend on `music-corpus`/`music-io`/`music-notation`/
  `music-performance` (L9) -- AND, being siblings at the same layer,
  MUST NOT depend on each other either (no
  `transform`<->`analysis`<->`generation` imports in any direction).
  This is why `generation/candidate_evaluator.h`'s `CandidateEvaluation`
  cannot reuse `analysis/analyzer.h`'s `AnalysisResult` even though both
  hold the identical Interpretation/Evidence shape -- see the L8 module
  notes below.

A module must not import anything outside its own layer's allowed
dependencies. Each module has an architecture test enforcing this
(e.g. `tests/identity/test_architecture_identity.c`,
`tests/core/test_architecture_core.c`, `tests/time/test_architecture_time.c`,
`tests/pitch/test_architecture_pitch.c`,
`tests/events/test_architecture_events.c`,
`tests/containers/test_architecture_containers.c`,
`tests/structure/test_architecture_structure.c`,
`tests/context/test_architecture_context.c`,
`tests/semantics/test_architecture_semantics.c`,
`tests/relationship/test_architecture_relationship.c`,
`tests/interpretation/test_architecture_interpretation.c`,
`tests/theory/test_architecture_theory.c`,
`tests/rules/test_architecture_rules.c`,
`tests/transform/test_architecture_transform.c`,
`tests/analysis/test_architecture_analysis.c`,
`tests/generation/test_architecture_generation.c`).

## Modules

| Module         | Layer | Path                   | Tests                      |
|----------------|-------|------------------------|-----------------------------|
| identity       | L0    | `src/identity/`        | `tests/identity/`           |
| core           | L1    | `src/core/`            | `tests/core/`               |
| time           | L2    | `src/time/`            | `tests/time/`               |
| pitch          | L2    | `src/pitch/`           | `tests/pitch/`              |
| events         | L3    | `src/events/`          | `tests/events/`             |
| containers     | L4    | `src/containers/`      | `tests/containers/`         |
| structure      | L5    | `src/structure/`       | `tests/structure/`          |
| context        | L6    | `src/context/`         | `tests/context/`            |
| semantics      | L6    | `src/semantics/`       | `tests/semantics/`          |
| relationship   | L6    | `src/relationship/`    | `tests/relationship/`       |
| interpretation | L6    | `src/interpretation/`  | `tests/interpretation/`     |
| theory         | L7    | `src/theory/`          | `tests/theory/`             |
| rules          | L7    | `src/rules/`           | `tests/rules/`              |
| transform      | L8    | `src/transform/`       | `tests/transform/`          |
| analysis       | L8    | `src/analysis/`        | `tests/analysis/`           |
| generation     | L8    | `src/generation/`      | `tests/generation/`         |

## music-core notes

`music-core` provides the base types other modules build concrete types
from: `Entity`, `Value`, `Occurrence`, the five equality concepts
(identity, value, structural, semantic, canonical), and three reference
types (`VoiceReference`, `RelationshipReference`, `ProvenanceReference`).

A few things worth knowing before building on top of it:

- The Entity/Occurrence/Event category boundary is still contested
  upstream (Domain Spec, System Registry). `Entity` and `Occurrence` are
  implemented without assuming how that boundary settles, and code built
  on them should do the same.
- `SemanticEquality` is a contract only at this layer (`SemanticEqualsFn`
  in `core/equality.h`). A real comparison needs Context, which is an L6
  concept, so it is deferred until that layer exists.
- `Entity`'s `provenance_reference` field from the System Registry is not
  implemented yet. `ProvenanceReference` exists as a type
  (`core/provenance_reference.h`), but wiring it into `Entity` is separate
  follow-up work, not part of this layer's initial pass.
- The "Canonical Musical Model / IR" described in the System Registry is
  not a module music-core imports. It is the graph formed when these L1
  base types are combined with higher-layer objects at consumption time.
  There is nothing to import here to satisfy that row.

## music-time notes

`music-time` provides `Duration` (magnitude), `MusicalTime` (position),
`TimeSpan` (a positioned interval between two `MusicalTime`s), and
`TimeInstant` (a zero-duration point, wrapping `MusicalTime`). All four
are exact rationals, never float/double (Law 19).

A few things worth knowing before building on top of it:

- `Duration` and `MusicalTime` are structurally identical (numerator/
  denominator, reduced to lowest terms) but are separate, non-convertible
  types: one is a magnitude, the other a position. Do not add a
  conversion between them.
- `TimeSpan`'s `boundary` vocabulary (`closed` / `open` / `left_closed` /
  `right_closed`) is PROVISIONAL per Domain Spec §16.3, not frozen. It is
  implemented as given, but a later revision to that vocabulary is not a
  breaking-change violation.
- `TimeSpan` requires `boundary` to be supplied explicitly at
  construction; there is no default. A deserializer built on top of
  `time_span_create` must reject a payload that omits it rather than
  assuming `closed`.
- `MusicalTime` has no lower bound enforced at this layer: negative
  positions (e.g. pickup measures) are legal here. Semantics for what a
  negative position means are deferred to a later spec.
- `TemporalAnchor` wraps a `MusicalTime` and situates a higher-layer
  object at that position, so callers building on `Occurrence` (L1) gain
  a temporal position without embedding position semantics in `Occurrence`
  itself, and without reimplementing `MusicalTime`'s rational
  representation outside `music-time`. Added as an L2 prerequisite for L3
  `music-events`' `Event` type; it carries no fields beyond the wrapped
  `MusicalTime`.

## music-pitch notes

`music-pitch` provides `PitchIdentity` (an opaque token identifying a
pitch) and `PitchSpelling` (a written pitch name: letter, accidental,
octave).

A few things worth knowing before building on top of it:

- `PitchIdentity`'s equality is a PLACEHOLDER: value equality on the raw
  token bytes. The true equivalence relation is deferred to Deliverable D,
  which does not exist yet. Expect this to be revised.
- `PitchIdentity`'s token is intentionally opaque and is NOT an EntityID.
  Its constructor takes a raw byte buffer, not an `EntityID*`, so there is
  no path to construct one from an EntityID-typed value.
- `PitchIdentity` construction takes no tuning-system parameters (no
  12-TET, no A440, no MIDI note number). Do not add any before
  Deliverable D exists.
- `PitchSpelling`'s `accidental` field is treated opaquely (an integer
  offset); its exact model is also deferred to Deliverable D.
  Enharmonic equivalents (e.g. C# and Db) are intentionally NOT
  value-equal, even though they may sound identical under some tunings.
- `PitchClass`, `PitchRealization`, `TuningSystem`, `WrittenPitch`, and
  `SoundingPitch` are Phase 2/3 (Deliverable D) and are out of scope for
  this module's initial pass.

## music-events notes

`music-events` provides `Event` (base type composing `Occurrence` (L1)
with `TemporalAnchor` (L2)), `NoteEvent`, `RestEvent`, `ControlEvent`
(the three Phase-1 event kinds), and `Tie` (a relationship connecting
two `NoteEvent`s by id).

A few things worth knowing before building on top of it:

- STATUS FLAG, carried forward, not resolved here: `NoteEvent`'s
  category (Entity vs Event vs pure Occurrence) is UNRESOLVED upstream
  (Domain Spec §9, §16.1). `NoteEvent` is implemented under the current
  provisional "Event" treatment and is deliberately not built on top of
  `core/entity.h`'s `Entity` type, since doing so would itself assume a
  resolution. Code built on `NoteEvent` should do the same.
- `NoteEvent`'s `optional_voice` field is typed as `VoiceReference*`
  (L1). There is no C type representing a resolved Voice (L4) anywhere
  in this codebase, so the L3/L4 non-dependency cannot be violated
  through that constructor -- and
  `tests/events/test_architecture_events.c` independently confirms no
  `containers/` include exists anywhere under `src/events/`.
- `NoteEvent` has two separate equality functions, not one:
  `note_event_equals_by_id` (IdentityEquality, same id regardless of
  other fields) and `note_event_equals_by_value` (StructuralEquality,
  same content independent of id). Value-equal does NOT imply the same
  event -- two independently constructed `NoteEvent`s can carry
  identical content under different ids.
- `NoteEvent` has no mutator. A revision is a new `NoteEvent` with a new
  id and/or provenance; constructing that from an old one needs an
  Activity/Transformation, which does not exist yet at this layer and is
  out of scope here.
- `NoteEvent`'s `provenance` field must always be an explicit
  `ProvenanceReference` (present or absent) -- never `NULL`. Hand-authored
  `NoteEvent`s may use `state=absent`; nothing yet enforces
  `state=present` for Activity-produced `NoteEvent`s, since no Activity/
  Transformation exists at this layer to produce that case.
- `Tie` references its two `NoteEvent`s by `EntityID` only, never by
  embedded `NoteEvent` instances -- constructing a `Tie` does not alter
  either referenced `NoteEvent`'s own fields.
- `ControlEvent`'s payload is generic/opaque at this layer, the same
  treatment `PitchIdentity` gives its token (`pitch/pitch_identity.h`).
  Protocol-specific meaning (MIDI, OSC, MusicXML, ...) is an adapter
  concern (L9+), not this module's job.
- Deferred, not implemented in this pass: `Slur`, `Articulation`,
  `Dynamics` (Registry Phase 2). `Instrument`, `InstrumentCapability`
  (Registry Phase "1+", provisional placement, property list explicitly
  flagged illustrative/non-normative) are also skipped this pass.
- Out of scope for `music-events` entirely, not missing: `Voice` itself
  (L4, `music-containers` -- only `VoiceReference` is used here),
  `Interval`, `Sonority`, `PerformanceEvent` (later layers, referenced
  only in `NoteEvent`'s relationships documentation), and any
  Transformation/Activity-driven provenance enforcement.

## music-containers notes

`music-containers` provides `Container` (a pure grouping base
category, no fields of its own) and `Voice` (Container), the only
Phase-1 concrete container type.

A few things worth knowing before building on top of it:

- `Voice`'s schema is `{id}` only (Domain Spec §10). It has no
  membership field -- no owned/mutable list of notes or events. The
  mechanism by which a Voice's membership is represented is deferred
  to a later Container/Structure spec that does not exist yet.
- A `Voice` MUST NOT be assumed to map 1:1 to an Instrument, Staff, or
  Part. A `Voice` MAY appear on multiple staves; a Staff MAY contain
  multiple voices -- semantic notes only, since `Staff` does not exist
  this pass.
- `Voice` has no mutator: a revision is a new `Voice` construction, not
  an in-place field change (Law C2).
- `Voice`'s identity equality (`voice_identity_equals`) compares by
  `EntityID` only, the same pattern `Entity` (`core/entity.h`) uses,
  reduced to the one field `Voice` actually has -- `Voice` carries no
  separate schema/version field to compare.
- This is also the layer where `VoiceReference` (built at L1 back in
  the `music-core` pass) gets a real resolution target. No code change
  was needed to `VoiceReference` itself; nothing in `music-containers`
  redefines or duplicates what it already does.
- Deferred, not implemented in this pass: `Layer`, `Staff`, `Part`,
  `Measure` (Registry Phase 2); `Phrase` (container use), `Section`,
  `Movement`, `Work`, `Edition`, `Arrangement`, `Revision`,
  `SourceVariant` (Registry Phase 5, several marked "Reference
  Concept" / non-normative).
- Out of scope for `music-containers` entirely, not missing:
  resolution logic that takes a `VoiceReference` and looks up the
  actual `Voice` (a repository/registry pattern is an application/
  query-layer, L9, concern), and any `Voice`<->`Instrument`,
  `Voice`<->`Staff`, or `Voice`<->`Part` relationship (deferred to
  `music-relationship`, L6, or later container work).

## music-structure notes

`music-structure` provides `Structure` (base category composing an
`Entity` (L1) with non-duplicating event refs), `Motif` (Structure),
`MotifOccurrence` (built on `Occurrence`, L1), and `Cadence` (Structure).

A few things worth knowing before building on top of it:

- `Structure` itself is a real, constructible base type, not a pure
  marker like `Container` (`music-containers`) -- the Registry requires
  an invariant test at this layer confirming referenced events are
  never duplicated, which means `Structure` needs actual event-ref
  state to test against.
- `Motif`, `MotifOccurrence`, and `Cadence` do not embed `Structure`'s
  or `Entity`'s wrapper structs directly. Each flattens to its own
  fields (a bare `EntityID` id plus `EntityID` event refs), the same
  approach `NoteEvent` takes relative to `Event` and `Voice` takes
  relative to `Container` elsewhere in this codebase -- concrete types
  reimplement the base type's field shape rather than composing the
  wrapper struct itself.
- `MotifOccurrence` is the one exception that does embed its L1 base
  type directly: it composes `Occurrence` (own id plus the target
  Motif's id) the same way `Event` composes `Occurrence` with a
  `TemporalAnchor` -- both are the object built immediately on top of
  `Occurrence`, so both embed it rather than flattening past it.
- No hierarchy is assumed among `Structure` subtypes (TDD §57). A
  Structure graph is not required to follow Motif -> Phrase -> Theme ->
  Section as a universal shape, and nothing in `music-structure` hard-
  codes that shape.
- `Cadence` is a Phase-5 structural shell only (System Registry: "Yes
  (explicit Phase 5/8 deliverable)"). Its `optional_harmonic_context`
  field is an opaque, nullable `Value` (`core/value.h`) -- the same
  opaque-token treatment `PitchIdentity` and `ControlEvent`'s payload
  use. Full harmonic-context semantics are an L6+/L7 concern and are
  not built here; construction never requires that machinery.
  Cadence DETECTION -- recognizing that a region of events forms a
  cadence -- is Phase 8 (`music-analysis`) work and is out of scope for
  this module.
- Deferred, not implemented in this pass (Registry Normative = No,
  "example only" / "naming-pattern example"): `Figure`; `Theme`,
  `ThemeOccurrence`; `PhraseOccurrence` (`Phrase` itself is L4
  container-side, deferred there too, not built here either).
- Out of scope for `music-structure` entirely, not missing:
  `SimilarityResult`, Motif Detection, Phrase Segmentation, Cadence
  Detection -- Phase 8 (`music-analysis`) procedures that PRODUCE
  `Motif`/`MotifOccurrence`/`Cadence` instances, not data shapes this
  layer defines. Also out of scope: `Section`, `Movement`, `Work` (L4
  containers, deferred there too, not L5 structures), and any harmonic-
  context resolution (L6/L7).

## L6: music-context, music-semantics, music-relationship, music-interpretation notes

L6 is four sibling modules, built together against Dependency & Layer
Spec v2 §2-§3: `context` (`Context`), `semantics` (`Interval`,
`Sonority`, `RhythmicPattern`), `relationship` (`Relationship`,
`PredicateDefinition`), and `interpretation` (`Uncertainty`,
`Evidence`, `Assertion`, `Observation`, `Hypothesis`,
`Interpretation`).

IMPORTANT CONTRADICTION FLAG (same shape as the Canonical IR issue at
L1, and carried forward from this pass's task brief -- do not resolve
this silently if revisiting it): the System Registry's "Depends On"
column for `Interpretation`, `ScaleDegree`, `Chord`, and `Key` lists
"Framework" (L7, `music-theory`) as a dependency. A literal import
would violate the layer rule the same way the Canonical IR row did at
L1. `ScaleDegree`, `Chord`, and `Key` sit at Registry Phase 3/8 or 8,
not 6, and are deferred whole (see below), so this pass only had to
resolve it for `Interpretation` itself:

- `Interpretation` (`interpretation/interpretation.h`) has NO resolved
  `framework` field. It is omitted entirely -- not even an opaque
  placeholder -- flagged as a follow-up once `music-theory` exists to
  define what that field should hold.

A few more things worth knowing before building on top of L6:

- **No sideways L6 imports.** `context`, `semantics`, `relationship`,
  and `interpretation` never import each other's headers. Every
  cross-module reference at this layer is an opaque `EntityID` (L0):
  `Relationship`'s `context_id`/`evidence_id`
  (`relationship/relationship.h`), `Interpretation`'s
  `context_id`/`evidence_id` (`interpretation/interpretation.h`), and
  `Relationship`'s `predicate_name` (correlated to a
  `PredicateDefinition` by name string, not by object reference) all
  follow this. Each module's architecture test only allowlists L0-L5
  plus its own directory -- none allowlist a sibling L6 module.
- **Not-yet-built dependencies get the opaque-Value placeholder
  treatment**, the same pattern `Cadence`'s `optional_harmonic_context`
  established at L5 (`structure/cadence.h`): `Relationship`'s
  `uncertainty` field is an opaque, nullable `Value` (`core/value.h`),
  since it was built (commit 8) before `Uncertainty`
  (`interpretation/uncertainty.h`, commit 11) existed. `Assertion`'s
  and `Interpretation`'s `author` fields are opaque agent-identifier
  strings, since `ProvenanceAgent` does not exist yet at the
  cross-cutting Provenance module. `Observation`'s `timestamp` is an
  opaque, nullable `Value` -- it is real-world wall-clock time, not a
  musical position, and `TimeInstant` (`time/time_instant.h`) is the
  wrong domain for it; no wall-clock timestamp type exists yet.
- **`Interval` and `Sonority` do not depend on `PitchClass`.**
  `PitchClass`, `PitchRealization`, and `TuningSystem` are Phase 3
  Deliverable D and are not built yet. `Interval`
  (`semantics/interval.h`) computes nothing itself -- since
  `PitchIdentity` (`pitch/pitch_identity.h`) is an opaque token with no
  numeric semantics, each of `Interval`'s three dimensions (directed,
  undirected, chromatic distance) is supplied by the caller as an
  independently-optional field, never defaulted to zero when unknown.
  Diatonic distance (the fourth TDD §67 dimension) is not represented
  at all, since it needs `PitchClass`.
- **`Sonority` is not a `Chord`.** `sonority.h` carries no chord-
  identification logic (root, quality, inversion); that is `Chord`
  (Phase 8, depends on Framework/L7, deferred below).
- **`RhythmicPattern` has no pitch fields.** `rhythmic_pattern.h` takes
  no dependency on `pitch/pitch_identity.h` or `pitch/pitch_spelling.h`
  -- a rhythmic query never needs pitch information.
- **`PredicateDefinition`'s vocabulary is open, not a fixed enum.** The
  System Registry marks the initial predicate vocabulary (TDD §77:
  `contains`, `precedes`, `derived_from`, `realizes`, `resembles`,
  `interpreted_as`, etc.) "explicitly extensible, not fixed" --
  `predicate_definition.h` is a registrable metadata type with an open
  `name` string, not a closed set of predicate constants.
- **`Uncertainty` is a small tagged/variant type, not a scalar.** TDD
  §84-86 names eight distinct uncertainty shapes; this pass implements
  three (`UNCERTAINTY_KIND_UNKNOWN`, `_SCALAR_CONFIDENCE`,
  `_CANDIDATE_SET`) and documents the rest (interval estimate,
  probability distribution, credible interval, qualitative
  uncertainty) as future `UncertaintyKind` variants, not something to
  force into the existing three. Confidence and candidate weights are
  exact rational fractions, never float/double (Law 19).
- **`Evidence`'s strength/relevance/reliability never collapse to one
  score.** Each is its own rational-fraction field
  (`evidence/*_numerator`/`*_denominator` accessor pairs); there is no
  single "evidence score" anywhere in `evidence.h`.
- **`Hypothesis` supports multiple candidates by construction.**
  `hypothesis_create` takes a `HypothesisCandidate` array
  (`candidate_count >= 1`, uncapped), not a single value plus a
  confidence score. Multiple `Hypothesis` instances may reference the
  same `Observation` id without conflict -- `observation_ids` is a
  reference list, never an ownership claim.
- **`Interpretation` never mutates its target, and never enforces
  uniqueness on it.** `target_id` is an opaque `EntityID` reference
  (Law 8: no accessor resolves it to a mutable object). Multiple
  competing `Interpretation`s pointing at the same `target_id` are
  always valid to construct (TDD §89) -- nothing in this module tracks
  "the" `Interpretation` for a given target.
- Deferred, not implemented in this pass (wrong phase and/or
  non-normative and/or blocked on `Framework`, which does not exist
  yet): `Scale` (Registry Normative = No, reference concept only),
  `ScaleDegree` (depends on Framework, Phase 3/8), `Contour` (Registry
  Normative = No, descriptive examples only), `Chord` and `Key` (Phase
  8, depend on Framework), `Annotation` and `ScholarlyAnnotation`
  ("post kernel" / partial-normative, soft "shall eventually support"
  requirement), and `PitchClass`/`PitchRealization`/`TuningSystem`
  (Phase 3 Deliverable D, not yet spec'd).
- Out of scope for L6 entirely, not missing: `Framework`, `Rule`,
  `Constraint`, `Preference`, `Heuristic`, `Tendency`,
  `StatisticalModel`, `Procedure`, `RuleEvaluation` (all L7,
  `music-theory`/`music-rules`); any actual harmonic/theoretical
  analysis -- chord ID, key estimation, Roman numeral analysis, cadence
  detection (Phase 8, `music-analysis`); and full
  `ProvenanceAgent`/`ProvenanceActivity`/`Derivation` objects (Phase 6
  cross-cutting Provenance module, separate from this pass).

## L7: music-theory, music-rules notes

L7 is two modules built together against Dependency & Layer Spec v2
§2-§3: `theory` (`Framework`, `Heuristic`, `Tendency`,
`StatisticalModel`, `StyleModel`) and `rules` (`Rule`, `Constraint`,
`Preference`). Unlike L6's four mutually-restricted siblings, `rules`
depends directly on `theory` -- `Rule`, `Constraint`, and `Preference`
each hold a direct, unowned `Framework` reference (the same
reference-only pattern `Context` uses for its own parent field,
`context/context.h`) -- while `theory` never depends back on `rules`.

OPEN ISSUE, GUARDRAIL, NOT RESOLVED (Dependency Spec §7.2): the System
Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` as
depending on `Corpus` (L9, `music-corpus`). This pass does not
implement that dependency -- the spec is explicit that no
implementation may establish the Theory -> Corpus dependency until the
resolution is recorded in the Domain Ontology Specification and the
System Registry is updated. `Tendency`, `StatisticalModel`, and
`StyleModel` each accept corpus-derived statistics only as an opaque,
already-computed `Value` (`core/value.h`) supplied by the caller --
none of the three, nor any other file under `src/theory/` or
`src/rules/`, includes a `music-corpus` header. `tests/theory/
test_architecture_theory.c` and `tests/rules/test_architecture_rules.c`
enforce this the same way the per-module L6 tests enforce their own
layer boundary: by allowlisting only L0-L6 directories.

This also resolves the other side of the L6 Interpretation<->Framework
open issue flagged above (Dependency Spec §7.1): `Framework`
(`theory/framework.h`) holds no reference back into
`music-interpretation`, and carries no forward reference to any of its
own siblings (`Rule`, `Constraint`, `Preference`, `Heuristic`,
`Tendency`, `StatisticalModel`, `StyleModel`) either -- it is the
thing an L6+ consumer resolves a reference against, never the thing
that reaches back out. `Interpretation` itself still has no `framework`
field; that remains unresolved from the L6 side, unchanged in this
pass.

A few more things worth knowing before building on top of L7:

- **`Constraint`'s type is a tagged type, not a boolean.**
  `ConstraintType` (`rules/constraint.h`) has five variants (`_HARD`,
  `_SOFT`, `_WEIGHTED`, `_CONDITIONAL`, `_CONTEXTUAL`, TDD §96), not a
  single `is_hard` flag. Only `CONSTRAINT_TYPE_WEIGHTED` uses the
  weight numerator/denominator fields; every other type ignores them.
  `constraint_evaluate` is a stub -- it validates its arguments and
  reports `CONSTRAINT_EVAL_NOT_IMPLEMENTED`, never actually deciding
  whether a candidate satisfies the constraint. Wiring real evaluation
  logic into a generation pipeline is `music-generation` (L8) work, out
  of scope here.
- **`Preference` cannot reject a candidate.** `preference_rank`
  (`rules/preference.h`) is structurally distinct from
  `constraint_evaluate`: its result type has no PASS/FAIL/invalid
  variant, only "computed" or "not implemented" -- there is no code
  path anywhere in `preference.h` through which a `Preference` could
  invalidate a candidate the way a `Constraint` can.
- **`Tendency`'s frequency is not a Rule/Constraint verdict.**
  `TendencyResult` (`theory/tendency.h`) is a disjoint enum from
  `RuleResult` and `ConstraintResult`/`ConstraintEvalResult` -- no
  function converts a `Tendency` into either, and no function in
  `rule.h`/`constraint.h` accepts a `Tendency`. An observed frequency
  (TDD §99) never automatically becomes a must/must-not/valid/invalid
  claim.
- **`StatisticalModel`'s output cannot be silently promoted to a
  `Rule`.** `theory/statistical_model.h` does not include
  `rules/rule.h`, and no function anywhere converts a
  `StatisticalModel` into a `Rule`. `training_corpus` is a plain
  string label (TDD §100), never a `music-corpus` object reference.
- **`Heuristic` exposes no evaluate/pass-fail API at all.**
  `theory/heuristic.h` is identity plus a label only (TDD §98) -- it
  cannot be treated as authoritative because there is nothing on the
  type that could be evaluated as true/false in the first place.
- **`StyleModel` is shape only, same as `Tendency` and
  `StatisticalModel`.** `style_features` is an opaque `Value`,
  `training_corpus` is a plain label string, and there is no fitting or
  comparison logic in this pass -- construction/shape tests only.
- Deferred, flagged for project-owner confirmation before building
  (Registry-First Dependency Rule, Dependency Spec §6): `RuleEvaluation`
  -- `Rule` depends on it, but no distinct Registry row/shape was found
  for it in this pass; do not invent its field shape from TDD §102
  alone without that confirmation.
- Deferred, wrong module for this pass (not `music-theory`/`music-rules`):
  `Procedure` -- despite appearing near Phase 9 material in the TDD,
  its Registry module is `music-analysis`/`music-generation` (L8).
- Deferred, explicitly out of this pass as independent extension
  modules (TDD §244-245, "No kernel changes should be permitted merely
  for convenience"): species-counterpoint frameworks (First-Fifth
  Species, TDD Phase 10) and common-practice harmony (Roman numeral
  analysis, harmonic function, voice leading, cadence, tonicization,
  modulation, TDD Phase 11). Both are built as extensions on top of
  `Framework`/`Rule`/`Constraint` after this pass, never as changes to
  the L7 kernel itself.
- Out of scope for L7 entirely, not missing: any actual
  constraint-solving, generation, or analysis pipeline that USES
  `Rule`/`Constraint`/`Preference`/`Heuristic` (`music-analysis`,
  `music-generation`, `music-transform`, L8); and corpus ingestion,
  normalization, or indexing itself (`music-corpus`, L9 -- blocked from
  being imported here regardless, per the OPEN ISSUE above).

## L8: music-transform, music-analysis, music-generation notes

L8 is three sibling modules, built together against Dependency & Layer
Spec v2 §2-§3: `transform` (`Transformation` and its eleven concrete
subtypes: `Transposition`, `Inversion`, `Retrograde`,
`RetrogradeInversion`, `Augmentation`, `Diminution`, `Fragmentation`,
`Expansion`, `Contraction`, `Displacement`, `Ornamentation`),
`analysis` (`Analyzer`, `AnalysisResult`, `FeatureExtraction`, and
eight concrete analyzers: `IntervalAnalysis`, `ScaleAnalysis`,
`ChordIdentification`, `KeyEstimation`, `VoiceLeadingAnalysis`,
`CadenceDetection`, `MotifDetection`, `PhraseSegmentation`), and
`generation` (`Intent`, `IntentCompiler`, `ConstraintExtraction`,
`ConflictDetection`, `GenerationPlan`, `Generator`, `Candidate`/
`CandidateSet`, `CandidateEvaluator`/`CandidateEvaluation`,
`GenerationRecord`). Like L7's `theory`/`rules` split but stricter: all
three L8 modules are mutually sideways-restricted the same way L6's
four siblings are (no `transform`<->`analysis`<->`generation` imports
in any direction) -- each depends on L0-L7 only, never on a sibling.
Each module's architecture test enforces this by allowlisting only
L0-L7 directories plus its own.

DEFERRED, FLAGGED RATHER THAN BUILT (Registry: `Chord` and `Key`
belong to `music-interpretation`, L6, not L8): `ChordIdentification`
and `KeyEstimation` (`analysis/chord_identification.h`,
`analysis/key_estimation.h`) identify a chord/key and report it as a
framework-relative `Interpretation`, but neither constructs a `Chord`
or `Key` object -- there is nothing at L6 yet to construct. This
mirrors `CadenceDetection` not constructing a `Cadence` and
`MotifDetection` not persisting the `MotifOccurrence` it validates
(both report by `Interpretation` and let a downstream caller build the
real L5/L6 object from the claim). Building `Chord`/`Key` themselves is
L6 follow-up work, not part of this L8 branch.

A few more things worth knowing before building on top of L8:

- **`transform`'s Transformation base-type composition pattern.**
  Every concrete subtype (`Transposition`, `Inversion`, ...) owns one
  `Transformation` (`transform/transformation.h`) built internally at
  construction time, rather than a caller assembling the base type
  itself -- `<subtype>_id`/`<subtype>_provenance` forward to the owned
  Transformation rather than duplicating identity/provenance on the
  subtype. `Fragmentation`/`Expansion`/`Contraction` share one
  array-of-`EntityID`-references-plus-count shape (extract/append/drop
  by reference, never by copying `NoteEvent` payloads); `Ornamentation`
  is the one subtype the Registry calls out as needing Provenance most,
  since its one-to-many fan-out (one source event to several derived
  events) is the trickiest case to get right, though every subtype
  carries provenance via the base contract regardless.
  Referentially transparent throughout (TDD §140): no subtype has a
  mutator or a reference back into an input object it could alter.
- **`analysis`'s Analyzer/AnalysisResult explainability pattern.** An
  `Analyzer` (`analysis/analyzer.h`) never returns a bare conclusion --
  it derives owned `Interpretation`/`Evidence` objects (L6) into an
  `AnalysisResult`, which OWNS them (there is nowhere else for them to
  live once `analyze()` returns). Every concrete analyzer follows the
  same "caller supplies the already-measured dimension, the analyzer
  packages it" division: `PitchIdentity` (`pitch/pitch_identity.h`)
  carries no tuning-system semantics, so none of the eight analyzers
  can derive their own measurements (semitone displacement, scale/
  chord/key fit, motion classification, boundary confidence) from raw
  pitch tokens -- each takes that dimension as an already-computed
  input field and reports it through the Interpretation/Evidence/
  Uncertainty architecture.
- **`generation`'s Intent-only entry point.** Natural language MUST NOT
  directly manipulate the IR (TDD §154) -- `Intent`
  (`generation/intent.h`) is the only type a natural-language request
  compiles into (via `IntentCompiler`, a contract-only interface with
  no NLP implementation in this pass), and every downstream module
  (`ConstraintExtraction`, `GenerationPlan`, `Generator`, ...) consumes
  `Intent`, never raw text.
- **`ConflictDetection` never silently drops a conflict (TDD §156).**
  `generation/conflict_detection.h` requires at least two conflicting
  ids per record (a conflict cannot be reported about a single
  requirement) and keeps `priority`/`severity`/`weight` as three
  distinct fields, not synonyms (TDD §157): a rank, a fixed-vocabulary
  category, and an exact-rational magnitude, independently settable.
- **`Generator`/`CandidateSet` forward-declaration, resolved in the
  same module.** `generation/generator.h` forward-declares
  `CandidateSet` as an opaque `struct CandidateSet` (Generator's own
  contract -- what it declares about itself, TDD §160 -- does not
  depend on CandidateSet's layout); `generation/candidate.h` supplies
  the real definition under the identical struct tag.
  `tests/generation/test_candidate.c` exercises both headers together
  to confirm they resolve to one type, not two conflicting ones.
- **`CandidateEvaluator` reuses `Interpretation`, not `AnalysisResult`
  (TDD §159).** `CandidateEvaluation` (`generation/candidate_evaluator.h`)
  holds the identical evaluations/violations-as-`Interpretation`-arrays
  shape `AnalysisResult` gives its own findings, but is defined
  independently in `generation/` rather than importing
  `analysis/analyzer.h` -- the L8 sideways-restriction above forbids
  that import regardless of the shape being identical.
- **`GenerationRecord` is distinct from `CandidateSet`'s and
  `Generator`'s own seed/version fields (TDD §163).**
  `generation/generation_record.h` is the durable, standalone log entry
  that a generation run happened -- `CandidateSet` carries the same
  reproducibility bundle (seed, `generator_version`, `configuration`,
  `input_ids`, `framework_version`) scoped to the one `generate()` call
  that produced it, and `Generator`'s own `version` field is a declared
  capability, present whether or not the generator was ever invoked.
- **`framework_version` is a KNOWN GAP, not fabricated.** TDD §163
  requires it on `CandidateSet` and `GenerationRecord`, but `Framework`
  (`theory/framework.h`, L7) carries no version field of its own yet --
  the same unresolved-dependency treatment the L7 notes above give
  `Interpretation`'s missing `framework` field. Both types keep it as
  an optional, opaque string (`NULL` when not available) rather than
  block on that gap.
- Deferred, explicitly out of this pass (Registry Normative = No,
  "Phase-list mention only, no dedicated contract" / reference concept
  only): `ConstraintSolver`, `Search`/`Optimizer`.
- Deferred, wrong phase for this pass (Registry Phase 13, not 12):
  Hierarchical Generators (`ThemeGenerator`, `PhraseGenerator`,
  `BasicIdeaGenerator`).
- Deferred, wrong layer for this pass (`music-io`/`music-notation`/
  `music-performance`/`music-corpus`, L9 Applications, not L8):
  `PracticeRequest`/`PracticeGenerator`, `DifficultyModel`.
- Out of scope for L8 entirely, not missing: actual I/O format adapters
  (MIDI/MusicXML/MEI/ABC, L9); corpus ingestion/statistics itself (L9,
  and separately blocked from being imported here regardless, per the
  L7 OPEN ISSUE above -- `CandidateEvaluator` consuming a
  `StatisticalModel` is fine, that is L8 -> L7, legal; nothing in L8
  reaches past L7 into `music-corpus` directly); and species
  counterpoint / common-practice harmony frameworks (Phase 10/11 --
  these consume `Framework`/`Rule`/`Constraint` as extension modules,
  not part of L8's own build).