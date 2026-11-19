/* The rhythm areas of the analysis document: rhythm and meter, and texture. */

#include "export/analysis_document_internal.h"

/* The line a density profile is of, as the document names it: "all" for every note of the phrase, then the outer-voice
   soprano and bass, and "other" for the rest. */
static const char *rhythm_line_name(int line) {
    switch (line) {
        case 0:
            return "soprano";
        case 1:
            return "bass";
        case 2:
            return "other";
        default:
            return "all";
    }
}

static cJSON *density_durations_json(DocBuild *build, const AnalysisDensity *density) {
    cJSON *list = cJSON_CreateArray();
    for (unsigned b = 0; list && b < density->bin_count; b++) {
        cJSON *bin = cJSON_CreateObject();
        if (!(bin &&
              json_add(bin, "duration",
                       doc_rational_json(doc_rational(build, (long)density->bins[b].value.num, (unsigned long)density->bins[b].value.den))) &&
              json_add(bin, "count", cJSON_CreateNumber((double)density->bins[b].count)) && json_append(list, bin))) {
            cJSON_Delete(bin);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static cJSON *density_per_beat_json(const AnalysisDensity *density) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "mean", cJSON_CreateNumber(density->attacks_per_beat_mean)) &&
                    json_add(object, "max", cJSON_CreateNumber(density->attacks_per_beat_max)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A density claim: the library's profile of a phrase's notes (all of them, or one line's), over the phrase's span. */
static cJSON *density_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, const AnalysisDensity *density) {
    const AnalysisPhrase *phrase = &report->phrases[density->phrase];
    Rational start = doc_rational(build, phrase->start.num, phrase->start.den);
    Rational end = doc_rational(build, phrase->end.num, phrase->end.den);
    cJSON *claim = doc_claim_json_with(build, density->framework, notes, density->note_ids, density->note_id_count, start, end,
                                   "density", cJSON_CreateNumber(density->uncertainty));
    if (!(claim && json_add(claim, "phrase", cJSON_CreateNumber((double)density->phrase)) &&
          json_add(claim, "measures", doc_measures_json(phrase->first_measure, phrase->last_measure)) &&
          json_add(claim, "line", cJSON_CreateString(rhythm_line_name(density->line))) &&
          json_add(claim, "notes", cJSON_CreateNumber((double)density->notes)) &&
          json_add(claim, "attacks", cJSON_CreateNumber((double)density->attacks)) &&
          json_add(claim, "measure_count", cJSON_CreateNumber(density->measures)) &&
          json_add(claim, "attacks_per_beat", density_per_beat_json(density)) &&
          json_add(claim, "mean_inter_onset", cJSON_CreateNumber(density->mean_inter_onset)) &&
          json_add(claim, "on_beat_share", cJSON_CreateNumber(density->on_beat_share)) &&
          json_add(claim, "off_beat_share", cJSON_CreateNumber(density->off_beat_share)) &&
          json_add(claim, "durations", density_durations_json(build, density)) &&
          json_add(claim, "durations_truncated", cJSON_CreateBool(density->bins_truncated)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *syncopation_through_json(const AnalysisSyncopation *syncopation) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "level", cJSON_CreateString(syncopation->through_level)) &&
                    json_add(object, "measure", cJSON_CreateNumber(syncopation->through_measure)) &&
                    json_add(object, "beat", cJSON_CreateNumber(syncopation->through_beat)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A syncopation claim: one note, attacked at a weaker level and held through a stronger one that no note of its line
   attacks. */
static cJSON *syncopation_json(DocBuild *build, const ImportedScore *notes, const AnalysisSyncopation *syncopation) {
    Rational start;
    Rational end;
    doc_notes_span(build, notes, &syncopation->note, 1, &start, &end);
    cJSON *claim = doc_claim_json_with(build, syncopation->framework, notes, &syncopation->note, 1, start, end, "syncopation",
                                   cJSON_CreateNumber(syncopation->uncertainty));
    if (!(claim && json_add(claim, "line", cJSON_CreateString(rhythm_line_name(syncopation->line))) &&
          json_add(claim, "measure", cJSON_CreateNumber(syncopation->measure)) &&
          json_add(claim, "beat", cJSON_CreateNumber(syncopation->beat)) &&
          json_add(claim, "attack_level", cJSON_CreateString(syncopation->attack_level)) &&
          json_add(claim, "strength", cJSON_CreateNumber((double)syncopation->strength)) &&
          json_add(claim, "held_through", syncopation_through_json(syncopation)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* An evidence value of a hemiola: true when it agrees, false when it does not, null when it is unavailable. */
static cJSON *evidence_json(int evidence) {
    return evidence < 0 ? cJSON_CreateNull() : cJSON_CreateBool(evidence != 0);
}

static cJSON *hemiola_evidence_json(const AnalysisHemiola *hemiola) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "duration", evidence_json(hemiola->duration_evidence)) &&
                    json_add(object, "chord", evidence_json(hemiola->chord_evidence)) &&
                    json_add(object, "accent", evidence_json(hemiola->accent_evidence)) &&
                    json_add(object, "agreeing", cJSON_CreateNumber((double)hemiola->agreeing)) &&
                    json_add(object, "available", cJSON_CreateNumber((double)hemiola->available)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *hemiola_json(DocBuild *build, const ImportedScore *notes, const AnalysisHemiola *hemiola) {
    Rational start;
    Rational end;
    doc_notes_span(build, notes, hemiola->note_ids, hemiola->note_id_count, &start, &end);
    cJSON *claim = doc_claim_json_with(build, hemiola->framework, notes, hemiola->note_ids, hemiola->note_id_count, start, end,
                                   "hemiola", cJSON_CreateNumber(hemiola->uncertainty));
    if (!(claim && json_add(claim, "measures", doc_measures_json(hemiola->first_measure, hemiola->last_measure)) &&
          json_add(claim, "evidence", hemiola_evidence_json(hemiola)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *hypermeasures_json(const AnalysisHypermeter *hypermeter) {
    cJSON *list = cJSON_CreateArray();
    for (size_t h = 0; list && h < hypermeter->hypermeasure_count; h++) {
        const AnalysisHypermeasure *hypermeasure = &hypermeter->hypermeasures[h];
        cJSON *item = cJSON_CreateObject();
        if (!(item && json_add(item, "index", cJSON_CreateNumber(hypermeasure->index)) &&
              json_add(item, "measures", doc_measures_json(hypermeasure->first_measure, hypermeasure->last_measure)) &&
              json_add(item, "kind", cJSON_CreateString(hypermeasure->kind)) &&
              json_add(item, "extra_hyperbeats", cJSON_CreateNumber(hypermeasure->extra_hyperbeats)) &&
              json_add(item, "contains_cadence", cJSON_CreateBool(hypermeasure->contains_cadence)) &&
              json_add(item, "score", cJSON_CreateNumber(hypermeasure->score)) &&
              json_add(item, "uncertainty", cJSON_CreateNumber(hypermeasure->uncertainty)) && json_append(list, item))) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The first measure of every hypermeasure of the phrase: its hyperdownbeats. */
static cJSON *hyperdownbeats_json(const AnalysisHypermeter *hypermeter) {
    cJSON *list = cJSON_CreateArray();
    for (size_t h = 0; list && h < hypermeter->hypermeasure_count; h++) {
        if (!json_append(list, cJSON_CreateNumber(hypermeter->hypermeasures[h].first_measure))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* A hypermeter claim: one phrase read in hypermeasures of two or four measures. */
static cJSON *hypermeter_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                              const AnalysisHypermeter *hypermeter) {
    const AnalysisPhrase *phrase = &report->phrases[hypermeter->phrase];
    Rational start = doc_rational(build, phrase->start.num, phrase->start.den);
    Rational end = doc_rational(build, phrase->end.num, phrase->end.den);
    cJSON *claim = doc_claim_json_with(build, hypermeter->framework, notes, hypermeter->note_ids, hypermeter->note_id_count,
                                   start, end, "hypermeter", cJSON_CreateNumber(1.0 - hypermeter->score));
    if (!(claim && json_add(claim, "phrase", cJSON_CreateNumber((double)hypermeter->phrase)) &&
          json_add(claim, "measures",
              doc_measures_json(hypermeter->first_measure, hypermeter->first_measure + hypermeter->measures - 1)) &&
          json_add(claim, "length", cJSON_CreateNumber((double)hypermeter->length)) &&
          json_add(claim, "score", cJSON_CreateNumber(hypermeter->score)) &&
          json_add(claim, "arrival_measure",
              hypermeter->has_arrival ? cJSON_CreateNumber(hypermeter->arrival_measure) : cJSON_CreateNull()) &&
          json_add(claim, "hyperdownbeats", hyperdownbeats_json(hypermeter)) &&
          json_add(claim, "hypermeasures", hypermeasures_json(hypermeter)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *tuplet_ratio_json(const AnalysisTuplet *tuplet) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "actual", cJSON_CreateNumber((double)tuplet->actual)) &&
                    json_add(object, "normal", cJSON_CreateNumber((double)tuplet->normal)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A tuplet claim: a run of notes that divide the beat the way the meter does not. The library gives it no uncertainty
   (the borrowed-division rules are rules, not a detector), so the claim's is null. */
static cJSON *tuplet_json(DocBuild *build, const ImportedScore *notes, const AnalysisTuplet *tuplet) {
    Rational start;
    Rational end;
    doc_notes_span(build, notes, tuplet->note_ids, tuplet->note_id_count, &start, &end);
    cJSON *claim = doc_claim_json_with(build, tuplet->framework, notes, tuplet->note_ids, tuplet->note_id_count, start, end,
                                   "tuplet", cJSON_CreateNull());
    if (!(claim && json_add(claim, "ratio", tuplet_ratio_json(tuplet)) && json_add(claim, "name", cJSON_CreateString(tuplet->name)) &&
          json_add(claim, "meter", cJSON_CreateString(tuplet->meter)) &&
          json_add(claim, "borrowed", cJSON_CreateBool(tuplet->borrowed)) &&
          json_add(claim, "level", tuplet->has_level ? cJSON_CreateString(tuplet->level) : cJSON_CreateNull()) &&
          json_add(claim, "level_typical", tuplet->has_level ? cJSON_CreateBool(tuplet->level_typical) : cJSON_CreateNull()) &&
          json_add(claim, "measure", cJSON_CreateNumber(tuplet->measure)) &&
          json_add(claim, "beat", cJSON_CreateNumber(tuplet->beat)) &&
          json_add(claim, "attacks", cJSON_CreateNumber((double)tuplet->attacks)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A meter reading of the meter check: the meter, the scale and the pickup as a notation reading states them. */
static cJSON *meter_reading_json(DocBuild *build, const AnalysisMeterReading *reading) {
    cJSON *object = cJSON_CreateObject();
    cJSON *meter = cJSON_CreateObject();
    bool ok = object && meter && json_add(meter, "numerator", cJSON_CreateNumber(reading->beats)) &&
              json_add(meter, "denominator", cJSON_CreateNumber(reading->beat_type));
    if (ok) {
        ok = json_add(object, "meter", meter); /* attached, or freed by add */
        meter = NULL;
    }
    ok = ok &&
         json_add(object, "scale", doc_rational_json(doc_rational(build, reading->scale.num, reading->scale.den))) &&
         json_add(object, "pickup",
                  doc_rational_json(doc_rational(build, reading->pickup.num, reading->pickup.den))) &&
         json_add(object, "score", cJSON_CreateNumber(reading->score));
    if (!ok) {
        cJSON_Delete(meter);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The meter-fit claim: the meter the analysis used against the best other reading, which is null unless it beats the
   effective one by the stage's margin. Over the whole piece, with no notes of its own. */
static cJSON *meter_fit_json(DocBuild *build, const ImportedScore *notes, const AnalysisMeterCheck *check) {
    Rational start;
    Rational end;
    doc_notes_span(build, notes, NULL, notes->count, &start, &end);
    cJSON *claim = doc_claim_json_with(build, "standard-meter-v1", notes, NULL, 0, start, end, "meter-fit",
                                   cJSON_CreateNumber(check->uncertainty));
    if (!(claim && json_add(claim, "declared", meter_reading_json(build, &check->effective)) &&
          json_add(claim, "alternative",
              check->has_alternative ? meter_reading_json(build, &check->other) : cJSON_CreateNull()))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The rhythm section: a list of claims, each labelled by the result it is ("meter-fit", "density", "syncopation",
   "hemiola", "hypermeter", "tuplet"): the meter fit first, then the densities and each kind in time order. The library's
   results are claims over the notes of a phrase or a measure, so the list is flat and the label says which kind. A
   piece without phrases has only the meter fit, or nothing when it has no chord segments. */
cJSON *doc_rhythm_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (list && report->meter_check.present && !json_append(list, meter_fit_json(build, notes, &report->meter_check))) {
        cJSON_Delete(list);
        return NULL;
    }
    for (size_t i = 0; list && i < report->density_count; i++) {
        if (!json_append(list, density_json(build, notes, report, &report->densities[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->syncopation_count; i++) {
        if (!json_append(list, syncopation_json(build, notes, &report->syncopations[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->hemiola_count; i++) {
        if (!json_append(list, hemiola_json(build, notes, &report->hemiolas[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->hypermeter_count; i++) {
        if (!json_append(list, hypermeter_json(build, notes, report, &report->hypermeters[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->tuplet_count; i++) {
        if (!json_append(list, tuplet_json(build, notes, &report->tuplets[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The measured features of a texture passage: the library's means over its measures, the voice count the largest. */
static cJSON *texture_features_json(const AnalysisTexturePassage *passage) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "voices_max", cJSON_CreateNumber(passage->voices_max)) &&
                    json_add(object, "voices_mean", cJSON_CreateNumber(passage->voices_mean)) &&
                    json_add(object, "synchrony", cJSON_CreateNumber(passage->synchrony)) &&
                    json_add(object, "independence", cJSON_CreateNumber(passage->independence)) &&
                    json_add(object, "prominence", cJSON_CreateNumber(passage->prominence)) &&
                    json_add(object, "top_gap", cJSON_CreateNumber(passage->top_gap)) &&
                    json_add(object, "register_span", cJSON_CreateNumber(passage->register_span)) &&
                    json_add(object, "spacing", cJSON_CreateNumber(passage->spacing)) &&
                    json_add(object, "density", cJSON_CreateNumber(passage->density)) &&
                    json_add(object, "pattern", cJSON_CreateNumber(passage->pattern)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *texture_confidence_json(const AnalysisTexturePassage *passage) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "texture", cJSON_CreateNumber(passage->texture_confidence)) &&
                    json_add(object, "voices", cJSON_CreateNumber(passage->voice_confidence)) &&
                    json_add(object, "overall", cJSON_CreateNumber(passage->confidence)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static Rational texture_passage_start(DocBuild *build, const AnalysisTexturePassage *passage) {
    return doc_rational(build, passage->start.num, passage->start.den);
}

static Rational texture_passage_end(DocBuild *build, const AnalysisTexturePassage *passage) {
    return doc_rational(build, passage->end.num, passage->end.den);
}

/* A texture passage claim: a run of measures with the same texture, as the library classifies it. */
static cJSON *texture_passage_json(DocBuild *build, const ImportedScore *notes, const AnalysisTexturePassage *passage) {
    cJSON *claim = doc_claim_json_with(build, passage->framework, notes, passage->note_ids, passage->note_id_count,
                                   texture_passage_start(build, passage), texture_passage_end(build, passage), "passage",
                                   cJSON_CreateNumber(passage->uncertainty));
    if (!(claim && json_add(claim, "texture", cJSON_CreateString(passage->texture)) &&
          json_add(claim, "category", cJSON_CreateString(passage->category)) &&
          json_add(claim, "measures", doc_measures_json(passage->first_measure, passage->last_measure)) &&
          json_add(claim, "windows", cJSON_CreateNumber(passage->windows)) &&
          json_add(claim, "unclassified_windows", cJSON_CreateNumber(passage->unclassified_windows)) &&
          json_add(claim, "confidence", texture_confidence_json(passage)) &&
          json_add(claim, "features", texture_features_json(passage)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A texture change claim: the boundary between two consecutive passages, spanning the passage changed to. */
static cJSON *texture_change_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                  const AnalysisTextureChange *change) {
    const AnalysisTexturePassage *to = &report->texture_passages[change->to];
    cJSON *claim = doc_claim_json_with(build, change->framework, notes, NULL, 0, texture_passage_start(build, to), texture_passage_end(build, to),
                                   "change", cJSON_CreateNumber(change->uncertainty));
    if (!(claim && json_add(claim, "from", cJSON_CreateNumber((double)change->from)) &&
          json_add(claim, "to", cJSON_CreateNumber((double)change->to)) &&
          json_add(claim, "boundary_measure", cJSON_CreateNumber(change->boundary_measure)) &&
          json_add(claim, "from_texture", cJSON_CreateString(change->from_texture)) &&
          json_add(claim, "to_texture", cJSON_CreateString(change->to_texture)) &&
          json_add(claim, "category_changed", cJSON_CreateBool(change->category_changed)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The texture section: a list of claims, the passages in time order (labelled "passage") and then the changes between
   them (labelled "change"). Empty when the library found no texture. */
cJSON *doc_texture_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < report->texture_passage_count; i++) {
        if (!json_append(list, texture_passage_json(build, notes, &report->texture_passages[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->texture_change_count; i++) {
        if (!json_append(list, texture_change_json(build, notes, report, &report->texture_changes[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}
