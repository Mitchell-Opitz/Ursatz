#include "framework_v1_chain.h"
#include "framework_v1_deviations.h"
#include "chain/chain_inputs.h"

#include "analysis/form/thematic_return_detection.h"
#include "ursatz_test_knowledge.h"

/*
 * Framework v1 verification, thematic returns (Mozart K. 545 and Clementi Op. 36 No. 1), and the Bach regression for PR #37.
 *
 * THE GLUE. The themes are those of the L3.6 standard-form adapter on the cadences the chain found (framework_v1_chain.h). Each theme becomes a
 * theme occurrence (its own id and the ids of the soprano notes of the theme, which the adapter keeps in the theme's shell) and a MotifWindow: the Contour of those notes' pitches (up, down or same), their RhythmicPattern (the durations of
 * the notes), and their event ids. The kernel's ThematicReturnDetection (analysis/thematic_return_detection.h, with its minimum window of
 * THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH events) compares the windows and builds one "resembles" pair from each later occurrence to
 * each earlier one it matches. Nothing in the library changed to feed it.
 *
 * THE CHECKS. An annotated return (a later statement of a named original) is FOUND when an occurrence that starts within one measure of it
 * resembles an occurrence that starts within one measure of the original. Where it is not, the location is a deviation in
 * tests/fixtures/framework-v1-corpus/expected_deviations.md (area "returns") with the actual value, as in framework_v1_verification.c.
 *
 * THE BACH REGRESSION (PR #37, the minimum window). Reusing the exact comparison rule at theme granularity with no floor made short generic windows
 * (a three-event stepwise run) match across unrelated places. On Bach 114: of all the sliding three-note windows of the soprano, many pairs share
 * contour and rhythm (asserted, so the check has teeth), and the detector reports none of them; and two identical long windows (the written-out
 * repeat of the first strain) are matched, so the floor is not a blanket block.
 */

typedef struct {
    int first;
    int last;
    char form[24];
    char id[48];
    EntityID *occurrence_id;
    EntityID **region_ids;
    size_t region_count;
    Contour *contour;
    RhythmicPattern *rhythm;
    const EntityID **window_ids;
    MotifWindow window;
    bool usable;
} Occ;

typedef struct {
    Occ *items;
    size_t count;
} Occs;

/* Octave 4 C is 60. */
static long fv1_midi_number(const PitchSpelling *spelling) {
    return 12L * (pitch_spelling_octave(spelling) + 1) + pitch_letter_semitone(pitch_spelling_letter(spelling)) +
           pitch_spelling_accidental(spelling);
}

static PitchIdentity *fv1_pitch_identity(long semitone, size_t index) {
    char token[48];
    int length = snprintf(token, sizeof(token), "p%ld-%zu", semitone, index);
    PitchIdentity *id = NULL;
    assert(pitch_identity_create(token, (size_t)length, &id) == PITCH_IDENTITY_OK);
    return id;
}

/* The Contour, the RhythmicPattern and the window of `count` notes (count >= 2); the window borrows `ids`. */
static bool fv1_window_of_events(const NoteEvent *const *events, size_t count, const EntityID *const *ids, Contour **contour,
                                 RhythmicPattern **rhythm, MotifWindow *window) {
    if (count < 2) {
        return false;
    }
    PitchIdentity **pitch_ids = (PitchIdentity **)calloc(count, sizeof(PitchIdentity *));
    ContourDirection *directions = (ContourDirection *)calloc(count - 1, sizeof(ContourDirection));
    Duration *durations = (Duration *)calloc(count, sizeof(Duration));
    assert(pitch_ids != NULL && directions != NULL && durations != NULL);
    long previous = 0;
    for (size_t i = 0; i < count; i++) {
        long semitone = fv1_midi_number(note_event_optional_pitch_spelling(events[i]));
        pitch_ids[i] = fv1_pitch_identity(semitone, i);
        if (i > 0) {
            directions[i - 1] = semitone > previous ? CONTOUR_DIRECTION_UP : (semitone < previous ? CONTOUR_DIRECTION_DOWN : CONTOUR_DIRECTION_SAME);
        }
        previous = semitone;
        durations[i] = *note_event_duration(events[i]);
    }
    assert(contour_create(pitch_ids, count, directions, count - 1, contour) == CONTOUR_OK);
    assert(rhythmic_pattern_create(durations, count, rhythm) == RHYTHMIC_PATTERN_OK);
    /* the Contour owns the elements and copies the arrays; the pattern copies the durations; the arrays are ours to free */
    free(pitch_ids);
    free(directions);
    free(durations);
    window->contour = *contour;
    window->rhythm = *rhythm;
    window->event_ids = ids;
    window->event_id_count = count;
    return true;
}

