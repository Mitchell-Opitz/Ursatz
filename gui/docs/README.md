# GUI docs index

Every doc of the Ursatz GUI, one line each. Back to the [README](../README.md). The data, the commands, the jobs and the events are the [workshop's](../../workshop/docs/README.md).

## Start here

- [architecture.md](architecture.md): what the GUI is (a client of the workshop), its route table, what it calls and the data flow.

## Build and test

- [testing.md](testing.md): the backend and frontend tests and the checks CI runs.

## Using it

- [views/library.md](views/library.md): the Library, the Tags panel and the other top-level views.
- [views/motifs.md](views/motifs.md): the Materials views and the Motif editor.
- [views/themes-and-pieces.md](views/themes-and-pieces.md): the Themes view, the Pieces view and item references.
- [views/piece-sections.md](views/piece-sections.md): a section's region, harmony and the overlays of a sized form.
- [views/analysis-and-score.md](views/analysis-and-score.md): Subjects, Motive links, the Analysis view, the Score, its layers and the Inspector.
- [views/playback.md](views/playback.md): playback of pieces and motifs.

## API reference

- [api/envelope.md](api/envelope.md): the one answer shape, the real statuses, the body caps, 202 jobs and base versions.
- [api/analysis-routes.md](api/analysis-routes.md): the Analyzer, Corpus, Styles and Motive links routes.
- [api/materials-routes.md](api/materials-routes.md): the materials and subjects routes.
- [api/composer-routes.md](api/composer-routes.md): the Composer routes, the job routes and the event stream.

## How it works inside

- [backend-modules.md](backend-modules.md): the backend's files, the route table and the route helpers.
- [frontend.md](frontend.md): how the frontend is organized (ES modules), the DOM harness, the score layers and the score geometry.
