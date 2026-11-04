#include "galant_analysis_match.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "galant_analysis_claim.h"
#include "galant_matching_rules.h"
#include "galant_prototypes.h"
#include "core/rational.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "pitch/pitch_spelling.h"
#include "time/time_rational.h"

#define NONE ((size_t)-1)
#define MAX_FRAMES 8

GalantMatchConfig galant_match_config_default(void) {
    GalantMatchConfig config = {2.0, 1};
    return config;
}

/* --- exact times --- */

/* The time of a segment edge, read exactly. False when it does not fit a Rational. */
static bool seg_start(const GalantSegmentList *s, size_t i, Rational *out) {
    return rational_of_musical_time(time_span_start(s->items[i].span), out) == RATIONAL_OK;
}

static bool seg_end(const GalantSegmentList *s, size_t i, Rational *out) {
    return rational_of_musical_time(time_span_end(s->items[i].span), out) == RATIONAL_OK;
}

/* The start of segment `i` less `from`, as whole notes. False when a step does not fit a Rational. */
static bool gap_from(const GalantSegmentList *s, size_t i, Rational from, Rational *gap) {
    Rational start;
    return seg_start(s, i, &start) && rational_sub(start, from, gap) == RATIONAL_OK;
}

static double seg_beat(const GalantSegmentList *s, size_t i) {
    return (double)s->items[i].beat_numerator / (double)s->items[i].beat_denominator;
}

/* --- a candidate match --- */

typedef struct {
    GalantSchemaId id;
    GalantPitchDegree frame;
    size_t stage_count;
    size_t seg[GALANT_MAX_STAGES];
    size_t seg2[GALANT_MAX_STAGES];
    double credit[GALANT_MAX_STAGES];
    bool bass_only[GALANT_MAX_STAGES];
    double share;
    double fit;
    double uncertainty;
    double confidence;
    size_t first;
    size_t last;
} Candidate;

typedef struct {
    const GalantSegmentList *segments;
    GalantMatchConfig config;
    const GalantPrototype *prototype;
    GalantPitchDegree frame;
    Candidate *found;
    size_t found_count;
    size_t found_capacity;
    bool failed;
} Search;

static BeatStrength stage_strength(BeatStrength strength) {
    return strength == BEAT_STRENGTH_WEAK ? BEAT_STRENGTH_WEAK : BEAT_STRENGTH_STRONG;
}

static int rank_of(BeatStrength strength) {
    return (int)strength; /* STRONG 0, MEDIUM 1, WEAK 2: a smaller rank is stronger */
}

typedef struct {
    bool ok;
    double credit;
    bool bass_only;
    size_t second;
} StageEval;

static GalantPitchDegree in_frame(const GalantSegment *segment, GalantPitchDegree degree, GalantPitchDegree frame) {
    return galant_degree_in_frame(degree, segment->key, frame);
}

static StageEval eval_stage(const GalantSegmentList *s, size_t j, const GalantStage *stage, bool bass_defining,
                            GalantPitchDegree frame, bool bass_only_free) {
    StageEval eval = {false, 0.0, false, NONE};
    const GalantSegment *segment = &s->items[j];
    if (stage_strength(segment->strength) != stage->strength || !segment->has_bass) {
        return eval;
    }
    GalantPitchDegree bass = in_frame(segment, segment->bass, frame);
    bool bass_ok = galant_degree_allows(&stage->bass, &bass);
    if (!bass_ok && bass_defining) {
        return eval;
    }
    if (!segment->has_soprano) {
        if (!bass_only_free || !bass_ok || stage->melody_next.degree != 0) {
            return eval;
        }
        eval.ok = true;
        eval.bass_only = true;
        eval.credit = 0.5;
        return eval;
    }
    GalantPitchDegree melody = in_frame(segment, segment->soprano, frame);
    if (!galant_degree_allows(&stage->melody, &melody)) {
        return eval;
    }
    if (stage->melody_next.degree != 0) {
        if (j + 1 >= s->count) {
            return eval;
        }
        const GalantSegment *next = &s->items[j + 1];
        if (!next->has_bass || !next->has_soprano ||
            !galant_degree_equal(in_frame(next, next->bass, frame), bass)) {
            return eval;
        }
        GalantPitchDegree second = in_frame(next, next->soprano, frame);
        if (!galant_degree_allows(&stage->melody_next, &second)) {
            return eval;
        }
        eval.second = j + 1;
    }
    eval.ok = true;
    eval.credit = bass_ok ? 1.0 : 0.75;
    return eval;
}