static EntityID *fv1_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static const NoteEvent *fv1_find_event(const WeightedPipeline *p, const char *id) {
    for (size_t i = 0; i < p->note_count; i++) {
        if (strcmp(entity_id_value(note_event_id(p->events[i])), id) == 0) {
            return p->events[i];
        }
    }
    return NULL;
}

/* One occurrence per L3.6 theme of the chain. */
static Occs fv1_occurrences(const Chain *c) {
    Occs occs = {NULL, 0};
    occs.items = (Occ *)calloc(c->form.themes.count > 0 ? c->form.themes.count : 1, sizeof(Occ));
    assert(occs.items != NULL);
    for (size_t t = 0; t < c->form.themes.count; t++) {
        const StandardFormTheme *theme = &c->form.themes.items[t];
        Occ *o = &occs.items[occs.count++];
        o->first = theme->claim.first_measure;
        o->last = theme->claim.last_measure;
        snprintf(o->form, sizeof(o->form), "%s", phrase_level_form_name((PhraseLevelForm)theme->claim.form));
        snprintf(o->id, sizeof(o->id), "theme-occurrence-%zu", t);
        size_t n = structure_event_id_count(theme->shell);
        EntityID **region = (EntityID **)calloc(n > 0 ? n : 1, sizeof(EntityID *));
        const NoteEvent **events = (const NoteEvent **)calloc(n > 0 ? n : 1, sizeof(NoteEvent *));
        assert(region != NULL && events != NULL);
        for (size_t i = 0; i < n; i++) {
            const char *value = entity_id_value(structure_event_id_at(theme->shell, i));
            region[i] = fv1_id(value);
            events[i] = fv1_find_event(&c->p, value);
            assert(events[i] != NULL);
        }
        if (n == 0) {
            free(region);
            free(events);
            continue;
        }
        o->occurrence_id = fv1_id(o->id);
        o->region_ids = region; /* the occurrence owns the ids and the array */
        o->region_count = n;
        o->window_ids = (const EntityID **)calloc(n, sizeof(EntityID *));
        assert(o->window_ids != NULL);
        for (size_t i = 0; i < n; i++) {
            o->window_ids[i] = o->region_ids[i];
        }
        o->usable = fv1_window_of_events(events, n, o->window_ids, &o->contour, &o->rhythm, &o->window);
        free(events);
    }
    return occs;
}

static void fv1_occurrences_destroy(Occs *occs) {
    for (size_t i = 0; i < occs->count; i++) {
        Occ *o = &occs->items[i];
        if (o->contour != NULL) {
            contour_destroy(o->contour);
        }
        if (o->rhythm != NULL) {
            rhythmic_pattern_destroy(o->rhythm);
        }
        free(o->window_ids);
        if (o->occurrence_id != NULL) {
            entity_id_destroy(o->occurrence_id);
            for (size_t k = 0; k < o->region_count; k++) {
                entity_id_destroy(o->region_ids[k]);
            }
            free(o->region_ids);
        }
    }
    free(occs->items);
}

/* --- the returns of one piece --- */

