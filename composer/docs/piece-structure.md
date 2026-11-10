# Piece structures: the game loop

The game loop (`forms/game-loop.json`, R-169, R-419) is a game track as data: an intro, a body, a turnaround and an ending,
each a list of sections with a length in beats, a pickup, a cadence and a fill. The body and the turnaround are the loop:
the intro is played once before it and the ending once after it. It is a form like the others ([forms.md](forms.md)): its
document has `"sections": "request"`, so the piece request gives the sections (`PieceRequest.sections`), and it has the
`roles`, `loop`, `cadences` and `default_ending` the document table there describes. Its plan's form node is named
`structure` (`plan_name`). The sections are any number of beats long, not only 4 or 8 whole bars, which the forms that come
in sizes still need ([piece.md](piece.md) has them). The same builder, `form_build_plan`, makes the plan of the game loop
and of every other form; there is no separate game loop builder. Back to the [docs index](README.md).

```c
PieceSection sections[3] = {
    {.id = "a", .letter = "A", .part = PIECE_PART_BODY, .beats = 32, .fill = PIECE_FILL_MOTIF},
    {.id = "b", .letter = "B", .part = PIECE_PART_BODY, .beats = 6, .fill = PIECE_FILL_EMPTY},
    {.id = "t", .letter = "T", .part = PIECE_PART_TURNAROUND, .beats = 4, .fill = PIECE_FILL_EMPTY},
};
PieceRequest request = {.form = "game-loop", .key = key, .meter = meter, .sections = sections, .section_count = 3};
PiecePlan *plan;
ComposeConfig config = {.seed = 7};
ComposeOutcome outcome;
if (piece_build_plan(&request, &plan) == PIECE_OK && compose_run(plan, &config, &outcome) == PIECE_OK) {
    /* every section is written; outcome.sections[i].id and .part say which section each entry is */
}
```

## The request

| Field | Meaning |
| --- | --- |
| `form` | `"game-loop"`. |
| `key`, `meter` | The key and the meter of the whole piece. 2/4, 3/4, 4/4 and 6/8, as the harmony operators support. |
| `sections`, `section_count` | In time order, grouped by part: intro, body, turnaround, ending. |
| `motive` | The piece's one basic motive (R-170), or NULL: then the motive of the first motif section in time order, and when no section has one, a motive invented (`INVENT_MOTIVE` on `m0`). |
| `has_accompaniment`, `accompaniment` | An accompaniment under every section, as in [grow.md](grow.md). |
| `has_voicing`, `voicing` | A four-part voicing on every section, as an accompaniment is; not both. |
| `has_style`, `style` | A style, as in [style.md](style.md). |

The game loop ignores `bars`, `count`, `modulate`, `source` and `given_melodies`: it never modulates, and `piece_plan_keys`
is not called for it.

A section is:

