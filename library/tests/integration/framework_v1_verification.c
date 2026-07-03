#include <stdarg.h>

#include "framework_v1_chain.h"
#include "framework_v1_deviations.h"

/*
 * Framework v1 verification: the full chain over the five reference pieces (Bach BWV Anh. 114, Clementi Op. 36 No. 1, Field Nocturne No. 1,
 * Field Nocturne No. 5, Mozart K. 545) against exact expected values, for three areas, one case per piece per area:
 *
 *   KEYS         the home key; every key region (its key, with its first and last measure each within one measure); every key change
 *                (within one measure, with its KeyChangeExtent).
 *   CADENCES     every annotated cadence (arrival within one beat, CadenceType exactly: PAC and IAC are different), and no cadence that is not
 *                annotated inside the cadence scope.
 *   PHRASE FORM  every annotated phrase end (within one measure), every annotated theme (its form, and its two parts, each edge within one
 *                measure) and every annotated expansion.
 *
 * The expected values are tests/fixtures/annotations/<piece>_v1_expected.txt: DRAFT hand readings, unverified against the editions (see their
 * headers). Where the chain does not give an expected value, the location is a DEVIATION listed in tests/fixtures/framework-v1-corpus/
 * expected_deviations.md with the exact actual value and its cause (a method limit of a stage and the roadmap item that would remove it); see
 * framework_v1_deviations.h. A check that fails without a listed deviation fails the test; a listed deviation that is no longer one fails it.
 * Nothing is skipped, no threshold is chosen, and no default is tuned to a piece.
 *
 * Also asserted, before any of it: the v1 files agree with the older annotation files and with each other (phrase ends coincide with cadences).
 * The Bach regression assertions for PR #36 (unique Chord ids across the piece) are below; the one for PR #37 (thematic-return window) is in
 * framework_v1_thematic_returns.c.
 *
 * URSATZ_CORPUS_DIR, URSATZ_ANNOTATION_DIR, URSATZ_DEVIATIONS_FILE and URSATZ_VERIFICATION_SUMMARY come from CMake.
 */

static const FormPiece *fv1_piece(size_t i) {
    return &FORM_PIECES[i];
}

static const char *fv1_key_name(int key_index, char out[24]) {
    snprintf(out, 24, "%s %s", common_practice_key_index_tonic_name(key_index),
             common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR ? "minor" : "major");
    return out;
}

static const char *fv1_type_text(int type) {
    switch ((CadenceType)type) {
    case CADENCE_TYPE_PERFECT_AUTHENTIC:
        return "pac";
    case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
        return "iac";
    case CADENCE_TYPE_HALF:
        return "hc";
    case CADENCE_TYPE_DECEPTIVE:
        return "deceptive";
    case CADENCE_TYPE_PLAGAL:
        return "plagal";
    case CADENCE_TYPE_SUBVERTED:
        return "subverted";
    default:
        return "other";
    }
}

static void fv1_append(char *out, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));
static void fv1_append(char *out, size_t size, const char *format, ...) {
    size_t length = strlen(out);
    if (length + 1 >= size) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(out + length, size - length, format, args);
    va_end(args);
}

/* --- keys --- */

