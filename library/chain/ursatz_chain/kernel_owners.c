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

static bool ratio_is(const MotiveRelationStep *step, long num, long den) {
    return (step->value[0] == 0 && step->den[0] == 0) || (step->value[0] == num && step->den[0] == den);
}

/* What the form reads of a candidate as long as its prototype: one step. */
static bool only_matching_steps(const MotiveRelationStep *step, const void *context) {
    (void)context;
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
    default:
        return false;
    }
}

static double score_of(const MotiveSketch *from, const MotiveSketch *to) {
    MotiveDimensions dimensions;
    motive_variant_compare(from, to, &dimensions);
    return motive_variant_nearness(&dimensions);
}

/*
 * A candidate shorter than the prototype is a fragment when the kernel's fragment relation, applied to the prototype as a leading (1) or
 * trailing (2) part of the candidate's length, gives the candidate. The relation is applied directly, not searched for: the form asks
 * this of every window of every length, and the recognizer's search over every relation is far too slow for that. The sketches are
 * settled the way the recognizer settles them (an exact step), so the score is the one it would report.
 */
static bool fragment_of(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate, const MotiveSketch *source,
                        const MotiveSketch *target, StandardFormRelationMatch *out) {
    size_t length = candidate->intervals + 1; /* notes */
    if (candidate->intervals < standard_form_motive_fragment_min_intervals(prototype->intervals)) {
        return false;
    }
    MotiveRelationStep exact = {MOTIVE_RELATION_EXACT, {0}, {0}};
    MotiveSketch from;
    MotiveSketch to;
    if (motive_relation_apply(source, &exact, &from) != MOTIVE_RELATION_OK ||
        motive_relation_apply(target, &exact, &to) != MOTIVE_RELATION_OK) {
        return false;
    }
    for (int part = 0; part < 2; part++) {
        MotiveRelationStep step = {MOTIVE_RELATION_FRAGMENT, {part + 1, (long)length, 0}, {0}};
        MotiveSketch cut;
        if (motive_relation_apply(&from, &step, &cut) == MOTIVE_RELATION_OK && motive_sketch_equal(&cut, &to)) {
            out->kind = MOTIVE_OCCURRENCE_FRAGMENTED;
            out->fragment_part = part;
            out->fragment_intervals = candidate->intervals;
            out->score = score_of(&from, &to);
            return out->score > 0.0;
        }
    }
    return false;
}

bool chain_match_motive_relation(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                 StandardFormRelationMatch *out, void *context) {
    if (prototype == NULL || candidate == NULL || out == NULL || candidate->intervals > prototype->intervals) {
        return false;
    }
    MotiveSketch source;
    MotiveSketch target;
    if (!sketch_of(prototype, (const ChainRelationContext *)context, &source) ||
        !sketch_of(candidate, (const ChainRelationContext *)context, &target)) {
        return false;
    }
    out->fragment_part = -1;
    out->fragment_intervals = 0;
    if (candidate->intervals < prototype->intervals) {
        return fragment_of(prototype, candidate, &source, &target, out);
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
