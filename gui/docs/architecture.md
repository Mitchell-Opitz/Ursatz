# GUI architecture

How the GUI is built, what it calls, and where a request goes. [Docs index](README.md).

## Purpose

A local web UI over the Ursatz platform: it imports MIDI and MusicXML files, shows their analysis next to the score, plays them, and composes motifs, themes and pieces. It is a client of the [workshop](../../workshop/docs/architecture.md): the backend parses a request, asks the workshop and sends the answer; the frontend draws it. Every music answer comes from the Analyzer or the Composer through the workshop, and every file the GUI shows is held by the workshop in the data folder.

## Where it sits

- Below it, the workshop (`workshop/`, one API header `ursatz_workshop/workshop_api.h`), which holds the data folder, the jobs and the events and calls the Analyzer (`analyzer/`) and the Composer (`composer/`).
- The backend includes only `workshop_api.h`, `common/`, civetweb and cJSON (its row in `cmake/layer-table.txt`: `uses gui workshop-api common cjson civetweb`); it has no service layer and calls no Analyzer or Composer function.
- Above it, the browser only.

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `backend/src/main.c`, `server.c` | The process: flags, settings, migration, the workshop, the HTTP server | [Backend](backend-modules.md) |
| `backend/src/routes/` | One route table over the workshop: the envelope, the body reader, one file per family of routes | [api/](api/envelope.md) |
| `backend/tests/` | Route tests, the HTTP test support and fixtures | [testing.md](testing.md) |
| `frontend/public/` | The pages as ES modules: `core/`, `models/`, `views/`, `score/`, `main.js`, `index.html`, `style.css` | [frontend.md](frontend.md), [views/](views/library.md) |
| `frontend/tests/` | Node tests of the modules and of every view on a fake DOM | [testing.md](testing.md) |
| `../tools/ci/` (repository root) | Include, theory and docs checks, shared by every part | [testing.md](testing.md) |

## Route table

Every route of `/api` is one row `{method, pattern, handler}` of a `<family>_routes.c` file; `route_table.c` matches a path against the rows (`<id>`, `<n>` and `<name>` take one segment each) and answers a path no row fits with `404` and one another method fits with `405`. A handler reads the request, builds the params, runs a workshop query or command and answers. Paths under `/api/materials/` are shortened to `...`.

