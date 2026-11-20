#ifndef URSATZ_ANALYZER_ANALYZER_API_H
#define URSATZ_ANALYZER_ANALYZER_API_H

/*
 * The Analyzer API (R-640): every entry an application outside the Analyzer calls, in one header. The names start with
 * analyzer_ (the types keep theirs). Everything else of the Analyzer (its command line, the corpus run, the similar-material
 * search, the chord sequence of a document) is internal, in src/include/ursatz_analyzer_internal/.
 *
 * Every entry answers with the common status set (<common/status.h>, R-639). The imported score, the knowledge snapshot, the
 * reading and the style profile are the library's: this header includes the library headers it names, and a caller reaches the
 * other library functions through the library's own headers.
 */

#include <stdbool.h>
#include <stddef.h>

#include <common/status.h>

#include <chain/chain_result.h>
#include <io/imported_score.h>
#include <reading/reading.h>
#include <realization/work.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/style/style_profile.h>

#include "analysis/motive/motive_link.h"
#include "analysis/motive/motive_relation.h"
#include "analysis/motive/motive_variant_recognition.h"
#include "model/core/rational.h"

/* ------------------------------------------------------------------ versions */

/* The Analyzer's version and producer key (R-554): what a stored analysis, a corpus index and a link cache are compared with. */
const char *analyzer_version(void);
const char *analyzer_producer_key(void);

/*
 * The version of each format the Analyzer persists (R-640); each file writes its constant, and a breaking change bumps it.
 * The analysis document is described by schema/analysis-document.v13.schema.json; the style model's version is the library's.
 */
#define ANALYZER_DOCUMENT_SCHEMA_VERSION 13
#define ANALYZER_TEXT_EXPORT_VERSION 2 /* written as "# Ursatz export v2" on the first line of a text export */
#define ANALYZER_CORPUS_INDEX_VERSION 2
#define ANALYZER_LINK_CACHE_VERSION 2
#define ANALYZER_MOTIVE_LINKS_VERSION 1
#define ANALYZER_SIMILAR_VERSION 1

/* ------------------------------------------------------------------ import */

/*
 * The Analyzer's import entries for the GUI (R-614): the imported score, the formats and the readings are the library's
 * (io/imported_score.h, included here), and this call and analyzer_import_buffer (internal) are the thin way in. They read the file or the bytes into
 * *out, which the caller releases with imported_score_destroy, print nothing, and put the human-readable message of a
 * failure into error (NULL for none; its path is the file's).
 *
 * URSATZ_OK on success; URSATZ_ERR_REQUEST for a NULL path, NULL or empty bytes or a NULL out; URSATZ_ERR_ALLOC when memory runs
 * out; URSATZ_ERR_NOT_FOUND for a file that cannot be read; URSATZ_ERR_FORMAT for every other failure (a format that cannot be told,
 * a document that is not readable MIDI or MusicXML, a compressed or empty score). On a failure *out is untouched.
 */
UrsatzStatus analyzer_import_file(const char *path, ImportedFormat format, ImportedScore *out, UrsatzError *error);

/* ------------------------------------------------------------------ analysis */

typedef struct AnalyzerResult AnalyzerResult;

/*
 * The stages of a run. `stages` holds ANALYSIS_CHAIN_STAGE_* bits (chain/analysis_chain.h): bits 0 to 11 are the library chain,
 * bits 12 to 17 the stages of the collected result only. The chain stages pull in the stages they need.
 *
 * The report view, and so the document, is collected only when every chain stage (ANALYSIS_CHAIN_STAGE_PERCEPTION) is selected.
 * With fewer, the result holds the chain's results of those stages, and analyzer_result_report is an empty result.
 * The result-only stages read the chain's results: a bit set without the chain stage it reads gives no entries.
 */
typedef struct {
    unsigned stages;
    /* The knowledge snapshot the analysis reads its pitch materials from (borrowed). Required: the defaults carry none, so
       analyzer_analyze with no knowledge is URSATZ_ERR_REQUEST. It must outlive the result: the keys of the result point into it. */
    const KnowledgeSnapshot *knowledge;
} AnalyzerOptions;

