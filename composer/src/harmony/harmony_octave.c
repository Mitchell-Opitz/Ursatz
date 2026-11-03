#include <ursatz_composer/harmony_octave.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

/* Sequences tried per call, and candidates kept. */
#define OCTAVE_ATTEMPTS 32
#define OCTAVE_KEEP 6
/* The chords that have a given bass pitch class, at most. */
#define MAX_OPTIONS_PER_PC 24

/* One run of the same bass pitch class: where it starts, how many cells it lasts. */
typedef struct {
    int pc;
    size_t cells;
} BassRun;

typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKey key;
    PhraseMetre metre;
    size_t count;
    BassRun runs[HARMONY_MAX_CHORDS];
} OctaveContext;

typedef struct {
    OctaveContext context;
    PhraseChord chords[HARMONY_MAX_CHORDS];
} OctaveCandidate;

/* The cells of the metre a note covers exactly, from its start to its end; none is not allowed. */
static bool cells_of(const PhraseMetre *metre, PlanTime start, PlanTime length, size_t *cells) {
    size_t first;
    PlanSpan note = {start, length};
    return length.num > 0 && harmony_metre_find_cells(metre, note, &first, cells);
}

/* The scale degree (1 to 7) of a pitch class in the key, 0 when it is not in the scale. */
static int degree_of(const PlanKey *key, int pc) {
    static const int MAJOR[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};
    static const int MINOR[12] = {1, 0, 2, 3, 0, 4, 0, 5, 6, 0, 7, 7};
    int relative = ((pc - key->tonic_pitch_class) % 12 + 12) % 12;
    return key->mode == PLAN_MODE_MINOR ? MINOR[relative] : MAJOR[relative];
}

static bool is_step(int a, int b) {
    int distance = a > b ? a - b : b - a;
    return distance == 1 || distance == 6;
}

static bool octave_context_read(const PiecePlan *plan, LaneSpanId target, OctaveContext *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_HARMONY ||
        plan_lane_span_voice(plan, target) != 0 ||
        plan_lane_span_find_constraint(plan, target, PLAN_CONSTRAINT_EMBELLISH) != NULL) {
        return false;
    }
    OctaveContext context;
    memset(&context, 0, sizeof(context));
    context.target = target;
    context.owner = plan_lane_span_owner(plan, target);
    context.span = plan_lane_span_span(plan, target);
    if (!harmony_find_key(plan, target, context.owner, context.span, &context.key) ||
        !harmony_read_metre(plan, context.span, &context.metre)) {
        return false;
    }

    /* The bass: Given spans tiling the span, notes on cell boundaries, one after another. */
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, context.span, ids, HARMONY_MAX_CHORDS);
    if (found == 0 || found > HARMONY_MAX_CHORDS) {
        return false;
    }
    PlanTime position = context.span.start;
    int previous_degree = 0;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        PlanTime end;
        if (plan_lane_span_state(plan, ids[i]) != PLAN_STATE_GIVEN || plan_time_compare(span.start, position) != 0 ||
            plan_span_end(span, &end) != PLAN_OK) {
            return false;
        }
        PlanTime at = {0, 1};
        for (size_t n = 0; n < notes->note_count; n++) {
            const PlanNote *note = &notes->notes[n];
            size_t cells;
            PlanTime note_start;
            if (plan_time_compare(note->offset, at) != 0 || plan_time_add(span.start, at, &note_start) != PLAN_OK ||
                !cells_of(&context.metre, note_start, note->duration, &cells) || cells == 0 ||
                plan_time_add(at, note->duration, &at) != PLAN_OK) {
                return false;
            }
            int pc = ((note->midi_pitch % 12) + 12) % 12;
            int degree = degree_of(&context.key, pc);
            if (degree == 0) {
                return false;
            }
            if (context.count > 0 && context.runs[context.count - 1].pc == pc) {
                context.runs[context.count - 1].cells += cells;
                continue;
            }
            if (context.count > 0 && !is_step(previous_degree, degree)) {
                return false;
            }
            if (context.count >= HARMONY_MAX_CHORDS) {
                return false;
            }
            context.runs[context.count].pc = pc;
            context.runs[context.count].cells = cells;
            context.count++;
            previous_degree = degree;
        }
        if (plan_time_compare(at, span.length) != 0) {
            return false;
        }
        position = end;
    }
    PlanTime span_end;
    size_t total = 0;
    for (size_t i = 0; i < context.count; i++) {
        total += context.runs[i].cells;
    }
    if (plan_span_end(context.span, &span_end) != PLAN_OK || plan_time_compare(position, span_end) != 0 ||
        total != context.metre.cells || context.count < 2) {
        return false;
    }
    /* The rule's scale starts and ends on the tonic. */
    if (context.runs[0].pc != context.key.tonic_pitch_class ||
        context.runs[context.count - 1].pc != context.key.tonic_pitch_class) {
        return false;
    }
    *out = context;
    return true;
}

