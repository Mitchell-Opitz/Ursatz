#include "analysis_runs.h"

static bool same_chord(const HarmonySegment *a, const HarmonySegment *b) {
    if (!a->valid || !b->valid || a->applied != b->applied) {
        return false;
    }
    if (a->applied) {
        return a->applied_chord.kind == b->applied_chord.kind && a->applied_chord.quality == b->applied_chord.quality &&
               a->applied_chord.target == b->applied_chord.target && a->input->inversion == b->input->inversion &&
               a->input->degree == b->input->degree;
    }
    return a->chord.degree == b->chord.degree && a->chord.inversion == b->chord.inversion &&
           a->chord.seventh == b->chord.seventh && a->chord.cadential == b->chord.cadential &&
           a->chord.leading_tone == b->chord.leading_tone && a->chord.half_diminished == b->chord.half_diminished;
}

size_t harmony_analysis_build_runs(const HarmonySegment *segments, size_t count, HarmonyRun *runs) {
    size_t n = 0;
    size_t i = 0;
    while (i < count) {
        HarmonyRun run;
        run.first = i;
        run.last = i;
        run.valid = segments[i].valid;
        run.applied = segments[i].applied;
        run.uncertainty = segments[i].input->uncertainty;
        while (run.last + 1 < count &&
               (run.valid ? same_chord(&segments[i], &segments[run.last + 1]) : !segments[run.last + 1].valid)) {
            run.last++;
            if (segments[run.last].input->uncertainty > run.uncertainty) {
                run.uncertainty = segments[run.last].input->uncertainty;
            }
        }
        runs[n++] = run;
        i = run.last + 1;
    }
    return n;
}