static void fv1_check_keys(const Chain *c, Fv1Verifier *v) {
    const V1Expected *x = &c->x;
    const CommonPracticeKeyRegions *r = &c->regions;
    char name[24];
    char name2[24];

    /* the home key: the key the chain's own estimator gives the whole scope taken as one window (key_window_scores, the library's profiles) */
    CommonPracticeKeyRegionsConfig whole = common_practice_key_regions_config_default();
    whole.window_measures = (size_t)(x->keys_last - x->keys_first + 1);
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(c->key_segments, c->count, c->map, &whole, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK && windows.count >= 1);
    int home = 0;
    for (int k = 1; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        if (windows.items[0].correlation[k] > windows.items[0].correlation[home]) {
            home = k;
        }
    }
    common_practice_key_windows_destroy(&windows);
    fv1_check(v, FV1_AREA_KEYS, "home", home == x->home_key, fv1_key_name(x->home_key, name), fv1_key_name(home, name2));

    for (size_t i = 0; i < x->region_count; i++) {
        const XRegion *want = &x->regions[i];
        if (want->unscored) {
            continue;
        }
        char location[72];
        char expected[160];
        char actual[480] = "";
        snprintf(location, sizeof(location), "region %d-%d", want->first, want->last);
        snprintf(expected, sizeof(expected), "%s %d-%d", fv1_key_name(want->key_index, name), want->first, want->last);
        const CommonPracticeKeyRegion *match = NULL;
        for (size_t k = 0; k < r->region_count; k++) {
            const CommonPracticeKeyRegion *g = &r->regions[k];
            if (g->key_index == want->key_index && abs(g->first_measure - want->first) <= 1 && abs(g->last_measure - want->last) <= 1) {
                match = g;
            }
        }
        if (match != NULL) {
            snprintf(actual, sizeof(actual), "%s %d-%d", fv1_key_name(match->key_index, name), match->first_measure, match->last_measure);
        } else {
            for (size_t k = 0; k < r->region_count; k++) {
                const CommonPracticeKeyRegion *g = &r->regions[k];
                if (g->last_measure >= want->first && g->first_measure <= want->last) {
                    fv1_append(actual, sizeof(actual), "%s%s %d-%d", actual[0] != '\0' ? ", " : "", fv1_key_name(g->key_index, name),
                               g->first_measure, g->last_measure);
                }
            }
            if (actual[0] == '\0') {
                snprintf(actual, sizeof(actual), "none");
            }
        }
        fv1_check(v, FV1_AREA_KEYS, location, match != NULL, expected, actual);
    }

    for (size_t i = 0; i < x->change_count; i++) {
        const XChange *want = &x->changes[i];
        char location[72];
        char expected[160];
        char actual[480];
        snprintf(location, sizeof(location), "change %d", want->measure);
        snprintf(expected, sizeof(expected), "%s%s at %d", key_change_extent_name((KeyChangeExtent)want->extent),
                 want->is_return ? " return" : "", want->measure);
        const CommonPracticeKeyChange *best = NULL;
        int best_boundary = 0;
        bool right = false;
        for (size_t k = 0; k < r->change_count; k++) {
            int boundary = r->regions[r->changes[k].to_region].first_measure;
            if (abs(boundary - want->measure) > 1) {
                continue;
            }
            bool this_right = (int)r->changes[k].extent == want->extent && (!want->is_return || r->changes[k].is_return);
            if (best == NULL || (this_right && !right) ||
                (this_right == right && abs(boundary - want->measure) < abs(best_boundary - want->measure))) {
                best = &r->changes[k];
                best_boundary = boundary;
                right = this_right;
            }
        }
        if (best != NULL) {
            snprintf(actual, sizeof(actual), "%s%s at %d", key_change_extent_name(best->extent), best->is_return ? " return" : "",
                     best_boundary);
        } else {
            snprintf(actual, sizeof(actual), "none");
        }
        fv1_check(v, FV1_AREA_KEYS, location, best != NULL && right, expected, actual);
    }
}

/* --- cadences --- */

static void fv1_beat_text(double beat, char out[16]) {
    snprintf(out, 16, "%g", beat);
}

/* A readings side against one reading of a found cadence: the same kind (a type, or a prolongation), within one beat of the arrival. */
static bool fv1_reading_matches(const Chain *c, const XReadingSide *want, bool prolongation, CadenceType type, size_t arrival_first) {
    double at = fv1_time_of(c, want->measure, want->beat);
    return want->prolongation == prolongation && (prolongation || want->type == type) &&
           fabs(c->p.starts[arrival_first] - at) <= c->shim.beat_length + 1e-9;
}

static void fv1_reading_text(const Chain *c, bool prolongation, CadenceType type, size_t arrival_first, int share, char *out, size_t size) {
    int measure = 0;
    double b = 0.0;
    fv1_where(c, arrival_first, &measure, &b);
    char bt[16];
    fv1_beat_text(b, bt);
    if (prolongation) {
        snprintf(out, size, "prolongation at m%d b%s (%d)", measure, bt, share);
    } else {
        snprintf(out, size, "%s at m%d b%s (%d)", fv1_type_text(type), measure, bt, share);
    }
}

