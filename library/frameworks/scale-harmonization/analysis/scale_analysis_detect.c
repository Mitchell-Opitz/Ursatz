#include "scale_analysis_detect.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "pitch/pitch_spelling.h"
#include "scale_analysis_claim.h"
#include "scale_harmonization.h"
#include "scale_sequence_rules.h"

#define NONE ((size_t)-1)

ScaleDetectConfig scale_detect_config_default(void) {
    ScaleDetectConfig config = {3};
    return config;
}

/* --- the chord segments, in the home key --- */

typedef struct {
    size_t original;
    GalantPitchDegree bass;
    GalantPitchDegree soprano;
    GalantPitchDegree root;
    bool has_soprano;
    bool has_root;
    double uncertainty;
} Chord;

static int mod7(int value) {
    int result = value % 7;
    return result < 0 ? result + 7 : result;
}

/* The signed diatonic step from degree a to degree b, the shortest way round (-3 to 3). */
static int step_between(GalantPitchDegree a, GalantPitchDegree b) {
    int d = mod7(b.degree - a.degree);
    return d > 3 ? d - 7 : d;
}

static int root_motion(GalantPitchDegree a, GalantPitchDegree b) {
    return mod7(b.degree - a.degree);
}

static bool adjacent(const Chord *chords, size_t k) {
    return chords[k + 1].original == chords[k].original + 1;
}

/* --- spelled pitches in a C frame --- */

static PitchLetter letter_of(int degree) {
    static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    return LETTERS[(degree - 1) % 7];
}

static PitchSpelling *spell_at(GalantPitchDegree degree, long position) {
    PitchSpelling *pitch = NULL;
    if (pitch_spelling_create(letter_of(degree.degree), degree.alteration, (int)(position / 7), &pitch) != PITCH_SPELLING_OK) {
        return NULL;
    }
    return pitch;
}

typedef struct {
    ScaleStage *stages;
    PitchSpelling **owned;
    size_t owned_count;
} Stages;

static bool build_stages(const Chord *chords, size_t n, Stages *out) {
    memset(out, 0, sizeof(*out));
    out->stages = (ScaleStage *)calloc(n, sizeof(ScaleStage));
    out->owned = (PitchSpelling **)calloc(2 * n + 1, sizeof(PitchSpelling *));
    if (out->stages == NULL || out->owned == NULL) {
        return false;
    }
    long bass_position = 28;
    for (size_t k = 0; k < n; k++) {
        if (k > 0 && adjacent(chords, k - 1)) {
            int delta = mod7(chords[k].bass.degree - chords[k - 1].bass.degree);
            bass_position += delta > 3 ? delta - 7 : delta;
        } else {
            bass_position = 28 + chords[k].bass.degree - 1;
        }
        PitchSpelling *bass = spell_at(chords[k].bass, bass_position);
        if (bass != NULL) {
            out->owned[out->owned_count++] = bass;
        }
        out->stages[k].pitches[0] = bass;
        if (chords[k].has_soprano) {
            int gap = mod7(chords[k].soprano.degree - chords[k].bass.degree);
            PitchSpelling *soprano = spell_at(chords[k].soprano, bass_position + (gap == 0 ? 7 : gap));
            if (soprano != NULL) {
                out->owned[out->owned_count++] = soprano;
            }
            out->stages[k].pitches[1] = soprano;
        }
    }
    return true;
}

static void destroy_stages(Stages *stages) {
    for (size_t i = 0; i < stages->owned_count; i++) {
        pitch_spelling_destroy(stages->owned[i]);
    }
    free(stages->owned);
    free(stages->stages);
    memset(stages, 0, sizeof(*stages));
}

/* --- a candidate --- */

typedef struct {
    SequenceKind kind;
    int pattern;
    bool rule_backed;
    int direction;
    size_t unit_length;
    size_t units;
    size_t first; /* index into the chords */
    size_t stage_count;
    int transposition;
    double share;
    double bonus;
    double uncertainty;
    double confidence;
} Candidate;

typedef struct {
    Candidate *items;
    size_t count;
    size_t capacity;
    bool failed;
} Candidates;

static void push(Candidates *candidates, const Candidate *candidate) {
    if (candidates->count == candidates->capacity) {
        size_t next = candidates->capacity == 0 ? 64 : candidates->capacity * 2;
        Candidate *grown = (Candidate *)realloc(candidates->items, next * sizeof(Candidate));
        if (grown == NULL) {
            candidates->failed = true;
            return;
        }
        candidates->items = grown;
        candidates->capacity = next;
    }
    candidates->items[candidates->count++] = *candidate;
}

