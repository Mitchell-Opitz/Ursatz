# The parts of larger forms

The section kinds a larger form is made of, after Schoenberg ch. XVIII: the transition, the subordinate theme, the retransition and the coda (R-503). Back to the [docs index](README.md); the form side is in [forms.md](forms.md), the constraint in [plan-constraints.md](plan-constraints.md).

## The four parts

A form document names a part as the role of a section (`transition`, `subordinate`, `retransition`, `coda`), and `form_build_plan` gives the section a `section_function` constraint with the function, `independent` and `lyric` (all three always written). Any form may use them. No built-in form does yet, so every output is unchanged.

| Part | In the form document | In the plan | Grown |
| --- | --- | --- | --- |
| Transition | A theme-like section: `beginning` `presentation` or `compound_basic_idea`, `ending` `continuation`, `cadence` `half`; 8 bars when it modulates (`keys.end` is not its start), else 4 or 8, or 3 to 16 with a `construction`. No `returns`, `variation`, `figure`, `mode` or `pedal`. `independent` (default false) gives it a motive of its own (R-504), so it has no `motive`. | `archetype`, `theme_pairing`, `section_function` (`transition`) | As a theme today; B2 liquidates its continuation to the upbeat chord, B3 gives an independent one its own motive |
| Subordinate theme | Any pairing, cadence, keys and construction a theme-like section takes. `lyric` (default false): a period whose motive is m0 augmented (R-506); a lyric theme has no `motive` or `returns`, and the reader sets its motive to `augment` with an open ratio. | `archetype`, `theme_pairing`, `section_function` (`subordinate`), and for a lyric theme a `motive_ref` of `augment` | As a theme today; the open ratio is drawn as any section's motive is (the first usual ratio the meter writes, else an interval change) |
| Retransition | 4 bars, `cadence` `half`, nothing else: no `beginning` or `ending` and none of the members of a theme. | `section_function` (`retransition`), no `archetype` or `theme_pairing` | By its own operator in B4 (R-505); until then a plan with one stops at the structure stage |
| Coda | 8 bars, `cadence` `perfect-authentic`, nothing else. | `section_function` (`coda`), no `archetype` or `theme_pairing` | By its own operator in B5 (R-509); until then a plan with one stops at the structure stage |

A retransition and a coda count as theme sections in a compose run and are skipped when the form is heard (they have no theme shape to be heard as), and the style leaves their form alone, as it does a liquidation's.

## What is composed and what is not heard

The parts are composed to the plan and recorded in it; the hearing does not find them (R-408). The checks read the plan: the function of each section, its key and its cadence. B2 to B7 add what each part's section is made of and what was measured.

The test form is [parts-of-forms.json](../tests/fixtures/forms/parts-of-forms.json): 48 bars with the parts in the order of a sonata-like ternary (A, T, S, a codetta, R, A return, K). `test_form_spec` reads it and checks the plan it builds in C major 4/4; `test_plan_json` writes and reads the plan back byte for byte.