static void push_candidate(Search *search, const Candidate *candidate) {
    if (search->found_count == search->found_capacity) {
        size_t next = search->found_capacity == 0 ? 64 : search->found_capacity * 2;
        Candidate *grown = (Candidate *)realloc(search->found, next * sizeof(Candidate));
        if (grown == NULL) {
            search->failed = true;
            return;
        }
        search->found = grown;
        search->found_capacity = next;
    }
    search->found[search->found_count++] = *candidate;
}

static PitchLetter letter_of(int degree) {
    static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    return LETTERS[(degree - 1) % 7];
}

static PitchSpelling *spell(GalantPitchDegree degree, int octave) {
    PitchSpelling *pitch = NULL;
    if (pitch_spelling_create(letter_of(degree.degree), degree.alteration, octave, &pitch) != PITCH_SPELLING_OK) {
        return NULL;
    }
    return pitch;
}

/* The stages as a GalantExemplar on a C tonic: kept only when the framework's hard matching rules hold. */
static bool framework_agrees(const Search *search, const Candidate *c) {
    const GalantSegmentList *s = search->segments;
    const GalantPrototype *prototype = search->prototype;
    GalantExemplarStage stages[GALANT_MAX_STAGES];
    PitchSpelling *owned[3 * GALANT_MAX_STAGES + 1];
    size_t owned_count = 0;
    bool ok = true;
    memset(stages, 0, sizeof(stages));
    for (size_t k = 0; ok && k < c->stage_count; k++) {
        const GalantStage *stage = &prototype->stages[k];
        const GalantSegment *segment = &s->items[c->seg[k]];
        int expected_alteration = stage->melody.alteration == GALANT_ALTERATION_LOWERED
                                      ? -1
                                      : (stage->melody.alteration == GALANT_ALTERATION_RAISED ? 1 : 0);
        GalantPitchDegree melody = c->bass_only[k] ? (GalantPitchDegree){stage->melody.degree, expected_alteration}
                                                   : in_frame(segment, segment->soprano, c->frame);
        GalantPitchDegree bass = in_frame(segment, segment->bass, c->frame);
        PitchSpelling *melody_pitch = spell(melody, 5);
        PitchSpelling *bass_pitch = spell(bass, 3);
        if (melody_pitch != NULL) {
            owned[owned_count++] = melody_pitch;
        }
        if (bass_pitch != NULL) {
            owned[owned_count++] = bass_pitch;
        }
        PitchSpelling *second_pitch = NULL;
        if (c->seg2[k] != NONE) {
            const GalantSegment *next = &s->items[c->seg2[k]];
            second_pitch = spell(in_frame(next, next->soprano, c->frame), 5);
            if (second_pitch != NULL) {
                owned[owned_count++] = second_pitch;
            }
        }
        ok = melody_pitch != NULL && bass_pitch != NULL && (c->seg2[k] == NONE || second_pitch != NULL);
        stages[k].melody = melody_pitch;
        stages[k].bass = bass_pitch;
        stages[k].strength = stage_strength(segment->strength);
        stages[k].melody_next = second_pitch;
    }
    PitchSpelling *tonic = spell((GalantPitchDegree){1, 0}, 4);
    bool agrees = false;
    if (ok && tonic != NULL) {
        GalantExemplar exemplar = {stages, c->stage_count, tonic};
        GalantMatch match = {prototype, &exemplar};
        unsigned hard = 0;
        unsigned soft = 0;
        agrees = galant_match_valid(&match) && galant_matching_check(&match, &hard, &soft) && hard == 0;
    }
    for (size_t i = 0; i < owned_count; i++) {
        pitch_spelling_destroy(owned[i]);
    }
    pitch_spelling_destroy(tonic);
    return agrees;
}

