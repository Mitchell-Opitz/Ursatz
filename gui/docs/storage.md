# Storage

The backend server and its file stores: pieces, materials, theme and piece versions, item references and subjects. [Docs index](README.md).

C HTTP server using [civetweb](https://github.com/civetweb/civetweb) (MIT),
fetched by CMake `FetchContent`. Civetweb is small, permissively licensed
and has a simple C API, with no GPL or commercial dual-licensing.

## Piece store

Each piece is a directory `pieces/<id>/`. Ids are `piece-<16 lowercase hex>`
from `ursatz_gui_new_id` in `service/id_service.c` (8 bytes of
`/dev/urandom`, with a hash fallback), redrawn if the directory exists, at
most 8 tries.

- `source.mid` or `source.musicxml`: the upload's bytes, written unchanged. A
  `.xml` or `.mxl` upload keeps its extension (`source.xml`, `source.mxl`).
- `analysis.json`: the Analyzer's analysis document (schema v9).
- `meta.json`: `{"filename", "format": "midi"|"musicxml", "uploaded_at"
  (ISO 8601 UTC), "analyzer_version", "document_schema_version",
  "ursatz_commit"}` plus the
  optional `"composer"` and `"source"` (strings) and `"tags"` (array of
  strings). A member that is absent reads as `""` or no tags, so pieces
  stored before the metadata existed list as unassigned.

Metadata rules (`service/piece_metadata.c`): composer and source are trimmed,
runs of whitespace become one space and control characters are dropped; `""`
means unset; at most 120 bytes. A tag gets the same treatment, is lowercased
(ASCII), must not be empty, longer than 48 bytes or contain a comma, and a
piece holds at most 32 tags in insertion order. Values and tags are compared
case-insensitively.

Every file is written to `<name>.tmp` and renamed, and a failed analysis
removes the directory. Re-running the analysis rewrites `analysis.json` and
the three version fields of `meta.json`; the filename and `uploaded_at` stay.

Staleness keys: a stored analysis is current only when its `schema_version`, `metadata.analyzer_version` and
`metadata.ursatz_commit` all equal this build's (`analyzer_document_status` on `analysis.json`; Analyzer
`staleness.h`). `meta.json` holds the same three values as `document_schema_version`, `analyzer_version` and
`ursatz_commit`, so the Library list reads the flag without opening a document. A `meta.json` from before
`ursatz_commit` gets it from its document's `metadata.ursatz_commit` the first time the piece is opened (written back
under the piece lock); with neither it is stale. A stale analysis is analyzed again once, under the piece lock, when
the piece is opened, when a style is built from it and before a link run reads it (`ursatz_gui_analysis_ensure_current`);
the link worker's re-analysis after a feature or format error stays as a fallback. If that analysis fails, the
stored document is served. Motif and theme analyses are Composer output, not analysis documents, and are not checked.
Pieces saved by earlier versions (`pieces/<id>.db`) are ignored: they are
not listed and cannot be opened, so upload those files again.

### Concurrency

One lock per piece directory (`service/piece_lock.c`, a table of per-id mutexes with a reference count; an entry
is freed when its count reaches 0) is held around every writer of `pieces/<id>/` and every read that must not see
half a write. All of them are in `service/analysis_service.c` and take the lock themselves, so no caller holds it:

- upload (`ursatz_gui_analysis_import_and_save`, from the moment the directory exists until it is complete);
- PATCH (`ursatz_gui_analysis_update_piece`: the metadata, the re-analysis a changed notation needs and, if that
  fails, the undo of the notation, as one hold);
- re-analyze (`ursatz_gui_analysis_reanalyze`), which the style build and the link worker call for a stale piece;
- delete (the directory removal; the collection and subject cleanups run after the lock is released);
- the reads of `analysis.json` plus the source: detail, `/musicxml`, `/musicxml/meta` and `/playback`.

### Import cache

`service/import_cache.c` keeps the imported notes of up to 8 pieces, so opening a piece imports its source once: the
detail's note ids, `/musicxml`, `/musicxml/meta` and `/playback` share one `ImportedNotes`. An entry belongs to one
piece id and one source version (the notation fields of `meta.json` plus the source file's size and modification
time); a different version imports again. It also holds the exported score (with the title and composer added) and
whether the meter was defaulted, so a second `/musicxml` exports nothing. The least recently used entry that no
request holds is dropped to make room. Entries are made and read under the piece lock; PATCH, re-analyze and delete
invalidate the piece's entry. Memory only: a restart starts empty.