/* All stages, the chain's default settings (chords on harmonic spans), no knowledge snapshot (the caller sets one). */
AnalyzerOptions analyzer_options_default(void);

/*
 * Analyzes notes. options may be NULL for the defaults, which have no knowledge snapshot, so every caller passes options with one
 * (URSATZ_ERR_REQUEST otherwise). On URSATZ_OK *out is the result, which the caller releases with
 * analyzer_result_destroy and which borrows notes: notes must outlive it. On any other status *out is left NULL and
 * nothing is kept: URSATZ_ERR_REQUEST for a NULL notes, options with no stages, no knowledge or out, URSATZ_ERR_ALLOC,
 * URSATZ_ERR_INTERNAL when the chain refuses the notes.
 */
UrsatzStatus analyzer_analyze(const ImportedScore *notes, const AnalyzerOptions *options, AnalyzerResult **out);

/* NULL is a no-op. */
void analyzer_result_destroy(AnalyzerResult *result);

/* The realization of the notes (R-628) and the Analyzer's reading of it (R-630): the reading's target names the work, the movement and
   the realization's digest, its evidence the performer notes of the realization, its detail the chain of the result. Never NULL for a
   result; both are owned by it. */
const Work *analyzer_result_realization(const AnalyzerResult *result);
const Reading *analyzer_result_reading(const AnalyzerResult *result);

/* ------------------------------------------------------------------ document */

/* An in-memory analysis document: an export of an AnalyzerResult. The JSON file is rendered from it. */
typedef struct AnalysisDocument AnalysisDocument;

/*
 * Builds the document from an analysis result: analysis_document_build_from_reading over the result's reading, notes and knowledge.
 * NULL for a NULL result, a result that did not run every chain stage, or on allocation failure. The document does not reference the
 * result after returning.
 */
AnalysisDocument *analyzer_document_build(const char *input_path, const AnalyzerResult *result);

/*
 * Serializes document as pretty-printed JSON with a 2-space indent and a
 * fixed key order, ending in a newline. Returns a heap string the caller
 * frees, or NULL on allocation failure.
 */
char *analyzer_document_to_json(const AnalysisDocument *document);

void analyzer_document_destroy(AnalysisDocument *document);

/* ------------------------------------------------------------------ staleness */

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

/* Compares the three keys with ANALYZER_DOCUMENT_SCHEMA_VERSION, analyzer_version() and
   analyzer_producer_key(). NULL strings are UNREADABLE. Never returns UNREADABLE for non-NULL strings. */
AnalyzerDocumentStatus analyzer_document_status_of(int schema_version, const char *written_by, const char *written_against);

/* Reads schema_version and metadata (analyzer_version, producer_key) from an analysis document's JSON text, and
   returns analyzer_document_status_of() of them. A document whose schema_version is a number other than the current one
   and whose metadata.analyzer_version is a string is STALE_SCHEMA whatever its other members. Nothing else in the
   document is read. */
AnalyzerDocumentStatus analyzer_document_status(const char *document_json);


/* ------------------------------------------------------------------ score */

/* What the export writes besides the notes: the work title and the composer, which the caller keeps with the piece. A NULL member
 * or an empty one is left out. */
typedef struct {
    const char *title;    /* <work><work-title> */
    const char *composer; /* <creator type="composer"> */
} AnalyzerScoreMetadata;

/*
 * The score of an import as MusicXML (R-218), written the way the GUI wrote it before the Analyzer owned it: from the
 * imported notes, their parts, staves and General MIDI programs, the source's own meter, and its key signatures in
 * part 0, with times quantized to notated values (score_quantize_options_default). Tempo markings and dynamics are left
 * out. The title and composer are not in an import: the caller gives them in metadata (NULL for none), and they are
 * written in the score's own <work> and <identification>.
 *
 * On URSATZ_OK *xml is a MusicXML 3.1 string the caller frees with free(); otherwise *xml is NULL.
 * URSATZ_ERR_REQUEST for a NULL argument or an import with a note count but no events; URSATZ_ERR_INTERNAL when the
 * library cannot write the score (overlapping or unwritable times, a missing spelling); URSATZ_ERR_ALLOC. On a failure error (NULL for
 * none) holds a message.
 */
