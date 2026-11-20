# Composer routes

The Composer routes, the job routes and the event stream. Every route answers the one envelope and the real HTTP status ([the envelope](envelope.md)). [Docs index](../README.md).

## Composer routes

The Composer tool grows themes, plans and fills pieces, and makes variations. The quick requests (version,
motif analysis, degrees to notes, MIDI to motif, motif render, the operation list, recognize and random motifs)
are calls of the Composer API (`composer_api_*`, `ursatz_composer/composer_api.h`) in process (R-219): the backend
links the Composer library (the root build's `composer` target; the command is built separately), which reuses the
cJSON, Ursatz and Analyzer targets the root builds, so one version of each is in the build. A request is a cJSON
object printed to text, never an argument list; `workshop/src/composition/composer_client.c` moves the answer into an
`WorkshopComposerReply` (`status`, `json`, `plan_json`, `midi`, `musicxml`, and the refusal `message` and `path`). The
CLI's exit 1 is `URSATZ_ERR_REQUEST`, exit 2 `URSATZ_ERR_UNSATISFIABLE`; a request error is 400 and an unsatisfiable
request is 422 on every route, each with the Composer's message and the `path` of the request member at fault (additive
to `error`); anything else is a failure of the run (500, or 499 when cancelled).

The long runs (theme and piece composes, regenerate, variations and choose, variants, each-op and reroll) are calls of
the same API on a job queue (R-219, R-274); no request thread waits on a compose and no process is started. The Composer
command is no longer built or run.

civetweb's request threads have a 32 MB stack (`CIVETWEB_THREAD_STACK_SIZE`, address space only): its 100 KB default is
too small for entries such as the random motive. The job workers get the same 32 MB.

### Jobs

