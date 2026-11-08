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
| C5 | [method] Chords are read from the pitch classes sounding in each adaptive segment, ranked among the diatonic triads and sevenths of the region's key: a segment holding only a melody note (a sparse left hand), an alternating or ornamented texture, or a chord outside the vocabulary is read as another chord or none, so the V-I that the music has is hidden and a V-I the music does not have can appear | Chord identification with context (non-chord-tone filtering, harmonic continuity across melody-only segments, non-diatonic chords) | L8.5 |
| C6 | [method] A bass that is not a member of the chord reads as root position (`roman_numeral_adapter.h`); the segment's bass is its lowest sounding pitch, so a neighbour or passing bass tone turns a dominant seventh that is prolonged in an inversion into a root-position V7 (and a tonic with an ornamental bass into I6) | Inversion read from the chord's bass tone when the lowest pitch is ornamental | L8.6 |
| C8 | [method] L3.6 builds phrases, themes and expansions from the cadences it is given and has no cadence detection of its own: a phrase ends at every goal cadence, so a missing cadence drops the phrase end and the themes and expansions built on it, and a false one cuts a short phrase. Re-verified in P1 B8: every remaining phrase, theme, expansion and return deviation traces to a cadence the upstream stages did not give (C1, C4, C5, C6); no L3.6 rule was found wrong. Rows carry C8 where the theme, expansion or return cannot be built until those cadences are right | Re-verify L3.6 again once the cadence causes (C1, C4, C5, C6) are removed | L8.7 |

## Deviations

