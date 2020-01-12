# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/` with its tests mirrored under `tests/<module>/`. This page covers the layer model and the rules that hold it together. Per-module detail (what each module provides, what is deferred, what to know before building on it) lives under `docs/modules/`.

## Layers

| Layer | Modules |
|---|---|
| L0 | `identity` |
| L1 | `core` |
| L2 | `time`, `pitch` (siblings, do not depend on each other) |
| L3 | `events` |
| L4 | `containers` |
| L5 | `structure` |
| L6 | `context`, `semantics`, `relationship`, `interpretation` (four mutually-restricted siblings) |
| L7 | `theory`, `rules` (`rules` depends on `theory`; `theory` never depends back) |
| L8 | `transform`, `analysis`, `generation` (three mutually-restricted siblings) |
| L9 | `io` (partial: `MIDIImporter` only, other L9 concepts not yet implemented), `persistence` (partial: minimal SQLite storage for NoteEvent/Chord/Key only, not the full Repository contract) |

## Dependency rule

A module in layer N may depend only on modules in layers 0 through N. It must never depend sideways within its own layer unless that specific dependency is documented (as `rules` on `theory` is), and never depend upward on a higher layer.

Every module has an architecture test enforcing this by allowlisting only the directories it is permitted to import (for example `tests/events/test_architecture_events.c`). A module that imports outside its allowlist fails the build.

## Open issues

One forward-dependency contradiction between the layer model and the System Registry is tracked but not resolved. It is not implemented; the guardrail is enforced by the relevant architecture tests.

- **Theory to Corpus** (Dependency Spec §7.2). The Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` (L7) as depending on `Corpus` (L9, not yet implemented). All three accept corpus-derived data only as an opaque, already-computed value. Detail in `docs/modules/theory.md`.

## Resolved issues

- **Interpretation-family to Framework** (Dependency Spec §7.1, closed). The Registry lists `Interpretation`, `ScaleDegree`, `Chord`, and `Key` (L6) as depending on `Framework` (L7). Per System Registry v3, `ScaleDegree`, `Chord`, and `Key` remain deferred whole (they do not exist in this codebase); `Interpretation` now carries an optional `FrameworkReference` (`core`, L1) rather than a live `Framework` reference. Detail in `docs/modules/l6-notes.md`.

## Reference-type Registry synchronization gap

Domain Ontology Specification v0.2 marks three L1 reference types, `VoiceReference`, `RelationshipReference`, and `ProvenanceReference`, as proposed only, blocked pending System Registry synchronization that has not happened. All three are implemented in `core` regardless. See `docs/modules/core.md`.

## Module status

| Module | Layer | Status | Doc |
|---|---|---|---|
| identity | L0 | Implemented | `docs/modules/identity.md` |
| core | L1 | Implemented | `docs/modules/core.md` |
| time | L2 | Implemented | `docs/modules/time.md` |
| pitch | L2 | Implemented (partial, see doc) | `docs/modules/pitch.md` |
| events | L3 | Implemented (partial, see doc) | `docs/modules/events.md` |
| containers | L4 | Implemented (partial, see doc) | `docs/modules/containers.md` |
| structure | L5 | Implemented (partial, see doc) | `docs/modules/structure.md` |
| context | L6 | Implemented | `docs/modules/context.md` |
| semantics | L6 | Implemented (partial, see doc) | `docs/modules/semantics.md` |
| relationship | L6 | Implemented | `docs/modules/relationship.md` |
| interpretation | L6 | Implemented (partial, see doc) | `docs/modules/interpretation.md` |
| theory | L7 | Implemented (partial, see doc) | `docs/modules/theory.md` |
| rules | L7 | Implemented (partial, see doc) | `docs/modules/rules.md` |
| transform | L8 | Implemented | `docs/modules/transform.md` |
| analysis | L8 | Implemented | `docs/modules/analysis.md` |
| generation | L8 | Implemented (partial, see doc) | `docs/modules/generation.md` |
| io | L9 | Implemented (partial, see doc) | `docs/modules/io.md` |
| persistence | L9 | Implemented (partial, see doc) | `docs/modules/persistence.md` |

"Partial" means some types from the module's Registry row are implemented and some are explicitly deferred; the module's own doc lists which.

## Build and test

See `docs/BUILDING.md`.
