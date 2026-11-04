#include "analysis_motive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_motive_match.h"
#include "analysis_motive_shape.h"
#include "analysis_time.h"
#include "phrase_ending.h"
#include "core/occurrence.h"
#include "core/value.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"

const char *standard_form_motive_kind_name(StandardFormMotiveKind kind) {
    return motive_occurrence_kind_name(kind);
}

int standard_form_motive_kind_rank(StandardFormMotiveKind kind) {
    return motive_occurrence_kind_rank(kind);
}

/* --- the internal families --- */

typedef struct {
    size_t first;
    size_t n;
    int kind;
    double score;
    int mismatches;
    size_t intervals;
    double rhythm_factor;
    int fragment_part;
    size_t fragment_intervals;
    bool in_continuation;
} Occ;

typedef struct {
    int line;
    size_t proto_first;
    size_t proto_n;
    bool generic;
    StandardFormMotiveShape proto;
    Occ *occ;
    size_t occ_count;
    size_t occ_capacity;
} Fam;

static bool push_occ(Fam *fam, const Occ *occ) {
    if (fam->occ_count == fam->occ_capacity) {
        size_t capacity = fam->occ_capacity == 0 ? 8 : fam->occ_capacity * 2;
        Occ *grown = (Occ *)realloc(fam->occ, capacity * sizeof(Occ));
        if (grown == NULL) {
            return false;
        }
        fam->occ = grown;
        fam->occ_capacity = capacity;
    }
    fam->occ[fam->occ_count++] = *occ;
    return true;
}

typedef struct {
    size_t first;
    size_t n;
    bool generic;
    char *key;
} Cand;

static char *make_key(const StandardFormMotiveShape *shape) {
    size_t size = 16 + 64 * shape->intervals;
    char *key = (char *)malloc(size);
    if (key == NULL) {
        return NULL;
    }
    size_t at = 0;
    key[at++] = shape->generic ? 'G' : 'S';
    key[at++] = '|';
    for (size_t k = 0; k < shape->intervals; k++) {
        at += (size_t)snprintf(key + at, size - at, "%d,%lld/%lld;", shape->steps[k], shape->ioi[k].n, shape->ioi[k].d);
    }
    key[at] = '\0';
    return key;
}

static int compare_cands(const void *a, const void *b) {
    const Cand *left = (const Cand *)a;
    const Cand *right = (const Cand *)b;
    int order = strcmp(left->key, right->key);
    if (order != 0) {
        return order;
    }
    return (left->first > right->first) - (left->first < right->first);
}

/* The kind of an occurrence of the prototype's pattern: the same semitone intervals from the same first pitch is EXACT, from
   another pitch REAL; with other semitones (a generic pattern only) TONAL. */
static StandardFormMotiveKind pattern_kind(const StandardFormMotiveShape *proto, const StandardFormMotiveShape *other) {
    if (memcmp(proto->semitones, other->semitones, proto->intervals * sizeof(int)) != 0) {
        return STANDARD_FORM_MOTIVE_TONAL;
    }
    return proto->first_semitone == other->first_semitone ? STANDARD_FORM_MOTIVE_EXACT : STANDARD_FORM_MOTIVE_REAL;
}

typedef struct {
    size_t first_cand; /* into the sorted candidates */
    size_t cand_count;
    size_t n;
    size_t *firsts; /* the taken occurrences, in order */
    size_t occ_count;
    bool kept;
} Group;

/* True when [first, first + n) lies inside an occurrence of a kept group longer than n, on the line. */
static bool covered(const Group *groups, size_t group_count, size_t self, size_t first, size_t n) {
    for (size_t g = 0; g < group_count; g++) {
        if (g == self || !groups[g].kept || groups[g].n <= n) {
            continue;
        }
        for (size_t k = 0; k < groups[g].occ_count; k++) {
            if (groups[g].firsts[k] <= first && first + n <= groups[g].firsts[k] + groups[g].n) {
                return true;
            }
        }
    }
    return false;
}

static int compare_group_length_desc(const void *a, const void *b) {
    const Group *left = (const Group *)a;
    const Group *right = (const Group *)b;
    if (left->n != right->n) {
        return left->n < right->n ? 1 : -1;
    }
    return (left->firsts[0] > right->firsts[0]) - (left->firsts[0] < right->firsts[0]);
}

