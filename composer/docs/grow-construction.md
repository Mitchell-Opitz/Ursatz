# Theme construction

A theme need not be 4 or 8 bars with ideas of fixed fractions of it (Schoenberg ch. XIV, R-407, R-468). A construction gives the bars of each idea, and up to 3 extensions. Back to [grow.md](grow.md).

## The request

`GrowRequest.construction` (`const PlanConstruction *`, NULL for none) is added to the theme section as a `construction` constraint ([plan-format.md](plan-format.md)). `idea_bars` is indexed by `GrowIdea`: an idea the pairing has needs 1 bar or more, an idea it does not have is 0. `bars` is the sum, 3 to 16; without a construction it is 4 or 8. The reason is `given` for a request, `motive` or `length` when Composer chooses a construction (B4).

## The layout

`grow_layout_of` lays the ideas end to end in place order inside each half, each as long as its bars; the beginning and the ending are the sums of their halves, and `basic_bars` is the basic idea's. Everything that read a half of the theme as `bars / 2`, or an idea's room as eighths of the theme, reads this layout: the room for each cadence, the antecedent's last bar for given chords, the rooms of idea and slot motives, and whether a motive as long as the first part fills it whole.

## Refusals

`/bars` outside 3 to 16 with a construction, other than 4 or 8 without one. `/construction/ideas`: the ideas do not add up to the bars (`the ideas are 6 bars; the theme has 7`), the pairing has no such idea (`the antecedent+consequent has no fragment 1`), an idea has no bar, or, in a continuation, fragment 1 or fragment 2 has as many bars as the basic idea or more (`the fragment 1 needs fewer bars than the basic idea`: standard-form's fragmentation rule needs the fragment shorter, so a 1-bar basic idea has no whole-bar fragment). A construction with extensions is refused at `/construction/extensions/<k>` when an extension cannot follow its idea, and its extension bars count in the sum (see Extensions).

## What changes with a construction

- The basic idea's length rule of standard-form (1, 2 or 4 bars) only lowers the score, so a theme with a 3-bar basic idea can compose.
- A contrasting idea or consequent contrasting idea shorter than the motive it is made from is made from the leading notes of that motive that fit its bars (`grow_shorten_source`). The shortened motive is a Realized node of the material, `shortened for m<place>`, and the idea's motive node refers to it.
- A continuation of fragments of a bar or more changes chord every cell when `faster_harmony` is set.
- The label names the halves, `sentence 4+5 bars ...`.

A request without a construction is built exactly as before. Measured in C major 4/4 and A minor 3/4, seeds 1 to 10 (`test_grow_uneven`): a 3+3, 5+5 and 4+6 period and a 4+5, 5+4 and 6+5 sentence each compose 10 of 10.

## Extensions

A construction has up to 3 extensions (Schoenberg ch. XIV, R-468), applied in the order listed. Each is `{kind, idea, bars, varied}`; `bars` 0 means the idea's own bars (1 for a stretch and an addition). `grow_layout_of` lays them out: a repeat shifts every later idea of its half and the halves after it, a stretch lengthens its idea and shifts the rest, the idea after an evaded cadence follows the ending's last idea (inside the ending), and an addition follows the ending (outside it). The bars of all of them are in the theme's `bars`.

| Kind | What it makes | Allowed |
|---|---|---|
| `repeat` (internal repetition) | A new idea right after idea `idea`, named `<idea> repeated`, in the same half and container, with its own motive node `extension <k>`. `varied` false: the repeated idea's motive in sequence one step down the key's scale. `varied` true: its last interval widened by 2 semitones with the contour kept (else narrowed by 2), whichever differs from the idea. In a presentation it has a harmony target of the repetition's shape, elsewhere the container's chords are spread over it. | After the repetition, the contrasting idea, fragment 1, fragment 2, the consequent contrasting idea and cadential 1. |
| `stretch` (stretched cadence) | The idea is `bars` bars longer; no new node, the motive is grown to the span. | The cadential idea, the consequent contrasting idea, cadential 2, and an antecedent's contrasting idea. |
| `addition` (after the cadence) | A phrase `addition` after the theme's phrase with an idea `addition idea`: a tonic prolonged from tonic to tonic, no cadence, its melody the longest trailing segment (2 notes or more) of the theme's last idea that fits and ends on the cadence's tone (motive node `extension <k>`). | After a PAC or an IAC; 1 or 2 bars; one. |
| `evaded` (evaded cadence) | The theme's last idea first arrives on a deceptive cadence (V to vi), then a new idea `<idea> after the evaded cadence` restates its motive exactly and takes the theme's cadence. The ending's chords are split: the container's end at the deceptive cadence, then the new idea's own cadential ones. | After a PAC or an IAC; one. |

The melody of a deceptive cadence ends on the tonic or the third, both tones of vi (`melody_cadence_mask`). With `faster_harmony` the split applies to the last idea's own target, and the setting is off for a cadential idea or an evaded idea that has more bars than a progression of a chord a cell can fill.

Refusals, at `/construction/extensions/<k>`: `a repeat cannot follow the basic idea` (also `a stretch cannot follow ...`), `the presentation+continuation has no contrasting idea` (an idea the pairing does not have), `an HC cannot be evaded`, `an addition follows a PAC or an IAC`, `a theme has one evaded cadence` and `a theme has one addition`, `an addition is 2 bars at most`; at `/bars`, an idea after an evaded cadence with no room for the cadence. The made motives of a repeat and an addition are nodes of the material (`extension <k>`, `k` the extension's place in the construction from 1). The soft rules of the checker are asked on the construction's ideas without the extensions (`idea_beginning_bars`, `idea_ending_bars`), so an extension never lowers the score; the label names the halves and the extensions, `sentence 4+6 bars evaded cadence ...`, `antecedent + consequent 4+6 bars repeated consequent contrasting idea ...`.

Measured in C major 4/4 and A minor 3/4, seeds 1 to 10 (`test_grow_extensions`): an 8-bar sentence with a repeat of fragment 2 (9 bars), a stretched cadential idea (9), an addition of 2 bars (10) or an evaded cadence (10), and a 4+6 period by a varied repeat of the consequent contrasting idea each compose 10 of 10. A theme without extensions is built exactly as before.
