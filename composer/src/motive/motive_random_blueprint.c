#include <stdlib.h>
#include <string.h>

#include <analysis/motive/motive_sketch.h>
#include <plan/plan_common.h>
#include <plan/plan_state.h>
#include <theory/knowledge/pattern_entry.h>

#include <ursatz_composer/motive_blueprint.h>
#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_random_internal.h"
#include "util/util.h"

/*
 * The blueprint strategy of the random motive (P69c B4, R-753): a two-bar motif drawn the way the generator draws a theme's bars (R-751), from
 * the K8 slot expectations of the sentence's basic idea and the blueprint preferences (R-750): its chords one of the idea's harmony
 * alternatives; a K11 rhythm cell for bar 0 by the weights of jobs.basic; bar 1 a cell, a variant of bar 0 or a contrast to it; the
 * downbeat of bar 0 a ninth, an appoggiatura or neither by the sounds weights; the notes by composer_blueprint_motive.
 */

#define SOUND_NONE_WEIGHT 2
#define MOTIVE_BARS 2

/* The harmony cells of a bar the K8 chord texts count (one per accent beat of a simple meter, one per beat group of a compound one). */
static size_t cells_per_bar(const PlanMeter *meter) {
    if (meter->beat_unit == 8 && meter->beats_per_bar == 6) {
        return 2;
    }
    if (meter->beat_unit == 4 && meter->beats_per_bar == 4) {
        return 2;
    }
    return meter->beat_unit == 4 && (meter->beats_per_bar == 2 || meter->beats_per_bar == 3) ? 1 : 0;
}

/*
 * The two bars of an alternative as one own-harmony text from the motive's start ("I IV@4 V@6"), the chords' onsets in beats of the beat
 * unit; false when a bar has more chords than the meter has cells.
 */
static bool alternative_text(const ThemeHarmonyTemplate *template, const PlanMeter *meter, size_t cells, char *out, size_t size) {
    if (template->restate || template->chord_count != MOTIVE_BARS) {
        return false;
    }
    long cell_beats = meter->beats_per_bar / (long)cells;
    out[0] = '\0';
    for (size_t bar = 0; bar < MOTIVE_BARS; bar++) {
        char copy[96];
        size_t chords = 0;
        snprintf(copy, sizeof copy, "%s", template->chords[bar]);
        for (char *token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
            char *mark = strchr(token, '@');
            long cell = mark ? strtol(mark + 1, NULL, 10) : 1;
            size_t used = strlen(out);
            if (cell < 1 || (size_t)cell > cells || ++chords > cells) {
                return false;
            }
            if (mark) {
                *mark = '\0';
            }
            long at = (long)bar * meter->beats_per_bar + (cell - 1) * cell_beats;
            if (used == 0) {
                snprintf(out, size, "%s", token);
            } else {
                snprintf(out + used, size - used, " %s@%ld", token, at);
            }
        }
    }
    return out[0] != '\0';
}

/* The unit's jobs as the weights of jobs.basic: the job bits of K8 in the order the preferences list them. */
static unsigned job_weight(const BlueprintPreferences *preferences, unsigned theme_job) {
    int job = theme_job == THEME_JOB_RUNNING ? BLUEPRINT_JOB_RUNNING
              : theme_job == THEME_JOB_LONG ? BLUEPRINT_JOB_LONG
              : theme_job == THEME_JOB_SYNCOPATED ? BLUEPRINT_JOB_SYNCOPATED
              : theme_job == THEME_JOB_VARIANT ? BLUEPRINT_JOB_VARIANT
                                               : BLUEPRINT_JOB_CONTRAST;
    return preferences->jobs[BLUEPRINT_FUNCTION_BASIC][job];
}

static unsigned pattern_job(unsigned theme_job) {
    return theme_job == THEME_JOB_RUNNING ? PATTERN_JOB_RUNNING : theme_job == THEME_JOB_LONG ? PATTERN_JOB_LONG : PATTERN_JOB_SYNCOPATED;
}

