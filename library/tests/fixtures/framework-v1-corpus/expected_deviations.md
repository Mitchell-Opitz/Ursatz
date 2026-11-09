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
| DEV-003 | bach114 | keys | region 26-28 | D major 26-28 | G major 24-26, D major 27-30 | C1 |
| DEV-004 | bach114 | keys | region 29-32 | G major 29-32 | D major 27-30, G major 31-32 | C1 |
| DEV-006 | bach114 | keys | change 29 | weak-modulation return at 29 | none | C1 |
| DEV-007 | bach114 | cadences | m4 b3 | hc | none | C4 |
| DEV-008 | bach114 | cadences | m8 b2.5 | pac | none | C5 |
| DEV-010 | bach114 | cadences | m16 b2.5 | pac | none | C5 |
| DEV-012 | bach114 | cadences | m24 b2.5 | pac | none | C5 |
| DEV-014 | bach114 | cadences | m32 b2.5 | pac | none | C5 |
| DEV-017 | bach114 | phrase-form | end m4 | hc at m4 | none | C4 |
| DEV-021 | bach114 | phrase-form | theme period 1-8 | period 1-8 parts 1-4 5-8 | none | C4,C5,C8 |
| DEV-022 | bach114 | phrase-form | theme period 9-16 | period 9-16 parts 9-12 13-16 | none | C5,C8 |
| DEV-024 | mozart545 | keys | region 13-28 | G major 13-28 | C major 1-13, G major 14-21, A minor 22-24, G major 25-28 | C1,C2 |
| DEV-026 | mozart545 | keys | region 41-56 | G major 41-56 | C major 29-41, G major 42-49, A minor 50-52, G major 53-59 | C1,C2 |
| DEV-104 | mozart545 | keys | region 57-69 | G minor 57-69 | G major 53-59, D minor 60-63, A minor 64-68, F major 69-79 | C1,C2 |
| DEV-028 | mozart545 | keys | region 79-101 | C major 79-101 | F major 69-79, C major 80-94, D minor 95-97, C major 98-103 | C1,C2 |
| DEV-105 | mozart545 | keys | region 102-114 | G minor 102-114 | C major 98-103, G minor 104-105, D minor 106-108, A minor 109-113, F major 114-124 | C1,C2 |
| DEV-030 | mozart545 | keys | region 124-146 | C major 124-146 | F major 114-124, C major 125-139, D minor 140-142, C major 143-146 | C1,C2 |
| DEV-106 | mozart545 | keys | change 57 | modulation at 57 | none | C1,C2 |
| DEV-033 | mozart545 | cadences | m12 b1 | hc | none | C5 |
| DEV-103 | mozart545 | cadences | readings m4 b1 | subverted m4 or prolongation m4 | none | C5 |
| DEV-041 | clementi36 | keys | region 16-23 | C minor 16-23 | C minor 16-19, G major 20-22, C major 23-38 | C1,C2 |
| DEV-045 | clementi36 | cadences | m15 b1 | pac | none | C5 |
| DEV-102 | clementi36 | cadences | readings m23 b1 | hc m23 or pac m24 | pac at m24 b1 (1000), no alternative | C1,C2 |
| DEV-047 | clementi36 | cadences | m38 b1 | pac | none | C5 |
| DEV-063 | clementi36 | phrase-form | theme period 1-8 | period 1-8 parts 1-4 5-8 | hybrid 1-6 parts 1-4 5-6 | C1,C5,C8 |
| DEV-064 | clementi36 | phrase-form | theme period 24-31 | period 24-31 parts 24-27 28-31 | hybrid 20-25 parts 20-23 24-25 | C5,C8 |
| DEV-067 | field1 | cadences | m12 b1 | iac | none | C5,C6 |
| DEV-068 | field1 | cadences | m16 b1 | pac | none | C5,C6 |
| DEV-073 | field1 | phrase-form | theme repeated-phrase 1-8 | repeated-phrase 1-8 parts 1-4 5-8 | hybrid 4-9 parts 4-7 8-9 | C5,C6,C8 |
| DEV-074 | field1 | phrase-form | theme period 9-16 | period 9-16 parts 9-12 13-16 | hybrid 4-9 parts 4-7 8-9, hybrid 10-15 parts 10-13 14-15 | C5,C6,C8 |
| DEV-075 | field5 | keys | region 1-8 | Bb major 1-8 | Bb major 1-4, D minor 5-8 | C1,C2 |
| DEV-078 | mozart545 | returns | second m42 | m42 resembles m14 | no theme there (themes: m1-11, m30-39, m64-70, m71-81, m109-115, m116-126) | C5,C8 |
| DEV-079 | mozart545 | returns | second m87 | m87 resembles m14 | no theme there (themes: m1-11, m30-39, m64-70, m71-81, m109-115, m116-126) | C5,C8 |
| DEV-080 | mozart545 | returns | second m132 | m132 resembles m14 | no theme there (themes: m1-11, m30-39, m64-70, m71-81, m109-115, m116-126) | C5,C8 |
| DEV-081 | clementi36 | returns | main m24 | m24 resembles m1 | no theme there (themes: m1-6, m9-19, m20-25) | C5,C8 |
| DEV-082 | clementi36 | returns | second m32 | m32 resembles m9 | no theme there (themes: m1-6, m9-19, m20-25) | C5,C8 |
| DEV-093 | mozart545 | keys | change 13 | modulation at 13 | tonicization at 14 | C5 |
| DEV-094 | mozart545 | keys | change 29 | modulation at 29 | weak-modulation at 29 | C5 |
| DEV-095 | mozart545 | keys | change 41 | modulation at 41 | weak-modulation return at 42 | C5 |
| DEV-097 | clementi36 | cadences | m31 b1 | pac | none | C5 |
| DEV-098 | clementi36 | phrase-form | end m31 | pac at m31 | none | C5 |
| DEV-107 | bach114 | cadences | m20 b2.5 | pac | none | C5 |
| DEV-108 | bach114 | cadences | m28 b2.5 | pac | none | C5 |
| DEV-113 | mozart545 | keys | change 102 | modulation at 102 | none | C1,C2 |
| DEV-121 | clementi36 | cadences | readings m12 b1 | subverted m12 or prolongation m12 | none | C5 |
| DEV-122 | clementi36 | cadences | readings m35 b1 | subverted m35 or prolongation m35 | none | C5 |
| DEV-123 | clementi36 | cadences | m4 b1 | hc | none | C5 |
| DEV-124 | clementi36 | cadences | m27 b1 | hc | none | C5 |
| DEV-128 | clementi36 | phrase-form | end m4 | hc at m4 | none | C5 |
| DEV-129 | clementi36 | phrase-form | end m15 | pac at m15 | none | C5 |
| DEV-136 | field5 | cadences | none in scope | no cadence | 1 cadence(s) | C5 |
| DEV-148 | clementi36 | keys | region 8-15 | G major 8-15 | G major 7-11, C major 12-15 | C1,C2 |
| DEV-150 | clementi36 | keys | change 8 | modulation at 8 | weak-modulation at 7 | C1,C2 |
| DEV-156 | clementi36 | phrase-form | end m27 | hc at m27 | none | C5 |
| DEV-157 | field1 | cadences | extra m2 b1 | none | pac | C5 |
| DEV-161 | field1 | phrase-form | end m12 | iac at m12 | none | C5 |
| DEV-163 | bach114 | keys | region 18-20 | D major 18-20 | D major 17-23 | C1,C2 |
| DEV-164 | bach114 | keys | region 21-25 | G major 21-25 | D major 17-23, G major 24-26 | C1,C2 |
| DEV-165 | bach114 | keys | change 18 | weak-modulation at 18 | tonicization at 17 | C1 |
| DEV-166 | bach114 | keys | change 21 | weak-modulation return at 21 | none | C1 |
| DEV-167 | bach114 | keys | change 26 | weak-modulation at 26 | tonicization return at 27 | C1 |
| DEV-170 | bach114 | phrase-form | end m28 | pac at m28 | none | C5 |
| DEV-185 | bach114 | phrase-form | end m12 | hc at m12 | none | C5 |
| DEV-186 | bach114 | phrase-form | end m20 | pac at m20 | none | C5 |
| DEV-187 | mozart545 | cadences | extra m11 b1 | none | hc | C5 |
| DEV-188 | clementi36 | cadences | m8 b1 | pac | iac at m8 b1 | C5 |
| DEV-189 | clementi36 | cadences | extra m5 b1 | none | pac | C5 |
| DEV-190 | clementi36 | cadences | extra m19 b1 | none | hc | C5 |
| DEV-191 | clementi36 | cadences | extra m28 b1 | none | pac | C5 |
| DEV-192 | bach114 | cadences | m12 b3 | hc | none | C5 |
| DEV-193 | bach114 | cadences | extra m31 b1 | none | hc | C5 |
| DEV-194 | field1 | cadences | m4 b1 | pac | none | C5 |
| DEV-195 | field1 | cadences | extra m3 b1 | none | hc | C5 |
| DEV-196 | field1 | cadences | extra m11 b3 | none | subverted | C5 |
| DEV-197 | field1 | cadences | extra m15 b1 | none | hc | C5 |
| DEV-198 | field5 | cadences | extra m6 b1 | none | other | C5 |
| DEV-199 | bach114 | phrase-form | end m24 | pac at m24 | none | C5 |
