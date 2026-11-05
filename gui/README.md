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
- **Subjects**: named characters, places, ideas and notes that items and
  Library pieces link to (see Subjects views, below).
- **Motifs**, **Themes**, **Pieces**: the items of the material store (see
  Materials views, below).
- **Motive links**: the links between motives of pieces and items (see Motive links view, below).
- **Health**: last entry in the sidebar. Shows the backend health check; a
  failed check appears only here.

The GUI opens on the Library.

### Materials views

The project picker sits at the top of the sidebar: All projects plus each
project, with New, Rename and Delete (Rename and Delete need a project
picked; deleting a project that still holds items shows the backend's 409
message). The choice filters the Motifs, Themes and Pieces views and is
remembered in the browser. Each view lists its items (name, project, current
version, updated) with a New row (name and project). A row opens the detail:
the name and project are editable, the uses are listed, and the versions list
(number, date, note) lets any version be viewed read-only. Themes and pieces
have body editors (below). Subjects and item references are described below.

#### Motif editor

A motif's detail (`motif-detail-view.js`) has the name, the project and
Delete, then four tabs: Motif, Variants, Used by and Other. The chosen tab is
remembered in the browser (`ursatz.motif.tab`). The Motif tab holds the score
with its play controls, the editor, the motif's Subjects, the Analysis panel
and the one Save. Save takes a version note (`What changed? (required)`): a
change to the editor writes a new version with that note and says `Give a
version note.` beside the note when it is empty; a changed name or project
goes through the item update and needs no note; with no change it says
`Nothing to save.` The backend refuses a new motif version without a note
(400 `give a version note`); a new motif's first version is noted `Created`.
Variants holds the chain builder with Generate, the candidates, then the saved
variant tree, each with its notation, and Try every operation. Used by is a table of the items that use the
motif (name, type, the version used, project, and `update available` when the
use is behind the motif's current version). Other holds Versions (open one to
read it), Derived from (the motif's old Uses section: the motifs it was
derived from, with Add and Update) and Compare.

The whole Motifs section (the Motifs list, the New Motif page and the motif detail, all in a `motifs-section` wrapper) shares one page layout (R-191, R-193); other views keep their widths. The tabs sit in a
`motifs-section` wrapper up to 1200px wide, with no 480px cap on the sections,
so the score uses the full width; the score's zoom controls stay, and the
compact scores in the Variants tab show zoom out, the zoom label and zoom in.
The motif fields are a two-column grid (`motif-form-grid`): labels right-aligned
in a 10rem column, controls in the other, the note list and Harmony at full
width, hints and field errors under their control; it is one column under
700px. Spacing is 0.5rem inside rows, 1rem between fields and 1.5rem between
panels; panels share one border, radius and padding. Each panel has one primary
button (`button-primary`: Save on the Motif tab, Generate on the Variants tab),
the others are `button-secondary`, and Delete and Reject are `button-danger`.
Button rows are right-aligned. Each tab has one status area at its top
(`motif-tab-status`, `role="status"`) for saves, errors and progress; the
sections write to it through `MaterialsModel.motifTabStatus`. The order is
score, editor, analysis, Save row on the Motif tab, and generate panel,
candidates, saved variants on the Variants tab.

New on the Motifs view opens the New Motif page: name, project, a Subjects
picker (the chosen subjects are linked after the first Save; a link that fails
shows on the detail page and the motif stays saved), the same editor, Save
(it sends the note `Created`) and a Random section (below). The form has the role (melodic or accompaniment), the form (only
full can be chosen; rhythm and contour are shown disabled, P13), the note
list with its syntax hint, the meter, a Mode select (major or minor), the pickup in exact
beats (`0`, `1` or `1/2`), an optional Base harmony field and a Segments table
(named note ranges such as head and tail: label of 1 to 8 characters, first
and last note counted from 1; rows are added and removed in place; Liquidation
and the motive-forms keep or drop them, and the variant picker calls the
`feature` parameter `segment`. The stored field stays `features`). Base harmony is the motif's own chords as the Composer
reads them (`I V6@2 I@3`: upper case Roman numerals, `b` or `#` before one
for an altered root, figures `6`, `64`, `7`, `65`, `43`, `42`, and `@` the
onset in beats; the first chord needs no onset; up to 8 chords, at most 2 in a
bar, at most 200 characters). When the field is empty the Composer proposes a
base harmony from the notes, and the placeholder shows it (`Proposed: I V@2`,
`I (proposed)` before the first analysis). The Composer reads the values while
they are edited (400 ms after the last change, `POST /api/materials/items/validate`):
a note list, meter, pickup or chord it cannot read shows its message, as given,
under the field its request path names. The tone choices, the modes and the tone
abbreviations are the Composer's lists (`GET /api/composer/vocabulary`); the
choice for no override is empty. So are the roles and forms of a motif, the phrase
functions and distances of the variant tree, and the slots a theme form has (the
slots of its ideas): the frontend and the backend keep none of these lists, and
`scripts/check-frontend-theory.sh` refuses their old table names.

