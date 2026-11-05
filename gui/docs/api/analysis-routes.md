# Analysis routes

The Analyzer, Corpus, Styles and Motive links routes. [Docs index](../README.md).

## Analyzer routes

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
- `GET /api/analyzer/pieces/<id>/export[?from=<m>&to=<m>]`: the piece as measure-addressed text
  (`text/plain; charset=utf-8`), written by the Analyzer (`analyzer_text_export_piece`,
  [format](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/text-export.md)) from the cached import and the stored `analysis.json`, so it shows what the Score
  and Analysis views show. `from` and `to` are measure numbers as written in the score; one alone means that end
  only. A non-integer, `from` after `to` or a range with no measure is `400` with `{"ok":false,"message":...}`;
  unknown id `404`; a source that no longer imports `422`; anything else `500`.
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

## Corpus routes

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

## Styles routes

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
When a piece's stored analysis is not current (another schema, Analyzer version or Ursatz commit; see
[Staleness keys](../storage.md#piece-store)), the piece is analyzed again first, so older pieces still count. Only analysis documents are read,
never the scores. A build runs on the request thread and one mutex serializes
the style store.

Storage: `styles/<id>/style-model.json` (written by `style_model_file_write`,
the same bytes for the same pieces) and `styles/<id>/meta.json` (`id`, `name`,
`created_at`, `updated_at`, `source`, `pieces`, `skipped`, `piece_count`),
each written to a temporary file and renamed. Ids are `sty-<16 lowercase hex>`.
A style is a snapshot: tagging, untagging or deleting a piece later changes
it only when it is rebuilt, and a Composer plan keeps naming the file, so a
rebuild changes what that plan's later steps use.

## Motive links routes

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
