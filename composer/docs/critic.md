# The critic

`<ursatz_composer/critic.h>`, `src/critic/` (module `critic`, between `check` and `piece`). The critic judges a candidate in Michelle's words (vision 4.7, R-716, R-719): it reads the candidate's melody line, eight evaluators each measure one thing she has said about a melody, and the score is minus the weighted sum of their penalties. Nothing composes with it yet: the entry that returns it comes in B4 and the ranking in B5. Back to the [docs index](README.md).

## The rules are data

Each evaluator is the knowledge entry `michelle.rule.<code>` ([knowledge](../../library/docs/modules/knowledge.md), R-717): its `code`, the `function` that computes it, its default weight and its parameters are the entry's. The code holds a table of the codes and the function names (`critic_evaluator_code`, `critic_evaluator_from_code`, `critic_evaluator_function`) and reads every parameter from the snapshot (`knowledge_snapshot_rule`); an entry or parameter that is missing is `CRITIC_ERR_KNOWLEDGE`. The test `test_critic_rules` pins the table to the entries both ways, and each evaluator's default weight to the default of `critic.weights.<code>`.

## The line

`CriticLine` is the melody of one candidate: its notes in time order, the length of one beat (of the first note's measure) and the pairs of ideas whose contrast is judged. A `CriticNote` has its `onset` and `duration` in whole notes, its `pitch`, its `bar` (the number the MusicXML export writes: a pickup is bar 0, the first full bar 1) and its `position` from the start of that bar. Every value is an exact `Rational`.

- `critic_line_from_render(plan, rendered, &line)` reads a render of the plan: the notes of the melody lane's voice 0. Bars and positions come from the render's meter map. The pairs are, for each idea node with a `contrast` constraint, the idea and the idea it contrasts against (the nearest idea of that name that starts at or before it); with no such constraint in the plan, a period's `contrasting idea` against `basic idea` and `consequent contrasting idea` against `consequent basic idea`. An idea's notes are those that start inside its span, its length is the span's.
- `critic_line_from_motive(motive, &line)` reads a motive from its own notes and meter, with no render (a plan is rendered in the hearing and the export only, `tools/ci/check-hearing.sh`). Time 0 is the start of the pickup, if there is one; the first barline is at the pickup's length. A rhythm motive sounds on its tonic, as its render does. No pairs.

## The evaluators

Each gives a `CriticFinding`: its penalty from 0 to 1, and where and by how much (`bar` is -1 when there is no place). The parameters are the entry's.

| Code | Penalty | Parameters |
| --- | --- | --- |
| `rhythmic_monotony` | The bars that are the `run`th or later of a run of bars with one rhythm, over the bars between the first and the last; 1 when every note but the last has one duration. A bar's rhythm is the positions and durations of the notes that start in it; only bars that hold a note onset count. | `run` 3 |
| `contrast_distance` | For each pair, the library's `dimension_contrast` of the two ideas: 1 - 2 x rhythm when the rhythm and the contour are both under `least` (too close), or up to 1 when the register is over `register` (too far, (register - `register`) / `register_span`); the mean over the pairs. The finding names the worst pair and the aspect. | `least` 1/2, `register` 7, `register_span` 5 |
| `leap_load` | (skips + 2 x leaps) / moves, less `allowance`, from 0 to 1; a skip is 3 or 4 semitones, a leap 5 or more. | `allowance` 1/2 |
| `open_ending` | 1 when the last note is no longer than the mean duration of the line's notes. | none |
| `stray_syncopation` | A note is syncopated when it starts off the beat and is held past the next one; stray when no other syncopated note starts at its place in the bar. The share of syncopated notes that are stray. The finding has the bar and the beat (1-based) of the first. | none |
| `one_rhythm` | The share of the other candidates of the set whose first bar has the rhythm of this line's first bar. 0 for a set of one. | none |
| `repeated_notes` | The notes that repeat the pitch before them, over the moves. | none |
| `wide_range` | (highest - lowest pitch - `limit`) / `span`, from 0 to 1. A tenth, OMT part II chapter 1. | `limit` 16, `span` 8 |

## The score

`critic_score(findings, settings)` is minus the sum of weight x penalty, each weight 0 to 20 (default 1); an evaluator in `CriticSettings.reversed` counts plus, so a listener can ask for what she usually avoids. The settings are the preferences' `critic` group ([preferences.md](preferences.md#the-critic-group-r-722)): `critic_settings_default` gives weights 1, nothing reversed, `rank` false, `candidates` 4 and `keep_rejected` 8. `critic_evaluate(line, set, set_count, knowledge, findings)` takes the lines of the candidates judged together as `set` (the line itself among them, skipped by pointer); an empty line is `CRITIC_ERR_EMPTY`.