/* Fills the unit share, the soprano bonus, the transposition and the uncertainty of a candidate whose units are the model repeated. */
static void compare_units(const Chord *chords, Candidate *c) {
    size_t ul = c->unit_length;
    const Chord *first = &chords[c->first];
    size_t matching = 0;
    bool roots_known = true;
    for (size_t j = 0; j < ul; j++) {
        roots_known = roots_known && first[j].has_root;
    }
    for (size_t u = 0; u < c->units; u++) {
        const Chord *unit = &first[u * ul];
        bool same = true;
        for (size_t j = 0; same && j + 1 < ul; j++) {
            same = step_between(unit[j].bass, unit[j + 1].bass) == step_between(first[j].bass, first[j + 1].bass);
            if (same && roots_known) {
                same = unit[j].has_root && unit[j + 1].has_root &&
                       root_motion(unit[j].root, unit[j + 1].root) == root_motion(first[j].root, first[j + 1].root);
            }
        }
        matching += same ? 1 : 0;
    }
    c->share = (double)matching / (double)c->units;
    c->transposition = c->units > 1 ? step_between(first[0].bass, first[ul].bass) : 0;

    /* the soprano's steps repeat with the unit's period */
    bool periodic = c->stage_count > ul + 1;
    for (size_t i = ul; periodic && i + 1 < c->stage_count; i++) {
        periodic = first[i].has_soprano && first[i + 1].has_soprano && first[i - ul].has_soprano && first[i - ul + 1].has_soprano &&
                   step_between(first[i].soprano, first[i + 1].soprano) == step_between(first[i - ul].soprano, first[i - ul + 1].soprano);
    }
    c->bonus = periodic ? 0.1 : 0.0;
    double unc = 0.0;
    for (size_t i = 0; i < c->stage_count; i++) {
        unc += first[i].uncertainty;
    }
    c->uncertainty = unc / (double)c->stage_count;
    double confidence = c->share * (1.0 - c->uncertainty) + c->bonus;
    c->confidence = confidence > 1.0 ? 1.0 : confidence;
}

static void find_by_pattern(const Chord *chords, size_t n, const Stages *stages, const GalantPitchDegree *tonic_unused, unsigned minimum,
                            Candidates *candidates) {
    (void)tonic_unused;
    PitchSpelling *tonic = spell_at((GalantPitchDegree){1, 0}, 28);
    if (tonic == NULL) {
        candidates->failed = true;
        return;
    }
    for (int p = 0; p < SCALE_PATTERN_COUNT; p++) {
        const ScalePattern *pattern = scale_pattern((ScalePatternId)p);
        for (int d = 0; d < SCALE_DIRECTION_COUNT; d++) {
            if (!scale_pattern_allows_direction(pattern, (ScaleDirection)d)) {
                continue;
            }
            for (size_t k = 0; k < n; k++) {
                size_t best = 0;
                for (size_t len = 1; k + len <= n && len <= SCALE_ANALYSIS_MAX_STAGES; len++) {
                    size_t last = k + len - 1;
                    if (!chords[last].has_soprano || (len > 1 && !adjacent(chords, last - 1))) {
                        break;
                    }
                    ScaleHarmonization harmonization = {&stages->stages[k], len, 2, (ScaleDirection)d, tonic};
                    ScaleSequence sequence = {pattern, &harmonization};
                    unsigned hard = 0;
                    unsigned soft = 0;
                    if (!scale_sequence_check(&sequence, &hard, &soft) || hard != 0) {
                        break;
                    }
                    best = len;
                }
                size_t whole = best / pattern->unit_length * pattern->unit_length;
                if (whole / pattern->unit_length < minimum) {
                    continue;
                }
                Candidate c;
                memset(&c, 0, sizeof(c));
                c.kind = pattern->kind;
                c.pattern = p;
                c.rule_backed = true;
                c.direction = d;
                c.unit_length = pattern->unit_length;
                c.units = whole / pattern->unit_length;
                c.first = k;
                c.stage_count = whole;
                compare_units(chords, &c);
                push(candidates, &c);
            }
        }
    }
    pitch_spelling_destroy(tonic);
}

