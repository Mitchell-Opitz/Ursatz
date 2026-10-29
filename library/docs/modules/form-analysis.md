# Form analysis

`frameworks/standard-form/analysis/` finds phrases, ideas and subphrases, themes and phrase expansion or contraction from real cadences and the soprano. It applies standard-form's existing rules and length conventions and defines no form of its own. standard-form includes no other framework and no kernel analysis (the architecture test enforces both, and now scans `analysis/` too): the input is neutral structs the caller fills, built in `tests/integration/` (`form_util.h`) from the standard-harmony cadences, the L2 soprano and the `MeterMap`.

The time helpers in `analysis_time.c` (`standard_form_measure_at`, `_measure_bounds`, `_last_measure_touched`, `_beat_length`, `_snap_to_beat`, `_on_beat`, `_first_counted_measure`) wrap `time/meter_arithmetic.h`; `StandardFormQ` keeps its layout and converts to `Rational` at each call.

## Input (`analysis_input.h`)

`StandardFormInput` is built with `standard_form_input_init`, `_add_cadence`, `_add_note`, `_add_segment`, `_finish`, `_destroy`. Ids are copied; spans, NoteEvents and the MeterMap are borrowed.

- **Cadence**: id, the arrival chord's TimeSpan, `CadenceType`, `strength_rank` (standard-harmony's; cadences are compared by it), `post_cadential`, uncertainty.
- **Soprano note**: id, NoteEvent, TimeSpan, optional MetricalPosition, `semitone`, and (from `_finish`) the interval and contour to the next note, absent across a rest. Notes are in time order and are distinct notes: a held note is one note.
- **Segment**: id, TimeSpan, the harmonic function and phrase-model label as the caller's codes. They are carried but only the segment boundaries are used (harmonic rhythm).
- **Config** (`StandardFormConfig`, defaults stated, not learned): `max_length_without_cadence` (0: the kernel's 8), `discontinuity_leap_semitones` (7), `cadence_lookahead_measures` (4), `max_absorbed_tail` (8), `similarity_min` (0.75), `no_signal_uncertainty` (0.5).

Lengths are in measures of the MeterMap. A pickup (measure 0) belongs to the first idea and is not counted.

## Phrases (`analysis_phrase.h`)

A phrase ends at the arrival of a phrase-goal cadence (PAC, IAC, HC, and an authentic cadence with no soprano). Subverted, deceptive and unclassified cadences end nothing; they are evidence of expansion.

**Reuse of kernel Phrase Boundary Detection.** The segmenter is the kernel's, not a second one. A framework may not include kernel analysis, so the caller injects it as a `StandardFormBoundaryDetector`; `tests/integration/form_kernel_bridge.h` maps the points onto `PhraseBoundaryDetectionPoint`s (the cadence label is `cadence_type_label`) and reads the PhraseSegmentation and signal Interpretations. The kernel is unchanged.

- **Points**: one per measure with a sounding soprano note, so the kernel's default of 8 points without a cadence is 8 measures. A goal cadence goes on the point of its arrival measure (the latest point at or before it).
- **Secondary signal**: a break after a measure (a measure with no note, a rest at the next barline, or a leap of `discontinuity_leap_semitones` or more into the next measure). The kernel counts it only after `max_length_without_cadence` points without a boundary. A break within `cadence_lookahead_measures` before a goal cadence is not reported: the cadence will close the phrase, so the break is inside it (without this, an expanded phrase of more than 8 measures is cut just before its cadence).
- **Snapping**: a phrase ends at the arrival chord's end snapped to the next beat of the MeterMap (a compound meter's beat is three units), never past that measure. Length = last measure − first measure + 1 (partial measures count whole).
- **Post-cadential absorption**: a `post_cadential` cadence is not given to the kernel. The preceding phrase extends to the end of its arrival chord (a post-cadential extension). After the last goal cadence a tail of up to `max_absorbed_tail` measures extends the last phrase: 1–2 measures a post-cadential extension, 3 or more a codetta. A longer tail, or any tail after a boundary the secondary signal made, is a phrase of its own with no cadence.
- **Output**: a kernel `PhraseOccurrence` per phrase (target phrase id `standard-form-phrase-<n>`) and an Interpretation targeting the occurrence. Uncertainty is the cadence's, 0.25 for a boundary by the secondary signal, or `no_signal_uncertainty` at the end of the input.

## Ideas and subphrases (`analysis_ideas.h`)

standard-form says an idea is usually two measures but gives no way to find its edge, so each phrase of n measures is cut into n / 2 (at least 1) ideas of two measures, the last taking the remainder. A phrase of three or more ideas has two subphrases: the first two ideas, and the rest. Labels: the first idea is the basic idea; a later idea that matches it (similarity) is a basic idea again, otherwise a fragment when shorter than the basic idea and a contrasting idea when not. Uncertainty adds 0.2 when the phrase length is odd and the similarity's.

**Idea similarity** (`analysis_similarity.h`): at each position the signed semitone interval and the onset rhythm are compared (a transposition keeps every interval); similarity is matched positions over the longer idea's intervals; a match is `>= similarity_min` (0.75). The margin is the distance from the threshold in [0, 1]. Similarity is interval and rhythm only; motive matching waits for L3.7.

## Themes (`analysis_theme.h`)

The framework's rules do the classifying: `standard_form_classify_beginning`, `standard_form_classify_ending`, `standard_form_pairing_form`, `standard_form_classify_phrase_pair`. In order at each phrase: a repeated phrase (the same cadence and a similar second phrase); a compound period (two one-phrase sentences, the second cadence stronger); a two-phrase theme (a two-idea antecedent and a phrase with a stronger cadence: a period when the second is a basic idea then a contrasting one with an authentic cadence, else a hybrid); a one-phrase theme (a presentation or compound basic idea plus an ending: a sentence or a hybrid). A theme is a Structure shell of note ids and a Relationship between its two parts (subphrase shells, or phrase occurrences), predicate the form's name; the Interpretation targets the Relationship. Uncertainty = 1 − (1 − u_cadences)(1 − u_similarity), u_cadences the greatest uncertainty of the phrases involved, u_similarity = 0.5 × (1 − margin).

## Expansion and contraction (`analysis_expansion.h`)

The expected length comes from the theme's own balance (an ending is as long as its beginning): longer is an expansion, shorter a contraction. An expansion of the ending is internal; standard-form has no separate rule for a "cadential expansion". The technique is named only from evidence (one more time: a subverted or deceptive cadence before the goal cadence; repetition: an ending idea that repeats an earlier one). A phrase's extension is an external expansion, with whether its length is typical and agrees with its location. A prefix, alternative path and stretching are not detected.

## Known debt

Kernel Period Detection duplicates standard-form's period rules. It is unchanged and the duplication is not resolved here. standard-form's `period_rules.c` already includes standard-harmony's `harmony_cadence_rules.h` for cadence order (missed by the architecture scan, since the include has no slash); the new code does not and compares cadences by the supplied rank.

## Limits

- Similarity is interval and rhythm only until L3.7.
- No section-level form, thematic returns or form trees.
- Ideas are cut at the nominal length, so a 1 + 1 + 2 continuation and fragmentation are rarely found; a cadential ending is not identified (no bass in the input); the only compound form is the compound period.
- Results depend on the soprano extraction and on the cadences supplied; with the cadence adapter's own, whole pieces are mostly unsegmented.
- Only one pickup is handled (the MeterMap's), not an anacrusis in the middle.