static void finish(Search *search, Candidate *c) {
    const GalantSegmentList *s = search->segments;
    double credit = 0.0;
    double uncertainty = 0.0;
    size_t used = 0;
    for (size_t k = 0; k < c->stage_count; k++) {
        credit += c->credit[k];
        uncertainty += s->items[c->seg[k]].uncertainty;
        used++;
        if (c->seg2[k] != NONE) {
            uncertainty += s->items[c->seg2[k]].uncertainty;
            used++;
        }
    }
    c->share = credit / (double)c->stage_count;
    c->uncertainty = uncertainty / (double)used;
    /* metric fit: every stage at least as strong as the segments between it and the stage before */
    bool strong_enough = true;
    for (size_t k = 1; k < c->stage_count; k++) {
        size_t previous_last = c->seg2[k - 1] != NONE ? c->seg2[k - 1] : c->seg[k - 1];
        for (size_t b = previous_last + 1; b < c->seg[k]; b++) {
            if (rank_of(s->items[c->seg[k]].strength) > rank_of(s->items[b].strength)) {
                strong_enough = false;
            }
        }
    }
    c->fit = strong_enough ? 1.0 : 0.8;
    c->confidence = c->share * c->fit * (1.0 - c->uncertainty);
    c->first = c->seg[0];
    c->last = c->seg2[c->stage_count - 1] != NONE ? c->seg2[c->stage_count - 1] : c->seg[c->stage_count - 1];
    if (framework_agrees(search, c)) {
        push_candidate(search, c);
    }
}

static void extend(Search *search, Candidate *c, size_t stage, size_t from, unsigned bass_only_used) {
    const GalantSegmentList *s = search->segments;
    const GalantPrototype *prototype = search->prototype;
    if (search->failed) {
        return;
    }
    if (stage == prototype->stage_count) {
        finish(search, c);
        return;
    }
    size_t previous_last = NONE;
    Rational previous_end = rational_ratio(0, 1);
    double previous_beat = 1.0;
    if (stage > 0) {
        previous_last = c->seg2[stage - 1] != NONE ? c->seg2[stage - 1] : c->seg[stage - 1];
        if (!seg_end(s, previous_last, &previous_end)) {
            return; /* a time that does not fit: no match is read from here */
        }
        previous_beat = seg_beat(s, previous_last);
    }
    for (size_t j = from; j < s->count && (stage > 0 || j == from); j++) {
        if (stage > 0) {
            Rational gap;
            if (!gap_from(s, j, previous_end, &gap)) {
                continue; /* the gap does not fit a Rational: this segment is refused */
            }
            if (rational_is_negative(gap)) {
                continue;
            }
            if (rational_to_double(gap) / previous_beat > search->config.max_gap_beats + 1e-9) {
                break;
            }
        }
        StageEval eval = eval_stage(s, j, &prototype->stages[stage], prototype->bass_defining, search->frame,
                                    bass_only_used < search->config.bass_only_allowance);
        if (!eval.ok) {
            continue;
        }
        c->seg[stage] = j;
        c->seg2[stage] = eval.second;
        c->credit[stage] = eval.credit;
        c->bass_only[stage] = eval.bass_only;
        size_t next_from = eval.second != NONE ? eval.second + 1 : j + 1;
        extend(search, c, stage + 1, next_from, bass_only_used + (eval.bass_only ? 1u : 0u));
    }
}

/* The frames to read the stages from segment `start` in: the home key and each key within reach. */
static size_t frames_for(const GalantSegmentList *s, size_t start, size_t stage_count, const GalantMatchConfig *config,
                         GalantPitchDegree *frames) {
    size_t count = 0;
    frames[count++] = (GalantPitchDegree){1, 0};
    double limit = (double)stage_count * (config->max_gap_beats + 4.0) * seg_beat(s, start);
    Rational begin;
    if (!seg_start(s, start, &begin)) {
        return count;
    }
    for (size_t j = start; j < s->count && count < MAX_FRAMES; j++) {
        Rational reach;
        if (!gap_from(s, j, begin, &reach) || rational_to_double(reach) > limit) {
            break;
        }
        bool seen = false;
        for (size_t f = 0; f < count; f++) {
            seen = seen || galant_degree_equal(frames[f], s->items[j].key);
        }
        if (!seen) {
            frames[count++] = s->items[j].key;
        }
    }
    return count;
}

/* --- overlaps --- */

static bool uses(const Candidate *c, size_t segment) {
    for (size_t k = 0; k < c->stage_count; k++) {
        if (c->seg[k] == segment || c->seg2[k] == segment) {
            return true;
        }
    }
    return false;
}

static int function_of(const Candidate *c) {
    return (int)galant_prototype(c->id)->function;
}

