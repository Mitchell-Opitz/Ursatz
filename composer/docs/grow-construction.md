# Theme construction

A theme need not be 4 or 8 bars with ideas of fixed fractions of it (Schoenberg ch. XIV, R-407, R-468). A construction gives the bars of each idea. Back to [grow.md](grow.md).

## The request

`GrowRequest.construction` (`const PlanConstruction *`, NULL for none) is added to the theme section as a `construction` constraint ([plan-format.md](plan-format.md)). `idea_bars` is indexed by `GrowIdea`: an idea the pairing has needs 1 bar or more, an idea it does not have is 0. `bars` is the sum, 3 to 16; without a construction it is 4 or 8. The reason is `given` for a request, `motive` or `length` when Composer chooses a construction (B4).

## The layout

`grow_layout_of` lays the ideas end to end in place order inside each half, each as long as its bars; the beginning and the ending are the sums of their halves, and `basic_bars` is the basic idea's. Everything that read a half of the theme as `bars / 2`, or an idea's room as eighths of the theme, reads this layout: the room for each cadence, the antecedent's last bar for given chords, the rooms of idea and slot motives, and whether a motive as long as the first part fills it whole.

## Refusals

`/bars` outside 3 to 16 with a construction, other than 4 or 8 without one. `/construction/ideas`: the ideas do not add up to the bars (`the ideas are 6 bars; the theme has 7`), the pairing has no such idea (`the antecedent+consequent has no fragment 1`), an idea has no bar, or, in a continuation, fragment 1 or fragment 2 has as many bars as the basic idea or more (`the fragment 1 needs fewer bars than the basic idea`: standard-form's fragmentation rule needs the fragment shorter, so a 1-bar basic idea has no whole-bar fragment). A construction with an extension is refused until B3.

## What changes with a construction

- The basic idea's length rule of standard-form (1, 2 or 4 bars) only lowers the score, so a theme with a 3-bar basic idea can compose.
- A contrasting idea or consequent contrasting idea shorter than the motive it is made from is made from the leading notes of that motive that fit its bars (`grow_shorten_source`). The shortened motive is a Realized node of the material, `shortened for m<place>`, and the idea's motive node refers to it.
- A continuation of fragments of a bar or more changes chord every cell when `faster_harmony` is set.
- The label names the halves, `sentence 4+5 bars ...`.

A request without a construction is built exactly as before. Measured in C major 4/4 and A minor 3/4, seeds 1 to 10 (`test_grow_uneven`): a 3+3, 5+5 and 4+6 period and a 4+5, 5+4 and 6+5 sentence each compose 10 of 10.