/* The `readings` lines: one found result whose primary and alternative are the two readings, in either order. The result is marked used
   (whether it matches or not, the nearest one) so it is neither matched against a `cadence` line nor reported as an extra. */
static void fv1_check_readings(const Chain *c, Fv1Verifier *v, bool *used) {
    const V1Expected *x = &c->x;
    const HarmonyCadences *found = &c->cadences;
    double tolerance = c->shim.beat_length + 1e-9;
    for (size_t i = 0; i < x->readings_count; i++) {
        const XReadings *want = &x->readings[i];
        char beat[16];
        fv1_beat_text(want->side[0].beat, beat);
        char location[72];
        char expected[160];
        char actual[480];
        char first[80];
        char second[80];
        snprintf(location, sizeof(location), "readings m%d b%s", want->side[0].measure, beat);
        snprintf(expected, sizeof(expected), "%s m%d or %s m%d%s", want->side[0].text, want->side[0].measure, want->side[1].text,
                 want->side[1].measure, want->prefer != 0 ? (want->prefer == 1 ? ", first likelier" : ", second likelier") : "");
        long match = -1;
        long near = -1;
        int match_order = 0;
        for (size_t k = 0; k < found->count && match < 0; k++) {
            const HarmonyCadenceResult *r = &found->items[k];
            double d0 = fabs(c->p.starts[r->arrival_first] - fv1_time_of(c, want->side[0].measure, want->side[0].beat));
            double d1 = fabs(c->p.starts[r->arrival_first] - fv1_time_of(c, want->side[1].measure, want->side[1].beat));
            if (near < 0 && (d0 <= tolerance || d1 <= tolerance)) {
                near = (long)k;
            }
            if (!r->alternative.present) {
                continue;
            }
            for (int order = 0; order < 2 && match < 0; order++) {
                if (fv1_reading_matches(c, &want->side[order], false, r->type, r->arrival_first) &&
                    fv1_reading_matches(c, &want->side[1 - order], r->alternative.reading == HARMONY_CADENCE_READING_PROLONGATION,
                                        r->alternative.type, r->alternative.arrival_first)) {
                    match = (long)k;
                    match_order = order;
                }
            }
        }
        bool right = match >= 0;
        long shown = match >= 0 ? match : near;
        if (shown >= 0) {
            const HarmonyCadenceResult *r = &found->items[(size_t)shown];
            used[shown] = true;
            fv1_reading_text(c, false, r->type, r->arrival_first, r->share, first, sizeof(first));
            if (r->alternative.present) {
                fv1_reading_text(c, r->alternative.reading == HARMONY_CADENCE_READING_PROLONGATION, r->alternative.type,
                                 r->alternative.arrival_first, r->alternative.share, second, sizeof(second));
                snprintf(actual, sizeof(actual), "%s, or %s", first, second);
            } else {
                snprintf(actual, sizeof(actual), "%s, no alternative", first);
            }
            if (right && want->prefer != 0) {
                /* side[order] is the primary's reading: the preferred side has the higher share when it is the primary and the primary
                   leads, or the alternative and it leads */
                int primary_side = match_order + 1;
                bool primary_leads = r->share > r->alternative.share;
                bool alternative_leads = r->alternative.share > r->share;
                right = (want->prefer == primary_side && primary_leads) || (want->prefer != primary_side && alternative_leads);
            }
        } else {
            snprintf(actual, sizeof(actual), "none");
        }
        fv1_check(v, FV1_AREA_CADENCES, location, right, expected, actual);
    }
}

