#ifndef URSATZ_PLAN_SECTION_JSON_H
#define URSATZ_PLAN_SECTION_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include "plan/section.h"

/* cJSON.h declares it; a header of this group needs only the name. */
typedef struct cJSON cJSON;

/*
 * The JSON of a section description, read and written through one table with one row per member: its name, the uses that
 * hold it, its reader and its writer. Nothing else names a member, so a member is added in the table and in the schema
 * fragment (library/plan/schema/section.schema.json) and nowhere else; a test fails when the two differ (R-605).
 */

/*
 * Reads a section object for one use. `skip` names members the caller reads itself (not read, not refused). An unknown member
 * is refused as "unknown member \"<name>\"", a member of another use as "\"<name>\" is not given here"; then the description
 * is checked (section_description_check). On a refusal *error names the member as a path relative to the object.
 */
bool section_from_json(const cJSON *object, SectionUse use, const char *const *skip, size_t skip_count,
                       SectionDescription *out, SectionError *error);

/* The object of a description for one use: its members in table order, only those set and allowed for the use. NULL when out of memory. */
cJSON *section_to_json(const SectionDescription *description, SectionUse use);

/* The table: its row count, a row's member name, and the SectionUse bits of the uses that hold it. */
size_t section_json_member_count(void);
const char *section_json_member_name(size_t index);
unsigned section_json_member_uses(size_t index);

#endif
