#include "analysis_motive.h"
#include "motive_util.h"

/*
 * The motive families of Mozart 545, the exposition (measures 1-28), against tests/fixtures/annotations/mozart545_motives.txt: a DRAFT hand
 * reading, not yet checked against the score (see its header), made from the pitch content of the file and not from this code's output.
 *
 * The motive adapter runs on a note-level soprano (the highest sounding note at each attack, motive_util.h) with its scale degrees, the
 * phrases the form adapter finds from the annotated phrase end of tests/fixtures/annotations/mozart545_phrase_form.txt, and the themes,
 * with the library's defaults and the kernel's window matching injected (form_kernel_bridge.h).
 *
 * A FAMILY is found when the adapter has a family with an occurrence starting within one beat of the annotated family's first occurrence.
 * An OCCURRENCE is found, with the right kind, when the adapter has an occurrence within one beat of it of the annotated kind (exact, real
 * or tonal; the adapter's prototype of a family is exact). Asserted: every annotated family is found, and at least 75% of the annotated
 * occurrences are found with the right kind. Reported: the families the adapter finds that match no annotation, and the misses.
 */

#define TARGET_OCCURRENCES 0.75

typedef struct {
    int measure;
    double beat;
    StandardFormMotiveKind kind;
} AnnotatedOccurrence;

typedef struct {
    char name[48];
    AnnotatedOccurrence occurrences[16];
    size_t occurrence_count;
} AnnotatedFamily;

typedef struct {
    int range_first;
    int range_last;
    AnnotatedFamily families[16];
    size_t family_count;
} MotiveAnnotation;

static MotiveAnnotation load_annotation_file(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/mozart545_motives.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    MotiveAnnotation a;
    memset(&a, 0, sizeof(a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[48];
        if (sscanf(line, "%47s", word) != 1) {
            continue;
        }
        if (strcmp(word, "range") == 0) {
            assert(sscanf(line, "range %d %d", &a.range_first, &a.range_last) == 2);
        } else if (strcmp(word, "family") == 0) {
            AnnotatedFamily *f = &a.families[a.family_count++];
            assert(sscanf(line, "family %47s", f->name) == 1);
        } else if (strcmp(word, "occurrence") == 0) {
            assert(a.family_count > 0);
            AnnotatedFamily *f = &a.families[a.family_count - 1];
            AnnotatedOccurrence *o = &f->occurrences[f->occurrence_count++];
            char kind[16];
            assert(sscanf(line, "occurrence %d %lf %15s", &o->measure, &o->beat, kind) == 3);
            o->kind = strcmp(kind, "exact") == 0 ? STANDARD_FORM_MOTIVE_EXACT
                                                 : (strcmp(kind, "real") == 0 ? STANDARD_FORM_MOTIVE_REAL : STANDARD_FORM_MOTIVE_TONAL);
        }
    }
    free(text);
    assert(a.range_first > 0 && a.family_count > 0);
    return a;
}

static double beats_of(int measure, double beat) {
    return (double)(measure - 1) * 4.0 + (beat - 1.0);
}

static double distance(double a, double b) {
    return a > b ? a - b : b - a;
}

/* The cadences of the form annotation: its phrase-end lines, each arriving in the whole measure. */
static void add_phrase_ends(FormBuild *build) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/mozart545_phrase_form.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        int measure = 0;
        char type[8];
        if (strncmp(line, "phrase-end ", 11) != 0 || sscanf(line, "phrase-end %d %7s", &measure, type) != 2) {
            continue;
        }
        CadenceType cadence = strcmp(type, "pac") == 0 ? CADENCE_TYPE_PERFECT_AUTHENTIC
                                                       : (strcmp(type, "iac") == 0 ? CADENCE_TYPE_IMPERFECT_AUTHENTIC : CADENCE_TYPE_HALF);
        TimeSpan *span = NULL;
        assert(meter_map_measure_span(build->input.map, measure, &span) == METER_MAP_OK);
        char id[32];
        snprintf(id, sizeof(id), "hand-%d", measure);
        form_build_cadence(build, id, time_span_start(span), time_span_end(span), cadence, harmony_cadence_strength(cadence), false, 0.0);
        time_span_destroy(span);
    }
    free(text);
}

