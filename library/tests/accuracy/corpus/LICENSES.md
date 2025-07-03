# Corpus licenses

**Status: private use only. Review or replace before publishing, sharing or commercializing this repository.**

Every file under `tests/accuracy/corpus/` is third-party material, copied byte for byte (nothing edited). It is
test data for the accuracy harness only and is not part of the library. The music (J. S. Bach, died 1750) is in the
public domain; the encodings and analyses below are separate works with the status recorded here. Nothing with a
known non-commercial or no-derivatives license is included (the Chorale-Corpus and Sapp kern editions, CC BY-NC-SA,
and the BCMH reductions derived from them, were left out for that reason). The corpus can be swapped for other
material with `accuracy_harness --corpus-dir`; a different corpus needs its own baseline.

Status words: **stated** means the source states a license that covers the file; **unclear** means the source gives
no license for that file or its origin, only a general statement.

## Sources

### Scores: music21 corpus

- Source: https://github.com/cuthbertLab/music21 at commit `9ad11788e7cfacb9a0fddf271409f07292e2206a`, path `music21/corpus/bach/<file>.mxl`.
  Each `.mxl` is a zip; the one MusicXML document inside is committed as `score.musicxml`, unmodified.
- License as stated by the source (README, "License Coverage and Other/Prior Licenses"): music21 code is BSD
  3-clause; "the encoded musical scores in the corpus have their own copyrights and licenses. The underlying music is
  believed to be in the public domain ... and the encodings are also either public domain or used by permission."
  No per-file statement exists, and the files carry no `<rights>` element.
- Status: **unclear** (public domain or used by permission, not stated per file).

### Analyses: When in Rome

- Source: https://github.com/MarkGotham/When-in-Rome at commit `1c61fe41b8c2910296d7d2bcbf6476c7c1f2fe35`, path
  `Corpus/Early_Choral/Bach,_Johann_Sebastian/Chorales/<number>/analysis.txt`, committed as `analysis.rntxt`,
  unmodified.
- License as stated by the source (README, "Licence"): new content, including converted analyses, is CC BY-SA 4.0;
  "for analyses that originated elsewhere and have been converted into the format used here, please refer to the
  original source for licence." These analyses are credited to Andrew Jones and Dmitri Tymoczko (proofreaders
  Dmitri Tymoczko and Hamish Robb); the original source is the `riemenschneider<number>.txt` set named in each
  chorale's `remote.json`, whose license is not stated.
- Status: **unclear** (CC BY-SA 4.0 for the repository's own content; origin license not stated).
- CC BY-SA 4.0 requires attribution and that adaptations be shared alike. Attribution: "When in Rome", Mark Gotham
  and contributors; analysts as credited in each file's header.

## Files

| Directory | BWV | Title | Score file (music21) | Analyst / proofreader | Score status | Analysis status |
|---|---|---|---|---|---|---|
| `chorale-001-bwv269/` | 269 | Aus meines Herzens Grunde | `bwv269.mxl` | Andrew Jones / Dmitri Tymoczko and Hamish Robb | unclear | unclear |
| `chorale-002-bwv347/` | 347 | Ich dank dir, lieber Herre | `bwv347.mxl` | Andrew Jones / Dmitri Tymoczko and Hamish Robb | unclear | unclear |
| `chorale-003-bwv153-1/` | 153.1 | Schau, lieber Gott, wie meine Feind (1) | `bwv153.1.mxl` | Andrew Jones / Dmitri Tymoczko and Hamish Robb | unclear | unclear |
| `chorale-005-bwv267/` | 267 | An Wasserflüssen Babylon (for B308) | `bwv267.mxl` | Andrew Jones / Dmitri Tymoczko and Hamish Robb | unclear | unclear |
| `chorale-006-bwv281/` | 281 | Christus, der ist mein Leben | `bwv281.mxl` | Andrew Jones / Dmitri Tymoczko and Hamish Robb | unclear | unclear |
| `chorale-007-bwv17-7/` | 17.7 | Nun lob, mein Seel, den Herren (3) | `bwv17.7.mxl` | Dmitri Tymoczko / Hamish Robb | unclear | unclear |
| `chorale-010-bwv38-6/` | 38.6 | Aus tiefer Not schrei ich zu dir (5) | `bwv38.6.mxl` | Dmitri Tymoczko / Hamish Robb | unclear | unclear |
| `chorale-012-bwv65-2/` | 65.2 | Ein Kind geborn zu Bethlehem (4) | `bwv65.2.mxl` | Dmitri Tymoczko / Hamish Robb | unclear | unclear |
| `chorale-013-bwv33-6/` | 33.6 | Allein zu dir, Herr Jesu Christ (4) | `bwv33.6.mxl` | Dmitri Tymoczko / Hamish Robb | unclear | unclear |
| `chorale-014-bwv184-5/` | 184.5 | O Herre Gott, dein göttlich Wort (8) | `bwv184.5.mxl` | Dmitri Tymoczko / Hamish Robb | unclear | unclear |

Per directory: `score.musicxml` is the score (source above), `analysis.rntxt` is the analysis (source above).
