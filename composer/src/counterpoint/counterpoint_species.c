#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "counterpoint/counterpoint_internal.h"
#include "util/util.h"

/*
 * The beam search of the counterpoint operator. A state is the counter-melody written so far, in slots (counterpoint_
 * internal.h); a step adds a bar of them, or in species 1 a note. A step is made note by note with the rules that need
 * no library (the pitches of the key, a consonance on a strong slot, a dissonance only reached by step, no leap a strict
 * counterpoint forbids), the children are ordered by cost and the rng's draw, and the cheapest are asked of the library
 * (counterpoint_eval) until the beam is full.
 *
 * Species 1 adds a note a step. Species 2 and 3 add the weak slots of a bar and the downbeat of the next, so that a
 * dissonance on a weak slot is judged with the note it resolves to. Species 4 adds a bar: the downbeat, which is the weak
 * note before it tied over or, when species is broken, a new note, and the new weak note. The last downbeat is the
 * tonic, in species 4 the whole last bar.
 */

#define CHILD_ADD_MAX 5
/* The most children the library is asked about in a step, for each place in the beam. */
#define CHECKS_PER_BEAM 400
/* What a step of the cost rules costs. */
#define REPEAT_COST 3
#define BROKEN_COST 20
#define PARALLEL_COST 6
#define DISSONANT_COST 1

typedef struct {
    size_t slots; /* the slots written */
    unsigned char kind[CP_MAX_SLOTS];
    int pitch[CP_MAX_SLOTS];
    long cost;
} State;

typedef struct {
    int parent;
    size_t add;
    unsigned char kind[CHILD_ADD_MAX];
    int pitch[CHILD_ADD_MAX];
    long cost;
    uint64_t tie;
} Child;

typedef struct {
    const CounterpointContext *context;
    FillRng *rng;
    Child *children;
    size_t count;
    size_t capacity;
    bool failed;
    int parent;
    const State *state;
} Gen;

static bool on_tonic(const CounterpointContext *context, int pitch) {
    return ((pitch - context->key.tonic_pitch_class) % 12 + 12) % 12 == 0;
}

/* A step a strict counterpoint allows between two notes of the line: no tritone, no seventh, no sixth but the rising minor one. */
static bool move_ok(int from, int to) {
    int size = util_absolute(to - from);
    return size <= 12 && size != 6 && size != 9 && size != 10 && size != 11 && !(size == 8 && to < from);
}

static bool ordered(const CounterpointContext *context, size_t note, int pitch) {
    return context->above ? pitch >= context->line[note] : pitch <= context->line[note];
}

/* The semitones of a leap over 2, the cost of the line's shape. */
static long leap_cost(int from, int to) {
    int size = util_absolute(to - from);
    return size > 2 ? size - 2 : 0;
}

/* Three downbeats in a row making the same imperfect consonance with the given line are parallel in all but name. */
static long parallel_cost(const CounterpointContext *context, size_t note, const int *downbeat) {
    int size[3];
    if (note < 2) {
        return 0;
    }
    for (size_t k = 0; k < 3; k++) {
        size[k] = util_absolute(downbeat[k] - context->line[note - 2 + k]) % 12;
    }
    return size[0] == size[1] && size[1] == size[2] && (size[0] == 3 || size[0] == 4 || size[0] == 8 || size[0] == 9)
               ? PARALLEL_COST
               : 0;
}

static bool push(Gen *gen, const unsigned char *kind, const int *pitch, size_t add, long cost) {
    Child *child;
    if (gen->count == gen->capacity) {
        size_t capacity = gen->capacity ? gen->capacity * 2 : 4096;
        Child *grown = realloc(gen->children, capacity * sizeof(*grown));
        if (!grown) {
            gen->failed = true;
            return false;
        }
        gen->children = grown;
        gen->capacity = capacity;
    }
    child = &gen->children[gen->count++];
    child->parent = gen->parent;
    child->add = add;
    memcpy(child->kind, kind, add);
    memcpy(child->pitch, pitch, add * sizeof(int));
    child->cost = gen->state->cost + cost;
    child->tie = fill_rng_next(gen->rng);
    return true;
}