Tone labels (R-195, R-196): under the note list the Tones row has one chip per
note, `<note> <label>` with `-> <n>` for the note it resolves to. The label is
`CT` (chord tone), a type (`PT` passing, `NT` neighbor, `APP` appoggiatura,
`SUS` suspension, `RET` retardation, `ET` escape, `ANT` anticipation, `DN`
double neighbor, `CT2` changing tone, `PED` pedal) or `?` (unclassified). A
chip with no resolution is drawn in the warning style. Clicking a chip opens a
select (Automatic, Chord tone, each type) that sets an override, stored in the
version as `motif.tone_overrides` (`{"2": "passing"}`, 1-based notes, the roles
of the Composer's `tones` request member) and sent as `tones` when the motif is analyzed on
save or Re-analyze; Automatic removes it, and a changed override is a new
version like any edit. The Composer's warnings (`Note 3 (B5) does not resolve`)
show in the Motif tab's status area. On the first start with this version every
full motif whose current analysis has no tones is analyzed again (a new version
noted `Re-analyzed`); the file `materials/tones-migrated` records that it ran,
and a motif the Composer cannot analyze keeps its `analysis_error`.

Motifs are written with C as the tonic (R-179): the note list is C-based and
the Mode select sets major or minor, so every motif is stored with the key
`C major` or `C minor`. A piece sets the real key. Upload MIDI moves the notes
to C and says so (`Moved from D minor to C minor.`, or `No key found: read as
C major.`). A motif saved before this change keeps its old key: the editor
shows `Written in D minor before motifs moved to C.` with a Rewrite in C
button that writes the stored degrees in C, and saving waits until it is
rewritten. The score and playback always use the stored key.

The Composer's
message for a note list it cannot read shows beside the note list and the
input is kept. The Analysis panel under the form shows the scale degrees,
intervals (semitones and steps), contour, rhythm and implied harmony stored
in `motif.analysis`. A status line above it reads "Analyzed" or "Not
analyzed: <reason>" (the reason is stored with the version as
`motif.analysis_error`), and Re-analyze runs the Composer on the current
version again and saves the result as a new version with the note
"Re-analyzed". A banner at the top of the page says when the Composer is not
available. The Variants section says why a motif cannot be varied when it has
no degrees. The helpers (`motifFormValues`,
`motifFormProblems`, `buildMotifBody`, `featureProblems`, `analysisRows`) are
in `materials-model.js`.

#### Themes view

A theme's detail is a page in four tabs at the full width (`theme-detail-view.js`,
`ThemeDetailView`; the editor is in `themes-view.js`, the helpers in
`themes-model.js`, global `ThemesModel`). The open tab is remembered. It edits
the theme body the backend composes (see Theme bodies and version files).

