#include <assert.h>
#include <string.h>

#include <common/hash.h>

/* The published FNV-1a 64 values (Fowler, Noll and Vo): the empty text, "a" and "foobar". */
static void test_published_values(void) {
    assert(common_fnv1a64("", 0, COMMON_FNV1A64_OFFSET) == 0xcbf29ce484222325ULL);
    assert(common_fnv1a64("a", 1, COMMON_FNV1A64_OFFSET) == 0xaf63dc4c8601ec8cULL);
    assert(common_fnv1a64("foobar", 6, COMMON_FNV1A64_OFFSET) == 0x85944171f73967e8ULL);
}

/* A hash continues: folding two parts gives the hash of the whole, and a seed of one's own gives another hash. */
static void test_continuing_and_seeds(void) {
    uint64_t whole = common_fnv1a64("foobar", 6, COMMON_FNV1A64_OFFSET);
    uint64_t part = common_fnv1a64("foo", 3, COMMON_FNV1A64_OFFSET);
    assert(common_fnv1a64("bar", 3, part) == whole);
    assert(common_fnv1a64("foobar", 6, 1469598103934665603ULL) != whole);
    /* A NUL byte is a byte. */
    assert(common_fnv1a64("a\0b", 3, COMMON_FNV1A64_OFFSET) != common_fnv1a64("a", 1, COMMON_FNV1A64_OFFSET));
}

static void test_hex_form(void) {
    char text[17];
    common_fnv1a64_hex(0xaf63dc4c8601ec8cULL, text);
    assert(strcmp(text, "af63dc4c8601ec8c") == 0);
    common_fnv1a64_hex(0x1ULL, text);
    assert(strcmp(text, "0000000000000001") == 0);
    common_fnv1a64_hex(common_fnv1a64("", 0, COMMON_FNV1A64_OFFSET), text);
    assert(strcmp(text, "cbf29ce484222325") == 0 && strlen(text) == 16);
}

int main(void) {
    test_published_values();
    test_continuing_and_seeds();
    test_hex_form();
    return 0;
}
