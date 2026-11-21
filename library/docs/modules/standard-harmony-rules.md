# standard-harmony rules

The rule groups of `theory/harmony/` for harmonic function, the phrase model rules, cadences, strong predominants, six-four chords, deceptive motion and predominant sevenths. [Docs index](../README.md)

Framework overview: [theory/harmony/README.md](../../theory/harmony/README.md).


The phrase model and cadence rule groups have pure check functions. A rule that states what
the music requires is hard; "usually", "almost always", and "not common" features are soft. Each
group says which of its rules are soft (`harmony_phrase_rule_soft`, `harmony_cadence_rule_soft`). The other groups classify chords and build no rules.


- **Harmonic function** (`harmony_functions.h`): the function of a chord by the
  scale degree of its root, whatever its inversion. Tonic: I (minor: i).
  Predominant, strong: IV and ii (minor: iv and ii diminished); weak: iii and vi
  (minor: VII, III, and VI). Dominant: V and vii diminished. In minor, degree 7 is
  VII (a weak predominant) unless the chord is marked as the one on the raised,
  leading-tone degree (vii, a dominant). `harmony_predominant_strength` says strong
  or weak.
- **Phrase model rules** (`harmony_phrase_rules.h`), on a run of chords in a mode.
  Hard: a tonic comes before a dominant (the least a phrase needs); when IV and ii
  (or ii6) both come before the dominant, in one run of predominants, ii comes after
  IV, never before (ii's root is a fifth from V's, which makes it the stronger
  predominant). Soft: no right to left motion (a predominant never follows a dominant, a tonic never follows a
  predominant); a weak predominant is followed by another predominant.
- **Cadence rules** (`harmony_cadence_rules.h`), on the last two chords and the
  soprano degree over the last. Hard: the progression is V to I or x to V.
  `harmony_cadence_classify` gives the type: V to I is a PAC when both chords are
  in root position and do is in the soprano, otherwise an IAC; x to V is a HC; a
  cadential progression the caller says is subverted is subverted.
  The cadence vocabulary (`harmony_cadence_kinds.h`, R-415) names each kind: the seven core types
  and evaded (a subverted cadence), abandoned (no cadence, core unknown) and medial caesura (a half
  cadence that ends a transition), each with a label, an abbreviation, its core type, its strength
  and whether it is conclusive; `harmony_cadence_kind_find` looks one up by name and
  `harmony_cadence_kind_of` finds the entry of a core type.
  `harmony_cadence_strength` is the one strength order for every `CadenceType` and reads the
  vocabulary: half,
  deceptive, plagal, authentic (unspecified), imperfect authentic, perfect authentic
  (a subverted cadence ranks 0, an unknown type -1). Kernel analysis receives it from
  its caller as a ranking function, and `harmony_cadence_compare` compares two types. A V7 to I in root position with do in the soprano is still a PAC.
  Soft: a half cadence does not end on V7 (the "19th-century HC", uncommon; it is
  still a half cadence).
- **Strong predominants** (`harmony_strong_predominants.h`): the chords that
  harmonize fa in the bass before V, so a phrase ends fa to sol (half cadence) or fa
  to sol to do (authentic cadence): IV, ii6 (the more common), and root position ii
  (a major key only, much less common). Only the triads in those inversions count.
- **6/4 chords** (`harmony_six_four.h`), 6/4 Chords as Forms of Prolongation: the three
  kinds of 6/4 chord besides the cadential one, `SixFourType` (cadential, passing, neighbor,
  arpeggiating), which the label depends on, not the Roman numeral alone. The subject is three
  chords (before, the 6/4, after), their notes as diatonic steps in four voices, and a beat
  strength for each; the 6/4 is a second inversion triad whose root is a fourth above the bass.
  `harmony_six_four_classify` says which kind it is: a chord marked cadential; a static bass
  with the same root position chord on both sides is a neighbor; a bass in three steps one way
  with the same function on both sides is passing; a bass leap to the fifth of the same chord
  with the upper voices sustaining it is arpeggiating.
- **Deceptive motion** (`harmony_deceptive.h`), La in the Bass at Beginnings, Middles, and
  Endings: V or V7 in root position to vi, or less commonly IV6, a motion and never a cadence.
  `harmony_deceptive_of` gives the kind. For the phrase model labels, a vi that connects the
  areas or stands in for a strong predominant is a weak predominant, so only a strong one gets
  PD.
- **Predominant seventh chords** (`harmony_predominant_seventh.h`), Predominant Seventh
  Chords: ii7 and its inversions (ii6/5, ii4/3, ii4/2), IV7, vi7, and iii7, which have
  predominant function. `harmony_predominant_seventh_of` gives the kind.