/* Roots that each descend a fifth, in units of two chords with the same bass interval. */
static void find_cycles_of_fifths(const Chord *chords, size_t n, unsigned minimum, Candidates *candidates) {
    for (size_t k = 0; k + 1 < n; k++) {
        if (!chords[k].has_root) {
            continue;
        }
        size_t run = 1;
        while (k + run < n && run < SCALE_ANALYSIS_MAX_STAGES && adjacent(chords, k + run - 1) && chords[k + run].has_root &&
               root_motion(chords[k + run - 1].root, chords[k + run].root) == 3) {
            run++;
        }
        size_t units = run / 2;
        size_t matching = 0;
        int bass_interval = 0;
        for (size_t u = 0; u < units; u++) {
            int interval = step_between(chords[k + 2 * u].bass, chords[k + 2 * u + 1].bass);
            if (u == 0) {
                bass_interval = interval;
            }
            if (interval != bass_interval) {
                break;
            }
            matching++;
        }
        if (matching < minimum) {
            continue;
        }
        Candidate c;
        memset(&c, 0, sizeof(c));
        c.kind = SEQUENCE_KIND_CYCLE_OF_FIFTHS;
        c.pattern = -1;
        c.rule_backed = false;
        c.direction = -1;
        c.unit_length = 2;
        c.units = matching;
        c.first = k;
        c.stage_count = 2 * matching;
        compare_units(chords, &c);
        push(candidates, &c);
    }
}

static int compare_candidates(const void *x, const void *y) {
    const Candidate *a = (const Candidate *)x;
    const Candidate *b = (const Candidate *)y;
    if (a->units != b->units) {
        return a->units > b->units ? -1 : 1;
    }
    if (fabs(a->confidence - b->confidence) > 1e-12) {
        return a->confidence > b->confidence ? -1 : 1;
    }
    if (a->rule_backed != b->rule_backed) {
        return a->rule_backed ? -1 : 1;
    }
    return (a->first > b->first) - (a->first < b->first);
}

static int compare_first(const void *x, const void *y) {
    const Candidate *a = (const Candidate *)x;
    const Candidate *b = (const Candidate *)y;
    return (a->first > b->first) - (a->first < b->first);
}

static bool overlaps(const Candidate *a, const Candidate *b) {
    return a->first < b->first + b->stage_count && b->first < a->first + a->stage_count;
}

static GalantAnalysisStatus emit(const GalantSegmentList *segments, const Chord *chords, const Candidate *c, int index,
                                 ScaleSequenceMatch *out) {
    ScaleSequenceClaim *claim = &out->claim;
    memset(claim, 0, sizeof(*claim));
    claim->kind = (int)c->kind;
    snprintf(claim->kind_name, sizeof(claim->kind_name), "%s", sequence_kind_name(c->kind));
    claim->pattern = c->pattern;
    claim->rule_backed = c->rule_backed ? 1 : 0;
    claim->direction = c->direction;
    claim->unit_length = (int)c->unit_length;
    claim->units = (int)c->units;
    claim->stage_count = (int)c->stage_count;
    claim->transposition_steps = c->transposition;
    claim->first_segment = (int)chords[c->first].original;
    claim->last_segment = (int)chords[c->first + c->stage_count - 1].original;
    claim->unit_share = c->share;
    claim->soprano_bonus = c->bonus;
    claim->mean_uncertainty = c->uncertainty;
    claim->confidence = c->confidence;
    for (size_t i = 0; i < c->stage_count; i++) {
        const Chord *chord = &chords[c->first + i];
        ScaleStageClaim *stage = &claim->stages[i];
        snprintf(stage->segment_id, sizeof(stage->segment_id), "%s", segments->items[chord->original].id);
        stage->bass_degree = chord->bass.degree;
        stage->bass_alteration = chord->bass.alteration;
        stage->has_soprano = chord->has_soprano ? 1 : 0;
        stage->soprano_degree = chord->has_soprano ? chord->soprano.degree : 0;
        stage->soprano_alteration = chord->has_soprano ? chord->soprano.alteration : 0;
        stage->has_root = chord->has_root ? 1 : 0;
        stage->root_degree = chord->has_root ? chord->root.degree : 0;
        stage->root_alteration = chord->has_root ? chord->root.alteration : 0;
    }
    out->uncertainty = 1.0 - c->confidence;
    char key[96];
    char interpretation_id[160];
    snprintf(key, sizeof(key), "%s-%d", claim->kind_name, index);
    out->relationship = scale_analysis_relationship("sequence", key, segments->items[claim->first_segment].id,
                                                    segments->items[claim->last_segment].id, "harmonic-sequence");
    if (out->relationship == NULL) {
        return GALANT_ANALYSIS_ERR_ALLOC;
    }
    snprintf(interpretation_id, sizeof(interpretation_id), "scale-harmonization-sequence-claim-%s", key);
    out->interpretation = scale_analysis_interpretation(interpretation_id, entity_id_value(relationship_id(out->relationship)),
                                                        SCALE_SEQUENCE_CLAIM_TYPE, claim, sizeof(*claim), out->uncertainty,
                                                        "sequence-detection");
    return out->interpretation == NULL ? GALANT_ANALYSIS_ERR_ALLOC : GALANT_ANALYSIS_OK;
}

