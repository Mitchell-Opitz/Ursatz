#ifndef URSATZ_ANALYZER_STALENESS_H
#define URSATZ_ANALYZER_STALENESS_H

/*
 * Whether a stored analysis is current. A stored analysis is current only when its schema version, the Analyzer
 * version that wrote it and the producer key it was analyzed against all match this build (Boundary rule 11).
 * The first mismatch found, in the order schema, Analyzer, producer key, names the status.
 */
typedef enum {
    ANALYZER_DOCUMENT_CURRENT = 0,
    ANALYZER_DOCUMENT_STALE_SCHEMA,   /* written under another document schema version */
    ANALYZER_DOCUMENT_STALE_ANALYZER, /* written by another Analyzer version */
    ANALYZER_DOCUMENT_STALE_URSATZ,   /* analyzed against another producer key */
    ANALYZER_DOCUMENT_UNREADABLE      /* not JSON, or schema_version / metadata members missing or wrong type (a document
                                         of another schema version with a metadata.analyzer_version string is STALE_SCHEMA) */
} AnalyzerDocumentStatus;

#ifdef __cplusplus
extern "C" {
#endif

/* Compares the three keys with URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION, ursatz_analyzer_version() and
   ursatz_analyzer_producer_key(). NULL strings are UNREADABLE. Never returns UNREADABLE for non-NULL strings. */
AnalyzerDocumentStatus analyzer_document_status_of(int schema_version, const char *analyzer_version,
                                                   const char *producer_key);

/* Reads schema_version and metadata (analyzer_version, producer_key) from an analysis document's JSON text, and
   returns analyzer_document_status_of() of them. A document whose schema_version is a number other than the current one
   and whose metadata.analyzer_version is a string is STALE_SCHEMA whatever its other members. Nothing else in the
   document is read. */
AnalyzerDocumentStatus analyzer_document_status(const char *document_json);

#ifdef __cplusplus
}
#endif

#endif