| Routes | File | Workshop calls | Doc |
|---|---|---|---|
| `/api/analyzer/pieces` (`GET POST`), `.../<id>` (`GET PATCH DELETE`), `.../<id>/document`, `/export`, `/musicxml`, `/musicxml/meta`, `/playback`, `/reanalyze`, `/api/analyzer/groups` | `library_routes.c` | `library.*` | [Analyzer routes](api/analysis-routes.md#analyzer-routes) |
| `/api/corpus`, `/api/corpus/<id>`, `.../<id>/pieces`, `.../pieces/<piece_id>` | `collections_routes.c` | `collection.*` | [Corpus routes](api/analysis-routes.md#corpus-routes) |
| `/api/styles`, `/api/styles/<id>`, `.../<id>/rebuild` | `styles_routes.c` | `style.*` | [Styles routes](api/analysis-routes.md#styles-routes) |
| `/api/links/runs`, `/api/links/runs/<id>`, `/api/links/pieces/<id>` | `links_routes.c` | `links.*` | [Motive links routes](api/analysis-routes.md#motive-links-routes) |
| `.../projects`, `.../projects/<id>`, `.../items`, `.../items/<id>`, `.../items/validate`, `.../items/<id>/versions`, `/versions/<n>`, `/files/<name>`, `/score`, `/playback`, `/compose`, `/regenerate`, `/variations`, `/variations/<k>/choose`, `/reanalyze`, `/uses/<source>/update` | `materials_routes.c` | `project.*`, `item.*` | [Materials routes](api/materials-routes.md#materials-and-subjects-routes) |
| `.../items/<id>/variants`, `/variants/save`, `/variant-tree`, `/reroll`, `/each-op`, `/op-checks`, `/link-form`, `/promote`, `/flag`, `.../variant-operations`, `.../recognize`, `.../render`, `.../motifs/random`, `.../motifs/import` (their rows come first: `/items/validate` is a word, not an id) | `motifs_routes.c` | `item.*`, `composer.*` | same |
| `/api/subjects`, `/api/subjects/groups`, `/api/subjects/<id>`, `.../<id>/items/<item>`, `/pieces/<piece>`, `/related/<other>` | `subjects_routes.c` | `subject.*` | same |
| `GET /api/composer/status`, `/vocabulary`, `/forms` | `composer_routes.c` | `composer.*` | [Composer routes](api/composer-routes.md#composer-routes) |
| `/api/jobs/<id>` (`GET DELETE`) | `jobs_routes.c` | `job.get`, `job.cancel` | [Jobs](api/composer-routes.md#jobs) |
| `GET /api/events` | `events_routes.c` | `workshop_events_*` | [Events](api/composer-routes.md#events) |

## Version constants and schemas

| Name | Where reported | Current value | Doc |
|---|---|---|---|
| `version`, `api_version`, `analyzer_version`, `producer_key` | `GET /api/composer/status`, from `composer_api_versions` | the Composer of this tree (API 4.2) and the builds it links | [Composer routes](api/composer-routes.md#composer-routes) |
| `analyzer_version`, `producer_key`, `document_schema_version` | each piece's `piece.json` | the Analyzer of this tree | [The Library](../../workshop/docs/data-folder.md#the-library) |
| `api_version`, `composer_version`, `composition_key`, `render_key`, `tempo` | `render.json` beside a cached motif render | the Composer of this tree | [Motif score and playback](views/playback.md#motif-score-and-playback) |
| `format`, `format_version` | every file the workshop owns; `data-folder.json` | layout 2 | [Formats](../../workshop/docs/data-folder.md#formats) |

## Data flow

One piece, from file to composed motif.

1. **GUI.** The browser sends the file to `POST /api/analyzer/pieces`; the route reads it (cap 64 MB) and runs the command `library.import` with the file in `bytes`. The answer is `202` with the job.
2. **Workshop.** The job imports the file with the Analyzer (`analyzer_import_file`), analyzes it (`analyzer_analyze`), builds the analysis document and its layers and commits the piece as one transaction ([the data folder](../../workshop/docs/data-folder.md#changes-are-transactions)); `library-changed` and `job-done` events follow.
3. **GUI.** The page, following the event stream, reads the Library again and the table fills in.
4. **GUI and workshop.** Opening a piece asks for the summary, the sections the score draws, the MusicXML and the playback (`library.piece`, `library.document`, `library.musicxml`, `library.playback`); the frontend draws and plays them ([views](views/analysis-and-score.md)). A piece whose analysis is stale is answered as stored with `stale: true`, and the page starts the re-analysis job.
5. **GUI and workshop.** A motif, theme or piece is edited as a body and composed with `item.compose`; the workshop resolves the body's references into a Composer request and calls the Composer (`composer_api_*`) on a worker thread. The result is committed as a new version with its files and layers.

### GUI actions

| Action | Route | Workshop | Answered by |
|---|---|---|---|
| Import or analyze a piece | `POST /api/analyzer/pieces`, `.../<id>/reanalyze` | `library.import`, `library.reanalyze` | Analyzer: `analyzer_import_file`, `analyzer_analyze` |
| Open the Score | `GET .../musicxml`, `.../document?sections=` | `library.musicxml`, `library.document` | Analyzer: `analyzer_score_export_musicxml`, `analyzer_document_to_json` |
| Play | `GET .../pieces/<id>/playback` | `library.playback` | Analyzer: `analyzer_playback_build` |
| Create, analyze and save a motif | `POST /api/materials/items`, `.../<id>/versions`, `.../reanalyze` | `item.create`, `item.save`, `item.reanalyze` | Composer: `composer_api_motive` |
| Random motif | `POST /api/materials/motifs/random` | `composer.random-motifs` | Composer: `composer_api_random_motive` |
| Variants | `POST .../items/<id>/variants`, `.../variants/save` | `item.variants`, `item.save-variant` | Composer: `composer_api_variants`, `composer_api_variant_operations` |
| Compose a theme or piece | `POST .../items/<id>/compose` | `item.compose` | Composer: `composer_api_theme`, `composer_api_piece`, `composer_api_piece_variations` |
| Check a draft | `POST .../items/validate` | `item.validate` | Composer: `composer_api_validate` |
| Export | `GET .../versions/<n>/files/<name>`, `/score` | `item.file`, `item.score` | the files the compose wrote |

## Finding things

Every doc is in the [docs index](README.md); the workshop's are in [its index](../../workshop/docs/README.md).