/*
 * The chords that may stand over a bass pitch class: the triads in root position and first inversion
 * (the diminished ones in first inversion only) and V7 in every inversion. The key says which pitch
 * class each has in the bass, so this is a search through standard-harmony's chords, not a table.
 */
static size_t options_for(const PlanKey *key, int pc, PhraseChord *out) {
    size_t count = 0;
    PhraseChord chord;
    for (int degree = 1; degree <= 7; degree++) {
        for (int inversion = 0; inversion <= 1; inversion++) {
            for (int variant = 0; variant < 2; variant++) {
                int bass;
                bool made = variant == 0 ? harmony_make_chord(key->mode, degree, inversion, false, false, 1, &chord)
                                         : (key->mode == PLAN_MODE_MINOR && degree == 7 &&
                                            harmony_make_leading_tone(key->mode, inversion, 1, &chord));
                /* A diminished triad is not written in root position. */
                bool diminished = made && ((key->mode == PLAN_MODE_MAJOR && degree == 7) ||
                                           (key->mode == PLAN_MODE_MINOR && (degree == 2 || chord.chord.leading_tone)));
                if (made && !(diminished && inversion == 0) && harmony_chord_bass_pc(key, &chord, &bass) &&
                    bass == pc && count < MAX_OPTIONS_PER_PC) {
                    out[count++] = chord;
                }
            }
        }
    }
    for (int inversion = 0; inversion <= 3; inversion++) {
        int bass;
        if (harmony_make_chord(key->mode, 5, inversion, true, false, 1, &chord) &&
            harmony_chord_bass_pc(key, &chord, &bass) && bass == pc && count < MAX_OPTIONS_PER_PC) {
            out[count++] = chord;
        }
    }
    return count;
}

static bool octave_applies(const PiecePlan *plan, FillTarget target, void *context) {
    OctaveContext read;
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && octave_context_read(plan, (LaneSpanId)target.id, &read);
}

static bool same_chords(const OctaveCandidate *a, const OctaveCandidate *b) {
    for (size_t i = 0; i < a->context.count; i++) {
        if (strcmp(a->chords[i].roman, b->chords[i].roman) != 0) {
            return false;
        }
    }
    return true;
}

static void chords_label(const OctaveCandidate *line, char *label, size_t size) {
    size_t used = 0;
    label[0] = '\0';
    for (size_t i = 0; i < line->context.count && used + 1 < size; i++) {
        int written = snprintf(label + used, size - used, i ? " %s" : "%s", line->chords[i].roman);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }
}

/* One harmonization drawn at random: tonic at both ends, any chord that fits the bass between. */
static bool draw_chords(const OctaveContext *context, FillRng *rng, OctaveCandidate *out) {
    memset(out, 0, sizeof(*out));
    out->context = *context;
    for (size_t i = 0; i < context->count; i++) {
        PhraseChord options[MAX_OPTIONS_PER_PC];
        size_t count;
        if (i == 0 || i + 1 == context->count) {
            if (!harmony_make_chord(context->key.mode, 1, 0, false, false, context->runs[i].cells, &out->chords[i])) {
                return false;
            }
            continue;
        }
        count = options_for(&context->key, context->runs[i].pc, options);
        if (count == 0) {
            return false;
        }
        out->chords[i] = options[fill_rng_below(rng, count)];
        out->chords[i].cells = context->runs[i].cells;
    }
    return true;
}

