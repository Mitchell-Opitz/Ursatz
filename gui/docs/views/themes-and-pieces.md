# Theme and piece views

The Themes view, the Pieces view and item references in the GUI. [Docs index](../README.md).

## Themes view

A theme's detail is a page in four tabs at the full width (`theme-detail-view.js`,
`ThemeDetailView`; the editor is in `themes-view.js`, the helpers in
`themes-model.js`, global `ThemesModel`). The open tab is remembered. It edits
the theme body the backend composes (see [Theme bodies and version files](../storage.md#theme-bodies-and-version-files)).

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

## Pieces view

A piece's detail holds the piece editor (`pieces-view.js`; the helpers are in
`pieces-model.js`, global `PiecesModel`). It edits the piece body the backend
composes (see [Piece bodies and version files](../storage.md#piece-bodies-and-version-files)) and reuses the Themes view's
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

## Item references in the GUI

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
