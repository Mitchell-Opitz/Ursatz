#include "analysis/motive_link.h"

#include <string.h>

void motive_link_options_default(MotiveLinkOptions *options) {
    options->max_depth = 2;
    options->beam = 64;
    options->min_nearness = 0.5;
    options->contour = false;
}

double motive_link_nearness(const MotiveSketch *a, const MotiveSketch *b) {
    MotiveDimensions dimensions;
    motive_variant_compare(a, b, &dimensions);
    return motive_variant_nearness(&dimensions);
}

static MotiveLinkResult from_recognize(MotiveRecognizeResult result) {
    return result == MOTIVE_RECOGNIZE_NO_MEMORY ? MOTIVE_LINK_NO_MEMORY : MOTIVE_LINK_INVALID;
}

MotiveLinkResult motive_link_pair(const MotiveSketch *a, const MotiveSketch *b, const MotiveLinkOptions *options,
                                  MotiveLink *out) {
    MotiveLinkOptions defaults;
    motive_link_options_default(&defaults);
    if (!options) {
        options = &defaults;
    }
    if (!out || !motive_sketch_validate(a) || !motive_sketch_validate(b) || options->max_depth < 1 ||
        options->max_depth > MOTIVE_RECOGNIZE_MAX_DEPTH || options->beam < 1 ||
        options->beam > MOTIVE_RECOGNIZE_MAX_BEAM || !(options->min_nearness >= 0.0) ||
        !(options->min_nearness <= 1.0)) {
        return MOTIVE_LINK_INVALID;
    }
    MotiveLink link;
    memset(&link, 0, sizeof(link));
    link.nearness = motive_link_nearness(a, b);
    if (link.nearness < options->min_nearness) {
        link.gated = true;
        *out = link;
        return MOTIVE_LINK_OK;
    }
    MotiveRecognizeOptions search;
    motive_recognize_options_default(&search);
    search.max_depth = options->max_depth;
    search.beam = options->beam;
    search.contour = options->contour;
    MotiveRecognizeResult result = motive_variant_recognize(a, b, &search, &link.recognition);
    if (result != MOTIVE_RECOGNIZE_OK) {
        return from_recognize(result);
    }
    if (link.recognition.found) {
        link.linked = true;
        *out = link;
        return MOTIVE_LINK_OK;
    }
    MotiveRecognition back;
    result = motive_variant_recognize(b, a, &search, &back);
    if (result != MOTIVE_RECOGNIZE_OK) {
        return from_recognize(result);
    }
    if (back.found) {
        link.linked = true;
        link.reversed = true;
        link.recognition = back;
    }
    *out = link;
    return MOTIVE_LINK_OK;
}