/* ------------------------------------------------------ species 1 */

static void generate_first(Gen *gen) {
    const CounterpointContext *context = gen->context;
    const State *state = gen->state;
    size_t note = state->slots;
    bool last = note + 1 == context->count;
    for (int pitch = context->low; pitch <= context->high; pitch++) {
        long cost = 0;
        unsigned char kind = CP_SLOT_ATTACK;
        if (!context->consonant[note][pitch] || (last && !on_tonic(context, pitch))) {
            continue;
        }
        if (note > 0) {
            int from = state->pitch[note - 1];
            if (!move_ok(from, pitch)) {
                continue;
            }
            cost = leap_cost(from, pitch) + (pitch == from ? REPEAT_COST : 0);
        }
        if (note >= 2) {
            int downbeat[3] = {state->pitch[note - 2], state->pitch[note - 1], pitch};
            cost += parallel_cost(context, note, downbeat);
        }
        if (!push(gen, &kind, &pitch, 1, cost)) {
            return;
        }
    }
}

/* ------------------------------------------------- species 2 and 3 */

typedef struct {
    Gen *gen;
    size_t first; /* the slot the first new note takes */
    size_t slots; /* how many new notes */
    int pitch[CHILD_ADD_MAX];
    unsigned char kind[CHILD_ADD_MAX];
} Bar;

/* The pitch of a slot of the line being written: the state's, or the bar's own. */
static int pitch_of(const Bar *bar, size_t slot) {
    return slot < bar->gen->state->slots ? bar->gen->state->pitch[slot] : bar->pitch[slot - bar->first];
}

/*
 * The rules of a downbeat that look back, as the species states them, so that the library is not asked about lines that
 * break them: no tritone or seventh between downbeats; in species 2 neither the same perfect interval on two downbeats nor
 * on a weak beat and the downbeat after it; in species 3 no perfect fifth or octave on a downbeat when beats of the bar
 * before made it, no beat 4 and downbeat making the same perfect interval, and no downbeat reaching a perfect interval by
 * similar motion with the given line.
 */
static bool downbeat_ok(const Bar *bar, size_t slot, size_t note, int pitch) {
    const CounterpointContext *context = bar->gen->context;
    size_t per_bar = context->per_bar;
    int earlier = pitch_of(bar, slot - per_bar);
    int last = pitch_of(bar, slot - 1);
    int now = util_absolute(pitch - context->line[note]) % 12;
    int size = util_absolute(pitch - earlier);
    bool perfect = now == 0 || now == 7;
    if (size == 6 || size == 10 || size == 11) {
        return false;
    }
    if (context->species == 2) {
        int was = util_absolute(earlier - context->line[note - 1]) % 12;
        int weak = util_absolute(last - context->line[note - 1]) % 12;
        return !(perfect && (was == now || weak == now));
    }
    if (perfect) {
        int before = util_absolute(last - context->line[note - 1]) % 12;
        bool similar = (pitch - last > 0 && context->line[note] - context->line[note - 1] > 0) ||
                       (pitch - last < 0 && context->line[note] - context->line[note - 1] < 0);
        if (before == now || similar) {
            return false;
        }
        for (size_t beat = 1; beat < per_bar; beat++) {
            int heard = util_absolute(pitch_of(bar, slot - per_bar + beat) - context->line[note - 1]) % 12;
            if ((now == 7 && beat >= 2 && heard == 7) || (now == 0 && heard == 0)) {
                return false;
            }
        }
    }
    return true;
}