Lock order, stated in `service/piece_lock.h`: a store lock (style, link, collection, subject) first, then one piece
lock; never the reverse, and never two piece locks. The lock is not recursive. A reader of a piece that is deleted
meanwhile gets `404` or the whole piece. Lists read only `meta.json`, which is replaced by a rename, so they take no
lock. `test_piece_lock` covers it.

## Material store

Motifs, themes and pieces live in `materials/` beside `pieces/` and
`collections/`. `service/material_service.c` is the only code that touches
it; every operation runs under one mutex, and every file is written to
`<name>.tmp` and renamed.

```
materials/
  projects/<prj-id>.json   {id, name, description, created_at, updated_at}
  items/<id>/item.json     {id, type, name, project_id, current_version,
                            versions: [{number, created_at, note}],
                            created_at, updated_at}
  items/<id>/v<n>.json     the version body
  items/<id>/v<n>/         a composed theme or piece version's files (see "Theme bodies and version files",
                           "Piece bodies and version files")
```

- Ids are `<prefix>-<16 lowercase hex>` from `ursatz_gui_new_id`: `prj` for
  projects, `mot`, `thm` and `pce` for motif, theme and piece items.
- On first start a project named `General` is created. A project name is 1 to
  200 characters after trimming, a description at most 2000; a project that
  holds items cannot be deleted.
- Versions are numbered from 1 and never deleted or rewritten. Saving a new
  body writes `v<n+1>.json` and then `item.json`, which advances
  `current_version`; rename and move change `item.json` only. Deleting an item
  removes its whole directory.
- A version body is validated by `ursatz_gui_material_body_validate` before it
  is written: an object with a `uses` array, and the member of its own type
  (`motif`, `theme` or `piece`) and no other. A motif carries `role`, `form`,
  `notes`, `meter`, `key`, `pickup`, `harmony` (null or text, at most 200
  characters), `features`, `segments_proposed`, `analysis` and `derived`; the rules are listed in
  `service/material_service.h`. `features` are the motif's segments. `segments_proposed` (a bool; a body without it
  reads as false) is true while the segments are the Composer's proposal rather than ones Michelle set: the proposal is
  stored as `features` and proposed again when the notes, pickup, meter or form change.
- Projects and items are passed as cJSON objects; the caller frees what it
  receives.

### Theme bodies and version files

A theme body (the rules are in `service/material_service.h`) is a placeholder or a composable theme (R-167). The
placeholder, `form` null or a string and `slots` an array of anything, stays valid so earlier versions still read. A
body with a `form` object, a slot written as `{slot, item_id, version}` or any of the members below is a composable theme:

- `form`: `{into, hybrid, mode, meter, bars, cadence, first_cadence}`: each member null or text (`bars` a number), in
  the words of the Composer's vocabulary (`GET /api/composer/vocabulary`: `into` `sentence`, `period` or `hybrid`, `mode`
  `major`, `minor` or `style`, `meter` `2/4`, `3/4`, `4/4` or `6/8`, `bars` 4 or 8...). A save keeps the type and size
  of a word and not the word: a value the Composer does not read is saved, `POST /api/materials/items/validate` marks
  it and compose refuses it with its path (R-276). Compose needs `into`, `mode` and `meter`. A theme is in C: a body
  with a `key` is refused (`themes are in C; remove key`).
- `slots`: `[{slot, item_id, version}]`, `slot` one of the vocabulary's `theme_slots` (today `basic`, `contrasting`,
  `continuation`, `cadential`), the one marked `required` (`basic`) present and each slot at most once; the same item may be
  in several slots. The slot names, the required one and how many a theme may have are read from the Composer vocabulary, not
  kept in the GUI (R-431). Each item is a melodic motif of form `full` at an existing
  version.
