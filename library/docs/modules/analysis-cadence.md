# Analysis: cadences

CadenceDetection and CadenceClassification in the kernel `analysis` module. [Docs index](../README.md)

**`CadenceDetection` / `CadenceClassification` -- real root-motion rules, explicit caller-supplied refinements, no false-confidence guesses:**

`CadenceDetection` (`analysis/cadence_detection.h`) is the generic half: given a region and an already-weighted `UncertaintyCandidate` set, it picks the top candidate and reports it as a single `Interpretation` targeting the region (an existing L5 `Cadence` shell, `structure/cadence.h`) -- the same generic-Interpretation pattern `ChordIdentification`/`Motif`/`Theme`/`PhraseOccurrence` use; `CadenceDetection` does not itself get a new named output type. It now optionally accepts a `Framework`, threading a real `FrameworkReference` (e.g. to common-practice-minimal) into the Interpretation when supplied.

`CadenceClassification` (`analysis/cadence_classification.h`) is the real classifier: it builds that candidate set from chord ROOT MOTION -- each chord's scale-degree root (1-7), the same real, already-computed dimension the caller supplies to `ChordIdentification` itself, not a re-parse of chord label strings (a seventh chord's label, e.g. `"dominant7"`, carries no root information at all). Its rules:

- V(7) -> I root motion: authentic-cadence candidate.
- IV -> I: plagal-cadence candidate.
- ends on V: half-cadence (HC) candidate.
- V -> vi: deceptive-cadence candidate.
- anything else: a single `CADENCE_TYPE_UNKNOWN` candidate at full weight -- no guess.

Every candidate is a `CadenceType` (`semantics/cadence_type.h`), the one cadence vocabulary; the label the `Interpretation` carries comes from `cadence_type_label`, never from a hand-written string.

What's REAL vs. CALLER-SUPPLIED-REFINEMENT vs. UNCLASSIFIED, the same transparency standard `VoiceLeadingAnalysis`'s docs use:

- Real: the from/to root-degree motion driving which rule fires.
- Optional caller-supplied refinement (never required): `from_is_dominant_seventh` and Voice Leading's bass-motion classification (`analysis/voice_leading_analysis.h`) each only adjust the winning candidate's confidence tier within the authentic-cadence rule -- a caller with no Voice Leading data still gets a classification, just a lower-confidence one.
- Optional caller-supplied refinement that changes the LABEL, not just its weight: `soprano_resolves_to_tonic`, distinguishing PAC from IAC. `CadenceClassification` itself does not find the soprano: when the flag is absent, the authentic-cadence candidate is reported as `CADENCE_TYPE_AUTHENTIC_UNSPECIFIED` rather than guessing PAC or IAC. The real-music path is the standard-harmony analysis adapter (`frameworks/standard-harmony/analysis/analysis_cadence.h`, `docs/modules/standard-harmony-analysis.md`): it reads the soprano scale degree at the arrival from the outer voices, so PAC and IAC are distinguished there, and `AUTHENTIC_UNSPECIFIED` remains only when the soprano is absent at the arrival (with an uncertainty penalty). `CadenceClassification` and period detection are unchanged.

Competing readings (C7, C9; R-090, R-091, R-093, R-094): the adapter reports two readings, with shares in thousandths that sum to 1000, at two places. A root-position V(7) followed by I6 at a cadence point is a `SUBVERTED` (evaded) cadence or a prolongation of the dominant; a root-position V directly before a section start is a half cadence of the earlier section or the authentic cadence (PAC, IAC or unspecified) into the tonic that starts the next, which replaces the single half-cadence reading of R-038. Each is one `HarmonyCadenceResult` whose `share` is the primary reading's and whose `alternative` carries the other, with its own Interpretation (id of the primary's plus `-alt`, same Cadence target); the claims carry `reading`, `rank` and `share`. The shares are scored from evidence (a strong-beat arrival, a cadential approach, a following authentic cadence, a long dominant, a soprano rest, a PAC-shaped arrival) by weights in `HarmonyCadenceConfig` (`analysis_cadence_readings.h`), clamped to [50, 950]. The primary reading is the only one the accuracy harness and the phrase stage count; phrase-unit position is not used as evidence because phrases are built after cadences (L3.6).
