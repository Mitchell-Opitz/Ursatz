# Layers and dependency rules

The layer table, the dependency rule that holds it together, and the open and resolved dependency issues. [Docs index](README.md)

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
| L8 | `analysis`, `generation` (two mutually-restricted siblings) |
| L9 | `io` (MIDI import, MIDI key-signature scan), `export` (MusicXML), `persistence` (SQLite), `corpus`, `corpus_stats`, `query` |
| Extensions | `frameworks/<name>/`: 17 Theory Frameworks built on L7 (see `frameworks/README.md`); `chain/`: the analysis chain (`ursatz_chain`), above the kernel and the frameworks |

## Dependency rule

A module in layer N may depend only on modules in layers 0 through N. It must never depend sideways within its own layer unless that specific dependency is documented, and never depend upward on a higher layer. The documented sideways edges are `rules` on `theory` (L7), `interpretation` on `relationship` through `claim_interpretation.h` (L6), and in L9 `export`, `corpus`, `corpus_stats` and `query` on `persistence` plus `corpus_stats` and `query` on `corpus`. No `src/` module includes a framework header. Frameworks never compile kernel sources (`architecture_framework_sources` checks every `frameworks/*/CMakeLists.txt` and `chain/CMakeLists.txt`); code that needs both a framework and the kernel lives in `chain/`. The former exception, `analysis`'s `ChordIdentification` and `KeyEstimation`, moved into the chain (R-010 finished, R-241).

The analysis chain (`chain/`, target `ursatz_chain`) is an extension above the numbered stack. It may include kernel L0 to L8 and the framework headers named in `chain/tests/test_architecture_chain.c`, and never L9 (`io`, `export`, `persistence`, `corpus`, `corpus_stats`, `query`). Headers are included as `ursatz_chain/<name>.h`. A framework test may link the chain; a framework library may not. The chain keeps the stage inputs it builds (pitches, harmony inputs, prepared harmony, form and motive inputs, voice grid, galant segments) and exposes them through `analysis_chain_*` accessors, and it reads the outer lines on the chord segments (`chain_build_segment_lines`, `analysis_chain_segment_outer_voices`) alongside the lines on the voice segments (`analysis_chain_outer_voices`).

A framework may depend on L0 to L6, on `theory/framework.h`, `theory/framework_singleton.h`, `rules/rule.h`, `rules/constraint.h`, `rules/preference.h` and `rules/rule_pair.h`, and on another framework only where that edge is recorded in `frameworks/README.md`.

Every module and every framework has an architecture test that allowlists the directories it may include (for example `tests/events/test_architecture_events.c`). A disallowed include fails that test. Framework architecture tests run from the root build like the module ones (since PR #99); see `docs/BUILDING.md`. `tests/test_architecture_tests.c` applies the same layer rules to the test sources (`tests/<module>/` and `frameworks/<name>/tests/`); the test-only edges it allows are listed in its `ALLOWED_TEST_EDGES`, one reason each. Two of them reach kernel L8 from a framework test: `common-practice-minimal` tests include `analysis/outer_voice.h` and `standard-harmony` tests include `analysis/period_detection.h`; framework sources never include L8.

## Open issues

- **Theory to Corpus** (Dependency Spec §7.2). The Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` (L7) as depending on `Corpus` (L9). All three accept corpus-derived data only as an opaque, already-computed value; no L7 file includes a corpus header. Detail in `docs/modules/theory.md`.

## Resolved issues

- **Interpretation-family to Framework** (Dependency Spec §7.1, closed). `Interpretation`, `ScaleDegree`, `Chord`, and `Key` (L6) carry an optional `FrameworkReference` (`core`, L1) rather than a live `Framework` reference. `Chord`, `Key`, `ScaleDegree` and `Texture` require it at construction. Detail in `docs/modules/l6-notes.md`.
- **Reference-type Registry synchronization.** `VoiceReference`, `RelationshipReference`, and `ProvenanceReference` are authorized in the System Registry and implemented in `core`. See `docs/modules/core.md`.