UrsatzStatus analyzer_score_export_musicxml(const ImportedScore *score, const AnalyzerScoreMetadata *metadata, char **out,
                                            UrsatzError *error);

/*
 * The base xml:id of each note of an import in the MusicXML analyzer_score_export_musicxml writes (R-273), in
 * ImportedScore order: the library's musicxml_export_note_xml_id of the note's EntityID value, cut to 63 characters as
 * the export cuts it. A tied note's segments carry this id with the suffixes the export adds (_a to _z, then _26 and
 * on), so a caller matches an id as the base or the base plus a suffix.
 *
 * On URSATZ_OK *ids is an array of *count strings (NULL and 0 for an import with no notes) that the caller frees with
 * analyzer_score_note_xml_ids_destroy; otherwise *ids is NULL and *count is 0. URSATZ_ERR_REQUEST for a NULL argument
 * or an import with a note count but no events; URSATZ_ERR_INTERNAL when an id cannot be formed; URSATZ_ERR_ALLOC.
 */
UrsatzStatus analyzer_score_note_xml_ids(const ImportedScore *notes, char ***ids, size_t *count, UrsatzError *error);

/* Frees what analyzer_score_note_xml_ids gave. NULL is allowed. */
void analyzer_score_note_xml_ids_destroy(char **ids, size_t count);

/*
 * True when the import's meter was not read from the source: its meter map is missing or defaulted to 4/4
 * (meter_map_is_defaulted), as for a MIDI file with no time signature. False for a NULL argument.
 */
bool analyzer_imported_meter_defaulted(const ImportedScore *notes);


/* ------------------------------------------------------------------ playback */

/*
 * Playback timing of an import: the tempo regions and the notes as a player needs them. It is what the GUI asked the
 * library for before the Analyzer owned it (R-218), with the same values: the tempo source names, the tempo rounded to
 * thousandths of a quarter note per minute, the MIDI number clamped to 0 to 127.
 *
 * tempo_source is "file" (the MIDI file's own set-tempo events), "score" (a MusicXML score's declared tempo and
 * metronome marks) or "default" (the file or score gives none, so 120 quarter notes per minute from 0). It points to a
 * static string. tempo is in time order, never empty, the first region at 0; a MusicXML metronome mark at the position of
 * an earlier region replaces it. at is in whole notes. When a notation reading with a value scale was applied, the
 * tempo is multiplied by the scale (a scale of 2 doubles it) so the piece sounds as fast as before; a MIDI file's region
 * positions are scaled too, a score's marks were scaled with its notes.
 *
 * notes are in the order of the ImportedScore, index being that order. onset and length are in whole notes, midi is
 * 0 to 127, part is the ImportedScore stream (the part index). end is the latest onset plus length, 0 for no notes.
 */
typedef struct {
    Rational at;
    double quarters_per_minute;
} AnalyzerTempoRegion;

typedef struct {
    size_t index;
    Rational onset;
    Rational length;
    int midi;
    size_t part;
} AnalyzerPlaybackNote;

typedef struct {
    const char *tempo_source;
    AnalyzerTempoRegion *tempo;
    size_t tempo_count;
    AnalyzerPlaybackNote *notes; /* NULL when note_count is 0 */
    size_t note_count;
    Rational end;
} AnalyzerPlayback;

/*
 * Builds the playback of an import (input_pipeline_import_file or a pipeline import: a MIDI import carries its tempo
 * map, B4). URSATZ_OK fills *out, which the caller releases with analyzer_playback_destroy; any other status leaves it
 * zeroed. URSATZ_ERR_REQUEST for a NULL argument or a MIDI import with no tempo map; URSATZ_ERR_INTERNAL for a tempo
 * region the library cannot read or a time that does not fit; URSATZ_ERR_ALLOC.
 */
