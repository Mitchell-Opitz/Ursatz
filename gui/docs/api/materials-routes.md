# Materials and subjects routes

The routes for materials (motifs, themes, pieces) and subjects. [Docs index](../README.md).

## Materials and subjects routes

Both stores are described under [Material store](../storage.md#material-store) and [Subjects](../storage.md#subjects); these
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
