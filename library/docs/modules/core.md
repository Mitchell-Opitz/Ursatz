# core

Layer L1 (`music-core`). Path `src/core/`, tests `tests/core/`. Depends on `identity` only.

## Provides

`Entity`, `Value`, `Occurrence`, the five equality concepts (identity, value, structural, semantic, canonical), and four reference types (`VoiceReference`, `RelationshipReference`, `ProvenanceReference`, `FrameworkReference`).

## Notes

- The Entity/Occurrence/Event category boundary is still contested upstream (Domain Spec, System Registry). `Entity` and `Occurrence` are implemented without assuming how that boundary settles, and code built on them should do the same.
- `SemanticEquality` is a contract only at this layer (`SemanticEqualsFn` in `core/equality.h`). A real comparison needs `Context`, an L6 concept, so it is deferred until that layer exists.
- `Entity`'s `provenance_reference` field from the System Registry is not wired yet. `ProvenanceReference` exists as a type (`core/provenance_reference.h`), but attaching it to `Entity` is separate follow-up work.
- The "Canonical Musical Model / IR" described in the System Registry is not a module `core` imports. It is the graph formed when these L1 base types are combined with higher-layer objects at consumption time. There is nothing to import here to satisfy that row.
- `VoiceReference`, `RelationshipReference`, and `ProvenanceReference` are implemented here. Domain Ontology Specification v0.2 marks all three "PROPOSED ONLY," blocked pending System Registry synchronization, and states implementation "MUST NOT" proceed until that sync happens. That sync has not happened; the repo implements them anyway. Flagged here as a known gap between spec status and code, not resolved by this note.
- `RelationshipReference` currently has no consumer anywhere in the codebase. It exists as a type with no module holding one yet.
- `FrameworkReference` follows the same opaque-EntityID-wrapper shape as the other three. Its consumer is `interpretation`'s `Interpretation.framework_reference` field. See `docs/modules/interpretation.md`.

## Dependencies

Depends on `identity` (L0) only.