- Theme tab: subjects, mode (major, minor, From the brief; the first basic
  motif sets it to that motif's mode unless it was changed by hand), meter and
  tempo (blank takes the brief's). An Advanced section, closed by default,
  holds the accompaniment, the brief, the switches, weight, Check with the
  Analyzer and Seed. Below are the composed theme (Result) and the one Save row.
- Structure tab: form: into (sentence, period, hybrid with its pairing, and the
  first cadence for a period or hybrid), bars and cadence, all from the Composer's
  vocabulary. A theme is always in C: there is no key. The slots are below.
- Used by tab: the items that use the theme. Other tab: the versions with their
  notes (View opens one read only) and Derived from.
- Slots: only the slots the form has are shown. Basic idea (required) is always
  there; Contrasting idea comes with an antecedent or cbi beginning or a
  consequent ending, Continuation with a continuation ending, Cadential with a
  continuation or cadential ending. Each is a motif picker: a project select, a
  name search, the motif and its version (current by default). Only a full
  melodic motif is accepted. The picker shows the motif's degrees and a Play
  button (the render route). Beside it a Use select takes the motif itself or
  one of its saved variants (rejected ones left out; the derivation is the
  option title); a variant stores its own item id and version. The same motif
  or variant may fill several slots.
- More slots (closed by default): every idea of the form but the basic idea,
  each "From <slot>" until it gets a motif (and Use) of its own. Ideas are kept
  in the body's `ideas`; those the form does not have are dropped on save. The
  result table says "From its own motif" for them.
- Chords: under the slots, one text field per bar (`Bar 1` to `Bar 4` or `Bar 8`),
  placeholder `Composer`. Roman numerals in C, at most 2 a bar, `@` the beat a
  chord starts on from 0. The Composer writes chords on the cells of the meter
  only: a second chord at `@2` in 4/4, `@3` in 6/8, none in 2/4 and 3/4 (the
  hint under the row follows the meter). The Composer checks the chords, like the
  other values of the theme (meter, bars, hybrid, tempo, seed), while the form is edited
  (`POST /api/materials/items/validate`, 400 ms after the last change): its message shows under the
  Save row, as given, and the field its request path names is marked. It does not block the form: a
  draft the Composer refuses still saves, and Compose refuses it with the same path. Only what the
  form lacks to be a request (the basic motif, a motif for an idea, the accompaniment motif, a brief
  for the mode From the brief) stops Save and Compose. The Composer reads a chord as a numeral with
  onsets that rise, are whole or half beats, fall inside the bar and on a cell
  of the meter; its message names the bar. Bars 8 to 4 clears the chords of bars 5 to
  8 with a status line. The body keeps `chords` as `[{bar, text}]` (bar from 0)
  for filled bars only; the result lists them as Given chords. A Composer
  refusal naming a bar shows in the tab status.
- Accompaniment: None, a pattern (any, block, arpeggiated, Alberti, oom-pah)
  or a motif with the role accompaniment.
- Brief: collections, composer / source / tag filters, subjects, projects,
  single pieces and styles, listed as chips with Remove. An empty brief means
  no style. Four checkboxes ("Use the brief's motive degrees, intervals,
  rhythm, non-chord figures") are all on by default; an unchecked one goes to
  `without`. Weight is Light (1/4), Normal (1/2) or Strong (1). Check with the
  Analyzer and Seed (blank draws one).

Save is the one save of the page: a changed name or project goes through the
item update, a changed form writes a version without `result` with the note
("What changed? (required)"), and nothing changed says "Nothing to save."
Compose saves first when the form changed (the note is required), then calls the compose route with the buttons disabled
and a spinner; a refusal shows the backend's message, which carries the
Composer's message. A slot or accompaniment motif with a newer version shows
"Source updated: <name> v<n>" with Update and compose (the compose route's
`update_uses`) and Keep.

The Result panel of the current version, and of any version opened from the
versions list (read-only), has a row for each idea (idea, slot, motif, source,
chain, connection), the brief's tempo, mode and note density, the motive
categories read without, the style distance, the form heard and the Analyzer
check. Below it are the score of `theme.musicxml` in the score workspace and a
MIDI download. Playback is served by the backend, built by the Analyzer from
`theme.mid` (`GET .../versions/<n>/playback?file=theme`).

#### Pieces view

A piece's detail holds the piece editor (`pieces-view.js`; the helpers are in
`pieces-model.js`, global `PiecesModel`). It edits the piece body the backend
composes (see Piece bodies and version files) and reuses the Themes view's
builders (motif picker, pool, accompaniment and brief in `themes-view.js`).

- Header: key, mode (major, minor, From the brief), meter and tempo (blank
  takes the brief's). The Composer reads the piece while it is edited (400 ms after the last change,
  `POST /api/materials/items/validate`): the key, meter, tempo, seed, pickups, numerals and sizes it refuses show its
  message, as given, and mark the header or the strip its request path names. Nothing of that blocks Save as new
  version; only a missing brief for the mode From the brief, the loop body, the order of the parts and a theme of
  another length do.
- Structure strip: four lanes, Intro, Loop body, Turnaround and Ending, each a
  row of section blocks as wide as their beats. Add section puts a section at
  the end of its lane. Selecting a block opens its editor: letter, length (a
  bars and a beats input, giving beats), pickup (beats of anacrusis, `0` for
  none, or `n` or `n/d` such as `1` and `1/2`; the choices are the Composer's for the meter, 2/4 has no `2`), cadence
  (Default shows the cadence the Composer gives a section that has none, from the validate reply and asked again
  when the structure changes; the GUI computes none, so the label reads plain Default until the reply is in), Move left and Move right
  within the part, and Remove section. A piece has at least one section in the
  loop body (the Composer holds the largest number of sections and beats).
- Fill: Empty, Motif, Theme or Progression. Any fill may be left empty and
  saved; one that names nothing is saved as empty. A motif fill has a motif
  picker and, at 4 or 8 whole bars without a pickup, into sentence, period or
  hybrid (with its pairing); any other length shows "Free phrase". A theme
  fill picks a composed theme version (a theme with no composed version asks
  to be composed first), shows its form and key and sets the section's length
  from the theme; a section whose length differs from the theme's is a
  problem. A progression fill is numerals (such as `I vi IV V`) and cells per
  chord; it does not change the section's cadence.
- Basic motive (optional), Pool, Accompaniment (none or a pattern; a piece has
  no accompaniment motif) and Brief (chips, the four motive switches, weight,
  seed, Check with the Analyzer) are the Themes view's.

Save as new version writes a version without `result`. Compose saves first
when the form changed, then calls the compose route with the buttons disabled
and a spinner; a refusal shows the backend's message and the Composer's. A
motif or theme with a newer version shows "Source updated: <name> v<n>" with
Update and compose and Keep.

The Result panel of the current version, and of any version opened from the
versions list (read-only), shows the seed, key, loop region, brief and, for
the loop version and the ending version, a row for each section (section,
part, letter, start, length, cadence planned and heard, fill, source), the
score, the seam rules that held (loop only), the Analyzer check and the style
distance. Below are the MIDI downloads and the scores in the score workspace,
`loop.musicxml` with a tab for `ending.musicxml`, with playback served by the
backend from `loop.mid` or `ending.mid` (`GET .../versions/<n>/playback?file=loop|ending`).

On the current version each section of the loop has Regenerate (lane: all
lanes, harmony, bass, melody or inner; seed) and Variations (lane, count 1 to
8, seed). A regenerate becomes the next version. Variations show side by side
as small scores with Choose, which makes one the next version. A section
filled with a theme offers harmony and bass only; an ending section offers
neither. Harmonize is Variations on harmony.

### Item references in the GUI

An item's detail has a Uses section (Derived from on a motif, in its Other tab): each use shows the source's name, type,
project and the version used, with Remove. Add use picks from the items of
every project (search by name, filter by type and project, the item itself and
its current uses left out); the use points at the source's current version.
Adding or removing a use writes a new version of the item with a note. A use
whose source has a newer version reads "source updated to v<n>" with Update
(moves the use to the current version, as a new version) and Compare (the used
and current versions side by side, read only). A use whose source was deleted
reads "source deleted"; its copy is kept. Used by lists the items that use
this one, each opening its detail (a table on a motif). In the Motifs, Themes and Pieces lists a
row gets an "uses updated" badge when any of its uses has a newer source.
Delete asks to confirm; when other items use the item it names them and asks
again, and confirming deletes with `?force=1` (their copies stay and their
uses report the source as deleted).

### Subjects views

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

### Motive links view

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
- Loop (pieces): the loop version of a piece in the Pieces view plays its
  intro once, then repeats the loop region from the result (`result.loop`)
  without a gap. The transport schedules each pass ahead of the clock
  (`createTransport` option `loop: {start, end}`, whole notes), so notes after
  the wrap are queued before it is reached; a note that sounds past the loop
  end is cut there. The readout and the cursor show the position inside the
  piece. The Loop button turns the repeat on or off while playing (on by
  default); a claim or range play ignores it.
- Play ending (pieces): the pass goes on to the end of the last body section,
  then the notes of the ending version (`ending.mid`) play from there to its
  end and playback stops. Pressed before the body's end of the current pass it
  switches there; pressed in the turnaround it waits for the next pass's body
  end (`transport.playEnding(endingTimeline, bodyEnd)`). Stopped, it starts
  from the beginning. While the ending plays the workspace shows the ending
  version's score (without layer highlights) and goes back to the loop score
  when playback stops.
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

- `util/fs_util.c`: `ursatz_gui_dup_string`, `ursatz_gui_iso_utc_now`, `ursatz_gui_path_is_dir`, `ursatz_gui_make_dir`,
  `ursatz_gui_ensure_dir`, `ursatz_gui_write_file_atomic` (writes `<path>.tmp`, then renames), `ursatz_gui_read_file`
  and `ursatz_gui_read_json_object`. Every store and the routes use these; none keeps a copy. `link_service.c` keeps
  its own `iso_utc_now`, which adds milliseconds so two runs in one second still order.
- `util/store_lock.h`: `UrsatzGuiLock` (a pthread mutex, an SRWLOCK on Windows) with `URSATZ_GUI_LOCK_INIT` and
  `ursatz_gui_lock_acquire/release`. Each store wraps it in `LOCK()` and `UNLOCK()`.
- `service/piece_lock.c`: `ursatz_gui_piece_lock(id)` and `_unlock`, one mutex per piece directory (see Piece store,
  Concurrency).
- `routes/route_util.c`: `ursatz_gui_send_json_status` and `ursatz_gui_read_json_body`, the JSON object body of a
  request (up to 1 MiB; with a status pointer it also sends the 400, 413 or 500 itself). `routes/materials.c` passes
  its own 256 KiB limit to `ursatz_gui_read_json_body_max`.
- `service/compose_common.c`: the helpers the theme and piece services share (`ursatz_gui_compose_say`, `_text_of`,
  `_attach`, `_add_copy_or_null`, `_add_copy_if`) and `ursatz_gui_compose_item`, the steps around a Composer run
  (update the uses, make the version folder, build the brief and the request, run, add the version, rename the folder
  when another writer added a version). Each service passes an `UrsatzGuiComposeKind` with its own checks, request,
  run, note and result body; there is no compose lock: each compose makes its files in a staging folder
  (`ursatz_gui_compose_stage_make`) and moves it to its version's folder when the version is written.
- `service/job_queue.c` and `service/compose_jobs.c`: the job queue and the compose runs as jobs (`routes/jobs.c` are
  its two routes). `frontend/public/backend-jobs.js` is the client: `runJob` sends a job request, polls every 500 ms
  and resolves with the status and body the route answered before jobs; `sendMaterialsRequest` runs every request
  marked `job: true` through it and shows the stage and a Cancel button while it runs.

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

#### Concurrency

One lock per piece directory (`service/piece_lock.c`, a table of per-id mutexes with a reference count; an entry
is freed when its count reaches 0) is held around every writer of `pieces/<id>/` and every read that must not see
half a write. All of them are in `service/analysis_service.c` and take the lock themselves, so no caller holds it:

- upload (`ursatz_gui_analysis_import_and_save`, from the moment the directory exists until it is complete);
- PATCH (`ursatz_gui_analysis_update_piece`: the metadata, the re-analysis a changed notation needs and, if that
  fails, the undo of the notation, as one hold);
- re-analyze (`ursatz_gui_analysis_reanalyze`), which the style build and the link worker call for a stale piece;
- delete (the directory removal; the collection and subject cleanups run after the lock is released);
- the reads of `analysis.json` plus the source: detail, `/musicxml`, `/musicxml/meta` and `/playback`.

#### Import cache

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

### Material store

Motifs, themes and pieces live in `materials/` beside `pieces/` and
`collections/`. `service/material_service.c` is the only code that touches
it; every operation runs under one mutex, and every file is written to
`<name>.tmp` and renamed.

Compositions from the old Composer view (`compositions/`) are no longer read and may be deleted.

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
  characters), `features`, `analysis` and `derived`; the rules are listed in
  `service/material_service.h`.
- Projects and items are passed as cJSON objects; the caller frees what it
  receives.

#### Theme bodies and version files

A theme body (the rules are in `service/material_service.h`) is a placeholder or a composable theme (R-167). The
placeholder, `form` null or a string and `slots` an array of anything, stays valid so earlier versions still read. A
body with a `form` object, a slot written as `{slot, item_id, version}` or any of the members below is a composable theme:

- `form`: `{into, hybrid, mode, meter, bars, cadence, first_cadence}`: each member null or text (`bars` a number), in
  the words of the Composer's vocabulary (`GET /api/composer/vocabulary`: `into` `sentence`, `period` or `hybrid`, `mode`
  `major`, `minor` or `style`, `meter` `2/4`, `3/4`, `4/4` or `6/8`, `bars` 4 or 8...). A save keeps the type and size
  of a word and not the word: a value the Composer does not read is saved, `POST /api/materials/items/validate` marks
  it and compose refuses it with its path (R-276). Compose needs `into`, `mode` and `meter`. A theme is in C: a body
  with a `key` is refused (`themes are in C; remove key`).
- `slots`: `[{slot: "basic" | "contrasting" | "continuation" | "cadential", item_id, version}]`, `basic` once and each
  slot at most once; the same item may be in several slots. Each item is a melodic motif of form `full` at an existing
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

#### Piece bodies and version files

A piece body (the rules are in `service/material_service.h`) is a placeholder or a composable piece (R-174). The
placeholder, `structure` null, a string, an array or an object without a list of `sections`, stays valid so earlier
versions still read. A body whose `structure` has a `sections` list, or that has any of the members below, is a
composable piece. A structure alone is valid: every fill may be left out.

- `structure`: `{key, mode, meter, sections}`: `key`, `mode` and `meter` null or text, in the Composer's words (a
  value it does not read is saved and marked by validate, R-276). `sections` is 1 to 32 of
  `{id, part, letter, beats, pickup, cadence, fill}`: `id` is `s<n>` or `end`, each once; `part` is `intro`, `body`,
  `turnaround` or `ending`, in that order, with at least one `body`; `letter` 1 to 8 characters; `beats` a whole
  number from 1 to 2048; `pickup` `n` or `n/d` beats; `cadence` null or text.
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
(when the piece has a brief), `plan.json` (the loop version), `ending-plan.json`, `loop.mid`, `loop.musicxml`,
`ending.mid`, `ending.musicxml`, and `theme-<section id>.plan.json`, a copy of the plan of each theme version a
section uses, so the version keeps its own. The folder is written before the version exists. A regenerated or chosen
version keeps the same files (its `request.json` names the copies of the theme plans and the style file beside it).
A variations set is a folder `variations-<k>/` in the version it was made from, numbered over the whole item:
`variation-<i>.json` (the loop plan), `variation-<i>.mid`, `variation-<i>.musicxml` and the Composer's `result.json`.
It is not a version; deleting the item removes it.

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

