# Ursatz GUI

A local web UI for the Ursatz platform. You upload a MIDI or MusicXML file,
the Analyzer library analyzes it, and the GUI shows the analysis document
section by section next to the score. Pieces can be grouped into
Collections. This repo is a host/shell around independent tool modules, not
a monolithic application.

## Views

- **Library**: upload one or several files (`.mid`, `.midi`, `.musicxml`,
  `.xml`, `.mxl`) with the picker or by dropping them on the upload box.
  Files are uploaded and analyzed one after another; a list under the box
  shows each file as queued, analyzing, uploaded or failed, with a summary
  line, and the table fills in as each one finishes. Dropped files with
  another extension are listed as failed. The table lists the stored pieces
  with a select box, Title, Composer, Source, Tags (chips) and Actions; an
  empty composer or source shows a dash. Clicking a title opens the Analysis
  view. Delete a piece, or run its analysis again (the round arrow).
  - Edit (the pencil) opens an editor row for the title, composer, source and
    tags. Composer and source suggest existing values; type a tag and press
    Enter or comma to make a chip, Backspace on an empty box removes the last
    chip, and × removes one. Save sends one request; Escape or Cancel closes
    the editor.
  - The filter row has a search box (title, composer, source, tags), Composer,
    Source and Tag selects (Any, Unassigned or Untagged, then each value with
    its piece count), a sort (Date added, Title, Composer) and Clear filters.
    The selects filter on the backend; search and sort apply to the result. A
    count line ("12 of 40 pieces") shows while a filter is on.
  - Bulk edit: check rows (the header box checks every shown row) and the bulk
    bar offers Set composer, Set source, Add tag and Remove tag. Apply updates
    the selected pieces one at a time and stops at the first failure, naming
    the piece.
- **Tags panel** (sidebar, under the tools): pieces grouped automatically by
  Composer, Source and Tags. Each group lists its values with piece counts
  (the top 12, then "Show all (n)"), then Unassigned (composer, source) or
  Untagged (tags) when any pieces are in it; an empty group reads "None yet".
  Clicking a row opens the Library filtered to that group, and the row stays
  highlighted while that filter is on. Each group folds open or closed and
  the choice is remembered in the browser. The panel refreshes after any
  upload, edit, bulk edit or delete; if the groups cannot be fetched it shows
  "Tags unavailable" and the tool list keeps working. The sidebar scrolls
  when the panel is long.
- **Analysis**: one piece. The top-level Analysis nav entry reopens the piece
  viewed last this session; before any piece has been opened it points at
  the Library.
- **Collections**: named groups of piece ids (the Corpus routes, below).
- **Composer**: start a theme or a piece and work on it (Composer view,
  below).
- **Health**: last entry in the sidebar. Shows the backend health check; a
  failed check appears only here.

The GUI opens on the Library.

### Composer view

The Composer entry drives the Composer routes (see Composer routes below).
The page has two forms and the list of compositions; a row opens the
composition. Every action waits for its answer with its buttons disabled and a
spinner (a run can take tens of seconds); nothing is polled. A failed run shows
the backend's message and the Composer's stderr.

- **Grow a theme**: fill New theme (name, the motive as a note list such as
  `D4:q F4:e A4:e D5:h`, into sentence, period or hybrid, key, meter, bars 4 to
  8, optional cadence, accompaniment, check, seed and tempo) and press Grow
  theme. The composition starts at version 1.
- **Plan a piece**: fill New piece (name, form, key, meter, bars, count,
  modulate, optional motive, optional given melody with where it goes, for
  example `B` or `B:2`, accompaniment, check, seed, tempo) and press Plan
  piece. The piece is planned and filled; the result is version 1.
- **Compose in a style**: both forms have a Style select (None, then every
  style built in the Collections view, with its piece count) and a Weight
  select (Light 1/4, Normal 1/2, Strong 1; Normal by default, disabled while
  Style is None). The result text includes the Composer's style distance line.
  A composition keeps the path of its style file, so a later fill or
  regenerate after the style was deleted fails with the Composer's message.
- **A composition**: the version selector lists each version with its action,
  seed and time. Below it: the plan tree (form, sections with their letters,
  phrases and ideas, each with its bars and its state, Given or Realized), the
  summary table (the form heard and the cadences per section), the score of the
  version drawn with OpenSheetMusicDisplay, and a MIDI download.
- **Lock and regenerate**: every section and node of the tree has Lock and
  Regenerate. The Lane and Seed fields above the tree apply to the button
  pressed (all lanes and a new random seed when left blank). Each action
  writes the next version and opens it; the earlier versions stay.