static void fv1_check_cadences(const Chain *c, Fv1Verifier *v) {
    const V1Expected *x = &c->x;
    const HarmonyCadences *found = &c->cadences;
    bool *used = (bool *)calloc(found->count > 0 ? found->count : 1, sizeof(bool));
    assert(used != NULL);
    bool *claimed = (bool *)calloc(found->count > 0 ? found->count : 1, sizeof(bool)); /* results a `readings` line took */
    assert(claimed != NULL);
    fv1_check_readings(c, v, claimed);
    for (size_t k = 0; k < found->count; k++) {
        used[k] = claimed[k];
    }
    double tolerance = c->shim.beat_length + 1e-9;
    for (size_t i = 0; i < x->cadence_count; i++) {
        const XCadence *want = &x->cadences[i];
        char beat[16];
        fv1_beat_text(want->beat, beat);
        char location[72];
        char expected[160];
        char actual[480];
        snprintf(location, sizeof(location), "m%d b%s", want->measure, beat);
        snprintf(expected, sizeof(expected), "%s", want->type_text);
        double at = fv1_time_of(c, want->measure, want->beat);
        long best = -1;
        for (size_t k = 0; k < found->count; k++) {
            if (claimed[k] || fabs(c->p.starts[found->items[k].arrival_first] - at) > tolerance) {
                continue;
            }
            if (best < 0 || (found->items[k].type == want->type && found->items[(size_t)best].type != want->type) ||
                (fabs(c->p.starts[found->items[k].arrival_first] - at) <
                     fabs(c->p.starts[found->items[(size_t)best].arrival_first] - at) &&
                 (found->items[k].type == want->type) == (found->items[(size_t)best].type == want->type))) {
                best = (long)k;
            }
        }
        bool right = false;
        if (want->optional && (best < 0 || found->items[(size_t)best].type != want->type)) {
            continue; /* an optional cadence that is missing is no deviation */
        }
        if (best >= 0) {
            used[best] = true;
            int measure = 0;
            double b = 0.0;
            fv1_where(c, found->items[(size_t)best].arrival_first, &measure, &b);
            char bt[16];
            fv1_beat_text(b, bt);
            snprintf(actual, sizeof(actual), "%s at m%d b%s", fv1_type_text(found->items[(size_t)best].type), measure, bt);
            right = found->items[(size_t)best].type == want->type;
        } else {
            snprintf(actual, sizeof(actual), "none");
        }
        fv1_check(v, FV1_AREA_CADENCES, location, right, expected, actual);
    }
    if (x->cadence_count == 0 && x->readings_count == 0) {
        /* a piece with no cadence in its scope: the claim is that there is none */
        size_t in_scope = 0;
        for (size_t k = 0; k < found->count; k++) {
            int measure = 0;
            double b = 0.0;
            fv1_where(c, found->items[k].arrival_first, &measure, &b);
            in_scope += (measure >= x->cadence_first && measure <= x->cadence_last) ? 1 : 0;
        }
        char actual[64];
        snprintf(actual, sizeof(actual), in_scope == 0 ? "no cadence" : "%zu cadence(s)", in_scope);
        fv1_check(v, FV1_AREA_CADENCES, "none in scope", in_scope == 0, "no cadence", actual);
    }
    for (size_t k = 0; k < found->count; k++) {
        int measure = 0;
        double b = 0.0;
        fv1_where(c, found->items[k].arrival_first, &measure, &b);
        if (used[k] || measure < x->cadence_first || measure > x->cadence_last) {
            continue;
        }
        char bt[16];
        fv1_beat_text(b, bt);
        char location[72];
        snprintf(location, sizeof(location), "extra m%d b%s", measure, bt);
        fv1_check(v, FV1_AREA_CADENCES, location, false, "none", fv1_type_text(found->items[k].type));
    }
    free(claimed);
    free(used);
}

/* --- phrase form --- */

static void fv1_theme_text(const StandardFormThemeClaim *t, char *out, size_t size) {
    snprintf(out, size, "%s %d-%d parts %d-%d %d-%d", phrase_level_form_name((PhraseLevelForm)t->form), t->first_measure, t->last_measure,
             t->first_part_first_measure, t->first_part_last_measure, t->second_part_first_measure, t->second_part_last_measure);
}

