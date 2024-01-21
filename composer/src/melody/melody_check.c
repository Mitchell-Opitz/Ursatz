#include <ursatz_composer/melody_fill.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <non_chord_tones.h>
#include <consonance_rules.h>
#include <counterpoint_limits.h>
#include <counterpoint_line.h>
#include <harmonic_rules.h>
#include <harmony_resolution_rules.h>
#include <melodic_rules.h>
#include <pitch/pitch_spelling.h>
#include <semantics/non_chord_tone.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

#include "melody/melody_internal.h"

/*
 * The checker for the melody operator. The rules are the frameworks', called here and not written again:
 *   - the non-chord tones: every melody note outside the chord it sounds in is labeled by common-practice-minimal's
 *     non-chord-tone labeler, and must be a figure the parameters allow;
 *   - standard-counterpoint's melodic rules over the melody, and its harmonic rules between the melody and the bass;
 *   - standard-harmony's resolution of the leading tone and the seventh from chord to chord;
 * and the stable tone the cadence asks for. Hard rules reject; soft rules score.
 */

#define TICKS_PER_WHOLE ((double)MELODY_TICKS)
#define ID_SIZE 24

/* The voices of the labeler's members, told apart by address. */
static const char MELODY_VOICE[] = "melody";
static const char BASS_VOICE[] = "bass";

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

/* The scale degree a pitch is spelled as in the key; the altered degrees share the degree of the natural one. */
static int pitch_degree(const PlanKey *key, int pitch) {
    static const int MAJOR[12] = {1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7};
    static const int MINOR[12] = {1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7};
    int offset = pitch_class(pitch - key->tonic_pitch_class);
    return (key->mode == PLAN_MODE_MINOR ? MINOR : MAJOR)[offset];
}

/* The diatonic step of a pitch from the tonic: 0 is do, 7 the octave above. */
static int pitch_step(const PlanKey *key, int pitch) {
    int relative = pitch - key->tonic_pitch_class;
    int octave = relative >= 0 ? relative / 12 : -((-relative + 11) / 12);
    return octave * 7 + pitch_degree(key, pitch) - 1;
}

unsigned melody_cadence_mask(const MelodyContext *context) {
    if (!context->has_cadence) {
        return 0xFFFu;
    }
    unsigned tonic = 1u << context->key.tonic_pitch_class;
    unsigned third = 1u << ((context->key.tonic_pitch_class + (context->key.mode == PLAN_MODE_MINOR ? 3 : 4)) % 12);
    switch (context->cadence) {
    case PLAN_CADENCE_PAC:
    case PLAN_CADENCE_PC:
        return tonic;
    case PLAN_CADENCE_IAC:
        return third;
    default:
        return 0xFFFu;
    }
}

static unsigned bits_set(unsigned bits) {
    unsigned count = 0;
    for (; bits; bits >>= 1) {
        count += bits & 1u;
    }
    return count;
}

/* The note of the melody sounding at a tick, or the count when none is. */
static size_t note_at(const MelodyCandidate *melody, long tick) {
    for (size_t i = 0; i < melody->count; i++) {
        if (melody->notes[i].onset <= tick && tick < melody->notes[i].onset + melody->notes[i].duration) {
            return i;
        }
    }
    return melody->count;
}

static unsigned nct_bit(NonChordTone tone) {
    switch (tone) {
    case NON_CHORD_TONE_PASSING:
        return MELODY_NCT_PASSING;
    case NON_CHORD_TONE_NEIGHBOR:
        return MELODY_NCT_NEIGHBOR;
    case NON_CHORD_TONE_APPOGGIATURA:
        return MELODY_NCT_APPOGGIATURA;
    case NON_CHORD_TONE_SUSPENSION:
        return MELODY_NCT_SUSPENSION;
    case NON_CHORD_TONE_ANTICIPATION:
        return MELODY_NCT_ANTICIPATION;
    default:
        return 0;
    }
}