#### Motif analysis on save

`POST /api/materials/items` and `POST /api/materials/items/<id>/versions`
analyze a motif with `form` `full` before it is written. The route builds the
motive request of the Composer API (`input.text` the `notes`, `input.format`
`notes`, `meter`, `key`, `pickup`, and `harmony`, `features`, `role` and
`tones` when the motif has them) and calls `composer_api_motive` in process,
then stores the output as `motif.analysis` (scale degrees, implied harmony,
intervals, rhythm). Meter (`^[0-9]{1,2}/[0-9]{1,2}$`) and key
(`^[A-Ga-g][#b]? (major|minor)$`) are checked before the request is built.

- A note list or key the Composer refuses is 400 (a malformed request) or 422
  (`COMPOSER_ERR_UNSATISFIABLE`: a valid request nothing satisfies) with its
  message and, as `path`, the request member at fault (`/meter`, `/harmony`,
  `/input/text`); no version is written.
- A failure of the Composer that is not the request's (out of memory, an
  internal error): the version is saved with `analysis: null` and
  `motif.analysis_error` holds the reason (`composer could not analyze the
  motif: <message>`); `analysis_error` is `null` when the motif was analyzed.
  The answer carries `analysis_error` too.
- `POST /api/materials/items/<id>/reanalyze` (motifs only, else 400 `only
  motifs are analyzed`) analyzes the current version again and writes a new
  version with the note `Re-analyzed`. A refusal is 400 and writes nothing.