static bool octave_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    OctaveContext read;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !octave_context_read(plan, (LaneSpanId)target.id, &read)) {
        return false;
    }
    OctaveCandidate *scratch = malloc(sizeof(*scratch));
    OctaveCandidate *kept[OCTAVE_KEEP];
    size_t kept_count = 0;
    if (!scratch) {
        return false;
    }
    bool ok = true;
    for (size_t attempt = 0; ok && attempt < OCTAVE_ATTEMPTS && kept_count < OCTAVE_KEEP; attempt++) {
        if (!draw_chords(&read, rng, scratch)) {
            break;
        }
        bool repeat = false;
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_chords(kept[i], scratch);
        }
        HeardPhrase heard = {scratch->chords, read.count, read.key.mode, PLAN_CADENCE_PAC,
                             read.metre.cells_per_bar, read.metre.cell_length, true, NULL, false, false, NULL, NULL, false,
                             &read.metre};
        PhraseVerdict verdict;
        if (repeat) {
            continue;
        }
        if (!harmony_judge(&heard, &verdict)) {
            ok = false;
        } else if (!verdict.hard) {
            OctaveCandidate *payload = malloc(sizeof(*payload));
            char label[FILL_LABEL_MAX];
            if (!payload) {
                ok = false;
                break;
            }
            *payload = *scratch;
            chords_label(payload, label, sizeof(label));
            ok = fill_candidates_add(out, label, payload, free);
            kept[kept_count++] = payload;
        }
    }
    free(scratch);
    return ok;
}

/* The first chord takes the target span (it keeps its id) and one span is added for each chord after it. */
static PlanResult octave_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const OctaveCandidate *line = candidate ? candidate->payload : NULL;
    OctaveContext read;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !line) {
        return PLAN_ERR_NULL;
    }
    LaneSpanId first = (LaneSpanId)target.id;
    if (!octave_context_read(plan, first, &read) || read.count != line->context.count) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    PlanResult result = PLAN_OK;
    size_t cell = 0;
    for (size_t i = 0; result == PLAN_OK && i < read.count; i++) {
        PlanSpan span;
        PlanLanePayload payload;
        result = harmony_metre_cells_span(&read.metre, cell, read.runs[i].cells, &span);
        if (result != PLAN_OK) {
            break;
        }
        harmony_chord_payload(&line->chords[i], &payload);
        if (i == 0) {
            result = plan_lane_span_resize(plan, first, span.length);
            if (result == PLAN_OK) {
                result = plan_lane_span_write(plan, first, &payload);
            }
        } else {
            LaneSpanId id;
            result = plan_add_lane_span(plan, read.owner, PLAN_LANE_HARMONY, 0, span, &payload, &id);
            if (result == PLAN_OK) {
                result = plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED);
            }
        }
        cell += read.runs[i].cells;
    }
    return result;
}

bool harmony_octave_check(const void *payload, FillCheck *out);

bool harmony_octave_check(const void *payload, FillCheck *out) {
    const OctaveCandidate *line = payload;
    HeardPhrase heard;
    PhraseVerdict verdict;
    if (!line || !out) {
        return false;
    }
    heard = (HeardPhrase){line->chords, line->context.count, line->context.key.mode, PLAN_CADENCE_PAC,
                          line->context.metre.cells_per_bar, line->context.metre.cell_length, true, NULL, false, false, NULL, NULL, false,
                          &line->context.metre};
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!harmony_judge(&heard, &verdict)) {
        return false;
    }
    if (verdict.hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        return true;
    }
    out->score_numerator = verdict.soft_held;
    out->score_denominator = verdict.soft_total ? verdict.soft_total : 1;
    snprintf(out->reason, sizeof(out->reason), "standard-harmony");
    return true;
}

FillResult harmony_octave_register(FillOperatorRegistry *registry) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_OCTAVE_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span over a Given stepwise bass";
    operator_.generator.supported_outputs = "PiecePlan harmony lane spans";
    operator_.generator.configuration_description = "none";
    operator_.generator.constraints_description = "standard-harmony phrase model, chord bass members";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = octave_applies;
    operator_.generate = octave_generate;
    operator_.apply = octave_apply;
    return fill_registry_register(registry, &operator_);
}
