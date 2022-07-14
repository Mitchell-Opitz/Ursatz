# standard-harmony

Theory Framework instance `standard-harmony-v1`: the basics of harmony in
common-practice Western music (Open Music Theory, Introduction to Harmony,
Cadences, and Phrase Endings). Path `frameworks/standard-harmony/`, tests in
`frameworks/standard-harmony/tests/`. A Framework extension module physically
separate from `src/theory/`, the same shape as `frameworks/standard-tonality/`.

It depends on no other framework. Chords are plain data (`HarmonyChord`,
`harmony_chord.h`: the scale degree of the root, 1 to 7, the inversion, and
whether it is a seventh chord), not
Roman numerals (`standard-tonality`) and not pitches. Voicing the inner voices is
`standard-voicing`'s. File names carry a `harmony_` prefix so they never collide
with the other frameworks' headers on the include path.

The entities these rules use live in `src/semantics/`: `HarmonicFunction`
(tonic, predominant, dominant), `CadenceType` (perfect authentic, imperfect
authentic, half, subverted), and `SeventhResolution` (default, incomplete,
leading-tone drop). They name things and carry no rule; which chords have
which function, and which progressions make which cadence, is this framework's.

## Scope

Every rule group has Rule/Constraint pairs referencing this Framework plus pure
check functions. A rule that states what the music requires is hard; "usually",
"almost always", and "not common" features are soft. `constraint_evaluate` is
never called here.

- **Harmonic function** (`harmony_functions.h`): the function of a chord by the
  scale degree of its root, whatever its inversion. Tonic: I (minor: i).
  Predominant, strong: IV and ii (minor: iv and ii diminished); weak: iii and vi
  (minor: VII, III, and VI). Dominant: V and vii diminished. In minor, degree 7 is
  VII (a weak predominant) unless the chord is marked as the one on the raised,
  leading-tone degree (vii, a dominant). `harmony_predominant_strength` says strong
  or weak.
- **Phrase model rules** (`harmony_phrase_rules.h`), on a run of chords in a mode.
  Hard: a tonic comes before a dominant (the least a phrase needs). Soft: no right
  to left motion (a predominant never follows a dominant, a tonic never follows a
  predominant); a weak predominant is followed by another predominant.
- **Cadence rules** (`harmony_cadence_rules.h`), on the last two chords and the
  soprano degree over the last. Hard: the progression is V to I or x to V.
  `harmony_cadence_classify` gives the type: V to I is a PAC when both chords are
  in root position and do is in the soprano, otherwise an IAC; x to V is a HC; a
  cadential progression the caller says is subverted is subverted.
  `harmony_cadence_strength` orders them: half, imperfect authentic, perfect
  authentic. A V7 to I in root position with do in the soprano is still a PAC.
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

## Not covered

- **Inner voices and doubling.** Filling in the inner voices (the notes still
  needed to complete each chord, doubling the bass in a root position chord) is
  `standard-voicing`'s.
- **Hearing.** The caller says whether a beginning follows and whether material
  repeats; this module does not detect either.
- **V7 inversions.** Only root position V7 to root position I is modeled; V6/5,
  V4/3, and V4/2 are not.
- **Voicing the V7.** `standard-voicing`'s `SEVENTH_CHORD` doubling rule says a
  seventh chord has four notes with none doubled, which the incomplete V7 (the fifth
  omitted, the root doubled) breaks. The two frameworks are independent, so this
  one allows the incomplete V7; relaxing that voicing rule is a separate change.
- **Other cadences.** Plagal and deceptive endings are not cadences in this
  chapter's vocabulary, so a progression like IV to I is reported as not
  cadential. Chords before V other than as "x" are not modeled.
- **The strength of an IAC.** An imperfect authentic cadence can be made more or
  less strong by its melody, rhythm, and what follows; that is not modeled.
- **Minor-mode melodic alterations** beyond the leading tone in V.
