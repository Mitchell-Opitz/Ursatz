#include <ursatz_composer/harmony_bass.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

#define DEFAULT_RANGE_LOW 40
#define DEFAULT_RANGE_HIGH 60
#define DEFAULT_SUBDIVISION 250
#define DEFAULT_STEPWISE 750
#define CHANCE_SCALE 1000

/* Bass candidates tried per call, and kept; ones that repeat an earlier one are dropped. */
#define BASS_ATTEMPTS 16
#define BASS_KEEP 6
/* The most notes one chord is split into. */
#define MAX_NOTES_PER_CHORD 4

HarmonyBassParams harmony_bass_params_default(void) {
    HarmonyBassParams params = {DEFAULT_RANGE_LOW, DEFAULT_RANGE_HIGH, DEFAULT_SUBDIVISION, DEFAULT_STEPWISE};
    return params;
}

static const HarmonyBassParams *params_of(void *context) {
    static const HarmonyBassParams defaults = {DEFAULT_RANGE_LOW, DEFAULT_RANGE_HIGH, DEFAULT_SUBDIVISION,
                                               DEFAULT_STEPWISE};
    return context ? context : &defaults;
}

static unsigned chance_of(unsigned value) {
    return value > CHANCE_SCALE ? CHANCE_SCALE : value;
}

bool harmony_chord_bass_pc(const PlanKey *key, const PhraseChord *chord, int *pc) {
    int intervals[4];
    size_t count;
    int root;
    int member;
    if (chord->applied) {
        if (!harmony_applied_intervals(&chord->applied_chord, intervals, &count)) {
            return false;
        }
        root = harmony_applied_root_semitone(harmony_mode_of(key->mode), &chord->applied_chord);
        member = 0;
    } else {
        if (!harmony_diatonic_intervals(key->mode, &chord->chord, intervals, &count)) {
            return false;
        }
        /* The chord on the leading tone of a minor key stands on the raised seventh degree. */
        root = key->mode == PLAN_MODE_MINOR && chord->chord.leading_tone
                   ? 11
                   : harmony_applied_scale_semitone(harmony_mode_of(key->mode), chord->chord.degree);
        member = chord->chord.inversion;
    }
    if (root < 0 || member < 0 || (size_t)member >= count) {
        return false;
    }
    *pc = (key->tonic_pitch_class + root + intervals[member]) % 12;
    return true;
}

bool harmony_bass_context_read(const PiecePlan *plan, LaneSpanId target, BassContext *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_BASS ||
        plan_lane_span_voice(plan, target) != 0) {
        return false;
    }
    BassContext context;
    memset(&context, 0, sizeof(context));
    context.target = target;
    context.owner = plan_lane_span_owner(plan, target);
    context.span = plan_lane_span_span(plan, target);
    if (!harmony_find_key(plan, target, context.owner, context.span, &context.key) ||
        !harmony_read_lane(plan, context.span, context.key.mode, context.chords, &context.count,
                           &context.metre)) {
        return false;
    }
    /* The harmony is written: no chord span under the bass is still open. */
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, context.span, ids, HARMONY_MAX_CHORDS);
    for (size_t i = 0; i < found; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
    }
    *out = context;
    return true;
}

/* The pitches in range with this pitch class, lowest first. */
static size_t pitches_in_range(int pc, int low, int high, int *out, size_t capacity) {
    size_t count = 0;
    for (int pitch = low; pitch <= high && count < capacity; pitch++) {
        if (((pitch % 12) + 12) % 12 == pc) {
            out[count++] = pitch;
        }
    }
    return count;
}

/* The first note of a chord: the octave nearest the note before with the stepwise chance, otherwise any. */
static int choose_pitch(const int *options, size_t count, int previous, unsigned stepwise, FillRng *rng) {
    bool near = fill_rng_below(rng, CHANCE_SCALE) < stepwise;
    size_t random = (size_t)fill_rng_below(rng, count);
    if (previous < 0 || !near) {
        return options[random];
    }
    size_t best = 0;
    for (size_t i = 1; i < count; i++) {
        int here = options[i] > previous ? options[i] - previous : previous - options[i];
        int then = options[best] > previous ? options[best] - previous : previous - options[best];
        if (here < then) {
            best = i;
        }
    }
    return options[best];
}

