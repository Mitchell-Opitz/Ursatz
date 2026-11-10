# Composer docs

Index of every Composer doc, grouped by what you want to do. Back to the [repository README](../README.md).

## Start here

- [architecture.md](architecture.md): what the Composer is, its module map, public API, versions, what it calls below and the data flow of a request.
- [structure.md](structure.md): the source directories, the CLI snapshot tests and the file scope state check.

## Build and test

- [building.md](building.md): the slow tests, the public header rule, the submodule pins and what comes from the library.

## Using it

- [cli.md](cli.md): every command with one example, middle-out, checking, styles and corpora.
- [cli-motive.md](cli-motive.md): the `motive` command.
- [cli-develop.md](cli-develop.md): the `develop` command.
- [cli-variants.md](cli-variants.md): the `variants` command.
- [cli-recognize.md](cli-recognize.md): the `recognize` command.
- [cli-random-motive.md](cli-random-motive.md): the `random-motive` command.
- [cli-progression.md](cli-progression.md): the `progression` command.
- [cli-counterpoint.md](cli-counterpoint.md): the `counterpoint` command.
- [cli-grow.md](cli-grow.md): the `grow` command.
- [cli-theme.md](cli-theme.md): the `theme` command.
- [cli-piece.md](cli-piece.md): the `piece` command.
- [cli-plan.md](cli-plan.md): the `plan` command.
- [cli-fill.md](cli-fill.md): the `fill` command.
- [cli-regenerate.md](cli-regenerate.md): the `lock` and `regenerate` commands.
- [cli-variations.md](cli-variations.md): the `variations` and `harmonize` commands.
- [cli-listen.md](cli-listen.md): the `listen` command and a listening round.
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

- [plan-format.md](plan-format.md): the plan JSON file, its nodes, lanes, state and versions.
- [plan-constraints.md](plan-constraints.md): the constraints of a plan node and the motive relation steps.
- [motive-format.md](motive-format.md): the motive document and its analysis.
- [variant-registry.md](variant-registry.md): the registry of variant operations, chains and records.
- [variant-parameters.md](variant-parameters.md): the parameters of the variant operations.
- [piece-structure.md](piece-structure.md): a game track as data: intro, body, turnaround and ending.

## How it works inside

- [fill-loop.md](fill-loop.md): the engine that fills a plan with seeded, recorded operators.
- [melody-fill.md](melody-fill.md): the melody operator.
- [preferences.md](preferences.md): the settings document (R-411): the melody group, and how it is read and written.
- [accompaniment.md](accompaniment.md): the accompaniment operator.
- [voicing.md](voicing.md): the four-part voicing operator.
- [counterpoint.md](counterpoint.md): the counter-melody operator.
- [grow.md](grow.md): growing a theme from a motive: request, slots, stages and structure.
- [grow-harmony.md](grow-harmony.md): the motive-forms, harmony, melody and free phrases of the grow stages.
- [grow-segments.md](grow-segments.md): segments of the motif as fragments and liquidation in a theme (P34).
- [grow-development.md](grow-development.md): the theme `development` setting and its weight tables (P34).
- [grow-hearing.md](grow-hearing.md): how a grown theme is heard, with periods and hybrids.
- [piece.md](piece.md): pieces from form templates and the compose run.
- [loop-seam.md](loop-seam.md): how the end of a loop leads back into its start.
- [regenerate.md](regenerate.md): locking and regenerating nodes of a plan.
- [form-hear.md](form-hear.md): hearing the phrase form of a plan.
- [testing/span-hearing.md](testing/span-hearing.md): the Composer baselines heard on harmonic spans, for P41.
- [analyzer-check.md](analyzer-check.md): the Analyzer check of a composed section.
- [explain.md](explain.md): why each candidate was chosen or rejected.

## Limits and changes

- [changelog.md](changelog.md): the Composer versions and what each changed.
