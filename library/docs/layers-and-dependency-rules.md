# Layers and dependency rules

The layer table, the dependency rule that holds it together, and the open and resolved dependency issues. [Docs index](README.md)

## Groups

The library is eight groups, each one folder under `library/` and one CMake target (R-546, R-578). Inside a group, one folder per area.

| Group | Target | Areas |
|---|---|---|
| `model/` | `ursatz_model` | `identity`, `core`, `time`, `pitch`, `events`, `containers`, `structure`: the value types (the former layers L0 to L6 without `context` and `semantics`, in that order) |
| `theory/` | `ursatz_theory` | `style`, `rules` (the rule table), `vocabulary` (shared theory enums), `tonality`, `harmony`, `voicing`, `counterpoint`, `form`, `meter`, `galant`, `markings`: the rule libraries and vocabularies |
| `realization/` | `ursatz_realization` | lines, performer parts, scoring assignments, movements and works, one folder, on `model` and `theory` |
| `plan/` | `ursatz_plan` | the section description shared by forms, plans and piece requests, one folder, on `model` and `theory` (and cJSON for its codec) |
| `analysis/` | `ursatz_analysis` | `notes` (the neutral analysis types and segmentation), `motive`, `voices`, `harmony`, `form`, `meter`, `texture`, `counterpoint`, `galant`, `similarity`, `dimension`: the analyses and the framework adapters |
| `io/` | `ursatz_io` | MIDI and MusicXML import (also as a realization: `imported_realization.h`), MIDI and MusicXML export |
| `chain/` | `ursatz_chain` | the analysis chain, one folder |
| `reading/` | `ursatz_reading` | what one reader says about one realization, as claims (regions, chords, cadences, groups, motive occurrences, meters, lines, voice faults), the builder from a chain run, the JSON codec and the conversion into a plan; above `chain` and `plan` |

Headers are included by their path from `library/` (`model/time/meter_map.h`, `theory/harmony/harmony_chord.h`, `analysis/motive/motive_sketch.h`, `chain/analysis_chain.h`, `io/midi_importer.h`).

## Dependency rule

The groups depend downward only: `model`, then `theory` (on `model`), then `realization` and `plan` (each on `model` and `theory`, neither on the other), then `analysis` (on `theory`), then `io` (on `model` and `realization`), then `chain` (on `analysis` and `io`), then `reading` (on `realization`, `plan`, `analysis` and `chain`); nothing depends on `reading` but the applications. There are no claim modules below it: the stages write typed results and a reading is the one place claims live. Inside `model` the old layer order holds: `identity`, `core`, `time` and `pitch`, `events`, `containers`, `structure`, each only on the ones before it. Inside `theory`, `rules` and the area libraries depend on `framework` and `vocabulary`; `harmony` on `rules`; `form` on `harmony`; `galant` on `counterpoint`. Inside `analysis`, every area depends on `notes`; `form` on `motive`; `voices` on `meter`. The former frameworks are gone as targets: their rule libraries are theory areas and their adapters are analysis areas. Their identities stay (`standard-harmony-v1` and the others), one `Framework` singleton per former framework ([theory/README.md](../theory/README.md), [analysis/README.md](../analysis/README.md)).

Above the library are the applications, and the two apps are siblings (R-620): the Analyzer and the Composer both build on the library and neither includes or links the other; only the CLIs and the GUI call both. The Composer hears through the chain (`analysis_chain_run_score` on an `imported_score_view`), the Analyzer runs it on a file. The layer table's `uses` rows and the `forbid composer ursatz_analyzer` rule enforce it.

Under everything is `common/` (target `ursatz_common`, R-638): the status set, file and folder helpers, strings, JSON, the hash, the job handle and the event bus, on libc and cJSON only ([common/docs](../../common/docs/README.md)). The library, both apps, the workshop and the GUI include it; it includes none of them. Above the two apps is the workshop (`workshop/`, R-641): the one process that holds the data folder, the jobs and the events, with the one header `ursatz_workshop/workshop_api.h`; it calls the Analyzer and the Composer through their one API header each and nothing else of theirs ([workshop docs](../../workshop/docs/architecture.md)). The GUI is above the workshop and reaches it only through `workshop_api.h` (with `common/`, civetweb and cJSON): it includes no Analyzer, Composer or library header. Each application exports only its API (`analyzer_*`, `composer_api_*`, `workshop_*`), checked by the test `app_exports`; the module rows in `cmake/layer-table.txt` are `common`, `analyzer-api`, `composer-api`, `workshop-api` and a module per workshop folder.

The analysis chain (`chain/`, target `ursatz_chain`) joins `analysis` and everything below it; a theory or analysis file never includes it. Headers are included as `chain/<name>.h`. The chain keeps the stage inputs it builds (harmony inputs, prepared harmony, form and motive inputs, voice grid, galant segments) and exposes them through `analysis_chain_*` accessors, and it reads the outer lines on the chord segments (`chain_build_segment_lines`) alongside the lines on the voice segments (`analysis_chain_outer_voices`).

The rules are enforced by `cmake/layer-table.txt` (test `layer_table`, in the fast set): every C file belongs to a module, each module row lists what it may include, with a reason for each exception, and a disallowed include fails the test. A `uses` token that no include of the module's own files needs fails it too (delete the token); test modules (every path in a `tests` folder) and `free` modules are exempt, and a token a test module reaches through `like` is listed on that test module's row. The `target` rows name the modules one CMake target builds; an include between two targets (or from an application) must name a staged header, and a staged header that nothing outside its target includes fails as well ([public-surface.md](public-surface.md)). Test sources follow the same rules (the `tests-*` rows, one per test folder).

## Open issues

- **Theory to Corpus** (Dependency Spec §7.2). The Registry lists `StyleModel` (L7) as depending on `Corpus` (L9). It accepts corpus-derived data only as an opaque, already-computed value; no L7 file includes a corpus header. Detail in `docs/modules/theory.md`.

## Resolved issues

- **Interpretation-family to Framework** (Dependency Spec §7.1, closed by P62 B11). The `Interpretation`, `Chord`, `Key`, `Texture` and `Relationship` types and the claims built from them were deleted: the stages write typed results, the vocabulary gives `ChordCandidate` and `KeyStructure` values, and what a reader says about a realization is a reading (`library/reading`).
- **Reference-type Registry synchronization.** `VoiceReference` and `ProvenanceReference` are authorized in the System Registry and implemented in `core`. See `docs/modules/core.md`.
