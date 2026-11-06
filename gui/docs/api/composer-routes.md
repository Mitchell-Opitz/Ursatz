# Composer routes

The Composer routes and the job queue that runs long work. [Docs index](../README.md).

## Composer routes

The Composer tool grows themes, plans and fills pieces, and makes variations. The quick requests (version,
motif analysis, degrees to notes, MIDI to motif, motif render, the operation list, recognize and random motifs)
are calls of the Composer API (`composer_api_*`, `ursatz_composer/composer_api.h`) in process (R-219): the backend
links the Composer library (`add_subdirectory(external/ursatz-composer)` with the command off), which reuses the
cJSON, Ursatz and Analyzer targets the GUI builds, so one version of each is in the build. A request is a cJSON
object printed to text, never an argument list; `service/composer_client.c` moves the answer into an
`UrsatzGuiComposerReply` (`status`, `json`, `plan_json`, `midi`, `musicxml`, and the refusal `message` and `path`). The
CLI's exit 1 is `COMPOSER_ERR_REQUEST`, exit 2 `COMPOSER_ERR_UNSATISFIABLE`; a request error is 400 and an unsatisfiable
request is 422 on every route, each with the Composer's message and the `path` of the request member at fault (additive
to `error`); anything else is a failure of the run (500, or 499 when cancelled).

The long runs (theme and piece composes, regenerate, variations and choose, variants, each-op and reroll) are calls of
the same API on a job queue (R-219, R-274); no request thread waits on a compose and no process is started. The Composer
command is no longer built or run.

civetweb's request threads have a 32 MB stack (`CIVETWEB_THREAD_STACK_SIZE`, address space only): its 100 KB default is
too small for entries such as the random motive. The job workers get the same 32 MB.

### Jobs

`service/job_queue.c` keeps worker threads (`URSATZ_GUI_JOB_WORKERS`, 1 to 8, default 2) and a list of jobs in memory;
it is started in `server.c` before the routes and stopped on shutdown (every job is cancelled, the workers are joined).
A job has a kind, a state (`queued`, `running`, `done`, `failed`, `cancelled`), the `stage`, `done` and `total` the Composer's progress callback reports (the stages are in the Composer's [Jobs](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/api.md#jobs) and [Job rules](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/api.md#job-rules)) and, once it ends, the status and body its route used to send.
Cancel is asked for the Composer's `cancelled` callback, so a running job stops at its next report and a queued job at
once. Finished jobs are kept 10 minutes or the newest 64 and are lost on restart.

A route that runs long work validates the request, queues the job and answers `202 {"ok": true, "job": {"id", "kind",
"state"}}`. The kinds are `theme`, `piece`, `regenerate`, `variations`, `choose`, `variants`, `each-op` and `reroll`.

- `URSATZ_GUI_JOB_WORKERS`: the number of workers (1 to 8, default 2).

The routes check the shape of a request before it reaches the Composer: types, counts in range, form letters
(`^[A-Z]'*$`), lanes, node ids and the length of text. The words of music (keys, meters, pickups, note lists, harmony)
are the Composer's to read: it refuses what it does not read with a message and the `path` of the member at fault
(`400`, nothing starts). A job the Composer refuses fails with the same `error` and `path` in its result
(`{"ok": false, "error", "path"}`, no `path` when no member is at fault), and the Theme and Piece forms mark that field.

Routes (bodies are JSON; success bodies have `"ok":true`):

- `GET /api/composer/status`: `{"ok":true,"available":true,"version":"ursatz-composer 0.4.0","api_version":"1.1",
  "analyzer_version":"0.5.0","ursatz_commit":"..."}` from `composer_api_versions` (the last three are additive).

- `GET /api/composer/vocabulary`: the lists the Composer reads (`composer_api_vocabulary`), built once and kept: meters
  with their beats and second chord beat, modes, keys and `pitch_class_names`, cadences, pickups for each meter, the
  theme words (slots, ideas, beginnings and endings, bars, pools, weights, categories), accompaniment patterns, piece
  parts, intos and fill kinds, tone labels with their abbreviations, and `limits`. The frontend loads it once before
  its views mount (`vocabulary.js`) and keeps no table of music of its own; the GUI keeps only its own limits (cells of a
  chord, tabs, the number of chord bars) and its rules (themes and motifs are in C, the pool rule, storage length caps).
- `POST /api/materials/items/validate`: `{"type": "motif"|"theme"|"piece", "body"}` builds the Composer request the
  body would compose (or, for a motif, analyze) with and has the Composer read it without composing
  (`composer_api_validate`; nothing is written). `{"ok":true,"complete":true,"request"}` gives the request with the
  Composer's defaults filled; `{"ok":true,"complete":false,"message"}` means the body is not composable yet (no
  Composer call); a request the Composer refuses is `400` (malformed) or `422` (nothing satisfies it) with its message and `path` (the member at fault).
  For a theme the Composer checks the whole plan, so validate refuses what Compose refuses before seeding: a motif too long
  for the form (`/slots/<slot>`, `/ideas/<name>`), a given chord that cannot end the cadence (`/chords/<index>`, the entry of
  the chords sent), a pairing nothing satisfies. A 2-bar motif in 3/4 with one chord a bar is accepted.
- `GET /api/jobs/<id>`: `{"ok": true, "job": {"id", "kind", "state", "stage", "done", "total", "http_status"?,
  "result"?}}`. `http_status` and `result` (the body the route answered before jobs) are there once the job is done or
  failed. `404` for an unknown job.
- `DELETE /api/jobs/<id>`: asks the job to stop (`204`; for a finished job nothing changes); `404` for an unknown job.

Themes and pieces compose in process on the job queue (see [Theme bodies and version files](../storage.md#theme-bodies-and-version-files), [Piece bodies and version files](../storage.md#piece-bodies-and-version-files)). Each compose makes its files in a staging folder beside the version's (`v<n>.stage-<k>-<pid>`) and moves it to
the number the version is written with, so two composes of one item may run at once and both versions are kept.
