#ifndef URSATZ_COMPOSER_VOICING_INTERNAL_H
#define URSATZ_COMPOSER_VOICING_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <pitch/pitch_spelling.h>

#include <ursatz_composer/voicing.h>

#include "texture/texture_internal.h"

/* The voices the operator chooses, top to bottom; the bass is written. */
#define VOICING_PARTS 3
#define VOICING_SOPRANO 0
#define VOICING_ALTO 1
#define VOICING_TENOR 2
#define VOICING_BASS 3
/* Every voice of a chord, the bass last. */
#define VOICING_VOICES 4
#define VOICING_MAX_CHORDS HARMONY_MAX_CHORDS
/* The most voicings of one chord the enumeration keeps. */
#define VOICING_MAX_OPTIONS 4096
#define VOICING_PAYLOAD_MAGIC 0x56434F31UL
/* A voice moves by "a third or less" when it moves this many semitones or fewer. */
#define VOICING_SMOOTH_SEMITONES 4
/* A leap wider than a fifth costs this much more. */
#define VOICING_LEAP_SEMITONES 7
#define VOICING_LEAP_COST 12

/* What the operator reads from the plan: the target, the chords, the bass and the soprano it is voiced over. */
typedef struct {
    LaneSpanId target;     /* the alto */
    LaneSpanId tenor_span; /* the tenor: none, or an open span over the same time */
    LaneSpanId melody_span; /* with write_soprano, the open melody span the soprano is written in, else none */
    NodeId owner;
    PlanSpan span;
    PlanKey key;
    bool write_soprano;
    long length;
    size_t chord_count;
    long chord_start[VOICING_MAX_CHORDS + 1]; /* the last entry is the length */
    PhraseChord chord[VOICING_MAX_CHORDS];
    /* The tones of each chord, root first, as pitch classes, scale degrees and spellings (octave 4, unused). */
    int pcs[VOICING_MAX_CHORDS][4];
    int tone_degree[VOICING_MAX_CHORDS][4];
    size_t tone_count[VOICING_MAX_CHORDS];
    PitchSpelling *tone[VOICING_MAX_CHORDS][4];
    /* The spelling of every MIDI pitch whose pitch class is a tone of the chord, else NULL. */
    PitchSpelling *spell[VOICING_MAX_CHORDS][128];
    /* The leading tone standard-voicing is told of: the key's, or the temporary key's for an applied chord. */
    const PitchSpelling *leading[VOICING_MAX_CHORDS];
    PitchSpelling *key_leading;
    PitchSpelling *key_tonic; /* octave 4 */
    /* The bass sounding at the start of the chord, and the member of the chord it is (the inversion). */
    int bass[VOICING_MAX_CHORDS];
    int inversion[VOICING_MAX_CHORDS];
    /* The soprano of each chord when it is written: its pitch, or -1 when no chord tone sounds in the chord. */
    int soprano[VOICING_MAX_CHORDS];
    /* The pitch classes a soprano the voicing writes may take, as a bit mask: all but over the last chord of a cadence. */
    unsigned soprano_mask[VOICING_MAX_CHORDS];
    bool chord_ok[VOICING_MAX_CHORDS]; /* the bass (and a written soprano) are tones of the chord */
} VoicingContext;

/* A candidate's payload: the soprano, alto and tenor of each chord, held for the chord. */
typedef struct {
    unsigned long magic;
    size_t chord_count;
    int pitch[VOICING_MAX_CHORDS][VOICING_PARTS];
} VoicingCandidate;

/* One voicing of a chord: soprano, alto and tenor. */
typedef struct {
    int pitch[VOICING_PARTS];
} VoicingOption;

/* Reads a target; false when it is not one the operator applies to. The caller frees it with voicing_context_free. */
bool voicing_read(const PiecePlan *plan, LaneSpanId target, VoicingContext *out);
void voicing_context_free(VoicingContext *context);

/* Whether standard-voicing holds for the voicing (soprano, alto, tenor) of the chord over its bass. */
bool voicing_chord_valid(const VoicingContext *context, size_t chord, const int pitch[VOICING_PARTS]);

/*
 * Every valid voicing of a chord, in a fixed order, at most max. *count is how many; false when the chord cannot be
 * voiced at all (its bass or its written soprano is not a tone of it) or max was too small.
 */
bool voicing_options(const VoicingContext *context, size_t chord, VoicingOption *out, size_t max, size_t *count);

/*
 * Whether the connection of chord - 1 to chord is allowed: no parallel fifths or octaves between any pair of voices,
 * no direct fifths or octaves between the outer two when the soprano is written, and each of the upper three voices
 * resolves a chordal seventh and the leading tone of a V. why names the rule broken when it is not (NULL to not ask).
 */
bool voicing_connection_ok(const VoicingContext *context, size_t chord, const int from[VOICING_PARTS],
                           const int to[VOICING_PARTS], const char **why);

/* The semitones the upper three voices move from one voicing to the next, and the cost of the leaps among them. */
long voicing_connection_cost(const int from[VOICING_PARTS], const int to[VOICING_PARTS]);

/* Whether every upper voice moves a third or less. */
bool voicing_connection_smooth(const int from[VOICING_PARTS], const int to[VOICING_PARTS]);

/* Judges a candidate against the plan: hard rules, and the soft ones held. */
typedef struct {
    const char *hard; /* NULL when no hard rule is broken, else which */
    unsigned held;
    unsigned total;
} VoicingVerdict;

void voicing_judge(const VoicingContext *context, const VoicingCandidate *candidate, VoicingVerdict *verdict);

/*
 * Proposes up to wanted complete voicings of all the chords, cheapest first, as VoicingCandidate payloads in out, which
 * the caller frees; *count is how many. None when a chord cannot be voiced or no path joins the chords. Ties in cost
 * are ordered by rng. False only on allocation failure. wanted is at most the size of out.
 */
bool voicing_search(const VoicingContext *context, FillRng *rng, size_t wanted, size_t beam, VoicingCandidate **out,
                    size_t *count);

#endif
