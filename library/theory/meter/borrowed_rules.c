#include "theory/meter/borrowed_rules.h"

#include <stddef.h>

#include "model/core/rational.h"

static bool is_triplet(const Tuplet *tuplet) {
    return tuplet != NULL && tuplet_actual(tuplet) == 3 && tuplet_normal(tuplet) == 2;
}

static bool is_duplet(const Tuplet *tuplet) {
    return tuplet != NULL && tuplet_actual(tuplet) == 2 && tuplet_normal(tuplet) == 3;
}

bool standard_meter_tuplet_borrowed(const Tuplet *tuplet, MeterType type) {
    return (type == METER_TYPE_SIMPLE && is_triplet(tuplet)) || (type == METER_TYPE_COMPOUND && is_duplet(tuplet));
}

bool standard_meter_tuplet_level(const TimeSignature *time_signature, const Duration *span, TupletLevel *out) {
    if (time_signature == NULL || span == NULL || span->value.num == 0) {
        return false;
    }
    Duration beat;
    if (!standard_meter_beat_duration(time_signature, &beat)) {
        return false;
    }

    /* span / beat as a checked, reduced Rational: overflow is a refusal, never a wrapped product. */
    Rational ratio;
    if (rational_div(span->value, beat.value, &ratio) != RATIONAL_OK) {
        return false;
    }

    bool ok = true;
    if (ratio.num == ratio.den) {
        *out = TUPLET_LEVEL_BEAT;
    } else if (ratio.num > ratio.den && ratio.den == 1) {
        *out = TUPLET_LEVEL_MULTI_BEAT;
    } else if (ratio.num < ratio.den && ratio.num == 1) {
        *out = TUPLET_LEVEL_SUBDIVISION; /* the beat is a whole number of spans */
    } else {
        ok = false;
    }
    return ok;
}

bool standard_meter_tuplet_level_typical(const Tuplet *tuplet, TupletLevel level) {
    if (is_triplet(tuplet)) {
        return true;
    }
    if (is_duplet(tuplet)) {
        return level == TUPLET_LEVEL_BEAT || level == TUPLET_LEVEL_SUBDIVISION;
    }
    return false;
}
