# core

Layer L1 (`music-core`). Path `src/core/`, tests `tests/core/`. Depends on `identity` only.

## Provides

`Entity`, `Value`, `Occurrence`, the five equality concepts (identity, value, structural, semantic, canonical), four reference types (`VoiceReference`, `RelationshipReference`, `ProvenanceReference`, `FrameworkReference`), and the exact `Rational` (`core/rational.h`).

## Notes

- The Entity/Occurrence/Event category boundary is still contested upstream (Domain Spec, System Registry). `Entity` and `Occurrence` are implemented without assuming how that boundary settles, and code built on them should do the same.
- `SemanticEquality` is a contract only at this layer (`SemanticEqualsFn` in `core/equality.h`). A real comparison needs `Context` (L6, now built); no concrete `SemanticEqualsFn` is provided yet.
- `Entity` carries a required `provenance_reference` field (`ProvenanceReference`, `core/provenance_reference.h`). It is required at construction — `entity_create` rejects a null provenance with `ENTITY_ERR_NULL_PROVENANCE` — and does not factor into `entity_identity_equals`: provenance is lineage, not identity (Law 8/Domain Spec), the same rule `events/note_event.h`'s `note_event_equals_by_id` follows.
- The "Canonical Musical Model / IR" described in the System Registry is not a module `core` imports. It is the graph formed when these L1 base types are combined with higher-layer objects at consumption time. There is nothing to import here to satisfy that row.
- `VoiceReference`, `RelationshipReference`, and `ProvenanceReference` are implemented here. Domain Ontology Specification v0.2 once marked all three "proposed only"; all three have since been authorized in the System Registry, so there is no longer a spec gap.
- `RelationshipReference` currently has no consumer anywhere in the codebase. It exists as a type with no module holding one yet.
- `FrameworkReference` follows the same opaque-EntityID-wrapper shape as the other three. Its consumer is `interpretation`'s `Interpretation.framework_reference` field. See `docs/modules/interpretation.md`.
- `core/string_copy.h` holds `ursatz_string_copy`, the one heap string copy for modules that may include `core/` (NULL in, NULL out; NULL on allocation failure). `corpus/` may not include `core/`, so its two copies stay local.
- `core/rational.h` holds `Rational`: signed, `den > 0`, lowest terms, zero is `0/1`. `rational_make`, `rational_add`, `rational_sub`, `rational_mul`, `rational_div`, `rational_neg`, `rational_mul_int`, `rational_div_int`, `rational_floor` and `rational_ceil` return a `RationalResult` and write `*out` only on `RATIONAL_OK`; `LLONG_MIN` in any part is overflow. `rational_cmp` (Euclid's method) never multiplies and cannot overflow. `rational_gcd_unsigned`, `rational_cmp_unsigned` and the checked unsigned steps serve types whose stored fields use the full unsigned range. `rational_ratio` is for arguments already known valid (invalid yields `0/1`). `__int128` is used for products only where the compiler has it; behavior is identical either way.

## Dependencies

Depends on `identity` (L0) only.