- A motif body whose `key` is not `C major` or `C minor` is 400 `motifs are
  written with C as the tonic`, checked before the Composer runs (re-analyze
  keeps an older motif's key).
- `POST /api/materials/motifs/to-c` `{degrees, mode, meter, pickup?}` writes a
  full motif's degrees as C notes: `composer_api_motive` with `input.format`
  `degrees`, `key` `"C <mode>"`, `meter` and `pickup`, answering `{ok, notes_text}`.
  `degrees` is 1 to 4000 characters; the Composer refuses a mode it does not know.
- `POST /api/materials/motifs/import` answers the notes in C: `{notes_text,
  meter, key: "C <mode>", moved_from: "D minor" | null, key_found, pickup,
  analysis}`. With no key found the notes are unchanged and the key is `C
  major`.
- `rhythm` and `contour` motifs are saved with `analysis: null` until P13.
  The stored `pickup` is not passed to the Composer yet.

#### Motif score and playback

A motif's stored `motif.degrees` is shown in the score workspace and sounded
in the motif's stored key (C major or C minor). Nothing is saved.

- `POST /api/materials/render` takes `{form, degrees, meter, key?, pickup?,
  render_key?, tempo?, role?, harmony?, features?}` (`degrees` 1 to 4000 characters; `tempo` a number or text like `195/2`;
  the form and the tempo range are the Composer's to refuse) and answers
  `{ok, musicxml, playback}`. The route calls `composer_api_motive_render` with
  `input.format` `degrees` (`rhythm` or `contour` for those forms), `input.form`, `meter`, `key`
  (when given), `pickup`, `harmony`, `features`, `role`, `render_key`, `tempo` and `musicxml: true`; the
  MusicXML is the response's `musicxml`, not a file.
  `render_key` defaults to `key`; without either the answer is 400 "give a
  key". A list the Composer refuses is 400 (malformed) or 422 (nothing satisfies it) with its message.
- `playback` is the Analyzer's playback of the render's MIDI file
  (`analyzer_playback_to_json`: `tempo_source`, `tempo`, `parts`, `end`,
  `notes`). Positions are whole notes from the pickup start, so the first note
  is at `[0, 1]`. The backend writes the MIDI bytes to
  `materials/.render-tmp/<random>.mid` (the Analyzer imports by path), reads
  it and deletes it; nothing is cached for this route.
- `GET /api/materials/items/<id>/versions/<n>/score?key=` answers MusicXML
  and `.../playback?key=` answers the playback JSON for that stored version;
  `key` defaults to the motif's own key, and a version without degrees is 404.
  Both come from one render kept under the version in
  `items/<id>/v<n>/render/`: `score.musicxml`, `motif.mid` and `render.json`
  `{api_version, composer_version, ursatz_commit, render_key, tempo}` (the three
  versions from `composer_api_versions`). A request with the same key, tempo
  and Composer build and Ursatz commit reads these files, an older cache missing once, so the Composer runs at most once
  per version and key.
- `GET /api/materials/items/<id>/versions/<n>/playback?file=theme|loop|ending`
  answers the playback of a theme or piece version's `theme.mid`, `loop.mid`
  or `ending.mid` (400 for another `file`, 404 when the version has no such
  file).
- `GET /api/materials/items/<pce-id>/versions/<n>/variations/<k>/<file>/playback`
  answers the playback of a variation's MIDI file (`variation-<i>.mid`; 404 for
  any other file).
- What the served playback differs in from the browser's former reader:
  `tempo_source` is `file` or `default` (the browser said `given`); tempo
  values are rounded to thousandths; positions are reduced fractions; part
  names are empty for a MIDI file (the score workspace shows `Part <n>`).
- The motif detail mounts `ScoreWorkspace` with these URLs. The routes still
  accept `render_key` and `?key=` (pieces may use them), but the motif detail
  sends neither. `MaterialsModel.renderRequest(body)` builds the POST for an
  unsaved candidate.

#### Variant tree

The motif detail has a Variants section (`variant-tree-view.js`, rendering
from the pure `variant-tree.js`, global `VariantTree`).

- Generate: a count (1 to 16, default 6), a seed (blank draws one) and an
  optional chain. The operation picker lists the registry grouped by category
  (exact, rhythm, intervals, harmony, melody adaptation) with each operation's
  citation and its parameters; a parameter left empty is drawn from the seed.
  With no steps the route draws random chains. Candidates are not saved.
- A candidate shows its derivation, its notation, kind, kept and changed chips
  and recognizability. The notation is a compact score workspace (title,
  Play/Stop and tempo only) rendered with `POST /api/materials/render`, at most
  four requests at a time, and mounted through blob URLs that are revoked when
  the candidates are redrawn or the tab is left. Approve and Reject save it as
  a derived motif with that flag; Reroll replaces only that candidate with the
  next seed (its drawn parameters drawn again); Play plays its own score.
- Saved variants appear under the motif as the variant tree, indented by
  depth, with the derivation, flag, a kind select, Approve, Reject, Unflag,
  Reroll (the reroll route), Play (the stored version's score and playback;
  it opens the notation first) and Show notation. Show notation mounts
  a compact score for that row; open rows stay open when the tree refreshes.
  Only one workspace plays at a time on the page: starting one pauses the
  others. Rejected variants and everything under them are hidden unless
  "Show rejected" is on; the choice is kept in `localStorage`.
- A variant belongs to its motif (R-187). A motif version is a variant when
  its `motif.derived` record exists and `derived.standalone` is not `true`
  (`standalone` is an optional boolean the derived validation accepts;
  `ursatz_gui_material_item_is_variant(body)`). Every item answer from the list
  routes and the item GET carries `variant_of`: the `derived.source_item` of a
  variant, else `null`; it is computed from the current version and never
  stored. Variants are left out of the Motifs list, the motif, theme and piece
  pickers and their search (`MaterialsModel.isVariantItem`), and have no page
  of their own: the tree has no Open button, and a link to a variant lands on
  its motif's Variants tab. Deleting a motif deletes its variants, recursively,
  after the confirm `Delete "<name>" and its <n> variants?`; a theme or piece
  that uses the motif or any of its variants still gets the 409 with `used_by`
  (the users outside that set), and `?force=1` deletes them all. Linking a
  variant to a subject is refused (400 `variants have no subjects; save it as a
  new motif first`). Flag, kind and reroll stay on the variant.
- Variant harmony and tones (R-197): each candidate card, saved variant row and
  Try every operation result shows `Harmony: <base harmony text>`, the read
  only tone chips of the Motif tab and its warnings (for example `Does not
  outline its harmony`). The derivation line adds `, harmony <source text> to
  <result text>` when the base harmony differs from the one it was made from
  (the source motif for a candidate, the parent row for a saved variant).
  Saving a candidate stores `derived.base_harmony`, `derived.tones` and
  `derived.warnings` from its analysis, which the variant tree route returns
  with the node.
- Try every operation (R-190): the last panel of the Variants tab runs every
  registry operation on the motif so each can be checked by ear and eye. Run
  (with an optional seed) calls `POST /api/materials/items/<id>/each-op`
  `{"seed"?}`, which runs the Composer's `each_op` variants (a job) on the
  current version with the same motif arguments as Generate and answers
  `{seed, variants, skipped}` plus `ok`; nothing is saved. `400` for a seed
  that is not a whole number from 0. Results are grouped by category in
  Schoenberg's order and stay in registry order inside it. Each shows the
  operation's label and citation, a compact score with Play (rendered through
  the same queue of four as the candidates), the derivation line, `Works` and
  `Wrong` buttons and a comment field. Operations the motif refuses are listed
  last with their reason.
  - Marks are kept per motif in `items/<id>/op-checks.json`, beside the
    versions and not a version, so marking never makes a new version.
    `GET /api/materials/items/<id>/op-checks` answers `{"checks"}` (empty when
    there are none) and `PUT` replaces it: `{"checks": {"<op>": {verdict
    ("works" | "wrong"), comment (up to 500 characters), seed, notes_text (the
    result checked), date ("YYYY-MM-DD")}}}`, at most 64 operations, validated
    by `ursatz_gui_material_op_checks_validate` (`400` with the reason, `404`
    for an item that is not a motif). Deleting the motif deletes the file.
  - A marked result shows its verdict, comment and date, and `changed since`
    when the stored `notes_text` differs from the new result.
  - `Copy findings` copies plain text for P18b: the motif's name, `Notes:`,
    `Mode:` and `Meter:`, a blank line, then one line per marked operation,
    `<op>: works|wrong - <comment> (result <notes_text>)`, the wrong ones first.
    When the operation's current result has a base harmony, the line ends
    `(result <notes_text> harmony <text>)`. P18b starts from that text.
