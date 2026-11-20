#ifndef URSATZ_WORKSHOP_ANALYSIS_ANALYZE_TOOL_H
#define URSATZ_WORKSHOP_ANALYSIS_ANALYZE_TOOL_H

#include <stdbool.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The tool "analyze" (R-656): one file analyzed as the Analyzer's command line did, with the outputs as text in the answer. The coordinator's
 * tools.c reads the params into this request; the strings are the params' own (NULL for absent).
 */
typedef struct {
    const char *path;         /* required: the file as given (the document's source names it) */
    const char *input_format; /* "auto" (NULL too), "midi" or "musicxml" */
    const char *meter;        /* the notation reading: "3/4" */
    const char *value_scale;  /* "2" or "1/2" */
    const char *pickup;       /* "1/4" */
    bool layers;              /* answer the realization and the reading */
    bool model_plan;          /* answer the model plan of the reading */
    bool text;                /* answer the text export and no document */
    int first_measure;        /* the text's range as written in the score; 0 and 0 for the whole piece */
    int last_measure;
    const char *title;        /* the text's title */
} WorkshopAnalyzeRequest;

/*
 * *out is {"warnings": [...], "document", "document_path"} (the document text and where the command line wrote it; none with text),
 * with layers "realization" and "reading" (texts), with model_plan "plan" (text, producer ursatz-analyzer) and with text "text".
 * An import that fails is URSATZ_ERR_FORMAT with the importer's message at "/params/path"; a notation reading that does not apply is
 * URSATZ_ERR_REQUEST at "/params/pickup" or "/params/value_scale"; an analysis that fails answers its status with "the analysis failed";
 * a text range with no measure is URSATZ_ERR_REQUEST at "/params/text"; a model plan that cannot be made is URSATZ_ERR_UNSATISFIABLE.
 */
UrsatzStatus workshop_analyze(const WorkshopAnalyzeRequest *request, cJSON **out, UrsatzError *error);

#endif
