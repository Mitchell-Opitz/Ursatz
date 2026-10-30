#include "pipeline/motive_features.h"

#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/style_labels.h>

const char *motive_features_degree_label(int pitch_class, int tonic_pc) {
    return style_label_degree(pitch_class, tonic_pc);
}

static Rational fraction(unsigned long long numerator, unsigned long long denominator) {
    return msk_make(numerator, denominator);
}

bool motive_features_sketch(const AnalysisMotiveFeatures *features, MotiveSketch *sketch) {
    memset(sketch, 0, sizeof(*sketch));
    if (!features || features->note_count == 0 || features->note_count > MOTIVE_SKETCH_MAX_NOTES ||
        !features->notes) {
        return false;
    }
    for (size_t i = 0; i < features->note_count; i++) {
        const AnalysisMotiveNote *note = &features->notes[i];
        MotiveSketchNote *out = &sketch->notes[i];
        out->pitch = note->pitch;
        out->onset = fraction(note->onset_numerator, note->onset_denominator);
        out->duration = fraction(note->duration_numerator, note->duration_denominator);
    }
    sketch->note_count = features->note_count;
    for (size_t i = 0; i + 1 < sketch->note_count; i++) {
        Rational gap = msk_sub(sketch->notes[i + 1].onset, sketch->notes[i].onset);
        if (msk_ok(gap) && msk_cmp(sketch->notes[i].duration, gap) > 0) {
            sketch->notes[i].duration = gap;
        }
    }
    const MotiveSketchNote *last = &sketch->notes[sketch->note_count - 1];
    sketch->has_scale = true;
    sketch->tonic_pc = features->tonic_pc;
    sketch->minor = features->minor;
    sketch->bar = fraction(features->bar_numerator, features->bar_denominator);
    sketch->beat = fraction(features->beat_numerator, features->beat_denominator);
    Rational position = fraction(features->pickup_numerator, features->pickup_denominator);
    sketch->pickup = msk_is_zero(position) ? position : msk_sub(sketch->bar, position);
    sketch->length = msk_add(last->onset, last->duration);
    if (!motive_sketch_validate(sketch)) {
        memset(sketch, 0, sizeof(*sketch));
        return false;
    }
    return true;
}

void motive_features_destroy(AnalysisMotiveFeatures *features) {
    if (!features) {
        return;
    }
    if (features->ncts) {
        for (size_t i = 0; i < features->note_count; i++) {
            free(features->ncts[i]);
        }
    }
    free(features->ncts);
    free(features->notes);
    free(features->degrees);
    free(features->intervals);
    free(features->steps);
    memset(features, 0, sizeof(*features));
}
