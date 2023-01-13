# standard-form

Theory Framework instance `standard-form-v1`: how Western tonal music is
organized into phrase-level form. Path `frameworks/standard-form/`, tests
in `frameworks/standard-form/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-meter/` and `frameworks/standard-harmony/`. The names
it uses are plain value types in `src/semantics/` (`FormLevel`,
`MotiveKind`); this framework gives them their conventions.

## Scope

- **Hierarchy** (`hierarchy_rules.h`): form is a hierarchical grouping of
  units, piece, movement, section, theme, phrase, subphrase, idea, motive
  (`FormLevel`, largest to smallest). A larger level contains a smaller
  one. Two levels may collapse into one (a single phrase may be a whole
  section), so the hierarchy is a guide, not a strict definition.
  Phrase-level form is the phrase, subphrase, idea and motive levels.
- **Motives** (`motive_rules.h`): a motive is a short, regularly recurring
  snippet of music, shorter than the idea that contains it; a whole theme
  or melody is not a motive. A bare "motive" means a pitch motive
  (`MotiveKind`); rhythmic, contour and timbre motives are named for their
  kind. The usual transformations are enlargement, contraction, inversion,
  displacement, retrograde, intervallic manipulation and embellishment,
  each mapped to the `src/transform/` type that carries it out (enlargement
  is `augmentation`, contraction is `diminution`, intervallic manipulation
  is `intervallic_manipulation`, embellishment is `ornamentation`).
- **Phrases** (`phrase_rules.h`): a phrase is a relatively complete
  thought with a beginning, middle and end, trending toward a goal and
  arriving at closure. A cadence signals closure; the lack of a cadence
  does not say a phrase is open, since closure has other signals. A phrase
  can be any length, with 4, 8 and 16 measures particularly common. In
  tonal classical music the goal is a perfect authentic, imperfect
  authentic or half cadence; a subverted cadence is avoided, so it is not a
  goal. Cadence strength is half, then imperfect authentic, then perfect
  authentic (this framework's own ordering). In diagrams, square brackets
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
  size; the first, second and fourth map to `ornamentation`,
  `transposition` and `intervallic_manipulation`, and change of
  harmonization has no `NoteEvent` transformation. A sentential phrase
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
- **Unique forms** (`unique_rules.h`): a form organized as neither a
  sentence nor a period is unique. Archetype and unique are ends of a
  spectrum, not exclusive categories, and a repeated phrase takes the
  category of the phrase repeated. Labels are applied flexibly and dropped
  where they do not support the interpretation.
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
- **Cadential bass** (`cadential_bass_rules.h`): the core line is mi fa sol
  do (3 4 5 1), harmonized I6, ii6 or IV, V or V7 (often with a cadential
  six-four), I. It may be embellished by tonicizing the dominant with fi
  before sol. The clearest cadential ending is four measures with one bass
  note per measure. A bass note is a degree and an alteration, since this
  framework does no pitch arithmetic.
- **Hybrids** (`hybrid_rules.h`): any beginning can pair with any ending,
  nine pairings. Presentation + continuation is a sentence and antecedent
  + consequent is a period; the other seven are hybrids. Commonality: the
  sentence and period are very common; compound basic idea + consequent,
  compound basic idea + continuation and antecedent + continuation are
  slightly less common; compound basic idea + cadential and antecedent +
  cadential are occasional; presentation + cadential and presentation +
  consequent are rare or non-existent, which is still a valid pairing. The
  rules carry no length: hybrids are not limited to eight measures.

Related types: `Idea` (`src/containers/idea.h`) and `Subphrase`
(`src/containers/subphrase.h`) are reference-only groupings of events at
the idea and subphrase levels, built like `Phrase`. `PhraseLevelForm`,
`SubphraseRole`, `PhraseRole`, `IdeaLabel` and `ContinuationTrait` in
`src/semantics/` name the forms, roles, labels and traits. Segmentation,
sentence and period are procedures over `Phrase`, `Subphrase`, `Idea` and
`Cadence`; none is a stored entity.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

The suggested listening process for phrase-level forms (its tables are
encoded as the beginning and ending classifiers), other signals of closure
besides the cadence, a numeric size limit on a
motive (only its relation to the idea is asserted), a closeness score
between archetype and unique (an analysis judgment with its own
uncertainty), and the harmonic start of a presentation. `analysis/` does
not use these rules yet (`phrase_segmentation`, `motif_detection`,
`sentence_detection`, `period_detection`).