/* True when `a` and `b` may both be kept: they share no segment, or one boundary segment (a's last stage, b's first) of schemas whose
   functions lead on. */
static bool compatible(const Candidate *a, const Candidate *b) {
    size_t shared = 0;
    size_t shared_segment = NONE;
    for (size_t k = 0; k < a->stage_count; k++) {
        size_t segments[2] = {a->seg[k], a->seg2[k]};
        for (size_t m = 0; m < 2; m++) {
            if (segments[m] != NONE && uses(b, segments[m])) {
                shared++;
                shared_segment = segments[m];
            }
        }
    }
    if (shared == 0) {
        return true;
    }
    const Candidate *earlier = a->first <= b->first ? a : b;
    const Candidate *later = earlier == a ? b : a;
    size_t earlier_last_stage = earlier->stage_count - 1;
    size_t earlier_tail = earlier->seg2[earlier_last_stage] != NONE ? earlier->seg2[earlier_last_stage] : earlier->seg[earlier_last_stage];
    return shared == 1 && shared_segment == earlier_tail && shared_segment == later->seg[0] && earlier != later &&
           function_of(earlier) <= function_of(later);
}

static int compare_confidence(const void *x, const void *y) {
    const Candidate *a = (const Candidate *)x;
    const Candidate *b = (const Candidate *)y;
    if (fabs(a->confidence - b->confidence) > 1e-12) {
        return a->confidence > b->confidence ? -1 : 1;
    }
    if (a->stage_count != b->stage_count) {
        return a->stage_count > b->stage_count ? -1 : 1;
    }
    if (a->first != b->first) {
        return a->first < b->first ? -1 : 1;
    }
    return (a->id > b->id) - (a->id < b->id);
}

static int compare_first(const void *x, const void *y) {
    const Candidate *a = (const Candidate *)x;
    const Candidate *b = (const Candidate *)y;
    if (a->first != b->first) {
        return a->first < b->first ? -1 : 1;
    }
    return (a->id > b->id) - (a->id < b->id);
}

/* --- output --- */

static GalantAnalysisStatus emit(const GalantSegmentList *s, const Candidate *c, int index, GalantSchemaMatch *out) {
    const GalantPrototype *prototype = galant_prototype(c->id);
    GalantSchemaClaim *claim = &out->claim;
    memset(claim, 0, sizeof(*claim));
    claim->schema = (int)c->id;
    snprintf(claim->name, sizeof(claim->name), "%s", prototype->name);
    claim->function = (int)prototype->function;
    claim->stage_count = (int)c->stage_count;
    claim->frame_degree = c->frame.degree;
    claim->frame_alteration = c->frame.alteration;
    claim->first_segment = (int)c->first;
    claim->last_segment = (int)c->last;
    claim->pair_share = c->share;
    claim->metric_fit = c->fit;
    claim->mean_uncertainty = c->uncertainty;
    claim->confidence = c->confidence;
    for (size_t k = 0; k < c->stage_count; k++) {
        GalantStageClaim *stage = &claim->stages[k];
        const GalantSegment *segment = &s->items[c->seg[k]];
        snprintf(stage->segment_id, sizeof(stage->segment_id), "%s", segment->id);
        GalantPitchDegree bass = in_frame(segment, segment->bass, c->frame);
        stage->bass_degree = bass.degree;
        stage->bass_alteration = bass.alteration;
        stage->bass_only = c->bass_only[k] ? 1 : 0;
        stage->credit = c->credit[k];
        claim->bass_only_stages += stage->bass_only;
        if (!c->bass_only[k]) {
            GalantPitchDegree melody = in_frame(segment, segment->soprano, c->frame);
            stage->has_melody = 1;
            stage->melody_degree = melody.degree;
            stage->melody_alteration = melody.alteration;
        }
        if (c->seg2[k] != NONE) {
            const GalantSegment *next = &s->items[c->seg2[k]];
            GalantPitchDegree second = in_frame(next, next->soprano, c->frame);
            snprintf(stage->second_segment_id, sizeof(stage->second_segment_id), "%s", next->id);
            stage->has_second = 1;
            stage->second_degree = second.degree;
            stage->second_alteration = second.alteration;
        }
    }
    out->uncertainty = 1.0 - c->confidence;
    char key[96];
    char interpretation_id[160];
    snprintf(key, sizeof(key), "%s-%d", prototype->name, index);
    out->relationship = galant_analysis_relationship("match", key, s->items[c->first].id, s->items[c->last].id, "galant-schema");
    if (out->relationship == NULL) {
        return GALANT_ANALYSIS_ERR_ALLOC;
    }
    snprintf(interpretation_id, sizeof(interpretation_id), "galant-schemas-match-claim-%s", key);
    out->interpretation = galant_analysis_interpretation(interpretation_id, entity_id_value(relationship_id(out->relationship)),
                                                         GALANT_SCHEMA_CLAIM_TYPE, claim, sizeof(*claim), out->uncertainty,
                                                         "schema-matching");
    return out->interpretation == NULL ? GALANT_ANALYSIS_ERR_ALLOC : GALANT_ANALYSIS_OK;
}