UrsatzStatus analyzer_playback_build(const ImportedScore *notes, AnalyzerPlayback *out);

/* Releases a playback and zeroes it. NULL is a no-op. */
void analyzer_playback_destroy(AnalyzerPlayback *playback);

/*
 * The playback as JSON text for a player: {"ok":true,"tempo_source":s,"tempo":[{"at":[num,den],"qpm":q}],
 * "parts":[{"index":i,"name":s,"program":0}],"end":[num,den],"notes":[{"i":i,"on":[num,den],"len":[num,den],
 * "pitch":m,"part":p}]}. Positions are [numerator, denominator] pairs in whole notes. parts lists each part that has a
 * note, by index; the name is the MusicXML part name, empty for a MIDI file. notes is the ImportedScore the playback was
 * built from. On URSATZ_OK *out is a string the caller frees with free(); otherwise *out is NULL.
 */
UrsatzStatus analyzer_playback_to_json(const AnalyzerPlayback *playback, const ImportedScore *notes, char **out);


/* ------------------------------------------------------------------ text export */

/*
 * The text export (docs/text-export.md): what the Score and Analysis tabs show, as plain text a person can paste into a
 * conversation to say "measure 27 is flagged as X, I read it as Y". The notes are read back from the score MusicXML
 * (analyzer_score_export_musicxml), so the text shows exactly what the Score tab draws, wrong or not; the claims come
 * from the analysis document, placed in the measure where each starts.
 */
typedef struct {
    /* Printed as "title:". NULL or empty: the file name the document records, else "untitled". */
    const char *title;
    /* An inclusive range of measure numbers as written in the score. 0 and 0 export the whole piece. A pickup measure
       (number 0) is included when first_measure is 0 or 1. */
    int first_measure;
    int last_measure;
} AnalyzerTextExportOptions;

/* The whole piece, no title. */
AnalyzerTextExportOptions analyzer_text_export_options_default(void);

/*
 * Exports one analyzed piece. notes is the import the analysis was run on; document_json is its analysis document
 * (analysis_document_render_result or the stored analysis.json).
 *
 * On URSATZ_OK *text is the export, which the caller frees with free(); otherwise *text is NULL.
 * URSATZ_ERR_REQUEST for a NULL argument, a document that is not JSON or whose schema_version is not
 * ANALYZER_DOCUMENT_SCHEMA_VERSION, or an empty range (first greater than last, or no measure in range);
 * URSATZ_ERR_INTERNAL when the score cannot be written or read back; URSATZ_ERR_ALLOC. On a failure error (NULL for none) holds a
 * message.
 */
UrsatzStatus analyzer_text_export_piece(const ImportedScore *notes, const char *document_json,
                                          const AnalyzerTextExportOptions *options, char **text, UrsatzError *error);

/* A name and its value, printed as "<name>: <value>" on one line (a line break in either becomes a space). */
typedef struct {
    const char *name;
    const char *value;
} AnalyzerTextExportFact;

/*
 * A run of notes in one section that the caller names, printed as "[label]" after the segment's first note and
 * "[/label]" after its last. first and last are 0-based indexes into the section's notes in time order, inclusive; a
 * chord is one note, rests are not counted, and notes that start together go by part, staff and voice.
 */
typedef struct {
    const char *label;
    size_t first;
    size_t last;
} AnalyzerTextExportSegment;

/* One MusicXML score of a piece of material (a motif, one of its variants, a theme, a loop). */
typedef struct {
    const char *label;
    /* The score as MusicXML (or MIDI) bytes, NUL-terminated for MusicXML. NULL: the section is printed as "not
       composed", with its label and facts only. */
    const char *musicxml;
    const AnalyzerTextExportFact *facts;
    size_t fact_count;
    const AnalyzerTextExportSegment *segments;
    size_t segment_count;
} AnalyzerTextExportSection;

