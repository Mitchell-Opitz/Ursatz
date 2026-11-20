# Testing

The backend and frontend tests and the checks CI runs. [Docs index](README.md).

Every test is registered with `ursatz_test()` (`cmake/UrsatzTest.cmake` at the repository root): a backend test is `ursatz_test(<name> LIBS ursatz_gui_core ... WORKING_DIRECTORY <its own build folder>)`, a frontend test is `ursatz_test(frontend_<name> COMMAND node <file>)`. The timeout is 120 s, or 1800 s for a `slow` test.

One rule: a fast-set test over 10 s is sped up or labeled `slow`. `tools/ci/slowest-tests.sh build/gui` (from the repository root) warns about any fast-set test over 10 s.

The frontend tests share `frontend/tests/support/check.js` (`check`, `same`, `near`, `finish`: call `finish()` last, it sets the exit code) and `frontend/tests/support/fixtures.js` (`generated(name)` and `analyzerSnapshot(name)`, below). The backend tests use `tests/support/ursatz_test.h` and the HTTP client `tests/support/test_http.h` at the repository root.

The reference pieces and the link fixtures come from the root `tests/fixtures/` (`URSATZ_FIXTURES_DIR`).

Fixtures come in three kinds:

- **Generated in the build.** `vocabulary.json`, `forms.json` and `piece-<form>.json` (the Composer's vocabulary, forms and the summary of a piece of each form with sizes) are written by the `gui_frontend_fixtures` target (part of `ALL`), which runs `gui_write_frontend_fixtures` (`backend/tests/write_frontend_fixtures.c`): it writes the exact strings the two routes serve, from the in-tree Composer, to `<build>/gui/frontend/tests/fixtures`. The frontend tests find that folder in `URSATZ_GUI_GENERATED_FIXTURES`, which `ursatz_test` sets for them, and read the files with `generated("vocabulary.json")`; run through `ctest`, as the variable is not set otherwise. `test_composer_route` compares each route answer with its generated file, so the files are what the routes give by construction.
- **The Analyzer's own snapshots.** The Mozart 545 document that `analysis-view`, `inspector-model` and `score-layers` read is `analyzer/tests/snapshots/mozart-545.analysis.json` (`analyzerSnapshot("mozart-545.analysis.json")`); the Bach 114 playback that `test_piece_snapshot` compares is `analyzer/tests/snapshots/gui/bach-114.playback.json`. The Analyzer's tests own both. The snapshots drop the Analyzer's volatile members, so `analysis-view` sets `metadata.analyzer_version`, `producer_key` and `input` on its parsed copy.
- **Captured.** What the GUI itself made stays in `backend/tests/fixtures` and `frontend/tests/fixtures`: the piece snapshots of `test_piece_snapshot` (`backend/tests/fixtures/pieces`; `update-goldens` (`URSATZ_UPDATE_GOLDENS=1`) rewrites them, the GUI adds the work title to the MusicXML), the piece summaries the Composer cannot recompose here (a style, a motive) and the theme summary, `theme.playback.json`, `theme.mid` and `multivoice.musicxml`, the GUI-computed defaults `backend/tests/fixtures/composer/b11_captures.json`, and the motif playback and export fixtures. They stay captured until the frontend moves onto the workshop API (P63).

`ctest` runs the backend tests (the startup data check, analyzer, corpus, styles, composer, materials, theme, subjects, links and id routes, the piece metadata rules, the material store and the link service) and,
when `node` is installed, the frontend tests too (`frontend/tests/*.test.js`,
registered as `frontend_<name>`). Without node the configure prints one status line
and skips them (`-DURSATZ_REQUIRE=node` makes it fail instead). The frontend tests are plain Node scripts; the ones that read a generated fixture need `URSATZ_GUI_GENERATED_FIXTURES` (run them through `ctest`), the others also run directly, for example `node frontend/tests/analysis-view.test.js`.
`analysis-view.test.js` loads `analysis-tabs.js` and checks it against the Analyzer's snapshot of Mozart 545
(`analyzer/tests/snapshots/mozart-545.analysis.json`), and that each tab loaded per section equals the tab built
from the whole document.
`test_analyzer_route` uploads Mozart 545 and a MusicXML score, checks the stored analysis by section and by detail (it does not run the Analyzer CLI: the Analyzer's own tests own the CLI's output), and
covers list, detail, metadata editing, filtering and groups, rename,
re-run and delete; the summary and the `/document` route; and, through a counter in the import cache, that the
detail, score, meta and playback of one piece import it once and a PATCH or re-analyze makes the next open import
again. `test_styles_route` uploads Clementi 36 and Mozart 545,
tags them and builds styles from a tag, a collection and ids, then reads the
model back with the Analyzer, rebuilds, re-analyzes an old piece and deletes.
`test_link_service` resolves each scope kind (a project through its subjects), runs the four link fixtures
(`tests/fixtures/links` at the repository root), checks that a second run over the same
scope compares nothing, analyzes a document without features again and reads an interrupted run as failed.
`test_links_route` starts runs over HTTP, polls them to done, and covers the `409`, `400`, `404` and `422` answers,
the list, delete and a piece's links.
`test_piece_lock` covers the piece lock without a server: threads counting under one id, 50 PATCHes and 50
re-analyses of one piece while a reader checks that each detail, score and playback reply parses and that the
document's meter matches the notation override returned with it, and deletes during reads.
`test_piece_metadata` covers the normalizing, grouping and
filter rules without a server. `test_piece_snapshot` compares a piece's detail, score and playback replies with the snapshots in `backend/tests/fixtures/pieces` (the Bach 114 playback with the Analyzer's snapshot). `test_stale_analysis` covers the stale-analysis rules (Staleness keys, above). `test_material_service` covers the projects,
items, versions, version body rules and item references without a server. `style-view.test.js` covers the style detail
rows (order, fractions, "and n more") and `piece-metadata.test.js` the style
names and request bodies. `test_job_queue` covers the queue without a server: a job's answer and kind, progress seen
while it runs, the cancel of a running theme compose (it ends `cancelled`) and of a queued job, two workers running two
jobs at once, and stop joining the workers. `backend-jobs.test.js` covers the job client with a fake fetch. `test_materials_route` sends 35 requests to the motif routes from a table in the test (motif analysis, degrees to notes, MIDI import, render, operations, recognize, random motifs, status) and checks each answers its status, a refusal an `error` and a success the top-level members its route documents; the `analyze_` cases check the stored motif holds `analysis`, `degrees` and `analysis_error`. It also checks the refusal paths (`/meter`, `/input/text`) and the export, which installs `backend/tests/fixtures/export/one-bar.musicxml` as the composed score of a theme and a piece. `test_composer_route` checks the versions the status reports and that the vocabulary and forms routes give the generated `vocabulary.json` and `forms.json`. `test_theme_route` checks the theme body rules, the request a compose
writes (slots, a pool of approved and unflagged forms without the rejected, the brief, `without`, read back from the
version's `request.json`), the version it writes with its files, the files route, a stuck run writing nothing and
leaving no staging folder, `update_uses`, two composes of one theme at once, the jobs routes, and one compose over a
brief of fixture pieces whose `plan.json`, `theme.mid` and `theme.musicxml` are what they claim to be (`backend/tests/version_check.h`: a plan with `"format": "ursatz-composer-plan"`, a MIDI file starting `MThd`, a score with `<score-partwise`) and whose served playback equals the playback service's for the stored MIDI file. `test_piece_route` does the same for a piece: each stored plan, MIDI and MusicXML file of its compose, regenerate and choose passes those checks, the summary names the stored plans, and the sections of the loop and ending versions follow the request the version was composed from (a chosen variation's ending is made from a seed read from the clock, so only its loop version is checked).

CI (`.github/workflows/ci.yml`) builds the whole repository once and runs the checks and the fast set on every push and
pull request; no secret is needed.

## Smoke runs

`piece_route_smoke`, `theme_route_smoke`, `analyzer_route_smoke` and `styles_route_smoke` run the slow route tests' first case with the real Composer or the Analyzer (server start, the setup, then `test_real_composer`; the Bach 114 upload and its detail, score meter, document sections and export; Clementi 36 and one style) in 2 to 4 s each, in the fast set. The full route tests stay `slow` (nightly).