GalantAnalysisStatus scale_sequence_detect(const GalantSegmentList *segments, const ScaleDetectConfig *config, ScaleSequenceMatches *out) {
    if (segments == NULL || out == NULL) {
        return GALANT_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    ScaleDetectConfig fallback = scale_detect_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    if (config->minimum_units < 2) {
        return GALANT_ANALYSIS_ERR_INVALID;
    }
    ScaleSequenceMatches found = {NULL, 0};
    Chord *chords = (Chord *)calloc(segments->count == 0 ? 1 : segments->count, sizeof(Chord));
    if (chords == NULL) {
        return GALANT_ANALYSIS_ERR_ALLOC;
    }
    size_t n = 0;
    for (size_t i = 0; i < segments->count; i++) {
        const GalantSegment *segment = &segments->items[i];
        if (!segment->has_bass) {
            continue;
        }
        Chord *chord = &chords[n++];
        chord->original = i;
        chord->bass = galant_degree_in_home(segment->bass, segment->key);
        chord->has_soprano = segment->has_soprano;
        chord->soprano = segment->has_soprano ? galant_degree_in_home(segment->soprano, segment->key) : (GalantPitchDegree){1, 0};
        chord->has_root = segment->has_numeral;
        chord->root = segment->has_numeral ? galant_degree_in_home(segment->root, segment->key) : (GalantPitchDegree){1, 0};
        chord->uncertainty = segment->uncertainty;
    }
    if (n == 0) {
        free(chords);
        *out = found;
        return GALANT_ANALYSIS_OK;
    }
    Stages stages;
    Candidates candidates;
    memset(&candidates, 0, sizeof(candidates));
    if (!build_stages(chords, n, &stages)) {
        destroy_stages(&stages);
        free(chords);
        return GALANT_ANALYSIS_ERR_ALLOC;
    }
    find_by_pattern(chords, n, &stages, NULL, config->minimum_units, &candidates);
    find_cycles_of_fifths(chords, n, config->minimum_units, &candidates);
    destroy_stages(&stages);
    if (candidates.failed) {
        free(candidates.items);
        free(chords);
        return GALANT_ANALYSIS_ERR_ALLOC;
    }

    qsort(candidates.items, candidates.count, sizeof(Candidate), compare_candidates);
    size_t kept = 0;
    for (size_t i = 0; i < candidates.count; i++) {
        bool keep = true;
        for (size_t k = 0; keep && k < kept; k++) {
            keep = !overlaps(&candidates.items[i], &candidates.items[k]);
        }
        if (keep) {
            candidates.items[kept++] = candidates.items[i];
        }
    }
    qsort(candidates.items, kept, sizeof(Candidate), compare_first);

    GalantAnalysisStatus status = GALANT_ANALYSIS_OK;
    if (kept > 0) {
        found.items = (ScaleSequenceMatch *)calloc(kept, sizeof(ScaleSequenceMatch));
        if (found.items == NULL) {
            status = GALANT_ANALYSIS_ERR_ALLOC;
        }
    }
    for (size_t i = 0; status == GALANT_ANALYSIS_OK && i < kept; i++) {
        status = emit(segments, chords, &candidates.items[i], (int)i, &found.items[i]);
        found.count = i + 1;
    }
    free(candidates.items);
    free(chords);
    if (status != GALANT_ANALYSIS_OK) {
        scale_sequence_matches_destroy(&found);
        return status;
    }
    *out = found;
    return GALANT_ANALYSIS_OK;
}

void scale_sequence_matches_destroy(ScaleSequenceMatches *matches) {
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

bool scale_sequence_claim_read(const Interpretation *interpretation, ScaleSequenceClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), SCALE_SEQUENCE_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