| Field | Meaning |
| --- | --- |
| `id`, `letter` | The section's id (unique, 1 to 15 characters) and its form letter (1 to 3). |
| `part` | `PIECE_PART_INTRO`, `_BODY`, `_TURNAROUND` or `_ENDING`: the form's role of that name. |
| `beats` | The length in beats of the meter: any whole number, 1 or more. |
| `pickup` | The anacrusis before the section: a multiple of 1/32 of a whole note and shorter than a bar; 0 for none. It is the section's own, not part of `beats`. |
| `has_cadence`, `cadence` | The cadence the section ends in; without one `piece_default_cadences` decides. |
| `fill` | `PIECE_FILL_EMPTY`, `_MOTIF`, `_THEME` or `_PROGRESSION` (see [Fills](#fills)). |
| `motive`, `has_into`, `into`, `hybrid` | A motif section's motive (read as the piece's basic motive when it is the first), and the archetype a theme section is built as (`into`: sentence by default; a hybrid names its pairing in `hybrid`). |
| `theme_plan` | A theme fill: the composed theme (the plan the `theme` command writes) whose first theme section is given to this one. It must outlive the build. |
| `numerals`, `numeral_count`, `cells_per_chord` | A progression fill: the numerals, one chord each, and the harmony cells each lasts (0 for a bar). They must outlive the build. |

`piece_request_check` returns `PIECE_ERR_BAD_REQUEST` for a form that is not a built-in whose sections the request gives, a
request with no body section, parts out of order, ids that repeat, more than 32 sections or 512 bars, a length under one
beat, a pickup that is not a multiple of 1/32 or not shorter than a bar, a meter or key that is not supported, or an
accompaniment and a voicing together.

## Fills

A section's fill says where its material comes from (R-170).

- **empty** and **motif**: grown from the piece's basic motive. A motif section's own motive is the basic motive when it
  is the first.
- **theme**: a composed theme used as it was composed. The first theme section of `theme_plan` is given to the section
  with `plan_give_section` ([middle-out.md](middle-out.md#a-section-from-another-plan)): its melody, harmony, bass and
  inner voice spans are Given in the section, moved to its start and transposed by the key difference (D minor to E minor
  is up a whole tone), and its phrase and idea nodes are copied under the section so that `form_hear` hears the theme it
  was. The section is Realized when the plan is built, so no stage targets it and `compose_run` leaves it as it is; change
  a theme in the Themes view, not here. The section takes the theme's archetype, pairing and cadences, not the
  request's, and the section's `cadence` for it is not used. The theme must be as long as the section, in the same
  meter and in the same mode; otherwise `piece_build_plan` is `PIECE_ERR_BAD_REQUEST`. The theme's `m0`, in the
  request's key, is the piece's basic motive when no `motive` and no earlier motif section has one. The theme's
  references to its motive nodes are not kept.
- **progression**: the numerals are Given from the section's first beat (`plan_give_progression`), `cells_per_chord`
  cells each (a cell is a beat or half a bar, a bar when 0), and the harmony stage writes what is left of the section; the
  melody grows from the basic motive over the chords as it does in an empty section. A progression that is too long for the
  section, or has a numeral that cannot be read, is `PIECE_ERR_BAD_REQUEST`.

## Defaults

`piece_default_cadences(form, request)` gives a section without a cadence the one the document's `cadences` say (R-169): the
last section of the loop (the turnaround, else the body) and the last of the intro lead back, an HC; the last section of
the ending closes, a PAC; every other is an IAC. `piece_default_ending(form, request, capacity)` adds the document's
`default_ending`, one empty ending section of 2 bars (id `end`, letter `E`, PAC), when the request has none.

## The plan

`form_build_plan` makes a Form `structure` with the meter and the key Given over everything, one Section for each section
of the intro, the body and the turnaround, and the material section. The ending is not laid out here: the ending version
is built from the realized loop (R-172).

- Each section starts where the one before ends and is `beats` long. It has `section_letter`, `cadence`, `key`,
  `piece_part` (its part and id) and, with a pickup, `pickup`. The pickup is a node constraint: the melody operator reads
  it for a melody span of a whole phrase; the ideas of a theme or a free phrase do not take a pickup yet (see
  [melody-fill.md](melody-fill.md#pickups)).
- A motif or empty section of 4 or 8 whole bars that starts on a barline, has no pickup and is a theme the meter and the
  cadence allow is a theme section: it has the `archetype` and `theme_pairing` of its `into`, and an antecedent's second
  `cadence` (HC), and `grow-theme` builds it as [grow.md](grow.md) says.
- Any other section is a free phrase: it has `free_phrase`, and `grow-theme` builds it as ideas of a bar
  ([grow-harmony.md](grow-harmony.md#free-phrases)).
- The loop's end (the last section of the turnaround, else the last body section) is held: it has `hold`, and the seam
  stage fills it last against the seam, the section `seam` (one bar, `seam`) that follows it, a Given copy of the body's
  first bar. A theme fill, a progression fill that covers its section and the first body section are not held. See
  [loop-seam.md](loop-seam.md).
- The material section starts on the first barline at or after the end of the seam, since the motives in
  it are bars of the meter. `m0` is the basic motive, Given, or to be invented. An empty section is made from it as a
  motif section is: the same basic motive, with no motive of its own.

`compose_run` fills the plan with its stages, then the seam stage ([loop-seam.md](loop-seam.md)): a free phrase is a
structure-stage target like a theme. Its outcome has one entry for each section, in time order, with the section's `id` and
`part`. A free phrase is not heard as a form, so only its structure's score counts.

`piece_build_ending(loop_plan, request, &ending)` is the ending version (R-172): the intro and the body Given from the
realized loop, the turnaround and the seam left out, the ending appended open and composed from seed + 20. It is
`form_build_plan`'s own layout with a loop plan, not another builder. The helpers a regenerated piece needs
(`piece_section_find`, `piece_held_sections`, `piece_section_holds_body_start`, `piece_seam_reset`, `piece_reopen_lane`) are
in `<ursatz_composer/piece_reopen.h>`.

## What does not compose

- A progression fill does not move the section's cadence. A progression that ends on V in a section whose default cadence
  is an IAC (any section that is not the last of the loop) leaves the melody stage stuck: give the section a half cadence
  (`has_cadence`, `PLAN_CADENCE_HC`). A progression of 4 bars that ends on V, followed by 4 open bars in the same 8-bar
  section that must end in a half cadence, leaves the harmony stage stuck; `I IV V I`, `I IV I` and `I` composed for all of
  seeds 1 to 10.
- A free phrase that ends on an odd beat of a 4/4 bar (5, 7, 9 beats long after a section that ends on a barline) can be
  planned and its chords written, but the melody leaves it open: the melody stage is stuck. Free phrases of 6 beats and
  longer that end on an even beat composed for all of seeds 1 to 10.
- A free phrase that ends in a half cadence in a minor key composes (P44 B6): its chords are capped so the two before
  the cadence's dominant fill the room (`test_piece_structure minor_half_free`, a game loop in D minor 4/4 with a body of
  32 beats and a free phrase of 20 beats). A game loop in A minor 3/4 whose ending version is stuck at the structure
  stage (the default two-bar ending) is unchanged.
- A free phrase of whole bars needs the room the cadence's chords need: a bar alone is too short for any cadence; the
  structure stays open at the structure stage.
- A one-bar turnaround, or any section of one bar on a barline, does not compose either (the structure stage leaves it open);
  a turnaround of two bars does. The `piece` command ([cli-piece.md](cli-piece.md)) reports the stage and tells you.

## The piece command

`ursatz-composer piece` builds the request, composes the loop, and builds the ending version from it
(`piece_build_ending`). See [cli-piece.md](cli-piece.md) and [piece-request.md](piece-request.md). The API reads the piece
request file into a `PieceRequest` of the form `game-loop` and builds through `piece_build_plan` and `piece_build_ending`;
requests, responses and errors are as they were.