/* Range, and a chord tone on every strong beat (but for the figures that put a non-chord tone there). */
static void check_basics(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    unsigned strong_figures = MELODY_NCT_APPOGGIATURA | MELODY_NCT_SUSPENSION;
    for (size_t i = 0; !verdict->hard && i < melody->count; i++) {
        const MelodyNote *note = &melody->notes[i];
        if (note->pitch < melody->params.range_low || note->pitch > melody->params.range_high) {
            verdict->hard = "outside the melody range";
        } else if (note->onset % context->cell == 0 && !(context->chord_mask[note->chord] & (1u << pitch_class(note->pitch))) &&
                   !(melody->params.nct_types & strong_figures)) {
            verdict->hard = "strong beat without a chord tone";
        }
    }
}

/* Labels the leftover tones of each chord's segment with the framework, and holds each to the types allowed. */
static bool check_tones(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    size_t total = 0;
    size_t used = 0;
    for (size_t s = 0; s < context->count; s++) {
        long from = context->chord_start[s];
        long to = context->chord_start[s + 1];
        for (size_t i = 0; i < melody->count; i++) {
            total += melody->notes[i].onset < to && melody->notes[i].onset + melody->notes[i].duration > from;
        }
        for (size_t b = 0; b < context->bass_count; b++) {
            total += context->bass[b].onset < to && context->bass[b].end > from;
        }
    }
    CommonPracticeMember *members = malloc((total ? total : 1) * sizeof(*members));
    CommonPracticeSegment *segments = malloc(context->count * sizeof(*segments));
    unsigned *masks = malloc(context->count * sizeof(*masks));
    char(*ids)[ID_SIZE] = malloc((melody->count + context->bass_count) * sizeof(*ids));
    bool ok = members && segments && masks && ids;
    for (size_t i = 0; ok && i < melody->count + context->bass_count; i++) {
        if (i < melody->count) {
            snprintf(ids[i], ID_SIZE, "m%zu", i);
        } else {
            snprintf(ids[i], ID_SIZE, "b%zu", i - melody->count);
        }
    }
    for (size_t s = 0; ok && s < context->count; s++) {
        long from = context->chord_start[s];
        long to = context->chord_start[s + 1];
        segments[s].members = &members[used];
        segments[s].member_count = 0;
        segments[s].id = "segment";
        masks[s] = context->chord_mask[s];
        for (size_t i = 0; i < melody->count + context->bass_count; i++) {
            bool bass = i >= melody->count;
            long onset = bass ? context->bass[i - melody->count].onset : melody->notes[i].onset;
            long end = bass ? context->bass[i - melody->count].end : melody->notes[i].onset + melody->notes[i].duration;
            if (onset >= to || end <= from) {
                continue;
            }
            CommonPracticeMember *member = &members[used++];
            member->note_id = ids[i];
            member->pitch = bass ? context->bass[i - melody->count].pitch : melody->notes[i].pitch;
            member->held = onset < from;
            member->duration = (double)((end < to ? end : to) - (onset > from ? onset : from)) / TICKS_PER_WHOLE;
            member->strength = onset % context->cell == 0 ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_WEAK;
            member->voice_id = bass ? BASS_VOICE : MELODY_VOICE;
            member->onset = (double)onset / TICKS_PER_WHOLE;
            segments[s].member_count++;
        }
    }
    for (size_t s = 0; ok && !verdict->hard && s < context->count; s++) {
        CommonPracticeToneLabel *labels = NULL;
        size_t count = 0;
        if (common_practice_label_leftover_tones(segments, masks, context->count, s, &labels, &count) !=
            COMMON_PRACTICE_TONES_OK) {
            ok = false;
            break;
        }
        for (size_t k = 0; !verdict->hard && k < count; k++) {
            const CommonPracticeMember *member = &segments[s].members[labels[k].member_index];
            if (member->voice_id == BASS_VOICE) {
                verdict->hard = "bass note outside its chord";
            } else if (!labels[k].classified) {
                verdict->hard = "non-chord tone of no figure";
            } else if (!(nct_bit(labels[k].tone) & melody->params.nct_types)) {
                verdict->hard = "non-chord tone of a figure not allowed";
            }
        }
        free(labels);
    }
    free(members);
    free(segments);
    free(masks);
    free(ids);
    return ok;
}

