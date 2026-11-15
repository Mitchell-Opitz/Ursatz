#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "texture/voicing_internal.h"

/*
 * One path to a chord: the path it extends in the layer before, the voicing it chose, what it cost so far, and the
 * rng's draw that orders it among paths of the same cost.
 */
typedef struct {
    int prev;
    int option;
    long cost;
    uint64_t tie;
} Path;

static int compare_steps(const void *a, const void *b) {
    const Path *x = a;
    const Path *y = b;
    if (x->cost != y->cost) {
        return x->cost < y->cost ? -1 : 1;
    }
    if (x->tie != y->tie) {
        return x->tie < y->tie ? -1 : 1;
    }
    return 0;
}

static void free_layers(Path **layers, VoicingOption **options, size_t chords) {
    for (size_t i = 0; i < chords; i++) {
        free(layers[i]);
        free(options[i]);
    }
}

bool voicing_search(const VoicingContext *context, FillRng *rng, size_t wanted, size_t beam, VoicingCandidate **out,
                    size_t *count) {
    size_t chords = context->chord_count;
    VoicingOption *options[VOICING_MAX_CHORDS] = {NULL};
    size_t option_count[VOICING_MAX_CHORDS] = {0};
    Path *layers[VOICING_MAX_CHORDS] = {NULL};
    size_t layer_size[VOICING_MAX_CHORDS] = {0};
    VoicingOption *scratch = malloc(VOICING_MAX_OPTIONS * sizeof(*scratch));
    Path *steps = NULL;
    bool ok = scratch != NULL;
    bool open = ok;
    *count = 0;
    if (beam == 0) {
        beam = VOICING_DEFAULT_BEAM;
    }
    if (wanted > beam) {
        wanted = beam;
    }
    /* Every valid voicing of every chord; a chord with none ends the search. */
    for (size_t i = 0; ok && open && i < chords; i++) {
        open = voicing_options(context, i, scratch, VOICING_MAX_OPTIONS, &option_count[i]) && option_count[i] > 0;
        if (open) {
            options[i] = malloc(option_count[i] * sizeof(**options));
            ok = options[i] != NULL;
            if (ok) {
                memcpy(options[i], scratch, option_count[i] * sizeof(**options));
            }
        }
    }
    free(scratch);
    if (ok && open) {
        steps = malloc(beam * VOICING_MAX_OPTIONS * sizeof(*steps));
        ok = steps != NULL;
    }
    for (size_t i = 0; ok && open && i < chords; i++) {
        size_t made = 0;
        size_t kept = 0;
        Path *layer = malloc(beam * sizeof(*layer));
        if (!layer) {
            ok = false;
            break;
        }
        layers[i] = layer;
        if (i == 0) {
            /* Every first voicing costs the same, so the rng picks which are kept. */
            for (size_t o = 0; o < option_count[0]; o++) {
                steps[made].prev = -1;
                steps[made].option = (int)o;
                steps[made].cost = 0;
                steps[made].tie = fill_rng_next(rng);
                made++;
            }
        } else {
            for (size_t s = 0; s < layer_size[i - 1]; s++) {
                const int *from = options[i - 1][layers[i - 1][s].option].pitch;
                for (size_t o = 0; o < option_count[i]; o++) {
                    steps[made].prev = (int)s;
                    steps[made].option = (int)o;
                    steps[made].cost = layers[i - 1][s].cost + voicing_connection_cost(from, options[i][o].pitch);
                    steps[made].tie = fill_rng_next(rng);
                    made++;
                }
            }
        }
        qsort(steps, made, sizeof(*steps), compare_steps);
        /* The cheapest paths the rules allow, in order, until the beam is full. */
        for (size_t n = 0; n < made && kept < beam; n++) {
            if (i > 0 && !voicing_connection_ok(context, i, options[i - 1][layers[i - 1][steps[n].prev].option].pitch,
                                                options[i][steps[n].option].pitch, NULL)) {
                continue;
            }
            layer[kept].prev = steps[n].prev;
            layer[kept].option = steps[n].option;
            layer[kept].cost = steps[n].cost;
            layer[kept].tie = steps[n].tie;
            kept++;
        }
        layer_size[i] = kept;
        open = kept > 0;
    }
    /* The cheapest complete paths, read back from the end. */
    for (size_t p = 0; ok && open && p < wanted && p < layer_size[chords - 1]; p++) {
        VoicingCandidate *path = calloc(1, sizeof(*path));
        int state = (int)p;
        if (!path) {
            ok = false;
            break;
        }
        path->magic = VOICING_PAYLOAD_MAGIC;
        path->chord_count = chords;
        for (size_t i = chords; i-- > 0;) {
            const Path *at = &layers[i][state];
            memcpy(path->pitch[i], options[i][at->option].pitch, sizeof(path->pitch[i]));
            state = at->prev;
        }
        out[(*count)++] = path;
    }
    if (!ok) {
        for (size_t i = 0; i < *count; i++) {
            free(out[i]);
        }
        *count = 0;
    }
    free(steps);
    free_layers(layers, options, chords);
    return ok;
}
