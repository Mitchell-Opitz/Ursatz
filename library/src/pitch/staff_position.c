#include "pitch/staff_position.h"

#include <stdlib.h>
#include <string.h>
#include "core/value.h"

struct StaffPosition {
    Value *value;
};

typedef struct {
    int steps;
} StaffPositionContent;

StaffPositionResult staff_position_create(int steps, StaffPosition **out) {
    StaffPositionContent content;
    memset(&content, 0, sizeof(content));
    content.steps = steps;

    StaffPosition *position = (StaffPosition *)malloc(sizeof(StaffPosition));
    if (position == NULL) {
        return STAFF_POSITION_ERR_ALLOC;
    }

    if (value_create("StaffPosition", &content, sizeof(content), &position->value) != VALUE_OK) {
        free(position);
        return STAFF_POSITION_ERR_ALLOC;
    }

    *out = position;
    return STAFF_POSITION_OK;
}

void staff_position_destroy(StaffPosition *position) {
    if (position == NULL) {
        return;
    }
    value_destroy(position->value);
    free(position);
}

int staff_position_steps(const StaffPosition *position) {
    return ((const StaffPositionContent *)value_content(position->value, NULL))->steps;
}

bool staff_position_is_line(const StaffPosition *position) {
    return staff_position_steps(position) % 2 == 0;
}

bool staff_position_is_space(const StaffPosition *position) {
    return !staff_position_is_line(position);
}

bool staff_position_is_on_staff(const StaffPosition *position) {
    int steps = staff_position_steps(position);
    return steps >= STAFF_POSITION_BOTTOM_LINE && steps <= STAFF_POSITION_TOP_LINE;
}

int staff_position_ledger_line_count(const StaffPosition *position) {
    int steps = staff_position_steps(position);
    if (steps > STAFF_POSITION_TOP_LINE) {
        return (steps - STAFF_POSITION_TOP_LINE) / 2;
    }
    if (steps < STAFF_POSITION_BOTTOM_LINE) {
        return (STAFF_POSITION_BOTTOM_LINE - steps) / 2;
    }
    return 0;
}

bool staff_position_equals(const StaffPosition *a, const StaffPosition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}