static void fv1_check_returns(const Chain *c, Fv1Verifier *v) {
    const V1Expected *x = &c->x;
    Occs occs = fv1_occurrences(c);
    ThematicReturnDetectionEntry *entries = (ThematicReturnDetectionEntry *)calloc(occs.count > 0 ? occs.count : 1, sizeof(*entries));
    assert(entries != NULL);
    const Occ **entry_occ = (const Occ **)calloc(occs.count > 0 ? occs.count : 1, sizeof(*entry_occ));
    assert(entry_occ != NULL);
    size_t entry_count = 0;
    for (size_t i = 0; i < occs.count; i++) {
        if (occs.items[i].usable) {
            entries[entry_count].theme_occurrence_id = occs.items[i].occurrence_id;
            entries[entry_count].window = &occs.items[i].window;
            entry_occ[entry_count] = &occs.items[i];
            entry_count++;
        }
    }
    ThematicReturnPair *rels = NULL;
    size_t rel_count = 0;
    assert(thematic_return_detection_build_pairs_configured(entries, entry_count, NULL, &rels, &rel_count) == THEMATIC_RETURN_DETECTION_OK);

    char themes[240] = "";
    for (size_t i = 0; i < occs.count; i++) {
        char one[40];
        snprintf(one, sizeof(one), "%sm%d-%d", themes[0] != '\0' ? ", " : "", occs.items[i].first, occs.items[i].last);
        if (strlen(themes) + strlen(one) < sizeof(themes)) {
            strcat(themes, one);
        }
    }

    for (size_t i = 0; i < x->occurrence_count; i++) {
        const XOccurrence *ret = &x->occurrences[i];
        if (!ret->is_return) {
            continue;
        }
        const XOccurrence *original = NULL;
        for (size_t k = 0; k < x->occurrence_count; k++) {
            if (!x->occurrences[k].is_return && strcmp(x->occurrences[k].name, ret->name) == 0) {
                original = &x->occurrences[k];
            }
        }
        assert(original != NULL);
        char location[72];
        char expected[160];
        char actual[480] = "";
        snprintf(location, sizeof(location), "%s m%d", ret->name, ret->first);
        snprintf(expected, sizeof(expected), "m%d resembles m%d", ret->first, original->first);

        bool found = false;
        bool located = false;
        char targets[200] = "";
        char at[64] = "";
        for (size_t r = 0; r < rel_count; r++) {
            const Occ *later = entry_occ[rels[r].later];
            const Occ *earlier = entry_occ[rels[r].earlier];
            if (later == NULL || earlier == NULL || abs(later->first - ret->first) > 1) {
                continue;
            }
            located = true;
            snprintf(at, sizeof(at), "m%d-%d", later->first, later->last);
            char one[40];
            snprintf(one, sizeof(one), "%sm%d", targets[0] != '\0' ? ", " : "", earlier->first);
            if (strlen(targets) + strlen(one) < sizeof(targets)) {
                strcat(targets, one);
            }
            found = found || abs(earlier->first - original->first) <= 1;
        }
        if (located) {
            snprintf(actual, sizeof(actual), "%s resembles %s", at, targets);
        } else {
            bool occurrence_here = false;
            for (size_t k = 0; k < occs.count; k++) {
                if (abs(occs.items[k].first - ret->first) <= 1) {
                    occurrence_here = true;
                    snprintf(at, sizeof(at), "m%d-%d", occs.items[k].first, occs.items[k].last);
                }
            }
            if (occurrence_here) {
                snprintf(actual, sizeof(actual), "theme %s resembles none", at);
            } else {
                snprintf(actual, sizeof(actual), "no theme there (themes: %s)", themes[0] != '\0' ? themes : "none");
            }
        }
        fv1_check(v, FV1_AREA_RETURNS, location, found, expected, actual);
    }
    free(rels);
    free(entry_occ);
    free(entries);
    fv1_occurrences_destroy(&occs);
}

/* --- the Bach regression for PR #37 --- */

