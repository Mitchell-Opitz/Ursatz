# Input and meter

The input formats the Analyzer reads, the notation readings applied to the notes before analysis, and how meter and measure numbers are decided. Back to the [docs index](README.md).

## Input formats

The analyzer reads MIDI and MusicXML. Both go through the same pipeline: each input becomes the
same `ImportedNotes` (notes, a `MeterMap`, key signature events), so every later stage is
independent of the format, and the analyzer adds no parsing or music theory of its own. MusicXML is
read by Ursatz's MusicXMLImporter.

- **Supported files.** MIDI (`.mid`, `.midi`; SMF type 0 and 1) and uncompressed MusicXML
  `score-partwise` (`.musicxml`, `.xml`).
- **Choosing the format.** By extension first; when the extension says nothing, by the file's first
  bytes (an `MThd` header is MIDI, an XML document is MusicXML). `--input-format midi|musicxml` forces
  the format and overrides both. A file whose format cannot be told is an error that names the flag.
- **What is notated and what is inferred.** MusicXML states what MIDI does not: pitch spelling, the
  time and key signatures as written, the measures, a pickup, parts and voices. The report's Summary
  says which input was read and whether the meter, the key signature and the pitch spelling were
  *notated* (the score's own), *embedded in the file* (a MIDI event) or *inferred* (the input has none:
  4/4 is assumed, or the key is found from the notes alone). The same is in the analysis document's
  `metadata.input`. A key is always *detected* from the notes; a notated key signature is only a
  boundary for the detection (see [Key regions](analyses/harmony.md#key-regions-tonicizations-and-modulations)), for either input.
- **Pitch spelling.** For a MusicXML score the chord roots and basses and the outer-voice pitch names
  follow the score's spelling (`Db` stays `Db`, `C#` stays `C#`). For MIDI, which has none, they are
  spelled by the pipeline's rule (flats in keys that read in flats). Key labels are always spelled by
  the rule. Where a transposing part's written pitch is not the sounding pitch, the sounding pitch is
  analyzed and the name falls back to the rule.
- **Several parts.** All parts are analyzed together as one texture: none is dropped, and the soprano
  and bass are the highest and lowest notes sounding in any part. Each note keeps its part, staff and
  voice (`ImportedSource` in `imported_notes.h`), and `metadata.input.parts` lists the parts with their
  note counts. Per-part and per-voice analysis is not done.
- **MIDI parts.** A MIDI file has one part per track and channel that has notes, in order of first
  appearance. A part is named from the track name (meta 0x03) and the General MIDI instrument of its
  program change (a channel that never sends one is program 0; channel 10 is "Percussion", program -1):
  the track name alone when the track has one part, the track name and the instrument when it has
  several, the instrument when the track has no name. Keyboard-like programs (pianos, harpsichord,
  celesta, organs, accordion, harp) are on two staves, split at middle C by Ursatz's
  `score_staves_assign`; every other part has one. The file is parsed once, by the library's
  `midi_importer_import_complete`; the parts, their names and programs, the meter, the key signatures and the
  tempo map all come from that one import.
- **Tempo map.** A MIDI file's set-tempo events are kept as `ImportedNotes.tempo_map` (one region per distinct
  tick; a file with none gets one defaulted region of 120 quarters per minute). A MusicXML file has none (NULL).
  Note times do not depend on it.
- **Tempo, dynamics, articulation.** Tempo and dynamics markings are imported, kept with their part
  and position, and reported in `markings` and a `## Markings` section, read with the library's
  standard-markings rules (tempo group and change, dynamic rank and change, louder or quieter than the
  previous level marking in the part). Wedges are not read. The importer does not expose
  articulation, so `markings.articulations` is `null` with `articulations_imported: false`; this is
  deferred until the importer reads them.
- **Errors.** Malformed XML, a root other than `score-partwise`, an unreadable value, a score with no
  notes, a compressed file and an unreadable file each stop with a message on stderr and exit 1, and
  no output file is written. Things the importer skips or rounds (a grace note, an unpitched note, a
  fractional alteration) are warnings on stderr and are counted in `metadata.input.warning_count`.
- **Not supported.** Compressed `.mxl` (the Ursatz importer reads uncompressed documents only; the
  analyzer reports it as unsupported rather than unzipping it: export the score as uncompressed
  MusicXML), `score-timewise`, grace notes, wedges (crescendo and diminuendo), additive meters
  (summed) and senza-misura. Repeats and endings are not unfolded: the score is analyzed once, in
  written order.
- **Schema.** The analysis document is schema version 9 (see
  [analysis-document.md](analysis-document.md)).


## Notation readings

A file can be right about its notes and wrong about how they are written: a minuet exported in 4/4 with
every value half as long as the printed ones, or a pickup the file does not mark. A notation reading
says how to read the notes instead, and is applied to the imported notes before analysis:

- `--meter <n/d>` replaces the meter map with one region of that meter (`3/4`, `6/8`; the lower number
  is 1, 2, 4, 8 or 16, the upper 1 to 64).
- `--value-scale <n or n/d>` multiplies every start and duration by that factor. The allowed factors are
  `1/4`, `1/2`, `1`, `2` and `4`; `1` changes nothing. Scale 2 reads each note as twice as long, so a
  4/4 file read in `3/4` at scale 2 has `24 * 2 / (3/4) = 64` measures.
- `--pickup <n/d>` makes the first measure a pickup of that many whole notes, counted after scaling and
  shorter than one measure of the meter. Without `--meter` it uses the file's first full measure's
  signature and drops the file's later signature changes.

A note keeps its id, pitch, spelling, voice and provenance, so ids in any list stay valid. Key
signature events, the end time, tempo markings and dynamics move with the notes. Nothing applies a
reading on its own. The analysis document's `meter` says what was done: `source` is `"override"` when a
meter was set, and `reading` is `{"scale", "meter_overridden"}` (`null` when no reading was applied).
A reading that cannot be applied (a pickup as long as the measure, a scale that is not allowed) exits 1
with a message and writes nothing. The code is `notation_reading.h`; `notation_reading_apply` is the
library entry point.


## Meter and measure numbers

The meter comes from the file's time signature events, read by Ursatz's `MeterMap`. The report's
Meter section gives the time signature, the number of measures, a pickup if there is one, and any
later change of signature. The analysis document's `meter` section carries the same, and every
claim in the document has a `location` with the measure and beat where it starts and ends. The
report writes these as `m. N, beat B`.

- The first full measure is measure 1. A pickup (anacrusis) is measure 0, so the music after it
  starts at measure 1. A piece with no pickup has no measure 0. A pickup is found from what the file
  says (a partial first measure ended by a later time signature, or a short leading signature such
  as 1/8 before 12/8), never from the notes: a leading rest is a rest, not a pickup.
- A beat is the signature's beat: a quarter note in 4/4, 3/4 and 2/4, a dotted quarter in 6/8 and
  12/8. Beats count from 1. A fractional beat is shown as a decimal (`beat 2.5`); the document
  keeps the exact fraction.
- A file with no time signature event gets the library's default 4/4. The report and the document
  mark it as inferred, an assumption and not a fact about the piece. A MusicXML score's time
  signatures are its own `<time>` elements and are marked as notated.
- The measure and beat are the file's: the analyzer does not check them against a printed score.

See [analysis-document.md](analysis-document.md) for the fields.
