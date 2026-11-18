#include "realization/piano_reduction.h"

#include <stdio.h>
#include <string.h>

/* The staff of a line on the grand staff: 1 (treble) or 2 (bass). */
static int staff_of(const Line *line) {
    if (line_role(line) == LINE_ROLE_MELODY) {
        return 1;
    }
    if (line_role(line) == LINE_ROLE_BASS) {
        return 2;
    }
    size_t count = line_note_count(line);
    long long sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += line_note_at(line, i)->midi;
    }
    return count == 0 || sum >= 60 * (long long)count ? 1 : 2;
}

RealizationResult movement_reduce_to_piano(Movement *movement, const Instrument *piano, const char *run) {
    if (movement == NULL || piano == NULL || run == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (movement_part_count(movement) > 0 || movement_assignment_count(movement) > 0 || piano->staff_count < 2) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    size_t lines = movement_line_count(movement);
    int staff_count = 1;
    for (size_t i = 0; i < lines; i++) {
        char id[REALIZATION_ID_MAX + 8];
        if (snprintf(id, sizeof(id), "a-%s", line_id(movement_line_at(movement, i))) > REALIZATION_ID_MAX - 1) {
            return REALIZATION_ERR_BAD_VALUE;
        }
        if (staff_of(movement_line_at(movement, i)) == 2) {
            staff_count = 2;
        }
    }
    PerformerPart *part = NULL;
    RealizationResult result = performer_part_create("piano", piano->id, piano->name, 1, 1, staff_count, piano->general_midi_program,
                                                     &part);
    if (result != REALIZATION_OK) {
        return result;
    }
    result = movement_add_part(movement, part);
    if (result != REALIZATION_OK) {
        performer_part_destroy(part);
        return result;
    }
    for (size_t i = 0; i < lines && result == REALIZATION_OK; i++) {
        const Line *line = movement_line_at(movement, i);
        ScoringAssignment assignment;
        memset(&assignment, 0, sizeof(assignment));
        snprintf(assignment.id, sizeof(assignment.id), "a-%s", line_id(line));
        strcpy(assignment.line, line_id(line));
        strcpy(assignment.part, "piano");
        assignment.whole = true;
        assignment.staff = staff_of(line);
        assignment.voice = (int)i + 1;
        result = movement_add_assignment(movement, &assignment);
    }
    if (result != REALIZATION_OK) {
        return result;
    }
    return movement_score(movement, run);
}