| ID | Piece | Area | Location | Expected | Actual | Cause |
|---|---|---|---|---|---|---|
| DEV-003 | bach114 | keys | region 26-28 | D major 26-28 | G major 21-26, D major 27-30 | C1 |
| DEV-004 | bach114 | keys | region 29-32 | G major 29-32 | D major 27-30, G major 31-32 | C1 |
| DEV-006 | bach114 | keys | change 29 | weak-modulation return at 29 | none | C1 |
| DEV-007 | bach114 | cadences | m4 b3 | hc | none | C4 |
| DEV-008 | bach114 | cadences | m8 b2.5 | pac | none | C5 |
| DEV-009 | bach114 | cadences | m12 b3 | hc | none | C4 |
| DEV-010 | bach114 | cadences | m16 b2.5 | pac | none | C5 |
| DEV-012 | bach114 | cadences | m24 b2.5 | pac | none | C5 |
| DEV-014 | bach114 | cadences | m32 b2.5 | pac | none | C5 |
| DEV-016 | bach114 | cadences | extra m31 b1 | none | subverted | C5 |
| DEV-017 | bach114 | phrase-form | end m4 | hc at m4 | none | C4 |
| DEV-018 | bach114 | phrase-form | end m12 | hc at m12 | none | C4 |
| DEV-020 | bach114 | phrase-form | end m24 | pac at m24 | none | C5 |
| DEV-021 | bach114 | phrase-form | theme period 1-8 | period 1-8 parts 1-4 5-8 | none | C4,C5,C8 |
| DEV-022 | bach114 | phrase-form | theme period 9-16 | period 9-16 parts 9-12 13-16 | hybrid 9-20 parts 9-12 13-20 | C5,C8 |
| DEV-023 | mozart545 | keys | region 1-12 | C major 1-12 | C major 1-8, F major 9-11, G major 12-21 | C1,C2 |
| DEV-024 | mozart545 | keys | region 13-28 | G major 13-28 | G major 12-21, A minor 22-24, G major 25-28 | C1,C2 |
| DEV-025 | mozart545 | keys | region 29-40 | C major 29-40 | C major 29-36, F major 37-39, G major 40-49 | C1,C2 |
| DEV-026 | mozart545 | keys | region 41-56 | G major 41-56 | G major 40-49, A minor 50-52, G major 53-61 | C1,C2 |
| DEV-104 | mozart545 | keys | region 57-69 | G minor 57-69 | G major 53-61, D minor 62-65, A minor 66-68, F major 69-84 | C1,C2 |
| DEV-027 | mozart545 | keys | region 70-78 | F major 70-78 | F major 69-84 | C1 |
| DEV-028 | mozart545 | keys | region 79-101 | C major 79-101 | F major 69-84, G major 85-86, E minor 87-90, C major 91-94, D minor 95-98, C major 99-102 | C1,C2 |
| DEV-105 | mozart545 | keys | region 102-114 | G minor 102-114 | C major 99-102, G minor 103-106, D minor 107-110, A minor 111-113, F major 114-129 | C1,C2 |
| DEV-029 | mozart545 | keys | region 115-123 | F major 115-123 | F major 114-129 | C1 |
| DEV-030 | mozart545 | keys | region 124-146 | C major 124-146 | F major 114-129, G major 130-131, E minor 132-135, C major 136-139, D minor 140-143, C major 144-146 | C1,C2 |
| DEV-031 | mozart545 | keys | change 79 | modulation at 79 | none | C1,C2 |
| DEV-106 | mozart545 | keys | change 57 | modulation at 57 | none | C1,C2 |
| DEV-032 | mozart545 | keys | change 124 | modulation at 124 | none | C1,C2 |
| DEV-033 | mozart545 | cadences | m12 b1 | hc | none | C5 |
| DEV-103 | mozart545 | cadences | readings m4 b1 | subverted m4 or prolongation m4 | none | C5 |
| DEV-041 | clementi36 | keys | region 16-23 | C minor 16-23 | C minor 15-19, G major 20-23 | C1,C2 |
| DEV-043 | clementi36 | keys | change 16 | modulation at 16 | weak-modulation at 15 | C3 |
| DEV-044 | clementi36 | cadences | m8 b1 | pac | none | C1,C5 |
| DEV-045 | clementi36 | cadences | m15 b1 | pac | none | C5 |
| DEV-102 | clementi36 | cadences | readings m23 b1 | hc m23 or pac m24 | pac at m24 b1 (750), or hc at m23 b4.5 (250) | C1,C2 |
| DEV-047 | clementi36 | cadences | m38 b1 | pac | none | C5 |
| DEV-048 | clementi36 | cadences | extra m2 b1 | none | pac | C5 |
| DEV-049 | clementi36 | cadences | extra m2 b3 | none | hc | C5 |
| DEV-051 | clementi36 | cadences | extra m10 b1 | none | iac | C5 |
| DEV-053 | clementi36 | cadences | extra m14 b3 | none | hc | C5 |
| DEV-055 | clementi36 | cadences | extra m25 b1 | none | pac | C5 |
| DEV-056 | clementi36 | cadences | extra m25 b3 | none | hc | C5 |
| DEV-058 | clementi36 | cadences | extra m33 b1 | none | iac | C5 |
| DEV-060 | clementi36 | cadences | extra m37 b3 | none | subverted | C5 |
| DEV-061 | clementi36 | phrase-form | end m8 | pac at m8 | none | C1,C5 |
| DEV-063 | clementi36 | phrase-form | theme period 1-8 | period 1-8 parts 1-4 5-8 | hybrid 5-10 parts 5-8 9-10 | C1,C5,C8 |
| DEV-064 | clementi36 | phrase-form | theme period 24-31 | period 24-31 parts 24-27 28-31 | period 15-24 parts 15-19 20-24, hybrid 28-33 parts 28-31 32-33 | C5,C8 |
| DEV-066 | field1 | cadences | m8 b1 | pac | none | C5,C6 |
| DEV-067 | field1 | cadences | m12 b1 | iac | none | C5,C6 |
| DEV-068 | field1 | cadences | m16 b1 | pac | none | C5,C6 |
| DEV-070 | field1 | cadences | extra m3 b1 | none | hc | C5 |
| DEV-071 | field1 | cadences | extra m7 b1 | none | hc | C5 |
| DEV-072 | field1 | phrase-form | end m12 | iac at m12 | none | C5,C6 |
| DEV-073 | field1 | phrase-form | theme repeated-phrase 1-8 | repeated-phrase 1-8 parts 1-4 5-8 | none | C5,C6,C8 |
| DEV-074 | field1 | phrase-form | theme period 9-16 | period 9-16 parts 9-12 13-16 | none | C5,C6,C8 |
| DEV-075 | field5 | keys | region 1-8 | Bb major 1-8 | Bb major 1-5, D minor 6-8 | C1,C2 |
| DEV-076 | mozart545 | returns | main m70 | m70 resembles m1 | no theme there (themes: m22-27, m50-55, m95-100, m140-145) | C5,C8 |
| DEV-077 | mozart545 | returns | main m115 | m115 resembles m1 | no theme there (themes: m22-27, m50-55, m95-100, m140-145) | C5,C8 |
| DEV-078 | mozart545 | returns | second m42 | m42 resembles m14 | no theme there (themes: m22-27, m50-55, m95-100, m140-145) | C5,C8 |
| DEV-079 | mozart545 | returns | second m87 | m87 resembles m14 | no theme there (themes: m22-27, m50-55, m95-100, m140-145) | C5,C8 |
| DEV-080 | mozart545 | returns | second m132 | m132 resembles m14 | no theme there (themes: m22-27, m50-55, m95-100, m140-145) | C5,C8 |
| DEV-081 | clementi36 | returns | main m24 | m24 resembles m1 | no theme there (themes: m5-10, m15-24, m28-33) | C5,C8 |
| DEV-082 | clementi36 | returns | second m32 | m32 resembles m9 | no theme there (themes: m5-10, m15-24, m28-33) | C5,C8 |
| DEV-083 | clementi36 | cadences | extra m19 b1 | none | hc | C5 |
| DEV-087 | mozart545 | returns | main m29 | m29 resembles m1 | no theme there (themes: m22-27, m50-55, m95-100, m140-145) | C5,C8 |
| DEV-088 | bach114 | phrase-form | end m16 | pac at m16 | none | C5 |
| DEV-089 | bach114 | phrase-form | end m32 | pac at m32 | none | C5 |
| DEV-090 | clementi36 | cadences | extra m38 b2.5 | none | pac | C4 |
| DEV-093 | mozart545 | keys | change 13 | modulation at 13 | tonicization at 12 | C5 |
| DEV-094 | mozart545 | keys | change 29 | modulation at 29 | tonicization at 29 | C5 |
| DEV-095 | mozart545 | keys | change 41 | modulation at 41 | tonicization at 40 | C5 |
| DEV-097 | clementi36 | cadences | m31 b1 | pac | none | C5 |
| DEV-098 | clementi36 | phrase-form | end m31 | pac at m31 | none | C5 |
