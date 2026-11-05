# standard-form hybrids and expansion

The hybrid, length, internal expansion and external expansion rule groups of `frameworks/standard-form/`. [Docs index](../README.md)

Framework overview: [frameworks/standard-form/README.md](../../frameworks/standard-form/README.md).

- **Hybrids** (`hybrid_rules.h`): any beginning can pair with any ending,
  nine pairings. Presentation + continuation is a sentence and antecedent
  + consequent is a period; the other seven are hybrids. Commonality: the
  sentence and period are very common; compound basic idea + consequent,
  compound basic idea + continuation and antecedent + continuation are
  slightly less common; compound basic idea + cadential and antecedent +
  cadential are occasional; presentation + cadential and presentation +
  consequent are rare or non-existent, which is still a valid pairing. The
  rules carry no length: hybrids are not limited to eight measures.
- **Length** (`length_rules.h`): expansion makes a phrase last longer than
  expected, contraction shorter. Contraction is always internal; expansion
  is internal or external. A phrase has an expected length when its
  cadential motion lets a cadence be anticipated where it is evaded, or
  when the piece states an unexpanded version to compare against. The
  caller supplies the expected length; the classifier compares it with the
  actual length. An ending is expected to be as long as its beginning
  (proportional balance), so a longer one is expanded and a shorter one is
  contracted. This is a judgment against an expected length, not the
  event-level `src/transform/` Expansion and Contraction.
- **Internal expansion** (`internal_expansion_rules.h`): added length after
  the beginning and before the cadence, by repetition (only an unexpected
  one: a presentation's repeated basic idea is predictable), stretching,
  the one-more-time technique (try a cadence, evade it, retry it) or an
  alternative path (a detour returns to previous material before the
  cadence, a reroute does not). Techniques are often combined. One-more-time
  has already tried and failed to cadence; an alternative path has not.
- **External expansion** (`external_expansion_rules.h`): a prefix comes
  before the beginning, usually an introduction. Suffixes come after the
  cadence, and location decides the kind: a post-cadential extension within
  a section, a codetta after a section, a coda after a piece or movement.
  Length is a guideline (1-2, 4-8 and 8 or more measures) checked against
  the location, since the terms are used flexibly. External expansions are
  drawn with dashed lines, internal ones with solid.

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
