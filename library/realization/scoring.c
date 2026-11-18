#include "realization/scoring.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "realization/performer_part_internal.h"

RealizationResult scoring_assignment_check(const ScoringAssignment *assignment) {
    if (assignment == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(assignment->id) || !realization_id_valid(assignment->line) ||
        !realization_id_valid(assignment->part)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if (assignment->staff < 1 || assignment->voice < 1 || (int)assignment->transform.kind < (int)SCORING_TRANSFORM_NONE ||
        (int)assignment->transform.kind > (int)SCORING_TRANSFORM_INTERVAL || assignment->transform.octaves < -4 ||
        assignment->transform.octaves > 4 || (!assignment->whole && !time_span_holds(assignment->span))) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    return REALIZATION_OK;
}

/* One generated note, with the hash of its id so the id lookups compare numbers first. */
typedef struct {
    PerformerNote note;
    uint32_t hash;
    bool skip; /* a kept note holds the id */
} Generated;

typedef struct {
    PerformerNote *notes;
    size_t count;
} Fresh;

static uint32_t id_hash(const char *text) {
    uint32_t hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; p++) {
        hash = (hash ^ *p) * 16777619u;
    }
    return hash;
}

static bool in_span(const ScoringAssignment *assignment, MusicalTime onset) {
    return assignment->whole || (musical_time_cmp(onset, assignment->span.start) >= 0 &&
                                 musical_time_cmp(onset, assignment->span.end) < 0);
}

static int effective_octaves(const ScoringAssignment *assignment) {
    return assignment->transform.kind == SCORING_TRANSFORM_OCTAVE ? assignment->transform.octaves : 0;
}

/* A stable merge sort of notes by onset. */
static bool sort_by_onset(PerformerNote *notes, size_t count) {
    if (count < 2) {
        return true;
    }
    PerformerNote *buffer = malloc(count * sizeof(PerformerNote));
    if (buffer == NULL) {
        return false;
    }
    PerformerNote *from = notes;
    PerformerNote *to = buffer;
    for (size_t width = 1; width < count; width *= 2) {
        for (size_t start = 0; start < count; start += 2 * width) {
            size_t middle = start + width < count ? start + width : count;
            size_t end = start + 2 * width < count ? start + 2 * width : count;
            size_t i = start, j = middle, k = start;
            while (i < middle && j < end) {
                to[k++] = musical_time_cmp(from[j].onset, from[i].onset) < 0 ? from[j++] : from[i++];
            }
            while (i < middle) {
                to[k++] = from[i++];
            }
            while (j < end) {
                to[k++] = from[j++];
            }
        }
        PerformerNote *swap = from;
        from = to;
        to = swap;
    }
    if (from != notes) {
        memcpy(notes, from, count * sizeof(PerformerNote));
    }
    free(buffer);
    return true;
}

static void release_fresh(Fresh *fresh, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(fresh[i].notes);
    }
    free(fresh);
}

static size_t find_by_name(void *const *items, size_t count, const char *id, const char *(*name_of)(const void *)) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(name_of(items[i]), id) == 0) {
            return i;
        }
    }
    return count;
}

static const char *line_name_of(const void *line) {
    return line_id((const Line *)line);
}

static const char *part_name_of(const void *part) {
    return performer_part_id((const PerformerPart *)part);
}