- **Variations**: pick a target (a section or node), a lane, a count from 1 to
  8, an optional seed, and press Generate. The results show side by side as
  small scores with their seeds and the form heard; Choose makes one the next
  version. Harmonize (harmonizations of the whole section, no lane) is enabled
  only when the section has a form letter and every melody span of the
  section is Given.

`composer.js` (loaded after `app.js`) holds the request builders and the plan
tree as plain functions, tested by `frontend/tests/composer-view.test.js`
against `frontend/tests/fixtures/composer-plan.json`, a plan the Composer CLI
wrote. The score renderer (`drawMusicXmlScore`) and the fetch helpers
(`fetchJsonResult`, `postJsonResult`) live in `app.js` and are shared with the
Analysis view.

### Analysis view

Opening a piece shows the score workspace (`frontend/public/score-workspace.js`,
global `ScoreWorkspace`). `ScoreWorkspace.mount(rootEl, options)` takes a
title, a subtitle, the score and meta URLs, the analysis document, the note
`xml:id` list, the toolbar actions and a back callback, and knows nothing
else about the Analyzer, so the Composer can mount it too. It returns
`{destroy, osmd, layout, xmlText, rerender, on, mode, setMode, setStatus,
dataEl}` and emits `rendered` (with the extracted layout, after every OSMD
render) and `modechange`.

The toolbar (48 px) holds, from the left, a "Library" link, the filename and a
dim line with the format, measure count and note count; a transport slot in
the middle (empty until playback); and on the right a Score | Data switch, zoom
out, the zoom (click to reset to 100%), zoom in, the Layers and Inspector
toggles and "Re-run analysis", which runs the Analyzer again on the stored
source and reloads the view. Below it are three columns: the Layers rail
(248 px), the score and the Inspector (360 px). A closed panel takes no width.

A Notation row (R-110) sits under the toolbar and at the top of the Data
view's "Rhythm and meter" tab. It shows the reading the analysis used, the
meter check's line from the `meter-fit` claim ("Declared 4/4 fits 41%. 3/4 with
values doubled fits 93%."), Apply when the check found a better reading, and
Edit, which opens meter, value scale (x1/4 to x4) and pickup fields with Save,
Clear and Cancel. Apply and Save send `PATCH /api/analyzer/pieces/<id>` with
`notation` (blank fields left out, Clear sends `null`), then load the analysis
and score again. A rejected notation shows the backend's message
(`invalid notation`) in the row. The model is `frontend/public/notation-panel.js`
(global `NotationPanel`: `meterFitSummary`, `notationBody`, `readingText`),
tested by `frontend/tests/notation-panel.test.js`. Nothing is applied
automatically.

| Window width | Panels |
| --- | --- |
| 1280 px and wider | Rail and Inspector beside the score |
| 1024 to 1279 px | Rail beside the score, Inspector floats over its right side |
| Below 1024 px | Both float |

A floating panel starts closed, sits above the score with a shadow and
closes on Escape or a click on the score. Only the score canvas, the rail and
the Inspector scroll. The score is drawn on white paper with OSMD (no title or
composer, part names on, no automatic resize). A `ResizeObserver` redraws it
150 ms after the width stops changing, and every redraw keeps the measure at
the top of the viewport at the top. The meter caption stays above the paper.

Zoom runs 50%, 62.5%, 75%, 87.5%, 100%, 125%, 150%, 175%, 200%. Keys (ignored
while typing in a field, or with Ctrl, Meta or Alt held; the `?` dialog lists
them):

| Key | Action |
| --- | --- |
| `Space` | Play or pause |
| `Home` | Stop and go back to the start |
| `C` | Follow the cursor: turn Follow on and scroll to it |
| `Enter` | Play the selected claim (not while a button or the List has focus) |
| `L` | Layers panel |
| `I` | Inspector panel |
| `F` | Focus mode: both panels closed, then restored |
| `+` or `=` | Zoom in |
| `-` | Zoom out |
| `0` | Zoom to 100% |
| `[` and `]` | Select the previous or next item of the selected layer |
| `?` | Shortcut list |
| `Escape` | Close the shortcut list, else a floating panel, else clear the selection |

The zoom, the docked panels and the mode are remembered in `localStorage` under
`ursatz.workspace.v1` (`{layersOpen, inspectorOpen, zoom, mode}`). The pure
part (zoom steps, panel modes, shortcut table, saved state) is
`frontend/public/workspace-state.js` (global `WorkspaceState`, no DOM).

Data mode replaces the score with the Provenance list and the claim tabs
below. Provenance lists the document's metadata (source file, analyzer
version, Ursatz commit, document schema, input format, source type, meter,
key signature, spelling, importer warnings).

There is one tab per group of document sections, in this order:

