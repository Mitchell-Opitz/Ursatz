# standard-harmony rules

The rule groups of `frameworks/standard-harmony/` for harmonic function, the phrase model rules, cadences, V7 resolution, strong predominants, embellishing tones, six-four chords, deceptive motion, the mediant and predominant sevenths. [Docs index](../README.md)

Framework overview: [frameworks/standard-harmony/README.md](../../frameworks/standard-harmony/README.md).


Every rule group has Rule/Constraint pairs referencing this Framework plus pure
check functions. A rule that states what the music requires is hard; "usually",
"almost always", and "not common" features are soft. Each rule table registers an
evaluation callback on its Constraints, so `constraint_evaluate` runs the same check
(`harmony_rule_table_evaluate`) on a candidate `Value` tagged `standard-harmony/<table prefix>`
holding the rule's subject struct. Each soft rule also has a `Preference` (`harmony_*_preference`)
whose scoring callback makes `preference_rank` score the same candidate 1/1 when the rule holds
and 0/1 when it does not.


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
  `harmony_cadence_strength` is the one strength order for every `CadenceType`: half,
  deceptive, plagal, authentic (unspecified), imperfect authentic, perfect authentic
  (a subverted cadence ranks 0, an unknown type -1). Kernel analysis receives it from
  its caller as a ranking function, and `harmony_cadence_compare` compares two types. A V7 to I in root position with do in the soprano is still a PAC.
  Soft: a half cadence does not end on V7 (the "19th-century HC", uncommon; it is
  still a half cadence).
- **V7 resolution rules** (`harmony_seventh_rules.h`), V7 to I in four voices, as
  scale degrees for each voice from the soprano to the bass, in root position (the
  chapter Strengthening Endings with V7). Hard: the bass is sol to do; V7 is
  complete (sol, ti, re, fa) or incomplete (the fifth omitted, the root doubled);
  fa goes to mi, re to do or mi, ti to do, or (alto or tenor of a complete V7 only)
  ti drops to sol; the tonic chord has only do, mi, and sol, with do and mi; in minor
  the V7 has the leading tone. Soft: avoid re to mi (two roots, two thirds).
  `harmony_seventh_classify` gives the `SeventhResolution`: default (three roots, one
  third, no fifth), incomplete V7 (a complete tonic chord; only the root can be
  doubled, since doubling ti or fa makes parallel octaves), or leading-tone drop (a
  complete tonic chord).
- **Strong predominants** (`harmony_strong_predominants.h`): the chords that
  harmonize fa in the bass before V, so a phrase ends fa to sol (half cadence) or fa
  to sol to do (authentic cadence): IV, ii6 (the more common), and root position ii
  (a major key only, much less common). Only the triads in those inversions count.
- **Strong predominant part-writing rules** (`harmony_predominant_rules.h`), the
  predominant, V, and (for an authentic cadence) I in four voices, as diatonic steps
  for each voice from the soprano to the bass (`harmony_steps.h`; the octave is kept
  so the direction of motion is known). Hard: IV, ii6, or ii; the bass is fa (re for
  ii) to sol to do; each chord has only the notes of its triad, with its root and
  third; ti goes up to do and re down to do over I; in minor, V has the leading
  tone; and no parallel fifths or octaves from the predominant to V or from V to I
  (a fifth between ti and fa is diminished, so it does not count). Soft: not all
  voices move the same way into V; the soprano reaches V from above or by common
  tone; IV doubles its bass; over ii the soprano is re or fa, not la; root position
  ii is for a major key. A half cadence leaves off I (`has_tonic` false).
