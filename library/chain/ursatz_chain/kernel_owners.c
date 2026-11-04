#include "ursatz_chain/kernel_owners.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/motive_sketch.h"
#include "analysis/motive_tone_roles.h"
#include "analysis/motive_variant_recognition.h"
#include "standard-counterpoint/analysis/analysis_melody_nct.h"
#include "standard-form/analysis/analysis_motive_match.h"

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

/* What the form matcher admits: one step, the relations the form reads. */
typedef struct {
    size_t prototype_intervals;
} AdmitContext;

static bool ratio_is(const MotiveRelationStep *step, long num, long den) {
    return (step->value[0] == 0 && step->den[0] == 0) || (step->value[0] == num && step->den[0] == den);
}

static bool only_matching_steps(const MotiveRelationStep *step, const void *context) {
    const AdmitContext *admit = (const AdmitContext *)context;
    switch (step->kind) {
    case MOTIVE_RELATION_EXACT:
    case MOTIVE_RELATION_TRANSPOSE:
    case MOTIVE_RELATION_INVERT:
    case MOTIVE_RELATION_RETROGRADE:
        return true;
    case MOTIVE_RELATION_AUGMENT:
        return ratio_is(step, 2, 1);
    case MOTIVE_RELATION_DIMINISH:
        return ratio_is(step, 1, 2);
    case MOTIVE_RELATION_FRAGMENT: {
        /* a leading or trailing part with at least the form's fewest intervals (length counts notes) */
        long position = step->value[0];
        long length = step->value[1];
        return (position == 1 || position == 2) && length >= 2 &&
               (size_t)(length - 1) >= standard_form_motive_fragment_min_intervals(admit->prototype_intervals);
    }
    default:
        return false;
    }
}

bool chain_match_motive_relation(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                 StandardFormRelationMatch *out, void *context) {
    if (prototype == NULL || candidate == NULL || out == NULL || candidate->intervals > prototype->intervals) {
        return false;
    }
    bool whole = prototype->intervals == candidate->intervals;
    MotiveSketch source;
    MotiveSketch target;
    if (!sketch_of(prototype, (const ChainRelationContext *)context, &source) ||
        !sketch_of(candidate, (const ChainRelationContext *)context, &target)) {
        return false;
    }
    AdmitContext admit = {prototype->intervals};
    MotiveRecognizeOptions options;
    motive_recognize_options_default(&options);
    options.max_depth = 1;
    options.admit = only_matching_steps;
    options.admit_context = &admit;
    MotiveRecognition recognition;
    if (motive_variant_recognize(&source, &target, &options, &recognition) != MOTIVE_RECOGNIZE_OK || !recognition.found ||
        recognition.step_count != 1) {
        return false;
    }
    const MotiveRelationStep *step = &recognition.steps[0];
    out->fragment_part = -1;
    out->fragment_intervals = 0;
    if (whole != (step->kind != MOTIVE_RELATION_FRAGMENT)) {
        return false; /* only a fragment is shorter, and a fragment is shorter */
    }
    switch (step->kind) {
    case MOTIVE_RELATION_EXACT:
        out->kind = MOTIVE_OCCURRENCE_EXACT;
        break;
    case MOTIVE_RELATION_TRANSPOSE:
        out->kind = step->value[0] != 0 ? MOTIVE_OCCURRENCE_TONAL : MOTIVE_OCCURRENCE_REAL;
        break;
    case MOTIVE_RELATION_INVERT:
        out->kind = MOTIVE_OCCURRENCE_INVERTED;
        break;
    case MOTIVE_RELATION_RETROGRADE:
        out->kind = MOTIVE_OCCURRENCE_RETROGRADE;
        break;
    case MOTIVE_RELATION_AUGMENT:
        out->kind = MOTIVE_OCCURRENCE_AUGMENTED;
        break;
    case MOTIVE_RELATION_DIMINISH:
        out->kind = MOTIVE_OCCURRENCE_DIMINISHED;
        break;
    case MOTIVE_RELATION_FRAGMENT:
        out->kind = MOTIVE_OCCURRENCE_FRAGMENTED;
        out->fragment_part = (int)step->value[0] - 1;
        out->fragment_intervals = (size_t)(step->value[1] - 1);
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
    out->score = motive_variant_nearness(&dimensions);
    return out->score > 0.0;
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
