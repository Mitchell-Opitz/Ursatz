#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_ANALYSIS_GALANT_ANALYSIS_MATCH_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_ANALYSIS_GALANT_ANALYSIS_MATCH_H

#include "galant_analysis_input.h"
#include "galant-schemas/galant_prototype.h"
#include "interpretation/interpretation.h"
#include "relationship/relationship.h"

/*
 * Schema matching (the galant-schemas adapter): the prototypes of galant_prototypes.h (none added) matched against the outer-voice degrees of
 * a run of segments (galant_analysis_input.h).
 *
 * A MATCH assigns the prototype's stages, in order, to segments of the input:
 *   stage degrees   a stage's melody degree and bass degree are checked against the segment's soprano and bass with galant_degree_allows (an
 *                   accepted alteration counts: la/le and mi/me take either form). A prototype whose bass is not defining (the Meyer) takes a bass
 *                   other than its usual one at a lower credit; one whose bass is defining does not
 *   stage meter     the prototype's strong or weak beat (the METER rule): a segment on a STRONG or MEDIUM beat is on a strong beat, a WEAK one on a
 *                   weak beat, and the stage must be on the beat the prototype gives it
 *   two-note stage  a stage with a second melody pitch (the Monte's do-te and re-do) takes the next segment too, which must keep the bass degree
 *                   of the first and have the second pitch in the soprano
 *   bass-only stage a segment with no soprano may take a stage on its bass alone, at most config.bass_only_allowance times in a match (1), at
 *                   half the credit
 *   gaps            between one stage and the next, segments that embellish may lie: their total length from the end of one stage to the start
 *                   of the next is at most config.max_gap_beats beats (2), counted in the beat of the earlier segment
 *   key-relative    the stages are read in ONE frame key: a degree of the segment's local key is converted by the keys' tonics to that frame, so a
 *                   schema that moves through keys (the Fonte begins in the key of ii, the Monte climbs through IV and V) is read as its prototype
 *                   has it, in the key it is written in. The frames tried are the home key and the key of each segment from the first stage to the
 *                   end of its reach; the match records the frame it was found in
 *   cross-check     the stages are also assembled as a GalantExemplar (degrees as pitches on a C tonic; a bass-only stage takes the prototype's
 *                   own melody) and the match is kept only if galant_matching_check reports no hard violation, so the framework's rules decide
 *
 * CONFIDENCE = pair share x metric fit x (1 - mean uncertainty):
 *   pair share   the mean credit of the stages: 1 for a stage whose melody and bass both match, 0.75 for a non-defining bass that is not the usual
 *                one, 0.5 for a bass-only stage
 *   metric fit   1 when every stage's strength is at least that of every segment between it and the stage before (STRONG over MEDIUM over WEAK),
 *                0.8 when some stage is weaker than an embellishing segment
 *   uncertainty  the mean of the uncertainty of the segments the stages use
 *
 * OVERLAPS. Matches that share a segment are resolved by confidence (the more stages, then the earlier, break a tie): the better is kept and the
 * others that overlap it dropped. One boundary segment may be shared, the last stage of an earlier match and the first of a later one, when the
 * earlier prototype's SchemaFunction does not come after the later's in the order opening gambit, continuation, pre-cadential, cadence,
 * post-cadential (schemas that lead on to one another).
 *
 * OUTPUT, per match: a Relationship over the stage segments (from the first stage's segment to the last's, predicate "galant-schema") and an
 * Interpretation (FrameworkReference galant-schemas, scalar confidence = 1 - uncertainty) targeting the Relationship's id, with the claim
 * "galant-schemas/schema": the schema name, the stage list with the segment refs and the degrees as read, the frame, and the confidence parts.
 */

#define GALANT_SCHEMA_CLAIM_TYPE "galant-schemas/schema"

typedef struct {
    double max_gap_beats;
    unsigned bass_only_allowance;
} GalantMatchConfig;

/* max_gap_beats 2, bass_only_allowance 1. */
GalantMatchConfig galant_match_config_default(void);

typedef struct {
    char segment_id[GALANT_ANALYSIS_ID_MAX];
    char second_segment_id[GALANT_ANALYSIS_ID_MAX]; /* the second melody note of a two-note stage; empty otherwise */
    int has_melody;
    int melody_degree;
    int melody_alteration;
    int bass_degree;
    int bass_alteration;
    int has_second;
    int second_degree;
    int second_alteration;
    int bass_only;
    double credit;
} GalantStageClaim;

typedef struct {
    int schema; /* GalantSchemaId */
    char name[32];
    int function; /* SchemaFunction */
    int stage_count;
    GalantStageClaim stages[GALANT_MAX_STAGES];
    int frame_degree;
    int frame_alteration;
    int bass_only_stages;
    int first_segment; /* the index of the first stage's segment in the input */
    int last_segment;  /* the index of the last segment the stages use */
    double pair_share;
    double metric_fit;
    double mean_uncertainty;
    double confidence;
} GalantSchemaClaim;

typedef struct {
    GalantSchemaClaim claim;
    double uncertainty;             /* 1 - confidence */
    Relationship *relationship;     /* owned */
    Interpretation *interpretation; /* owned */
} GalantSchemaMatch;

typedef struct {
    GalantSchemaMatch *items; /* in order of the first stage; NULL when none */
    size_t count;
} GalantSchemaMatches;

/* Matches every prototype against the segments and resolves the overlaps. `config` may be NULL for the defaults. *out is untouched on failure. */
GalantAnalysisStatus galant_schema_match(const GalantSegmentList *segments, const GalantMatchConfig *config, GalantSchemaMatches *out);

void galant_schema_matches_destroy(GalantSchemaMatches *matches);

/* Reads the claim back out of a match's Interpretation. False if it is not one. */
bool galant_schema_claim_read(const Interpretation *interpretation, GalantSchemaClaim *out);

#endif