static void fv1_check_form(const Chain *c, Fv1Verifier *v) {
    const V1Expected *x = &c->x;
    const FormAnalysis *f = &c->form;
    for (size_t i = 0; i < x->end_count; i++) {
        const XEnd *want = &x->ends[i];
        char location[72];
        char expected[160];
        char actual[480];
        snprintf(location, sizeof(location), "end m%d", want->measure);
        snprintf(expected, sizeof(expected), "%s at m%d", want->type_text, want->measure);
        /* an end at the measure of either reading of a `readings` line may be found at either reading's measure */
        int targets[1 + 2 * 8];
        size_t target_count = 0;
        targets[target_count++] = want->measure;
        for (size_t r = 0; r < x->readings_count; r++) {
            if (x->readings[r].side[0].measure == want->measure || x->readings[r].side[1].measure == want->measure) {
                targets[target_count++] = x->readings[r].side[0].measure;
                targets[target_count++] = x->readings[r].side[1].measure;
            }
        }
        const StandardFormPhraseClaim *best = NULL;
        int best_distance = 0;
        for (size_t k = 0; k < f->phrases.count; k++) {
            const StandardFormPhraseClaim *p = &f->phrases.items[k].claim;
            for (size_t t = 0; t < target_count; t++) {
                int distance = abs(p->end_measure - targets[t]);
                if (distance <= 1 && (best == NULL || distance < best_distance)) {
                    best = p;
                    best_distance = distance;
                }
            }
        }
        if (best != NULL) {
            snprintf(actual, sizeof(actual), "%s at m%d", best->cadence_type >= 0 ? fv1_type_text(best->cadence_type) : "no cadence",
                     best->end_measure);
        } else {
            snprintf(actual, sizeof(actual), "none");
        }
        fv1_check(v, FV1_AREA_PHRASE_FORM, location, best != NULL, expected, actual);
    }
    for (size_t i = 0; i < x->theme_count; i++) {
        const XTheme *want = &x->themes[i];
        char location[72];
        char expected[160];
        char actual[480] = "";
        snprintf(location, sizeof(location), "theme %s %d-%d", phrase_level_form_name(want->form), want->first, want->last);
        snprintf(expected, sizeof(expected), "%s %d-%d parts %d-%d %d-%d", phrase_level_form_name(want->form), want->first, want->last,
                 want->a_first, want->a_last, want->b_first, want->b_last);
        const StandardFormThemeClaim *match = NULL;
        for (size_t k = 0; k < f->themes.count; k++) {
            const StandardFormThemeClaim *t = &f->themes.items[k].claim;
            if (t->form != (int)want->form || abs(t->first_measure - want->first) > 1 || abs(t->last_measure - want->last) > 1) {
                continue;
            }
            if (want->has_parts && (abs(t->first_part_first_measure - want->a_first) > 1 || abs(t->first_part_last_measure - want->a_last) > 1 ||
                                    abs(t->second_part_first_measure - want->b_first) > 1 ||
                                    abs(t->second_part_last_measure - want->b_last) > 1)) {
                continue;
            }
            match = t;
        }
        if (match != NULL) {
            fv1_theme_text(match, actual, sizeof(actual));
        } else {
            for (size_t k = 0; k < f->themes.count; k++) {
                const StandardFormThemeClaim *t = &f->themes.items[k].claim;
                if (t->last_measure >= want->first && t->first_measure <= want->last) {
                    char one[120];
                    fv1_theme_text(t, one, sizeof(one));
                    fv1_append(actual, sizeof(actual), "%s%s", actual[0] != '\0' ? ", " : "", one);
                }
            }
            if (actual[0] == '\0') {
                snprintf(actual, sizeof(actual), "none");
            }
        }
        fv1_check(v, FV1_AREA_PHRASE_FORM, location, match != NULL, expected, actual);
    }
    for (size_t i = 0; i < x->expansion_count; i++) {
        const XExpansion *want = &x->expansions[i];
        char location[72];
        char expected[160];
        char actual[480] = "";
        static const char *const KIND[3] = {"internal", "external", "contraction"};
        snprintf(location, sizeof(location), "expansion %s %d-%d", KIND[want->kind], want->first, want->last);
        snprintf(expected, sizeof(expected), "%s %d-%d %d measures", KIND[want->kind], want->first, want->last, want->measures);
        const StandardFormExpansionClaim *match = NULL;
        for (size_t k = 0; k < f->expansions.count; k++) {
            const StandardFormExpansionClaim *e = &f->expansions.items[k].claim;
            if (e->kind == (int)want->kind && abs(e->measures_changed - want->measures) <= 1 && abs(e->first_measure - want->first) <= 1 &&
                abs(e->last_measure - want->last) <= 1) {
                match = e;
            }
        }
        if (match != NULL) {
            snprintf(actual, sizeof(actual), "%s %d-%d %d measures", KIND[match->kind], match->first_measure, match->last_measure,
                     match->measures_changed);
        } else {
            for (size_t k = 0; k < f->expansions.count; k++) {
                const StandardFormExpansionClaim *e = &f->expansions.items[k].claim;
                if (e->last_measure >= want->first && e->first_measure <= want->last) {
                    fv1_append(actual, sizeof(actual), "%s%s %d-%d %d measures", actual[0] != '\0' ? ", " : "", KIND[e->kind],
                               e->first_measure, e->last_measure, e->measures_changed);
                }
            }
            if (actual[0] == '\0') {
                snprintf(actual, sizeof(actual), "none");
            }
        }
        fv1_check(v, FV1_AREA_PHRASE_FORM, location, match != NULL, expected, actual);
    }
}

