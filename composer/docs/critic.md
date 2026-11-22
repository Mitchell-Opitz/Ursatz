# The critic

`<ursatz_composer/critic.h>`, `src/critic/` (module `critic`, between `check` and `piece`). The critic judges a candidate in Michelle's words (vision 4.7, R-716, R-719): it reads the candidate's melody line, eight evaluators each measure one thing she has said about a melody, and the score is minus the weighted sum of their penalties. With `critic.rank` on it also chooses a theme among candidates ([ranking](#ranking-theme-candidates-r-722)); off, nothing composes with it. Back to the [docs index](README.md).

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

## Observations (R-720)

An observation is a note in plain words from the message template of an entry. `critic_message` fills the template: `{bar}`, `{beat}` (1-based), `{first}`, `{last}`, `{count}`, `{value}` and `{limit}` come from a `CriticFinding`, `{idea}`, `{against}` and `{aspect}` from its names, `{planned}`, `{heard}` and `{amount}` from the `CriticFields` the caller gives. A name with no value (a bar of -1, a field left empty) and an unknown name stay as written.

`critic_judge(line, set, set_count, knowledge, settings, &judgment)` runs the evaluators, scores them and writes one **scored** observation for each finding with a penalty above 0, in evaluator order, from the entry `michelle.rule.<code>`: "bars 2 to 4 keep one rhythm", "the line spans 19 semitones, more than 16". `CriticJudgment` holds the score, the eight findings and at most 32 observations.

`critic_observe(hearing, plan, theme, line, knowledge, &judgment)` appends the observations of the Composer's hearing of the theme node against its plan (the hearing of [form-hear.md](form-hear.md) and [analyzer-check.md](analyzer-check.md), over the hearing's render, so nothing is analyzed twice):

| Rule | When | Message |
| --- | --- | --- |
| `cadence_heard` | A planned cadence of the theme (`analyzer_check_section_in`) that the Analyzer does not hear as planned. The bar is the theme's first bar number plus the item's 0-based bar. | the perfect-authentic cadence planned in bar 8 is heard as half |
| `theme_heard` | The form the Analyzer hears over the theme (`form_hear_in`) is not the form the section's pairing asks for, decided as `compose_run` decides `heard_as_requested` (a hybrid also needs the beginning and the ending asked for); "no theme" when it hears none. | planned as a sentence, heard as a hybrid |
| `contrast_heard` | A contrast constraint of an idea of the theme whose aspect the line's melody measures under its amount (`dimension_contrast` over the two ideas' notes). | the contrasting idea's melody is 0 from the basic idea in rhythm; the plan asks for 1/2 |

A theme the hearing cannot read (a lane left open, no meter) adds none: it is not held against the plan. The tests measure 12 themes composed from two fixtures at seeds 1 to 6 with the contrast target on: 5 heard as another form than asked, 1 with a planned cadence heard otherwise, none missing its contrast target.

None of them is scored. They say what was heard, in the words of the plan; whether they should count waits for her verdicts on them (R-720, B6 stores them), and the observation `voice_fault_heard` of the whole-piece check is listed by the check, never here.

## The critique entry

`composer_api_critique(context, request_json, job, out)` (API 4.6, R-721) judges candidates together. The request is `ursatz-critique-request` version 1 ([schema](../schema/api/critique.request.schema.json)):

```json
{"schema": "ursatz-critique-request", "schema_version": 1,
 "candidates": [{"motive": {"meter": ..., "notes": [...]}}, {"plan": {...}}],
 "preferences": {"critic": {"weights": {"leap_load": 0}, "reverse": ["wide_range"]}}}
```

`candidates` holds 1 to 16 objects, each with a `plan` (the plan document of a theme or a piece) or a `motive` (a motive document, as the `motive` member of a listen sample), not both. `preferences` is optional and read as everywhere else (its `critic` group counts). Each plan is heard once (`hearing_create`) and its melody line read from that render; each motive is read with `critic_line_from_motive`. Every candidate is judged with the lines of all of them as the set, and a plan with a node named `theme` is also observed against the hearing.

The response is `ursatz-critique` version 1 ([schema](../schema/api/critique.response.schema.json)): `candidates` has, in the order given, `index`, `score`, `evaluators` (each `rule`, `penalty`, `weight`, `reversed`) and `observations` (each `rule`, `scored`, `bar`, `beat`, `message`, with `bar` and `beat` null when there is no place). A rational is `{"numerator", "denominator"}`. Refusals: a candidate that is neither, an unreadable plan or motive and a request of more than 16 are `URSATZ_ERR_REQUEST` at `/candidates` or `/candidates/<i>` (`/plan`, `/motive`); a bad preference is refused at `/preferences/critic/...`.

## Ranking theme candidates (R-722)

With the preferences' `critic.rank` on (off by default) a theme request makes several themes for each one it keeps and lets the critic choose (`api_grow_run`, `rank_by_critic`). `critic.candidates` (2 to 8, default 4) is how many are drawn for each theme asked for (`suggest`), to the most a run holds (16).

1. The run draws `candidates x suggest` distinct themes, trying up to 16 seeds for each, from the request's seed on (a seed that does not compose is passed over, as it is without ranking).
2. Each theme is heard once (`hearing_create`) and its melody line read from that render. Every theme is judged with the lines of all of them as the set (`critic_judge`), so `one_rhythm` sees what the others open with, and observed against its own hearing (`critic_observe`).
3. The `suggest` themes of highest score are kept, in that order. A tie keeps the order drawn (a stable sort), so with every score equal the first seeds win. A theme the hearing cannot read is not judged and ranks after the others.
4. The rest are the **rejected** candidates: the seed, the score and each evaluator's finding of at most `critic.keep_rejected` of them (0 to 16, default 8, to the number of candidates not kept), best first. Their plans are dropped.

A run that made fewer candidates than asked ranks what it made. The response of a theme request that ranked has `critic` in each kept theme (`score`, `evaluators`, `observations`, the shapes of the critique response) and `rejected` in the document (`seed`, `score`, `evaluators`); a request that did not has neither, and its output is the bytes it was. The same request with `rank` on and off differs only by the theme the critic chose (the first seed that composes without ranking is always among the candidates). `ursatz listen` writes both with the settings, and with `--plans` the plan, so `ursatz critique` can judge the pick.

**Gate 3 (R-543, P68).** A `Listen Ursatz` round plays themes with `critic.rank` on against off: the first rejected candidate is the one the critic passed over, so a pair that plays the pick against it says whether she hears what the critic hears. Her ratings of the picks, on average, are the third gate. Whether the critic's score should also weigh the observations waits for her verdicts on them (B6).

## Variety in theme sets

`candidates.variety` above 0 ([preferences](preferences.md#the-candidates-group-r-673)) chooses each random motive for being unlike the ones before it. `ursatz listen --what themes` hands that choice the motifs of the themes before, as it has for a batch of motifs (R-722, [cli-listen.md](../../cli/docs/cli-listen.md)): the generated motif of each theme carries `avoid`, the motifs of the earlier themes that were written (the last 64). A theme made from a motif or a request the user gave has no generated motif and is as it was. A varied draw may be a motif a theme refuses (a pickup that makes it 3 bars); the sample is then reported as stuck.