static bool duplicate_names(void *const *items, size_t count, const char *(*name_of)(const void *)) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (strcmp(name_of(items[i]), name_of(items[j])) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* Every check; fills the line and part index of each assignment. */
static RealizationResult check_all(Line *const *lines, size_t line_count, const ScoringAssignment *assignments,
                                   size_t assignment_count, PerformerPart *const *parts, size_t part_count, const char *run,
                                   size_t *line_of, size_t *part_of) {
    NoteDerivation probe;
    memset(&probe, 0, sizeof(probe));
    RealizationResult result = note_derivation_set_run(&probe, run);
    if (result != REALIZATION_OK) {
        return result;
    }
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] == NULL) {
            return REALIZATION_ERR_NULL;
        }
    }
    for (size_t i = 0; i < part_count; i++) {
        if (parts[i] == NULL) {
            return REALIZATION_ERR_NULL;
        }
    }
    if (duplicate_names((void *const *)lines, line_count, line_name_of) ||
        duplicate_names((void *const *)parts, part_count, part_name_of)) {
        return REALIZATION_ERR_DUPLICATE_ID;
    }
    for (size_t i = 0; i < assignment_count; i++) {
        result = scoring_assignment_check(&assignments[i]);
        if (result != REALIZATION_OK) {
            return result;
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(assignments[i].id, assignments[j].id) == 0) {
                return REALIZATION_ERR_DUPLICATE_ID;
            }
        }
    }
    for (size_t i = 0; i < assignment_count; i++) {
        const ScoringAssignment *a = &assignments[i];
        line_of[i] = find_by_name((void *const *)lines, line_count, a->line, line_name_of);
        part_of[i] = find_by_name((void *const *)parts, part_count, a->part, part_name_of);
        if (line_of[i] == line_count || part_of[i] == part_count) {
            return REALIZATION_ERR_UNKNOWN_REFERENCE;
        }
        if (a->staff > performer_part_staff_count(parts[part_of[i]])) {
            return REALIZATION_ERR_BAD_VALUE;
        }
        if (a->transform.kind == SCORING_TRANSFORM_INTERVAL) {
            return REALIZATION_ERR_NOT_BUILT;
        }
        int shift = 12 * effective_octaves(a);
        const Line *line = lines[line_of[i]];
        for (size_t n = 0; n < line_note_count(line); n++) {
            const LineNote *note = line_note_at(line, n);
            if (in_span(a, note->onset) && (note->midi + shift < 0 || note->midi + shift > 127)) {
                return REALIZATION_ERR_BAD_VALUE;
            }
        }
    }
    return REALIZATION_OK;
}

static bool kept(const PerformerNote *note) {
    return note->edited || note->detached || note->derivation.source_count == 0;
}

static bool differs(const PerformerNote *kept_note, const PerformerNote *made) {
    return kept_note->midi != made->midi || !musical_time_same(kept_note->onset, made->onset) ||
           !duration_same(kept_note->duration, made->duration);
}

