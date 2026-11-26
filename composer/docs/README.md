# Composer docs

Index of every Composer doc, grouped by what you want to do. Back to the [repository README](../README.md).

## Start here

- [architecture.md](architecture.md): what the Composer is, its module map, public API, versions, what it calls below and the data flow of a request.
- [structure.md](structure.md): the source directories, the CLI snapshot tests and the file scope state check.

## Build and test

- [building.md](building.md): the slow tests, the public header rule and what comes from the library.

## Using it

- [The command line](../../cli/docs/README.md): the `ursatz` commands, one page each ([motive](../../cli/docs/cli-motive.md), [grow](../../cli/docs/cli-grow.md), [plan](../../cli/docs/cli-plan.md), [piece](../../cli/docs/cli-piece.md), [listen](../../cli/docs/cli-listen.md) and the others), the workflows (middle-out, checking, styles and corpora) and the flag table.
- [export.md](export.md): writing a plan as MIDI or MusicXML.
- [theme-request.md](theme-request.md): the JSON request of the `theme` command and API entry.
- [piece-request.md](piece-request.md): the JSON request of the `piece` command and API entry.
- [style.md](style.md): style models, blending and seeding from a corpus.
- [middle-out.md](middle-out.md): planning a piece from a given phrase, progression or bass line.

## API reference

- [api.md](api.md): what every entry shares (versioning, memory, jobs, errors) and the entry index.
- [api/motives.md](api/motives.md): export, motive requests, motive, MIDI, render, variants, recognize and random motive.
- [api/theme-and-piece.md](api/theme-and-piece.md): theme, piece and piece variations.
- [api/plan-workflow.md](api/plan-workflow.md): plan, fill, lock, regenerate, variations and harmonize.
- [api/material-commands.md](api/material-commands.md): progression, counterpoint, develop and grow.
- [api/vocabulary-and-validation.md](api/vocabulary-and-validation.md): versions, vocabulary and validation.
- [api/examples.md](api/examples.md): requests and responses from runs of the API.

## Formats and schemas

- [plan-format.md](../../library/docs/modules/plan-format.md): the plan JSON file, its nodes, lanes, state and versions.
- [plan-constraints.md](plan-constraints.md): the constraints of a plan node and the motive relation steps.
- [motive-format.md](motive-format.md): the motive document and its analysis.
- [variant-registry.md](variant-registry.md): the registry of variant operations, chains and records.
- [variant-parameters.md](variant-parameters.md): the parameters of the variant operations.

## How it works inside