/* A piece of store material: a motif and its variants, a theme and its slot motifs, a piece's parts. */
typedef struct {
    const char *kind; /* printed as given: "motif", "theme", "piece" */
    const char *id;
    const char *name;
    int version;
    const AnalyzerTextExportFact *facts;
    size_t fact_count;
    const AnalyzerTextExportSection *sections;
    size_t section_count;
    /* The knowledge snapshot the sections are analyzed with (borrowed; URSATZ_ERR_REQUEST without it). */
    const KnowledgeSnapshot *knowledge;
} AnalyzerTextExportMaterial;

/*
 * Exports material: a header (kind, name, id, version, facts), then each section as "## <label>", its facts and its
 * measures, analyzed and formatted as analyzer_text_export_piece does. Every value is printed as the caller passed it;
 * the Analyzer reads no store or Composer format.
 *
 * On URSATZ_OK *text is the export, which the caller frees with free(); otherwise *text is NULL.
 * URSATZ_ERR_REQUEST for a NULL material, kind, knowledge or text, a NULL label, or a segment outside its section's notes;
 * URSATZ_ERR_INTERNAL when a section's bytes cannot be imported or its score cannot be written or read back;
 * URSATZ_ERR_ALLOC.
 */
UrsatzStatus analyzer_text_export_material(const AnalyzerTextExportMaterial *material, char **text, UrsatzError *error);


/* ------------------------------------------------------------------ style profile */

/*
 * The Analyzer's entries to a style profile (the profile, its counting, its comparison and its model file are the library's,
 * theory/style/style_profile.h, R-617): a profile counts analysis documents. It reads only the documents, never the scores
 * (R-079).
 */

/* Counts one analysis document (the text of an "analysis-document" with schema_version 13). */
StyleProfileResult analyzer_style_add_document_json(StyleProfile *profile, const char *json);


/* ------------------------------------------------------------------ motive links */

/*
 * Motive links: which motive families of different pieces are variants of one another (R-154). A family's prototype
 * is read from the "features" of its analysis document, as a melodic sketch (no chords) in the key region of its first
 * note, moved to the key of C; two families are linked when the library's variant recognizer finds a chain of at most max_depth relations
 * from one to the other (analysis/motive_link.h). Like the style profile, it reads only analysis documents (R-079).
 *
 * A scope is a set of pieces in the order they were added. For each family the nearest families of other pieces are
 * its candidates: those at nearness at least min_nearness, nearest first (ties by piece order, then family index), at
 * most `candidates`. The union of these as unordered pairs is compared once each, the earlier family in scope order
 * as the source, unless the cache has the pair. Linked families are grouped by union.
 *
 * Incremental: adding one piece to a scope and running again compares only pairs with a family of that piece. A new
 * candidate can only push an old pair out of a family's top list, never into it, so every other pair is in the cache.
 */

typedef struct MotiveLinkScope MotiveLinkScope;
typedef struct MotiveLinkCache MotiveLinkCache;

/*
 * An empty scope that reads the materials of the documents it adds from knowledge. The snapshot is borrowed and must outlive the
 * scope: a family's sketch points to its material. NULL for a NULL snapshot or on allocation failure.
 */
MotiveLinkScope *analyzer_links_scope_create(const KnowledgeSnapshot *knowledge);
void analyzer_links_scope_destroy(MotiveLinkScope *scope);

/*
 * Adds the piece whose analysis document is json: every motive family's features become a sketch (the same one the
 * Analyzer fingerprints, motive_features_to_sketch). A piece without families is added with none. On any result other
 * than URSATZ_OK the scope is unchanged.
 */
UrsatzStatus analyzer_links_scope_add_document(MotiveLinkScope *scope, const char *piece_id, const char *json);

