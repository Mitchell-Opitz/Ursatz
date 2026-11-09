# Known limits

Where the Analyzer reads a piece differently from its annotation, and why. Back to the [docs index](README.md).

- Key regions use fixed four-measure windows, so a boundary can land a measure or two from where a
  phrase would put it and a modulation shorter than about half a window is smoothed away. A stretch of
  four or more measures built on the subdominant, dominant or supertonic can read as its own key.
  The chord evidence of the first pass lets the library hold the home key through a passage on IV, V or
  ii when no tone outside the key is heard.
  Measured here (`test_key_regions`, still `WILL_FAIL`): Clementi Op. 36 No. 1 reads as annotated except
  measure 20 (the dominant of C minor reads as G major). Mozart K. 545 (the file holds all three
  movements) comes out as 43 regions, and 38 measures inside the cores of the annotated first-movement
  regions read in a neighbouring key (F major, G major, A minor, D minor or E minor); no modulation is
  found near measures 13 (to G), 29 (to C), 41 (to G), 79 or 124 (to C): the first three regions have no cadence
  arriving in them, so they are tonicizations, and the returns at 79 and 124 are not read. Bach 114 is read as G major, D major from measure 14, G major from measure 16 and D major from
  measure 20 to the end. Its file declares 4/4 for a 3/4 piece and the analyzer reads the file's meter, so its
  measures and beat strengths are those of a 4/4 reading: the two perfect authentic cadences of the 3/4 score (m. 20
  and m. 28) arrive at m. 15 and m. 21, beat 3.5. A region read in a key the music is not in also reads its chords in that key.
  The profile fits chromatic music less well. A key signature is trusted as a boundary: many exporters
  write a default C major at tick 0 whatever the key.
- Themes: the reference pieces yield no sentence, period or compound period (17 of Mozart 545's 18 themes and all
  the others are hybrids, one is a repeated phrase), so the sentence, period and compound period cases are tested on
  constructed pieces only. The library's ideas are the nominal two-measure cut, its cadential ending needs the bass
  (`bass_is_cadential_pattern`) and is never identified, fragmentation is judged on those nominal ideas and is rarely
  found, and its claim gives no acceleration or sequence trait. A constructed period cannot end its antecedent on a
  half cadence: a V followed by I reads as an authentic cadence, so the constructed antecedent ends on an imperfect
  authentic one.
- Motives: only the soprano is read (the library's `include_bass` is off), so there are no bass motives. The soprano is
  the highest sounding note at each attack, so a left-hand note is the line where the right hand rests. Clementi 36
  yields no family. Mozart 545 yields 214 families and 1610 occurrences, 168 of which cross a phrase boundary and lie in
  no phrase. A family's prototype can come after fragments of itself.
- Melody: only the soprano is read (the bass line is not asked for), and the outer voice extraction keeps one soprano
  note per voice segment, which drops the quick notes some leaps arrive on, and adopts a left-hand note where the right
  hand rests (Bach 114 measure 4). Bach 114 gets none of the annotated ranges and 4 of the 11 annotated leaps with the
  annotated status (36.4%, target 90%): `test_melody_bach_114_target` is `WILL_FAIL`. A MIDI file has no spellings, so the
  library reads a leap's size from the scale degrees, and two notes a semitone apart whose degrees are a fifth apart read
  as a leap of a fifth (Bach 114, measures 13 and 19). A phrase is one line, so a leap at its end has nothing after it (not judged). The line
  tendencies treat a rest in the line as no gap.
- Rhythm: the form adapter does not report elided cadences, so the hypermeter reads no phrase as elided and reports no
  hypermetric reinterpretation. Field 5's annotation has one (measure 9, where the phrase of measures 5 to 8 is elided with
  the next): `test_rhythm_field5_target` is `WILL_FAIL`, with the syncopation, the hemiolas (none) and 7 of 8
  hyperdownbeats (87.5%, target 80%; measure 18 is missed) meeting its target. The hypermeter reads the phrases as the
  form adapter cuts them, so a phrase of one or three measures takes the better of the two lengths and ends in a short
  hypermeasure. Tuplets are read from times alone: only triplets, sextuplets and duplets are looked for, a MIDI file
  whose times are not on the grid of 96 to the whole note (an expressive performance) yields few or none, and a duplet
  in a compound meter is two notes that halve a beat, which sound the same as two dotted values. Field 1, a performed
  12/8 file, yields 83 runs of triplets within the beat that the library does not call borrowed.
