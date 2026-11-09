# Expected deviations

The Framework v1 verification (`tests/integration/framework_v1_verification.c`, `framework_v1_thematic_returns.c`) checks the full chain on the five reference pieces against
`tests/fixtures/annotations/<piece>_v1_expected.txt`. A location where the chain does not give the expected value is a **deviation**, listed
below with the exact value the chain gives (the **actual** column) and its cause. The tests READ this file: a check that still deviates must match its row
exactly (expected, actual, cause known); a check that now passes while its row is still here fails; a row no check uses fails. Nothing is skipped, no threshold is
chosen, and no default is tuned to a piece. See `tests/integration/framework_v1_deviations.h`.

The expected values are **verified** (Michelle Opitz, 2026-10-17: read from the MIDI first and checked against standard theory and a printed edition, R-092), so a
deviation is never a draft reading to be corrected: the only cause kind left is a **method** limit (a documented or demonstrable limit of an L2/L3 stage, with the
roadmap item that would remove it). The two differences of convention that were listed before (a V to I6, and a dominant resolving into the recapitulation) are
resolved: the chain reports both readings of those places (R-090, R-091) and the expected files carry them as `readings` lines.

Every cause names its follow-up in the **Roadmap ID** column (R-095).

## Causes

| ID | Limit | Roadmap item | Roadmap ID |
|---|---|---|---|
| C1 | [method] Key regions come from 4-measure windows scored against pitch-class profiles with a change penalty (`key_regions.h`, REGION PATH); a window labels the measure at its centre, so a region's edge lands up to two measures from the barline where the music changes, and a one- or two-measure excursion is stretched to a window or absorbed | Key regions placed on harmonic evidence (cadence- and pivot-informed boundaries) | L8.1 |
| C2 | [method] The key of a window is the best correlation of its pooled pitch-class weights with the 24 profiles; closely related keys (the dominant, the subdominant, the relative minor) and passages of scales or a dominant pedal are read as the neighbouring key | Harmony-aware key estimation (the key from chord functions, not pitch-class weights alone) | L8.2 |
| C3 | [method] The extent of a key change is read from region lengths and return, then confirmed by the cadences found (`harmony_key_change_confirm`); a change whose cadence is not found, or whose region edge is off, still reads wrongly | Region length rule confirmed by cadences (B5) | L8.3 |
| C4 | [method] A cadence arriving on a weak beat is found only as a tonic after a root-position dominant on a stronger beat (or on the beat against a tonic off it) with a phrase end signal (`allow_weak_arrival`, `analysis_cadence.h`); a half cadence arriving on a weak beat is not found, and a closing V-I tag after the cadence of the draft is read as a second cadence | Half cadences on weak beats; the draft cadence list covering closing tags | L8.4 |
| C5 | [method] Chords are read (on harmonic spans since P39: one chord per span, a beat to a measure) from the pitch classes sounding in each span, ranked among the diatonic triads and sevenths of the region's key: a segment holding only a melody note (a sparse left hand), an alternating or ornamented texture, or a chord outside the vocabulary is read as another chord or none, so the V-I that the music has is hidden and a V-I the music does not have can appear | Chord identification with context (non-chord-tone filtering, harmonic continuity across melody-only segments, non-diatonic chords) | L8.5 |
| C6 | [method] A bass that is not a member of the chord reads as root position (`roman_numeral_adapter.h`); the bass is the lowest sounding pitch of the span's first cell, or the step an accented bass tone resolves to (R-373), so a neighbour or passing bass tone turns a dominant seventh that is prolonged in an inversion into a root-position V7 (and a tonic with an ornamental bass into I6) | Inversion read from the chord's bass tone when the lowest pitch is ornamental | L8.6 |
| C8 | [method] L3.6 builds phrases, themes and expansions from the cadences it is given and has no cadence detection of its own: a phrase ends at every goal cadence, so a missing cadence drops the phrase end and the themes and expansions built on it, and a false one cuts a short phrase. Re-verified in P1 B8: every remaining phrase, theme, expansion and return deviation traces to a cadence the upstream stages did not give (C1, C4, C5, C6); no L3.6 rule was found wrong. Rows carry C8 where the theme, expansion or return cannot be built until those cadences are right | Re-verify L3.6 again once the cadence causes (C1, C4, C5, C6) are removed | L8.7 |

## Deviations