typedef struct {
    MotiveLinkOptions link;
    size_t candidates; /* per family, default 4 */
    /* Optional: called with (0, total) when the pairs are known and after each pair, compared or cached. */
    void (*progress)(size_t done, size_t total, void *context);
    void *progress_context;
    /* Optional: asked before each pair is compared; true stops the run with URSATZ_ERR_CANCELLED (nothing is returned). */
    bool (*cancelled)(void *user);
    void *cancel_user;
} MotiveLinksOptions;

/* link as motive_link_options_default, 4 candidates, no progress or cancel callback. */
void analyzer_links_options_default(MotiveLinksOptions *options);

/* One end of a link: a family of a piece. degrees is the prototype's, joined by spaces. */
typedef struct {
    char *piece;
    size_t family;
    int first_measure;
    int last_measure;
    double beat;
    char *degrees;
} MotiveLinkEnd;

#define MOTIVE_LINK_MAX_STEPS MOTIVE_RECOGNIZE_MAX_DEPTH

/*
 * A linked pair. a is the earlier family in scope order. reversed says the chain runs from b to a, else from a to b;
 * steps are in the order they apply. distance is the recognizer's, exact; similarity is indexed by MotiveDimension.
 */
typedef struct {
    MotiveLinkEnd a;
    MotiveLinkEnd b;
    bool reversed;
    MotiveRelationStep steps[MOTIVE_LINK_MAX_STEPS];
    char *step_text[MOTIVE_LINK_MAX_STEPS];
    size_t step_count;
    unsigned long long distance_numerator;
    unsigned long long distance_denominator;
    double similarity[MOTIVE_DIMENSION_COUNT];
} MotiveLinkFound;

typedef struct {
    char *piece;
    size_t family;
} MotiveLinkMember;

/* Families joined by links, at least two, in scope order. */
typedef struct {
    MotiveLinkMember *members;
    size_t member_count;
} MotiveLinkGroup;

typedef struct {
    MotiveLinkFound *links; /* in scope order of a, then of b */
    size_t link_count;
    MotiveLinkGroup *groups; /* largest first, then by first member in scope order */
    size_t group_count;
    size_t pieces;
    size_t pairs;    /* considered */
    size_t compared; /* run through the recognizer now */
    size_t cached;   /* taken from the cache */
    bool cache_reset; /* the cache was for another producer key, analyzer version or set of link options (or an older schema) and was emptied */
} MotiveLinks;

/*
 * The cache of compared pairs, kept by the two sketches' fingerprints so one cache serves every scope (R-156): a pair
 * compared for one scope is not compared again for another. It is valid only for the producer key, analyzer version and
 * link options it was made with; analyzer_links_run empties one that differs and says so (MotiveLinks.cache_reset).
 *
 * Load: a file that does not exist is an empty cache, and so is a file of an older schema version (it is keyed with
 * nothing, so the next run empties it and says so). Save: written to <path>.tmp and renamed over path. The file
 * follows schema/motive-link-cache.v2.schema.json.
 */
UrsatzStatus analyzer_links_cache_load(const char *path, MotiveLinkCache **out);
UrsatzStatus analyzer_links_cache_save(const MotiveLinkCache *cache, const char *path);
void analyzer_links_cache_destroy(MotiveLinkCache *cache);


/*
 * Links the scope's families. cache may be NULL (nothing is kept between runs); otherwise it is read and new results are
 * added to it, in memory: the caller saves it. *out is written on URSATZ_OK only.
 */
UrsatzStatus analyzer_links_run(const MotiveLinkScope *scope, MotiveLinkCache *cache,
                                   const MotiveLinksOptions *options, MotiveLinks *out);

void analyzer_links_destroy(MotiveLinks *links);

/*
 * The links as JSON text (schema/motive-links.v1.schema.json), formatted, with a final newline, newly allocated
 * and freed with free(). The same links give the same bytes.
 */
UrsatzStatus analyzer_links_render(const MotiveLinks *links, char **out);


/* ------------------------------------------------------------------ corpus passages */

