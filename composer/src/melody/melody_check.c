#include <ursatz_composer/melody_fill.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common-practice-minimal/non_chord_tones.h>
#include <standard-counterpoint/consonance_rules.h>
#include <standard-counterpoint/counterpoint_limits.h>
#include <standard-counterpoint/counterpoint_line.h>
#include <standard-counterpoint/harmonic_rules.h>
#include <standard-harmony/harmony_resolution_rules.h>
#include <standard-counterpoint/melodic_rules.h>
#include <pitch/pitch_spelling.h>
#include <semantics/non_chord_tone.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/seam_hear.h>

#include "check/shaping.h"
#include "melody/melody_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

/*
 * The checker for the melody operator. The rules are the frameworks', called here and not written again:
 *   - the non-chord tones: every melody note outside the chord it sounds in is labeled by common-practice-minimal's
 *     non-chord-tone labeler, and must be a figure the parameters allow;
 *   - standard-counterpoint's melodic rules over the melody, and its harmonic rules between the melody and the bass;
 *   - standard-harmony's resolution of the leading tone and the seventh from chord to chord;
 * and the stable tone the cadence asks for. Hard rules reject; soft rules score.
 */

#define ID_SIZE 24

/* The voices of the labeler's members, told apart by address. */
static const char MELODY_VOICE[] = "melody";
static const char BASS_VOICE[] = "bass";

/* The scale degree a pitch is spelled as in the key; the altered degrees share the degree of the natural one. */
static int pitch_degree(const AnalysisKey *key, int pitch) {
    int degree = 1;
    int alteration;
    util_scale_degree(analysis_key_tonic_pitch_class(key), key->mode == KEY_STRUCTURE_MODE_MINOR, pitch, &degree,
                      &alteration);
    return degree;
}

/* The diatonic step of a pitch from the tonic: 0 is do, 7 the octave above. */
static int pitch_step(const AnalysisKey *key, int pitch) {
    int relative = pitch - analysis_key_tonic_pitch_class(key);
    int octave = relative >= 0 ? relative / 12 : -((-relative + 11) / 12);
    return octave * 7 + pitch_degree(key, pitch) - 1;
}

unsigned melody_cadence_mask(const MelodyContext *context) {
    if (!context->has_cadence) {
        return 0xFFFu;
    }
    unsigned tonic = 1u << analysis_key_tonic_pitch_class(&context->key);
    unsigned third = 1u << ((analysis_key_tonic_pitch_class(&context->key) + (context->key.mode == KEY_STRUCTURE_MODE_MINOR ? 3 : 4)) % 12);
    switch (context->cadence) {
    case CADENCE_TYPE_PERFECT_AUTHENTIC:
    case CADENCE_TYPE_PLAGAL:
        return tonic;
    case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
        return third;
    default:
        return 0xFFFu;
    }
}

/* The note of the melody sounding at a position, or the count when none is. */
static size_t note_at(const MelodyCandidate *melody, Rational position) {
    for (size_t i = 0; i < melody->count; i++) {
        if (rational_cmp(melody->notes[i].onset, position) <= 0 &&
            rational_cmp(position, position_add(melody->notes[i].onset, melody->notes[i].duration)) < 0) {
            return i;
        }
    }
    return melody->count;
}

/* The note of the melody sounding just before a position (it starts before it and ends at or after it), or the count. */
static size_t note_before(const MelodyCandidate *melody, Rational position) {
    for (size_t i = 0; i < melody->count; i++) {
        if (rational_cmp(melody->notes[i].onset, position) < 0 &&
            rational_cmp(position, position_add(melody->notes[i].onset, melody->notes[i].duration)) <= 0) {
            return i;
        }
    }
    return melody->count;
}

unsigned melody_nct_bit(int tone) {
    switch ((NonChordTone)tone) {
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
    case NON_CHORD_TONE_RETARDATION:
        return MELODY_NCT_RETARDATION;
    case NON_CHORD_TONE_ESCAPE_TONE:
        return MELODY_NCT_ESCAPE;
    case NON_CHORD_TONE_DOUBLE_NEIGHBOR:
        return MELODY_NCT_DOUBLE_NEIGHBOR;
    case NON_CHORD_TONE_CHANGING_TONE:
        return MELODY_NCT_CHANGING_TONE;
    default:
        return 0;
    }
}

