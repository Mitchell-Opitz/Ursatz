#ifndef MUSIC_PITCH_STAFF_POSITION_H
#define MUSIC_PITCH_STAFF_POSITION_H

#include <stdbool.h>

/*
 * StaffPosition: where a notehead sits on a five-line staff, as an
 * integer count of line/space steps from the bottom line.
 *
 *   0 = bottom line, 1 = first space, 2 = second line, ... 8 = top line
 *
 * Even steps are lines, odd steps are spaces. Steps below 0 or above 8
 * lie off the staff (steps 9 and -1 are the spaces just outside it;
 * steps 10 and -2 are the first ledger lines above and below).
 *
 * The ledger-line count is derived from the step, never stored: a
 * position needs as many ledger lines as lie between it and the staff.
 * Pitch is NOT part of a StaffPosition -- the same position names
 * different pitches under different clefs (pitch/clef_resolution.h).
 *
 * Value type: immutable, value equality on steps.
 */

typedef struct StaffPosition StaffPosition;

typedef enum {
    STAFF_POSITION_OK = 0,
    STAFF_POSITION_ERR_ALLOC
} StaffPositionResult;

#define STAFF_POSITION_BOTTOM_LINE 0
#define STAFF_POSITION_MIDDLE_LINE 4
#define STAFF_POSITION_TOP_LINE 8
#define STAFF_POSITION_LINE_COUNT 5

StaffPositionResult staff_position_create(int steps, StaffPosition **out);
void staff_position_destroy(StaffPosition *position);

int staff_position_steps(const StaffPosition *position);

bool staff_position_is_line(const StaffPosition *position);
bool staff_position_is_space(const StaffPosition *position);

/* True when the position is on the five-line staff itself (steps 0..8). */
bool staff_position_is_on_staff(const StaffPosition *position);

/* Ledger lines needed to notate this position: 0 on or adjacent to the
   staff, 1 for steps 10/11 and -2/-3, 2 for 12/13 and -4/-5, ... */
int staff_position_ledger_line_count(const StaffPosition *position);

bool staff_position_equals(const StaffPosition *a, const StaffPosition *b);

#endif