/* The pitch spelled as a degree of the key; the caller destroys it. */
static PitchSpelling *spell_melody(const PlanKey *key, int pitch) {
    return harmony_spell_pitch(key, pitch_degree(key, pitch), pitch);
}

static unsigned melodic_soft_rules(void) {
    unsigned count = 0;
    for (int rule = 0; rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
        count += standard_counterpoint_melodic_preference((StandardCounterpointMelodicRule)rule) != NULL;
    }
    return count;
}

static unsigned harmonic_soft_rules(void) {
    unsigned count = 0;
    for (int rule = 0; rule < STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT; rule++) {
        count += standard_counterpoint_harmonic_preference((StandardCounterpointHarmonicRule)rule) != NULL;
    }
    return count;
}

/* standard-counterpoint's melodic rules over the melody. */
static bool check_melodic(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    PitchSpelling **spellings = calloc(melody->count, sizeof(*spellings));
    bool ok = spellings != NULL;
    for (size_t i = 0; ok && !verdict->hard && i < melody->count; i++) {
        spellings[i] = spell_melody(&melody->context.key, melody->notes[i].pitch);
        if (!spellings[i]) {
            verdict->hard = "a melody pitch cannot be spelled";
        }
    }
    if (ok && !verdict->hard) {
        StandardCounterpointLine line = {(const PitchSpelling *const *)spellings, melody->count};
        StandardCounterpointLimits limits = standard_counterpoint_default_limits();
        unsigned hard = 0;
        unsigned soft = 0;
        verdict->soft_total += melodic_soft_rules();
        if (!standard_counterpoint_melodic_check(&line, &limits, &hard, &soft)) {
            ok = false;
        } else {
            /* A tonal melody need not end by step, as a strict counterpoint line must. */
            hard &= ~(1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA);
            for (int rule = 0; rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
                if (hard & (1u << rule)) {
                    verdict->hard = standard_counterpoint_melodic_rule_name((StandardCounterpointMelodicRule)rule);
                    break;
                }
            }
            verdict->soft_held += melodic_soft_rules() - bits_set(soft);
        }
    }
    for (size_t i = 0; spellings && i < melody->count; i++) {
        if (spellings[i]) {
            pitch_spelling_destroy(spellings[i]);
        }
    }
    free(spellings);
    return ok;
}

/*
 * standard-counterpoint's harmonic rules between the melody (the upper part) and the bass, read where either
 * one changes: no voice crossing, no parallel fifths or octaves, and a dissonance against the bass only in a
 * non-chord tone or the chord's seventh. The rules for the first and last interval and the largest gap are for
 * strict species and are not asked. Imperfect consonances on strong beats score.
 */
