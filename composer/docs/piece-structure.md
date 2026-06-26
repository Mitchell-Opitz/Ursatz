# Piece structures

A piece structure (`<ursatz_composer/piece_structure.h>`, R-169) is a game track as data: an intro, a body, a turnaround
and an ending, each a list of sections with a length in beats, a pickup, a cadence and a fill. The body and the
turnaround are the loop: the intro is played once before it and the ending once after it. A structure is not a form
template: the sections are any number of beats long, not only 4 or 8 whole bars ([piece.md](piece.md) has the templates,
which are unchanged).

```c
PieceSection sections[3] = {
    {.id = "a", .letter = "A", .part = PIECE_PART_BODY, .beats = 32, .fill = PIECE_FILL_MOTIF},
    {.id = "b", .letter = "B", .part = PIECE_PART_BODY, .beats = 6, .fill = PIECE_FILL_EMPTY},
    {.id = "t", .letter = "T", .part = PIECE_PART_TURNAROUND, .beats = 4, .fill = PIECE_FILL_EMPTY},
};
PieceStructure structure = {.key = key, .meter = meter, .sections = sections, .section_count = 3};
PiecePlan *plan;
ComposeConfig config = {.seed = 7};
ComposeOutcome outcome;
if (piece_structure_build_plan(&structure, &plan) == PIECE_OK && compose_run(plan, &config, &outcome) == PIECE_OK) {
    /* every section is written; outcome.sections[i].id and .part say which section each entry is */
}
```

## The structure

| Field | Meaning |
| --- | --- |
| `key`, `meter` | The key and the meter of the whole piece. 2/4, 3/4, 4/4 and 6/8, as the harmony operators support. |
| `sections` | In time order, grouped by part: intro, body, turnaround, ending. |
| `motive` | The piece's one basic motive (R-170), or NULL: then the motive of the first motif section in time order, and when no section has one, a motive invented (`INVENT_MOTIVE` on `m0`). |
| `has_accompaniment`, `accompaniment` | An accompaniment under every section, as in [grow.md](grow.md). |
| `has_style`, `style` | A style, as in [style.md](style.md). |

A section is:

| Field | Meaning |
| --- | --- |
| `id`, `letter` | The section's id (unique, 1 to 15 characters) and its form letter (1 to 3). |
| `part` | `PIECE_PART_INTRO`, `_BODY`, `_TURNAROUND` or `_ENDING`. |
| `beats` | The length in beats of the meter: any whole number, 1 or more. |
| `pickup` | The anacrusis before the section: a multiple of 1/32 of a whole note and shorter than a bar; 0 for none. It is the section's own, not part of `beats`. |
| `has_cadence`, `cadence` | The cadence the section ends in; without one `piece_structure_default_cadences` decides. |
| `fill` | `PIECE_FILL_EMPTY`, `_MOTIF`, `_THEME` or `_PROGRESSION`. Theme and progression fills are not planned yet: `piece_structure_build_plan` refuses them. |
| `motive`, `has_into`, `into`, `hybrid` | A motif section's motive (read as the piece's basic motive when it is the first), and the archetype a theme section is built as (`into`: sentence by default; a hybrid names its pairing in `hybrid`). |

`piece_structure_check` returns `PIECE_ERR_BAD_REQUEST` for a structure with no body section, parts out of order, ids that
repeat, more than 32 sections or 512 bars, a length under one beat, a pickup that is not a multiple of 1/32 or not
shorter than a bar, or a meter or key that is not supported.

## Defaults

`piece_structure_default_cadences` gives a section without a cadence one (R-169): the last section of the loop (the
turnaround, else the body) and the last of the intro lead back, an HC; the last section of the ending closes, a PAC;
every other is an IAC. `piece_structure_default_ending` adds one empty ending section of 2 bars (id `end`, letter `E`,
PAC) when the structure has none.

## The plan

`piece_structure_build_plan` makes a Form `structure` with the meter and the key Given over everything, one Section for
each section of the intro, the body and the turnaround, and the material section. The ending is not laid out here: the
ending version is built from the realized loop (R-172).

- Each section starts where the one before ends and is `beats` long. It has `section_letter`, `cadence`, `key`,
  `piece_part` (its part and id) and, with a pickup, `pickup`. The pickup is a node constraint: the melody operator reads
  it for a melody span of a whole phrase; the ideas of a theme or a free phrase do not take a pickup yet (see
  [melody-fill.md](melody-fill.md#pickups)).
- A motif or empty section of 4 or 8 whole bars that starts on a barline, has no pickup and is a theme the meter and the
  cadence allow is a theme section: it has the `archetype` and `theme_pairing` of its `into`, and an antecedent's second
  `cadence` (HC), and `grow-theme` builds it as [grow.md](grow.md) says.
- Any other section is a free phrase: it has `free_phrase`, and `grow-theme` builds it as ideas of a bar
  ([grow.md](grow.md#free-phrases)).
- The material section starts on the first barline at or after the end of the last section laid out, since the motives in
  it are bars of the meter. `m0` is the basic motive, Given, or to be invented. An empty section is made from it as a
  motif section is: the same basic motive, with no motive of its own.

`compose_run` fills the plan with its stages unchanged: a free phrase is a structure-stage target like a theme. Its
outcome has one entry for each section, in time order, with the section's `id` and `part`. A free phrase is not heard as
a form, so only its structure's score counts.

## What does not compose

- A free phrase that ends on an odd beat of a 4/4 bar (5, 7, 9 beats long after a section that ends on a barline) can be
  planned and its chords written, but the melody leaves it open: the melody stage is stuck. Free phrases of 6 beats and
  longer that end on an even beat composed for all of seeds 1 to 10.
- A free phrase of whole bars needs the room the cadence's chords need: a bar alone is too short for any cadence; the
  structure stays open at the structure stage.
