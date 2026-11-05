# Analysis and score views

The Subjects views, the Motive links view, the Analysis view, the Score, its layers, and the Inspector. [Docs index](../README.md).

## Subjects views

A Subjects panel sits in the sidebar under Tags: subjects grouped by kind
(Characters, Places, Ideas, Notes), each group collapsible (the choice is
remembered in the browser) with the number of links beside each name. A click
opens the subject. The Subjects entry lists every subject (name, kind, item,
piece and related counts) with a New row (name, kind, note). The detail edits
the name, kind and note, deletes the subject (its links go with it), and
lists its related subjects (add from a picker, remove), linked items (any
project; a click opens the item in its own tool) and linked Library pieces (a
click opens the analysis). Linking starts from the other side: an item's
detail and the Library piece editor have a Subjects field with chips (with
remove), a picker over the existing subjects, and a name and kind to create
one inline. The sidebar panel refreshes after any subject or link change.
Style tags are separate and unchanged.

## Motive links view

The Motive links entry (after Subjects) shows which motive families of different pieces are variants of one another
(routes: "Motive links routes"). A scope picker offers All Library pieces, Collection, Composer / Source / Tag
(any of the three), Subject and Project, each with a picker over the existing lists. Run starts a run and the
progress line reads `Comparing <done> of <total> pairs`, polled every second; when it is done it reads
`compared <n>, cached <n> of <n> pairs`, with the skipped pieces beside it. The result lists the groups of linked
families first (piece, measures, degrees; a click opens that piece's Analysis on the motive), then the links table
(both sides with measures and degrees, the steps joined by "then", the distance as a percent, and the dimensions kept
and changed). Earlier runs are listed with their scope and date; each opens or is deleted. The model is
`frontend/public/motive-links-model.js` (global `MotiveLinksModel`: `scopeBody`, `progressText`, `linkRows`,
`groupRows`, `motiveFeatureRows`, `pieceLinkRows`); the DOM is `motive-links-view.js`.

## Analysis view

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

Opening a piece asks for `GET /api/analyzer/pieces/<id>?document=summary`, then for the document sections the
score draws its layers from (`ScoreLayers.scoreSectionNames`: every section but the ignored ones, and `rhythm` for
the meter check) with `GET .../document?sections=`. The other sections load when a Data mode tab opens
(`loadTabState` in `analysis-tabs.js`) and are kept per piece in the page (`createDocumentCache`), so a section is
fetched once. A tab's count shows once its sections have loaded.

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

In the Motives tab each family shows its features (degrees, intervals, rhythm, non-chord tones by note) and a
"Linked" list from `GET /api/links/pieces/<id>` (the other piece, its measures and the steps, each opening that
piece on the motive); with none it reads "No links yet. Run Motive links over a scope with this piece."

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

## Score

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

### Layers on the score

The Layers rail (left of the score) lists the layers of
[Score layers](../../README.md#score-layers) in groups, each with a switch, its color, its
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

### Selecting claims and the Inspector

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
