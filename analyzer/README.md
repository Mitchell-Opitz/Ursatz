# Ursatz-Analyzer

## Key estimation

`estimate_global_key` (`src/pipeline/key_pipeline.c`) picks a piece's global tonic and mode in this order, stopping at the first one that succeeds:

1. **MIDI key signature meta-event.** If the imported file carries a Key Signature meta-event (`0xFF 0x59`, read by `midi_key_signature_scan` in the `Ursatz` submodule), its sharps/flats count and major/minor flag are decoded directly via a circle-of-fifths table. No pitch-class scanning happens on this path.
2. **First/last-chord bass note.** When no key signature is present, the bass pitch class of the piece's first and last onset-grouped chord is tried as a tonic candidate -- the first and last chord of a piece is very often the tonic triad in root position.
3. **Brute-force pitch-class scan.** If neither of the above yields a match, every pitch class 0-11 is tried in order (C, C#, D, ...) against `KeyEstimation`'s major/natural-minor collections, and the first match wins.

Flat spelling (`Db`, `Eb`, ... via `PITCH_CLASS_NAMES_FLAT` in `pipeline/pitch_class_util.c`) is used for a key label only when the key signature that produced it has a negative (flats) sharps/flats count. Every other label -- the brute-force scan, chord/note spelling elsewhere in the report -- stays on the sharp-only spelling used throughout the rest of this codebase.