- `ideas`: `[{idea, item_id, version}]`, a motif for a single idea in place of its slot's. `idea` is `basic idea`,
  `repetition`, `contrasting idea`, `consequent basic idea`, `consequent contrasting idea`, `fragment 1`, `fragment 2`,
  `cadential`, `cadential 1` or `cadential 2`; each at most once. `chords`: `[{bar, text}]`, the chords given for a
  bar (`bar` a whole number 0 to 7, `text` up to 64 characters, each bar once; the Composer checks the syntax).
  `pool` is refused (`themes have no pool`). `accompaniment`: null, `{pattern}` (`block`,
  `arpeggiated`, `alberti`, `oom-pah` or `any`) or `{pattern: "motive", item_id, version}`, a motif of role
  `accompaniment`. `brief`: null or `{scopes, piece_ids, style_ids}` as the styles route takes it. `style_weight`
  `n/d`. `without`: up to 4 of `motive-degrees`, `motive-intervals`, `motive-rhythm`, `motive-nct-figure`. `tempo`:
  null or a whole number (the Composer's range is in its vocabulary). `check`: boolean. `seed`: null or a whole number. `result`: null or an object, written by the
  compose route.
- The server fills `uses` from the slots, the ideas and the accompaniment motif: a client use of those items is replaced
  by one use for each version named, with a fresh `copy` (other client uses stay). Moving a use to a newer version moves
  the slot, idea or accompaniment version with it.
- The request the compose route writes is `ursatz-theme-request` version 2: key `C`, one motive for each distinct motif
  version (`M1`, `M2`, ...; the motif's base harmony, its own or the analysis', and its `tone_overrides`), `slots`,
  `ideas` and `chords`.
- A new theme version needs a note (`give a version note`); the first is noted `Created` and a compose `Composed`.
- At start the backend deletes every theme once, for the v2 body (marker `themes-cleared` in `materials/`); a piece
  that used one then shows its source missing.

A composed version keeps its files beside its body in `items/<id>/v<n>/`: `request.json` (what the Composer read),
`brief.style.json` (when the theme has a brief), `plan.json`, `theme.mid` and `theme.musicxml`. The folder is written
before the version exists, so a version that exists has its files whole. Deleting the item removes the folders with its
directory.

### Piece bodies and version files

A piece body (the rules are in `service/material_service.h`) is a placeholder or a composable piece (R-174). The
placeholder, `structure` null, a string, an array or an object without a list of `sections`, stays valid so earlier
versions still read. A body whose `structure` has a `sections` list, or that has any of the members below, is a
composable piece. A structure alone is valid: every fill may be left out.

- `structure`: `{form?, key, mode, meter, ...}`: `key`, `mode` and `meter` null or text, in the Composer's words (a
  value it does not read is saved and marked by validate, R-276). `form` names a form of the Composer's forms entry
  (`GET /api/composer/forms`); absent or null is `game-loop`, so a piece saved before the forms is a game loop (R-430).
  The form says what else the structure has. A form whose `sections` is `request` (the game loop) has `sections`, 1 to
  32 of `{id, part, letter, beats, pickup, cadence, fill}`: `id` is `s<n>` or `end`, each once; `part` is one of the
  form's roles (the game loop's: `intro`, `body`, `turnaround`, `ending`), in their order, with every required role
  present (a `body`); `letter` 1 to 8 characters; `beats` a whole number from 1 to the vocabulary's
  `limits.piece_beats`; `pickup` `n` or `n/d` beats; `cadence` null or text. It has no `bars`, `count` or `modulate`.
  A form with sizes has no `sections` (absent or null; its sections are its size's), `bars` and `count` (whole
  numbers, absent is 0: the default size) that pick one of the form's sizes by bars, by count or by both, and `modulate`
  (a boolean) only where the form's settings allow it.
- `fill`: null or `{kind: "empty"}`; `{kind: "motif", item_id, version, into?, hybrid?}`, a melodic motif of form
  `full`; `{kind: "theme", item_id, version}`, a theme version that was composed (it has a `result` and a `plan.json`);
  `{kind: "progression", numerals, cells?}`, the numerals as text (the Composer reads them) and 1 to 8 cells for each chord.
- `motive`: null or `{item_id, version}`, a melodic motif of form `full`: the piece's basic motive. `pool`,
  `accompaniment` (null or `{pattern}`; the pattern `motive` is not available), `brief`, `style_weight`, `without`,
  `tempo`, `check`, `seed` and `result` are the theme's.