static bool check_against_bass(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    size_t capacity = melody->count + context->bass_count;
    long *times = malloc(capacity * sizeof(*times));
    PitchSpelling **upper = calloc(capacity, sizeof(*upper));
    PitchSpelling **lower = calloc(capacity, sizeof(*lower));
    bool *strong = calloc(capacity, sizeof(*strong));
    bool ok = times && upper && lower && strong;
    size_t count = 0;
    size_t samples = 0;
    for (size_t i = 0; ok && i < capacity; i++) {
        long at = i < melody->count ? melody->notes[i].onset : context->bass[i - melody->count].onset;
        size_t place = count;
        bool seen = false;
        for (size_t k = 0; k < count; k++) {
            seen = seen || times[k] == at;
        }
        if (seen) {
            continue;
        }
        while (place > 0 && times[place - 1] > at) {
            times[place] = times[place - 1];
            place--;
        }
        times[place] = at;
        count++;
    }
    for (size_t t = 0; ok && !verdict->hard && t < count; t++) {
        size_t m = note_at(melody, times[t]);
        size_t b = context->bass_count;
        for (size_t k = 0; k < context->bass_count; k++) {
            if (context->bass[k].onset <= times[t] && times[t] < context->bass[k].end) {
                b = k;
            }
        }
        if (m == melody->count || b == context->bass_count) {
            continue;
        }
        const PhraseChord *chord = &context->chords[context->bass[b].chord];
        upper[samples] = spell_melody(&context->key, melody->notes[m].pitch);
        lower[samples] = harmony_spell_pitch(&context->key, harmony_member_degree(context->key.mode, chord),
                                             context->bass[b].pitch);
        if (!upper[samples] || !lower[samples]) {
            verdict->hard = "a pitch cannot be spelled";
            samples++;
            break;
        }
        unsigned pitch_bit = 1u << pitch_class(melody->notes[m].pitch);
        size_t sounding = context->bass[b].chord;
        if (!standard_counterpoint_pitches_consonant(lower[samples], upper[samples]) &&
            (context->chord_mask[sounding] & pitch_bit) && !(context->chord_seventh[sounding] & pitch_bit)) {
            verdict->hard = "dissonance against the bass";
        }
        strong[samples] = times[t] % context->cell == 0;
        samples++;
    }
    if (ok && !verdict->hard && samples == 0) {
        verdict->hard = "melody does not sound over the bass";
    }
    if (ok && !verdict->hard) {
        StandardCounterpointLine up = {(const PitchSpelling *const *)upper, samples};
        StandardCounterpointLine down = {(const PitchSpelling *const *)lower, samples};
        StandardCounterpointPair pair = {&up, &down};
        unsigned hard = 0;
        unsigned soft = 0;
        unsigned asked = (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING) |
                         (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES) |
                         (1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS);
        verdict->soft_total += harmonic_soft_rules() + 1;
        if (!standard_counterpoint_harmonic_check(&pair, &hard, &soft)) {
            ok = false;
        } else {
            hard &= asked;
            for (int rule = 0; rule < STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT; rule++) {
                if (hard & (1u << rule)) {
                    verdict->hard = standard_counterpoint_harmonic_rule_name((StandardCounterpointHarmonicRule)rule);
                    break;
                }
            }
            verdict->soft_held += harmonic_soft_rules() - bits_set(soft);
            /* Imperfect consonances (thirds and sixths) on at least half of the strong beats. */
            unsigned on_strong = 0;
            unsigned imperfect = 0;
            for (size_t i = 0; i < samples; i++) {
                StandardCounterpointIntervalCategory category;
                if (strong[i]) {
                    on_strong++;
                    imperfect += standard_counterpoint_classify_pitches(lower[i], upper[i], &category) &&
                                 category == STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE;
                }
            }
            verdict->soft_held += on_strong == 0 || imperfect * 2 >= on_strong;
        }
    }
    for (size_t i = 0; upper && lower && i < capacity; i++) {
        if (upper[i]) {
            pitch_spelling_destroy(upper[i]);
        }
        if (lower[i]) {
            pitch_spelling_destroy(lower[i]);
        }
    }
    free(times);
    free(upper);
    free(lower);
    free(strong);
    return ok;
}

/* standard-harmony's resolution of the leading tone and the seventh, for the melody as the outer voice. */
static void check_tendency(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    for (size_t i = 0; !verdict->hard && i + 1 < context->count; i++) {
        long boundary = context->chord_start[i + 1];
        size_t from = note_at(melody, boundary - 1);
        size_t to = note_at(melody, boundary);
        if (context->chords[i].applied || context->chords[i + 1].applied || from == melody->count ||
            to == melody->count) {
            continue;
        }
        HarmonyVoiceResolution move;
        memset(&move, 0, sizeof(move));
        move.mode = harmony_mode_of(context->key.mode);
        move.from = context->chords[i].chord;
        move.to = context->chords[i + 1].chord;
        move.from_step = pitch_step(&context->key, melody->notes[from].pitch);
        move.to_step = pitch_step(&context->key, melody->notes[to].pitch);
        move.outer = true;
        for (int rule = 0; rule < HARMONY_RESOLUTION_RULE_COUNT; rule++) {
            if (harmony_resolution_rule_applies((HarmonyResolutionRule)rule, &move) &&
                !harmony_resolution_rule_holds((HarmonyResolutionRule)rule, &move)) {
                verdict->hard = harmony_resolution_rule_name((HarmonyResolutionRule)rule);
            }
        }
    }
}

