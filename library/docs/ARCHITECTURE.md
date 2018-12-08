# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/`
with its tests mirrored under `tests/<module>/`.

## Layers

- **L0** - no dependencies on any other module. Currently: `identity`.
- **L1+** - may depend only on lower layers, never sideways or upward.
  Currently: `core` (music-core), which depends on `identity` only.

A module must not import anything outside its own layer's allowed
dependencies. Each module has an architecture test enforcing this
(e.g. `tests/identity/test_architecture_identity.c`,
`tests/core/test_architecture_core.c`).

## Modules

| Module     | Layer | Path             | Tests                |
|------------|-------|------------------|-----------------------|
| identity   | L0    | `src/identity/`  | `tests/identity/`     |
| core       | L1    | `src/core/`      | `tests/core/`         |

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