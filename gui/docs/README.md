# GUI docs index

Every doc of the Ursatz GUI, one line each. Back to the [README](../README.md).

## Start here

- [architecture.md](architecture.md): what the GUI is, its route table, what it calls, the data flow across the four repos and the GUI action table.

## Build and test

- [testing.md](testing.md): the backend and frontend tests and the checks CI runs.

## Using it

- [views/library.md](views/library.md): the Library, the Tags panel and the other top-level views.
- [views/motifs.md](views/motifs.md): the Materials views and the Motif editor.
- [views/themes-and-pieces.md](views/themes-and-pieces.md): the Themes view, the Pieces view and item references.
- [views/analysis-and-score.md](views/analysis-and-score.md): Subjects, Motive links, the Analysis view, the Score, its layers and the Inspector.
- [views/playback.md](views/playback.md): playback of pieces and motifs.

## API reference

- [api/analysis-routes.md](api/analysis-routes.md): the Analyzer, Corpus, Styles and Motive links routes.
- [api/materials-routes.md](api/materials-routes.md): the materials and subjects routes.
- [api/composer-routes.md](api/composer-routes.md): the Composer routes and the job queue.

## How it works inside

- [storage.md](storage.md): the piece, material, theme, piece and subject stores and their files.
- [motifs-backend.md](motifs-backend.md): motif analysis on save, the variant tree, comparing and linking motifs, motive-forms and random motifs.
- [frontend.md](frontend.md): how the frontend is organized, the score layers and the score geometry.
- [backend-modules.md](backend-modules.md): the backend helper modules, the backend services and who calls the Composer.