- **Embellishing tones** (`harmony_embellishing.h`, `harmony_embellishing_rules.h`),
  notes that embellish a chord tone, as a gesture of two or three notes (diatonic
  steps, whether each is a chord tone, and its beat strength). The kind comes from
  how the middle note is approached and left (a step is one diatonic step, a leap
  more): passing tone (step, step, same direction), neighbor tone (step, step,
  opposite), appoggiatura (leap, step, opposite), escape tone (step, leap,
  opposite), suspension (static, step down), retardation (static, step up), pedal
  (static, static), and anticipation (a two-note gesture: the same pitch twice).
  Hard: the gesture is one of those eight; a suspension or retardation is on a
  stronger beat than the notes around it. Soft: the first and last notes are chord
  tones; the embellishing note is not; an appoggiatura is on a stronger beat and an
  escape tone on a weaker one; an anticipation is on a weaker beat than the note
  after it; an appoggiatura or escape tone is left downward.
- **Cadential 6/4 rules** (`harmony_cadential_rules.h`), the cadential 6/4, an
  embellishment of V from two embellishing tones, a sixth and a fourth above the bass
  sol, and its resolution to V or V7 (Strengthening Endings with Cadential 6/4). The
  chord is marked `cadential` on `HarmonyChord` (the tonic triad in second
  inversion): it has dominant function, not tonic, so it counts as a dominant in the
  phrase model. The rules take the chords as diatonic steps in four voices. Hard: the
  bass is sol and stays; the chord is sol, sol, mi, do (the bass doubled); the voice
  on mi falls a step to re and the voice on do falls a step to ti (6-5, 4-3), in the
  same voice; the voice doubling the bass stays on sol (V) or falls a step to fa
  (V7, 8-7); in minor, V has the leading tone; and no parallel fifths or octaves
  into the 6/4 or out of it. Soft: no voice but the bass leaps into the 6/4; no voice
  leaps to the fourth above the bass, the dissonance; the chord that approaches it is
  IV or ii6. The approach chord is optional.
- **6/4 chords** (`harmony_six_four.h`, `harmony_passing_rules.h`,
  `harmony_neighbor_rules.h`, `harmony_arpeggiating_rules.h`), 6/4 Chords as Forms of
  Prolongation: the three kinds of 6/4 chord besides the cadential one, `SixFourType`
  (cadential, passing, neighbor, arpeggiating), which the label depends on, not the Roman
  numeral alone. The subject is three chords (before, the 6/4, after), their notes as
  diatonic steps in four voices, and a beat strength for each; the 6/4 is a second
  inversion triad whose root is a fourth above the bass. `harmony_six_four_classify` says
  which kind it is: a chord marked cadential; a static bass with the same root position
  chord on both sides is a neighbor; a bass in three steps one way with the same function on
  both sides is passing; a bass leap to the fifth of the same chord with the upper voices
  sustaining it is arpeggiating. Passing, which prolongs tonic or predominant: hard, the bass
  steps one way, both sides have the same function, and the 6/4 is the bass doubled with the
  fourth and sixth above it; soft, tonic or predominant, the soprano moves by step, and the
  inner voices stay or step. Neighbor (pedal), over do do do or sol sol sol: hard, the bass is
  static, the same root position chord is on both sides, the bass is that chord's root, the 6/4
  notes, and two upper voices go up a step and back while the third stays; soft, I or V.
  Arpeggiating: hard, the same chord throughout, the bass leaps to its fifth, and the upper
  voices sustain; it is not labeled in analysis. The cadential 6/4 gets a soft rule that it is
  on a stronger beat than the V(7) it resolves to.
- **Cadence hearing rules** (`harmony_hearing_rules.h`), whether a potential
  cadence point is a true cadence, from what the caller says is heard. Hard: the
  harmonies are V to I or x to V; a sense of beginning follows; what follows does
  not repeat material from the middle of the phrase. Soft: the phrase is more than
  two measures long unless the tempo is quite slow.
- **Cadence writing rules** (`harmony_writing_rules.h`), the bass and soprano of a
  cadence in I and V only, as degrees. PAC: bass 5 to 1, soprano 2 to 1 or 7 to 1.
  IAC: bass 5 to 1, soprano 2 to 3 or 5 to 5. HC: bass 1 to 5, soprano 1 to 7 or 3
  to 2 (5 to 5 is possible but not common, a soft rule). In minor the V chord needs
  the leading tone. A subverted cadence is not written.
