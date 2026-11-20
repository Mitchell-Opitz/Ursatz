# Docs index

Every Markdown doc in the repository, by topic. [Back to the repository README](../README.md)

## Start here

- [README.md](../README.md): What Ursatz is and how to build and use it.
- [docs/architecture.md](architecture.md): What the library is for, its module map, public API and data flow.
- [docs/layers-and-dependency-rules.md](layers-and-dependency-rules.md): The five groups, the dependency rule, open and resolved dependency issues.
- [docs/public-surface.md](public-surface.md): Which headers each library group stages, the narrowing rule and the tests that guard it.
- [docs/placement-rule.md](placement-rule.md): Where an analysis algorithm lives: model, theory, analysis, chain or app.
- [docs/vocabulary-enums.md](vocabulary-enums.md): Shared theory enums in theory/vocabulary and when a new one is admitted.
- [docs/module-status.md](module-status.md): Implementation status and doc of each kernel module.
- [theory/README.md](../theory/README.md): The theory group: its areas, the former framework each holds, the catalog of the 11 Theory Frameworks and their shared conventions.
- [analysis/README.md](../analysis/README.md): The analysis group: its areas and the former framework adapters each holds.
- [chain/README.md](../chain/README.md): The analysis chain: stages, owners and how to call it.

## Build and test

- [docs/BUILDING.md](BUILDING.md): Requirements, build and test commands, CI.
- [docs/testing/accuracy.md](testing/accuracy.md): The accuracy harness: what it measures and the baseline gate.
- [docs/testing/framework_v1_verification.md](testing/framework_v1_verification.md): The Framework v1 reference set and how each piece is verified.

## Using it

- [docs/shared-primitives.md](shared-primitives.md): Small public helpers every layer reuses (rationals, meter arithmetic, pitch arithmetic, import notes).
- [docs/modules/midi_renderer.md](modules/midi_renderer.md): Writing canonical events back to a Standard MIDI File.

## API reference

- [docs/modules/identity.md](modules/identity.md): L0 identity: EntityID, Version, SchemaVersion.
- [docs/modules/core.md](modules/core.md): L1 core: values, rationals, references.
- [docs/modules/time.md](modules/time.md): L2 time: musical time, meter map, tempo map.
- [docs/modules/pitch.md](modules/pitch.md): L2 pitch: identity, spelling, MIDI numbers, key signatures.
- [docs/modules/events.md](modules/events.md): L3 events: the canonical NoteEvent.
- [docs/modules/containers.md](modules/containers.md): L4 containers: sections and phrases.
- [docs/modules/structure.md](modules/structure.md): L5 structure: occurrences of structures.
- [docs/modules/context.md](modules/context.md): L6 context: the opaque Context type.
- [docs/modules/semantics.md](modules/semantics.md): L6 semantics: shared theory vocabulary and neutral analysis inputs.
- [docs/modules/theory.md](modules/theory.md): L7 theory: Theory Framework type, style models.
- [docs/modules/knowledge.md](modules/knowledge.md): L7 knowledge: the snapshot of knowledge entries, the instrument kind.
- [docs/modules/realization.md](modules/realization.md): Realization: lines, performer parts, scoring assignments.
- [docs/modules/reading.md](modules/reading.md): Reading: what one reader says about one realization, as claims; the builder and the JSON codec.
- [text/README.md](../text/README.md): Text: Ursatz Text, the lossless text form of plans, readings, realizations and knowledge (grammar 1, codecs, API).
- [docs/modules/plan.md](modules/plan.md): Plan: the plan (nodes, lanes, constraints, states, edits, the JSON codec) and the section description.
- [docs/modules/plan-format.md](modules/plan-format.md): the plan JSON file, its nodes, lanes, state and versions.
- [docs/modules/rules.md](modules/rules.md): L7 rules: Rule, Constraint, Preference and their callbacks.
- [docs/modules/analysis.md](modules/analysis.md): L8 analysis: overview of the analyzers and what is deferred.
- [docs/modules/io.md](modules/io.md): L9 io: importers, key signature scans, neutral import notes.
- [docs/modules/export.md](modules/export.md): L9 export: MusicXML writer.
- [docs/modules/query.md](modules/query.md): L9 query: similarity scoring.
- [docs/modules/motive-link.md](modules/motive-link.md): Motive links: chains of relations between motives, choosing and grouping.
- [docs/modules/motive-relations.md](modules/motive-relations.md): Motive sketches and relations.
- [docs/modules/motive-variant-recognition.md](modules/motive-variant-recognition.md): Recognizing varied statements of a motive.
- [docs/modules/motive-tone-roles.md](modules/motive-tone-roles.md): Tone roles of a motive against a harmony.
- [docs/modules/standard-harmony-rules.md](modules/standard-harmony-rules.md): standard-harmony rules: function, cadences, predominants, six-fours.
- [docs/modules/standard-harmony-prolongation-and-keys.md](modules/standard-harmony-prolongation-and-keys.md): standard-harmony rules: prolongation, plagal, tonicization, modulation.
- [docs/modules/standard-counterpoint-species.md](modules/standard-counterpoint-species.md): standard-counterpoint rules for the first four species.
- [docs/modules/standard-form-expansion.md](modules/standard-form-expansion.md): standard-form hybrid, length and expansion rules.
- [docs/modules/standard-meter-metric-position.md](modules/standard-meter-metric-position.md): standard-meter metric position rules.
- [docs/modules/standard-tonality-chords.md](modules/standard-tonality-chords.md): standard-tonality triad and seventh quality rules.
- [docs/modules/standard-tonality-numerals-and-keys.md](modules/standard-tonality-numerals-and-keys.md): standard-tonality Roman numerals, adapter and key functions.
- [docs/modules/common-practice-minimal-weighted.md](modules/common-practice-minimal-weighted.md): common-practice-minimal weighted identification and chords in context.
- [analysis/harmony/README.md](../analysis/harmony/README.md): Diatonic triads and sevenths, major and minor keys, scope and boundaries.
- [theory/galant/README.md](../theory/galant/README.md): Galant schema prototypes.
- [theory/galant/sequences.md](../theory/galant/sequences.md): Rule of the octave and sequences over a bass scale.
- [theory/counterpoint/README.md](../theory/counterpoint/README.md): Species counterpoint framework: scope, adding a rule module, Fux conformance.
- [theory/form/README.md](../theory/form/README.md): Form framework: phrases, sentence, period.
- [theory/harmony/README.md](../theory/harmony/README.md): Harmony framework: scope list and what is not covered.
- [theory/markings/README.md](../theory/markings/README.md): Dynamics and tempo rules.
- [theory/meter/README.md](../theory/meter/README.md): Meter framework: scope, accented beats, what is not covered.
- [analysis/texture/README.md](../analysis/texture/README.md): Texture rules and classification.
- [theory/tonality/README.md](../theory/tonality/README.md): Tonality framework: key functions, chord qualities, Roman numerals.
- [theory/voicing/README.md](../theory/voicing/README.md): SATB voicing of one chord.