static void fv1_bach_minimum_window(const Chain *c) {
    /* the note-level soprano of the first strain and its written-out repeat (measures 1-16): at each attack the highest sounding note */
    ChainFormInput soprano;
    assert(chain_build_motive_input(&c->p.set, analysis_chain_segments(c->chain), 1, 16, 0, COMMON_PRACTICE_KEY_MAJOR, test_materials(), NULL, &soprano) ==
           CHAIN_INPUT_OK);
    size_t n = soprano.input.note_count;
    const NoteEvent **events = (const NoteEvent **)calloc(n > 0 ? n : 1, sizeof(NoteEvent *));
    const EntityID **ids = (const EntityID **)calloc(n > 0 ? n : 1, sizeof(EntityID *));
    int *measures_of = (int *)calloc(n > 0 ? n : 1, sizeof(int));
    assert(events != NULL && ids != NULL && measures_of != NULL);
    for (size_t k = 0; k < n; k++) {
        events[k] = soprano.input.notes[k].event;
        ids[k] = note_event_id(events[k]);
        int measure = 0;
        assert(meter_map_measure_index_at(c->map, temporal_anchor_position(note_event_temporal_anchor(events[k])), &measure) == METER_MAP_OK);
        measures_of[k] = measure;
    }
    chain_form_input_destroy(&soprano);
    assert(n > 40);

    /* every sliding window of three notes */
    size_t windows = n - 2;
    Contour **contours = (Contour **)calloc(windows, sizeof(Contour *));
    RhythmicPattern **rhythms = (RhythmicPattern **)calloc(windows, sizeof(RhythmicPattern *));
    MotifWindow *motif_windows = (MotifWindow *)calloc(windows, sizeof(MotifWindow));
    ThematicReturnDetectionEntry *entries = (ThematicReturnDetectionEntry *)calloc(windows, sizeof(*entries));
    EntityID **entry_ids = (EntityID **)calloc(windows, sizeof(EntityID *));
    assert(contours != NULL && rhythms != NULL && motif_windows != NULL && entries != NULL && entry_ids != NULL);
    for (size_t w = 0; w < windows; w++) {
        assert(fv1_window_of_events(&events[w], 3, &ids[w], &contours[w], &rhythms[w], &motif_windows[w]));
        char name[32];
        snprintf(name, sizeof(name), "bach-window-%zu", w);
        entry_ids[w] = fv1_id(name);
        entries[w].theme_occurrence_id = entry_ids[w];
        entries[w].window = &motif_windows[w];
    }
    /* the pairs that would match with no floor: same contour and same durations */
    size_t same_shape = 0;
    for (size_t i = 0; i < windows; i++) {
        for (size_t j = i + 1; j < windows; j++) {
            bool same = true;
            for (size_t k = 0; k < 2 && same; k++) {
                same = contour_direction_at(contours[i], k) == contour_direction_at(contours[j], k);
            }
            for (size_t k = 0; k < 3 && same; k++) {
                same = duration_same(*rhythmic_pattern_duration_at(rhythms[i], k), *rhythmic_pattern_duration_at(rhythms[j], k));
            }
            same_shape += same ? 1 : 0;
        }
    }
    ThematicReturnPair *rels = NULL;
    size_t rel_count = 0;
    assert(thematic_return_detection_build_pairs_configured(entries, windows, NULL, &rels, &rel_count) == THEMATIC_RETURN_DETECTION_OK);
    printf("  PR #37 regression: %zu sliding three-note windows, %zu pairs of them share contour and rhythm, %zu reported as returns\n",
           windows, same_shape, rel_count);
    assert(same_shape >= 20);
    assert(rel_count == 0);
    free(rels);
    for (size_t w = 0; w < windows; w++) {
        contour_destroy(contours[w]);
        rhythmic_pattern_destroy(rhythms[w]);
        entity_id_destroy(entry_ids[w]);
    }
    free(entry_ids);
    free(entries);
    free(motif_windows);
    free(rhythms);
    free(contours);

    /* two identical long windows (measures 1-4 and 9-12 of the written-out repeat) are matched: the floor is not a blanket block */
    size_t first_start = 0;
    size_t second_start = 0;
    while (second_start < n && measures_of[second_start] < 9) {
        second_start++;
    }
    assert(second_start > 12 && second_start < n);
    size_t length = second_start - first_start; /* the notes of measures 1-8 */
    assert(length >= 12 && second_start + length <= n);
    size_t opening = 0; /* the notes of measures 1-4: the opening idea alone */
    while (opening < n && measures_of[opening] < 5) {
        opening++;
    }
    assert(opening >= 6 && opening < length);
    free(measures_of);
    Contour *c1 = NULL, *c2 = NULL;
    RhythmicPattern *r1 = NULL, *r2 = NULL;
    MotifWindow w1, w2;
    assert(fv1_window_of_events(&events[first_start], length, &ids[first_start], &c1, &r1, &w1));
    assert(fv1_window_of_events(&events[second_start], length, &ids[second_start], &c2, &r2, &w2));
    EntityID *e1 = fv1_id("bach-first-strain");
    EntityID *e2 = fv1_id("bach-first-strain-repeat");
    ThematicReturnDetectionEntry pair[2] = {{e1, &w1}, {e2, &w2}};
    assert(thematic_return_detection_build_pairs_configured(pair, 2, NULL, &rels, &rel_count) == THEMATIC_RETURN_DETECTION_OK);
    printf("  PR #37 regression: the first strain (%zu notes) and its written-out repeat: %zu \"resembles\" pair(s)\n", length, rel_count);
    assert(rel_count == 1);
    assert(rels[0].later == 1 && rels[0].earlier == 0); /* the repeat recalls the first strain */
    free(rels);

    /* a return that restates only the opening idea (measures 1-4 against the written-out repeat, measures 9-16) is found inside the longer theme,
       at the start of it, with a perfect score; it was invisible when whole themes were compared */
    Contour *co = NULL;
    RhythmicPattern *ro = NULL;
    MotifWindow wo;
    assert(fv1_window_of_events(&events[first_start], opening, &ids[first_start], &co, &ro, &wo));
    Contour *cl = NULL;
    RhythmicPattern *rl = NULL;
    MotifWindow wl;
    assert(fv1_window_of_events(&events[second_start], n - second_start, &ids[second_start], &cl, &rl, &wl));
    EntityID *eo = fv1_id("bach-opening-idea");
    EntityID *el = fv1_id("bach-second-strain");
    ThematicReturnDetectionEntry partial[2] = {{eo, &wo}, {el, &wl}};
    assert(thematic_return_detection_build_pairs_configured(partial, 2, NULL, &rels, &rel_count) == THEMATIC_RETURN_DETECTION_OK);
    printf("  partial return: the opening idea (%zu notes) inside the repeat (%zu notes): %zu \"resembles\" pair(s)", opening, n - second_start,
           rel_count);
    assert(rel_count == 1);
    const ThematicReturnPair *evidence = &rels[0];
    printf(", similarity %.2f at offset %zu\n", evidence->similarity, evidence->later_offset);
    assert(evidence->similarity == 1.0 && evidence->earlier_offset == 0 && evidence->later_offset == 0);
    free(rels);
    entity_id_destroy(eo);
    entity_id_destroy(el);
    contour_destroy(co);
    contour_destroy(cl);
    rhythmic_pattern_destroy(ro);
    rhythmic_pattern_destroy(rl);

    entity_id_destroy(e1);
    entity_id_destroy(e2);
    contour_destroy(c1);
    contour_destroy(c2);
    rhythmic_pattern_destroy(r1);
    rhythmic_pattern_destroy(r2);
    free(ids);
    free(events);
}

