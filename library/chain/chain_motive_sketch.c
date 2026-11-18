#include "chain/chain_motive_sketch.h"

#include <string.h>

static Rational fraction(Rational value) {
    return msk_make((unsigned long long)value.num, (unsigned long long)value.den);
}

bool motive_features_to_sketch(const AnalysisMotiveFeatures *features, MotiveSketch *sketch) {
    memset(sketch, 0, sizeof(*sketch));
    if (!features || features->note_count == 0 || features->note_count > MOTIVE_SKETCH_MAX_NOTES ||
        !features->notes) {
        return false;
    }
    for (size_t i = 0; i < features->note_count; i++) {
        const AnalysisMotiveNote *note = &features->notes[i];
        MotiveSketchNote *out = &sketch->notes[i];
        out->pitch = note->pitch;
        out->onset = fraction(note->onset);
        out->duration = fraction(note->duration);
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
    sketch->material = features->material;
    sketch->bar = fraction(features->bar);
    sketch->beat = fraction(features->beat);
    Rational position = fraction(features->pickup);
    sketch->pickup = msk_is_zero(position) ? position : msk_sub(sketch->bar, position);
    sketch->length = msk_add(last->onset, last->duration);
    if (!motive_sketch_validate(sketch)) {
        memset(sketch, 0, sizeof(*sketch));
        return false;
    }
    return true;
}