## Formats and schemas

- Plan, format 6: [docs/modules/plan-format.md](modules/plan-format.md), schema [plan/schema/plan.v6.schema.json](../plan/schema/plan.v6.schema.json).
- Realization, format 1 (`ursatz-realization`): [docs/modules/realization.md](modules/realization.md), schema [realization/schema/realization.v1.schema.json](../realization/schema/realization.v1.schema.json).
- Reading, format 1 (`ursatz-reading`): [docs/modules/reading.md](modules/reading.md), schema [reading/schema/reading.v1.schema.json](../reading/schema/reading.v1.schema.json).
- [docs/modules/export.md](modules/export.md): The MusicXML output format and its limits.
- [docs/modules/io-midi.md](modules/io-midi.md): The MIDI input format reading.
- [docs/modules/io-musicxml.md](modules/io-musicxml.md): The MusicXML input subset.

## How it works inside

- [docs/analytic-job-owners.md](analytic-job-owners.md): The owner of each analytic job, and the production chain.
- [docs/theory-owners.md](theory-owners.md): The library owner of each piece of theory the apps used to copy.
- [docs/modules/analysis-voices.md](modules/analysis-voices.md): Outer voices, soprano at every attack, voice separation.
- [docs/modules/analysis-phrase-form.md](modules/analysis-phrase-form.md): Phrase boundaries, period, form structure, thematic return, the phrase grid.
- [docs/modules/common-practice-minimal-analysis.md](modules/common-practice-minimal-analysis.md): Chord identification, outer scale degrees and key regions in the kernel.
- [docs/modules/standard-harmony-analysis.md](modules/standard-harmony-analysis.md): The standard-harmony analysis adapter.
- [docs/modules/standard-tonality-analysis.md](modules/standard-tonality-analysis.md): The standard-tonality analysis adapter.
- [docs/modules/form-analysis.md](modules/form-analysis.md): The standard-form analysis adapter: phrases, ideas, themes, expansion.
- [docs/modules/analysis-repeats.md](modules/analysis-repeats.md): Written-out repeats and melodic returns over measures; the phrase grid follows repeats.
- [docs/modules/motive-analysis.md](modules/motive-analysis.md): The standard-form motive adapter.
- [docs/modules/melody-analysis.md](modules/melody-analysis.md): The standard-counterpoint melody adapter.
- [docs/modules/voice-leading-analysis.md](modules/voice-leading-analysis.md): The voice-leading adapters over a voice-pitch grid.
- [docs/modules/rhythm-meter-analysis.md](modules/rhythm-meter-analysis.md): The standard-meter analysis adapter.
- [docs/modules/texture-analysis.md](modules/texture-analysis.md): The standard-texture analysis adapter.
- [docs/modules/schema-sequence-analysis.md](modules/schema-sequence-analysis.md): Schema and sequence matching.
- [docs/modules/l6-notes.md](modules/l6-notes.md): Shared notes on the four L6 modules.
- [docs/modules/l8-notes.md](modules/l8-notes.md): Shared notes on the L8 modules.

## Limits and changes

- [third_party/ezxml/README.md](../../third_party/ezxml/README.md): The vendored ezxml XML parser, its license and use (in the repository's `third_party/`).
