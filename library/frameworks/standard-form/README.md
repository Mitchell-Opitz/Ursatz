# standard-form

Theory Framework instance `standard-form-v1`: how Western tonal music is
organized into phrase-level form. Path `frameworks/standard-form/`, tests
in `frameworks/standard-form/tests/`. The name
it uses is a plain value type in `src/semantics/` (`FormLevel`); this
framework gives it its conventions.

## Scope

- **Phrases** (`phrase_rules.h`): a phrase is a relatively complete
  thought with a beginning, middle and end, trending toward a goal and
  arriving at closure. A cadence signals closure; the lack of a cadence
  does not say a phrase is open, since closure has other signals. A phrase
  can be any length, with 4, 8 and 16 measures particularly common. In
  tonal classical music the goal is a perfect authentic, imperfect
  authentic or half cadence; a subverted cadence is avoided, so it is not a
  goal. Cadence strength is half, then imperfect authentic, then perfect
  authentic (the ordering comes from standard-harmony). In diagrams, square brackets
  mark ideas and subphrases and arcs mark the phrase level and above.
- **Segmentation analysis** (`segmentation_rules.h`): divides a passage
  into grouping units with square brackets. The steps run in order:
  identify points of closure (usually cadences), divide each phrase into
  smaller units, bracket those units, verify and label the cadences. An
  idea is often two measures long but may be longer or shorter, and the
  ideas of a phrase together fill it exactly. Lengths are whole measures.
- **Sentence** (`sentence_rules.h`): one phrase of two subphrases, a
  presentation then a continuation. The presentation is a basic idea and
  its repetition, often 4 measures; basic ideas are often 2 measures, with
  1 and 4 also used. The continuation is as long as the presentation or
  longer, and uncommonly shorter. A continuation is marked by
  fragmentation (unit sizes shorter than before, length only), faster
  durations, sequence and faster harmonic rhythm; more traits make it
  easier to identify, and fragmentation alone usually signals it. A second
  idea with the same contour reads as a varied repetition, a different
  contour as contrast. A basic idea is commonly varied by embellishment,
  transposition, change of harmonization, or change of interval quality or
  size. A sentential phrase
  needs at minimum a presentation and a continuation; the archetypal
  sentence is 4 or 8 measures with a balanced continuation that has
  fragmentation.
- **Period** (`period_rules.h`): two phrases, an antecedent then a
  consequent, each a basic idea then a contrasting idea. The antecedent
  ends with a weaker cadence, most often a half cadence; the consequent a
  stronger one, most often a perfect authentic cadence. The antecedent is
  often 4 measures, and the consequent is as long or longer, rarely
  shorter. A modulation usually happens in the consequent. The
  consequent's basic idea is usually like the antecedent's (numbered b.i.1
  and b.i.2 when they differ) and its contrasting idea almost always
  differs.
- **Combinations** (`combination_rules.h`): a repeated phrase is a phrase
  followed by a written-out repeat that ends with the same cadence; repeat
  signs add no measures, so they do not make one. A compound period is a
  period whose two phrases are sentences. Two consecutive phrases classify
  from their cadences: the same cadence on a written-out repeat is a
  repeated phrase, a stronger second cadence is a period (compound when
  both are sentences), and a weaker one is neither.
- **Beginnings** (`beginning_rules.h`): a hybrid form pairs the beginning
  of one archetype with the ending of another. The three beginnings are
  the antecedent (ends with a cadence; basic idea then contrasting idea),
  the compound basic idea (an antecedent without a cadence: the melody may
  rest but the harmony evades the half cadence) and the presentation (no
  cadence; basic idea then its repetition). A classifier tells them apart
  by cadence and the two ideas.
- **Endings** (`ending_rules.h`): the consequent (PAC or IAC, never HC;
  basic idea then contrasting idea; begins stable), the continuation (HC,
  IAC or PAC; often divided 1+1+2 or 2+2; begins unstable) and the
  cadential ending (PAC or IAC, never HC; core bass line; 2+2 or
  indivisible; never fragmented). Stability at the start of a cadential
  ending is not characterized. A classifier takes cadence, bass, ideas and
  fragmentation; a cadential bass with an authentic cadence and no
  fragmentation settles a combination that also fits a consequent.
- **Hybrids, length, internal and external expansion**: see [standard-form-expansion.md](../../docs/modules/standard-form-expansion.md).
## Not covered

The suggested listening process for phrase-level forms (its tables are
encoded as the beginning and ending classifiers), other signals of closure
besides the cadence, and the harmonic start of a presentation. `analysis/` does
not use these rules yet (`phrase_segmentation`, `motif_detection`,
`sentence_detection`, `period_detection`). Phrase expansion and contraction
are rules over caller-supplied lengths only: detecting them in real music
arrives with the form adapter (L3.6).

## Analysis of real music

`analysis/` finds phrases (feeding the kernel's Phrase Boundary Detection with real cadences, absorbing post-cadential extensions and snapping to the beat), ideas and subphrases by the length conventions above, themes (sentence, period, hybrid, compound period, repeated phrase) from idea similarity and cadences, and phrase expansion and contraction. See `docs/modules/form-analysis.md`.

`analysis/` also finds motives: recurring short note groups of the outer lines, grouped into families by transposition-invariant interval and rhythm, with inversion, retrograde, augmentation, diminution, fragmentation and variation matched against each family's prototype. See `docs/modules/motive-analysis.md`.

`analysis/` also finds written-out repeats and melodic returns over measures, and lets a phrase grid follow the repeats. See `docs/modules/analysis-repeats.md`.

## Vocabulary enums

Owned here (moved from `src/semantics/`; type names and values unchanged), each in its own header: `PhraseRole`, `PhraseBeginning`, `PhraseEnding`, `IdeaLabel`, `ContinuationTrait`, `PhraseLevelForm`, `PhraseLengthChange`, `ExpansionPlacement`, `InternalExpansionTechnique`, `ExternalExpansionKind`. `FormLevel` and `SubphraseRole` stay in `src/semantics/` because `src/containers` uses them; `CadenceType` stays because it is shared.

Two of them look a value up by name, false with the output untouched for an unknown name or NULL: `phrase_beginning_from_name` and `phrase_ending_from_name`.

## Neutral analysis input

`standard_form_input_add_analysis_note` (`analysis/analysis_input.h`) adds an `AnalysisNote` to the soprano or the bass line (`semitone` is `midi`, no metrical position; the interval and contour to the next note are still set by `standard_form_input_finish`). `standard_form_input_add_analysis_segment` adds an `AnalysisSegment` with its function and phrase model label. A note with an unknown pitch is rejected.

`StandardFormConfig.relation_match` (a `StandardFormRelationMatchFn`, `analysis/analysis_input.h`) is the injected owner of every occurrence kind but varied, and is also asked about a shorter window to find a prefix or suffix fragment. It names the kind with `MotiveOccurrenceKind` (`semantics/motive_occurrence_kind.h`); `StandardFormMotiveKind` is a kept alias until the Analyzer stops using it. The motive analysis requires it: with NULL (the default) it returns `STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR`. Varied always stays the framework's.
