# Harmony Lab

The Harmony Lab (R-769) is the tool where a harmony plan is heard before any melody: chords from one or two modes, laid on bars, shaped by an arc of brightness or tension, sounded with an accompaniment, kept as a plan, a schema of hers, or applied to a theme or a piece section. It is `views/harmony-lab-view.js` over `models/harmony-lab-model.js`, a tool after Pieces in the shell. It holds no music: every chord, text and number is the Composer's (`check-frontend-theory.sh`). Back to the [docs index](../README.md).

## The page

- **Key**: the tonic (names from the vocabulary's `pitch_class_names`), the mode and a second mode (`with`), the meter, the bars (1 to 64), the chords a bar (the choices of the meter's vocabulary entry: 1, 2 or its beats) and the cadence (`none`, `PAC`, `HC`).
- **Palette**: chips for the chords of the two modes, from the query `knowledge.palette` ([workshop knowledge](../../../workshop/docs/knowledge.md)): `<name> <numeral>`, marked shared (both modes hold it), color (it carries a tone the other mode lacks) or own. A chip puts the Composer's chord text for it (`I`, `v`, `IV:dorian`) on the focused unit; a chord the Composer has no text for has no chip.
- **Bars**: a text input for each unit, which may be typed (the chord text of a theme's `chords`). A bar whose first unit is open starts its chord at its beat (`V@2`). Under the bars, after Hear, each bar's tension and brightness.
- **Spans** (`palette.spans`): from bar, to bar, the pool of chords (any, first, second, shared, color) and a progression schema (the list is `progression_schemas` of `list_schemas`); **Shapes**: brightness and tension, rise, fall, hold, peak or step (with its bar); **accompaniment**: a pattern (the vocabulary's list) and a voicing.
- **Hear** posts the request to `composer.progression` and mounts the score workspace from the answer's MusicXML and playback (Blob URLs); nothing is saved. **Variants** asks for 4 and lists each with **Use**. **Save** creates the harmony item (`item.create`, type `harmony`, body `{uses: [], request}`) or saves a new version (`item.save`) and composes it (`item.compose`, a job). **Saved** opens a plan of the project's list (`item.list`, type `harmony`, then `item.get`).
- **Keep as my schema** (`knowledge.keep-schema`: the plan's version, a name and a slug) adds `user.schema.<slug>`, offered at once in the span's schema list. **Apply to** a theme or a piece (a picker of `item.list`) sends `item.apply-harmony` with the plan's version and, for a piece, the section.

A refusal of the Composer names a request path; `fieldForPath` maps it to the field it is about (`/palette/chords/<i>` to its bar, `/palette/spans/<i>` to its row) and the page says where.

## The theme editor's chord hint

The theme editor's chords field says "Roman numerals in C, at most N per bar (N from the meter's `chords_max`); @ sets the beat a chord starts on, from 0, on a beat or a half beat: I IV@1 V@2 I@3." The Composer takes up to four given chords a bar on beats and half beats (R-761); the hint is `chordHint` of `models/themes-model.js`.