- The server fills `uses` from the motif and theme fills and the motive: a client use of those items is replaced by one
  use for each version named, with a fresh `copy`. Moving a use to a newer version moves the fills and the motive
  with it (a theme can only move to a version that was composed).

A composed piece version keeps its files beside its body in `items/<id>/v<n>/`: `request.json`, `brief.style.json`
(when the piece has a brief), for each version of its form named `<name>` in `result.summary.versions` (the form's
`versions`: `loop` and `ending` for the game loop, `piece` for a form with sizes) its `<name>.mid` and `<name>.musicxml`
and its plan (`plan.json` for the first version, `<name>-plan.json` for each later one), and
`theme-<section id>.plan.json`, a copy of the plan of each theme version a section uses, so the version keeps its own.
A game loop keeps `plan.json`, `ending-plan.json`, `loop.mid`, `loop.musicxml`, `ending.mid` and `ending.musicxml`, as
it always did; a rounded binary keeps `plan.json`, `piece.mid` and `piece.musicxml` and no loop or ending file. The
folder is written before the version exists. A regenerated or chosen version keeps the same files (its `request.json`
names the copies of the theme plans and the style file beside it). A variations set is a folder `variations-<k>/` in
the version it was made from, numbered over the whole item: `variation-<i>.json` (the plan of the first version),
`variation-<i>.mid`, `variation-<i>.musicxml` and the Composer's `result.json`. It is not a version; deleting the item
removes it.

### Item references

An item can use other items, from any project. Each entry of a body's `uses` is
`{item_id, version, copy}`.

- On create and on every new version the service checks that the item and
  version exist (an unknown one, or an item using itself, is `INVALID_BODY`)
  and fills `copy` with that version's body. A `copy` the client sends is
  replaced.
- `ursatz_gui_material_uses_status` reports each use of the current version
  with the source's name, type, project and `current_version`. `updated` is
  true when the source is newer than the used version.
- `ursatz_gui_material_used_by` lists the items whose current version uses an
  item.
- `ursatz_gui_material_update_use` writes a new version that points the use at
  the source's current version with a fresh copy (note
  `updated <name> to v<n>`). Older versions keep their old copy.
- `ursatz_gui_material_item_delete(id, force)` returns `ERR_REFERENCED` while
  other items use the item. A forced delete leaves the users' copies as they
  are; their uses report `missing: true`. Saving a new version of such a user
  needs the missing use removed first.

## Subjects

A subject is a character, place, idea or note that items and Library pieces
link to. They live in `subjects/`, one file per subject, beside `materials/`.
`service/subject_service.c` is the only code that touches them; every
operation runs under one mutex, and every file is written to `<name>.tmp` and
renamed.

```
subjects/
  <sub-id>.json   {id, name, kind, note, items: [item ids],
                   pieces: [library piece ids], related: [subject ids],
                   created_at, updated_at}
```

- Ids are `sub-<16 lowercase hex>`. `kind` is `character`, `place`, `idea` or
  `note`. A name is 1 to 200 characters after trimming, a note at most 4000.
- `ursatz_gui_subject_link_item` and `ursatz_gui_subject_link_piece` check
  that the item is in the material store and the piece is in `pieces/`.
  Linking something already linked is not an error: `*out_already` is set and
  nothing changes. Unlinking something that is not linked is `ERR_NOT_LINKED`.
- `ursatz_gui_subject_relate` stores the relation on both subjects. A subject
  cannot relate to itself (`ERR_SELF`); relating twice reports `already`.
- Deleting a subject removes it from every `related` list. It does not touch
  the items or pieces it linked.
- Deleting a Library piece (`DELETE /api/analyzer/pieces/<id>`) calls
  `ursatz_gui_subject_forget_piece`, and `ursatz_gui_material_item_delete`
  calls `ursatz_gui_subject_forget_item`, so a subject never keeps a link to
  something that is gone. A failure there is logged, not answered.
- `ursatz_gui_subject_list` returns the subjects in name order with
  `item_count`, `piece_count` and `related_count`. `ursatz_gui_subject_groups`
  is the panel's view: one group per kind, each with its subjects and their
  `item_count`, `piece_count` and `link_count` (items plus pieces).
- `server.c` starts both the material store (`materials/`) and the subject
  store (`subjects/`) with the collections.
