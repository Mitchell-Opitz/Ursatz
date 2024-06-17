#include <ursatz_composer/voicing.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <harmonic_rules.h>
#include <harmony_resolution_rules.h>
#include <range_rules.h>
#include <voicing_pitch.h>
#include <voicing_rules.h>

#include "texture/voicing_internal.h"

static const VoicePart PART_OF[VOICING_PARTS] = {VOICE_PART_SOPRANO, VOICE_PART_ALTO, VOICE_PART_TENOR};

/* ------------------------------------------------------------ chords */

bool voicing_chord_valid(const VoicingContext *context, size_t chord, const int pitch[VOICING_PARTS]) {
    const PitchSpelling *voice[VOICING_VOICES];
    unsigned violations;
    if (chord >= context->chord_count || !context->chord_ok[chord]) {
        return false;
    }
    if (context->write_soprano && !(context->soprano_mask[chord] & (1u << (((pitch[VOICING_SOPRANO] % 12) + 12) % 12)))) {
        return false;
    }
    for (size_t part = 0; part < VOICING_PARTS; part++) {
        if (pitch[part] < 0 || pitch[part] > 127 || !(voice[part] = context->spell[chord][pitch[part]])) {
            return false;
        }
        /* What the voicing writes keeps to its range; a written melody or bass is not held to one. */
        if ((part != VOICING_SOPRANO || context->write_soprano) && !standard_voicing_in_range(PART_OF[part], voice[part])) {
            return false;
        }
    }
    if (!(voice[VOICING_BASS] = context->spell[chord][context->bass[chord]])) {
        return false;
    }
    StandardVoicingChord satb = {{voice[VOICING_SOPRANO], voice[VOICING_ALTO], voice[VOICING_TENOR], voice[VOICING_BASS]}};
    StandardVoicingInput input = {&satb,
                                  (const PitchSpelling *const *)context->tone[chord],
                                  context->tone_count[chord],
                                  context->inversion[chord],
                                  context->leading[chord],
                                  NULL};
    if (!standard_voicing_check(&input, &violations)) {
        return false;
    }
    violations &= ~(1u << STANDARD_VOICING_RULE_RANGE);
    /*
     * A written melody over a written bass can double a note between them that the rules forbid doubled (the leading
     * tone in a first-inversion V under a melody on the leading tone). That is not the voicing's to mend: when it is the
     * only note doubled, the doubling rule is not asked of the chord.
     */
    if ((violations & (1u << STANDARD_VOICING_RULE_DOUBLING)) && !context->write_soprano) {
        int forced = pitch[VOICING_SOPRANO] % 12;
        if (forced == context->bass[chord] % 12 && pitch[VOICING_ALTO] % 12 != forced && pitch[VOICING_TENOR] % 12 != forced &&
            pitch[VOICING_ALTO] % 12 != pitch[VOICING_TENOR] % 12) {
            violations &= ~(1u << STANDARD_VOICING_RULE_DOUBLING);
        }
    }
    return violations == 0;
}

/* ------------------------------------------------------- connections */

static const PitchSpelling *spelling_of(const VoicingContext *context, size_t chord, const int *pitches, size_t voice) {
    int pitch = voice == VOICING_BASS ? context->bass[chord] : pitches[voice];
    return pitch >= 0 && pitch < 128 ? context->spell[chord][pitch] : NULL;
}

/* The two voices, from one chord to the next, as a two-note-long pair of lines for standard-counterpoint. */
static bool pair_holds(const VoicingContext *context, size_t chord, const int from[VOICING_PARTS],
                       const int to[VOICING_PARTS], size_t upper, size_t lower, StandardCounterpointHarmonicRule rule) {
    const PitchSpelling *upper_notes[2] = {spelling_of(context, chord - 1, from, upper), spelling_of(context, chord, to, upper)};
    const PitchSpelling *lower_notes[2] = {spelling_of(context, chord - 1, from, lower), spelling_of(context, chord, to, lower)};
    StandardCounterpointLine upper_line = {upper_notes, 2};
    StandardCounterpointLine lower_line = {lower_notes, 2};
    StandardCounterpointPair pair = {&upper_line, &lower_line};
    if (!upper_notes[0] || !upper_notes[1] || !lower_notes[0] || !lower_notes[1]) {
        return false;
    }
    return standard_counterpoint_harmonic_rule_holds(rule, &pair);
}