static void descend_bar(Bar *bar, size_t at, int prev, long cost) {
    const CounterpointContext *context = bar->gen->context;
    const State *state = bar->gen->state;
    size_t per_bar = context->per_bar;
    size_t slot = bar->first + at;
    size_t note = slot / per_bar;
    bool strong = slot % per_bar == 0;
    bool final = strong && note + 1 == context->count;
    if (at == bar->slots) {
        push(bar->gen, bar->kind, bar->pitch, bar->slots, cost);
        return;
    }
    for (int pitch = context->low; pitch <= context->high && !bar->gen->failed; pitch++) {
        long extra = 0;
        if (!context->spell[pitch] || (final && !on_tonic(context, pitch))) {
            continue;
        }
        /* No note is more than a twelfth from the given line. */
        if (util_absolute(pitch - context->line[note]) > 19) {
            continue;
        }
        if (strong) {
            if (!context->consonant[note][pitch]) {
                continue;
            }
            if (note > 0 && !downbeat_ok(bar, slot, note, pitch)) {
                continue;
            }
        } else if (!context->consonant[note][pitch]) {
            /* A dissonance on a weak slot is reached by step, and not by crossing the given line. */
            if (!ordered(context, note, pitch) || util_absolute(pitch - prev) > 2 || pitch == prev) {
                continue;
            }
            extra += DISSONANT_COST;
        }
        if (slot > 0) {
            int from = at > 0 ? prev : state->pitch[slot - 1];
            if (!move_ok(from, pitch)) {
                continue;
            }
            /* A note that leaves a dissonance does so by a step or a third. */
            if (slot > 0 && !context->consonant[(slot - 1) / per_bar][from] && (slot - 1) % per_bar != 0 &&
                util_absolute(pitch - from) > 4) {
                continue;
            }
            extra += leap_cost(from, pitch) + (pitch == from ? REPEAT_COST : 0);
        }
        if (strong && note >= 2) {
            int downbeat[3];
            downbeat[0] = state->pitch[(note - 2) * per_bar];
            downbeat[1] = state->pitch[(note - 1) * per_bar];
            downbeat[2] = pitch;
            extra += parallel_cost(context, note, downbeat);
        }
        bar->pitch[at] = pitch;
        bar->kind[at] = CP_SLOT_ATTACK;
        descend_bar(bar, at + 1, pitch, cost + extra);
    }
}

static void generate_bar(Gen *gen) {
    const State *state = gen->state;
    Bar bar;
    bar.gen = gen;
    bar.first = state->slots;
    /* The first step writes the whole first bar and the downbeat of the next. */
    bar.slots = state->slots == 0 ? gen->context->per_bar + 1 : gen->context->per_bar;
    descend_bar(&bar, 0, state->slots > 0 ? state->pitch[state->slots - 1] : 0, 0);
}

/* ------------------------------------------------------ species 4 */