/*
 * Passages of an analyzed corpus, ready to give to the Composer (R-619). The Composer API seeds a motive or a progression from
 * passages given inline in a request (`corpus_passages`); this is how an orchestrator gets them: the search is the Analyzer's
 * (similar_material.h, the same ranking), and each match's document is read once, for the chords and for whether it is current.
 * Only the analysis documents of the corpus are read, never the scores (R-079).
 */

/* One soprano note of a passage: a MIDI note number, the onset from the passage's start and the duration, in whole notes. */
typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} AnalyzerPassageNote;

/* One chord of the matched piece, as its analysis document joins them (analyzer_chord_sequence_from_document): the
   root-position numeral in the Analyzer's words, where it starts (the beat plus the fraction of it) and the key label of the
   entry it starts at ("C major", "" when the entry has none). */
typedef struct {
    char *numeral;
    int measure;
    double beat;
    char *key_label;
} AnalyzerPassageChord;

/*
 * One match. piece is the piece's path in the corpus as the corpus index lists it. The place is where the passage starts and ends
 * (a measure and a beat, the beat plus the fraction of it: 2.5 is half way through beat 2). similarity is the Analyzer's score in
 * [0, 1], 1 is identical. notes are the passage's soprano notes (NULL with note_count 0 when it has none or one does not fit a
 * Rational); chords are every chord of the piece in time order (NULL with chord_count 0 when none), so a reader finds the
 * chords from the one that starts where the passage does. stale is true when the document was analyzed by another version
 * (staleness.h); it is read all the same.
 */
typedef struct {
    char *piece;
    int start_measure;
    double start_beat;
    int end_measure;
    double end_beat;
    Rational similarity;
    AnalyzerPassageNote *notes;
    size_t note_count;
    AnalyzerPassageChord *chords;
    size_t chord_count;
    bool stale;
} AnalyzerCorpusPassage;

/* Matches, best first, then by piece path and start (similar_material_find's order). */
typedef struct {
    AnalyzerCorpusPassage *passages;
    size_t count;
} AnalyzerCorpusPassages;

/*
 * The passages of the corpus (the output directory of `ursatz-analyzer --corpus`) that look like the melody of the MIDI or
 * MusicXML file at passage_path: the best `max` with a similarity of 1/2 or more. No match is URSATZ_OK with
 * count 0. `knowledge` reads the passage and the documents (URSATZ_ERR_REQUEST without it). out is filled only on
 * URSATZ_OK and released with analyzer_corpus_passages_destroy; on any other status it is empty.
 */
UrsatzStatus analyzer_corpus_passages_like(const char *corpus_dir, const KnowledgeSnapshot *knowledge,
                                                     const char *passage_path, size_t max, AnalyzerCorpusPassages *out);

/*
 * The passages that look like the progression `numerals` (Roman numerals separated by spaces or '-', as
 * similar_query_from_numerals reads them): the best `max` with a similarity of 1/2 or more. Otherwise as
 * analyzer_corpus_passages_like.
 */
UrsatzStatus analyzer_corpus_passages_numerals(const char *corpus_dir, const KnowledgeSnapshot *knowledge,
                                                         const char *numerals, size_t max, AnalyzerCorpusPassages *out);

/* Frees the passages and clears *passages. Safe on a value that was never filled. */
void analyzer_corpus_passages_destroy(AnalyzerCorpusPassages *passages);

/*
 * The passages as the Composer API's `corpus_passages` request member (schema composer/schema/api/corpus-passages.schema.json):
 * a JSON array text, each passage {"piece", "start": {"measure", "beat"}, "end": {"measure", "beat"}, "similarity": {"num",
 * "den"}, "stale", "notes": [{"midi", "onset": {"num", "den"}, "duration": {"num", "den"}}], "chords": [{"numeral", "measure",
 * "beat", "key"}]}. A heap string the caller frees; NULL on allocation failure or a NULL argument. An empty list is "[]".
 */
char *analyzer_corpus_passages_to_json(const AnalyzerCorpusPassages *passages);


#endif
