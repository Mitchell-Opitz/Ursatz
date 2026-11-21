/* The voice-leading areas of the analysis document: markings, voice leading, schemas and sequences. */

#include "export/analysis_document_internal.h"
#include "theory/vocabulary/framework_names.h"

/* The metronome mark of a tempo claim: the beat unit in whole notes and the beats per minute, both rational. */
static cJSON *metronome_json(DocBuild *build, const AnalysisTempoMarking *marking) {
    if (!marking->has_metronome) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(common_json_add(
              object, "beat_unit",
              doc_rational_json(doc_rational(build, (long long)marking->beat_unit.num, marking->beat_unit.den))) &&
          common_json_add(object, "per_minute",
                   doc_rational_json(doc_rational(build, (long long)marking->bpm.num, marking->bpm.den))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *tempo_marking_json(DocBuild *build, const ImportedScore *notes, const AnalysisTempoMarking *marking) {
    cJSON *claim = doc_claim_json_with(build, marking->framework, notes, NULL, 0,
                                   doc_rational(build, marking->start.num, marking->start.den),
                                   doc_rational(build, marking->end.num, marking->end.den), "tempo",
                                   cJSON_CreateNumber(marking->uncertainty));
    if (!(claim && common_json_add(claim, "text", doc_optional_string_json(marking->text)) &&
          common_json_add(claim, "metronome", metronome_json(build, marking)) &&
          common_json_add(claim, "part", cJSON_CreateNumber((double)marking->part)) &&
          common_json_add(claim, "rank", marking->has_rank ? cJSON_CreateNumber(marking->rank) : cJSON_CreateNull()) &&
          common_json_add(claim, "group", doc_optional_string_json(marking->group)) &&
          common_json_add(claim, "change", cJSON_CreateString(marking->change)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *dynamic_json(DocBuild *build, const ImportedScore *notes, const AnalysisDynamic *dynamic) {
    cJSON *claim = doc_claim_json_with(build, dynamic->framework, notes, NULL, 0,
                                   doc_rational(build, dynamic->start.num, dynamic->start.den),
                                   doc_rational(build, dynamic->end.num, dynamic->end.den), "dynamic",
                                   cJSON_CreateNumber(dynamic->uncertainty));
    if (!(claim && common_json_add(claim, "marking", cJSON_CreateString(dynamic->marking)) &&
          common_json_add(claim, "part", cJSON_CreateNumber((double)dynamic->part)) &&
          common_json_add(claim, "staff", cJSON_CreateNumber(dynamic->staff)) &&
          common_json_add(claim, "rank", dynamic->has_rank ? cJSON_CreateNumber(dynamic->rank) : cJSON_CreateNull()) &&
          common_json_add(claim, "change", cJSON_CreateString(dynamic->change)) &&
          common_json_add(claim, "relation", doc_optional_string_json(dynamic->relation)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The markings section: what the notated input carries, read with standard-markings' rules. MIDI input has none
   ("none"). The importer reads no articulations (R-056), so they are null and say so. */
cJSON *doc_markings_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    bool notated = notes->source.format == IMPORTED_FORMAT_MUSICXML;
    cJSON *object = cJSON_CreateObject();
    cJSON *tempo = cJSON_CreateArray();
    cJSON *dynamics = cJSON_CreateArray();
    bool ok = object && tempo && dynamics;
    for (size_t i = 0; ok && i < report->tempo_marking_count; i++) {
        ok = common_json_append(tempo, tempo_marking_json(build, notes, &report->tempo_markings[i]));
    }
    for (size_t i = 0; ok && i < report->dynamic_count; i++) {
        ok = common_json_append(dynamics, dynamic_json(build, notes, &report->dynamics[i]));
    }
    ok = ok && common_json_add(object, "source", cJSON_CreateString(notated ? "notated" : "none"));
    if (ok) {
        ok = common_json_add(object, "tempo", tempo); /* common_json_add() frees the item itself when it fails */
        tempo = NULL;
    }
    if (ok) {
        ok = common_json_add(object, "dynamics", dynamics);
        dynamics = NULL;
    }
    ok = ok && common_json_add(object, "articulations", cJSON_CreateNull()) &&
         common_json_add(object, "articulations_imported", cJSON_CreateBool(false));
    if (!ok) {
        cJSON_Delete(tempo);
        cJSON_Delete(dynamics);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static const char *const MOTION_NAMES[ANALYSIS_MOTION_TYPES] = {"contrary", "similar", "parallel", "oblique", "static"};

static cJSON *motion_counts_json(const size_t *counts) {
    cJSON *object = cJSON_CreateObject();
    for (size_t type = 0; object && type < ANALYSIS_MOTION_TYPES; type++) {
        if (!common_json_add(object, MOTION_NAMES[type], cJSON_CreateNumber((double)counts[type]))) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    return object;
}

static cJSON *motion_pairs_json(const AnalysisVoiceLeading *voice_leading) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < voice_leading->pair_count; i++) {
        const AnalysisVoicePairMotion *pair = &voice_leading->pairs[i];
        cJSON *object = cJSON_CreateObject();
        if (!(object && common_json_add(object, "upper_voice", cJSON_CreateNumber((double)pair->upper_voice)) &&
              common_json_add(object, "lower_voice", cJSON_CreateNumber((double)pair->lower_voice)) &&
              common_json_add(object, "total", cJSON_CreateNumber((double)pair->total)) &&
              common_json_add(object, "counts", motion_counts_json(pair->counts)) && common_json_append(list, object))) {
            cJSON_Delete(object);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The start of the first entry and the end of the last: the span the voice leading covers. */
static void voice_leading_span(DocBuild *build, const ChainResult *report, Rational *start, Rational *end) {
    const ChainEntry *first = &report->entries[0];
    const ChainEntry *last = &report->entries[report->entry_count - 1];
    *start = doc_rational(build, first->time.num, first->time.den);
    *end = doc_rational(build, last->end.num, last->end.den);
}

/* The motion claim: how the voices move against each other between consecutive chord segments, in total and per pair of
   voices. It covers the whole piece. */
static cJSON *voice_motions_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    const AnalysisVoiceLeading *voice_leading = &report->voice_leading;
    Rational start;
    Rational end;
    voice_leading_span(build, report, &start, &end);
    cJSON *claim = doc_claim_json_with(build, voice_leading->framework, notes, NULL, 0, start, end, "motions",
                                   cJSON_CreateNumber(voice_leading->motion_uncertainty));
    if (!(claim && common_json_add(claim, "voices", cJSON_CreateNumber((double)voice_leading->voice_count)) &&
          common_json_add(claim, "segments", cJSON_CreateNumber((double)voice_leading->row_count)) &&
          common_json_add(claim, "motion_count", cJSON_CreateNumber((double)voice_leading->motion_count)) &&
          common_json_add(claim, "totals", motion_counts_json(voice_leading->motion_totals)) &&
          common_json_add(claim, "pairs", motion_pairs_json(voice_leading)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The span from the start of one entry to the end of a later one. */
static void voice_leading_entries_span(DocBuild *build, const ChainResult *report, size_t from, size_t to, Rational *start,
                                       Rational *end) {
    const ChainEntry *first = &report->entries[from];
    const ChainEntry *last = &report->entries[to];
    *start = doc_rational(build, first->time.num, first->time.den);
    *end = doc_rational(build, last->end.num, last->end.den);
}

/* A fault of the voices' leading, as the claim for the move between two consecutive chords. */
static cJSON *voice_violation_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                   const AnalysisVoiceLeading *voice_leading, const AnalysisVoiceViolation *violation) {
    Rational start;
    Rational end;
    voice_leading_entries_span(build, report, violation->entry_from, violation->entry_to, &start, &end);
    cJSON *claim = doc_claim_json_with(build, voice_leading->framework, notes, violation->note_ids, 4, start, end, "violation",
                                   cJSON_CreateNumber(violation->uncertainty));
    if (!(claim && common_json_add(claim, "rule", cJSON_CreateString(violation->rule)) &&
          common_json_add(claim, "upper_voice", cJSON_CreateNumber((double)violation->upper_voice)) &&
          common_json_add(claim, "lower_voice", cJSON_CreateNumber((double)violation->lower_voice)) &&
          common_json_add(claim, "interval", cJSON_CreateNumber(violation->arrival_interval)) &&
          common_json_add(claim, "from_chord", cJSON_CreateNumber((double)violation->entry_from)) &&
          common_json_add(claim, "to_chord", cJSON_CreateNumber((double)violation->entry_to)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A tendency tone left unresolved when its chord changes. */
static cJSON *voice_resolution_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                    const AnalysisVoiceLeading *voice_leading,
                                    const AnalysisVoiceResolution *resolution) {
    Rational start;
    Rational end;
    voice_leading_entries_span(build, report, resolution->entry_from, resolution->entry_to, &start, &end);
    cJSON *claim = doc_claim_json_with(build, FRAMEWORK_STANDARD_HARMONY, notes, resolution->note_ids, 2, start, end, "resolution",
                                   cJSON_CreateNumber(resolution->uncertainty));
    if (!(claim && common_json_add(claim, "rule", cJSON_CreateString(resolution->rule)) &&
          common_json_add(claim, "voice", cJSON_CreateNumber((double)resolution->voice)) &&
          common_json_add(claim, "from_chord", cJSON_CreateNumber((double)resolution->entry_from)) &&
          common_json_add(claim, "to_chord", cJSON_CreateNumber((double)resolution->entry_to)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static const char *const CHORD_MEMBER_NAMES[ANALYSIS_CHORD_MEMBERS] = {"root", "third", "fifth", "seventh"};

static cJSON *chord_gaps_json(const AnalysisVoiceChord *chord) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < chord->gap_count; i++) {
        if (!common_json_append(list, cJSON_CreateNumber((double)chord->gaps[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The spacing of a chord: whether the library judged it, and if so the upper voice of the first pair too wide. */
static cJSON *chord_spacing_json(const AnalysisVoiceChord *chord) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(common_json_add(object, "judged", cJSON_CreateBool(chord->spacing_judged)) &&
          common_json_add(object, "valid", chord->spacing_judged ? cJSON_CreateBool(chord->wide_voice < 0) : cJSON_CreateNull()) &&
          common_json_add(object, "wide_voice",
              chord->spacing_judged && chord->wide_voice >= 0 ? cJSON_CreateNumber(chord->wide_voice) : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The doubling of a chord: the member most voices sound when more than one does, or null. */
static cJSON *chord_doubled_json(const AnalysisVoiceChord *chord) {
    if (chord->doubled_member < 0) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "member", cJSON_CreateString(CHORD_MEMBER_NAMES[chord->doubled_member])) &&
                    common_json_add(object, "voices", cJSON_CreateNumber((double)chord->member_voices[chord->doubled_member])))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One chord's doubling and spacing, on the chord's own span. */
static cJSON *voice_chord_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                               const AnalysisVoiceLeading *voice_leading, const AnalysisVoiceChord *chord) {
    Rational start;
    Rational end;
    voice_leading_entries_span(build, report, chord->entry, chord->entry, &start, &end);
    cJSON *claim = doc_claim_json_with(build, FRAMEWORK_STANDARD_VOICING, notes, chord->note_ids, chord->note_id_count, start, end,
                                   "doubling", cJSON_CreateNumber(chord->uncertainty));
    if (!(claim && common_json_add(claim, "chord", cJSON_CreateNumber((double)chord->entry)) &&
          common_json_add(claim, "voices", cJSON_CreateNumber((double)chord->voices_sounding)) &&
          common_json_add(claim, "doubled", chord_doubled_json(chord)) &&
          common_json_add(claim, "non_chord_voices", cJSON_CreateNumber((double)chord->non_chord_voices)) &&
          common_json_add(claim, "gaps", chord_gaps_json(chord)) && common_json_add(claim, "spacing", chord_spacing_json(chord)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The voice leading section: a list of claims, each labelled by the result it is. Empty when the library declined the
   voices. */
cJSON *doc_voice_leading_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    const AnalysisVoiceLeading *voice_leading = &report->voice_leading;
    if (!list || !voice_leading->present) {
        return list;
    }
    bool ok = common_json_append(list, voice_motions_json(build, notes, report));
    for (size_t i = 0; ok && i < voice_leading->violation_count; i++) {
        ok = common_json_append(list, voice_violation_json(build, notes, report, voice_leading, &voice_leading->violations[i]));
    }
    for (size_t i = 0; ok && i < voice_leading->resolution_count; i++) {
        ok = common_json_append(list, voice_resolution_json(build, notes, report, voice_leading, &voice_leading->resolutions[i]));
    }
    for (size_t i = 0; ok && i < voice_leading->chord_count; i++) {
        ok = common_json_append(list, voice_chord_json(build, notes, report, voice_leading, &voice_leading->chords[i]));
    }
    if (!ok) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}

/* The measure a time lies in, as the library numbers them (a pickup is 0); 0 when the position cannot be read. */
static int measure_of_time(const MeterMap *map, Rational time) {
    MusicalTime musical_time = musical_time_of(time);
    MetricalPosition *position = NULL;
    int measure = 0;
    if (metrical_position_from_time(map, &musical_time, &position) == METRICAL_POSITION_OK) {
        measure = metrical_position_measure(position);
        metrical_position_destroy(position);
    }
    return measure;
}

/* A degree as the library's galant-schemas reads it: the letter distance from the tonic (1 to 7) and the alteration in
   semitones against the major scale (-1 lowered, +1 raised). */
static cJSON *galant_degree_json(int degree, int alteration) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "degree", cJSON_CreateNumber(degree)) &&
                    common_json_add(object, "alteration", cJSON_CreateNumber(alteration)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The first and last measure of the chords entries[first..last] start in. */
static cJSON *entry_measures_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, size_t first, size_t last) {
    const MeterMap *map = notes->meter.meter_map;
    return doc_measures_json(measure_of_time(map, doc_entry_start_time(build, &report->entries[first])),
                         measure_of_time(map, doc_entry_start_time(build, &report->entries[last])));
}

/* One stage of a schema: where it sits (span, location and the measure it starts in), the soprano degree the library
   matched (null for a bass-only stage), a two-note stage's second soprano degree (otherwise null), the bass degree and
   the stage's credit. */
static cJSON *schema_stage_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                const AnalysisSchemaStage *stage) {
    Rational start = doc_entry_start_time(build, &report->entries[stage->first_entry]);
    Rational end = doc_entry_end_time(build, &report->entries[stage->last_entry]);
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "span", doc_span_json(start, end)) &&
          common_json_add(object, "location", doc_location_json(build, notes->meter.meter_map, start, end)) &&
          common_json_add(object, "measure", cJSON_CreateNumber(measure_of_time(notes->meter.meter_map, start))) &&
          common_json_add(object, "soprano",
              stage->has_melody ? galant_degree_json(stage->melody_degree, stage->melody_alteration)
                                : cJSON_CreateNull()) &&
          common_json_add(object, "second_soprano",
              stage->has_second ? galant_degree_json(stage->second_degree, stage->second_alteration)
                                : cJSON_CreateNull()) &&
          common_json_add(object, "bass", galant_degree_json(stage->bass_degree, stage->bass_alteration)) &&
          common_json_add(object, "bass_only", cJSON_CreateBool(stage->bass_only)) &&
          common_json_add(object, "credit", cJSON_CreateNumber(stage->credit)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *schema_stages_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, const AnalysisSchema *schema) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < schema->stage_count; i++) {
        if (!common_json_append(list, schema_stage_json(build, notes, report, &schema->stages[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per schema, in order of the first stage. The claim's span runs from the first stage's chord to the last
   chord the stages use. frame is the key the stages were read in, as a degree of the home key; confidence holds the
   library's parts (pair share, metric fit, mean uncertainty) and the whole. Everything else is the library's: the name,
   function, stages and degrees. */
static cJSON *schema_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, const AnalysisSchema *schema) {
    size_t id_count = 0;
    size_t *ids = doc_entry_range_note_ids(report, schema->first_entry, schema->last_entry, &id_count);
    cJSON *claim = ids ? doc_claim_json_with(build, schema->framework, notes, ids, id_count,
                                         doc_entry_start_time(build, &report->entries[schema->first_entry]),
                                         doc_entry_end_time(build, &report->entries[schema->last_entry]), "schema",
                                         cJSON_CreateNumber(schema->uncertainty))
                       : NULL;
    free(ids);
    cJSON *confidence = cJSON_CreateObject();
    bool taken = false; /* common_json_add() takes the confidence over, and frees it itself when it fails */
    if (!(claim && confidence && common_json_add(confidence, "pair_share", cJSON_CreateNumber(schema->pair_share)) &&
          common_json_add(confidence, "metric_fit", cJSON_CreateNumber(schema->metric_fit)) &&
          common_json_add(confidence, "mean_uncertainty", cJSON_CreateNumber(schema->mean_uncertainty)) &&
          common_json_add(confidence, "overall", cJSON_CreateNumber(schema->confidence)) &&
          common_json_add(claim, "name", cJSON_CreateString(schema->name)) &&
          common_json_add(claim, "function", cJSON_CreateString(schema->function)) &&
          common_json_add(claim, "measures", entry_measures_json(build, notes, report, schema->first_entry, schema->last_entry)) &&
          common_json_add(claim, "frame", galant_degree_json(schema->frame_degree, schema->frame_alteration)) &&
          common_json_add(claim, "bass_only_stages", cJSON_CreateNumber((double)schema->bass_only_stages)) &&
          (taken = true, common_json_add(claim, "confidence", confidence)) &&
          common_json_add(claim, "stages", schema_stages_json(build, notes, report, schema)))) {
        if (!taken) {
            cJSON_Delete(confidence);
        }
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One stage of a sequence: where it sits and the bass, soprano and chord root degrees the library read there in the home
   key (soprano and root null where the segment has none). */
static cJSON *sequence_stage_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                  const AnalysisSequenceStage *stage) {
    Rational start = doc_entry_start_time(build, &report->entries[stage->first_entry]);
    Rational end = doc_entry_end_time(build, &report->entries[stage->last_entry]);
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "span", doc_span_json(start, end)) &&
          common_json_add(object, "location", doc_location_json(build, notes->meter.meter_map, start, end)) &&
          common_json_add(object, "measure", cJSON_CreateNumber(measure_of_time(notes->meter.meter_map, start))) &&
          common_json_add(object, "bass", galant_degree_json(stage->bass_degree, stage->bass_alteration)) &&
          common_json_add(object, "soprano",
              stage->has_soprano ? galant_degree_json(stage->soprano_degree, stage->soprano_alteration)
                                 : cJSON_CreateNull()) &&
          common_json_add(object, "root",
              stage->has_root ? galant_degree_json(stage->root_degree, stage->root_alteration) : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *sequence_stages_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                   const AnalysisSequence *sequence) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < sequence->stage_count; i++) {
        if (!common_json_append(list, sequence_stage_json(build, notes, report, &sequence->stages[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per sequence, in order of the first segment. The claim's span runs from the first segment to the end of
   the last; the label is "sequence". kind is the library's name for what repeats; pattern and direction are null where
   the library encodes none (the cycle of fifths has no rule, so rule_backed is false). */
static cJSON *sequence_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, const AnalysisSequence *sequence) {
    size_t id_count = 0;
    size_t *ids = doc_entry_range_note_ids(report, sequence->first_entry, sequence->last_entry, &id_count);
    cJSON *claim = ids ? doc_claim_json_with(build, sequence->framework, notes, ids, id_count,
                                         doc_entry_start_time(build, &report->entries[sequence->first_entry]),
                                         doc_entry_end_time(build, &report->entries[sequence->last_entry]), "sequence",
                                         cJSON_CreateNumber(sequence->uncertainty))
                       : NULL;
    free(ids);
    cJSON *confidence = cJSON_CreateObject();
    bool taken = false; /* common_json_add() takes the confidence over, and frees it itself when it fails */
    if (!(claim && confidence && common_json_add(confidence, "unit_share", cJSON_CreateNumber(sequence->unit_share)) &&
          common_json_add(confidence, "soprano_bonus", cJSON_CreateNumber(sequence->soprano_bonus)) &&
          common_json_add(confidence, "mean_uncertainty", cJSON_CreateNumber(sequence->mean_uncertainty)) &&
          common_json_add(confidence, "overall", cJSON_CreateNumber(sequence->confidence)) &&
          common_json_add(claim, "kind", cJSON_CreateString(sequence->kind)) &&
          common_json_add(claim, "pattern", doc_optional_string_json(sequence->pattern)) &&
          common_json_add(claim, "rule_backed", cJSON_CreateBool(sequence->rule_backed)) &&
          common_json_add(claim, "direction", doc_optional_string_json(sequence->direction)) &&
          common_json_add(claim, "unit_length", cJSON_CreateNumber(sequence->unit_length)) &&
          common_json_add(claim, "units", cJSON_CreateNumber(sequence->units)) &&
          common_json_add(claim, "transposition_steps", cJSON_CreateNumber(sequence->transposition_steps)) &&
          common_json_add(claim, "measures", entry_measures_json(build, notes, report, sequence->first_entry, sequence->last_entry)) &&
          (taken = true, common_json_add(claim, "confidence", confidence)) &&
          common_json_add(claim, "stages", sequence_stages_json(build, notes, report, sequence)))) {
        if (!taken) {
            cJSON_Delete(confidence);
        }
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The schemas section: one flat list of claims, the schemas (labelled "schema") in order of their first stage and then
   the sequences (labelled "sequence") in order of their first segment. */
cJSON *doc_schemas_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < report->schema_count; i++) {
        if (!common_json_append(list, schema_json(build, notes, report, &report->schemas[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->sequence_count; i++) {
        if (!common_json_append(list, sequence_json(build, notes, report, &report->sequences[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}
