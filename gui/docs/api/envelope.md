# The envelope

How every route of the GUI backend answers. [Docs index](../README.md).

## One shape

Every route under `/api` answers JSON in one of two shapes:

```json
{"ok": true, "...": "the members of the answer"}
{"ok": false, "error": "what is wrong, as text", "code": "request", "path": "/params/count"}
```

`code` is the name of the workshop's status (`request`, `unsatisfiable`, `not-found`, `conflict`, `format`, `io`, `cancelled`, `alloc`, `internal`) and `path` is present only when a member is at fault: a JSON Pointer into the call or into the Composer request. Files (MusicXML, MIDI, downloads) are sent as their own type; a failure to send one is the envelope. The one exception to the envelope on success is `GET /api/events`, a stream ([Events](composer-routes.md#events)); `DELETE /api/jobs/<id>` answers `204` with no body.

## Statuses

The HTTP status is the one real status of the failure (`ursatz_gui_http_status`, the only table; the workshop's own `workshop_http_status` serves job results):

| Status of the workshop | HTTP |
|---|---|
| `REQUEST` | `400`, or `422` when `path` points into a Composer request (a member of a motif, theme or piece body); `413` for a body over its cap |
| `UNSATISFIABLE` | `422` |
| `NOT_FOUND` | `404` |
| `CONFLICT` | `409` (a duplicate id, an item that other items use, a project that still holds items, a version changed since it was read) |
| `CANCELLED` | `499` |
| `FORMAT`, `IO`, `ALLOC`, `INTERNAL` | `500` |

A path no route fits is `404`, a path another method fits is `405`, both in the envelope. A path that points into the call itself (`/params/...`, `/target`, `/bytes`, `/body/...`, `/base_version`) is a `400`; one into the request the Composer reads is a `422`.

## Bodies and caps

One reader (`ursatz_gui_read_body`) reads a request body with a cap: JSON 1 MB, uploads 64 MB, a MIDI motif 256 KB. Over the cap is `413`; an unreadable body is `400`.

## Jobs

A route that starts long work answers `202` `{"ok": true, "job": {"id", "kind", "state"}}`: piece imports and re-analyses (`analysis`), style builds and rebuilds (`style`), motive link runs (`links`, with the run), composes and variants. The job's own answer, the status and body the route would have sent, is `result` and `http_status` of `GET /api/jobs/<id>` once it has ended; the frontend's `BackendJobs.runJob` sends the request, follows the job on the event stream and resolves with that status and body. See [jobs and events](../../../workshop/docs/jobs-and-events.md).

## Base versions

A write to an item that has versions carries the version the page read: `base_version` in the JSON body, or `?base_version=<n>` on a `DELETE`. When the item has moved on the answer is `409` with `"error": "changed since you read it"`, `"path": "/base_version"` and `current_version`. The frontend's `request.js` keeps the version of every item an answer holds and adds `base_version` to these writes. See [commands](../../../workshop/docs/commands.md).