/*
 * One bar's rhythm from the unit's jobs: a cell job draws a K11 cell, a variant is the variant of `before` and a contrast the contrast to it
 * (bar 0 has no `before`, so it takes neither). The jobs with weight 0 or no cell are not candidates; none left takes the lowest bit.
 */
static bool draw_bar(const MotiveRandomOptions *options, const BlueprintPreferences *preferences, unsigned jobs, const PlanNote *before,
                     size_t before_count, FillRng *rng, PlanNote *out, size_t *count) {
    unsigned candidates[5];
    unsigned weights[5];
    size_t have = 0;
    bool before_half = before_count > 0 && before[before_count - 1].duration.num == 1 && before[before_count - 1].duration.den == 2;
    for (unsigned bit = THEME_JOB_VARIANT; bit <= THEME_JOB_SYNCOPATED; bit <<= 1) {
        bool cell = bit != THEME_JOB_VARIANT && bit != THEME_JOB_CONTRAST;
        PlanNote probe[COMPOSER_RHYTHM_NOTES_MAX];
        size_t probe_count = 0;
        if ((jobs & bit) == 0 || job_weight(preferences, bit) == 0 || (!cell && before == NULL) ||
            (cell && !composer_rhythm_draw_cell(options->knowledge, &options->meter, pattern_job(bit), before_half,
                                                preferences->sounds[BLUEPRINT_SOUND_LONG_WEAK], false, rng, probe, &probe_count))) {
            continue;
        }
        candidates[have] = bit;
        weights[have++] = job_weight(preferences, bit);
    }
    size_t pick = composer_draw_weighted(rng, weights, have);
    unsigned chosen = pick < have ? candidates[pick] : jobs & (~jobs + 1u);
    if (chosen == THEME_JOB_VARIANT) {
        return before && composer_rhythm_variant(before, before_count, rng, out, count);
    }
    if (chosen == THEME_JOB_CONTRAST) {
        return before && composer_rhythm_contrast(options->knowledge, &options->meter, before, before_count, rng, out, count);
    }
    return composer_rhythm_draw_cell(options->knowledge, &options->meter, pattern_job(chosen), before_half,
                                     preferences->sounds[BLUEPRINT_SOUND_LONG_WEAK], true, rng, out, count) != NULL;
}

/* The pitch of a pitch class nearest the middle of the range, moved by octaves to lie in [low, high]; -1 for none. */
static int place_near(int pc, int middle, int low, int high) {
    int pitch = pc + 12 * ((middle - pc + (middle >= pc ? 6 : -6)) / 12);
    while (pitch > high) {
        pitch -= 12;
    }
    while (pitch < low) {
        pitch += 12;
    }
    return pitch >= low && pitch <= high ? pitch : -1;
}

/* The downbeat of bar 0 as a sound: an anchor and an appoggiatura event, or none. */
static void draw_downbeat(const MotiveRandomOptions *options, const BlueprintPreferences *preferences, const SpelledKey *key,
                          const MotiveSketchChord *chord, int low, int high, FillRng *rng, BlueprintMotiveRequest *request) {
    unsigned weights[3] = {preferences->sounds[BLUEPRINT_SOUND_NINTH_DOWNBEAT], preferences->sounds[BLUEPRINT_SOUND_APPOGGIATURA],
                           SOUND_NONE_WEIGHT};
    size_t pick = composer_draw_weighted(rng, weights, 3);
    Motive keyed;
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    motive_init(&keyed, options->meter);
    motive_set_key(&keyed, key, MOTIVE_SKETCH_KEY_GIVEN);
    size_t members = motive_sketch_chord_tones(&keyed, chord, pcs);
    int middle = (low + high) / 2;
    int anchor = -1;
    if (pick >= 2 || members == 0) {
        return;
    }
    if (pick == 0) {
        int root = place_near(pcs[0], middle, low, high - 2);
        anchor = root < 0 ? -1 : root + 2;
    } else {
        int tone = -1;
        for (size_t m = 0; m < members; m++) {
            int pitch = place_near(pcs[m], middle, low, high - 3);
            tone = pitch >= 0 && (tone < 0 || abs(pitch - middle) < abs(tone - middle)) ? pitch : tone;
        }
        int step = 0;
        int rel = tone < 0 ? 0 : ((tone - spelled_key_tonic_pitch_class(key)) % 12 + 12) % 12;
        for (int up = 1; tone >= 0 && step == 0 && up <= 3; up++) {
            step = pitch_material_contains(key->material, rel + up) ? up : 0;
        }
        anchor = tone < 0 ? -1 : tone + (step ? step : 2);
    }
    if (anchor >= low && anchor <= high) {
        request->anchors[request->anchor_count++] = (PlanAnchor){{0, 1}, anchor};
        request->events[request->event_count++] = (PlanEvent){{0, 1}, PLAN_EVENT_APPOGGIATURA};
    }
}