- Form plan: the kernel's sections close only on perfect and imperfect authentic cadences, so a piece of half
  cadences and deceptive endings is one section, and the final phrase never closes one. Returns compare six-note
  stretches by contour and rhythm, so a figure that recurs inside a hybrid reads as a return (Mozart 545: 77). A
  section's key is the key region its first chord lies in, not the key it ends in.
- Pivot chords are read in the old key and its mode; a minor old key is read in its minor vocabulary.
- Chord identification is Ursatz's weighted identification, read in each segment's key region and its
  mode: the diatonic triads and sevenths of a major key, or of natural, harmonic and melodic minor in a
  minor region, and since P40 the applied chords of the key (V/x, V7/x, vii°/x, vii°7/x). When no key is
  found, no chord is identified. Chord coverage on the reference pieces: Bach 114 92.6%, Clementi 36 97.8%,
  Field 1 99.1%, Field 5 100.0%, Mozart 545 98.1%. Borrowed (modal mixture) chords, the Neapolitan and
  augmented sixths are not candidates, so a segment built on one is matched to the nearest candidate or not at
  all.
- Roman numerals follow chord identification, so a chord has a numeral only where the weighted
  identification found one. The library's adapter reads a chord that is not a degree of its key, or has a
  quality the key does not give that degree, as an applied chord (`V6/5/V`) when it is one, and otherwise as
  having no numeral; it names no borrowed (modal mixture) chord, Neapolitan or augmented sixth, so the
  borrowed group of the vocabulary is empty. An applied chord that resolves to none is named all the same and
  exports `resolves_to` null ("no resolution" in the report). A major key's vii°7 of I reads as vii°7/vi.
- Chords are harmonic spans, not sonority segments: the over-cutting of one chord per change of sounding notes is replaced
  by Ursatz's span rule (whole measures partitioned from the bass, one chord a span), so the reference pieces read
  Bach 114 54 chords, Clementi 36 46, Field 1 107, Field 5 82 and Mozart 545 210. Two-voice textures read on spans
  (P40: a root and fifth complete a part, a held bass is not cut, an accented step over a held bass is evidence for the
  key, not a chord tone, and a bass that ends as the step lands holds nothing). Cadences, phrases, themes, form and the
  schemas still read the chords on the spans, and phrases and themes read the soprano at span starts until P41, so
  they moved: Bach 114 reads 0 cadences and 2 phrases, Clementi 36 5 cadences and 7 phrases, Mozart 545 23 cadences and
  27 phrases, Field 1 17 cadences, Field 5 3 cadences and 2 phrases. Phrases and cadences wait for P41, keys for P42:
  Mozart 545 reads 21 key regions and Clementi 36 6, with the key edges where the 4-measure windows put them.
- The outer voices in the document are per voice segment (one entry per melody note, several to a chord), but a voice
  segment is cut where the sounding pitch classes change, so melody notes that are all chord tones of one held chord share
  one entry (the highest, as the library reads it); a chord's own `outer` is the lines' entry at its start.
- Non-chord tones are classified only where the library can: its passing, neighbor, suspension,
  anticipation, appoggiatura, escape-tone and pedal rules. Many tones come back unclassified.
- Progressions are the library's base numerals counted as they come: Bach 114's most common progression is
  I > IV7 (6 occurrences, tied with ii > iii7), which ends on IV7, so `test_progressions_bach_114_top` (its top
  progression ends on I) is `WILL_FAIL`. The 4/4 reading of Bach 114's 3/4 meter (see below) also shapes its
  measures and so its per-measure harmonic rhythm.
- SMPTE-division MIDI files are rejected.
- Compressed `.mxl` MusicXML is not read: the Ursatz importer takes uncompressed documents only.
- MusicXML repeats and endings are not unfolded, and the pickup is found only when the first measure
  is marked `implicit="yes"`.
- Measure and beat follow the file's time signature events. A file that declares the wrong meter
  (for example, a minuet marked 4/4) is reported as declared.
