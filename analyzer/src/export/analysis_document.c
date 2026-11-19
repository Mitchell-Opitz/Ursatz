/* The analysis document: assembly, the public API (analysis_document.h) and the file name. The areas are built by the
   document_*.c files of this folder; analysis_document_internal.h declares what they share. */

#include "export/analysis_document_internal.h"

static cJSON *document_json(DocBuild *build, const char *input_path, const ImportedScore *notes, const ChainResult *report) {
    cJSON *document = cJSON_CreateObject();
    if (document &&
        !(common_json_add(document, "schema_version", cJSON_CreateNumber(URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION)) &&
          common_json_add(document, "metadata", doc_metadata_json(build, input_path, notes, report)) &&
          common_json_add(document, "meter", doc_meter_json(build, notes)) && common_json_add(document, "keys", doc_keys_json(build, notes, report)) &&
          common_json_add(document, "tonicizations", doc_tonicizations_json(build, notes, report)) &&
          common_json_add(document, "modulations", doc_modulations_json(build, notes, report)) &&
          common_json_add(document, "pivot_chords", doc_pivot_chords_json(build, notes, report)) &&
          common_json_add(document, "chords", doc_chords_json(build, notes, report)) &&
          common_json_add(document, "chord_vocabulary",
              doc_chord_vocabulary_json(build, report, cJSON_GetObjectItemCaseSensitive(document, "chords"))) &&
          common_json_add(document, "prolongations", doc_prolongations_json(build, notes, report)) &&
          common_json_add(document, "non_chord_tones", doc_non_chord_tones_json(build, notes, report)) &&
          common_json_add(document, "outer_voices", doc_outer_voices_json(build, notes, report)) &&
          common_json_add(document, "cadences", doc_cadences_json(build, notes, report)) &&
          common_json_add(document, "phrases", doc_phrases_json(build, notes, report)) &&
          common_json_add(document, "themes", doc_themes_json(build, notes, report)) && common_json_add(document, "form", doc_form_json(build, notes, report)) &&
          common_json_add(document, "repeats", doc_repeats_json(build, notes, report)) &&
          common_json_add(document, "motives", doc_motives_json(build, notes, report)) && common_json_add(document, "melody", doc_melody_json(build, notes, report)) &&
          common_json_add(document, "rhythm", doc_rhythm_json(build, notes, report)) && common_json_add(document, "texture", doc_texture_json(build, notes, report)) &&
          common_json_add(document, "voice_leading", doc_voice_leading_json(build, notes, report)) && common_json_add(document, "schemas", doc_schemas_json(build, notes, report)) &&
          common_json_add(document, "progressions", doc_progressions_json(build, notes, report)) && common_json_add(document, "harmonic_rhythm", doc_harmonic_rhythm_json(build, report)) &&
          common_json_add(document, "markings", doc_markings_json(build, notes, report))  &&
          common_json_add(document, "notes", doc_notes_json(build, notes, report)))) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

AnalyzerStatus analysis_document_build_from_reading(const char *path, const Reading *reading, const ImportedScore *score,
                                                    const KnowledgeSnapshot *knowledge, AnalysisDocument **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    const AnalysisChain *chain = reading ? reading_detail(reading) : NULL;
    if (!path || !score || !knowledge || !chain || (reading_source(reading)->stages & ANALYSIS_CHAIN_STAGE_PERCEPTION) != ANALYSIS_CHAIN_STAGE_PERCEPTION) {
        return ANALYZER_ERR_ARGUMENT;
    }
    DocBuild build = {false, {NULL, NULL, NULL, NULL}, reading};
    if (!common_practice_materials_resolve(knowledge, &build.materials, NULL, 0)) {
        return ANALYZER_ERR_ARGUMENT;
    }
    AnalysisDocument *document = malloc(sizeof(AnalysisDocument));
    if (!document) {
        return ANALYZER_ERR_ALLOC;
    }
    document->root = document_json(&build, path, score, analysis_chain_result(chain));
    if (document->root && build.overflowed) {
        cJSON_Delete(document->root);
        document->root = NULL;
    }
    if (!document->root) {
        free(document);
        return ANALYZER_ERR_ALLOC;
    }
    *out = document;
    return ANALYZER_OK;
}

AnalysisDocument *analysis_document_build_from_result(const char *input_path, const AnalyzerResult *result) {
    if (!result) {
        return NULL;
    }
    AnalysisDocument *document = NULL;
    if (analysis_document_build_from_reading(input_path, analyzer_result_reading(result), analyzer_result_notes(result),
                                             analyzer_result_knowledge(result), &document) != ANALYZER_OK) {
        return NULL;
    }
    return document;
}

char *analysis_document_render_result(const char *input_path, const AnalyzerResult *result) {
    AnalysisDocument *document = analysis_document_build_from_result(input_path, result);
    if (!document) {
        return NULL;
    }
    char *json = analysis_document_to_json(document);
    analysis_document_destroy(document);
    return json;
}

char *analysis_document_to_json(const AnalysisDocument *document) {
    CommonText buffer;
    common_text_init(&buffer);
    if (!doc_print_value(&buffer, document->root, 0) || !common_text_append(&buffer, "\n")) {
        common_text_free(&buffer);
        return NULL;
    }
    return common_text_take(&buffer);
}

void analysis_document_destroy(AnalysisDocument *document) {
    if (!document) {
        return;
    }
    cJSON_Delete(document->root);
    free(document);
}

char *derive_analysis_document_path(const char *input_path) {
    return path_with_suffix(input_path, ".analysis.json");
}