/* Discovers the families of one line: the prototype's pattern and its exact, real and tonal occurrences. */
static StandardFormAnalysisStatus discover(const StandardFormMotiveLine *line, const StandardFormConfig *config, Fam **families,
                                           size_t *family_count, size_t *family_capacity) {
    size_t cap = line->count * (config->motive_max_notes - config->motive_min_notes + 1);
    if (cap == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    Cand *cands = (Cand *)calloc(cap, sizeof(Cand));
    if (cands == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    size_t cand_count = 0;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    for (size_t first = 0; first < line->count && status == STANDARD_FORM_ANALYSIS_OK; first++) {
        if (!line->can_start[first]) {
            continue;
        }
        for (size_t n = config->motive_min_notes; n <= config->motive_max_notes; n++) {
            StandardFormMotiveShape shape;
            if (!standard_form_motive_window_ok(line, first, n)) {
                break; /* a longer window contains the break too */
            }
            bool generic = standard_form_motive_shape(line, first, n, true, &shape);
            if (!generic && !standard_form_motive_shape(line, first, n, false, &shape)) {
                continue;
            }
            Cand *cand = &cands[cand_count];
            cand->first = first;
            cand->n = n;
            cand->generic = generic;
            cand->key = make_key(&shape);
            if (cand->key == NULL) {
                status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                break;
            }
            cand_count++;
        }
    }
    Group *groups = NULL;
    size_t group_count = 0;
    if (status == STANDARD_FORM_ANALYSIS_OK && cand_count > 0) {
        qsort(cands, cand_count, sizeof(Cand), compare_cands);
        groups = (Group *)calloc(cand_count, sizeof(Group));
        if (groups == NULL) {
            status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
    }
    for (size_t i = 0; status == STANDARD_FORM_ANALYSIS_OK && i < cand_count;) {
        size_t j = i;
        while (j < cand_count && strcmp(cands[j].key, cands[i].key) == 0) {
            j++;
        }
        if (j - i >= 2) {
            size_t *firsts = (size_t *)calloc(j - i, sizeof(size_t));
            if (firsts == NULL) {
                status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                break;
            }
            size_t taken = 0;
            size_t next_free = 0;
            for (size_t k = i; k < j; k++) {
                if (cands[k].first >= next_free) {
                    firsts[taken++] = cands[k].first;
                    next_free = cands[k].first + cands[k].n;
                }
            }
            if (taken >= 2) {
                groups[group_count].first_cand = i;
                groups[group_count].cand_count = j - i;
                groups[group_count].n = cands[i].n;
                groups[group_count].firsts = firsts;
                groups[group_count].occ_count = taken;
                group_count++;
            } else {
                free(firsts);
            }
        }
        i = j;
    }
    if (status == STANDARD_FORM_ANALYSIS_OK && group_count > 0) {
        /* maximality: the longer groups first */
        qsort(groups, group_count, sizeof(Group), compare_group_length_desc);
        for (size_t g = 0; g < group_count; g++) {
            bool all_covered = true;
            for (size_t k = 0; k < groups[g].occ_count && all_covered; k++) {
                all_covered = covered(groups, group_count, g, groups[g].firsts[k], groups[g].n);
            }
            groups[g].kept = !all_covered;
        }
        for (size_t g = 0; g < group_count && status == STANDARD_FORM_ANALYSIS_OK; g++) {
            if (!groups[g].kept) {
                continue;
            }
            if (*family_count == *family_capacity) {
                size_t capacity = *family_capacity == 0 ? 16 : *family_capacity * 2;
                Fam *grown = (Fam *)realloc(*families, capacity * sizeof(Fam));
                if (grown == NULL) {
                    status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                    break;
                }
                *families = grown;
                *family_capacity = capacity;
            }
            Fam *fam = &(*families)[*family_count];
            memset(fam, 0, sizeof(*fam));
            fam->line = line->line;
            fam->proto_first = groups[g].firsts[0];
            fam->proto_n = groups[g].n;
            fam->generic = cands[groups[g].first_cand].generic;
            if (!standard_form_motive_shape(line, fam->proto_first, fam->proto_n, fam->generic, &fam->proto)) {
                status = STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
                break;
            }
            (*family_count)++;
            for (size_t k = 0; k < groups[g].occ_count; k++) {
                StandardFormMotiveShape shape;
                Occ occ;
                memset(&occ, 0, sizeof(occ));
                if (!standard_form_motive_shape(line, groups[g].firsts[k], fam->proto_n, fam->generic, &shape)) {
                    status = STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
                    break;
                }
                occ.first = groups[g].firsts[k];
                occ.n = fam->proto_n;
                occ.kind = k == 0 ? (int)STANDARD_FORM_MOTIVE_EXACT : (int)pattern_kind(&fam->proto, &shape);
                occ.score = 1.0;
                occ.intervals = shape.intervals;
                occ.rhythm_factor = 1.0;
                occ.fragment_part = -1;
                if (!push_occ(fam, &occ)) {
                    status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                    break;
                }
            }
        }
    }
    for (size_t g = 0; g < group_count; g++) {
        free(groups[g].firsts);
    }
    free(groups);
    for (size_t i = 0; i < cand_count; i++) {
        free(cands[i].key);
    }
    free(cands);
    return status;
}

/* --- the transformed occurrences --- */

typedef struct {
    size_t fam;
    int line;
    Occ occ;
} Rec;

static int compare_recs_by_window(const void *a, const void *b) {
    const Rec *left = (const Rec *)a;
    const Rec *right = (const Rec *)b;
    if (left->line != right->line) {
        return left->line - right->line;
    }
    if (left->occ.first != right->occ.first) {
        return left->occ.first < right->occ.first ? -1 : 1;
    }
    if (left->occ.n != right->occ.n) {
        return left->occ.n < right->occ.n ? -1 : 1;
    }
    int rank_left = standard_form_motive_kind_rank((StandardFormMotiveKind)left->occ.kind);
    int rank_right = standard_form_motive_kind_rank((StandardFormMotiveKind)right->occ.kind);
    if (rank_left != rank_right) {
        return rank_left - rank_right;
    }
    if (left->occ.score != right->occ.score) {
        return left->occ.score > right->occ.score ? -1 : 1;
    }
    return (left->fam > right->fam) - (left->fam < right->fam);
}

/* The order a family takes its occurrences in: the best kind first, then the longer, the better score, the earlier. */
static int compare_recs_by_quality(const void *a, const void *b) {
    const Rec *left = (const Rec *)a;
    const Rec *right = (const Rec *)b;
    if (left->fam != right->fam) {
        return left->fam < right->fam ? -1 : 1;
    }
    int rank_left = standard_form_motive_kind_rank((StandardFormMotiveKind)left->occ.kind);
    int rank_right = standard_form_motive_kind_rank((StandardFormMotiveKind)right->occ.kind);
    if (rank_left != rank_right) {
        return rank_left - rank_right;
    }
    if (left->occ.in_continuation != right->occ.in_continuation) {
        return left->occ.in_continuation ? -1 : 1; /* a fragment in a continuation is searched first */
    }
    if (left->occ.n != right->occ.n) {
        return left->occ.n > right->occ.n ? -1 : 1;
    }
    if (left->occ.score != right->occ.score) {
        return left->occ.score > right->occ.score ? -1 : 1;
    }
    return (left->occ.first > right->occ.first) - (left->occ.first < right->occ.first);
}

static int compare_occ_by_first(const void *a, const void *b) {
    const Occ *left = (const Occ *)a;
    const Occ *right = (const Occ *)b;
    return (left->first > right->first) - (left->first < right->first);
}

static bool overlaps(const Fam *fam, size_t first, size_t n) {
    for (size_t k = 0; k < fam->occ_count; k++) {
        if (first < fam->occ[k].first + fam->occ[k].n && fam->occ[k].first < first + n) {
            return true;
        }
    }
    return false;
}

/* True when another family whose pattern is as long as this one's or longer already holds the window as one of its occurrences. */
static bool held_by_equal_or_longer(const Fam *families, size_t family_count, size_t self, size_t first, size_t n) {
    for (size_t f = 0; f < family_count; f++) {
        if (f == self || families[f].line != families[self].line || families[f].proto_n < families[self].proto_n) {
            continue;
        }
        for (size_t k = 0; k < families[f].occ_count; k++) {
            if (families[f].occ[k].first == first && families[f].occ[k].n == n) {
                return true;
            }
        }
    }
    return false;
}

typedef struct {
    int first;
    int last;
} MeasureSpan;

/* The measures of the continuations of the themes: the second part of a theme whose ending is a continuation. */
static MeasureSpan *continuation_spans(const StandardFormThemes *themes, size_t *count) {
    *count = 0;
    if (themes == NULL || themes->count == 0 || themes->items == NULL) {
        return NULL;
    }
    MeasureSpan *spans = (MeasureSpan *)calloc(themes->count, sizeof(MeasureSpan));
    if (spans == NULL) {
        return NULL;
    }
    for (size_t t = 0; t < themes->count; t++) {
        const StandardFormThemeClaim *claim = &themes->items[t].claim;
        if (claim->ending == (int)PHRASE_ENDING_CONTINUATION && claim->second_part_last_measure >= claim->second_part_first_measure) {
            spans[*count].first = claim->second_part_first_measure;
            spans[*count].last = claim->second_part_last_measure;
            (*count)++;
        }
    }
    return spans;
}

static bool window_in_spans(const StandardFormMotiveLine *line, size_t first, size_t n, const MeasureSpan *spans, size_t span_count) {
    for (size_t k = 0; k < span_count; k++) {
        if (line->first_measure[first] >= spans[k].first && line->first_measure[first + n - 1] <= spans[k].last) {
            return true;
        }
    }
    return false;
}

static bool push_rec(Rec **recs, size_t *count, size_t *capacity, const Rec *rec) {
    if (*count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 64 : *capacity * 2;
        Rec *grown = (Rec *)realloc(*recs, grown_capacity * sizeof(Rec));
        if (grown == NULL) {
            return false;
        }
        *recs = grown;
        *capacity = grown_capacity;
    }
    (*recs)[(*count)++] = *rec;
    return true;
}

static void fill_rec(Rec *rec, size_t fam, int line, size_t first, size_t n, const StandardFormMotiveMatch *match) {
    memset(rec, 0, sizeof(*rec));
    rec->fam = fam;
    rec->line = line;
    rec->occ.first = first;
    rec->occ.n = n;
    rec->occ.kind = match->kind;
    rec->occ.score = match->score;
    rec->occ.mismatches = match->mismatches;
    rec->occ.intervals = match->intervals;
    rec->occ.rhythm_factor = match->rhythm_factor;
    rec->occ.fragment_part = match->fragment_part;
    rec->occ.fragment_intervals = match->fragment_intervals;
}

static size_t exact_count(const Fam *fam) {
    size_t count = 0;
    for (size_t k = 0; k < fam->occ_count; k++) {
        count += fam->occ[k].kind <= (int)STANDARD_FORM_MOTIVE_TONAL ? 1 : 0;
    }
    return count;
}

static int compare_fam_length_desc(const void *a, const void *b) {
    const Fam *const *left = (const Fam *const *)a;
    const Fam *const *right = (const Fam *const *)b;
    if ((*left)->proto_n != (*right)->proto_n) {
        return (*left)->proto_n < (*right)->proto_n ? 1 : -1;
    }
    return ((*left)->proto_first > (*right)->proto_first) - ((*left)->proto_first < (*right)->proto_first);
}

/* Adds to each family the windows that are its prototype under a transformation or a fragment of it, and then drops the families the
   longer ones explain. A window that is an occurrence of another family with a pattern as long or longer stays there; each other
   window goes to the one family it matches best (and leaves a shorter family it was an occurrence of); within a family, occurrences do
   not overlap, the better kind first and, among fragments, those inside a continuation first. A family is then dropped when it has
   fewer than two exact, real or tonal occurrences left, or when every one of its occurrences lies inside an occurrence of a kept
   family with a longer pattern. */
static StandardFormAnalysisStatus expand(const StandardFormMotiveLine *lines, Fam *families, size_t *family_count_io,
                                         const StandardFormConfig *config, const StandardFormThemes *themes) {
    size_t family_count = *family_count_io;
    size_t span_count = 0;
    MeasureSpan *spans = continuation_spans(themes, &span_count);
    size_t capacity = 0;
    Rec *recs = NULL;
    size_t rec_count = 0;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    for (size_t f = 0; f < family_count && status == STANDARD_FORM_ANALYSIS_OK; f++) {
        const Fam *fam = &families[f];
        const StandardFormMotiveLine *line = &lines[fam->line];
        size_t whole = fam->proto.intervals;
        size_t shortest = standard_form_motive_fragment_min_intervals(whole);
        for (size_t first = 0; first < line->count && status == STANDARD_FORM_ANALYSIS_OK; first++) {
            if (!line->can_start[first]) {
                continue;
            }
            StandardFormMotiveShape shape;
            StandardFormMotiveMatch match;
            Rec rec;
            /* the whole motive under a transformation */
            if (standard_form_motive_shape(line, first, fam->proto_n, fam->generic, &shape) &&
                !held_by_equal_or_longer(families, family_count, f, first, fam->proto_n) &&
                (config->relation_match != NULL
                     ? standard_form_motive_classify_match(&fam->proto, &shape, config->varied_max_mismatch, config->relation_match,
                                                           config->relation_context, &match)
                     : standard_form_motive_classify_with(&fam->proto, &shape, config->varied_max_mismatch, config->relation_matcher,
                                                          config->relation_context, &match)) &&
                match.kind > (int)STANDARD_FORM_MOTIVE_TONAL) {
                fill_rec(&rec, f, fam->line, first, fam->proto_n, &match);
                if (!push_rec(&recs, &rec_count, &capacity, &rec)) {
                    status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                }
            }
            /* a prefix or a suffix of it */
            for (size_t length = shortest; status == STANDARD_FORM_ANALYSIS_OK && length < whole; length++) {
                size_t n = length + 1;
                if (!standard_form_motive_shape(line, first, n, fam->generic, &shape) ||
                    held_by_equal_or_longer(families, family_count, f, first, n) ||
                    config->relation_match == NULL ||
                    !standard_form_motive_fragment_match(&fam->proto, &shape, config->relation_match, config->relation_context,
                                                         &match)) {
                    continue;
                }
                fill_rec(&rec, f, fam->line, first, n, &match);
                rec.occ.in_continuation = window_in_spans(line, first, n, spans, span_count);
                if (!push_rec(&recs, &rec_count, &capacity, &rec)) {
                    status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                }
            }
        }
    }
    free(spans);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        free(recs);
        return status;
    }
    if (rec_count > 0) {
        /* a window goes to one family: the best claim on it */
        qsort(recs, rec_count, sizeof(Rec), compare_recs_by_window);
        size_t kept = 0;
        for (size_t i = 0; i < rec_count; i++) {
            if (kept > 0 && recs[kept - 1].line == recs[i].line && recs[kept - 1].occ.first == recs[i].occ.first &&
                recs[kept - 1].occ.n == recs[i].occ.n) {
                continue;
            }
            recs[kept++] = recs[i];
        }
        rec_count = kept;
        qsort(recs, rec_count, sizeof(Rec), compare_recs_by_quality);
        for (size_t i = 0; i < rec_count; i++) {
            Fam *fam = &families[recs[i].fam];
            if (overlaps(fam, recs[i].occ.first, recs[i].occ.n)) {
                continue;
            }
            if (!push_occ(fam, &recs[i].occ)) {
                free(recs);
                return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
            }
            /* the window leaves a shorter family it was an occurrence of (never that family's prototype) */
            for (size_t g = 0; g < family_count; g++) {
                Fam *shorter = &families[g];
                if (g == recs[i].fam || shorter->line != fam->line || shorter->proto_n >= fam->proto_n) {
                    continue;
                }
                for (size_t k = 1; k < shorter->occ_count; k++) {
                    if (shorter->occ[k].first == recs[i].occ.first && shorter->occ[k].n == recs[i].occ.n &&
                        shorter->occ[k].kind <= (int)STANDARD_FORM_MOTIVE_TONAL) {
                        memmove(&shorter->occ[k], &shorter->occ[k + 1], (shorter->occ_count - k - 1) * sizeof(Occ));
                        shorter->occ_count--;
                        break;
                    }
                }
            }
        }
        for (size_t f = 0; f < family_count; f++) {
            if (families[f].occ_count > 2) {
                qsort(families[f].occ + 1, families[f].occ_count - 1, sizeof(Occ), compare_occ_by_first);
            }
        }
    }
    free(recs);

    /* the families the longer ones explain */
    const Fam **order = (const Fam **)calloc(family_count > 0 ? family_count : 1, sizeof(Fam *));
    bool *dropped = (bool *)calloc(family_count > 0 ? family_count : 1, sizeof(bool));
    if (order == NULL || dropped == NULL) {
        free(order);
        free(dropped);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t f = 0; f < family_count; f++) {
        order[f] = &families[f];
    }
    qsort(order, family_count, sizeof(Fam *), compare_fam_length_desc);
    for (size_t i = 0; i < family_count; i++) {
        const Fam *fam = order[i];
        size_t self = (size_t)(fam - families);
        bool all_covered = fam->occ_count > 0;
        for (size_t k = 0; all_covered && k < fam->occ_count; k++) {
            bool covered_here = false;
            for (size_t h = 0; !covered_here && h < family_count; h++) {
                if (h == self || dropped[h] || families[h].line != fam->line || families[h].proto_n <= fam->proto_n) {
                    continue;
                }
                for (size_t m = 0; !covered_here && m < families[h].occ_count; m++) {
                    covered_here = families[h].occ[m].first <= fam->occ[k].first &&
                                   fam->occ[k].first + fam->occ[k].n <= families[h].occ[m].first + families[h].occ[m].n;
                }
            }
            all_covered = covered_here;
        }
        dropped[self] = exact_count(fam) < 2 || all_covered;
    }
    size_t kept_families = 0;
    for (size_t f = 0; f < family_count; f++) {
        if (dropped[f]) {
            free(families[f].occ);
            continue;
        }
        if (kept_families != f) {
            families[kept_families] = families[f];
        }
        kept_families++;
    }
    *family_count_io = kept_families;
    free(order);
    free(dropped);
    return STANDARD_FORM_ANALYSIS_OK;
}

/* --- the kernel's window matching, injected --- */

typedef struct {
    Contour *contour;
    RhythmicPattern *rhythm;
    const EntityID **ids;
} KernelWindow;

static void destroy_kernel_windows(KernelWindow *windows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        contour_destroy(windows[i].contour);
        rhythmic_pattern_destroy(windows[i].rhythm);
        free(windows[i].ids);
    }
    free(windows);
}

static bool make_kernel_window(const StandardFormMotiveLine *line, size_t first, const StandardFormMotiveShape *shape,
                               KernelWindow *out) {
    size_t n = shape->intervals + 1;
    memset(out, 0, sizeof(*out));
    PitchIdentity **pitches = (PitchIdentity **)calloc(n, sizeof(PitchIdentity *));
    ContourDirection *directions = (ContourDirection *)calloc(shape->intervals, sizeof(ContourDirection));
    Duration **durations = (Duration **)calloc(shape->intervals, sizeof(Duration *));
    out->ids = (const EntityID **)calloc(n, sizeof(EntityID *));
    bool ok = pitches != NULL && directions != NULL && durations != NULL && out->ids != NULL;
    for (size_t k = 0; ok && k < n; k++) {
        const char *id = line->notes[first + k].note_id;
        ok = pitch_identity_create(id, strlen(id), &pitches[k]) == PITCH_IDENTITY_OK;
        out->ids[k] = note_event_id(line->notes[first + k].event);
    }
    for (size_t k = 0; ok && k < shape->intervals; k++) {
        directions[k] = shape->steps[k] > 0 ? CONTOUR_DIRECTION_UP
                                            : (shape->steps[k] < 0 ? CONTOUR_DIRECTION_DOWN : CONTOUR_DIRECTION_SAME);
        ok = duration_create((long)shape->ioi[k].n, (unsigned long)shape->ioi[k].d, &durations[k]) == DURATION_OK;
    }
    /* On success the contour and the pattern take the elements and copy the arrays; the arrays stay ours. */
    bool pitches_taken = false;
    bool durations_taken = false;
    if (ok && contour_create(pitches, n, directions, shape->intervals, &out->contour) == CONTOUR_OK) {
        pitches_taken = true;
    } else {
        ok = false;
    }
    if (ok && rhythmic_pattern_create(durations, shape->intervals, &out->rhythm) == RHYTHMIC_PATTERN_OK) {
        durations_taken = true;
    } else {
        ok = false;
    }
    if (pitches != NULL && !pitches_taken) {
        for (size_t k = 0; k < n; k++) {
            pitch_identity_destroy(pitches[k]);
        }
    }
    free(pitches);
    free(directions);
    if (durations != NULL && !durations_taken) {
        for (size_t k = 0; k < shape->intervals; k++) {
            duration_destroy(durations[k]);
        }
    }
    free(durations);
    if (!ok) {
        contour_destroy(out->contour);
        rhythmic_pattern_destroy(out->rhythm);
        free(out->ids);
        memset(out, 0, sizeof(*out));
    }
    return ok;
}

/* Every window of an exact, real or tonal occurrence goes to the kernel's matcher, and each must match another. */
static StandardFormAnalysisStatus check_with_kernel(const StandardFormMotiveLine *lines, const Fam *families, size_t family_count,
                                                    StandardFormWindowMatcher matcher, void *context) {
    size_t total = 0;
    for (size_t f = 0; f < family_count; f++) {
        for (size_t k = 0; k < families[f].occ_count; k++) {
            total += families[f].occ[k].kind <= (int)STANDARD_FORM_MOTIVE_TONAL ? 1 : 0;
        }
    }
    if (total == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    KernelWindow *kernel = (KernelWindow *)calloc(total, sizeof(KernelWindow));
    StandardFormMotifWindow *windows = (StandardFormMotifWindow *)calloc(total, sizeof(StandardFormMotifWindow));
    bool *matched = (bool *)calloc(total, sizeof(bool));
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    if (kernel == NULL || windows == NULL || matched == NULL) {
        status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    size_t made = 0;
    for (size_t f = 0; status == STANDARD_FORM_ANALYSIS_OK && f < family_count; f++) {
        const StandardFormMotiveLine *line = &lines[families[f].line];
        for (size_t k = 0; k < families[f].occ_count; k++) {
            const Occ *occ = &families[f].occ[k];
            StandardFormMotiveShape shape;
            if (occ->kind > (int)STANDARD_FORM_MOTIVE_TONAL) {
                continue;
            }
            if (!standard_form_motive_shape(line, occ->first, occ->n, families[f].generic, &shape) ||
                !make_kernel_window(line, occ->first, &shape, &kernel[made])) {
                status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
                break;
            }
            windows[made].contour = kernel[made].contour;
            windows[made].rhythm = kernel[made].rhythm;
            windows[made].event_ids = kernel[made].ids;
            windows[made].event_id_count = occ->n;
            made++;
        }
    }
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = matcher(windows, made, matched, context);
    }
    for (size_t i = 0; status == STANDARD_FORM_ANALYSIS_OK && i < made; i++) {
        if (!matched[i]) {
            status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR; /* equal intervals and rhythm imply the kernel's match */
        }
    }
    destroy_kernel_windows(kernel, made);
    free(windows);
    free(matched);
    return status;
}

/* --- the output --- */

void standard_form_motives_destroy(StandardFormMotives *motives) {
    if (motives == NULL) {
        return;
    }
    for (size_t f = 0; f < motives->family_count; f++) {
        StandardFormMotiveFamily *family = &motives->families[f];
        for (size_t k = 0; k < family->occurrence_count; k++) {
            StandardFormMotiveOccurrence *occurrence = &family->occurrences[k];
            relationship_destroy(occurrence->contained_relationship);
            relationship_destroy(occurrence->family_relationship);
            interpretation_destroy(occurrence->interpretation);
            motif_occurrence_destroy(occurrence->occurrence);
        }
        free(family->occurrences);
        interpretation_destroy(family->interpretation);
        motif_destroy(family->motif);
    }
    free(motives->families);
    motives->families = NULL;
    motives->family_count = 0;
}

static bool read_claim(const Interpretation *interpretation, const char *type, void *out, size_t size) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t content_size = 0;
    if (claim == NULL || strcmp(value_type(claim), type) != 0) {
        return false;
    }
    const void *content = value_content(claim, &content_size);
    if (content == NULL || content_size != size) {
        return false;
    }
    memcpy(out, content, size);
    return true;
}

bool standard_form_motive_claim_read(const Interpretation *interpretation, StandardFormMotiveClaim *out) {
    return read_claim(interpretation, STANDARD_FORM_MOTIVE_CLAIM_TYPE, out, sizeof(*out));
}

bool standard_form_motive_occurrence_claim_read(const Interpretation *interpretation,
                                                StandardFormMotiveOccurrenceClaim *out) {
    return read_claim(interpretation, STANDARD_FORM_MOTIVE_OCCURRENCE_CLAIM_TYPE, out, sizeof(*out));
}

/* The entity that holds a span of measures: the innermost idea shell, else the phrase occurrence; NULL with neither. */
static const char *container_of(const StandardFormPhrases *phrases, const StandardFormSegmentation *segmentation, int first_measure,
                                int last_measure) {
    if (segmentation != NULL) {
        for (size_t i = 0; i < segmentation->idea_count; i++) {
            const StandardFormIdea *idea = &segmentation->ideas[i];
            if (idea->shell != NULL && idea->claim.first_measure <= first_measure && last_measure <= idea->claim.last_measure) {
                return entity_id_value(entity_id(structure_entity(idea->shell)));
            }
        }
    }
    if (phrases != NULL) {
        for (size_t i = 0; i < phrases->count; i++) {
            const StandardFormPhraseClaim *claim = &phrases->items[i].claim;
            if (phrases->items[i].occurrence != NULL && claim->first_measure <= first_measure && last_measure <= claim->last_measure) {
                return entity_id_value(phrase_occurrence_id(phrases->items[i].occurrence));
            }
        }
    }
    return NULL;
}

static StandardFormAnalysisStatus emit_family(const StandardFormInput *input, const StandardFormMotiveLine *lines,
                                              const StandardFormPhrases *phrases, const StandardFormSegmentation *segmentation,
                                              const Fam *fam, int index, StandardFormMotiveFamily *out) {
    const StandardFormMotiveLine *line = &lines[fam->line];
    StandardFormMotiveClaim *claim = &out->claim;
    claim->family = index;
    claim->line = fam->line;
    claim->note_count = (int)fam->proto_n;
    claim->occurrence_count = (int)fam->occ_count;
    claim->generic = fam->generic ? 1 : 0;

    char text[160];
    EntityID **ids = (EntityID **)calloc(fam->proto_n, sizeof(EntityID *));
    if (ids == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    bool ok = true;
    for (size_t k = 0; ok && k < fam->proto_n; k++) {
        ok = entity_id_create(line->notes[fam->proto_first + k].note_id, &ids[k]) == ENTITY_ID_OK;
    }
    EntityID *motif_id_value = NULL;
    snprintf(text, sizeof(text), "standard-form-motive-%d-%d", fam->line, index);
    if (ok) {
        ok = entity_id_create(text, &motif_id_value) == ENTITY_ID_OK;
    }
    if (!ok || motif_create(motif_id_value, ids, fam->proto_n, &out->motif) != MOTIF_OK) {
        entity_id_destroy(motif_id_value);
        for (size_t k = 0; k < fam->proto_n; k++) {
            entity_id_destroy(ids[k]);
        }
        free(ids);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    free(ids);
    char motif_text[160];
    snprintf(motif_text, sizeof(motif_text), "%s", entity_id_value(motif_id(out->motif)));
    char interpretation_id[200];
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-form-motive-claim-%d-%d", fam->line, index);
    out->interpretation = standard_form_analysis_interpretation(interpretation_id, motif_text, STANDARD_FORM_MOTIVE_CLAIM_TYPE,
                                                                claim, sizeof(*claim), 0.0, "motive-discovery");
    if (out->interpretation == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    out->occurrences = (StandardFormMotiveOccurrence *)calloc(fam->occ_count, sizeof(StandardFormMotiveOccurrence));
    if (out->occurrences == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t j = 0; j < fam->occ_count; j++) {
        const Occ *occ = &fam->occ[j];
        StandardFormMotiveOccurrence *item = &out->occurrences[j];
        out->occurrence_count = j + 1;
        StandardFormMotiveOccurrenceClaim *oc = &item->claim;
        oc->family = index;
        oc->occurrence = (int)j;
        oc->line = fam->line;
        oc->note_first = (int)occ->first;
        oc->note_count = (int)occ->n;
        oc->kind = occ->kind;
        oc->dimension_generic = fam->generic ? 1 : 0;
        oc->mismatches = occ->mismatches;
        oc->intervals = (int)occ->intervals;
        oc->rhythm_factor = occ->rhythm_factor;
        oc->score = occ->score;
        oc->fragment_part = occ->fragment_part;
        oc->fragment_intervals = (int)occ->fragment_intervals;
        oc->in_continuation = occ->in_continuation ? 1 : 0;
        oc->first_measure = line->first_measure[occ->first];
        StandardFormQ measure_start;
        StandardFormQ beat;
        oc->first_beat = 1.0;
        if (standard_form_measure_bounds(input->map, oc->first_measure, &measure_start, NULL) &&
            standard_form_beat_length(input->map, oc->first_measure, &beat)) {
            StandardFormQ offset = standard_form_q_sub(line->start[occ->first], measure_start);
            oc->first_beat = 1.0 + ((double)offset.n / (double)offset.d) / ((double)beat.n / (double)beat.d);
        }
        item->uncertainty = 1.0 - occ->score;

        size_t count = occ->n;
        EntityID **region = (EntityID **)calloc(count, sizeof(EntityID *));
        EntityID *occurrence_id_value = NULL;
        EntityID *target = NULL;
        Occurrence *occurrence = NULL;
        if (region == NULL) {
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
        ok = true;
        for (size_t k = 0; ok && k < count; k++) {
            ok = entity_id_create(line->notes[occ->first + k].note_id, &region[k]) == ENTITY_ID_OK;
        }
        snprintf(text, sizeof(text), "standard-form-motive-%d-%d-occurrence-%zu", fam->line, index, j);
        ok = ok && entity_id_create(text, &occurrence_id_value) == ENTITY_ID_OK && entity_id_create(motif_text, &target) == ENTITY_ID_OK;
        if (ok && occurrence_create(occurrence_id_value, target, &occurrence) == OCCURRENCE_OK) {
            occurrence_id_value = NULL;
            target = NULL;
            ok = motif_occurrence_create(occurrence, region, count, &item->occurrence) == MOTIF_OCCURRENCE_OK;
            if (!ok) {
                occurrence_destroy(occurrence);
            }
        } else {
            ok = false;
        }
        if (!ok) {
            entity_id_destroy(occurrence_id_value);
            entity_id_destroy(target);
            for (size_t k = 0; k < count; k++) {
                entity_id_destroy(region[k]);
            }
            free(region);
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
        free(region);
        const char *occurrence_text = entity_id_value(motif_occurrence_id(item->occurrence));
        snprintf(interpretation_id, sizeof(interpretation_id), "standard-form-motive-occurrence-%d-%d-%zu", fam->line, index, j);
        item->interpretation = standard_form_analysis_interpretation(interpretation_id, occurrence_text,
                                                                     STANDARD_FORM_MOTIVE_OCCURRENCE_CLAIM_TYPE, oc, sizeof(*oc),
                                                                     item->uncertainty, "motive-matching");
        char key[64];
        snprintf(key, sizeof(key), "%d-%d-o%zu", fam->line, index, j);
        item->family_relationship = standard_form_analysis_relationship("motive-family", key, motif_text, occurrence_text,
                                                                        "occurrence-of");
        if (item->interpretation == NULL || item->family_relationship == NULL) {
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
        int last_measure = oc->first_measure;
        if (!standard_form_last_measure_touched(input->map, line->end[occ->first + occ->n - 1], &last_measure)) {
            last_measure = oc->first_measure;
        }
        const char *container = container_of(phrases, segmentation, oc->first_measure, last_measure);
        if (container != NULL) {
            item->contained_relationship =
                standard_form_analysis_relationship("motive-contained", key, occurrence_text, container, "contained-in");
            if (item->contained_relationship == NULL) {
                return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
            }
        }
    }
    return STANDARD_FORM_ANALYSIS_OK;
}

static int compare_families(const void *a, const void *b) {
    const Fam *left = (const Fam *)a;
    const Fam *right = (const Fam *)b;
    if (left->line != right->line) {
        return left->line - right->line;
    }
    if (left->proto_first != right->proto_first) {
        return left->proto_first < right->proto_first ? -1 : 1;
    }
    return (left->proto_n < right->proto_n) - (left->proto_n > right->proto_n); /* longer first */
}

StandardFormAnalysisStatus standard_form_find_motives(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                      const StandardFormSegmentation *segmentation,
                                                      const StandardFormThemes *themes, const StandardFormConfig *config,
                                                      StandardFormWindowMatcher matcher, void *matcher_context,
                                                      StandardFormMotives *out) {
    if (input == NULL || out == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardFormConfig defaults = standard_form_config_default();
    const StandardFormConfig *effective = config != NULL ? config : &defaults;
    if (!standard_form_config_valid(effective) || !input->finished) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG;
    }
    if (matcher == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR;
    }
    StandardFormMotives found = {NULL, 0};
    bool *idea_start = NULL;
    if (input->note_count > 0) {
        idea_start = (bool *)calloc(input->note_count, sizeof(bool));
        if (idea_start == NULL) {
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
        if (segmentation != NULL) {
            for (size_t i = 0; i < segmentation->idea_count; i++) {
                int first = segmentation->ideas[i].claim.note_first;
                if (segmentation->ideas[i].claim.note_end > first && (size_t)first < input->note_count) {
                    idea_start[first] = true;
                }
            }
        }
    }
    StandardFormMotiveLine lines[2];
    memset(lines, 0, sizeof(lines));
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    size_t line_total = effective->include_bass ? 2 : 1;
    for (size_t l = 0; l < line_total && status == STANDARD_FORM_ANALYSIS_OK; l++) {
        if (!standard_form_motive_line_build(&lines[l], input, (int)l, l == 0 ? idea_start : NULL)) {
            status = STANDARD_FORM_ANALYSIS_ERR_METER;
        }
    }
    Fam *families = NULL;
    size_t family_count = 0;
    size_t family_capacity = 0;
    for (size_t l = 0; l < line_total && status == STANDARD_FORM_ANALYSIS_OK; l++) {
        status = discover(&lines[l], effective, &families, &family_count, &family_capacity);
    }
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = expand(lines, families, &family_count, effective, themes);
    }
    if (status == STANDARD_FORM_ANALYSIS_OK && family_count > 1) {
        qsort(families, family_count, sizeof(Fam), compare_families);
    }
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = check_with_kernel(lines, families, family_count, matcher, matcher_context);
    }
    if (status == STANDARD_FORM_ANALYSIS_OK && family_count > 0) {
        found.families = (StandardFormMotiveFamily *)calloc(family_count, sizeof(StandardFormMotiveFamily));
        if (found.families == NULL) {
            status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
    }
    int per_line_index[2] = {0, 0};
    for (size_t f = 0; status == STANDARD_FORM_ANALYSIS_OK && f < family_count; f++) {
        status = emit_family(input, lines, phrases, segmentation, &families[f], per_line_index[families[f].line]++,
                             &found.families[f]);
        found.family_count = f + 1;
    }
    for (size_t f = 0; f < family_count; f++) {
        free(families[f].occ);
    }
    free(families);
    for (size_t l = 0; l < 2; l++) {
        standard_form_motive_line_destroy(&lines[l]);
    }
    free(idea_start);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        standard_form_motives_destroy(&found);
        return status;
    }
    if (found.family_count == 0) {
        free(found.families);
        found.families = NULL;
    }
    *out = found;
    return STANDARD_FORM_ANALYSIS_OK;
}