int main(void) {
    static Fv1DeviationTable table;
    fv1_load_deviations(URSATZ_DEVIATIONS_FILE, &table);
    size_t failures = 0;
    FILE *summary = fopen(URSATZ_RETURNS_SUMMARY, "w");
    assert(summary != NULL);
    for (size_t i = 0; i < 5; i++) {
        const FormPiece *fp = &FORM_PIECES[i];
        bool bach = strcmp(fp->prefix, "bach114") == 0;
        bool returns = strcmp(fp->prefix, "mozart545") == 0 || strcmp(fp->prefix, "clementi36") == 0;
        if (!bach && !returns) {
            continue;
        }
        Chain c;
        fv1_chain_run(&c, fp);
        if (bach) {
            printf("bach114:\n");
            fv1_bach_minimum_window(&c);
        } else {
            Fv1Verifier v;
            memset(&v, 0, sizeof(v));
            v.table = &table;
            v.piece = c.x.piece;
            printf("%s (%zu theme(s) from L3.6):\n", c.x.piece, c.form.themes.count);
            fv1_check_returns(&c, &v);
            printf("  %-12s %zu pass, %zu deviation(s), %zu fail\n", FV1_AREA_NAMES[FV1_AREA_RETURNS], v.counts[FV1_AREA_RETURNS].pass,
                   v.counts[FV1_AREA_RETURNS].deviation, v.counts[FV1_AREA_RETURNS].fail);
            fprintf(summary, "%s returns %zu %zu\n", c.x.piece, v.counts[FV1_AREA_RETURNS].pass, v.counts[FV1_AREA_RETURNS].deviation);
            failures += v.counts[FV1_AREA_RETURNS].fail;
        }
        fv1_chain_destroy(&c);
    }
    fclose(summary);
    for (size_t i = 0; i < table.deviation_count; i++) {
        const Fv1Deviation *d = &table.deviations[i];
        if (strcmp(d->area, "returns") == 0 && !d->used) {
            printf("  FAIL %s is not used by any check (%s %s %s)\n", d->id, d->piece, d->area, d->location);
            failures++;
        }
    }
    if (failures > 0) {
        printf("framework_v1_thematic_returns: %zu failure(s)\n", failures);
        return 1;
    }
    printf("framework_v1_thematic_returns: ok\n");
    return 0;
}