| ID | Piece | Area | Location | Expected | Actual | Cause |
|---|---|---|---|---|---|---|
| DEV-003 | bach114 | keys | region 26-28 | D major 26-28 | G major 1-32 | C1 |
| DEV-004 | bach114 | keys | region 29-32 | G major 29-32 | G major 1-32 | C1 |
| DEV-006 | bach114 | keys | change 29 | weak-modulation return at 29 | none | C1 |
| DEV-007 | bach114 | cadences | m4 b3 | hc | none | C4 |
| DEV-008 | bach114 | cadences | m8 b2.5 | pac | none | C5 |
| DEV-010 | bach114 | cadences | m16 b2.5 | pac | none | C5 |
| DEV-012 | bach114 | cadences | m24 b2.5 | pac | none | C5 |
| DEV-014 | bach114 | cadences | m32 b2.5 | pac | none | C5 |
| DEV-017 | bach114 | phrase-form | end m4 | hc at m4 | none | C4 |
| DEV-021 | bach114 | phrase-form | theme period 1-8 | period 1-8 parts 1-4 5-8 | none | C4,C5,C8 |
| DEV-022 | bach114 | phrase-form | theme period 9-16 | period 9-16 parts 9-12 13-16 | none | C5,C8 |
| DEV-104 | mozart545 | keys | region 57-69 | G minor 57-69 | G minor 57-59, D minor 60-63, F major 64-77 | C1,C2 |
| DEV-105 | mozart545 | keys | region 102-114 | G minor 102-114 | G minor 102-104, D minor 105-108, F major 109-122 | C1,C2 |
| DEV-103 | mozart545 | cadences | readings m4 b1 | subverted m4 or prolongation m4 | none | C5 |
| DEV-045 | clementi36 | cadences | m15 b1 | pac | none | C5 |
| DEV-047 | clementi36 | cadences | m38 b1 | pac | none | C5 |
| DEV-064 | clementi36 | phrase-form | theme period 24-31 | period 24-31 parts 24-27 28-31 | none | C5,C8 |
| DEV-067 | field1 | cadences | m12 b1 | iac | none | C5,C6 |
| DEV-068 | field1 | cadences | m16 b1 | pac | none | C5,C6 |
| DEV-073 | field1 | phrase-form | theme repeated-phrase 1-8 | repeated-phrase 1-8 parts 1-4 5-8 | hybrid 1-8 parts 1-4 5-8 | C5,C6,C8 |
| DEV-074 | field1 | phrase-form | theme period 9-16 | period 9-16 parts 9-12 13-16 | none | C5,C6,C8 |
| DEV-081 | clementi36 | returns | main m24 | m24 resembles m1 | no theme there (themes: m1-8, m9-23) | C5,C8 |
| DEV-082 | clementi36 | returns | second m32 | m32 resembles m9 | no theme there (themes: m1-8, m9-23) | C5,C8 |
| DEV-093 | mozart545 | keys | change 13 | modulation at 13 | weak-modulation at 13 | C5 |
| DEV-094 | mozart545 | keys | change 29 | modulation at 29 | weak-modulation return at 29 | C5 |
| DEV-095 | mozart545 | keys | change 41 | modulation at 41 | weak-modulation return at 41 | C5 |
| DEV-097 | clementi36 | cadences | m31 b1 | pac | none | C5 |
| DEV-098 | clementi36 | phrase-form | end m31 | pac at m31 | none | C5 |
| DEV-107 | bach114 | cadences | m20 b2.5 | pac | none | C5 |
| DEV-108 | bach114 | cadences | m28 b2.5 | pac | none | C5 |
| DEV-121 | clementi36 | cadences | readings m12 b1 | subverted m12 or prolongation m12 | none | C5 |
| DEV-122 | clementi36 | cadences | readings m35 b1 | subverted m35 or prolongation m35 | none | C5 |
| DEV-129 | clementi36 | phrase-form | end m15 | pac at m15 | none | C5 |
| DEV-161 | field1 | phrase-form | end m12 | iac at m12 | none | C5 |
| DEV-163 | bach114 | keys | region 18-20 | D major 18-20 | G major 1-32 | C1,C2 |
| DEV-164 | bach114 | keys | region 21-25 | G major 21-25 | G major 1-32 | C1,C2 |
| DEV-165 | bach114 | keys | change 18 | weak-modulation at 18 | none | C1 |
| DEV-166 | bach114 | keys | change 21 | weak-modulation return at 21 | none | C1 |
| DEV-167 | bach114 | keys | change 26 | weak-modulation at 26 | none | C1 |
| DEV-170 | bach114 | phrase-form | end m28 | pac at m28 | none | C5 |
| DEV-185 | bach114 | phrase-form | end m12 | hc at m12 | none | C5 |
| DEV-186 | bach114 | phrase-form | end m20 | pac at m20 | none | C5 |
| DEV-188 | clementi36 | cadences | m8 b1 | pac | iac at m8 b1 | C5 |
| DEV-192 | bach114 | cadences | m12 b3 | hc | none | C5 |
| DEV-194 | field1 | cadences | m4 b1 | pac | none | C5 |
| DEV-200 | bach114 | phrase-form | end m16 | pac at m16 | none | C5 |
| DEV-201 | field1 | phrase-form | end m4 | pac at m4 | none | C5 |
| DEV-202 | bach114 | keys | region 1-17 | G major 1-17 | G major 1-32 | C1 |
| DEV-203 | mozart545 | keys | region 70-78 | F major 70-78 | F major 64-77, C major 78-101 | C2 |
| DEV-204 | mozart545 | keys | region 115-123 | F major 115-123 | F major 109-122, C major 123-146 | C2 |