static void generate_fourth(Gen *gen) {
    const CounterpointContext *context = gen->context;
    const State *state = gen->state;
    size_t bar = state->slots / 2;
    bool last = bar + 1 == context->count;
    int held = state->slots > 0 ? state->pitch[state->slots - 1] : 0;
    for (int pitch = context->low; pitch <= context->high && !gen->failed; pitch++) {
        unsigned char kind[CHILD_ADD_MAX];
        int notes[CHILD_ADD_MAX];
        if (!context->spell[pitch] || !context->consonant[bar][pitch]) {
            continue;
        }
        if (state->slots == 0) {
            /* The first bar: a half rest and the first note, on the weak beat. */
            kind[0] = CP_SLOT_REST;
            notes[0] = 0;
            kind[1] = CP_SLOT_ATTACK;
            notes[1] = pitch;
            push(gen, kind, notes, 2, 0);
            continue;
        }
        if (!move_ok(held, pitch)) {
            continue;
        }
        if (last) {
            /* The last bar is a whole note on the tonic. */
            if (on_tonic(context, pitch)) {
                kind[0] = CP_SLOT_ATTACK;
                notes[0] = pitch;
                kind[1] = CP_SLOT_TIE;
                notes[1] = pitch;
                push(gen, kind, notes, 2, leap_cost(held, pitch));
            }
            continue;
        }
        /* The weak note, after the downbeat that is the weak note before it tied over, or a new note. */
        for (int broken = 0; broken < 2; broken++) {
            long cost = leap_cost(held, pitch) + (pitch == held ? REPEAT_COST : 0);
            int downbeat = held;
            if (broken) {
                /* Species is broken: a new consonant note on the downbeat, and the weak note follows second species. */
                for (downbeat = context->low; downbeat <= context->high && !gen->failed; downbeat++) {
                    if (!context->consonant[bar][downbeat] || !move_ok(held, downbeat) || !move_ok(downbeat, pitch) ||
                        downbeat == held) {
                        continue;
                    }
                    kind[0] = CP_SLOT_ATTACK;
                    notes[0] = downbeat;
                    kind[1] = CP_SLOT_ATTACK;
                    notes[1] = pitch;
                    push(gen, kind, notes, 2, cost + BROKEN_COST + leap_cost(downbeat, pitch));
                }
                continue;
            }
            /* A dissonance tied over is a suspension: it resolves by a step down. */
            if (!context->consonant[bar][downbeat] && !(pitch < held && held - pitch <= 2)) {
                continue;
            }
            kind[0] = CP_SLOT_TIE;
            notes[0] = downbeat;
            kind[1] = CP_SLOT_ATTACK;
            notes[1] = pitch;
            push(gen, kind, notes, 2, cost);
        }
    }
}

/* ------------------------------------------------------ the search */

static int compare_children(const void *a, const void *b) {
    const Child *x = a;
    const Child *y = b;
    if (x->cost != y->cost) {
        return x->cost < y->cost ? -1 : 1;
    }
    if (x->tie != y->tie) {
        return x->tie < y->tie ? -1 : 1;
    }
    return 0;
}

/* Whether the state holds the whole line, bar for bar, as far as the search writes it. */
static bool complete(const CounterpointContext *context, const State *state) {
    switch (context->species) {
    case 1:
        return state->slots == context->count;
    case 4:
        return state->slots == 2 * context->count;
    default:
        return state->slots == context->per_bar * (context->count - 1) + 1;
    }
}

/*
 * The slots to ask the library about: the state's, and for a line that is not finished the last bar as a whole note
 * (the downbeat held through the bar), which is the shape a finished bar takes in a prefix. *notes is how many notes of
 * the given line they sound against.
 */
static size_t assemble(const CounterpointContext *context, const State *state, unsigned char *kind, int *pitch,
                       size_t *notes) {
    size_t slots = state->slots;
    memcpy(kind, state->kind, slots);
    memcpy(pitch, state->pitch, slots * sizeof(int));
    if (context->species == 1) {
        *notes = slots;
    } else if (context->species == 4) {
        if (slots < 2 * context->count) {
            /* A leading tone goes up to the tonic, so the stand-in bar after it sounds the tonic. */
            int stand_in = pitch[slots - 1];
            if (((stand_in + 1 - context->key.tonic_pitch_class) % 12 + 12) % 12 == 0) {
                stand_in++;
            }
            kind[slots] = CP_SLOT_ATTACK;
            pitch[slots] = stand_in;
            kind[slots + 1] = CP_SLOT_TIE;
            pitch[slots + 1] = stand_in;
            slots += 2;
        }
        *notes = slots / 2;
    } else {
        while (slots % context->per_bar != 0) {
            kind[slots] = CP_SLOT_TIE;
            pitch[slots] = pitch[slots - 1];
            slots++;
        }
        *notes = slots / context->per_bar;
    }
    return slots;
}