- [fill-loop.md](fill-loop.md): the engine that fills a plan with seeded, recorded operators.
- [melody-fill.md](melody-fill.md): the melody operator.
- [blueprint-generator.md](blueprint-generator.md): the blueprint generator (R-751): the chords, rhythms, climax and sounds the Composer draws from a motif, the eligibility of a request and the echoed blueprint (API 4.9).
- [presets.md](presets.md): the presets `ff7` and `hollow-knight` (R-754): named settings of her listening, which a theme or random motive request names with `preset`.
- [blueprints.md](blueprints.md): theme blueprints: the request's `blueprint` member (R-743), the melody constraints, rhythm-only idea motives and the blueprint motive (R-746).
- [modes.md](modes.md): the church modes as a key (R-725): stacked chords, numerals, functions, the phrase model and cadences per mode, and what a modal key does not write.
- [preferences.md](preferences.md): the settings document (R-411): the melody, development, candidates, critic and harmony groups, and how it is read and written.
- [critic.md](critic.md): the critic: the melody line of a candidate, the ten evaluators of her listening notes and the score (R-719, R-755).
- [preferences-blueprint.md](preferences-blueprint.md): the blueprint group of the settings document: the weights of the blueprint generator and the blueprint motif strategy (R-750).
- [preferences-motif.md](preferences-motif.md): the motif group of the settings document: every setting of the segments motive generator.
- [accompaniment.md](accompaniment.md): the accompaniment operator.
- [voicing.md](voicing.md): the four-part voicing operator.
- [counterpoint.md](counterpoint.md): the counter-melody operator.
- [grow.md](grow.md): growing a theme from a motive: request, slots, stages and structure.
- [grow-harmony.md](grow-harmony.md): the motive-forms, harmony, melody and free phrases of the grow stages.
- [grow-segments.md](grow-segments.md): segments of the motif as fragments and liquidation in a theme (P34).
- [grow-development.md](grow-development.md): the theme `development` setting and its weight tables (P34).
- [grow-continuation.md](grow-continuation.md): the accelerating continuation settings: fragments from every segment, cut, diminished, and a liquidated fragment 2 (L3).
- [grow-contrast.md](grow-contrast.md): the contrast distance and new material: a contrasting idea that is similar to the basic idea, distant from it on a rhythm of its own, or a new motif (L4).
- [grow-liquidation.md](grow-liquidation.md): the liquidation section: 4 bars over a dominant pedal that take the pattern apart, a scale line and a broken chord of V ending the half cadence (P49).
- [figured-variations.md](figured-variations.md): the built-in figured variations form after Schoenberg ch. XVII (P50): the theme, five variations each with a figure, the minore, the root change, the climax and a coda, and what was measured.
- [variation-figure.md](variation-figure.md): the motive of variation: a figurate return rewrites the theme's melody from a skeleton, one figure to a cell, the cadence bars kept (P50).
- [main-and-subordinate.md](main-and-subordinate.md): the built-in main and subordinate themes form after Schoenberg ch. XVIII (P51): the main theme, a modulating transition, a subordinate theme and a lyric one, a codetta, the retransition, the return and the coda, the keys and cadences, and what was measured.
- [rondo.md](rondo.md): the built-in Andante and small rondo forms after Schoenberg ch. XIX (P52): the classification, A's returns varied, B in the tonic major and its changed transition, the contrast, the keys and cadences, and what was measured.
- [sonata.md](sonata.md): the built-in sonatina forms, the sonata-allegro at sonatina size after Schoenberg ch. XX (P53): the exposition, the elaboration and the recapitulation, the keys, the two minor-mode recapitulations, and what was measured.
- [form-parts.md](form-parts.md): the parts of larger forms after Schoenberg ch. XVIII: the transition, the subordinate theme, the retransition and the coda as form roles and a plan constraint (P51).
- [grow-construction.md](grow-construction.md): a theme of any whole bars from 3 to 16 and ideas of any whole bars, given by a construction (P47).
- [grow-hearing.md](grow-hearing.md): how a grown theme is heard, with periods and hybrids.
- [piece.md](piece.md): pieces from forms and the compose run.
- [piece-tests.md](piece-tests.md): the piece tests and their measured results.
- [forms.md](forms.md): forms as JSON documents (P44): the members, the game loop, how to add one.
- [form-sections.md](form-sections.md): the members of a section of a size.
- [forms-builtin.md](forms-builtin.md): the thirteen built-in forms.
- [small-ternary.md](small-ternary.md): the built-in ternary form after Schoenberg ch. XIII (P46): A, B over a pedal or in the away region, A1 varied, and what was measured.
- [minuet.md](minuet.md): the built-in minuet, trio and minuet and trio forms after Schoenberg ch. XV (P48): 3/4 at 112, written-out repeats, B leading into A1, the trio in its own key, and what was measured.
- [scherzo.md](scherzo.md): the built-in scherzo and scherzo and trio forms after Schoenberg ch. XVI (P49): a ternary with a modulating middle, sequences of a pattern, a liquidation and an extended return, the planned keys, and what was measured.
- [piece-structure.md](piece-structure.md): the game loop, a game track as data: intro, body, turnaround and ending.
- [loop-seam.md](loop-seam.md): how the end of a loop leads back into its start.
- [form-performance.md](form-performance.md): a section that leads into the next and its connective (P48); repeats and parts as they are added.
- [regenerate.md](regenerate.md): locking and regenerating nodes of a plan.
- [form-hear.md](form-hear.md): hearing the phrase form of a plan.
- [testing/span-hearing.md](testing/span-hearing.md): the Composer baselines heard on harmonic spans, for P41.
- [testing/theme-quality.md](testing/theme-quality.md): the theme quality sweep and measure of the formal function settings, for P67.
- [analyzer-check.md](analyzer-check.md): the Analyzer check of a composed section.
- [explain.md](explain.md): why each candidate was chosen or rejected.

## Limits and changes

- [changelog.md](changelog.md): the Composer versions and what each changed.
- [changelog-archive.md](changelog-archive.md): the changelog entries before P44.
- [changelog-archive-2.md](changelog-archive-2.md): the changelog entries of P51 and P50.
- [changelog-archive-3.md](changelog-archive-3.md): the changelog entries of P49 and P48.
- [changelog-archive-4.md](changelog-archive-4.md): the changelog entries of L4 and P47.
- [changelog-archive-5.md](changelog-archive-5.md): the changelog entries of L3, P46 and L2.
- [changelog-archive-7.md](changelog-archive-7.md): the changelog entries of P55, P53 and P52.
- [changelog-archive-8.md](changelog-archive-8.md): the changelog entries of P60 and P59.
- [changelog-archive-6.md](changelog-archive-6.md): the changelog entries of P45, L1 and P44.
- [changelog-archive-9.md](changelog-archive-9.md): the changelog entries of P62 and P61.