/* Range, and a chord tone on every strong beat (but for the figures that put a non-chord tone there). */
static void check_basics(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    unsigned strong_figures = MELODY_STRONG_FIGURES;
    /* The pickup is held to the range, and to the melodic rules in check_melodic; no chord tone is asked of it. */
    for (size_t i = 0; !verdict->hard && i < melody->pickup_count; i++) {
        if (melody->pickup_notes[i].pitch < melody->params.range_low ||
            melody->pickup_notes[i].pitch > melody->params.range_high) {
            verdict->hard = "outside the melody range";
        }
    }
    for (size_t i = 0; !verdict->hard && i < melody->count; i++) {
        const MelodyNote *note = &melody->notes[i];
        if (note->pitch < melody->params.range_low || note->pitch > melody->params.range_high) {
            verdict->hard = "outside the melody range";
        } else if (melody_on_cell(context, note->onset) && !(context->chord_mask[note->chord] & (1u << util_pitch_class(note->pitch))) &&
                   !(melody->params.nct_types & strong_figures)) {
            verdict->hard = "strong beat without a chord tone";
        }
    }
}

/*
 * Labels the leftover tones of each chord's segment with the framework, and holds each to the types allowed. With `loose`
 * (a survey, not a check) nothing is rejected: every melody note the labeler cannot classify has its loose[] set, and
 * `tones` holds the figure of every note it classifies, whether the parameters allow it or not.
 */
