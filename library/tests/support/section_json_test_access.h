#ifndef URSATZ_TESTS_SUPPORT_SECTION_JSON_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_SECTION_JSON_TEST_ACCESS_H

#include "plan/section_json_internal.h"

/* What the tests read of the section JSON table beyond its public entries: its row count, a row's member name, and the SectionUse bits
   of the uses that hold it. */

static inline size_t section_json_member_count(void) {
    return section_json_member_total;
}

static inline const char *section_json_member_name(size_t index) {
    return index < section_json_member_total ? section_json_members[index].name : NULL;
}

static inline unsigned section_json_member_uses(size_t index) {
    return index < section_json_member_total ? section_json_members[index].uses : 0;
}

#endif
