#ifndef URSATZ_PLAN_SECTION_JSON_INTERNAL_H
#define URSATZ_PLAN_SECTION_JSON_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "plan/section_json.h"

/* The table of section_json.c, private to it and to the test access header (tests/support/section_json_test_access.h). */

/* What a reader of a member carries: the use, where a refusal goes, and what it has seen. */
typedef struct {
    SectionUse use;
    SectionError *error;
    bool failed;
    bool seen_beginning;
    bool seen_ending;
} Reader;

/* One row of the table: the member's name, the uses that hold it, whether the description sets it, and its reader and writer. */
typedef struct Member Member;
struct Member {
    const char *name;
    unsigned uses;
    bool (*present)(const SectionDescription *);
    bool (*read)(Reader *, const cJSON *, SectionDescription *);
    cJSON *(*write)(const SectionDescription *, SectionUse);
};

extern const Member section_json_members[];
extern const size_t section_json_member_total;

#endif
