# Library and navigation views

The Library, Tags panel and the other top-level views of the GUI. [Docs index](../README.md).

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
- **Collections**: named groups of piece ids (the [Corpus routes](../api/analysis-routes.md#corpus-routes)).
- **Subjects**: named characters, places, ideas and notes that items and
  Library pieces link to (see [Subjects views](analysis-and-score.md#subjects-views)).
- **Motifs**, **Themes**, **Pieces**: the items of the material store (see
  [Materials views](motifs.md#materials-views)).
- **Motive links**: the links between motives of pieces and items (see [Motive links view](analysis-and-score.md#motive-links-view)).
- **Health**: last entry in the sidebar. Shows the backend health check; a
  failed check appears only here.

The GUI opens on the Library.