/* The step of a spelling from the tonic, in the letters of the scale: do is 0, re 1, an octave 7. */
static int step_from(const PitchSpelling *tonic, const PitchSpelling *note) {
    return standard_voicing_step_number(note) - standard_voicing_step_number(tonic);
}

/*
 * The resolution of one upper voice from chord - 1 to chord, as standard-harmony reads it. A diatonic chord is read in
 * the key. An applied chord is read as the V or vii of the temporary key its target stands on, when it is followed by its
 * target; followed by anything else it is not judged, as the analysis does not judge it. False when the voice breaks a rule.
 */
static bool voice_resolves(const VoicingContext *context, size_t chord, const int from[VOICING_PARTS],
                           const int to[VOICING_PARTS], size_t voice, const char **why) {
    const PhraseChord *before = &context->chord[chord - 1];
    const PhraseChord *after = &context->chord[chord];
    HarmonyVoiceResolution resolution;
    const PitchSpelling *tonic = context->key_tonic;
    const PitchSpelling *from_note = spelling_of(context, chord - 1, from, voice);
    const PitchSpelling *to_note = spelling_of(context, chord, to, voice);
    memset(&resolution, 0, sizeof(resolution));
    if (!from_note || !to_note) {
        return true;
    }
    if (!before->applied) {
        resolution.mode = harmony_mode_of(context->key.mode);
        resolution.from = before->chord;
        resolution.to = after->applied ? (HarmonyChord){0} : after->chord;
        if (after->applied) {
            resolution.to.degree = harmony_applied_root_degree(resolution.mode, &after->applied_chord);
        }
    } else {
        bool leading = before->applied_chord.kind == APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE;
        bool minor;
        if (after->applied || after->chord.degree != before->applied_chord.target) {
            return true;
        }
        /* The target's triad is minor when its third is three semitones above its root. */
        minor = context->tone_count[chord] >= 2 && (context->pcs[chord][1] - context->pcs[chord][0] + 12) % 12 == 3;
        resolution.mode = minor ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        resolution.from.degree = leading ? 7 : 5;
        resolution.from.inversion = before->applied_inversion;
        resolution.from.seventh = context->tone_count[chord - 1] == 4;
        resolution.from.leading_tone = leading && minor;
        resolution.from.half_diminished = leading && before->applied_chord.quality == HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH;
        resolution.to.degree = 1;
        resolution.to.inversion = after->chord.inversion;
        tonic = context->tone[chord][0];
    }
    resolution.from_step = step_from(tonic, from_note);
    resolution.to_step = step_from(tonic, to_note);
    resolution.outer = voice == VOICING_SOPRANO;
    if (context->tone_count[chord - 1] == 4) {
        bool all = true;
        for (size_t k = 0; k < 4; k++) {
            bool sounds = context->bass[chord - 1] % 12 == context->pcs[chord - 1][k];
            for (size_t v = 0; v < VOICING_PARTS; v++) {
                sounds = sounds || from[v] % 12 == context->pcs[chord - 1][k];
            }
            all = all && sounds;
        }
        resolution.complete_dominant_seventh = all;
    }
    if (!harmony_resolution_valid(&resolution)) {
        return true;
    }
    if (!harmony_resolution_rule_holds(HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN, &resolution)) {
        if (why) {
            *why = "a chordal seventh that does not resolve";
        }
        return false;
    }
    if (!harmony_resolution_rule_holds(HARMONY_RESOLUTION_LEADING_TONE_RESOLVES, &resolution)) {
        if (why) {
            *why = "a leading tone that does not resolve";
        }
        return false;
    }
    return true;
}

