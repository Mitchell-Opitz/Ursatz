# Testing

The backend and frontend tests and the checks CI runs. [Docs index](README.md).

`ctest` runs the backend tests (health, analyzer, corpus, styles, composer, materials, theme, subjects, links and id routes, the piece metadata rules, the material store and the link service) and,
when `node` is installed, the frontend tests too (`frontend/tests/*.test.js`,
registered as `frontend_<name>`). Without node CMake prints one status line
and skips them. The frontend tests are plain Node scripts that can also be
run directly, for example `node frontend/tests/analysis-view.test.js`.
`analysis-view.test.js` loads `analysis-tabs.js` and checks it against
`frontend/tests/fixtures/mozart545.analysis.json`, the Analyzer CLI's
document for Mozart 545 cut to its first 6 items per array, and that each tab loaded per section equals the tab built
from the whole document.
`test_analyzer_route` uploads Mozart 545 and a MusicXML score, compares the
stored analysis with the Analyzer CLI's document for the same file, and
covers list, detail, metadata editing, filtering and groups, rename,
re-run and delete; the summary and the `/document` route; and, through a counter in the import cache, that the
detail, score, meta and playback of one piece import it once and a PATCH or re-analyze makes the next open import
again. `test_styles_route` uploads Clementi 36 and Mozart 545,
tags them and builds styles from a tag, a collection and ids, then reads the
model back with the Analyzer, rebuilds, re-analyzes an old piece and deletes.
`test_link_service` resolves each scope kind (a project through its subjects), runs the four link fixtures
(`backend/tests/fixtures/links`, copied from the Analyzer's `tests/links`), checks that a second run over the same
scope compares nothing, analyzes a document without features again and reads an interrupted run as failed.
`test_links_route` starts runs over HTTP, polls them to done, and covers the `409`, `400`, `404` and `422` answers,
the list, delete and a piece's links.
`test_piece_lock` covers the piece lock without a server: threads counting under one id, 50 PATCHes and 50
re-analyses of one piece while a reader checks that each detail, score and playback reply parses and that the
document's meter matches the notation override returned with it, and deletes during reads.
`test_piece_metadata` covers the normalizing, grouping and
filter rules without a server. `test_piece_snapshot` compares a piece's detail, score and playback replies with the snapshots in `backend/tests/fixtures/pieces`. `test_stale_analysis` covers the stale-analysis rules (Staleness keys, above). `test_material_service` covers the projects,
items, versions, version body rules and item references without a server. `style-view.test.js` covers the style detail
rows (order, fractions, "and n more") and `piece-metadata.test.js` the style
names and request bodies. `test_job_queue` covers the queue without a server: a job's answer and kind, progress seen
while it runs, the cancel of a running theme compose (it ends `cancelled`) and of a queued job, two workers running two
jobs at once, and stop joining the workers. `backend-jobs.test.js` covers the job client with a fake fetch. `test_materials_route` replays 35 requests captured while the Composer still ran as a command (`backend/tests/fixtures/composer/b5_replies.json`: motif analysis, degrees to notes, MIDI import, render, operations, recognize, random motifs, status) and checks each reply equal (a refusal by status), and the refusal paths (`/meter`, `/input/text`); `test_composer_route` checks the versions the status reports. `test_theme_route` checks the theme body rules, the request a compose
writes (slots, a pool of approved and unflagged forms without the rejected, the brief, `without`, read back from the
version's `request.json`), the version it writes with its files, the files route, a stuck run writing nothing and
leaving no staging folder, `update_uses`, two composes of one theme at once, the jobs routes, and one compose over a
brief of fixture pieces whose plan, MIDI and MusicXML equal what the Composer command wrote in B6
(`backend/tests/fixtures/composer/b6_compose`). `test_piece_route` does the same for a piece: its compose,
regenerate, variations and choose are compared with the same capture (a chosen variation's ending is made from a seed
read from the clock, so only its loop version is compared).

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on every push and
pull request. The private submodules are checked out through the
`URSATZ_SUBMODULE_TOKEN` repository secret, which must be set.
