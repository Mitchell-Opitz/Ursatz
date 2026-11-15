# Theme construction

A theme need not be 4 or 8 bars with ideas of fixed fractions of it (Schoenberg ch. XIV, R-407, R-468). A construction gives the bars of each idea, and up to 3 extensions. Back to [grow.md](grow.md).

## The request

`GrowRequest.construction` (`const PlanConstruction *`, NULL for none) is added to the theme section as a `construction` constraint ([plan-format.md](plan-format.md)). `idea_bars` is indexed by `GrowIdea`: an idea the pairing has needs 1 bar or more, an idea it does not have is 0. `bars` is the sum, 3 to 16; without a construction it is 4 or 8. The reason is `given` for a request, `motive` or `length` when Composer chooses it (see Choosing a construction).

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

## Choosing a construction

A request with `bars` other than 4 or 8 and no `construction` gets the one `grow_construction_choose` gives (R-469), by rule and never by a draw; `grow_build_plan` refuses it (`/bars`, `no construction makes a 10-bar period from this motif`) when there is none. 4 and 8 bars with a motif of 2 bars or fewer get none and are built as ever. A construction the caller gives keeps reason `given`.

1. The basic idea `b` is the motif's bars when that is 3 or more (reason `motive`), else 2 from 6 bars up, else 1 (reason `length`); a motif longer than `b` is refused.
2. The beginning `h1` is 2b for a presentation (basic idea and repetition of `b` each). An antecedent or compound basic idea has 2b when that leaves the ending at least as long, else half the theme; its second idea is `h1 - b` bars.
3. The ending is `h2 = bars - h1`. A consequent is the basic idea restated and a contrasting idea of `h1 - b`; its excess over `h1` is a stretch of the contrasting idea for 1 bar, a varied repeat of it (`h1 - b` bars) and a stretch of the rest for more (at most 4). A continuation needs `b` of 2 or more: fragments of `b / 2` (at least 1) and a cadential idea of `b`; its excess over that is 1 bar a sequential repeat of fragment 2 (a stretch of the cadential idea when the fragments are longer than a bar), 2 an evaded cadence (`b` bars), 3 both, 4 an evaded cadence and an addition of 2 bars. A cadential ending is cadential 1 and 2 of `b` bars; an excess of 1 stretches cadential 2, 2 evades the cadence. A negative excess, an evaded cadence or addition on an HC, or a construction `grow_construction_buildable` refuses gives none.

So 6 bars make a 3+3 period (2,1 | 2,1), 10 bars a 4+6 period (2,2 | 2,2 and a varied repeat of 2) or a 4+6 sentence (2,2 | 1,1,2 and an evaded cadence of 2), 9 bars a 4+5 sentence (a sequential repeat of fragment 2), and a 3-bar motif a 5+5 period in 10 bars or a 6+6 period in 12.

`grow_construction_describe` writes the reason as one sentence, bars counted from 1: `4+5 sentence: the section is 9 bars, so fragment 2 is repeated a step lower (bar 7).`, `5+5 period: the motif is 3 bars, so the basic idea is 3 bars and each phrase 5.`, `4+6 sentence ... as given` for a given construction with its extensions named. `GrowOutcome.construction_text` and `ComposeHeard.construction_text` (192 characters) carry it for the theme section, `""` without a construction. Tested by `test_grow_construction`.

## Measured through the API

With the `theme` command (API 1.18), one theme from each start seed 1 to 20 (a seed counts when the theme comes from that seed, not from a later one), a motif of 1 bar (`1:q 3:e 5:e 1':h`, `1:q 3:e 5:e 1':q` in 3/4) or of 3 bars, no construction given, so each theme's is the chosen one (`reason` `length` or `motive`):

| Request | C major 4/4 | A minor 3/4 |
| --- | --- | --- |
| 6-bar period (3+3) | 20 of 20 | 20 of 20 |
| 9-bar sentence (4+5, a repeat of fragment 2) | 20 of 20 | 20 of 20 |
| 10-bar period (4+6, a varied repeat) | 20 of 20 | 20 of 20 |
| 10-bar sentence (4+6, an evaded cadence) | 20 of 20 | 20 of 20 |
| 10-bar period from a 3-bar motif (5+5) | 20 of 20 | 20 of 20 |
| 12-bar period from a 3-bar motif (6+6) | 20 of 20 | 20 of 20 |

The three requests of `test_api_theme` (`uneven-period.json`, `uneven-sentence.json`, `evaded-sentence.json`) are the 10-bar period, the 9-bar sentence and the 10-bar sentence rows; each composes at seed 1. The 4 and 8-bar rates of `test_grow_compose_rates` and of [grow.md](grow.md) are unchanged (rerun, not re-recorded). The measured rows live here because `grow.md` is at its size limit.