GalantAnalysisStatus galant_schema_match(const GalantSegmentList *segments, const GalantMatchConfig *config, GalantSchemaMatches *out) {
    if (segments == NULL || out == NULL) {
        return GALANT_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    GalantMatchConfig fallback = galant_match_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    if (!(config->max_gap_beats >= 0.0)) {
        return GALANT_ANALYSIS_ERR_INVALID;
    }
    GalantSchemaMatches found = {NULL, 0};
    if (segments->count == 0) {
        *out = found;
        return GALANT_ANALYSIS_OK;
    }
    Search search;
    memset(&search, 0, sizeof(search));
    search.segments = segments;
    search.config = *config;
    for (int id = 0; !search.failed && id < (int)GALANT_SCHEMA_COUNT; id++) {
        const GalantPrototype *prototype = galant_prototype((GalantSchemaId)id);
        if (prototype == NULL) {
            continue;
        }
        search.prototype = prototype;
        for (size_t start = 0; !search.failed && start < segments->count; start++) {
            GalantPitchDegree frames[MAX_FRAMES];
            size_t frame_count = frames_for(segments, start, prototype->stage_count, config, frames);
            for (size_t f = 0; f < frame_count; f++) {
                Candidate candidate;
                memset(&candidate, 0, sizeof(candidate));
                candidate.id = (GalantSchemaId)id;
                candidate.frame = frames[f];
                candidate.stage_count = prototype->stage_count;
                for (size_t k = 0; k < GALANT_MAX_STAGES; k++) {
                    candidate.seg[k] = NONE;
                    candidate.seg2[k] = NONE;
                }
                search.frame = frames[f];
                extend(&search, &candidate, 0, start, 0);
            }
        }
    }
    if (search.failed) {
        free(search.found);
        return GALANT_ANALYSIS_ERR_ALLOC;
    }

    qsort(search.found, search.found_count, sizeof(Candidate), compare_confidence);
    size_t kept = 0;
    for (size_t i = 0; i < search.found_count; i++) {
        bool keep = true;
        for (size_t k = 0; keep && k < kept; k++) {
            keep = compatible(&search.found[i], &search.found[k]);
        }
        if (keep) {
            search.found[kept++] = search.found[i];
        }
    }
    qsort(search.found, kept, sizeof(Candidate), compare_first);

    GalantAnalysisStatus status = GALANT_ANALYSIS_OK;
    if (kept > 0) {
        found.items = (GalantSchemaMatch *)calloc(kept, sizeof(GalantSchemaMatch));
        if (found.items == NULL) {
            status = GALANT_ANALYSIS_ERR_ALLOC;
        }
    }
    for (size_t i = 0; status == GALANT_ANALYSIS_OK && i < kept; i++) {
        status = emit(segments, &search.found[i], (int)i, &found.items[i]);
        found.count = i + 1;
    }
    free(search.found);
    if (status != GALANT_ANALYSIS_OK) {
        galant_schema_matches_destroy(&found);
        return status;
    }
    *out = found;
    return GALANT_ANALYSIS_OK;
}

void galant_schema_matches_destroy(GalantSchemaMatches *matches) {
    if (matches == NULL) {
        return;
    }
    for (size_t i = 0; i < matches->count; i++) {
        interpretation_destroy(matches->items[i].interpretation);
        relationship_destroy(matches->items[i].relationship);
    }
    free(matches->items);
    matches->items = NULL;
    matches->count = 0;
}

bool galant_schema_claim_read(const Interpretation *interpretation, GalantSchemaClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), GALANT_SCHEMA_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