/* Builds one bass line for the harmony, or false when a chord has no pitch in range. */
static bool build_bass(const BassContext *context, const HarmonyBassParams *params, FillRng *rng,
                       BassCandidate *out) {
    unsigned subdivision = chance_of(params->subdivision);
    unsigned stepwise = chance_of(params->stepwise);
    int previous = -1;
    memset(out, 0, sizeof(*out));
    out->context = *context;
    out->params = *params;
    for (size_t i = 0; i < context->count; i++) {
        const PhraseChord *chord = &context->chords[i];
        int pc;
        int options[16];
        if (!harmony_chord_bass_pc(&context->key, chord, &pc)) {
            return false;
        }
        size_t option_count = pitches_in_range(pc, params->range_low, params->range_high, options, 16);
        if (option_count == 0) {
            return false;
        }

        /* How many notes the chord is split into, and how long each is. */
        size_t n = chord->cells;
        size_t parts = 1;
        bool split = fill_rng_below(rng, CHANCE_SCALE) < subdivision;
        size_t most = n < MAX_NOTES_PER_CHORD ? n : MAX_NOTES_PER_CHORD;
        if (split && most >= 2) {
            parts = 2 + (size_t)fill_rng_below(rng, most - 1);
        }
        size_t lengths[MAX_NOTES_PER_CHORD];
        for (size_t k = 0; k < parts; k++) {
            lengths[k] = 1;
        }
        for (size_t extra = n - parts; extra > 0; extra--) {
            lengths[fill_rng_below(rng, parts)]++;
        }

        int pitch = choose_pitch(options, option_count, previous, stepwise, rng);
        for (size_t k = 0; k < parts; k++) {
            if (k > 0) {
                /* A later note repeats the pitch or moves it an octave, to another pitch of the chord's member. */
                int moved[2];
                size_t moves = 0;
                for (size_t o = 0; o < option_count; o++) {
                    if (options[o] == pitch + 12 || options[o] == pitch - 12) {
                        moved[moves++] = options[o];
                    }
                }
                size_t pick = (size_t)fill_rng_below(rng, 2);
                if (pick == 1 && moves > 0) {
                    pitch = moved[(size_t)fill_rng_below(rng, moves)];
                }
            }
            if (out->note_count >= BASS_MAX_NOTES) {
                return false;
            }
            out->notes[out->note_count].pitch = pitch;
            out->notes[out->note_count].cells = lengths[k];
            out->notes[out->note_count].chord = i;
            out->note_count++;
        }
        previous = pitch;
    }
    return true;
}

static bool same_line(const BassCandidate *a, const BassCandidate *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].pitch != b->notes[i].pitch || a->notes[i].cells != b->notes[i].cells) {
            return false;
        }
    }
    return true;
}

static void line_label(const BassCandidate *line, char *label, size_t size) {
    size_t used = 0;
    label[0] = '\0';
    for (size_t i = 0; i < line->note_count && used + 1 < size; i++) {
        int written = snprintf(label + used, size - used, i ? " %d" : "%d", line->notes[i].pitch);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }
}

static bool bass_applies(const PiecePlan *plan, FillTarget target, void *context) {
    BassContext read;
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && harmony_bass_context_read(plan, (LaneSpanId)target.id, &read);
}

