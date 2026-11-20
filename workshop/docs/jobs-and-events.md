# Jobs and events

How long work runs and how a program follows what the workshop does. [Docs index](README.md).

## The job queue

All long work goes to worker threads, so no request thread waits on it (`workshop/src/jobs/job_queue.c`, R-645). A command that starts one answers at once with `{"job": {"id", "kind", "state"}}` and the GUI route turns that into `202`. A job lives in memory only (a restart loses it); what it changes is committed through the journal by the job's own transactions, and a link run's `run.json` stays its durable record.

| Kind | Started by | Notes |
|---|---|---|
| `theme`, `piece` | `item.compose` | one compose of the item's request |
| `regenerate`, `variations`, `choose` | `item.regenerate`, `item.variations`, `item.choose` | a section of a composed piece |
| `variants`, `each-op`, `reroll` | the motif variant queries and `item.reroll` | candidates are not stored until saved |
| `links` | `links.run` | at most one runs; a second run is `queued` behind it |
| `style` | `style.build`, `style.rebuild` | at most one runs |
| `analysis` | `library.import`, `library.reanalyze` | an import or a re-analysis |

`job_workers` (default 2, 1 to 8 in `ursatz.json`) threads run the queue. A kind can have a limit on how many of its jobs run at once (`workshop_jobs_set_limit`; `links` and `style` start with 1). A finished job is kept for 10 minutes or the newest 64, whichever drops it first.

A job has a state, `queued`, `running`, `done` (its run answered with a 2xx status), `failed` (any other status) or `cancelled`, and reports a stage with a count (`melody 6/9`, `pieces 3/12`) through the common job handle (`common/job.h`: `progress(user, stage, done, total)` and `cancelled(user)`). Long loops check `cancelled` between steps: a style build and a link run between pieces, the motive links between pairs, the Composer at its next report. `job.cancel` stops a queued job at once and a running one at its next check; the run then ends `cancelled` and its transaction, if it had begun one, aborts.

`job.get` answers `{"job": {id, kind, state, stage, done, total, http_status?, result?}}`. When the job has finished, `result` is what the route answered before jobs existed (the same body, `http_status` its status), so a caller that waits for the job gets the same answer as one that never saw it: `BackendJobs.runJob` in the frontend returns that status and body. A job that finds a newer version of its item when it writes fails with `http_status` 409. `job.list` lists the queued and running jobs, newest first.

Queries never write. An analysis that is stale is answered as stored with `stale: true`; the page opens it, sees the flag and starts the re-analysis job itself. Only job code (a style build, a link run) calls `workshop_analysis_ensure_current`.

## Events

A program that follows the workshop subscribes to its event bus (`common/events.h`, R-648), waits for the next event and unsubscribes:

```c
WorkshopEvents *events = workshop_events_subscribe(workshop);
while (workshop_events_next(events, 15000, &name, &json)) { /* name and json are the caller's; free them */ }
workshop_events_unsubscribe(events);
```

An event is a name and a JSON object. Events about data are published after the transaction that made the change has committed (`workshop_txn_commit`), named by the command in its record; the id is the command's target when the service gave one (a subject, a collection or a style made by its command has an empty id in its event).

| Event | Data | When |
|---|---|---|
| `item-changed` | `item_id`, `version` | every item header a transaction wrote |
| `item-removed` | `item_id` | `item.delete` |
| `library-changed` | `piece_id` | a Library command |
| `collection-changed` | `collection_id` | a collection command |
| `subject-changed` | `subject_id` | a subject command |
| `style-changed` | `style_id` | a style command |
| `links-changed` | `run_id` | a link run started, finished or deleted |
| `job-progress` | `id`, `kind`, `stage`, `done`, `total` | a job reports; at most 4 a second per job, always the last |
| `job-done` | `id`, `kind`, `state`, `http_status` | a job ends |
| `analysis-stale` | `piece_id` | `library.piece` found the piece's analysis stale |
| `reset` | none | this subscriber's queue overflowed: read the state again |

A subscriber has a queue of 256 events. A full queue is cleared and holds one `reset`. `workshop_events_next` waits up to `timeout_ms` and answers false on a timeout and once `workshop_events_close` ran; the server calls it before it stops, so the waiting stream threads come back. The bus is made by `workshop_start` and removed by `workshop_stop`; without one, publishing does nothing.

The GUI serves the bus at `GET /api/events` as server-sent events ([the GUI API](../../gui/docs/api/composer-routes.md#events)); the page keeps one `EventSource` and reads again what an event says changed.