bool counterpoint_search(const CounterpointContext *context, FillRng *rng, size_t wanted, size_t beam,
                         CounterpointCandidate **out, size_t *count) {
    State *layer;
    State *next;
    size_t layer_size = 1;
    Gen gen;
    bool ok;
    *count = 0;
    if (beam == 0) {
        beam = COUNTERPOINT_DEFAULT_BEAM;
    }
    layer = calloc(beam, sizeof(*layer));
    next = calloc(beam, sizeof(*next));
    ok = layer && next;
    if (!ok) {
        free(layer);
        free(next);
        return false;
    }
    memset(&gen, 0, sizeof(gen));
    gen.context = context;
    gen.rng = rng;
    /* One empty state to begin with; every state of a layer has written as many slots as the others. */
    while (ok && layer_size > 0 && !complete(context, &layer[0])) {
        size_t kept = 0;
        gen.count = 0;
        for (size_t s = 0; ok && s < layer_size; s++) {
            gen.parent = (int)s;
            gen.state = &layer[s];
            if (context->species == 1) {
                generate_first(&gen);
            } else if (context->species == 4) {
                generate_fourth(&gen);
            } else {
                generate_bar(&gen);
            }
            ok = !gen.failed;
        }
        if (!ok) {
            break;
        }
        qsort(gen.children, gen.count, sizeof(*gen.children), compare_children);
        /*
         * The cheapest children the library allows. A line is kept for the way it ends: a first pass takes at most a share
         * of the beam for each last note, so that the notes the ending needs are not crowded out by cheaper ones, and a
         * second pass fills what is left.
         */
        {
            unsigned char *skipped = calloc(gen.count ? gen.count : 1, 1);
            size_t per_note[128] = {0};
            size_t share = beam / 24 > 2 ? beam / 24 : 2;
            size_t checks = 0;
            ok = skipped != NULL;
            for (size_t pass = 0; ok && pass < 2; pass++) {
                for (size_t n = 0; ok && n < gen.count && kept < beam && checks < beam * CHECKS_PER_BEAM; n++) {
                    const Child *child = &gen.children[n];
                    State *made = &next[kept];
                    unsigned char kind[CP_MAX_SLOTS + 4];
                    int pitch[CP_MAX_SLOTS + 4];
                    CounterpointVerdict verdict;
                    size_t notes;
                    int last = 0;
                    if (pass == 1 && !skipped[n]) {
                        continue;
                    }
                    for (size_t k = 0; k < child->add; k++) {
                        last = child->kind[k] == CP_SLOT_ATTACK ? child->pitch[k] : last;
                    }
                    if (pass == 0 && per_note[last & 127] >= share) {
                        skipped[n] = 1;
                        continue;
                    }
                    *made = layer[child->parent];
                    memcpy(made->kind + made->slots, child->kind, child->add);
                    memcpy(made->pitch + made->slots, child->pitch, child->add * sizeof(int));
                    made->slots += child->add;
                    made->cost = child->cost;
                    assemble(context, made, kind, pitch, &notes);
                    checks++;
                    if (!counterpoint_eval(context, notes, kind, pitch, complete(context, made), &verdict)) {
                        ok = false;
                    } else if (!verdict.hard) {
                        per_note[last & 127]++;
                        kept++;
                    }
                }
            }
            free(skipped);
        }
        if (!ok) {
            break;
        }
        {
            State *swap = layer;
            layer = next;
            next = swap;
        }
        layer_size = kept;
    }
    for (size_t p = 0; ok && layer_size > 0 && p < wanted && p < layer_size; p++) {
        CounterpointCandidate *line = calloc(1, sizeof(*line));
        unsigned char kind[CP_MAX_SLOTS + 4];
        int pitch[CP_MAX_SLOTS + 4];
        size_t notes;
        if (!line) {
            ok = false;
            break;
        }
        line->magic = CP_PAYLOAD_MAGIC;
        line->slots = assemble(context, &layer[p], kind, pitch, &notes);
        memcpy(line->kind, kind, line->slots);
        memcpy(line->pitch, pitch, line->slots * sizeof(int));
        out[(*count)++] = line;
    }
    if (!ok) {
        for (size_t i = 0; i < *count; i++) {
            free(out[i]);
        }
        *count = 0;
    }
    free(gen.children);
    free(layer);
    free(next);
    return ok;
}