static bool bass_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                          FillCandidateList *out) {
    const HarmonyBassParams *params = params_of(context);
    BassContext read;
    if (target.kind != FILL_TARGET_LANE_SPAN || !harmony_bass_context_read(plan, (LaneSpanId)target.id, &read)) {
        return false;
    }
    BassCandidate *kept[BASS_KEEP];
    size_t kept_count = 0;
    BassCandidate *scratch = malloc(sizeof(*scratch));
    if (!scratch) {
        return false;
    }
    bool ok = true;
    for (size_t attempt = 0; ok && attempt < BASS_ATTEMPTS && kept_count < BASS_KEEP; attempt++) {
        PhraseVerdict verdict;
        if (!build_bass(&read, params, rng, scratch)) {
            break;
        }
        /* Only lines that pass the melodic rules are proposed; none passing leaves the span open. */
        if (!harmony_bass_judge(scratch, &verdict)) {
            ok = false;
            break;
        }
        if (verdict.hard) {
            continue;
        }
        bool repeat = false;
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_line(kept[i], scratch);
        }
        if (repeat) {
            continue;
        }
        BassCandidate *payload = malloc(sizeof(*payload));
        char label[FILL_LABEL_MAX];
        if (!payload) {
            ok = false;
            break;
        }
        *payload = *scratch;
        line_label(payload, label, sizeof(label));
        ok = fill_candidates_add(out, label, payload, free);
        kept[kept_count++] = payload;
    }
    free(scratch);
    return ok;
}

/* Writes the notes of chord i as the payload of its span. */
static PlanResult chord_notes(const BassCandidate *line, size_t chord, PlanNote *notes, size_t *count) {
    PlanTime cell = line->context.metre.cell_length;
    size_t used = 0;
    size_t at = 0;
    for (size_t i = 0; i < line->note_count; i++) {
        if (line->notes[i].chord != chord) {
            continue;
        }
        PlanResult result = plan_time_make(cell.num * (long)at, cell.den, &notes[used].offset);
        if (result == PLAN_OK) {
            result = plan_time_make(cell.num * (long)line->notes[i].cells, cell.den, &notes[used].duration);
        }
        if (result != PLAN_OK) {
            return result;
        }
        notes[used].midi_pitch = line->notes[i].pitch;
        at += line->notes[i].cells;
        used++;
    }
    *count = used;
    return PLAN_OK;
}

/*
 * The target span becomes the first chord's bass (it keeps its id), and one span is added after it for
 * each chord that follows, so the bass spans line up with the chord spans and tile the phrase.
 */
static PlanResult bass_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const BassCandidate *line = candidate ? candidate->payload : NULL;
    BassContext read;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !line || line->note_count == 0) {
        return PLAN_ERR_NULL;
    }
    LaneSpanId first = (LaneSpanId)target.id;
    if (!harmony_bass_context_read(plan, first, &read) || read.count != line->context.count) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    for (size_t i = 0; i < read.count; i++) {
        if (read.chords[i].cells != line->context.chords[i].cells) {
            return PLAN_ERR_BAD_PAYLOAD;
        }
    }

    PlanResult result = PLAN_OK;
    size_t cell = 0;
    for (size_t i = 0; result == PLAN_OK && i < read.count; i++) {
        PlanNote notes[MAX_NOTES_PER_CHORD];
        size_t note_count = 0;
        PlanSpan span;
        PlanLanePayload payload;
        result = chord_notes(line, i, notes, &note_count);
        if (result == PLAN_OK && note_count == 0) {
            result = PLAN_ERR_BAD_PAYLOAD;
        }
        if (result == PLAN_OK) {
            result = harmony_cells_span(read.span.start, read.metre.cell_length, cell, read.chords[i].cells, &span);
        }
        if (result != PLAN_OK) {
            break;
        }
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = notes;
        payload.notes.note_count = note_count;
        if (i == 0) {
            result = plan_lane_span_resize(plan, first, span.length);
            if (result == PLAN_OK) {
                result = plan_lane_span_write(plan, first, &payload);
            }
        } else {
            LaneSpanId id;
            result = plan_add_lane_span(plan, read.owner, PLAN_LANE_BASS, 0, span, &payload, &id);
            if (result == PLAN_OK) {
                result = plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED);
            }
        }
        cell += read.chords[i].cells;
    }
    return result;
}

FillResult harmony_bass_register(FillOperatorRegistry *registry, const HarmonyBassParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_BASS_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan bass lane span over a written harmony lane";
    operator_.generator.supported_outputs = "PiecePlan bass lane spans";
    operator_.generator.configuration_description = "bass range, subdivision and stepwise chances in thousandths";
    operator_.generator.constraints_description = "chord bass members, standard-counterpoint melodic rules";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = bass_applies;
    operator_.generate = bass_generate;
    operator_.apply = bass_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