- Save as new motif (R-188): each saved variant has a `Save as new motif`
  button that opens an inline form (name, project defaulting to the motif's,
  Save). Save calls the promote route and opens the new motif's Motif tab,
  where subjects can be added. A promoted motif keeps its id, versions and
  `derived` (so Derived from and the lineage stay) and is written with
  `derived.standalone: true` (R-192), so it is a motif of its own: it appears
  in the Motifs list and pickers, no longer among the saved variants, and the
  Variants tab lists it under `Promoted` as `<name> (opens it)` with its
  derivation line. A motif linked as a form of another (link-form) is written
  standalone too. A promoted motif and its own variants stay out of the source
  motif's theme pool: `ursatz_gui_theme_add_pool` skips standalone entries and
  the derived chain it follows stops at one. Promoting cannot be undone.

#### Comparing and linking motifs

The motif detail has a Compare section (`motif-compare-view.js`, rendering from `VariantTree.compareRequest`,
`linkFormRequest` and `compareView`). Pick another motif, from the motif's own project or any project (a project select
and a name search), and press "Compare with". The result panel shows the chain, the derivation sentence, the kept and
changed chips and the distance as "N% related" (100 minus the Composer's distance in percent). When no chain of up to
four steps turns this motif into the other, the panel says "Not related within 4 steps" and offers no link. "Link as a
form of this motif" makes the picked motif a form of this one; the variant tree then shows it like a generated form
(a linked form has no seed, so it has no Reroll).

- `POST /api/materials/recognize` `{"source":{"item","version"},"target":{"item","version"}}` calls
  `composer_api_recognize` on both versions' degrees, with each motif's harmony and features, and answers its JSON (`found`,
  `chain`, `distance`, `remoteness`, `kept`, `changed`, `derivation`) plus `ok`. The two motifs need the same form,
  meter and key. `400` for a body that is not a motif, has no degrees (not analyzed) or does not match in form, meter
  and key, `404` for a version that is not there. The search is the Composer's beam search, not exhaustive.
- `POST /api/materials/items/<id>/link-form` `{"source":{"item","version"},"recognition"}` writes a new version of the
  item (the target) whose `derived` is `{source_item, source_version, operations, seed: null, kind, kept, changed,
  harmony, functions, remoteness, flag: "unflagged", standalone: true}` from the recognition: `kind` is `exact` when every step is
  `exact`, otherwise `motive-form`; `harmony` is the target's own chords; `remoteness` is the recognition's. The note
  is `linked: <derivation>`. Answers `{"item"}`. `400` when the recognition found no chain, `404` for a source
  version that is not there, `409` when the item is the source or one of its ancestors.

#### Motive-forms: harmony, functions and remoteness

Every Composer call for a motif (`motive` on save, `variants`, render and
`random-motive`) passes the motif's `harmony`, one `features` entry `label:first-last`
per feature (at most 8) and `role` from the body (as options of `variants`, which still runs the command). When the source of
`POST /api/materials/items/<id>/variants` is itself derived, the route follows
`derived.source_item` to the root of the tree (at most 32 steps) and passes it
as `--basic` with `--basic-format`, `--basic-pickup` and `--basic-harmony`, so
remoteness is measured from the basic motive, not from the parent.

- `POST .../variants` also takes `functions` (the vocabulary's, up to the
  Composer's limit of four), sent as `--function`; only
  forms that suit one of them come back (the Composer fails when none does).
  The generate panel has a checkbox for each, labelled `Only variants that
  suit`.
- `POST .../variants` also takes `distance` (`close`, `medium` or `far`), sent
  as `--distance` (R-189): `close` draws 1-step chains with remoteness at most
  a third, `medium` 2 steps up to two thirds, `far` 3 or 4 steps above that.
  It is refused with a `chain` (400 `distance takes random operations`) and
  `400` with the Composer's message and `path` for any other value. The Generate panel has a `Distance` select (Any,
  Close, Medium, Far; default Any), switched off while operations are chosen.
  When the distance leaves fewer variants than the count, the status says
  `<n> of <count> found at this distance.`
- A candidate carries `functions`, `remoteness` (0 to 1) and, when it has chords
  of its own, `harmony`. Cards and tree rows show "N% remote
  from the basic motive", a `Suits: beginning, cadential` line (`Suits: none`
  without functions) and a `Harmony:` line. A `Suits` select (Any, Beginning,
  Continuation, Contrast, Cadential) above the candidates filters the
  candidates and the saved variants in the page; it does not change what is
  generated or stored. Under it: "Where in a phrase a form fits, worked out
  from its harmony, rhythm and length (Schoenberg). Theme slots use it to pick
  forms."