static void test_mozart_545(void) {
    MotiveAnnotation annotation = load_annotation_file();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Mozart 545.mid", NULL);
    const MeterMap *map = p.meter.meter_map;

    FormBuild build;
    form_build_init(&build, map);
    motive_build_soprano(&build, &p, annotation.range_first, annotation.range_last, 0, COMMON_PRACTICE_KEY_MAJOR);
    add_phrase_ends(&build);
    assert(standard_form_input_finish(&build.input) == STANDARD_FORM_ANALYSIS_OK);
    printf("Mozart 545, measures %d-%d: %zu soprano notes\n", annotation.range_first, annotation.range_last, build.input.note_count);

    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    StandardFormThemes themes;
    StandardFormMotives motives;
    assert(standard_form_segment_phrases(&build.input, NULL, form_kernel_detect, NULL, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&build.input, &phrases, NULL, &segmentation) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_identify_themes(&build.input, &phrases, &segmentation, NULL, &themes) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_find_motives(&build.input, &phrases, &segmentation, &themes, NULL, form_kernel_match_windows, NULL,
                                      &motives) == STANDARD_FORM_ANALYSIS_OK);
    printf("  the adapter found %zu motive famil%s\n", motives.family_count, motives.family_count == 1 ? "y" : "ies");

    size_t families_found = 0;
    size_t occurrences = 0;
    size_t occurrences_right = 0;
    for (size_t a = 0; a < annotation.family_count; a++) {
        const AnnotatedFamily *want = &annotation.families[a];
        double first = beats_of(want->occurrences[0].measure, want->occurrences[0].beat);
        bool found = false;
        size_t right = 0;
        for (size_t o = 0; o < want->occurrence_count; o++) {
            double at = beats_of(want->occurrences[o].measure, want->occurrences[o].beat);
            bool occurrence_found = false;
            bool occurrence_right = false;
            for (size_t f = 0; f < motives.family_count; f++) {
                for (size_t k = 0; k < motives.families[f].occurrence_count; k++) {
                    const StandardFormMotiveOccurrenceClaim *c = &motives.families[f].occurrences[k].claim;
                    if (c->line != 0 || distance(beats_of(c->first_measure, c->first_beat), at) > 1.0) {
                        continue;
                    }
                    occurrence_found = true;
                    /* the adapter's prototype is exact; an occurrence read against the annotation's own prototype matches by kind */
                    occurrence_right = occurrence_right || c->kind == (int)want->occurrences[o].kind ||
                                       (c->kind == (int)STANDARD_FORM_MOTIVE_EXACT && want->occurrences[o].kind == STANDARD_FORM_MOTIVE_EXACT);
                    if (o == 0) {
                        found = true;
                    }
                }
            }
            occurrences++;
            occurrences_right += occurrence_right ? 1 : 0;
            right += occurrence_right ? 1 : 0;
            if (!occurrence_right) {
                printf("    %s: occurrence at measure %d beat %.2f (%s): %s\n", want->name, want->occurrences[o].measure,
                       want->occurrences[o].beat, standard_form_motive_kind_name(want->occurrences[o].kind),
                       occurrence_found ? "found with another kind" : "not found");
            }
        }
        families_found += found ? 1 : 0;
        printf("  %s: %s, %zu of %zu occurrences found with the right kind (first at measure %d beat %.2f)\n", want->name,
               found ? "found" : "NOT FOUND", right, want->occurrence_count, want->occurrences[0].measure, want->occurrences[0].beat);
    }

    size_t extra = 0;
    for (size_t f = 0; f < motives.family_count; f++) {
        bool annotated = false;
        for (size_t k = 0; k < motives.families[f].occurrence_count && !annotated; k++) {
            const StandardFormMotiveOccurrenceClaim *c = &motives.families[f].occurrences[k].claim;
            for (size_t a = 0; a < annotation.family_count; a++) {
                for (size_t o = 0; o < annotation.families[a].occurrence_count; o++) {
                    annotated = annotated || distance(beats_of(c->first_measure, c->first_beat),
                                                      beats_of(annotation.families[a].occurrences[o].measure,
                                                               annotation.families[a].occurrences[o].beat)) <= 1.0;
                }
            }
        }
        if (!annotated) {
            extra++;
            const StandardFormMotiveClaim *claim = &motives.families[f].claim;
            const StandardFormMotiveOccurrenceClaim *first = &motives.families[f].occurrences[0].claim;
            printf("    found, not annotated: %d notes, %d occurrence(s), first at measure %d beat %.2f%s\n", claim->note_count,
                   claim->occurrence_count, first->first_measure, first->first_beat, claim->generic ? " (generic)" : "");
        }
    }
    double rate = (double)occurrences_right / (double)occurrences;
    printf("  families found %zu of %zu, occurrences found with the right kind %zu of %zu (%.1f%%), %zu famil%s found that %s not annotated\n",
           families_found, annotation.family_count, occurrences_right, occurrences, 100.0 * rate, extra, extra == 1 ? "y" : "ies",
           extra == 1 ? "is" : "are");
    assert(families_found == annotation.family_count);
    assert(rate >= TARGET_OCCURRENCES);

    standard_form_motives_destroy(&motives);
    standard_form_themes_destroy(&themes);
    standard_form_segmentation_destroy(&segmentation);
    standard_form_phrases_destroy(&phrases);
    form_build_destroy(&build);
    pipeline_destroy(&p);
}

int main(void) {
    test_mozart_545();
    printf("test_motive_mozart545: ok\n");
    return 0;
}
