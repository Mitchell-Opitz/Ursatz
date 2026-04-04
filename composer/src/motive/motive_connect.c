#include <ursatz_composer/motive_connect.h>

#include <string.h>

#include <ursatz_composer/motive_sketch_bridge.h>

typedef enum {
    FUNCTION_TONIC,
    FUNCTION_PREDOMINANT,
    FUNCTION_DOMINANT
} Function;

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

static MotiveScore fraction(unsigned long long numerator, unsigned long long denominator) {
    MotiveScore score;
    unsigned long long divisor = gcd_ull(numerator, denominator);
    if (divisor == 0) {
        divisor = 1;
    }
    score.numerator = (unsigned long)(numerator / divisor);
    score.denominator = (unsigned long)(denominator / divisor);
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
    if (motive->harmony_count > 0) {
        const MotiveChord *chord = &motive->harmony[last ? motive->harmony_count - 1 : 0];
        *degree = chord->degree;
        *alteration = chord->alteration;
        return true;
    }
    MotiveSketch sketch;
    if (!motive_to_sketch(motive, &sketch) || sketch.chord_count == 0) {
        return false;
    }
    *degree = sketch.chords[0].degree;
    *alteration = sketch.chords[0].alteration;
    return true;
}

static Function function_of(int degree) {
    switch (degree) {
    case 2:
    case 4:
        return FUNCTION_PREDOMINANT;
    case 5:
    case 7:
        return FUNCTION_DOMINANT;
    default:
        return FUNCTION_TONIC;
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
    Function a = function_of(from);
    Function b = function_of(to);
    bool allowed = (a == FUNCTION_TONIC) || (a == FUNCTION_PREDOMINANT && b == FUNCTION_DOMINANT) ||
                   (a == FUNCTION_DOMINANT && b == FUNCTION_TONIC);
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
        remote = distance->denominator == 0 ? fraction(1, 1) : fraction(distance->numerator, distance->denominator);
    } else {
        VariantRecord record;
        memset(&record, 0, sizeof(record));
        if (variant_set_remoteness(basic, candidate, VARIANT_CHAIN_MAX, &record) != VARIANT_OK) {
            return false;
        }
        remote = fraction(record.remoteness_num, record.remoteness_den);
    }
    MotiveRecognizability recognizability;
    if (!motive_recognizability(previous, candidate, NULL, &recognizability)) {
        return false;
    }
    MotiveConnection connection;
    memset(&connection, 0, sizeof(connection));
    connection.remoteness = remote;
    connection.limit = limit && limit->denominator > 0 ? fraction(limit->numerator, limit->denominator) : fraction(3, 4);
    connection.common = fraction(remote.denominator - remote.numerator, remote.denominator);
    connection.rhythm = recognizability.rhythm;
    connection.harmony = coherence(previous, candidate);

    /* (2c + r + h) / 4 over the common denominator of the three parts. */
    unsigned long long den = (unsigned long long)connection.common.denominator * connection.rhythm.denominator /
                             gcd_ull(connection.common.denominator, connection.rhythm.denominator);
    den = den * connection.harmony.denominator / gcd_ull(den, connection.harmony.denominator);
    unsigned long long num = 2ULL * connection.common.numerator * (den / connection.common.denominator) +
                             (unsigned long long)connection.rhythm.numerator * (den / connection.rhythm.denominator) +
                             (unsigned long long)connection.harmony.numerator * (den / connection.harmony.denominator);
    den *= 4;
    /* Remoteness over the limit halves the score. */
    connection.halved = (unsigned long long)remote.numerator * connection.limit.denominator >
                        (unsigned long long)remote.denominator * connection.limit.numerator;
    if (connection.halved) {
        den *= 2;
    }
    connection.total = fraction(num, den);
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