| Tab | Sections |
| --- | --- |
| Keys | `keys`, `tonicizations`, `modulations`, `pivot_chords` |
| Chords | `chords`, `chord_vocabulary`, `non_chord_tones`, `prolongations` |
| Cadences | `cadences` |
| Progressions | `progressions`, `harmonic_rhythm` |
| Phrases | `phrases` |
| Themes and form | `themes`, `form` |
| Motives | `motives` |
| Melody | `melody`, `outer_voices` |
| Rhythm and meter | `meter`, `rhythm` |
| Texture | `texture`, `voice_leading` |
| Schemas | `schemas` |
| Markings | `markings` |
| Counterpoint | `counterpoint_check` |
| Notes | `notes` |

A tab shows the number of rows it holds. A tab whose sections are all empty
arrays or null is disabled and reads "(none)".

Each item of an array section is one row: its name (the item's `label`, else
its `numeral`, `type` or `key`), where it is (`m<measure> b<beat>` for the
start and the end, `-` when the location is null), and a Fields toggle that
shows the item's remaining fields as formatted JSON. An object section
(`keys`, `form`, `meter`, `harmonic_rhythm`, `outer_voices`, `markings`,
`counterpoint_check`) is shown as one JSON block, and the arrays of claims
inside it (the key regions, the form's sections and returns, the soprano and
bass lines) are rows. Choosing a row switches to Score mode and selects that
claim there (see [Selecting claims](#selecting-claims-and-the-inspector)).

The tab logic is in `frontend/public/analysis-tabs.js` (no DOM, loaded by
`index.html` before `app.js` and by the frontend tests); `app.js` renders it.

### Score

Score mode fetches `GET /api/analyzer/pieces/<id>/musicxml` and renders it with
[OpenSheetMusicDisplay](https://opensheetmusicdisplay.org/), vendored as a
static UMD build under `frontend/public/vendor/opensheetmusicdisplay/` and
loaded by a plain `<script>` tag (`frontend/` has no `package.json` or
bundler). A caption below the score ("This file declares no time signature;
shown in 4/4") shows when `GET .../musicxml/meta` reports
`meter_defaulted: true`, that is, when the source file gave no meter. When the
response is not a 2xx XML document, `ScoreNotes.scoreResponseProblem` turns it
into a message and the paper shows "Could not load score: <message>" instead of
handing the body to OSMD.

The score is written in notated values: the export quantizes the played times
to the beat grid (`score_quantize_options_default()`), so notes carry `<type>`,
dots, triplet brackets and `<beam>`. Each source part is a part with its
instrument and General MIDI program (`midi_program`), a piano part draws on two
staves, `<work-title>` is the piece title (the filename without its extension)
and the composer from `meta.json` is written as the composer credit.

A claim's `note_ids` are mapped to `<note xml:id>` values through the piece's
`note_xml_ids`. OSMD does not keep `xml:id`, so the score finds each
note by its position and pitch (`frontend/public/score-notes.js`, no DOM,
loaded by `index.html` before `app.js` and by the frontend tests):

- The key is `<part index>|<measure number>|<staff>|<voice>|<onset>|<pitch>`.
  Onset is the position in the measure as a reduced fraction of a whole note
  (`1/4`); pitch is step, alter and octave (`E-1/5`). Rests have no key.
- The key is built from the MusicXML text (`<divisions>`, `<duration>`,
  `<backup>`, `<forward>`, `<chord/>`) and from each OSMD `GraphicalNote`
  (`ParentStaff` for part and staff, measure number, voice id, `Timestamp`,
  `Pitch`). Equal keys are the same note, so multi-part, multi-voice scores
  highlight exactly the claim's notes.
- OSMD 3.10.1's voice ids equal the MusicXML `<voice>` numbers, so the voice
  is part of the key and there is no voice-less fallback.
- A claim's note id also matches its barline fragments (`<id>_a`, `<id>_2`).
- Rendered notes whose key matches no `xml:id` are counted; the count is
  logged once per score with `console.warn`.

#### Layers on the score

The Layers rail (left of the score) lists the layers of
[Score layers](#score-layers) in groups, each with a switch, its color, its
name and the number of items in the piece. A layer with no items is disabled
("None found in this piece"). The chips at the top apply a preset (None,
Harmony, Structure). Everything is off by default; the choice is remembered
and toggling never reloads the piece.

`frontend/public/score-overlay.js` (global `ScoreOverlay`) draws the layers
that are on in one SVG laid over the OSMD SVG, redrawn from scratch after
every render and toggle. Positions come from `ScoreGeometry` (a piece position
plus the timeline offset is an OSMD time).

- Ribbons (keys, key changes, prolongations, progressions, form, themes,
  phrases, schemas, texture, Other layers) sit in lanes above each system, one
  18 px row each, up to four rows per layer. A ribbon is a tinted bar with a
  solid edge at its start and its label inside, shortened to fit; a span that
  crosses systems continues on the next one with open ends. Tonicizations are
  dashed, subphrases thin. An item that fits none of the four rows becomes a
  "+" in the lane's last row whose tooltip counts the hidden items.
- Cadences are pills at the arrival (PAC, HC, ...). A cadence with a competing
  reading shows a white dot and the tooltip "Also read as <abbr> (<share>%)".
- Chord numerals sit below each system. A numeral that would come within 4 px
  of the previous drawn one becomes a tick (`chordLabelsToDraw`), so zooming
  in shows more.
- Notes layers (non-chord tones, motives, melody) color noteheads
  (`.vf-notehead` paths) without a re-render; the first layer in the table
  owns a shared note, and turning a layer off restores the original colors.
- Ribbons and marks have `role="button"` and an accessible name
  "<layer> <text>, <location>"; color is never the only cue.

Lanes need room, so after a toggle the workspace sets OSMD's
`MinimumDistanceBetweenSystems` and `PageTopMargin` from
`ScoreGeometry.systemSpacingUnits` and renders again only when those values
change (the scroll position is kept). The timeline offset is calibrated once
per piece from the first 50 notes found in the score, paired with their
positions from `GET /api/analyzer/pieces/<id>/playback`; a console warning
reports notes that do not fit it.

#### Selecting claims and the Inspector

One item is selected at a time. Click a ribbon, a cadence pill or a numeral,
or a notehead colored by a notes layer (of the layers that are on, the item
with the shortest span that holds the note is chosen; the others are listed
as "Also here" links). A claim row in Data mode and a row of the Inspector's
List also select: they switch to Score mode, turn the claim's layer on when it
is off (a polite status line says "Phrases layer turned on") and scroll so
the claim's first system sits at 25% of the canvas height. Selecting from
the score opens the Inspector when it is closed.

On the score the selected ribbon or pill gets a 2 px dark outline (a ribbon
turns solid). Cadence, chord and notes layers put an amber halo around each
notehead of the claim; ribbon layers tint the span over the full system
height instead, since a key region can hold thousands of notes. Escape clears
the selection, and `[` and `]` select the previous and next item of the same
layer.

The Inspector has two tabs.

- Claim: the layer, the title, the location and the measure count; Previous,
  Next and Copy JSON; Readings for a cadence (each reading with a share bar,
  and for a competing reading a button that shows its arrival notes with
  dashed halos); Evidence, one row per remaining field in claim order, where a
  sub-span such as `arrival` has a Show button that tints it, and a list of
  sub-claims (subphrases, occurrences) expands into rows that select their
  item when a layer shows it and tint their span otherwise; Scores
  (`uncertainty`, `score`, `confidence.*`, as the Analyzer wrote them); and
  the raw JSON under "Raw fields". The footer names the framework and the item
  key.
- List: a search field (matches the text and label), then the claims of the
  layers that are on, sorted by start. With no layer on it shows "Turn on
  layers to list their claims" and an "All layers" switch that lists every
  layer. The first 500 rows are drawn, then "Show more". Up and Down move the
  highlight, Enter selects.

The pure part (picking, neighbors, filtering, formatting, the evidence rows)
is `frontend/public/inspector-model.js` (global `InspectorModel`, no DOM); the
panel is `inspector-view.js`; the selection and the overlay drawing live in
`score-workspace.js` and `score-overlay.js`.

### Playback

The score workspace plays the piece from the toolbar. Space plays or pauses,
Home stops and goes back to the start. The first Play creates the audio
context (browsers block audio before a click) and loads the piano; the button
reads "Loading piano..." until the samples are decoded.

- Transport: Play/Pause, Stop and a readout "m<measure> <m:ss> / <m:ss>". The
  times follow the current tempo.
- Tempo: a percentage with minus and plus buttons in 5% steps from 50% to
  150%; clicking the percentage resets it to 100%. It applies while playing
  and resets for each piece. The tempo itself comes from the playback route's
  `tempo` regions (a MIDI file's set-tempo events, a score's tempo and
  metronome marks). With `tempo_source` "default" the file gave none, the
  piece plays at 120 and the Sound menu says so.
- Volume: a slider, 0 to 100, default 80. Saved in `ursatz.workspace.v1`.
- Sound menu: one row per part with a select of the registered sounds, a
  Reverb slider (0 to 100, default 20, saved with the volume), and the credit
  lines of the sounds in use. Reverb is a generated convolution tail
  (`reverb.js`, 2.2 s), no impulse file is shipped; its dry and wet gains are
  an equal-power crossfade that keeps the dry signal audible, and both move
  with a short time constant so the slider makes no clicks.
- Cursor: a 2 px line over the playing system's staves with a soft teal band
  behind it, drawn in the score overlay. It glides through each measure
  (redrawn every animation frame while playing); with reduced motion it steps
  from note onset to note onset. It stays at the pause or stop position and is
  hidden only before the first play or click.
- Follow: while playing, when the cursor's system leaves the band between 10%
  and 75% of the score's height, the score scrolls so that system's top sits
  at 20% (smooth unless reduced motion). The wheel, touch, a drag on the
  scrollbar or a scroll key (arrows, Page Up and Down, End) turns Follow off
  and shows a "Follow" pill at the bottom right. The pill or `C` turns it on
  and scrolls to the cursor; each new Play turns it on again. The rule is the
  pure `WorkspaceState.followScrollTarget(viewport, systemRect)`.
- Clicking to seek: a click on bare paper (not on a ribbon, mark or colored
  note) finds the system under the click, converts the x position to a
  position in the piece (`tAt` minus the timeline offset) and snaps it to the
  nearest note onset in that system (`PlaybackEngine.nearestOnset`). Playback
  continues from there if it was playing; stopped, the next Play starts there.
  A click on a colored note seeks to that note's onset and selects its claim.
- Playing a claim: the Play button in the Inspector's actions row, `Enter`
  with a claim selected, or a double-click on a ribbon or mark plays the
  claim's span (a cadence plays from the dominant to the end of the arrival)
  and, at the end, the cursor returns to the claim's start. A span with no
  length plays a quarter note.
- Output: every note is a voice into one bus (at most 64 voices, the oldest
  stops first), then the dry path and the reverb send, a master gain and a
  compressor. Velocity is fixed at 0.7 (the playback data has none).

Adding a sound is one `register` and one `mapProgram`, with no change to the
engine or the transport. A source is `{id, label, credit, load(ctx, {pitches}),
start(ctx, destination, {pitch, velocity, when, duration}) -> {stop(when)}}`:

```js
const registry = SoundSources.createDefaultRegistry(Instruments);
registry.register(mySource);
registry.mapProgram(73, "my-source"); // MIDI program 73 plays it
registry.setPartSource(2, "my-source"); // or override one part
```

The registry is `frontend/public/instruments.js` (global `Instruments`), the
built-in sounds are `sound-sources.js` (`gm-piano`, the default, and
`basic-synth`, the fallback that needs no files), the timing is
`playback-engine.js` (a timeline with tempo regions and a transport that
schedules notes 120 ms ahead of an injected clock) and the reverb helpers are
`reverb.js`. If the piano cannot load, playback uses the basic synth and the
status line says "Piano samples could not load; using basic synth".

The piano is the FluidR3 GM acoustic grand piano, vendored in
`frontend/public/vendor/soundfonts/fluidr3-gm/` with its source, checksum and
license in `vendor/soundfonts/README.md`. FluidR3_GM by Frank Wen, rendered by
Benjamin Gleitzman (midi-js-soundfonts), licensed Creative Commons
Attribution 3.0 (https://creativecommons.org/licenses/by/3.0/us/).

## Architecture

Three layers, in order of how rarely they should change:

- **Shared service layer** (`backend/src/service/`): the only code allowed to
  call into Ursatz's persistence and query functions or the Analyzer
  library. Every tool goes through this layer; nothing calls Ursatz directly
  from route code. This boundary is the one thing the scaffold is strict
  about, because it is expensive to untangle later.
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
`analysis_document_build`, and the GUI keeps no analysis code of its own.
Analysis therefore changes only when the Analyzer submodule is bumped.

## Backend

C HTTP server using [civetweb](https://github.com/civetweb/civetweb) (MIT),
fetched by CMake `FetchContent`. Civetweb is small, permissively licensed
and has a simple C API, with no GPL or commercial dual-licensing.

### Piece store

Each piece is a directory `pieces/<id>/`. Ids are `piece-<16 lowercase hex>`
from `ursatz_gui_new_id` in `service/id_service.c` (8 bytes of
`/dev/urandom`, with a hash fallback), redrawn if the directory exists, at
most 8 tries.

- `source.mid` or `source.musicxml`: the upload's bytes, written unchanged. A
  `.xml` or `.mxl` upload keeps its extension (`source.xml`, `source.mxl`).
- `analysis.json`: the Analyzer's analysis document (schema v9).
- `meta.json`: `{"filename", "format": "midi"|"musicxml", "uploaded_at"
  (ISO 8601 UTC), "analyzer_version", "document_schema_version"}` plus the
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
the two version fields of `meta.json`; the filename and `uploaded_at` stay.
Pieces saved by earlier versions (`pieces/<id>.db`) are ignored: they are
not listed and cannot be opened, so upload those files again.

### Material store

Motifs, themes and pieces live in `materials/` beside `pieces/`, `collections/`
and `compositions/`. `service/material_service.c` is the only code that touches
it; every operation runs under one mutex, and every file is written to
`<name>.tmp` and renamed.

```
materials/
  projects/<prj-id>.json   {id, name, description, created_at, updated_at}
  items/<id>/item.json     {id, type, name, project_id, current_version,
                            versions: [{number, created_at, note}],
                            created_at, updated_at}
  items/<id>/v<n>.json     the version body
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
  `notes`, `meter`, `key`, `pickup`, `features`, `analysis` and `derived`; the
  rules are listed in `service/material_service.h`.
- Projects and items are passed as cJSON objects; the caller frees what it
  receives.

#### Item references

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

### Subjects

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

### Analyzer routes

- `POST /api/analyzer/pieces`: the file as the raw request body, its name
  URL-encoded in the `X-Filename` header. The format is detected from the
  content (`input_format_detect`), not from the filename. Returns
  `{"ok":true,"id":...,"filename":...}`. A body that is neither MIDI nor
  MusicXML gives `415` with `{"error": "unsupported format"}`; a file the
  Analyzer cannot import or analyze gives `422` with
  `{"error": "import failed"}`; nothing is stored in either case.
- `GET /api/analyzer/pieces`: the stored pieces, newest upload first, then by
  id: `{"ok":true,"pieces":[{"id","filename","format","uploaded_at","composer","source","tags":[...]}]}`.
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
  `{"ok":true,"id","filename","format","uploaded_at","notation","note_xml_ids":[...],"document":{...}}`.
  `notation` is the piece's notation override or `null`. `document` is the analysis document as stored. `note_xml_ids[i]` is the
  `<note xml:id>` the score export writes for note `i` of the source; a
  claim's `note_ids` are indices into this array. An unknown id returns
  `{"ok":false,"message":"piece not found"}`.
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
  (`application/xml`), written by Ursatz's `musicxml_export_write_score` from
  the stored source: its parts and staves, its real meter and its key
  signatures. Tempo markings and dynamics are left out. Errors are JSON with
  an error status: unknown id `404`; a stored source that no longer imports
  `422` with `{"ok":false,"message":"could not import the stored source"}`;
  a failed export `500` with
  `{"ok":false,"message":"failed to export piece","code":"<MusicXmlExportResult name>"}`;
  anything else `500` with `{"ok":false,"message":"failed to export piece"}`.
- `GET /api/analyzer/pieces/<id>/musicxml/meta`:
  `{"ok":true,"meter_defaulted":<bool>}`, true when the source gave no meter
  and the barlines come from a default 4/4 (the export then carries
  `ursatz:meter-defaulted`). Same errors as the export route.
- `GET /api/analyzer/pieces/<id>/playback`: what a browser needs to play the
  piece and follow it, from the same Analyzer import as the score export (so
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
  - `"piece_ids": ["<id>",...]` (at most 5000; repeated ids count once).

  Answers `{"ok":true,"style":{list entry},"skipped":[{"id","reason"}]}`.
  `422` `{"ok":false,"message":"no analyzable pieces"}` when no piece could be
  added; nothing is written then. `name` is normalized like piece metadata
  values (at most 120 bytes, else `400`). When it is empty the default is
  `Composer: X`, `Source: X`, `Tag: x` joined with ` · ` for a filter,
  `Collection: <name>` for a collection and `Selected pieces` for ids.
- `GET /api/styles`: `{"ok":true,"styles":[list entry,...]}`, newest update
  first. A list entry is `{"id","name","piece_count","updated_at","source"}`,
  where `source` is `{"kind":"filter"|"collection"|"pieces","filter":{"composer",
  "source","tag"}|null,"collection_id":"..."|null,"piece_ids":[...]|null}`.
- `GET /api/styles/<id>`: `{"ok":true,"style":{...}}`, the list entry plus
  `created_at`, `pieces` (the ids used), `skipped` and `statistics`:
  `{"<dimension>":{"<category>":{"labels":[{"label","count","total"}],"more":n}}}`
  with dimension `harmony`, `rhythm`, `melody` or `form`, the labels of a
  category by count descending (at most 12) and `more` the number cut.
- `POST /api/styles/<id>/rebuild`: builds the style again from its stored
  source over today's pieces (same id and name, files replaced in place,
  `updated_at` set) and answers like `POST /api/styles`. `422` when no piece
  is left or the collection is gone; the style stays as it was.
- `DELETE /api/styles/<id>`: `{"ok":true}`.

An unknown style id is `404` `{"ok":false,"message":"style not found"}` on
every route.

A piece that cannot be added is skipped with a reason and the build goes on:
`not found` (no such piece), `re-analysis failed` or `unreadable analysis`.
When a piece's `meta.json` `document_schema_version` is not the Analyzer's
current one, the piece is analyzed again first (the same call as the
re-run route), so older pieces still count. Only analysis documents are read,
never the scores. A build runs on the request thread and one mutex serializes
the style store.

Storage: `styles/<id>/style-model.json` (written by `style_model_file_write`,
the same bytes for the same pieces) and `styles/<id>/meta.json` (`id`, `name`,
`created_at`, `updated_at`, `source`, `pieces`, `skipped`, `piece_count`),
each written to a temporary file and renamed. Ids are `sty-<16 lowercase hex>`.
A style is a snapshot: tagging, untagging or deleting a piece later changes
it only when it is rebuilt, and a Composer plan keeps naming the file, so a
rebuild changes what that plan's later steps use.

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

The Composer tool grows themes, plans and fills pieces, and makes variations.
It does not link the Composer library: `composer_service.c` is the only code
that starts the Composer, and it runs the `ursatz-composer` command line with
`posix_spawn` and an argument array (never a shell), capturing stdout and
stderr. The Composer brings its own copies of Ursatz and the Analyzer, so
linking it would put two versions of each into one build; as a command it is
built in its own tree (`external/ursatz-composer`, an `ExternalProject` named
`ursatz_composer_cli`) and its targets never meet the GUI's (R-086). Every
path in the arguments is absolute and the working directory is never changed.
A run is stopped with `SIGKILL` after 120 seconds.

- `URSATZ_GUI_COMPOSER`: the Composer executable to run instead of the one
  built beside the backend (`build/composer/cli/ursatz-composer`).
- `URSATZ_GUI_COMPOSER_TIMEOUT`: the time limit in seconds (default 120).

Every request field is checked against a fixed pattern or range before it
reaches an argument list: keys (`D`, `F#`, `Bb`, `D minor`), meters
(`2/4`, `3/4`, `4/4`, `6/8`), numbers in range, form letters
(`^[A-Z]'*$`), lanes, node ids, tempos (20 to 400, whole or `n/d`) and
note-list text. Anything else is 400 and starts nothing.

Compositions are stored in `compositions/<id>/` (`comp-<16 hex>`):

- `meta.json`: `{"name", "created_at", "kind": "grow"|"plan", "request",
  "versions": [{"n", "action", "args", "created_at", "seed", "summary"}]}`.
  `request` is the body that made the composition; `summary` is the command's
  JSON output (for `plan`, `{"plan": ..., "fill": ...}`, with the `fill`
  command's arguments in `fill_args`). `args` paths are relative to the
  composition.
- `v<n>/plan.json`, `v<n>/piece.mid`, `v<n>/piece.musicxml`: one directory per
  version. Every step is a new version; earlier versions are never changed.
- `motive.txt`, `given-melody.txt`: the note lists the request gave.
- `v<n>/variations-<k>/`: a set of variations (`variation-<i>.json`, `.mid`,
  `.musicxml`, and `result.json`, the command's output).

A command that fails adds no version and leaves nothing behind. A new version
or composition is written with the tempo the composition was made with.

Routes (bodies are JSON; success bodies have `"ok":true`):

- `GET /api/composer/status`: `{"ok":true,"available":true,"version":"..."}`,
  or 503 when the binary is missing.
- `POST /api/composer/grow`: `{name, motive, into, hybrid?, key, meter,
  bars?, cadence?, first_cadence?, accompaniment?, texture?, check?, seed?,
  tempo?, style?, style_weight?}`. `motive` is a note list (`D4:q F4:e A4:e
  D5:h`); `hybrid` is required when `into` is `hybrid`. `style` is a style id
  (see Styles routes), passed as `--style <model file>`; `style_weight` is a
  whole number or `n/d` with a non-zero denominator (`--style-weight`) and
  needs `style`. An unknown style is 400 `{"error":"unknown style"}`. Makes
  version 1. Returns
  `{"ok":true,"id","version":1,"summary"}`.
- `POST /api/composer/plan`: `{name, form, key, meter, bars?, count?,
  modulate?, motive?, given_melody? {notes, at}, accompaniment?, texture?,
  seed?, check?, tempo?, style?, style_weight?}`. Runs `plan`, then `fill`
  with the same seed, into version 1. `style` and `style_weight` are as for
  grow and go to `plan`; `fill` reads the plan's style constraint.
- `GET /api/composer/compositions`: `{"ok":true,"compositions":[{id, name,
  kind, created_at, versions}]}`, newest first.
- `GET /api/composer/compositions/<id>`: `meta.json` with `ok` and `id`.
- `PATCH /api/composer/compositions/<id>`: `{name}`. `DELETE` removes the
  composition.
- `GET /api/composer/compositions/<id>/versions/<n>/plan | musicxml | midi`:
  the file (`application/json`, `application/xml`, `audio/midi` as an
  attachment).
- `POST .../<id>/lock`: `{version, node | section, lane?}`. Writes the next
  version; only the plan changes, so it copies the piece files of `version`.
- `POST .../<id>/regenerate`: `{version, node | section, lane?, seed?}`.
  Writes the next version with its piece files. A grow plan has no form
  letters; address its nodes by `node` (the `composer.id` in `plan.json`).
- `POST .../<id>/variations`: `{version, node | section, lane?, count (1 to
  16), seed?, harmonize?}`. Writes `v<version>/variations-<k>/` and returns
  `{"ok":true,"id","version","set":<k>,"variations":[{index, seed,
  sections}],"kept","requested","seeds_tried"}`. `harmonize` runs
  `harmonize` and takes a `section` only, with a section whose melody is all
  Given. `k` is counted over the whole composition.
- `POST .../<id>/variations/<k>/<i>/choose`: makes variation `i` of set `k`
  the next version.
- `GET .../<id>/variations/<k>/<i>/plan | musicxml | midi`: the files of a
  variation, for previews.

Errors are `{"error": "..."}`: 400 for a bad field or a Composer usage error
(with `stderr`), 404 for an unknown composition, version or variation, 422
`{"error":"stuck","stderr"}` when the Composer cannot do it (exit status 2),
503 when the binary is missing, 504 on a timeout, 500 otherwise. The Composer
needs POSIX (`posix_spawn`), so this tool does not build on Windows.

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
pkg-config. Ursatz is a git submodule at `external/ursatz`, pinned at PR #147
(`e74cafc`), which writes chords and overlapping notes in the score export. The
Analyzer (version 0.1.0) is a second submodule, `external/ursatz-analyzer`,
pinned at PR #38 (`8a2fd29`).
`git submodule update --init --recursive` (or `git clone --recurse-submodules`
up front) is required before the first build. Ursatz's own tests are not
built (`URSATZ_BUILD_TESTS` is OFF). The Composer is a third submodule,
`external/ursatz-composer`, pinned at PR #34 (`1786b33`). It is built by an
`ExternalProject` in `build/composer` (its tests off), so it brings its own
Ursatz and Analyzer; the build runs it as `ursatz-composer` and does not link
it.

## Tests

`ctest` runs the backend tests (health, analyzer, corpus, styles, composer, materials, subjects and id routes, the piece metadata rules and the material store) and,
when `node` is installed, the frontend tests too (`frontend/tests/*.test.js`,
registered as `frontend_<name>`). Without node CMake prints one status line
and skips them. The frontend tests are plain Node scripts that can also be
run directly, for example `node frontend/tests/analysis-view.test.js`.
`analysis-view.test.js` loads `analysis-tabs.js` and checks it against
`frontend/tests/fixtures/mozart545.analysis.json`, the Analyzer CLI's
document for Mozart 545 cut to its first 6 items per array.
`composer-view.test.js` loads `composer.js` and checks the plan tree and the
request bodies against `frontend/tests/fixtures/composer-plan.json`, the
`plan.json` the Composer CLI wrote for a rounded binary form with seed 3.
`test_analyzer_route` uploads Mozart 545 and a MusicXML score, compares the
stored analysis with the Analyzer CLI's document for the same file, and
covers list, detail, metadata editing, filtering and groups, rename,
re-run and delete. `test_styles_route` uploads Clementi 36 and Mozart 545,
tags them and builds styles from a tag, a collection and ids, then reads the
model back with the Analyzer, rebuilds, re-analyzes an old piece and deletes.
`test_piece_metadata` covers the normalizing, grouping and
filter rules without a server. `test_material_service` covers the projects,
items, versions, version body rules and item references without a server. `style-view.test.js` covers the style detail
rows (order, fractions, "and n more") and `piece-metadata.test.js` the style
names and request bodies. `test_composer_route` runs
the real Composer: grow, regenerate, lock, variations, choose and plan make
versions and leave the earlier ones alone; bad requests start nothing and a
fake Composer (`URSATZ_GUI_COMPOSER`) that records its arguments or sleeps
shows the argument lists and the 504.

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on every push and
pull request. The private submodules are checked out through the
`URSATZ_SUBMODULE_TOKEN` repository secret, which must be set.

## Out of scope

Native webview packaging, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists). The
Composer view has no style, blend, corpus seeding or explain, and no playback
in the browser (MIDI is downloaded).