bool motive_random_blueprint_candidate(const MotiveRandomOptions *options, const SpelledKey *key, FillRng *rng, Motive *out, bool *found) {
    BlueprintPreferences defaults;
    const BlueprintPreferences *preferences = options->blueprint;
    *found = false;
    if (!preferences) {
        blueprint_preferences_default(&defaults);
        preferences = &defaults;
    }
    const ThemeType *type = knowledge_snapshot_theme_type_of(options->knowledge, "presentation", "continuation");
    size_t cells = cells_per_bar(&options->meter);
    Rational bar_length;
    if (!type || type->idea_count == 0 || cells == 0 || plan_meter_bar_length(&options->meter, &bar_length) != PLAN_OK) {
        return true;
    }
    const ThemeTypeExpect *expect = &type->ideas[0].expect;
    char texts[THEME_EXPECT_HARMONY_MAX][96];
    size_t fits[THEME_EXPECT_HARMONY_MAX];
    size_t fit_count = 0;
    for (size_t k = 0; k < expect->harmony_count; k++) {
        if (alternative_text(&expect->harmony[k], &options->meter, cells, texts[k], sizeof texts[k])) {
            fits[fit_count++] = k;
        }
    }
    if (fit_count == 0 || expect->job_count != MOTIVE_BARS) {
        return true;
    }
    const char *harmony = texts[fits[fill_rng_below(rng, fit_count)]];
    BlueprintMotiveRequest *request = calloc(1, sizeof(*request));
    if (!request) {
        return false;
    }
    request->key = *key;
    request->meter = options->meter;
    request->range_low = 60;
    request->range_high = 60 + (int)preferences->range;
    MotiveError detail;
    PlanNote bars[MOTIVE_BARS][COMPOSER_RHYTHM_NOTES_MAX];
    size_t counts[MOTIVE_BARS] = {0, 0};
    bool ok = motive_harmony_parse_in(options->knowledge, harmony, &options->meter, request->chords, &request->chord_count, &detail) == MOTIVE_OK &&
              draw_bar(options, preferences, expect->jobs[0], NULL, 0, rng, bars[0], &counts[0]) &&
              draw_bar(options, preferences, expect->jobs[1], bars[0], counts[0], rng, bars[1], &counts[1]);
    for (size_t bar = 0; ok && bar < MOTIVE_BARS; bar++) {
        Rational start;
        ok = rational_mul_int(bar_length, (long long)bar, &start) == RATIONAL_OK;
        for (size_t i = 0; ok && i < counts[bar] && request->note_count < MOTIVE_MAX_NOTES; i++) {
            ok = rational_add(start, bars[bar][i].offset, &request->onsets[request->note_count]) == RATIONAL_OK;
            request->durations[request->note_count++] = bars[bar][i].duration;
        }
    }
    if (ok) {
        draw_downbeat(options, preferences, key, &request->chords[0], request->range_low, request->range_high, rng, request);
    }
    Motive motive;
    bool drawn = ok && request->note_count > 0 && composer_blueprint_motive(NULL, request, rng, &motive, &detail) == MOTIVE_OK;
    free(request);
    const char *fault = NULL;
    if (drawn) {
        if (!motive_melodic_fault(&motive, &fault)) {
            return false;
        }
        *out = motive;
        *found = fault == NULL;
    }
    return true;
}
