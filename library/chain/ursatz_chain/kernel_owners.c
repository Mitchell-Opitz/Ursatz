#include "ursatz_chain/kernel_owners.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/motive_sketch.h"
#include "analysis/motive_tone_roles.h"
#include "analysis/motive_variant_recognition.h"
#include "analysis_melody_nct.h"
#include "analysis_motive_shape.h"

bool chain_classify_tones(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context) {
    return motive_tone_roles_line(notes, count, out, context);
}

static Rational gap_of(StandardFormQ q) {
    return msk_make((unsigned long long)q.n, (unsigned long long)q.d);
}

/* The sketch a shape stands for; false when it is not a valid one. */
static bool sketch_of(const StandardFormMotiveShape *shape, const ChainRelationContext *context, MotiveSketch *out) {
    if (shape == NULL || shape->intervals == 0 || shape->intervals + 1 > MOTIVE_SKETCH_MAX_NOTES) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    int pitch = (int)shape->first_semitone;
    Rational onset = msk_int(0);
    out->note_count = shape->intervals + 1;
    for (size_t k = 0; k <= shape->intervals; k++) {
        Rational gap = gap_of(shape->ioi[k < shape->intervals ? k : shape->intervals - 1]);
        out->notes[k].pitch = pitch;
        out->notes[k].onset = onset;
        out->notes[k].duration = gap;
        onset = msk_add(onset, gap);
        if (k < shape->intervals) {
            pitch += shape->semitones[k];
        }
    }
    out->length = onset;
    out->bar = msk_int(1);
    out->beat = msk_make(1, 4);
    out->pickup = msk_int(0);
    if (shape->generic && context != NULL && context->has_key) {
        out->has_scale = true;
        out->tonic_pc = context->tonic_pitch_class;
        out->minor = context->minor;
        out->key_source = MOTIVE_SKETCH_KEY_GIVEN;
    }
    return motive_sketch_validate(out);
}

static bool only_matching_steps(const MotiveRelationStep *step, const void *context) {
    (void)context;
    return step->kind == MOTIVE_RELATION_EXACT || step->kind == MOTIVE_RELATION_TRANSPOSE ||
           step->kind == MOTIVE_RELATION_INVERT || step->kind == MOTIVE_RELATION_RETROGRADE;
}

bool chain_match_motive_relation(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                 StandardFormMotiveKind *kind, double *score, void *context) {
    if (prototype == NULL || candidate == NULL || kind == NULL || score == NULL ||
        prototype->intervals != candidate->intervals) {
        return false;
    }
    MotiveSketch source;
    MotiveSketch target;
    if (!sketch_of(prototype, (const ChainRelationContext *)context, &source) ||
        !sketch_of(candidate, (const ChainRelationContext *)context, &target)) {
        return false;
    }
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.max_depth = 1;
    options.admit = only_matching_steps;
    MotiveRecognition recognition;
    if (motive_variant_recognize(&source, &target, &options, &recognition) != MOTIVE_RECOGNIZE_OK || !recognition.found ||
        recognition.step_count != 1) {
        return false;
    }
    const MotiveRelationStep *step = &recognition.steps[0];
    switch (step->kind) {
    case MOTIVE_RELATION_EXACT:
        *kind = STANDARD_FORM_MOTIVE_EXACT;
        break;
    case MOTIVE_RELATION_TRANSPOSE:
        *kind = step->value[0] != 0 ? STANDARD_FORM_MOTIVE_TONAL : STANDARD_FORM_MOTIVE_REAL;
        break;
    case MOTIVE_RELATION_INVERT:
        *kind = STANDARD_FORM_MOTIVE_INVERTED;
        break;
    case MOTIVE_RELATION_RETROGRADE:
        *kind = STANDARD_FORM_MOTIVE_RETROGRADE;
        break;
    default:
        return false;
    }
    MotiveDimensions dimensions;
    for (int d = 0; d < MOTIVE_DIMENSION_COUNT; d++) {
        dimensions.similarity[d] = recognition.similarity[d];
        dimensions.kept[d] = recognition.kept[d];
        dimensions.changed[d] = recognition.changed[d];
    }
    *score = motive_variant_nearness(&dimensions);
    return *score > 0.0;
}

bool chain_relabel_melody_line(ChainMelodyLine *melody, const CommonPracticeWeightedResult *results, size_t segment_count,
                               NonChordToneClassifier classifier, void *context) {
    if (melody == NULL || classifier == NULL || (segment_count > 0 && results == NULL)) {
        return false;
    }
    StandardCounterpointMelodyLine *line = &melody->line;
    unsigned *saved = (unsigned *)calloc(line->count > 0 ? line->count : 1, sizeof(unsigned));
    if (saved == NULL) {
        return false;
    }
    for (size_t i = 0; i < line->count; i++) {
        size_t s = melody->segment[i];
        saved[i] = line->notes[i].chord_pitch_classes;
        line->notes[i].chord_pitch_classes =
            line->notes[i].present && s < segment_count && results[s].matched
                ? common_practice_ranked_chord_mask(&results[s].best, 0)
                : 0u;
    }
    StandardCounterpointNctLabels labels;
    bool ok = standard_counterpoint_melody_nct_labels_with(line, classifier, context, &labels) ==
              STANDARD_COUNTERPOINT_MELODY_OK;
    if (ok) {
        for (size_t k = 0; k < labels.count; k++) {
            StandardCounterpointMelodyNote *note = &line->notes[labels.items[k].claim.index];
            note->nct = (StandardCounterpointNctStatus)labels.items[k].claim.status;
            if (note->nct == STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE) {
                note->nct_type = (NonChordTone)labels.items[k].claim.type;
            }
        }
        standard_counterpoint_nct_labels_destroy(&labels);
    }
    for (size_t i = 0; i < line->count; i++) {
        line->notes[i].chord_pitch_classes = saved[i];
    }
    free(saved);
    return ok;
}