The workshop's job queue (`workshop/src/jobs/job_queue.c`, [jobs and events](../../../workshop/docs/jobs-and-events.md)) keeps worker threads (`URSATZ_JOB_WORKERS`, 1 to 8, default 2) and a list of jobs in memory;
`workshop_start` starts it before the routes and `workshop_stop` ends it on shutdown (every job is cancelled, the workers are joined).
A job has a kind, a state (`queued`, `running`, `done`, `failed`, `cancelled`), the `stage`, `done` and `total` the Composer's progress callback reports (the stages are in the Composer's [Jobs](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/api.md#jobs) and [Job rules](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/api.md#job-rules)) and, once it ends, the status and body its route used to send.
Cancel is asked for the Composer's `cancelled` callback, so a running job stops at its next report and a queued job at
once. Finished jobs are kept 10 minutes or the newest 64 and are lost on restart.

A route that runs long work validates the request, queues the job and answers `202 {"ok": true, "job": {"id", "kind",
"state"}}`. The kinds are `theme`, `piece`, `regenerate`, `variations`, `choose`, `variants`, `each-op` and `reroll`, and
`links`, `style` and `analysis` for the Analyzer's long work (piece imports and re-analyses are `202` jobs too, see the [Analyzer routes](analysis-routes.md#analyzer-routes)).

- `URSATZ_JOB_WORKERS`: the number of workers (1 to 8, default 2), read by the workshop settings.

The routes check the shape of a request before it reaches the Composer: types, counts in range, form letters
(`^[A-Z]'*$`), lanes, node ids and the length of text. The words of music (keys, meters, pickups, note lists, harmony)
are the Composer's to read: it refuses what it does not read with a message and the `path` of the member at fault
(`422` for a member of a request, `400` for a member of the call itself; nothing starts). A job the Composer refuses fails with the same `error` and `path` in its result
(`{"ok": false, "error", "path"}`, no `path` when no member is at fault), and the Theme and Piece forms mark that field.

Routes (bodies are JSON; success bodies have `"ok":true`):

- `GET /api/composer/status`: `{"ok":true,"available":true,"version":"ursatz-composer 0.4.0","api_version":"1.1",
  "analyzer_version":"0.5.0","producer_key":"..."}` from `composer_api_versions` (`producer_key` holds the Analyzer's producer key; the member was `ursatz_commit` before API 4.0) (the last three are additive). The Composer API of this tree is 4.2.

- `GET /api/composer/vocabulary`: the lists the Composer reads (`composer_api_vocabulary`), built once and kept: meters
  with their beats and second chord beat, modes, keys and `pitch_class_names`, cadences, pickups for each meter, the
  theme words (slots, ideas, beginnings and endings, bars, pools, weights, categories), accompaniment patterns, piece
  parts, intos and fill kinds, tone labels with their abbreviations, and `limits` (API 4.1 adds `theme_developments` with their help text, `lanes` and the limits `theme_motives`, `theme_extensions`, `piece_variations` and `harmony_cells`; 4.2 adds `construction_bars` and `construction_pickup`). The frontend loads it once before
  its views mount (`vocabulary.js`) and keeps no table of music of its own; the GUI keeps only its own limits (cells of a
  chord, tabs, the number of chord bars) and its rules (themes and motifs are in C, the pool rule, storage length caps).
- `GET /api/composer/forms`: the Composer's built-in forms (`composer_api_forms`, R-428), built once and kept:
  `{"forms": [{name, label, closing, sections, versions, settings, regions, sizes, roles, loop_roles, cadences,
  default_ending}]}`. A form with `sections` `sizes` lists its sizes (`bars`, `count`, `default`) with the sections of
  each (name, letter, role, bars, beginning, ending, cadence, keys, slots); the game loop's `sections` is `request`,
  with its `roles`. `versions` names the versions a piece of the form has (`loop` and `ending`, `piece`), and
  `settings` says what the form takes (`modulate`, `accompaniment`, `voicing`, `section_fills`). The backend reads a
  piece's structure against it (`workshop_composer_form`) and the Pieces view shows it (B6). Only `GET` (`405`).
- `POST /api/materials/items/validate`: `{"type": "motif"|"theme"|"piece", "body"}` builds the Composer request the
  body would compose (or, for a motif, analyze) with and has the Composer read it without composing
  (`composer_api_validate`; nothing is written). `{"ok":true,"complete":true,"request"}` gives the request with the
  Composer's defaults filled; `{"ok":true,"complete":false,"message"}` means the body is not complete enough to be a request
  (a theme without slots, a piece without sections; no Composer call); a request the Composer refuses is `422` with its
  `error` and `path` (the member at fault). The store checks no word of music: a draft may hold any word, and this route and Compose are where the Composer judges it.
  For a theme the Composer checks the whole plan, so validate refuses what Compose refuses before seeding: a motif too long
  for the form (`/slots/<slot>`, `/ideas/<name>`), a given chord that cannot end the cadence (`/chords/<index>`, the entry of
  the chords sent), a pairing nothing satisfies. A 2-bar motif in 3/4 with one chord a bar is accepted.
- `GET /api/jobs/<id>`: `{"ok": true, "job": {"id", "kind", "state", "stage", "done", "total", "http_status"?,
  "result"?}}`. `http_status` and `result` (the body the route answered before jobs) are there once the job is done or
  failed. `404` for an unknown job.
- `DELETE /api/jobs/<id>`: asks the job to stop (`204`; for a finished job nothing changes); `404` for an unknown job.

Themes and pieces compose in process on the job queue (a body is a stored Composer request, see [Items and versions](../../../workshop/docs/data-folder.md#items-and-versions)). Each compose writes its files in a pending folder of the transaction that commits the version, so a version never exists without its files; a compose made against a version that is no longer the item's current one fails with `409` in its job result.

## Events

`GET /api/events` is the workshop's event bus as server-sent events ([jobs and events](../../../workshop/docs/jobs-and-events.md#events)). The answer is `text/event-stream` with `Cache-Control: no-cache` and no length; each event is `event: <name>` and `data: <json>` and a blank line, and a comment line (`: keep-alive`) comes every 15 seconds. At most 8 streams are open at once; a ninth is `503` in the envelope. The events are `item-changed`, `item-removed`, `library-changed`, `collection-changed`, `subject-changed`, `style-changed`, `links-changed`, `job-progress`, `job-done`, `analysis-stale` and `reset` (the stream's queue overflowed: read everything again). The frontend opens one `EventSource` (`core/events.js`); `core/backend-jobs.js` follows a job through `job-progress` and `job-done` and reads `GET /api/jobs/<id>` at the start and at each (re)opening of the stream, and polls only where there is no `EventSource`; the shell reloads the open view on the events that say what it shows changed (300 ms debounce).
