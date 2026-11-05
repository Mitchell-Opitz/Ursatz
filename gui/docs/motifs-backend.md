# Motifs in the backend

How the backend analyzes, varies, compares and links motifs. [Docs index](README.md).

## Motif analysis on save

`POST /api/materials/items` and `POST /api/materials/items/<id>/versions`
analyze a motif (`form` `full`, `rhythm` or `contour`) before it is written. The route builds the
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
- `rhythm` and `contour` motifs are analyzed on save like full ones: `motif.analysis` holds the Composer's result and
  `motif.degrees` the rhythm or contour list. The stored `pickup` is passed to the Composer.

## Variant tree

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

## Comparing and linking motifs

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

## Motive-forms: harmony, functions and remoteness

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

## Random motifs

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