bool melody_judge(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    bool ok = true;
    memset(verdict, 0, sizeof(*verdict));
    if (melody->count == 0 || melody->count > MELODY_MAX_NOTES) {
        verdict->hard = "no melody notes";
        return true;
    }
    check_basics(melody, verdict);
    if (!verdict->hard) {
        ok = check_tones(melody, verdict);
    }
    if (ok && !verdict->hard) {
        ok = check_melodic(melody, verdict);
    }
    if (ok && !verdict->hard) {
        ok = check_against_bass(melody, verdict);
    }
    if (ok && !verdict->hard) {
        check_tendency(melody, verdict);
    }
    /* The cadence arrives on its stable tone: the tonic for a PAC, the third for an IAC. */
    if (ok && !verdict->hard &&
        !(melody_cadence_mask(&melody->context) & (1u << pitch_class(melody->notes[melody->count - 1].pitch)))) {
        verdict->hard = "cadence without its stable tone";
    }
    if (ok && !verdict->hard) {
        unsigned held;
        unsigned total;
        melody_contour_score(melody, &held, &total);
        verdict->soft_held += held;
        verdict->soft_total += total;
    }
    return ok;
}

bool melody_check(const void *payload, FillCheck *out) {
    const MelodyCandidate *melody = payload;
    PhraseVerdict verdict;
    if (!melody || melody->magic != MELODY_PAYLOAD_MAGIC || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!melody_judge(melody, &verdict)) {
        return false;
    }
    if (verdict.hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        return true;
    }
    out->score_numerator = verdict.soft_held;
    out->score_denominator = verdict.soft_total ? verdict.soft_total : 1;
    snprintf(out->reason, sizeof(out->reason), "melody rules");
    return true;
}

bool melody_fill_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, MELODY_FILL_OPERATOR_NAME) == 0) {
        return melody_check(candidate->payload, out);
    }
    return harmony_phrase_checker(plan, target, operator_, candidate, context, out);
}

bool melody_check_lane(const PiecePlan *plan, PlanSpan span, PlanKey key, const MelodyFillParams *params,
                       const char **violation) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    MelodyCandidate *melody = calloc(1, sizeof(*melody));
    bool ok = melody && violation;
    size_t found = 0;
    long origin = 0;
    if (ok) {
        found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, HARMONY_MAX_CHORDS);
        ok = found > 0 && found <= HARMONY_MAX_CHORDS && melody_ticks(span.start, &origin) &&
             melody_phrase_read(plan, plan_lane_span_owner(plan, ids[0]), span, key, &melody->context);
    }
    if (melody) {
        melody->params = params ? *params : melody_fill_params_default();
        melody->magic = MELODY_PAYLOAD_MAGIC;
    }
    /* Every note of the melody lane over the phrase, from the start of the phrase. */
    for (size_t i = 0; ok && i < found; i++) {
        long base;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        ok = plan_span_contains(span, plan_lane_span_span(plan, ids[i])) &&
             melody_ticks(plan_lane_span_span(plan, ids[i]).start, &base);
        for (size_t n = 0; ok && n < notes->note_count; n++) {
            MelodyNote *note = &melody->notes[melody->count];
            ok = melody->count < MELODY_MAX_NOTES && melody_ticks(notes->notes[n].offset, &note->onset) &&
                 melody_ticks(notes->notes[n].duration, &note->duration);
            if (ok) {
                note->onset += base - origin;
                note->pitch = notes->notes[n].midi_pitch;
                note->motive_index = -1;
                note->strong = note->onset % melody->context.cell == 0;
                note->chord = melody_chord_at(&melody->context, note->onset);
                melody->count++;
            }
        }
    }
    PhraseVerdict verdict;
    ok = ok && melody_judge(melody, &verdict);
    if (ok) {
        *violation = verdict.hard;
    }
    free(melody);
    return ok;
}
