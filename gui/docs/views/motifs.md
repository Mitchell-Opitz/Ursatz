# Motif views

The Materials views and the Motif editor. [Docs index](../README.md).

## Materials views

The project picker sits at the top of the sidebar: All projects plus each
project, with New, Rename and Delete (Rename and Delete need a project
picked; deleting a project that still holds items shows the backend's 409
message). The choice filters the Motifs, Themes and Pieces views and is
remembered in the browser. Each view lists its items (name, project, current
version, updated) with a New row (name and project). A row opens the detail:
the name and project are editable, the uses are listed, and the versions list
(number, date, note) lets any version be viewed read-only. Themes and pieces
have body editors ([Themes view](themes-and-pieces.md#themes-view) and [Pieces view](themes-and-pieces.md#pieces-view)). Subjects and item references are described in [Subjects views](analysis-and-score.md#subjects-views) and [Item references in the GUI](themes-and-pieces.md#item-references-in-the-gui).

## Motif editor

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
(it sends the note `Created`) and a Random section (below). The form has the role (melodic or accompaniment), the form (full,
rhythm or contour), the note
list with its syntax hint, the meter, a Mode select (major or minor), the pickup in exact
beats (`0`, `1` or `1/2`), an optional Base harmony field and a Segments table
(named note ranges such as head and tail: label of 1 to 8 characters, first
and last note counted from 1; rows are added and removed in place; Liquidation
and the motive-forms keep or drop them, and the variant picker calls the
`feature` parameter `segment`. The stored field stays `features`). Segments are always present: when none are set the
Composer proposes them from the rhythm, the rows carry a Proposed tag, and editing, adding or removing a row makes them
the motif's own (the tag goes). Use proposal clears the rows and marks them proposed, so the save takes the Composer's
proposal; changed notes, pickup, meter or form propose again while the segments are still proposed. Existing motifs
without segments were given a proposal once, in a version noted `Segments proposed`. The Random section has one Random button (no Reroll): each press draws another motif built from segments, and Use fills the editor with its notes, base harmony and segments (marked proposed). Base harmony is the motif's own chords as the Composer
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

Export sits at the top of the motif page. It fetches `GET /api/materials/items/<id>/export?version=<n>` for the
current version once and then shows Copy and Download .txt (the file is the motif name as a slug, `.txt`). The text is
the Analyzer's: the motif with its segments marked on the notes and a section for each of its variants, ready to paste
into a conversation. An error shows beside the buttons.