static bool check_tones(const MelodyCandidate *melody, PhraseVerdict *verdict, unsigned *tones, bool *loose) {
    const MelodyContext *context = &melody->context;
    size_t total = 0;
    size_t used = 0;
    for (size_t s = 0; s < context->count; s++) {
        Rational from = context->chord_start[s];
        Rational to = context->chord_start[s + 1];
        for (size_t i = 0; i < melody->count; i++) {
            total += rational_cmp(melody->notes[i].onset, to) < 0 &&
                     rational_cmp(position_add(melody->notes[i].onset, melody->notes[i].duration), from) > 0;
        }
        for (size_t b = 0; b < context->bass_count; b++) {
            total += rational_cmp(context->bass[b].onset, to) < 0 && rational_cmp(context->bass[b].end, from) > 0;
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
        Rational from = context->chord_start[s];
        Rational to = context->chord_start[s + 1];
        segments[s].members = &members[used];
        segments[s].member_count = 0;
        segments[s].id = "segment";
        masks[s] = context->chord_mask[s];
        for (size_t i = 0; i < melody->count + context->bass_count; i++) {
            bool bass = i >= melody->count;
            Rational onset = bass ? context->bass[i - melody->count].onset : melody->notes[i].onset;
            Rational end = bass ? context->bass[i - melody->count].end
                                : position_add(melody->notes[i].onset, melody->notes[i].duration);
            if (rational_cmp(onset, to) >= 0 || rational_cmp(end, from) <= 0) {
                continue;
            }
            CommonPracticeMember *member = &members[used++];
            member->note_id = ids[i];
            member->pitch = bass ? context->bass[i - melody->count].pitch : melody->notes[i].pitch;
            member->held = rational_cmp(onset, from) < 0;
            member->duration = rational_to_double(position_sub(rational_cmp(end, to) < 0 ? end : to,
                                                              rational_cmp(onset, from) > 0 ? onset : from));
            member->strength = melody_on_cell(context, onset) ? BEAT_STRENGTH_STRONG : BEAT_STRENGTH_WEAK;
            member->voice_id = bass ? BASS_VOICE : MELODY_VOICE;
            member->onset = rational_to_double(onset);
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
            bool bass = member->voice_id == BASS_VOICE;
            size_t note = bass ? melody->count : (size_t)strtoul(member->note_id + 1, NULL, 10);
            if (bass) {
                verdict->hard = loose ? verdict->hard : "bass note outside its chord";
            } else if (!labels[k].classified) {
                if (loose && note < melody->count) {
                    loose[note] = true;
                } else if (!loose) {
                    verdict->hard = "non-chord tone of no figure";
                }
            } else if (!loose && !(melody_nct_bit((int)labels[k].tone) & melody->params.nct_types)) {
                verdict->hard = "non-chord tone of a figure not allowed";
            } else if (tones && note < melody->count && !tones[note]) {
                tones[note] = melody_nct_bit((int)labels[k].tone);
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
static PitchSpelling *spell_melody(const AnalysisKey *key, int pitch) {
    return harmony_spell_pitch(key, pitch_degree(key, pitch), pitch);
}

unsigned melody_melodic_soft_rules(void) {
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

unsigned melody_motive_waived(const MelodyContext *context) {
    PitchSpelling *spellings[MOTIVE_MAX_NOTES];
    unsigned hard = 0;
    unsigned soft = 0;
    bool spelled = context->motive_count > 0 && context->motive_count <= MOTIVE_MAX_NOTES;
    memset(spellings, 0, sizeof(spellings));
    for (size_t k = 0; spelled && k < context->motive_count; k++) {
        spellings[k] = spell_melody(&context->key, context->motive_pitch[k]);
        spelled = spellings[k] != NULL;
    }
    if (spelled) {
        StandardCounterpointLine line = {(const PitchSpelling *const *)spellings, context->motive_count};
        StandardCounterpointLimits limits = standard_counterpoint_default_limits();
        if (!standard_counterpoint_melodic_check(&line, &limits, &hard, &soft)) {
            hard = 0;
        }
    }
    for (size_t k = 0; k < MOTIVE_MAX_NOTES; k++) {
        if (spellings[k]) {
            pitch_spelling_destroy(spellings[k]);
        }
    }
    return hard;
}

unsigned melody_window_hard(const MelodyContext *context, const int *pitches, size_t count) {
    PitchSpelling *spellings[MELODY_WINDOW_MAX];
    unsigned hard = 0;
    unsigned soft = 0;
    bool spelled = count > 1 && count <= MELODY_WINDOW_MAX;
    memset(spellings, 0, sizeof(spellings));
    for (size_t i = 0; spelled && i < count; i++) {
        spellings[i] = spell_melody(&context->key, pitches[i]);
        spelled = spellings[i] != NULL;
    }
    if (spelled) {
        StandardCounterpointLine line = {(const PitchSpelling *const *)spellings, count};
        StandardCounterpointLimits limits = standard_counterpoint_default_limits();
        if (!standard_counterpoint_melodic_check(&line, &limits, &hard, &soft)) {
            hard = 0;
        }
    }
    for (size_t i = 0; i < MELODY_WINDOW_MAX; i++) {
        if (spellings[i]) {
            pitch_spelling_destroy(spellings[i]);
        }
    }
    return hard & ~(1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA) & ~context->motive_waived;
}

/* The rules of standard-counterpoint's melodic check that judge a run of notes by itself: what a relaxed melody still holds (R-338). */
#define LOCAL_RULES                                                                                         \
    ((1u << STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES) |                                            \
     (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CONSECUTIVE_LEAPS) |                                         \
     (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC) |                                          \
     (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP) | (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE))

/* How many hard rules of the melodic check there are besides the local ones and clausula vera. */
static unsigned other_hard_rules(void) {
    unsigned count = 0;
    for (int rule = 0; rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
        count += rule != STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA && !((LOCAL_RULES >> rule) & 1u) &&
                 standard_counterpoint_melodic_preference((StandardCounterpointMelodicRule)rule) == NULL;
    }
    return count;
}

/*
 * The local rules broken by any maximal run of generated notes (the pickup's and the notes after the motive's) with the
 * note on either side of it: a realized note, the last note before the target when the run starts it, the first after it
 * when the run ends it. Each run goes through the framework's melodic check (melody_window_hard).
 */
static unsigned generated_runs_hard(const MelodyCandidate *melody) {
    const MelodyContext *context = &melody->context;
    size_t capacity = context->lead_in_count + melody->pickup_count + melody->count + 1;
    int *pitch = malloc(capacity * sizeof(*pitch));
    bool *generated = malloc(capacity * sizeof(*generated));
    size_t total = 0;
    unsigned hard = 0;
    if (!pitch || !generated) {
        free(pitch);
        free(generated);
        return 0;
    }
    for (size_t i = 0; i < context->lead_in_count; i++) {
        pitch[total] = context->lead_in[i];
        generated[total++] = false;
    }
    for (size_t i = 0; i < melody->pickup_count; i++) {
        pitch[total] = melody->pickup_notes[i].pitch;
        generated[total++] = true;
    }
    for (size_t i = 0; i < melody->count; i++) {
        pitch[total] = melody->notes[i].pitch;
        generated[total++] = melody->notes[i].motive_index < 0;
    }
    if (context->has_lead_out) {
        pitch[total] = context->lead_out;
        generated[total++] = false;
    }
    for (size_t first = 0; first < total; first++) {
        if (!generated[first]) {
            continue;
        }
        size_t last = first;
        while (last + 1 < total && generated[last + 1]) {
            last++;
        }
        size_t from = first > 0 ? first - 1 : first;
        size_t to = last + 1 < total ? last + 1 : last;
        /* A run longer than the window is read in overlapping parts. */
        for (size_t at = from; at < to; at += MELODY_WINDOW_MAX - 3) {
            size_t end = at + MELODY_WINDOW_MAX <= to + 1 ? at + MELODY_WINDOW_MAX : to + 1;
            hard |= melody_window_hard(context, pitch + at, end - at) & LOCAL_RULES;
            if (end == to + 1) {
                break;
            }
        }
        first = last;
    }
    free(pitch);
    free(generated);
    return hard;
}

/* A group of soft rules counts weight times in the score (MelodyWeights): weight * held of weight * total. */
static void add_soft(PhraseVerdict *verdict, unsigned weight, unsigned held, unsigned total) {
    verdict->soft_held += weight * held;
    verdict->soft_total += weight * total;
}

/* standard-counterpoint's melodic rules over the melody. */
static bool check_melodic(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    /* A target that follows written melody is read as one line with the last notes of it, so a leap or a run across
       the join is judged like any other; so is the join to written or Given melody that follows it. */
    size_t lead = melody->context.lead_in_count;
    size_t trail = melody->context.has_lead_out ? 1 : 0;
    size_t pickup = melody->pickup_count;
    /* A realized motive starts and ends as its own pitches do: the join to the idea before or after it is not held to the
       melodic rules, as a repetition of a motive that ends a sixth above where it starts could not be said (R-335). */
    if (pickup == 0 && melody->count > 0 && melody->notes[0].motive_index >= 0) {
        lead = 0;
    }
    if (melody->count > 0 && melody->notes[melody->count - 1].motive_index >= 0) {
        trail = 0;
    }
    size_t total = lead + pickup + melody->count + trail;
    PitchSpelling **spellings = calloc(total, sizeof(*spellings));
    bool ok = spellings != NULL;
    for (size_t i = 0; ok && !verdict->hard && i < total; i++) {
        int pitch = i < lead ? melody->context.lead_in[i]
                    : i < lead + pickup
                        ? melody->pickup_notes[i - lead].pitch
                        : (i < lead + pickup + melody->count ? melody->notes[i - lead - pickup].pitch
                                                             : melody->context.lead_out);
        spellings[i] = spell_melody(&melody->context.key, pitch);
        if (!spellings[i]) {
            verdict->hard = "a melody pitch cannot be spelled";
        }
    }
    if (ok && !verdict->hard) {
        StandardCounterpointLine line = {(const PitchSpelling *const *)spellings, total};
        StandardCounterpointLimits limits = standard_counterpoint_default_limits();
        unsigned hard = 0;
        unsigned soft = 0;
        add_soft(verdict, melody->params.weights.melodic, 0, melody_melodic_soft_rules());
        if (!standard_counterpoint_melodic_check(&line, &limits, &hard, &soft)) {
            ok = false;
        } else {
            /* A tonal melody need not end by step, as a strict counterpoint line must. */
            hard &= ~(1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA);
            /* The rules the motive's own line breaks are not asked of a melody that carries it (R-335). */
            for (size_t i = 0; i < melody->count; i++) {
                if (melody->notes[i].motive_index >= 0) {
                    hard &= ~melody->context.motive_waived;
                    break;
                }
            }
            if (melody->relaxed) {
                /* A relaxed melody holds the local rules over its generated runs; the whole line's other hard rules score. */
                unsigned others = other_hard_rules();
                hard &= ~LOCAL_RULES;
                add_soft(verdict, melody->params.weights.melodic, others - util_bits_set(hard), others);
                hard = generated_runs_hard(melody);
            }
            for (int rule = 0; rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
                if (hard & (1u << rule)) {
                    verdict->hard = standard_counterpoint_melodic_rule_name((StandardCounterpointMelodicRule)rule);
                    break;
                }
            }
            add_soft(verdict, melody->params.weights.melodic, melody_melodic_soft_rules() - util_bits_set(soft), 0);
        }
    }
    for (size_t i = 0; spellings && i < total; i++) {
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
    Rational *times = malloc(capacity * sizeof(*times));
    PitchSpelling **upper = calloc(capacity, sizeof(*upper));
    PitchSpelling **lower = calloc(capacity, sizeof(*lower));
    bool *strong = calloc(capacity, sizeof(*strong));
    bool ok = times && upper && lower && strong;
    size_t count = 0;
    size_t samples = 0;
    bool dissonant = false;
    for (size_t i = 0; ok && i < capacity; i++) {
        Rational at = i < melody->count ? melody->notes[i].onset : context->bass[i - melody->count].onset;
        size_t place = count;
        bool seen = false;
        for (size_t k = 0; k < count; k++) {
            seen = seen || rational_cmp(times[k], at) == 0;
        }
        if (seen) {
            continue;
        }
        while (place > 0 && rational_cmp(times[place - 1], at) > 0) {
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
            if (rational_cmp(context->bass[k].onset, times[t]) <= 0 && rational_cmp(times[t], context->bass[k].end) < 0) {
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
        unsigned pitch_bit = 1u << util_pitch_class(melody->notes[m].pitch);
        size_t sounding = context->bass[b].chord;
        if (!standard_counterpoint_pitches_consonant(lower[samples], upper[samples]) &&
            (context->chord_mask[sounding] & pitch_bit) && !(context->chord_seventh[sounding] & pitch_bit)) {
            /* A relaxed melody counts a chord tone dissonant over the bass with the soft rules (R-338). */
            if (melody->relaxed) {
                dissonant = true;
            } else {
                verdict->hard = "dissonance against the bass";
            }
        }
        strong[samples] = melody_on_cell(context, times[t]);
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
        add_soft(verdict, melody->params.weights.against_bass, 0, harmonic_soft_rules() + 1);
        if (!standard_counterpoint_harmonic_check(&pair, &hard, &soft)) {
            ok = false;
        } else {
            hard &= asked;
            if (melody->relaxed) {
                /* Parallel fifths and octaves and a dissonant chord tone score; a voice crossing stays hard. */
                unsigned parallels = hard & ~(1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING);
                hard &= 1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING;
                add_soft(verdict, melody->params.weights.against_bass, 2 - util_bits_set(parallels) + (dissonant ? 0u : 1u), 3);
            }
            for (int rule = 0; rule < STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT; rule++) {
                if (hard & (1u << rule)) {
                    verdict->hard = standard_counterpoint_harmonic_rule_name((StandardCounterpointHarmonicRule)rule);
                    break;
                }
            }
            add_soft(verdict, melody->params.weights.against_bass, harmonic_soft_rules() - util_bits_set(soft), 0);
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
            add_soft(verdict, melody->params.weights.against_bass, on_strong == 0 || imperfect * 2 >= on_strong, 0);
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

int melody_bass_at(const MelodyContext *context, Rational position) {
    int pitch = -1;
    for (size_t b = 0; b < context->bass_count; b++) {
        if (rational_cmp(context->bass[b].onset, position) <= 0 && rational_cmp(position, context->bass[b].end) < 0) {
            pitch = context->bass[b].pitch;
        }
    }
    return pitch;
}

bool melody_parallel_perfects(int from, int to, int bass_from, int bass_to) {
    int a = util_pitch_class(from - bass_from);
    int b = util_pitch_class(to - bass_to);
    return bass_from >= 0 && bass_to >= 0 && from != to && a == b && (a == 0 || a == 7);
}

bool melody_dissonant_over_bass(const MelodyContext *context, int pitch, Rational onset, Rational end) {
    bool dissonant = false;
    unsigned pitch_bit = 1u << util_pitch_class(pitch);
    for (size_t b = 0; !dissonant && b < context->bass_count; b++) {
        if (rational_cmp(context->bass[b].onset, end) >= 0 || rational_cmp(context->bass[b].end, onset) <= 0) {
            continue;
        }
        size_t sounding = context->bass[b].chord;
        PitchSpelling *upper = spell_melody(&context->key, pitch);
        PitchSpelling *lower = harmony_spell_pitch(&context->key, harmony_member_degree(context->key.mode, &context->chords[sounding]),
                                                   context->bass[b].pitch);
        dissonant = upper && lower && !standard_counterpoint_pitches_consonant(lower, upper) &&
                    (context->chord_mask[sounding] & pitch_bit) && !(context->chord_seventh[sounding] & pitch_bit);
        if (upper) {
            pitch_spelling_destroy(upper);
        }
        if (lower) {
            pitch_spelling_destroy(lower);
        }
    }
    return dissonant;
}

/* standard-harmony's resolution of the leading tone and the seventh, for the melody as the outer voice. */
static void check_tendency(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    for (size_t i = 0; !verdict->hard && i + 1 < context->count; i++) {
        Rational boundary = context->chord_start[i + 1];
        size_t from = note_before(melody, boundary);
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
                /* A relaxed melody counts a resolution that does not hold with the soft rules (R-338). */
                if (melody->relaxed) {
                    add_soft(verdict, melody->params.weights.tendency, 0, 1);
                } else {
                    verdict->hard = harmony_resolution_rule_name((HarmonyResolutionRule)rule);
                }
            } else if (melody->relaxed && harmony_resolution_rule_applies((HarmonyResolutionRule)rule, &move)) {
                add_soft(verdict, melody->params.weights.tendency, 1, 1);
            }
        }
    }
}

/*
 * Schoenberg's shaping preferences (R-164), one soft rule each: they add to the share the checker scores, and a rule
 * that does not hold sets its bit in the verdict. They read the notes of the candidate; an idea reads its own.
 */

/* Phrase ending (ch. II): the idea that ends in a cadence is no busier than the phrase before it, and ends on its longest note. */
static bool ending_reduction_holds(const MelodyCandidate *melody) {
    const MelodyContext *context = &melody->context;
    if (!context->idea || !context->has_cadence || melody->count == 0) {
        return true;
    }
    for (size_t i = 0; i + 1 < melody->count; i++) {
        if (rational_cmp(melody->notes[i].duration, melody->notes[melody->count - 1].duration) > 0) {
            return false;
        }
    }
    if (rational_cmp(context->phrase_start, POSITION_ZERO) >= 0 || context->phrase_before_notes == 0 ||
        !position_is_positive(context->length)) {
        return true;
    }
    /* Attacks per beat of the idea against the mean of the phrase before it, both over time: cross-multiplied. */
    return rational_cmp(position_mul(position_sub(POSITION_ZERO, context->phrase_start), (long long)melody->count),
                        position_mul(context->length, (long long)context->phrase_before_notes)) <= 0;
}

/* Waves (ch. XI): a phrase of 8 or more notes has at least 2 local high points (repeated notes read as one) at or before its climax. */
static bool waves_hold(const MelodyCandidate *melody) {
    if (melody->count < 8) {
        return true;
    }
    int line[MELODY_MAX_NOTES];
    size_t count = 0;
    size_t top = 0;
    for (size_t i = 0; i < melody->count; i++) {
        if (count == 0 || line[count - 1] != melody->notes[i].pitch) {
            line[count++] = melody->notes[i].pitch;
            top = line[count - 1] > line[top] ? count - 1 : top;
        }
    }
    unsigned peaks = 0;
    for (size_t i = 1; i < count && i <= top; i++) {
        peaks += i + 1 < count && line[i] > line[i - 1] && line[i] > line[i + 1];
    }
    return peaks >= 2;
}

static int compare_pitch(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

/* Central range (ch. XI): at least 3/4 of the notes lie within 6 semitones of the median pitch. */
static bool central_range_holds(const MelodyCandidate *melody) {
    int pitches[MELODY_MAX_NOTES];
    for (size_t i = 0; i < melody->count; i++) {
        pitches[i] = melody->notes[i].pitch;
    }
    qsort(pitches, melody->count, sizeof(*pitches), compare_pitch);
    int median = pitches[melody->count / 2];
    size_t near = 0;
    for (size_t i = 0; i < melody->count; i++) {
        near += pitches[i] >= median - 6 && pitches[i] <= median + 6;
    }
    return near * 4 >= melody->count * 3;
}

/* Whether the notes starting in bar a and bar b (windows of `bar` from the start) have one rhythm and one set of intervals. */
static bool bars_alike(const MelodyCandidate *melody, Rational bar, size_t a, size_t b) {
    size_t from_a = melody->count;
    size_t from_b = melody->count;
    size_t count_a = 0;
    size_t count_b = 0;
    for (size_t i = 0; i < melody->count; i++) {
        long long bar_of = position_floor_units(melody->notes[i].onset, bar);
        if (bar_of == (long long)a) {
            from_a = count_a++ == 0 ? i : from_a;
        } else if (bar_of == (long long)b) {
            from_b = count_b++ == 0 ? i : from_b;
        }
    }
    if (count_a != count_b || count_a < 2) {
        return false;
    }
    for (size_t k = 0; k < count_a; k++) {
        const MelodyNote *x = &melody->notes[from_a + k];
        const MelodyNote *y = &melody->notes[from_b + k];
        if (rational_cmp(position_sub(x->onset, position_mul(bar, (long long)a)),
                         position_sub(y->onset, position_mul(bar, (long long)b))) != 0 ||
            rational_cmp(x->duration, y->duration) != 0 ||
            (k > 0 && x->pitch - x[-1].pitch != y->pitch - y[-1].pitch)) {
            return false;
        }
    }
    return true;
}

/* Monotony (ch. XII): no three consecutive bars of two or more notes with the same rhythm and the same intervals. */
static bool monotony_holds(const MelodyCandidate *melody) {
    Rational bar = melody->context.metre.bar_length;
    if (!position_is_positive(bar)) {
        return true;
    }
    for (size_t first = 0; rational_cmp(position_mul(bar, (long long)(first + 3)), melody->context.length) <= 0; first++) {
        if (bars_alike(melody, bar, first, first + 1) && bars_alike(melody, bar, first + 1, first + 2)) {
            return false;
        }
    }
    return true;
}

unsigned melody_shaping_broken(const MelodyCandidate *melody) {
    return (ending_reduction_holds(melody) ? 0 : SHAPING_ENDING_REDUCTION) | (waves_hold(melody) ? 0 : SHAPING_WAVES) |
           (central_range_holds(melody) ? 0 : SHAPING_CENTRAL_RANGE) |
           (monotony_holds(melody) ? 0 : SHAPING_MONOTONY);
}

static void check_shaping(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    if (melody->params.plain_shaping) {
        return;
    }
    unsigned broken = melody_shaping_broken(melody);
    const MelodyWeights *weights = &melody->params.weights;
    verdict->soft_total += weights->ending_reduction + weights->waves + weights->central_range + weights->monotony;
    verdict->soft_held += (broken & SHAPING_ENDING_REDUCTION ? 0 : weights->ending_reduction) +
                          (broken & SHAPING_WAVES ? 0 : weights->waves) +
                          (broken & SHAPING_CENTRAL_RANGE ? 0 : weights->central_range) +
                          (broken & SHAPING_MONOTONY ? 0 : weights->monotony);
    verdict->shaping_broken |= broken;
}

/*
 * A candidate that ends at the seam (R-171) is also scored by three of its rules: no parallel fifths or octaves between
 * bass and melody across it, a melody that moves by step (or a leap answered), and a leading tone that goes up. The
 * progression is the harmony's. Nothing is rejected for them.
 */
static void check_seam(const MelodyCandidate *melody, PhraseVerdict *verdict) {
    const MelodyContext *context = &melody->context;
    SeamNotes notes;
    SeamHeard heard;
    if (!context->at_seam || melody->count == 0) {
        return;
    }
    memset(&notes, 0, sizeof(notes));
    notes.key = context->key;
    notes.bass_before = context->bass_count > 0 ? context->bass[context->bass_count - 1].pitch : -1;
    notes.bass_after = context->seam_bass;
    notes.melody_before = melody->notes[melody->count - 1].pitch;
    notes.melody_previous = melody->count > 1 ? melody->notes[melody->count - 2].pitch
                            : (context->lead_in_count > 0 ? context->lead_in[context->lead_in_count - 1] : -1);
    notes.melody_after = context->lead_out;
    notes.melody_next = context->seam_next;
    seam_hear_notes(&notes, &heard);
    add_soft(verdict, melody->params.weights.seam, (unsigned)heard.no_parallels + (unsigned)heard.step + (unsigned)heard.leading_tone, 3);
}

bool melody_nct_types(const MelodyCandidate *melody, unsigned *types) {
    PhraseVerdict verdict;
    if (!melody || !types || melody->count == 0 || melody->count > MELODY_MAX_NOTES) {
        return false;
    }
    memset(&verdict, 0, sizeof(verdict));
    memset(types, 0, melody->count * sizeof(*types));
    return check_tones(melody, &verdict, types, NULL);
}

bool melody_nct_survey(const MelodyCandidate *melody, unsigned *types, bool *unclassified) {
    PhraseVerdict verdict;
    if (!melody || !types || !unclassified || melody->count == 0 || melody->count > MELODY_MAX_NOTES) {
        return false;
    }
    memset(&verdict, 0, sizeof(verdict));
    memset(types, 0, melody->count * sizeof(*types));
    memset(unclassified, 0, melody->count * sizeof(*unclassified));
    return check_tones(melody, &verdict, types, unclassified);
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
        ok = check_tones(melody, verdict, NULL, NULL);
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
    /* The target with the phrase's climax writes the phrase's single highest note, when it generates notes (R-335): a target
       of realized notes alone cannot be bent to make a climax, and its tops count with the soft rules. */
    if (ok && !verdict->hard && melody_has_generated(melody) && melody->context.idea && melody->context.phrase_has_top &&
        melody_role(&melody->context, &melody->params) == MELODY_ROLE_CLIMAX) {
        int top = 0;
        for (size_t i = 0; i < melody->count; i++) {
            top = melody->notes[i].pitch > top ? melody->notes[i].pitch : top;
        }
        if (melody->relaxed) {
            add_soft(verdict, melody->params.weights.climax, top > melody->context.phrase_top, 1);
        } else if (top <= melody->context.phrase_top) {
            verdict->hard = "climax not above the phrase before it";
        }
    }
    /* The cadence arrives on its stable tone: the tonic for a PAC, the third for an IAC. */
    if (ok && !verdict->hard &&
        !(melody_cadence_mask(&melody->context) & (1u << util_pitch_class(melody->notes[melody->count - 1].pitch)))) {
        verdict->hard = "cadence without its stable tone";
    }
    if (ok && !verdict->hard) {
        unsigned held;
        unsigned total;
        melody_contour_score(melody, &held, &total);
        add_soft(verdict, melody->params.weights.contour, held, total);
        check_shaping(melody, verdict);
        check_seam(melody, verdict);
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
    shaping_reason(out->reason, sizeof(out->reason), "melody rules", verdict.shaping_broken);
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

bool melody_check_lane(const PiecePlan *plan, PlanSpan span, AnalysisKey key, const MelodyFillParams *params,
                       const char **violation) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    MelodyCandidate *melody = calloc(1, sizeof(*melody));
    bool ok = melody && violation;
    size_t found = 0;
    Rational origin = span.start;
    Rational phrase_end;
    if (ok) {
        found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, HARMONY_MAX_CHORDS);
        ok = found > 0 && found <= HARMONY_MAX_CHORDS &&
             plan_span_end(span, &phrase_end) == PLAN_OK &&
             melody_phrase_read(plan, plan_lane_span_owner(plan, ids[0]), span, key, &melody->context);
    }
    if (melody) {
        melody->params = params ? *params : melody_fill_params_default();
        melody->magic = MELODY_PAYLOAD_MAGIC;
    }
    /* Every note of the melody lane over the phrase, from the start of the phrase. */
    for (size_t i = 0; ok && i < found; i++) {
        Rational base = plan_lane_span_span(plan, ids[i]).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        Rational lane_end;
        PlanSpan lane_span = plan_lane_span_span(plan, ids[i]);
        /* A melody span may start before the phrase, with a pickup (PLAN_CONSTRAINT_PICKUP), and end where it ends. */
        ok = plan_span_end(lane_span, &lane_end) == PLAN_OK &&
             (plan_span_contains(span, lane_span) ||
              (rational_cmp(lane_span.start, span.start) < 0 && rational_cmp(lane_end, phrase_end) == 0));
        size_t first_note = melody->count;
        for (size_t n = 0; ok && n < notes->note_count; n++) {
            MelodyNote *note = &melody->notes[melody->count];
            Rational onset = position_add(position_sub(base, origin), notes->notes[n].offset);
            ok = melody->count < MELODY_MAX_NOTES;
            note->duration = notes->notes[n].duration;
            if (ok && rational_cmp(onset, POSITION_ZERO) < 0) {
                /* A note before the phrase is the pickup. */
                MelodyNote *pickup = &melody->pickup_notes[melody->pickup_count];
                ok = melody->pickup_count < MELODY_MAX_PICKUP;
                pickup->onset = onset;
                pickup->duration = note->duration;
                pickup->pitch = notes->notes[n].midi;
                pickup->motive_index = -1;
                melody->pickup_count += ok;
            } else if (ok) {
                note->onset = onset;
                note->pitch = notes->notes[n].midi;
                note->motive_index = -1;
                note->strong = melody_on_cell(&melody->context, note->onset);
                note->chord = melody_chord_at(&melody->context, note->onset);
                melody->count++;
            }
        }
        /* The motive the span says (its motive_ref) is what the checker held it to: its first notes are the motive's. */
        for (size_t c = 0; ok && c < plan_lane_span_constraint_count(plan, ids[i]); c++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, ids[i], c);
            LaneSpanId motive_span;
            if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF || !plan_has_node(plan, constraint->params.motive.motive) ||
                !plan_melody_span_of(plan, constraint->params.motive.motive, &motive_span)) {
                continue;
            }
            const PlanNotes *motive = &plan_lane_span_payload(plan, motive_span)->notes;
            if (motive->note_count == 0 || motive->note_count > MOTIVE_MAX_NOTES || melody->count - first_note < motive->note_count) {
                continue;
            }
            melody->context.motive_count = motive->note_count;
            for (size_t k = 0; k < motive->note_count; k++) {
                melody->context.motive_pitch[k] = motive->notes[k].midi;
                melody->notes[first_note + k].motive_index = (int)k;
            }
            melody->context.motive_waived |= melody_motive_waived(&melody->context);
            /* A motive's own figures are the framework's to name: every type is allowed in a melody that carries one. */
            melody->params.nct_types |= MELODY_NCT_PASSING | MELODY_NCT_NEIGHBOR | MELODY_NCT_APPOGGIATURA |
                                        MELODY_NCT_SUSPENSION | MELODY_NCT_ANTICIPATION | MELODY_NCT_RETARDATION |
                                        MELODY_NCT_ESCAPE | MELODY_NCT_DOUBLE_NEIGHBOR | MELODY_NCT_CHANGING_TONE;
        }
    }
    PhraseVerdict verdict;
    ok = ok && melody_judge(melody, &verdict);
    /* A lane that carries a motive and fails the strict rules passes when the relaxed rules hold (R-338): the melody
       operator composes it that way when no strict attempt does. */
    if (ok && verdict.hard) {
        bool carries = false;
        for (size_t i = 0; i < melody->count; i++) {
            carries = carries || melody->notes[i].motive_index >= 0;
        }
        if (carries) {
            melody->relaxed = true;
            ok = melody_judge(melody, &verdict);
        }
    }
    if (ok) {
        *violation = verdict.hard;
    }
    free(melody);
    return ok;
}