/* --- the Bach regression for PR #36: every Chord of the piece has its own EntityID --- */

static int fv1_compare_strings(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static size_t fv1_chord_id_count(const Chain *c, bool *unique) {
    const char **ids = (const char **)calloc(c->count, sizeof(char *));
    assert(ids != NULL);
    size_t n = 0;
    for (size_t s = 0; s < c->count; s++) {
        if (!c->chords[s].matched) {
            continue;
        }
        ids[n++] = entity_id_value(interpretation_id(chord_interpretation(c->chords[s].best_chord)));
    }
    qsort(ids, n, sizeof(char *), fv1_compare_strings);
    *unique = true;
    for (size_t i = 1; i < n; i++) {
        if (strcmp(ids[i - 1], ids[i]) == 0) {
            *unique = false;
        }
    }
    free(ids);
    return n;
}

/* --- the files agree --- */

static void fv1_check_fixture_agreement(void) {
    for (size_t i = 0; i < 5; i++) {
        const FormPiece *fp = fv1_piece(i);
        V1Expected x;
        fv1_load_expected(fp->prefix, &x);
        /* the older cadence fixture: every cadence of it is in the v1 file */
        Annotation a = load_annotation(fp->piece.fixture);
        double beat_length = (double)a.beat_num / (double)a.beat_den;
        assert(x.beat_num == a.beat_num && x.beat_den == a.beat_den && x.beats == a.beats);
        assert(x.home_key >= 0 && (x.home_key % 12) == a.home);
        for (size_t e = 0; e < a.cadence_count; e++) {
            bool found = false;
            for (size_t k = 0; k < x.cadence_count; k++) {
                if (x.cadences[k].optional) {
                    continue;
                }
                double at = ((double)(x.cadences[k].measure - 1) * x.beats + (x.cadences[k].beat - 1.0)) * beat_length;
                found = found || (fabs(at - a.cadences[e].at) < 1e-9 && x.cadences[k].type == a.cadences[e].type);
            }
            for (size_t k = 0; k < x.readings_count; k++) {
                for (int side = 0; side < 2; side++) {
                    const XReadingSide *r = &x.readings[k].side[side];
                    double at = ((double)(r->measure - 1) * x.beats + (r->beat - 1.0)) * beat_length;
                    found = found || (!r->prolongation && fabs(at - a.cadences[e].at) < 1e-9 && r->type == a.cadences[e].type);
                }
            }
            if (!found) {
                printf("  %s: a cadence of %s is not in the v1 file\n", x.piece, fp->piece.fixture);
            }
            assert(found);
        }
        /* the older phrase form fixture */
        FormAnnotation f = load_form_annotation(fp->prefix);
        for (size_t e = 0; e < f.end_count; e++) {
            bool found = false;
            for (size_t k = 0; k < x.end_count; k++) {
                found = found || (x.ends[k].measure == f.ends[e].measure && x.ends[k].type == f.ends[e].type);
            }
            assert(found);
        }
        for (size_t t = 0; t < f.theme_count; t++) {
            bool found = false;
            for (size_t k = 0; k < x.theme_count; k++) {
                found = found || (x.themes[k].form == f.themes[t].form && x.themes[k].first == f.themes[t].first &&
                                  x.themes[k].last == f.themes[t].last);
            }
            assert(found);
        }
        for (size_t e = 0; e < f.expansion_count; e++) {
            bool found = false;
            for (size_t k = 0; k < x.expansion_count; k++) {
                found = found || (x.expansions[k].kind == f.expansions[e].kind && x.expansions[k].first == f.expansions[e].first &&
                                  x.expansions[k].last == f.expansions[e].last && x.expansions[k].measures == f.expansions[e].measures);
            }
            assert(found);
        }
        /* within the v1 file: every cadence is in its scope, every phrase end in a form scope coincides with a cadence (same type, within one
           measure) when the end is also in the cadence scope, and a theme's parts lie inside it */
        for (size_t k = 0; k < x.cadence_count; k++) {
            assert(x.cadences[k].measure >= x.cadence_first && x.cadences[k].measure <= x.cadence_last);
        }
        for (size_t k = 0; k < x.readings_count; k++) {
            for (int side = 0; side < 2; side++) {
                assert(x.readings[k].side[side].measure >= x.cadence_first && x.readings[k].side[side].measure <= x.cadence_last);
            }
        }
        for (size_t k = 0; k < x.end_count; k++) {
            assert(x.ends[k].measure >= x.form_first && x.ends[k].measure <= x.form_last);
            if (x.ends[k].measure < x.cadence_first || x.ends[k].measure > x.cadence_last) {
                continue;
            }
            bool coincides = false;
            for (size_t m = 0; m < x.cadence_count; m++) {
                if (x.cadences[m].optional) {
                    continue;
                }
                coincides = coincides || (abs(x.cadences[m].measure - x.ends[k].measure) <= 1 && x.cadences[m].type == x.ends[k].type);
            }
            for (size_t m = 0; m < x.readings_count; m++) {
                for (int side = 0; side < 2; side++) {
                    const XReadingSide *r = &x.readings[m].side[side];
                    coincides = coincides || (!r->prolongation && abs(r->measure - x.ends[k].measure) <= 1 && r->type == x.ends[k].type);
                }
            }
            if (!coincides) {
                printf("  %s: phrase end m%d has no cadence of its type\n", x.piece, x.ends[k].measure);
            }
            assert(coincides);
        }
        for (size_t k = 0; k < x.theme_count; k++) {
            assert(x.themes[k].first >= x.form_first && x.themes[k].last <= x.form_last);
            if (x.themes[k].has_parts) {
                assert(x.themes[k].a_first == x.themes[k].first && x.themes[k].b_last == x.themes[k].last &&
                       x.themes[k].a_last < x.themes[k].b_first);
            }
        }
    }
    /* the older key region fixtures (Mozart 545 and Clementi 36): their regions and changes are in the v1 files */
    static const char *const OLD_KEYS[2][2] = {{"mozart545", "mozart545_key_regions.txt"}, {"clementi36", "clementi36_key_regions.txt"}};
    for (size_t o = 0; o < 2; o++) {
        V1Expected x;
        fv1_load_expected(OLD_KEYS[o][0], &x);
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", URSATZ_ANNOTATION_DIR, OLD_KEYS[o][1]);
        size_t size = 0;
        unsigned char *text = pipeline_read_file(path, &size);
        for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
            int first = 0;
            int last = 0;
            char tonic[8];
            char mode[16];
            bool found = false;
            if (sscanf(line, "region %d %d %7s %15s", &first, &last, tonic, mode) == 4) {
                for (size_t k = 0; k < x.region_count; k++) {
                    found = found || (!x.regions[k].unscored && x.regions[k].first == first && x.regions[k].last == last &&
                                      x.regions[k].key_index == fv1_key_index(tonic, mode));
                }
                assert(found);
            } else if (sscanf(line, "unscored %d %d", &first, &last) == 2) {
                for (size_t k = 0; k < x.region_count; k++) {
                    found = found || (x.regions[k].unscored && x.regions[k].first == first && x.regions[k].last == last);
                }
                assert(found);
            } else if (sscanf(line, "change %d %15s", &first, mode) == 2 && strcmp(mode, "modulation") == 0) {
                for (size_t k = 0; k < x.change_count; k++) {
                    found = found || x.changes[k].measure == first;
                }
                assert(found);
            }
        }
        free(text);
    }
    /* every reference piece has its v1 file (the corpus regression asserts the same of every .mid in the directory) */
    printf("the v1 expected files agree with the older annotation files and with each other\n");
}