bool voicing_connection_ok(const VoicingContext *context, size_t chord, const int from[VOICING_PARTS],
                           const int to[VOICING_PARTS], const char **why) {
    if (chord == 0 || chord >= context->chord_count) {
        return false;
    }
    for (size_t upper = 0; upper < VOICING_VOICES; upper++) {
        for (size_t lower = upper + 1; lower < VOICING_VOICES; lower++) {
            /* A written melody over a written bass makes the motion between them; the voicing cannot change it. */
            if (!context->write_soprano && upper == VOICING_SOPRANO && lower == VOICING_BASS) {
                continue;
            }
            if (!pair_holds(context, chord, from, to, upper, lower, STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS)) {
                if (why) {
                    *why = "parallel fifths";
                }
                return false;
            }
            if (!pair_holds(context, chord, from, to, upper, lower, STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES)) {
                if (why) {
                    *why = "parallel octaves";
                }
                return false;
            }
        }
    }
    /* Direct fifths and octaves in the outer voices can be avoided only when the soprano is the voicing's to write. */
    if (context->write_soprano &&
        !pair_holds(context, chord, from, to, VOICING_SOPRANO, VOICING_BASS, STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS)) {
        if (why) {
            *why = "direct fifths or octaves in the outer voices";
        }
        return false;
    }
    /* A written melody is not the voicing's to mend: what it does with a seventh or a leading tone is left to it. */
    for (size_t voice = context->write_soprano ? VOICING_SOPRANO : VOICING_ALTO; voice < VOICING_PARTS; voice++) {
        if (!voice_resolves(context, chord, from, to, voice, why)) {
            return false;
        }
    }
    return true;
}

long voicing_connection_cost(const int from[VOICING_PARTS], const int to[VOICING_PARTS]) {
    long cost = 0;
    for (size_t voice = 0; voice < VOICING_PARTS; voice++) {
        long moved = to[voice] > from[voice] ? to[voice] - from[voice] : from[voice] - to[voice];
        cost += moved + (moved > VOICING_LEAP_SEMITONES ? VOICING_LEAP_COST : 0);
    }
    return cost;
}

bool voicing_connection_smooth(const int from[VOICING_PARTS], const int to[VOICING_PARTS]) {
    for (size_t voice = 0; voice < VOICING_PARTS; voice++) {
        int moved = to[voice] > from[voice] ? to[voice] - from[voice] : from[voice] - to[voice];
        if (moved > VOICING_SMOOTH_SEMITONES) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------ judging */

void voicing_judge(const VoicingContext *context, const VoicingCandidate *candidate, VoicingVerdict *verdict) {
    memset(verdict, 0, sizeof(*verdict));
    if (candidate->chord_count != context->chord_count) {
        verdict->hard = "not a voicing of every chord";
        return;
    }
    for (size_t i = 0; i < context->chord_count && !verdict->hard; i++) {
        if (!context->chord_ok[i]) {
            verdict->hard = "a chord that the bass or the melody leaves no voicing for";
        } else if (!context->write_soprano && candidate->pitch[i][VOICING_SOPRANO] != context->soprano[i]) {
            verdict->hard = "a soprano that is not the written melody";
        } else if (!voicing_chord_valid(context, i, candidate->pitch[i])) {
            verdict->hard = "a chord that breaks standard-voicing";
        }
    }
    for (size_t i = 1; i < context->chord_count && !verdict->hard; i++) {
        if (!voicing_connection_ok(context, i, candidate->pitch[i - 1], candidate->pitch[i], &verdict->hard)) {
            if (!verdict->hard) {
                verdict->hard = "a connection standard-counterpoint refuses";
            }
        } else {
            verdict->total++;
            verdict->held += voicing_connection_smooth(candidate->pitch[i - 1], candidate->pitch[i]) ? 1 : 0;
        }
    }
}

bool voicing_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                     const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, VOICING_OPERATOR_NAME) != 0) {
        return harmony_phrase_checker(plan, target, operator_, candidate, context, out);
    }
    const VoicingCandidate *voicing = candidate->payload;
    VoicingContext *read = calloc(1, sizeof(*read));
    VoicingVerdict verdict;
    bool ok = read && voicing && voicing->magic == VOICING_PAYLOAD_MAGIC && target.kind == FILL_TARGET_LANE_SPAN &&
              voicing_read(plan, (LaneSpanId)target.id, read);
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (ok) {
        voicing_judge(read, voicing, &verdict);
        out->hard_violation = verdict.hard != NULL;
        if (verdict.hard) {
            snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        } else {
            out->score_numerator = verdict.total ? verdict.held : 1;
            out->score_denominator = verdict.total ? verdict.total : 1;
        }
    }
    voicing_context_free(read);
    free(read);
    return ok;
}