- **La in the bass** (`harmony_deceptive.h`, `harmony_deceptive_rules.h`,
  `harmony_iv6_prolongation.h`, `harmony_iv6_prolongation_rules.h`,
  `harmony_plagal_six_rules.h`, `harmony_connection_rules.h`, `harmony_phrygian.h`,
  `harmony_phrygian_rules.h`), La in the Bass at Beginnings, Middles, and Endings. Each takes
  its chords in four voices as diatonic steps. Deceptive motion (V or V7 to vi, or less
  commonly IV6; a motion, never a cadence): hard, the chords, their notes, ti up to do (and fa
  down to mi in V7), every other upper voice down against the bass in V(7) to vi, and no
  parallels; soft, vi over IV6. Tonic prolongation with IV6 as a predominant (I IV6 V I, or I
  IV6 V6 I, bass do la sol|ti do): hard, the chords, the bass line, their notes, no parallels,
  no doubled leading tone; soft, the soprano do do re do, V6 over V, and a major key. Plagal
  (IV6) prolongation (I (IV6) I6, bass do la mi): hard, the bass leaps down each time, the
  notes, the other voices by step or common tone, no parallels; soft, the soprano mi fa sol.
  La connecting tonic to a strong predominant in a phrase middle (I, vi or IV6, IV or ii6,
  bass do la fa): all hard, with the bass leaping down and the upper voices by step or common
  tone. The phrygian half cadence (iv6 to V, minor only, a half cadence and not a
  `CadenceType`): hard, minor, the notes, do doubled in the iv6, no augmented second from le up
  to ti, no parallels. For the phrase model labels, a vi that connects the areas or stands in
  for a strong predominant is a weak predominant, so only a strong one gets PD.
- **The mediant** (`harmony_mediant.h`, `harmony_mediant_rules.h`), The Mediant Harmonizing
  Mi in the Bass: iii (III in minor), a weak predominant that usually travels through a strong
  predominant on its way to V, used after I in root position to harmonize a descending ti.
  Three chords, I (root position), iii (root position, or less often first inversion), then
  a strong predominant (IV, ii6, ii) or V directly: `harmony_mediant_of` says which of the two
  kinds. Hard: the chords; each chord's own bass (do mi, then fa or re, or sol to V); each
  chord's notes with root and third present; an upper voice on ti over iii that is do over the
  I and goes down to la over the predominant (or stays on ti into V); and no parallels. Soft:
  iii in root position, iii through a predominant rather than directly to V, and, in minor,
  no iii directly to V. iii is not a substitute for I6, which the prolongation rules
  harmonize I V4/3 I6, so no chord before iii but I is accepted.
- **Predominant seventh chords** (`harmony_predominant_seventh.h`,
  `harmony_predominant_seventh_rules.h`, `harmony_ii7_v7_rules.h`), Predominant Seventh
  Chords: ii7 and its inversions (ii6/5, ii4/3, ii4/2), IV7, vi7, and iii7, which have
  predominant function. `harmony_predominant_seventh_of` gives the kind, and
  `harmony_predominant_seventh_rank` how common it is (a value of the kind, not a rule): ii6/5
  most, ii7 somewhat, ii4/3 less, ii4/2 least; IV7 and vi7 less common, iii7 least. The
  treatment rules take three chords (before, the seventh chord, after) in four voices as
  diatonic steps. Hard: the middle chord is a predominant seventh; its notes (root, third, and
  seventh present, the bass on the root, third, fifth, or seventh of its inversion); the chordal
  seventh is approached by step or common tone and resolves down by step; and no parallels.
  Soft: ii7 is preceded by I6 rather than I, IV7 and vi7 are in root position, and vi7
  connects the tonic area to a strong predominant (it is not reached from V7 or followed by
  V). ii7 to V7, both in root position, is its own subject: hard, the notes, one of the two
  incomplete (ii7 with re doubled and no la, or V7 with sol doubled and no re), and no
  parallels.
