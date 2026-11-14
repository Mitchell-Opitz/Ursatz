# The parts of larger forms

The section kinds a larger form is made of, after Schoenberg ch. XVIII: the transition, the subordinate theme, the retransition and the coda (R-503). Back to the [docs index](README.md); the form side is in [forms.md](forms.md), the constraint in [plan-constraints.md](plan-constraints.md).

## The four parts

A form document names a part as the role of a section (`transition`, `subordinate`, `retransition`, `coda`), and `form_build_plan` gives the section a `section_function` constraint with the function, `independent` and `lyric` (all three always written). Any form may use them. No built-in form does yet, so every output is unchanged.

| Part | In the form document | In the plan | Grown |
| --- | --- | --- | --- |
| Transition | A theme-like section: `beginning` `presentation` or `compound_basic_idea`, `ending` `continuation`, `cadence` `half`; 8 bars when it modulates (`keys.end` is not its start), else 4 or 8, or 3 to 16 with a `construction`. No `returns`, `variation`, `figure`, `mode` or `pedal`. `independent` (default false) gives it a motive of its own (R-504), so it has no `motive`. | `archetype`, `theme_pairing`, `section_function` (`transition`) | As a theme, with its continuation liquidated to the upbeat chord (below); B3 gives an independent one its own motive |
| Subordinate theme | Any pairing, cadence, keys and construction a theme-like section takes. `lyric` (default false): a period whose motive is m0 augmented (R-506); a lyric theme has no `motive` or `returns`, and the reader sets its motive to `augment` with an open ratio. | `archetype`, `theme_pairing`, `section_function` (`subordinate`), and for a lyric theme a `motive_ref` of `augment` | As a theme today; the open ratio is drawn as any section's motive is (the first usual ratio the meter writes, else an interval change) |
| Retransition | 4 bars, `cadence` `half`, nothing else: no `beginning` or `ending` and none of the members of a theme. | `section_function` (`retransition`), no `archetype` or `theme_pairing` | By its own operator in B4 (R-505); until then a plan with one stops at the structure stage |
| Coda | 8 bars, `cadence` `perfect-authentic`, nothing else. | `section_function` (`coda`), no `archetype` or `theme_pairing` | By its own operator in B5 (R-509); until then a plan with one stops at the structure stage |

A retransition and a coda count as theme sections in a compose run and are skipped when the form is heard (they have no theme shape to be heard as), and the style leaves their form alone, as it does a liquidation's.

## What is composed and what is not heard

The parts are composed to the plan and recorded in it; the hearing does not find them (R-408). The checks read the plan: the function of each section, its key and its cadence. B2 to B7 add what each part's section is made of and what was measured.

The test form is [parts-of-forms.json](../tests/fixtures/forms/parts-of-forms.json): 48 bars with the parts in the order of a sonata-like ternary (A, T, S, a codetta, R, A return, K). `test_form_spec` reads it and checks the plan it builds in C major 4/4; `test_plan_json` writes and reads the plan back byte for byte.

## The transition

A transition is a sentence ending in a half cadence (R-504). Grow-theme reads its `section_function` and sets `continuation.liquidate` for that section only, on the request copy: the shared context and the preferences are untouched (R-405), so no other section and no existing output changes. Its continuation then liquidates: `fragment 2` is a residue of a segment of m0 made by liquidation steps and the cadential idea goes at least as far ([grow-continuation.md](grow-continuation.md)).

A transition that modulates is 8 bars and its second half is in the new key (`keys.end`), so its half cadence is that key's V, the upbeat chord. The second half opens on a pivot. From a major key into its dominant the pivots are the new key's IV, ii and vi, all predominants, and a predominant pivot alone leaves too few chords before the V for four bars; the harmony stage then deals the predominant pivots going on to the new key's V7 and I, then the predominant and the V (R-512, [piece.md](piece.md#keys-and-cadences)). Into III of a minor key the pivots are tonics and need no second deal. Modulating in stages is two transition sections, home to a region and that region to another: data, no new code.

Only the liquidation is the transition's own: the pivot rule serves any half-cadence phrase that opens a key.

### What was measured

`tests/test_form_parts.c` case `transition` (`slow`, about 25 s): a 32-bar form built in the test (A theme 8 bars, PAC, closed; T transition 8 bars, HC, `keys.end` `subordinate`; S subordinate 8 bars, antecedent and consequent, PAC, in `subordinate`; C codetta 8 bars, presentation and continuation, PAC, in `subordinate`; region major [5], minor [3]), `modulate` on, seeds 1 to 10, eight rows. At least 9 of 10 must compose in each row.

| Row | Composed |
|---|---|
| C major 4/4 | 10 of 10 |
| A minor 4/4 | 10 of 10 |
| C major 3/4 | 10 of 10 |
| A minor 3/4 | 10 of 10 |
| G major 2/4 | 10 of 10 |
| D minor 2/4 | 10 of 10 |
| C major 6/8 | 10 of 10 |
| D minor 6/8 | 10 of 10 |

In all 80 runs T carries the transition function, its second half is in the region's key (a key lane span starts at the half: V in major, III in minor), its last chord is V or V7, its `fragment 2` idea is a liquidation by 1 step or more on a segment, its cadential idea has at least as many steps, and the codetta's `fragment 2` has none. The liquidation is read from the plan, not counted in notes: a residue is regrown to fill its bar, so `fragment 2` often holds as many melody notes as `fragment 1` or more. Without the pivot rule the four major rows composed 0 of 10 (stuck at harmony); `tests/test_harmony_phrase.c` `test_pivot_into_the_dominant` holds the rule on its own.

