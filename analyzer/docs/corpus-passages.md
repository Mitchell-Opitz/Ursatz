# Corpus passages

The passages of an analyzed corpus, ready to give to the Composer. Back to the [docs index](README.md).

Library: `include/ursatz_analyzer/corpus_passages.h`, part of the `ursatz_analyzer` target. R-619.

The Composer API seeds a motive or a progression from passages given inline in a request (`corpus_passages`, the
[schema](../../composer/schema/api/corpus-passages.schema.json)); it does not search a corpus. This is how a caller (the
Composer CLI, a GUI) gets them. The search is the Analyzer's ([similar-material.md](similar-material.md)), with its ranking
and a minimum similarity of 1/2, and it reads only the analysis documents of the corpus (R-079). Each match's document is read once
more for what the Composer needs and the search does not keep: the chords and whether the document is current.

```c
AnalyzerCorpusPassages found;
UrsatzStatus status = analyzer_corpus_passages_like(corpus_dir, knowledge, "passage.mid", 10, &found);
/* or analyzer_corpus_passages_numerals(corpus_dir, knowledge, "I IV V I", 100, &found) */
char *json = analyzer_corpus_passages_to_json(&found); /* the request's corpus_passages */
analyzer_corpus_passages_destroy(&found);
```

| Entry | What it finds | The Composer asks for |
| --- | --- | --- |
| `analyzer_corpus_passages_like` | passages that look like the melody of a MIDI or MusicXML file | the best 10 |
| `analyzer_corpus_passages_numerals` | passages that look like a progression of Roman numerals | the best 100 |

`AnalyzerCorpusPassage` holds the piece's path in the corpus, where the passage starts and ends (a measure and a beat), its
`similarity` (a `Rational`), its soprano notes (MIDI number, onset and duration as `Rational` whole notes, onsets from the
passage's start), the chords of the piece in time order and `stale`. A chord is the joined chord of the document
(`analyzer_chord_sequence_from_document`'s sequence, one numeral per run of entries) with the place and key label of the entry it
starts at, so a reader finds the chords of the passage from the one that starts where it does, and the key it was heard in from the
last chord at or before it. `stale` is true when the document was analyzed by another version ([corpus.md](corpus.md)); it is
read all the same. A passage whose notes do not fit a `Rational` has no notes.

No match is `URSATZ_OK` with no passage. `URSATZ_ERR_REQUEST` is a passage file or a numerals text that cannot
be read as a query (fewer than two notes or numerals, a file that is neither MIDI nor MusicXML), `URSATZ_ERR_NOT_FOUND` a
corpus output directory that cannot be read, `URSATZ_ERR_REQUEST` a NULL argument or a `max` of 0.

`analyzer_corpus_passages_to_json` writes the array the Composer API reads: each passage `{"piece", "start": {"measure", "beat"},
"end": {...}, "similarity": {"num", "den"}, "stale", "notes": [{"midi", "onset", "duration"}], "chords": [{"numeral", "measure",
"beat", "key"}]}`. Test: `test_corpus_passages` (ranking, chords, stale, no match, the refusals, the JSON).