/* The new notes of one part (every assignment to it); on success *out holds them. */
static RealizationResult build_part(Line *const *lines, const ScoringAssignment *assignments, size_t assignment_count,
                                    const size_t *line_of, const size_t *part_of, size_t part_index, const PerformerPart *part,
                                    const char *run, Fresh *out) {
    Generated *made = NULL;
    size_t made_count = 0, made_capacity = 0;
    RealizationResult result = REALIZATION_OK;
    for (size_t i = 0; i < assignment_count && result == REALIZATION_OK; i++) {
        if (part_of[i] != part_index) {
            continue;
        }
        const ScoringAssignment *a = &assignments[i];
        const Line *line = lines[line_of[i]];
        int shift = 12 * effective_octaves(a);
        for (size_t n = 0; n < line_note_count(line) && result == REALIZATION_OK; n++) {
            const LineNote *source = line_note_at(line, n);
            if (!in_span(a, source->onset)) {
                continue;
            }
            char base[REALIZATION_ID_MAX + 16];
            char id[REALIZATION_ID_MAX + 16];
            int written = snprintf(base, sizeof(base), "%s-%s", part->id, source->id);
            if (written < 0 || written > REALIZATION_ID_MAX - 1) {
                result = REALIZATION_ERR_BAD_VALUE;
                break;
            }
            strcpy(id, base);
            for (int suffix = 2;; suffix++) {
                uint32_t hash = id_hash(id);
                bool taken = false;
                for (size_t g = 0; g < made_count && !taken; g++) {
                    taken = made[g].hash == hash && strcmp(made[g].note.id, id) == 0;
                }
                if (!taken) {
                    break;
                }
                written = snprintf(id, sizeof(id), "%s-%d", base, suffix);
                if (written < 0 || written > REALIZATION_ID_MAX - 1) {
                    result = REALIZATION_ERR_BAD_VALUE;
                    break;
                }
            }
            if (result != REALIZATION_OK) {
                break;
            }
            if (made_count == made_capacity) {
                size_t capacity = made_capacity == 0 ? 64 : made_capacity * 2;
                Generated *grown = realloc(made, capacity * sizeof(Generated));
                if (grown == NULL) {
                    result = REALIZATION_ERR_ALLOC;
                    break;
                }
                made = grown;
                made_capacity = capacity;
            }
            Generated *g = &made[made_count++];
            memset(g, 0, sizeof(*g));
            strcpy(g->note.id, id);
            g->hash = id_hash(id);
            g->note.onset = source->onset;
            g->note.duration = source->duration;
            g->note.midi = source->midi + shift;
            g->note.spelled = source->spelled;
            g->note.letter = source->letter;
            g->note.accidental = source->accidental;
            g->note.staff = a->staff;
            g->note.voice = a->voice;
            strcpy(g->note.derivation.sources[0], source->id);
            g->note.derivation.source_count = 1;
            strcpy(g->note.derivation.operation, "score");
            strcpy(g->note.derivation.run, run);
        }
    }
    PerformerNote *notes = NULL;
    size_t count = 0;
    if (result == REALIZATION_OK) {
        size_t capacity = made_count + part->note_count;
        notes = malloc((capacity > 0 ? capacity : 1) * sizeof(PerformerNote));
        if (notes == NULL) {
            result = REALIZATION_ERR_ALLOC;
        }
    }
    if (result == REALIZATION_OK) {
        /* the notes that stay, flagged against what the assignments now give */
        PerformerNote *stay = notes + made_count;
        size_t stay_count = 0;
        for (size_t k = 0; k < part->note_count; k++) {
            if (!kept(&part->notes[k])) {
                continue;
            }
            PerformerNote note = part->notes[k];
            uint32_t hash = id_hash(note.id);
            bool same_id = false;
            for (size_t g = 0; g < made_count; g++) {
                if (made[g].hash == hash && strcmp(made[g].note.id, note.id) == 0) {
                    made[g].skip = true;
                    same_id = true;
                    if (differs(&note, &made[g].note)) {
                        note.review = true;
                    }
                    break;
                }
            }
            if (!same_id && note.derivation.source_count > 0) {
                bool assigned = false;
                for (size_t g = 0; g < made_count && !assigned; g++) {
                    assigned = strcmp(made[g].note.derivation.sources[0], note.derivation.sources[0]) == 0;
                }
                if (!assigned) {
                    note.review = true;
                }
            }
            stay[stay_count++] = note;
        }
        for (size_t g = 0; g < made_count; g++) {
            if (!made[g].skip) {
                notes[count++] = made[g].note;
            }
        }
        memmove(notes + count, stay, stay_count * sizeof(PerformerNote));
        count += stay_count;
        if (!sort_by_onset(notes, count)) {
            result = REALIZATION_ERR_ALLOC;
        }
    }
    free(made);
    if (result != REALIZATION_OK) {
        free(notes);
        return result;
    }
    out->notes = notes;
    out->count = count;
    return REALIZATION_OK;
}

RealizationResult realization_score(Line *const *lines, size_t line_count, const ScoringAssignment *assignments,
                                    size_t assignment_count, PerformerPart *const *parts, size_t part_count, const char *run) {
    if (run == NULL || (lines == NULL && line_count > 0) || (assignments == NULL && assignment_count > 0) ||
        (parts == NULL && part_count > 0)) {
        return REALIZATION_ERR_NULL;
    }
    size_t *index = calloc(2 * (assignment_count > 0 ? assignment_count : 1), sizeof(size_t));
    Fresh *fresh = calloc(part_count > 0 ? part_count : 1, sizeof(Fresh));
    if (index == NULL || fresh == NULL) {
        free(index);
        free(fresh);
        return REALIZATION_ERR_ALLOC;
    }
    size_t *line_of = index;
    size_t *part_of = index + (assignment_count > 0 ? assignment_count : 1);
    RealizationResult result = check_all(lines, line_count, assignments, assignment_count, parts, part_count, run, line_of, part_of);
    size_t built = 0;
    for (; result == REALIZATION_OK && built < part_count; built++) {
        result = build_part(lines, assignments, assignment_count, line_of, part_of, built, parts[built], run, &fresh[built]);
        if (result != REALIZATION_OK) {
            break;
        }
    }
    if (result != REALIZATION_OK) {
        release_fresh(fresh, built);
        free(index);
        return result;
    }
    for (size_t p = 0; p < part_count; p++) {
        free(parts[p]->notes);
        parts[p]->notes = fresh[p].notes;
        parts[p]->note_count = fresh[p].count;
        parts[p]->note_capacity = fresh[p].count;
    }
    free(fresh);
    free(index);
    return REALIZATION_OK;
}
