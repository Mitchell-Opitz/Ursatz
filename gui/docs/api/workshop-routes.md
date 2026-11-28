# Workshop routes

The generic routes of the backend: any query or command of the workshop by its name, and the workshop's status. The command line (`ursatz`) calls them when it finds a data folder held by the process that serves them (R-659). [Docs index](../README.md), [the answer shape](envelope.md).

## The routes

| Method and path | Body | Answer |
|---|---|---|
| `POST /api/workshop/query/<name>` | the params object (an empty body is `{}`) | `{"ok": true, ...members}`, or the reply's file |
| `POST /api/workshop/command` | the command object, or a file (below) | `{"ok": true, "journal_seq", ...members}` |
| `GET /api/workshop/status` | none | `{"ok": true, ...}` |

`<name>` is a query of the workshop ([commands](../../../workshop/docs/commands.md)): `project.list`, `item.get`, `job.get`. The Harmony Lab uses `composer.progression` (a progression request written with its MusicXML and playback, nothing saved; with `list_schemas` the names of the progression schemas) and the knowledge queries `knowledge.palette`, `knowledge.find`, `knowledge.show` and `knowledge.check` ([knowledge](../../../workshop/docs/knowledge.md)). A query that answers a file is sent as the file with its content type and, for a download, its file name.

## Commands

A JSON body is the command object exactly as `workshop_command` takes it: `{"command", "actor", "target", "base_version", "params"}`. The actor is the caller's: the backend adds nothing, and the journal records what the body says. The frontend is the caller of every change the page makes (`core/commands.js`): the actor is `michelle`, the target the item or other id, `params` exactly the params the command's schema declares (a member no schema declares is refused at `/params/<member>`), and `base_version` the version the page holds for the target when it holds one (a stale one answers 409). There is no other route that changes anything.

A command that takes a file (`library.import`) is sent with `Content-Type: application/octet-stream`: the body is the file, the command object is percent-encoded in the header `X-Ursatz-Command`, and the file's name in `X-Filename` (decoded as the Library upload decodes it). A file up to 64 MB is taken.

The answer is `{"ok": true, "journal_seq", ...members}`: 202 when the reply holds a job (`"job": {id, kind, state}`; read it with the query `job.get`), 200 otherwise.

## Status

`GET /api/workshop/status` answers the members of `workshop_status` (`composer_version`, `api_version`, `producer_key`, `composition_key`, `plan_format_version`, `analyzer_version`, `analyzer_producer_key`, `data_dir`), `pid` (the process that serves the folder) and `recovery` (what the open recovered; `""` when nothing). The command line compares `data_dir` with the folder it was asked for before it connects.

## Refusals

A refusal is `{"ok": false, "error", "code", "path"}`: `code` is the name of the workshop's status (`request`, `not-found`, `conflict`, ...) and `path` the member at fault (left out when none). The members a failed call carries stay: a stale base version answers 409 with `"code": "conflict"`, `"path": "/base_version"` and `"current_version"`. The HTTP status is the table's (`ursatz_gui_http_status`, [envelope](envelope.md)); a path that names the call's own members (`/name`, `/command`, `/actor`, `/params/...`, `/body`) is a 400, not a 422.

| Case | Status | `code` | `path` |
|---|---|---|---|
| an unknown query | 400 | `request` | `/name` |
| an unknown command | 400 | `request` | `/command` |
| a body that is not a JSON object | 400 | `request` | `/body` |
| a stale base version | 409 | `conflict` | `/base_version` |
