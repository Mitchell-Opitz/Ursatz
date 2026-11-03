#include <ursatz_composer/motive_connect.h>

#include <string.h>

#include <standard-harmony/harmonic_function.h>


#include "motive/motive_internal.h"

/* A score from small constants or counts, which always fit. Values from a caller go through motive_score_make. */
static MotiveScore fraction(unsigned long long numerator, unsigned long long denominator) {
    MotiveScore score = {0, 1};
    motive_score_make(numerator, denominator, &score);
    return score;
}

bool motive_chain_distance(const VariantChain *chain, MotiveScore *out) {
    if (!chain || !out) {
        return false;
    }
    unsigned long long cost = 0;
    for (size_t s = 0; s < chain->step_count && s < VARIANT_CHAIN_MAX; s++) {
        const VariantOperation *operation = variant_registry_find(chain->steps[s].op);
        if (!operation) {
            return false;
        }
        cost += (unsigned long long)motive_relation_cost((MotiveRelationKind)operation->relation);
    }
    *out = fraction(cost, cost + 4);
    return true;
}

/* The first or last chord of the motive: its own harmony, else its best implied chord. False when it has none. */
static bool chord_of(const Motive *motive, bool last, int *degree, int *alteration) {
    if (motive->chord_count > 0) {
        const MotiveSketchChord *chord = &motive->chords[last ? motive->chord_count - 1 : 0];
        *degree = chord->degree;
        *alteration = chord->alteration;
        return true;
    }
    MotiveSketchChord implied;
    if (!motive_implied_chord(motive, &implied)) {
        return false;
    }
    *degree = implied.degree;
    *alteration = implied.alteration;
    return true;
}

/* The function of a scale degree's chord, ignoring its mode: the motive sketch carries no mode to ask the framework. */
static HarmonicFunction degree_function(int degree) {
    switch (degree) {
    case 2:
    case 4:
        return HARMONIC_FUNCTION_PREDOMINANT;
    case 5:
    case 7:
        return HARMONIC_FUNCTION_DOMINANT;
    default:
        return HARMONIC_FUNCTION_TONIC;
    }
}

/* 1, 1/2 or 0: the idea before's last chord to the candidate's first. */
static MotiveScore coherence(const Motive *previous, const Motive *candidate) {
    int from;
    int from_alteration;
    int to;
    int to_alteration;
    if (!chord_of(previous, true, &from, &from_alteration) || !chord_of(candidate, false, &to, &to_alteration)) {
        return fraction(0, 1);
    }
    if (from == to && from_alteration == to_alteration) {
        return fraction(1, 2);
    }
    HarmonicFunction a = degree_function(from);
    HarmonicFunction b = degree_function(to);
    bool allowed = (a == HARMONIC_FUNCTION_TONIC) || (a == HARMONIC_FUNCTION_PREDOMINANT && b == HARMONIC_FUNCTION_DOMINANT) ||
                   (a == HARMONIC_FUNCTION_DOMINANT && b == HARMONIC_FUNCTION_TONIC);
    return allowed ? fraction(1, 1) : fraction(0, 1);
}

bool motive_connection(const Motive *basic, const Motive *previous, const Motive *candidate, const MotiveScore *distance,
                       MotiveConnection *out) {
    return motive_connection_limited(basic, previous, candidate, distance, NULL, out);
}

bool motive_connection_limited(const Motive *basic, const Motive *previous, const Motive *candidate,
                               const MotiveScore *distance, const MotiveScore *limit, MotiveConnection *out) {
    if (!basic || !candidate || !out || basic->note_count == 0 || candidate->note_count == 0) {
        return false;
    }
    if (!previous) {
        previous = basic;
    }
    MotiveScore remote;
    if (distance) {
        if (distance->denominator == 0) {
            remote = fraction(1, 1);
        } else if (!motive_score_make(distance->numerator, distance->denominator, &remote)) {
            return false;
        }
    } else {
        VariantRecord record;
        memset(&record, 0, sizeof(record));
        if (variant_set_remoteness(basic, candidate, VARIANT_CHAIN_MAX, &record) != VARIANT_OK) {
            return false;
        }
        if (!motive_score_make(record.remoteness_num, record.remoteness_den, &remote)) {
            return false;
        }
    }
    MotiveRecognizability recognizability;
    if (!motive_recognizability(previous, candidate, NULL, &recognizability)) {
        return false;
    }
    MotiveConnection connection;
    memset(&connection, 0, sizeof(connection));
    connection.remoteness = remote;
    connection.limit = fraction(3, 4);
    if (limit && limit->denominator > 0 && !motive_score_make(limit->numerator, limit->denominator, &connection.limit)) {
        return false;
    }
    if (remote.numerator > remote.denominator ||
        !motive_score_make(remote.denominator - remote.numerator, remote.denominator, &connection.common)) {
        return false;
    }
    connection.rhythm = recognizability.rhythm;
    connection.harmony = coherence(previous, candidate);

    /* (2c + r + h) / 4, exact; halved when the remoteness is over the limit. */
    connection.halved = rational_cmp(motive_score_rational(remote), motive_score_rational(connection.limit)) > 0;
    Rational sum;
    Rational total;
    if (rational_mul_int(motive_score_rational(connection.common), 2, &sum) != RATIONAL_OK ||
        rational_add(sum, motive_score_rational(connection.rhythm), &sum) != RATIONAL_OK ||
        rational_add(sum, motive_score_rational(connection.harmony), &sum) != RATIONAL_OK ||
        rational_div_int(sum, connection.halved ? 8 : 4, &total) != RATIONAL_OK ||
        !motive_score_from_rational(total, &connection.total)) {
        return false;
    }
    *out = connection;
    return true;
}

bool motive_connection_score(const Motive *basic, const Motive *previous, const Motive *candidate, MotiveScore *out) {
    MotiveConnection connection;
    if (!out || !motive_connection(basic, previous, candidate, NULL, &connection)) {
        return false;
    }
    *out = connection.total;
    return true;
}