- Saving a candidate (and rerolling a stored form) writes `motif.harmony`,
  `motif.features` (the labelled runs the form kept) and `derived.harmony`,
  `derived.functions` and `derived.remoteness`.
- The operation picker offers the motif's feature labels for the `feature`
  parameter of `repeat-feature` and `shift-feature`.
- `POST /api/materials/motifs/random` also takes `role`.

#### Random motifs

The New Motif page has a Random section with one button and no fields. Random draws one motif: the seed is drawn
and sent with `mode` and `meter` set to `random`; the Composer draws the mode (major or minor) and the meter (`4/4`,
`3/4`, `2/4` or `6/8`) from the seed and answers them as `drawn`, form full, no key (the Composer writes C major or A
minor), no style. Its score is shown at full width in the score workspace with playback (nothing stored), with
Reroll (the next seed, which draws its own mode and meter) and Use, which fills the editor above (notes, mode, meter,
pickup) to name and save; nothing is stored until Save. The backend fills in the analysis and degrees as for any motif.

- `POST /api/materials/motifs/random` `{mode, meter, key?, form?, role?, style_id?, count?, seed?}` calls the Composer's
  `composer_api_random_motive` and answers its JSON (`seed`, `motives`, and `drawn` when `mode` or `meter` was `random`) plus
  `ok`. `mode` is `major`, `minor` or `random`, `meter` is like `3/4` or `random`, `key` is like `D minor`, `count` is 1 to
  16, `seed` a whole number from 0 (the Composer holds its upper limit). A bad input or an unknown style is 400. The style model path comes from `ursatz_gui_style_model_path`.
- Pure helpers in `materials-model.js`: `randomRequest` (one motif), `drawnSettings` (the mode and meter the answer
  names), `randomCandidateToBody`,
  `candidateKeyText`, `rerollSeed`.

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