/* --- main --- */

int main(void) {
    fv1_check_fixture_agreement();

    static Fv1DeviationTable table;
    fv1_load_deviations(URSATZ_DEVIATIONS_FILE, &table);
    size_t failures = 0;
    Fv1Count total[FV1_AREA_COUNT];
    memset(total, 0, sizeof(total));
    FILE *summary = fopen(URSATZ_VERIFICATION_SUMMARY, "w");
    assert(summary != NULL);

    for (size_t i = 0; i < 5; i++) {
        Chain c;
        fv1_chain_run(&c, fv1_piece(i));
        Fv1Verifier v;
        memset(&v, 0, sizeof(v));
        v.table = &table;
        v.piece = c.x.piece;
        printf("%s (measures %d-%d, %zu segments, %zu key regions, %zu cadences):\n", c.x.piece, c.x.keys_first, c.x.keys_last, c.count,
               c.regions.region_count, c.cadences.count);
        fv1_check_keys(&c, &v);
        fv1_check_cadences(&c, &v);
        fv1_check_form(&c, &v);
        for (int a = 0; a < FV1_AREA_COUNT; a++) {
            if (a == FV1_AREA_RETURNS) {
                continue;
            }
            printf("  %-12s %zu pass, %zu deviation(s), %zu fail\n", FV1_AREA_NAMES[a], v.counts[a].pass, v.counts[a].deviation,
                   v.counts[a].fail);
            fprintf(summary, "%s %s %zu %zu\n", c.x.piece, FV1_AREA_NAMES[a], v.counts[a].pass, v.counts[a].deviation);
            total[a].pass += v.counts[a].pass;
            total[a].deviation += v.counts[a].deviation;
            total[a].fail += v.counts[a].fail;
            failures += v.counts[a].fail;
        }
        if (strcmp(c.x.piece, "bach114") == 0) {
            bool unique = false;
            size_t n = fv1_chord_id_count(&c, &unique);
            printf("  PR #36 regression: %zu Chord ids across the piece, %s\n", n, unique ? "all distinct" : "DUPLICATES");
            assert(n > 20 && unique);
        }
        fv1_chain_destroy(&c);
    }
    fclose(summary);

    for (size_t i = 0; i < table.deviation_count; i++) {
        const Fv1Deviation *d = &table.deviations[i];
        bool area_here = strcmp(d->area, "returns") != 0;
        if (area_here && !d->used) {
            printf("  FAIL %s is not used by any check (%s %s %s)\n", d->id, d->piece, d->area, d->location);
            failures++;
        }
    }
    printf("All five pieces:");
    for (int a = 0; a < FV1_AREA_COUNT; a++) {
        if (a != FV1_AREA_RETURNS) {
            printf(" %s %zu pass / %zu deviation(s);", FV1_AREA_NAMES[a], total[a].pass, total[a].deviation);
        }
    }
    printf("\n");
    if (failures > 0) {
        printf("framework_v1_verification: %zu failure(s)\n", failures);
        return 1;
    }
    printf("framework_v1_verification: ok\n");
    return 0;
}
