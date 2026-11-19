# Layers and dependency rules

The layer table, the dependency rule that holds it together, and the open and resolved dependency issues. [Docs index](README.md)

## Groups

The library is seven groups, each one folder under `library/` and one CMake target (R-546, R-578). Inside a group, one folder per area.

| Group | Target | Areas |
|---|---|---|
| `model/` | `ursatz_model` | `identity`, `core`, `time`, `pitch`, `events`, `containers`, `structure`, `relationship`, `interpretation`: the value types (the former layers L0 to L6 without `context` and `semantics`, in that order) |
| `theory/` | `ursatz_theory` | `framework` (the Framework type and singleton), `style`, `rules` (Rule, Constraint, Preference, Context), `vocabulary` (shared theory enums), `tonality`, `harmony`, `voicing`, `counterpoint`, `form`, `meter`, `galant`, `markings`: the rule libraries and vocabularies |
| `realization/` | `ursatz_realization` | lines, performer parts, scoring assignments, movements and works, one folder, on `model` and `theory` |
| `plan/` | `ursatz_plan` | the section description shared by forms, plans and piece requests, one folder, on `model` and `theory` (and cJSON for its codec) |
| `analysis/` | `ursatz_analysis` | `notes` (the neutral analysis types and segmentation), `motive`, `voices`, `harmony`, `form`, `meter`, `texture`, `counterpoint`, `galant`, `similarity`: the analyses and the framework adapters |
| `chain/` | `ursatz_chain` | the analysis chain, one folder |
| `io/` | `ursatz_io` | MIDI and MusicXML import, MIDI and MusicXML export |

Headers are included by their path from `library/` (`model/time/meter_map.h`, `theory/harmony/harmony_chord.h`, `analysis/motive/motive_sketch.h`, `chain/analysis_chain.h`, `io/midi_importer.h`).

## Dependency rule

The groups depend downward only: `model`, then `theory` (on `model`), then `realization` and `plan` (each on `model` and `theory`, neither on the other), then `analysis` (on `theory`), then `chain` (on `analysis`); `io` depends on `model` alone and nothing depends on `io` but the chain and the applications. Inside `model` the old layer order holds: `identity`, `core`, `time` and `pitch`, `events`, `containers`, `structure`, `relationship` and `interpretation`, each only on the ones before it (`interpretation` on `relationship` through `claim_interpretation.h`). Inside `theory`, `rules` and the area libraries depend on `framework` and `vocabulary`; `harmony` on `rules`; `form` on `harmony`; `galant` on `counterpoint`. Inside `analysis`, every area depends on `notes`; `form` on `motive`; `voices` on `meter`. The former frameworks are gone as targets: their rule libraries are theory areas and their adapters are analysis areas. Their identities stay (`standard-harmony-v1` and the others), one `Framework` singleton per former framework ([theory/README.md](../theory/README.md), [analysis/README.md](../analysis/README.md)).

Above the library are the applications, and the two apps are siblings (R-620): the Analyzer and the Composer both build on the library and neither includes or links the other; only the CLIs and the GUI call both. The Composer hears through the chain (`analysis_chain_run_score` on an `imported_score_view`), the Analyzer runs it on a file. The layer table's `uses` rows and the `forbid composer ursatz_analyzer` rule enforce it.

The analysis chain (`chain/`, target `ursatz_chain`) joins `analysis` and everything below it; a theory or analysis file never includes it. Headers are included as `chain/<name>.h`. The chain keeps the stage inputs it builds (harmony inputs, prepared harmony, form and motive inputs, voice grid, galant segments) and exposes them through `analysis_chain_*` accessors, and it reads the outer lines on the chord segments (`chain_build_segment_lines`) alongside the lines on the voice segments (`analysis_chain_outer_voices`).

The rules are enforced by `cmake/layer-table.txt` (test `layer_table`, in the fast set): every C file belongs to a module, each module row lists what it may include, with a reason for each exception, and a disallowed include fails the test. The `target` rows name the modules one CMake target builds; an include between two targets (or from an application) must name a staged header, and a staged header that nothing outside its target includes fails as well ([public-surface.md](public-surface.md)). Test sources follow the same rules (the `tests-*` rows, one per test folder).

## Open issues

- **Theory to Corpus** (Dependency Spec §7.2). The Registry lists `StyleModel` (L7) as depending on `Corpus` (L9). It accepts corpus-derived data only as an opaque, already-computed value; no L7 file includes a corpus header. Detail in `docs/modules/theory.md`.

## Resolved issues

- **Interpretation-family to Framework** (Dependency Spec §7.1, closed). `Interpretation`, `Chord`, and `Key` (L6) carry an optional `FrameworkReference` (`core`, L1) rather than a live `Framework` reference. `Chord`, `Key` and `Texture` require it at construction. Detail in `docs/modules/l6-notes.md`.
- **Reference-type Registry synchronization.** `VoiceReference` and `ProvenanceReference` are authorized in the System Registry and implemented in `core`. See `docs/modules/core.md`.
