# Ursatz GUI

A local web UI for the Ursatz platform. You upload a MIDI or MusicXML file,
the Analyzer library analyzes it, and the GUI shows the analysis document
section by section next to the score. Pieces can be grouped into
Collections. This repo is a host/shell around independent tool modules, not
a monolithic application.

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`): the only code that
  calls the Analyzer and Composer libraries. Every tool goes through this
  layer; nothing calls them from route code. The GUI is a client of the
  apps (R-218, R-219): it displays and takes input, and every music answer
  comes from the Analyzer or the Composer. The one library edge left is
  storage: `persistence_service.c` includes `persistence/persistence.h` and
  `collection_service.c` includes `corpus/corpus_api.h`, both from Ursatz.
  No other backend file includes an Ursatz header.
- **Tool modules** (`backend/src/routes/`): independent GUI features
  (Analysis, Corpus and Composer). Each tool registers its own
  HTTP routes under an `/api/<tool_name>/...` prefix and never calls another
  tool directly. See `backend/include/ursatz_gui/tool.h` for the
  registration convention and `backend/src/routes/health.c` as the reference
  tool.
- **Host/shell** (`backend/src/server.c`, `frontend/public/`): starts the
  HTTP server, wires tool routes together, and serves the static frontend
  shell (top bar, tool sidebar, content area) that tool UIs render into.

New tool code goes in `backend/src/routes/`. New shared persistence or query
logic goes in `backend/src/service/`. If you find yourself calling an Ursatz
function from inside `routes/`, it belongs in `service/` instead.

The Analyzer library (`external/ursatz-analyzer`, linked as
`ursatz_analyzer`) is the only analysis path: `analysis_service.c` calls
`input_pipeline_import_file`, `run_analysis_pipeline` and
`analysis_document_build`; the piece detail's note ids, the score
(`analyzer_score_export_musicxml`, `analyzer_imported_meter_defaulted`) and the
playback (`analyzer_playback_build`) also come from the Analyzer. The file
includes no Ursatz header, and the GUI keeps no analysis code of its own.
Analysis therefore changes only when the Analyzer submodule is bumped.

### Backend helper modules

Everything but `main.c` is built as the static library `ursatz_gui_core` (`backend/CMakeLists.txt`); the server
(`ursatz-gui-backend`) is `main.c` linked to it, and each test is built with `ursatz_gui_add_test(name source workdir)`
(`backend/tests/CMakeLists.txt`), which links the same library. A new source file is added to the library once.

- `util/fs_util.c`: `ursatz_gui_dup_string`, `ursatz_gui_copy_message`, `ursatz_gui_iso_utc_now`, `ursatz_gui_path_exists`,
  `ursatz_gui_path_is_dir`, `ursatz_gui_make_dir`, `ursatz_gui_ensure_dir`, `ursatz_gui_write_file_atomic` (writes `<path>.tmp`, then renames), `ursatz_gui_read_file`
  and `ursatz_gui_read_json_object`. Every store and the routes use these; none keeps a copy. `link_service.c` keeps
  its own `iso_utc_now`, which adds milliseconds so two runs in one second still order.
- `util/json_util.c`: the cJSON helpers the services share: `ursatz_gui_json_string` (the string member, `""` when
  absent) and `_string_copy`, `_set_string`, `_attach`, `_add_copy_or_null` and `_add_copy_if`.
- `util/store_lock.h`: `UrsatzGuiLock` (a pthread mutex, an SRWLOCK on Windows) with `URSATZ_GUI_LOCK_INIT` and
  `ursatz_gui_lock_acquire/release`. Each store wraps it in `LOCK()` and `UNLOCK()`.
- `service/piece_lock.c`: `ursatz_gui_piece_lock(id)` and `_unlock`, one mutex per piece directory (see Piece store,
  Concurrency).
- `routes/route_util.c`: the senders every route uses (`ursatz_gui_send_json`, `_send_json_status`, `_send_failure`
  with `{"ok":false,"message"}`, `_send_object`, and `ursatz_gui_reason_phrase`) and `ursatz_gui_read_json_body`, the
  JSON object body of a request (up to 1 MiB; with a status pointer it also sends the 400, 413 or 500 itself). `routes/materials.c` passes
  its own 256 KiB limit to `ursatz_gui_read_json_body_max`.
- `service/compose_common.c`: `ursatz_gui_compose_item`, which the theme and piece services share, the steps around a Composer run
  (update the uses, make the version folder, build the brief and the request, run, add the version, rename the folder
  when another writer added a version). Each service passes an `UrsatzGuiComposeKind` with its own checks, request,
  run, note and result body; there is no compose lock: each compose makes its files in a staging folder
  (`ursatz_gui_compose_stage_make`) and moves it to its version's folder when the version is written.
- `service/job_queue.c` and `service/compose_jobs.c`: the job queue and the compose runs as jobs (`routes/jobs.c` are
  its two routes). `frontend/public/backend-jobs.js` is the client: `runJob` sends a job request, polls every 500 ms
  and resolves with the status and body the route answered before jobs; `sendMaterialsRequest` runs every request
  marked `job: true` through it and shows the stage and a Cancel button while it runs.

### Backend services and who calls the Composer

Each Composer area has one service that makes its Composer API calls; a route parses the request, calls the service
and answers.

| Area | Service | Composer entries |
|---|---|---|
| Motifs | `service/motif_service.c` | `composer_api_motive`, `_motive_from_midi`, `_motive_render`, `_random_motive`, `_recognize`, `_validate`, `_variants`, `_variant_operations`; it also builds the motive request of a motif and analyzes a motif body on save |
| Themes | `service/theme_service.c` | `composer_api_theme` |
| Pieces | `service/piece_service.c` | `composer_api_piece`, `_piece_variations` |
| Export | `service/compose_common.c` | `composer_api_export` |
| Build and vocabulary | `service/composer_client.c` | `composer_api_versions`, `_vocabulary`; the reply type, `ursatz_gui_composer_reply_refused` and `_failure_status` |

`routes/composer.c` answers the status route with `composer_api_versions` itself and the vocabulary route from
`composer_client.c`; `routes/materials.c`, `routes/links.c` and the other routes call no `composer_api_*` entry. The material store's one-time migrations (`ursatz_gui_material_migrate_tones`,
`ursatz_gui_material_clear_themes`) are in `service/material_service.c`, which calls `motif_service.c` to analyze a
motif again.

Piece documents are read only through `service/analysis_service.c`, under the piece lock: `ursatz_gui_analysis_get_document`
(with no names, the whole stored document), `_get_filename` and `_piece_exists`. The link, style and collection services
use these, and no other file names a piece's `meta.json` or `analysis.json`.

On the page, `frontend/public/request.js` (`ApiRequest`) holds `jsonRequest`, `errorText`, `fieldForPath` and
`validateRequest`; the models and views call it and keep no copy.

### Analyzer routes

- `POST /api/analyzer/pieces`: the file as the raw request body, its name
  URL-encoded in the `X-Filename` header. The format is detected from the
  content (`input_format_detect`), not from the filename. Returns
  `{"ok":true,"id":...,"filename":...}`. A body that is neither MIDI nor
  MusicXML gives `415` with `{"error": "unsupported format"}`; a file the
  Analyzer cannot import or analyze gives `422` with
  `{"error": "import failed"}`; nothing is stored in either case.
- `GET /api/analyzer/pieces`: the stored pieces, newest upload first, then by
  id: `{"ok":true,"pieces":[{"id","filename","format","uploaded_at","composer","source","tags":[...],"notation","analysis_status"}]}`.
  `analysis_status` is `current`, `stale_schema`, `stale_analyzer`, `stale_ursatz` or `unreadable`, from `meta.json`
  alone; the Library shows a stale badge for any other value.
  Optional query parameters filter the list, all given ones must match:
  `composer`, `source` and `tag` (case-insensitive, URL-decoded),
  `unassigned=composer` and `unassigned=source` (may appear twice) for pieces
  with no composer or source, and `untagged=1` for pieces without tags.
- `GET /api/analyzer/groups`: the Tags panel's data:
  `{"ok":true,"composers":[{"value","count"}],"sources":[...],"tags":[...],"unassigned":{"composer":n,"source":n},"untagged":n,"total":n}`.
  Values are distinct case-insensitively and sorted by count, then value; the
  shown spelling of a composer or source is the one most pieces use (ties by
  byte order). A tag repeated within one piece counts once.
- `GET /api/analyzer/pieces/<id>`:
  `{"ok":true,"id","filename","format","uploaded_at","notation","analysis_status","note_xml_ids":[...],"document":{...}}`.
  A stale analysis is analyzed again before it is read; when that fails, the stored document is served with
  `analysis_status` naming why and `"stale"` set to the same name (absent when current). The header shows a stale status.
  `notation` is the piece's notation override or `null`. `document` is the analysis document as stored. `note_xml_ids[i]` is the
  `<note xml:id>` the score export writes for note `i` of the source; a
  claim's `note_ids` are indices into this array. An unknown id returns
  `{"ok":false,"message":"piece not found"}`.
  With `?document=summary`, `document` holds only `schema_version`, `metadata` and `meter` (what the header and the
  Provenance list read) and the reply gains `"document_sections"`, the names of every section of the stored
  document. The note ids and the rest of the reply are the same.
- `GET /api/analyzer/pieces/<id>/document?sections=a,b`: `{"ok":true,"sections":{"a":...,"b":...}}`, only those
  sections of the stored `analysis.json`, read without importing the source. A name the document lacks is left out.
  Names are lowercase letters, digits and `_` (at most 64 names of 64 characters); a missing, empty or malformed
  `sections` is `400`; an unknown piece is `404`.
- `PATCH /api/analyzer/pieces/<id>`: a JSON object with any of `filename`
  (non-empty string), `composer`, `source` (strings; `""` clears), `tags`
  (array of strings, replaces all), `add_tags` and `remove_tags` (arrays of
  strings, applied after `tags`), and `notation` (see below). Unknown members
  are ignored. `meta.json` is written once. Returns `{"ok":true,"piece":{...the list entry...}}`, `404`
  for an unknown id, `400` with `{"ok":false,"message":"<reason>"}` for
  invalid metadata (`"composer is too long"`, `"source is too long"`,
  `"invalid tag"`, `"too many tags"`, `"invalid notation"`) and nothing written, and
  `{"ok":false,"message":"nothing to update"}` when none of the members is
  given.
  - `notation`: `{"meter":"3/4","value_scale":"2","pickup":"1/4"}` or `null`
    to clear. Each member is optional and `""` leaves it unset: `meter` is
    `beats/beat_type`, `value_scale` one of `1/4`, `1/2`, `1`, `2`, `4` (every
    start and duration is multiplied by it), `pickup` a fraction of a whole
    note (after scaling) shorter than one measure. It is stored in
    `meta.json` and read by the Analyzer's `notation_reading_*` functions; an
    invalid value, or a pickup that does not fit the meter, answers `400`
    `"invalid notation"` and the piece keeps its old reading. A changed
    notation analyzes the piece again before the answer. The analysis (meter
    source `override`), the score (`/musicxml`) and playback (`/playback`, its
    tempo scaled so the piece sounds as fast as before) all use it. The list
    entries and `GET /api/analyzer/pieces/<id>` include `notation` (`null`
    when unset).
- `POST /api/analyzer/pieces/<id>/reanalyze`: runs the Analyzer again on the
  stored source. Returns `{"ok":true,"id":...}`, `404` for an unknown id, or
  `422` with `{"error": "import failed"}` when the source no longer imports.
  The piece is unchanged unless the new analysis succeeded.
- `DELETE /api/analyzer/pieces/<id>`: removes the piece's directory.
- `GET /api/analyzer/pieces/<id>/musicxml`: the piece's score as MusicXML
  (`application/xml`), written by the Analyzer (`analyzer_score_export_musicxml`)
  from the stored source: its parts and staves, its real meter and its key
  signatures; the GUI adds the title and the composer credit, which the
  Analyzer leaves out. Tempo markings and dynamics are left out. Errors are JSON with
  an error status: unknown id `404`; a stored source that no longer imports
  `422` with `{"ok":false,"message":"could not import the stored source"}`;
  a failed export `500` with
  `{"ok":false,"message":"failed to export piece","code":"<AnalyzerStatus name>"}`;
  anything else `500` with `{"ok":false,"message":"failed to export piece"}`.
- `GET /api/analyzer/pieces/<id>/musicxml/meta`:
  `{"ok":true,"meter_defaulted":<bool>}`, true when the source gave no meter
  and the barlines come from a default 4/4 (the export then carries
  `ursatz:meter-defaulted`). Answered from the cached import
  (`analyzer_imported_meter_defaulted`); no score is exported. `404` for an unknown piece, `422` when the source no
  longer imports.
- `GET /api/analyzer/pieces/<id>/playback`: what a browser needs to play the
  piece and follow it, built by the Analyzer (`analyzer_playback_build`,
  `analyzer_playback_to_json`) from the same import as the score export (so
  `notes[i]` is note `i` of a claim's `note_ids` and of `note_xml_ids`):
  `{"ok":true,"tempo_source","tempo":[{"at":[n,d],"qpm"}],"parts":[{"index","name","program"}],"end":[n,d],"notes":[{"i","on":[n,d],"len":[n,d],"pitch","part"}]}`.
  Positions and lengths are whole notes from the start, as `[num, den]` in
  lowest terms; `pitch` is the MIDI note number; `part` is the MIDI track chunk
  or MusicXML part index. `parts` lists each distinct part ascending, with the
  MusicXML part name (`""` for MIDI) and `program` 0 (a note carries no
  program). `end` is the largest `on + len`.
  - Tempo: `tempo` is strictly ascending by `at`, `qpm` in quarter notes per
    minute to 3 decimals. A MIDI file gives its set-tempo regions and
    `tempo_source` `"file"`. A MusicXML score with a declared tempo gives it at
    0, then one region per metronome mark (`qpm = bpm * beat unit * 4`; a mark
    at an earlier region's position replaces it), and `"score"`. A source with
    no tempo gives `[{"at":[0,1],"qpm":120}]` and `"default"`.
  - Errors: unknown id `404` `{"ok":false,"message":"piece not found"}`; a
    stored source that no longer imports `422` with
    `{"ok":false,"message":"could not import the stored source"}`; anything else
    `500` with `{"ok":false,"message":"failed to build playback"}`.

### Corpus routes

The Collections tool keeps its `/api/corpus` paths. Collections are owned by
the GUI (`service/collection_service.c`), not by Ursatz's `corpus_api`:

- `POST /api/corpus`: body `{"name":"<name>"}`, with an optional
  `"description":"<text>"` and an optional `"id":"<collection-id>"` (accepted
  when it is a valid, unused id). Without an id, one is generated (`col-...`).
  Returns `{"ok":true,"id","name","description","piece_count","updated_at"}`
  or `{"ok":false,"message":...}`. A duplicate id fails.
- `GET /api/corpus`: lists all collections, sorted by name (ASCII
  case-insensitive), then id:
  `{"ok":true,"corpora":[{"id","name","description","piece_count","updated_at"},...]}`.
- `GET /api/corpus/<id>`: one collection, the list fields plus `created_at`
  and `pieces` (the piece ids, in add order). `404` when absent.
- `PATCH /api/corpus/<id>`: body with `name` and/or `description` (`""`
  clears the description). Answers with the list fields. A body with neither
  answers `{"ok":false,"message":"nothing to update"}`; `404` when absent.
- `DELETE /api/corpus/<id>`: deletes the collection. Its pieces are not
  touched. `404` when absent.
- `POST /api/corpus/<id>/pieces`: body `{"piece_id":"<id>"}` or
  `{"piece_ids":["<id>",...]}` (at most 5000). Adds the ones that are not
  members yet and answers `{"ok":true,"added":n,"already":n}`; a piece that is
  already a member (or repeated in the request) is counted, not an error. An
  id with no `pieces/<id>/meta.json` is `400`
  `{"ok":false,"message":"unknown piece: <id>"}` and nothing is added; an
  unknown collection is `404`.
- `DELETE /api/corpus/<id>/pieces/<piece_id>`: removes a piece id. Removing
  one that is not a member fails with `{"ok":false,...}`.
- `GET /api/corpus/<id>/pieces`: lists the piece ids in a collection:
  `{"ok":true,"pieces":[{"id":...},...]}`.

Other failures (bad id, bad name, malformed body) return HTTP 200 with
`{"ok":false,"message":...}`. `<id>` and `<piece_id>` path segments are
validated against the same alnum, `-`, `_` whitelist the Analyzer uses for
piece ids (1 to 64 characters). Names are normalized like piece metadata
values (trimmed, whitespace collapsed, at most 120 bytes, never empty);
descriptions the same with at most 500 bytes and may be empty.

Storage: one file per collection, `collections/<id>.json`, with `id`, `name`,
`description`, `pieces` (ids in add order), `created_at` and `updated_at`
(ISO 8601 UTC). A write goes to a temporary file that is renamed over the old
one, and one mutex serializes the writes. A collection is a fixed set of
piece ids: tagging a piece later does not add it.
`DELETE /api/analyzer/pieces/<id>` also removes the id from every collection
that holds it (a service call, `ursatz_gui_collection_remove_piece_everywhere`).

One-time import: when `collections/` does not exist yet and `data/corpus.db`
(the old Corpus store) does, the server start copies each corpus into a
collection (same id and name, no description) with the piece refs whose
`pieces/<id>/meta.json` exists; the others are dropped and counted on stderr.
`collections/.imported-from-corpus-db` marks it done and `data/corpus.db` is
left in place. If the import fails, the server does not start and the next
start tries again.

`GET /api/health/status` calls the persistence service to confirm
`persistence_open` succeeds against a test database path, and returns JSON.

### Styles routes

A style is a StyleModel file built from a set of pieces through the
Analyzer's style profile (`service/style_service.c`). The Composer reads it
with `--style` (the GUI option for that comes later). Styles are not
collections and not tags: they are a count of harmony, rhythm, melody and
form labels over the pieces they were built from.

- `POST /api/styles`: body `{"name"?, <source>}` with exactly one source (else
  `400`):
  - `"filter": {"composer"?, "source"?, "tag"?}` (the pieces that match, as the
    Library filters them; at least one value),
  - `"collection_id": "<id>"` (the pieces of that collection; `404` when it
    does not exist),
  - `"piece_ids": ["<id>",...]` (at most 5000; repeated ids count once),
  - `"brief": {"scopes": [scope,...], "piece_ids": [...], "style_ids": [...]}`
    (a union, at most 16 scopes, 5000 ids and 16 styles, at least one entry; a
    scope is an object as the links routes take it. The pieces are, in id
    order without repeats, those of every scope, the ids, and each style's
    pieces from its last build. A scope that names no piece adds none; an
    unknown collection, subject, project or style is `404`).

  Answers `{"ok":true,"style":{list entry},"skipped":[{"id","reason"}]}`.
  `422` `{"ok":false,"message":"no analyzable pieces"}` when no piece could be
  added; nothing is written then. `name` is normalized like piece metadata
  values (at most 120 bytes, else `400`). When it is empty the default is
  `Composer: X`, `Source: X`, `Tag: x` joined with ` · ` for a filter,
  `Collection: <name>` for a collection, `Selected pieces` for ids and `Brief`
  for a brief.
- `GET /api/styles`: `{"ok":true,"styles":[list entry,...]}`, newest update
  first. A list entry is `{"id","name","piece_count","updated_at","source"}`,
  where `source` is `{"kind":"filter"|"collection"|"pieces"|"brief","filter":{"composer",
  "source","tag"}|null,"collection_id":"..."|null,"piece_ids":[...]|null,
  "brief":{"scopes","piece_ids","style_ids"}|null}`.
- `GET /api/styles/<id>`: `{"ok":true,"style":{...}}`, the list entry plus
  `created_at`, `pieces` (the ids used), `skipped` and `statistics`:
  `{"<dimension>":{"<category>":{"labels":[{"label","count","total"}],"more":n}}}`
  with dimension `harmony`, `rhythm`, `melody` or `form`, the labels of a
  category by count descending (at most 12) and `more` the number cut.
- `POST /api/styles/<id>/rebuild`: builds the style again from its stored
  source over today's pieces (same id and name, files replaced in place,
  `updated_at` set) and answers like `POST /api/styles`. `422` when no piece
  is left or the collection, subject, project or style it names is gone; the
  style stays as it was. A brief is resolved again.
- `DELETE /api/styles/<id>`: `{"ok":true}`.

An unknown style id is `404` `{"ok":false,"message":"style not found"}` on
every route.

The backend can also build a style into one model file at a path without a
store entry (`ursatz_gui_style_build_file`, written to a temporary file and
renamed); a theme version uses it for its brief.

A piece that cannot be added is skipped with a reason and the build goes on:
`not found` (no such piece), `re-analysis failed` or `unreadable analysis`.
When a piece's stored analysis is not current (another schema, Analyzer version or Ursatz commit; see Staleness
keys), the piece is analyzed again first, so older pieces still count. Only analysis documents are read,
never the scores. A build runs on the request thread and one mutex serializes
the style store.

Storage: `styles/<id>/style-model.json` (written by `style_model_file_write`,
the same bytes for the same pieces) and `styles/<id>/meta.json` (`id`, `name`,
`created_at`, `updated_at`, `source`, `pieces`, `skipped`, `piece_count`),
each written to a temporary file and renamed. Ids are `sty-<16 lowercase hex>`.
A style is a snapshot: tagging, untagging or deleting a piece later changes
it only when it is rebuilt, and a Composer plan keeps naming the file, so a
rebuild changes what that plan's later steps use.

### Motive links routes

Motive links show which motive families of different pieces are variants of one another: for each family the
Analyzer finds the nearest families of other pieces and the chain of at most two relations that turns one prototype
into the other (`ursatz_analyzer/motive_links.h`). `service/link_service.c` is the only code that calls it; it reads
only analysis documents, never the scores.

A scope names the pieces to link:
`{"kind": "all" | "collection" | "filter" | "subject" | "project", "collection_id"?, "filter"?: {"composer"?,
"source"?, "tag"?}, "subject_id"?, "project_id"?}`.

- `all`: every Library piece.
- `collection`: the pieces of that collection (`404` when it does not exist).
- `filter`: the pieces that match, as the Library filters them; at least one value (else `400`).
- `subject`: the Library pieces the subject links (`404` when it does not exist).
- `project`: the Library pieces linked to every subject that links at least one item of the project (`404` when it
  does not exist).

Pieces are taken in id order. A scope that names no piece is `422`.

- `POST /api/links/runs`: body `{"scope": {...}}`. Starts a run on a worker thread and answers `202` `{"ok":true,
  "run":{...}}`. `400` for a scope that cannot be one (an unknown kind, a missing id, a filter without a value),
  `404` for an unknown collection, subject or project, `422` for an empty scope, `409` while another run is in
  progress (one run at a time).
- `GET /api/links/runs`: `{"ok":true,"runs":[{"id","scope","status","created_at","finished_at","done","total"},...]}`,
  newest first.
- `GET /api/links/runs/<id>`: `{"ok":true,"run":{...}}`, the whole run; `404` when unknown. While it runs, `done` and
  `total` are the pairs compared so far and the pairs to compare.
- `DELETE /api/links/runs/<id>`: `{"ok":true}`; `404` when unknown, `409` while it runs.
- `GET /api/links/pieces/<piece id>`: `{"ok":true,"links":[{"run","scope","link"},...]}`, the links with this piece on
  either side, taken from the newest done run of each scope.

A run is `{"id","scope","created_at","finished_at","status","message","done","total","skipped","result"}`. `status`
is `running`, `done` or `failed` (`message` says why: `no analyzable pieces`, `linking failed`, `interrupted`).
`skipped` lists `{"id","reason"}` for each piece that was left out, as styles do: `not found`, `re-analysis failed` or
`unreadable analysis`. On `done`, `result` is the Analyzer's `motive_links_render` output
(`schema/motive-links.v1.schema.json`): `pieces`, `pairs`, `compared`, `cached`, `cache_reset`, the `links` (each
with ends `a` and `b`, `reversed`, the `steps` as text, the `distance` and the `similarity` by dimension) and the
`groups` of linked families. Every link end and group member carries a `title` the route adds: the piece's filename
without its extension, or its id when the piece is gone.

A piece whose stored analysis has no motive features (an older analyzer version) is analyzed again first, as the
re-run route does, and added again; one that still fails is skipped.

Storage: `links/motive-link-cache.json` is one cache for every scope, keyed by the two sketches' fingerprints and valid
only for the same Ursatz commit, analyzer version and link options, so a pair compared for one scope is not compared
again for another and a second run over the same scope reports `compared` 0. Each run is
`links/runs/<id>/run.json`, written to a temporary file and renamed; ids are `lnk-<16 lowercase hex>`. Progress is
kept in memory and written to `run.json` at most once a second and at the end. A server restart marks a run left
`running` as `failed` with `interrupted`.

### Materials and subjects routes

Both stores are described under "Material store" and "Subjects" above; these
routes only dispatch into them (`routes/materials.c`). Every answer is JSON.
A success is `{"ok":true,...}`; a failure is `{"ok":false,"error":"..."}` with
`400` for an invalid id, name, kind, type or body, `404` for something not
found, `409` for a project that still holds items or an item that other items
use, `405` for a method the route does not take and `413` for a request body
over 256 KB.

Projects:

- `GET /api/materials/projects`: `{"projects":[{...,"item_count"}]}` by name.
- `POST /api/materials/projects`: `{"name","description"?}` answers `{"project"}`.
- `GET|PATCH|DELETE /api/materials/projects/<id>`: read, change `name` and/or
  `description`, delete (`409` while it holds items).

Items:

- `GET /api/materials/items?project=<id>&type=<motif|theme|piece>`: `{"items"}`,
  newest update first; both filters are optional.
- `POST /api/materials/items`: `{"type","name","project_id","body"}` answers
  `{"item"}` (the item meta) with the body stored as version 1.
- `GET /api/materials/items/<id>`: `{"item","body","uses_status","used_by",
  "subjects"}`: the meta, the current version's body, how each use's source
  stands, the items that use this one and the subjects that link it.
- `PATCH /api/materials/items/<id>`: `{"name"?,"project_id"?}` renames and/or
  moves (a missing project is `404` before anything changes).
- `DELETE /api/materials/items/<id>`: `409` with `{"ok":false,"error",
  "used_by":[...]}` while other items use it; `?force=1` deletes anyway and
  the users' uses then report `missing`.
- `POST /api/materials/items/<id>/versions`: `{"body","note"?}` writes the next
  version and answers `{"item"}`.
- `GET /api/materials/items/<id>/versions/<n>`: `{"number","body"}`; `404`
  outside 1 to the current version.
- `POST /api/materials/items/<id>/uses/<source>/update`: writes a new version
  of the item with every use of `<source>` moved to the source's current
  version; answers `{"item"}`.
- `GET /api/materials/items/<id>/versions/<n>/files/<name>`: one of the files of a composed theme or piece version.
  A theme version's: `request.json`, `brief.style.json` and `plan.json` as `application/json`, `theme.mid` as
  `audio/midi`, `theme.musicxml` as `application/xml`. A piece version's: `request.json`, `brief.style.json`,
  `plan.json`, `ending-plan.json` and `theme-<section id>.plan.json` as `application/json`, `loop.mid` and `ending.mid`
  as `audio/midi`, `loop.musicxml` and `ending.musicxml` as `application/xml`. Any other name, a version without the
  file or an unknown item is `404`.
- `POST /api/materials/items/<thm-id>/compose`: `{"seed"?,"update_uses"?}` composes the theme into a new version
  (R-166, R-167) and answers `{"item"}`. With `update_uses`, every updated use first moves to its source's current
  version (a version of its own each; they stay if the run then fails). The request is built from the current
  version: each slot motif from its use's copy; the pool, every motif item whose `derived` chain (at most 32 steps
  through `derived.source_item`) leads to a slot's item, flag `approved` (and `unflagged` with the default pool), at
  most 64 for each motive, never `rejected`, with its current degrees, harmony, features, chain text and functions;
  the brief built with the styles code into `brief.style.json`. It runs `composer_api_theme` on a job and answers
  `202 {"job"}`; the job's `result` is `{"item"}`. The files `plan.json`, `theme.mid` and `theme.musicxml` are written
  from the response. The new version is the body as it was plus `result: {seed, summary (the document `theme --format
  json` printed, with `files`), brief: {piece_count, skipped}, files}` with the note `composed seed <n>`. `seed` in the
  request wins over the body's. A bad body or an item that is not a composable theme is `400` at once; the job ends
  with `400` for a request the Composer refuses (its message), `404` for an unknown item, `422` when the Composer is
  stuck or the brief has no piece it can use, and `499` when it is cancelled. A failed or cancelled run writes no
  version and leaves no folder.
- `POST /api/materials/items/<pce-id>/compose`: `{"seed"?,"update_uses"?}` composes the piece into a new version
  (R-173, R-174) and answers `{"item"}`. `update_uses` is as for a theme. The request is built from the current
  version: the motive and each motif fill from their uses' copies (degrees, pickup, harmony, features, role), each
  motif fill with the pool of its saved forms as for a theme (the piece's `pool`), a theme fill as the absolute path
  of that theme version's `plan.json` copied into the new version's folder as `theme-<section id>.plan.json`,
  progressions as given, and the brief built into `brief.style.json`. It runs `composer_api_piece` on a job (the plan
  of a theme fill is read into the request, its path kept as the source) and writes `plan.json`, `ending-plan.json`,
  `loop.mid`, `loop.musicxml`, `ending.mid` and `ending.musicxml` from the response. The new version is the body as it was plus
  `result: {seed, summary (the command's JSON), brief: {piece_count, skipped}, loop: {start, end}, files}` with the
  note `composed seed <n>`. Errors are those of a theme compose.
- `POST /api/materials/items/<pce-id>/versions/<n>/regenerate`: `{"section_id","lane"?,"seed"?}` writes one section
  of a composed version (one with a `result`) again as the next version and answers `{"item"}`. `lane` is `harmony`,
  `bass`, `melody`, `inner` or `all` (default: the whole section); a section filled with a theme takes `harmony` or
  `bass` only, and an ending section is refused. It runs `composer_api_piece` on a job with `from` the plan of version
  `n` and `regenerate: {section, lanes}`, and the same eight outputs; without `seed` the Composer picks one. The new version is the source's body with `result` replaced and the note `regenerated <section id> <lane>
  seed <n>`; its folder holds its own `request.json` and copies of the theme plans and the style file. Errors: `400`
  for an item that is not a piece, a version that was never composed, an unknown section or lane, a lane a theme
  section does not take, or a bad seed; `404` for an unknown item or version; `422`, `503`, `504` as for compose. A
  failed run writes no version and leaves no folder.
- `POST /api/materials/items/<pce-id>/versions/<n>/variations`: `{"section_id","lane","count","seed"?}` (`count` 1 to
  8) makes variations of a section into the set folder `v<n>/variations-<k>/` and answers `{"ok","set":k,"variations":
  [{"index","seed","score":{"numerator","denominator"},"seam","heard"}]}`, `heard` being the cadence heard at the end
  of the section. Fewer than `count` may come back. Nothing becomes a version. Harmonize is `lane` `harmony` (also on a
  section whose melody is not a theme's). Errors as for regenerate.
- `GET /api/materials/items/<pce-id>/versions/<n>/variations/<k>/<file>`: `result.json` or `variation-<i>.json`,
  `.mid`, `.musicxml` of a set (`application/json`, `audio/midi`, `application/xml`); any other name, or a set the
  version does not hold, is `404`.
- `POST /api/materials/items/<pce-id>/versions/<n>/variations/<k>/choose`: `{"index"}` makes variation `index` of set
  `k` the next version and answers `{"item"}`: the variation's plan is the version's loop plan, and the ending version is
  built from it (`piece --from variation-<i>.json --request <new>/request.json --regenerate none`, the eight outputs).
  The result is the source's with the Composer's summary, the variation's loop version and its seed; the note is `chose
  variation <i> seed <n>`. `404` for a set or variation that is not there, `400` for a bad index.
- `GET /api/materials/variant-operations`: `{"operations"}` from
  `composer_api_variant_operations`.
- `POST /api/materials/items/<id>/variants`: `{"chain"?,"count"?,"seed"?,"functions"?,"distance"?}`
  runs `composer_api_variants` on a job over the motif's stored degrees, form, meter, key and pickup.
  `chain` is `{"steps":[{"op","params"?}]}` (random chains when absent),
  `count` 1 to 16 (default 6), `seed` a whole number from 0. Answers the
  Composer's `{seed, source, variants}` as the job's `result`; nothing is saved.
- `POST /api/materials/items/<id>/variants/save`: `{"variant","name"?,
  "flag"?}` with one candidate from the route above. Creates a motif in the
  source's project with the candidate's notes and degrees, the source's role,
  meter, key and pickup, and `derived` `{source_item, source_version,
  operations, seed, kind, kept, changed, harmony: null, functions: [], flag}`
  (`flag` `approved`, `rejected` or `unflagged`, default `unflagged`). The name
  defaults to `<source> <op>[ + <op>]`; version 1's note is the derivation
  line. Answers `{"item"}`.
- `POST /api/materials/items/<id>/flag`: `{"flag","kind"?}` writes a new
  version that changes only `derived.flag` (and `derived.kind`: `exact`,
  `motive-form` or `variant`); the note is the flag. `400` for a motif
  without `derived`.
- `POST /api/materials/items/<id>/each-op`: `{"seed"?}`, see Try every operation.
- `GET` and `PUT /api/materials/items/<id>/op-checks`: the operation marks.
- `POST /api/materials/items/<id>/promote`: `{"name","project_id","note"?}` on a
  variant writes a new version with `derived.standalone: true` (note default
  `Saved as new motif`), renames the item and moves it to the project. Answers
  `{"item"}`. `400` for an item that is not a variant or a body without a name
  and project, `404` for a project that is not there.
- `POST /api/materials/items/<id>/reroll`: reruns the stored chain, with the
  parameters the seed drew drawn again, on the source's stored version with
  seed + 1000003 (+1 while the result equals the current one, 16 tries).
  Writes a new version with the new degrees, seed, kept, changed, kind and
  analysis, the flag reset to `unflagged` and the note `rerolled: <derivation>`. It runs as a job.
  `400` without `derived`, `404` when the source is gone, `409` when no seed
  gives a different motif.
- `GET /api/materials/items/<id>/variant-tree`: `{"tree"}`, a node
  `{id, name, current_version, standalone, derived, children}` for the item and for every
  motif whose current `derived.source_item` leads back to it, built from one
  scan of the store.

Subjects:

- `GET /api/subjects`: `{"subjects"}` with `item_count`, `piece_count` and
  `related_count`; `GET /api/subjects/groups`: `{"groups"}`, four kinds in the
  order character, place, idea, note.
- `POST /api/subjects`: `{"name","kind","note"?}` answers `{"subject"}`.
- `GET|PATCH|DELETE /api/subjects/<id>`: read, change `name`, `kind` and/or
  `note`, delete.
- `POST|DELETE /api/subjects/<id>/items/<item>`, `.../pieces/<piece>` and
  `.../related/<other>`: link or unlink an item, a Library piece, or relate
  two subjects. POST answers `{"subject","already"}` (`already` is true when
  nothing changed); DELETE answers `{"subject"}`. A missing item, piece or
  subject is `404`, unlinking something not linked is `404`, relating a
  subject to itself is `400`.

`GET /api/analyzer/pieces/<id>` also returns `"subjects"`: the subjects that
link the piece, `[{id,name,kind}]`.

### Composer routes

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

#### Jobs

`service/job_queue.c` keeps worker threads (`URSATZ_GUI_JOB_WORKERS`, 1 to 8, default 2) and a list of jobs in memory;
it is started in `server.c` before the routes and stopped on shutdown (every job is cancelled, the workers are joined).
A job has a kind, a state (`queued`, `running`, `done`, `failed`, `cancelled`), the `stage`, `done` and `total` the
Composer's progress callback reports (`"melody"` 6 of 9) and, once it ends, the status and body its route used to send.
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
- `GET /api/jobs/<id>`: `{"ok": true, "job": {"id", "kind", "state", "stage", "done", "total", "http_status"?,
  "result"?}}`. `http_status` and `result` (the body the route answered before jobs) are there once the job is done or
  failed. `404` for an unknown job.
- `DELETE /api/jobs/<id>`: asks the job to stop (`204`; for a finished job nothing changes); `404` for an unknown job.

Themes and pieces compose in process on the job queue (see Theme bodies and version files, Piece bodies and version
files). Each compose makes its files in a staging folder beside the version's (`v<n>.stage-<k>-<pid>`) and moves it to
the number the version is written with, so two composes of one item may run at once and both versions are kept.

## Frontend

Plain static HTML, JS and CSS served by the backend from `frontend/public/`.
No framework or build pipeline. Theming lives entirely in `style.css` as CSS
custom properties on `:root`, so a theme picker can swap colors without
touching markup or `app.js`. Frontend logic makes no styling decisions.

The Corpus tool is shown as "Collections" in the sidebar and view title; the
API, module and route names stay `corpus`.

The Library, a collection's detail and the add-pieces picker share one table
(`buildPieceTable` in `app.js`): a select column, Title, Composer, Source,
Tags and Actions.

- **Cards**: name, a one-line description, "<n> pieces" and the top 3
  composers and top 3 tags among the collection's pieces. The `⋯` button
  opens Rename, Edit description and Delete (the confirm names the
  collection; its pieces are not deleted). The header form takes a name and
  an optional description.
- **Detail**: the title and description each have an inline editor. The
  table lists the members with search, Composer, Source and Tag filters
  applied in the browser. Row actions are Open (analysis) and Remove from
  collection; checked rows can be removed together.
- **Add pieces**: opens a picker with the Library filters (applied by the
  backend) over pieces not yet in the collection. `Add selected (n)` and
  `Add all matching (n)` each send one `POST /api/corpus/<id>/pieces` with
  `piece_ids`; the result reads "Added 5, 2 were already in it".
- **Save as collection**: shown in the Library while a filter is on. It
  creates a collection named from the filter ("John Field", "Tag: whimsical",
  parts joined with " · ") and adds every shown piece in one request, then
  opens it. The collection is a snapshot and does not follow later tag
  changes; the sidebar Tags panel is the live grouping.
- **Build style**: builds a style (a StyleModel learned from pieces, see
  Styles routes) from a set of pieces. The button appears in three places:
  the Library while a filter is on (next to Save as collection), a
  collection's detail, and on each Tags panel row (a small ♫ button shown on
  hover and focus, labelled "Build style from <row>"). It opens an inline name
  prompt prefilled with the default name ("Composer: X", "Source: X",
  "Tag: x" joined with " · ", "Collection: <name>" or "Selected pieces");
  Build sends `POST /api/styles` and the button reads "Building..." until the
  answer. A plain composer, source or tag filter builds from that filter;
  Unassigned, Untagged and search have no backend form, so those build from
  the ids of the pieces shown. A failure shows under the button.
- **Styles**: a section under the collection cards listing each style with
  its name, source ("Tag: whimsical"), piece count and updated date, and the
  actions Open, Rebuild (replaces the style in place from today's pieces) and
  Delete (confirm). A finished build opens the style detail with a result line
  ("Built from 12 pieces, 2 skipped").
- **Style detail**: the source and counts, the pieces used (each a link to its
  analysis), the skipped pieces with the reason, and what the style learned:
  for each dimension and category the most common labels as a bar of
  `count/total` with the fraction as text, then "and <n> more".
  The four motive categories read "Motive degrees", "Motive intervals", "Motive rhythms" and "Non-chord tones in
  motives"; the rhythm categories `tempo` and `note-density` read "Tempo (quarter notes per minute)" and "Note
  density (attacks per beat)". A style built from a brief reads "Brief" as its source (briefs are made in the Themes
  view; the Collections view builds none).

### Score layers

`frontend/public/score-layers.js` (global `ScoreLayers`, no DOM) is the data
model of the claim layers drawn on the score. Each layer turns sections of
the analysis document into items `{key, layerId, text, label, start, end, at,
noteIds, style, claim, parent}`, with `start` and `end` in whole notes from
the piece start. A layer has one of three kinds: `ribbon` (a bar over the
claim's span in a lane above or below the staff), `mark` (a short label at
one point in a lane) or `notes` (the claim's noteheads colored, no lane).

| Layer | Group | Kind | Color | Items from |
|---|---|---|---|---|
| Keys | Harmony | ribbon above | `#1f6fd1` | `keys.regions` |
| Key changes | Harmony | ribbon above | `#7b4fd8` | `tonicizations` (dashed), `modulations` (solid) |
| Chords | Harmony | mark below | `#0b7a70` | `chords`, the Roman numeral at the span start |
| Cadences | Harmony | mark above | `#d0342c` | `cadences`, abbreviated (PAC, IAC, HC, ...) at the arrival |
| Non-chord tones | Harmony | notes | `#8a6a00` | `non_chord_tones` |
| Prolongations | Harmony | ribbon above | `#3b8a2f` | `prolongations` |
| Progressions | Harmony | ribbon above | `#b5530a` | every `progressions[].occurrences[]` |
| Form sections | Structure | ribbon above | `#3f4b5e` | `form.sections` |
| Themes | Structure | ribbon above | `#c2185b` | `themes` |
| Phrases | Structure | ribbon above | `#006f99` | `phrases` and their `subphrases` (thin) |
| Motives | Structure | notes | `#a2369a` | every `motives[].occurrences[]` |
| Schemas | Structure | ribbon above | `#7a5a3a` | `schemas` |
| Texture | Texture and melody | ribbon above | `#5f6b00` | `texture` |
| Melody | Texture and melody | notes | `#2c3fa8` | `melody` |

Any other top-level section holding claims (objects with `span` or
`location`), directly or in arrays inside an object, becomes an Other layer
(`other:<section>`, gray `#6b7280` ribbon above), so a new Analyzer section
shows up without a GUI change. `schema_version`, `metadata`, `meter`,
`notes`, `chord_vocabulary`, `harmonic_rhythm`, `outer_voices`, `markings`,
`voice_leading` and `rhythm` are never layers. Overlapping ribbons are packed
into at most four rows; the rest are flagged as overflow. When two notes
layers share a note, the one higher in the table colors it.

Which layers are on is remembered in `localStorage` under
`ursatz.scoreLayers.v1` as a JSON array of layer ids. Nothing is on by
default; the presets are Harmony (keys, key changes, chords, cadences) and
Structure (form, themes, phrases, cadences). Ids a piece lacks are kept for
the next piece.

### Score geometry

`frontend/public/score-geometry.js` (global `ScoreGeometry`) turns positions
into places on the rendered score. `extractLayout(osmd)` reads OSMD once into
a plain layout, in CSS pixels from the OSMD SVG's top-left:
`{unit, systems: [{index, top, bottom, left, right, measures: [{number, start,
end, x, width, entries: [{t, x}]}]}]}`. Every other function takes that
object, so tests use hand-built layouts. Times in a layout are OSMD absolute
timestamps in whole notes.

- `xAt(layout, t)` gives `{system, x}`; inside a measure x is interpolated
  between the staff entries. `tAt(layout, system, x)` is its inverse.
- `spanSegments(layout, t1, t2)` gives one segment per system a span
  touches; a segment that continues on the next system is open at that end.
- `calibrateOffset(pairs)` returns the timeline offset, the median of
  `osmd - ursatz` over the first 50 matched notes, plus the number of pairs
  that disagree. OSMD time is the Analyzer position plus the offset, which
  absorbs pickup measure numbering.
- `laneStack(lanes)` stacks lanes from the staff outward, 18 px per row, 4 px
  between lanes and 8 px before the first. Above the staff, nearest first:
  cadences, Other layers, progressions, prolongations, key changes, keys,
  texture, schemas, phrases, themes, form sections. Chords go below.
  `systemSpacingUnits` converts the stack heights into OSMD
  `MinimumDistanceBetweenSystems` and `PageTopMargin`, never below the
  defaults.

## Building

```
git submodule update --init --recursive
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires CMake 3.20 or newer, a C11 compiler, SQLite development headers and
pkg-config. Ursatz is a git submodule at `external/ursatz`, pinned at PR #195
(`d2b85ce`). The Analyzer (version 0.6.0) is a second submodule,
`external/ursatz-analyzer`, pinned at PR #67 (`ee8a901`). cJSON 1.7.18 is
vendored at `external/cjson` and added first, so the Ursatz Analyzer reuses it.
`scripts/check-pins.sh` fails when a pin is not on its remote's `main` or when
one repository is pinned at two commits; CI runs it after the submodule
checkout.

Two more checks keep the GUI a client and CI runs both (the build is
configured with `URSATZ_LEGACY_INCLUDE_DIRS` OFF, so only staged public headers
are visible):

- `scripts/check-library-includes.sh build` fails when `backend/src` or
  `backend/include` includes an Ursatz header other than the two storage edge
  headers above, an Analyzer header outside `ursatz_analyzer/`, a Composer header
  other than `ursatz_composer/composer_api.h`, or any `*_internal.h`.
- `scripts/check-frontend-theory.sh` fails when a file under `frontend/public`
  (not `vendor/`) defines `playbackFromMidi`, `randomSettings`, `defaultCadence`,
  `parseHarmonyToken`, `HARMONY_TOKEN`, `KEY_TONICS`, `RANDOM_METERS`,
  `chordProblems` or `numeralsOk`.

Done-when for the client boundary (P27), checked by hand: analyze a reference
piece (same claims and score as before the phase); make the "Knight's house"
theme with the same seeded plan; during a compose `ps` shows no
`ursatz-composer` process; opening a piece imports it once (the import counter);
all checks above pass.
`git submodule update --init --recursive` (or `git clone --recurse-submodules`
up front) is required before the first build. Ursatz's own tests are not
built (`URSATZ_BUILD_TESTS` is OFF). The Composer is a third submodule,
`external/ursatz-composer`, pinned at PR #95 (`10ea20b`, version 0.4.1, API 1.1). The backend links the Composer
library from the submodule (see Composer routes), without the command, and it reuses the cJSON, Ursatz and Analyzer
targets above. If the submodules are missing the build stops: run `git submodule update --init
--recursive` and build again.

## Tests

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

## Out of scope

Native webview packaging, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists). The
Composer has no blend, corpus seeding or explain.
