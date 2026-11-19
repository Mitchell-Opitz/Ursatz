#include <assert.h>
#include <string.h>

#include "model/core/content_hash.h"

/* The published FNV-1a 64 values (Fowler, Noll and Vo): the empty text, "a" and "foobar". */
static void test_published_values(void) {
    assert(content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "", 0) == 0xcbf29ce484222325ULL);
    assert(content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "a", 1) == 0xaf63dc4c8601ec8cULL);
    assert(content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "foobar", 6) == 0x85944171f73967e8ULL);
}

/* A hash continues: folding two parts gives the hash of the whole, and a seed of one's own gives another hash. */
static void test_continuing_and_seeds(void) {
    uint64_t whole = content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "foobar", 6);
    uint64_t part = content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "foo", 3);
    assert(content_hash_fnv1a64(part, "bar", 3) == whole);
    assert(content_hash_fnv1a64(1469598103934665603ULL, "foobar", 6) != whole);
    /* A NUL byte is a byte. */
    assert(content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "a\0b", 3) != content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "a", 1));
}

static void test_hex_form(void) {
    char text[17];
    content_hash_hex16(0xaf63dc4c8601ec8cULL, text);
    assert(strcmp(text, "af63dc4c8601ec8c") == 0);
    content_hash_hex16(0x1ULL, text);
    assert(strcmp(text, "0000000000000001") == 0);
    content_hash_hex16(content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, "", 0), text);
    assert(strcmp(text, "cbf29ce484222325") == 0 && strlen(text) == 16);
}

int main(void) {
    test_published_values();
    test_continuing_and_seeds();
    test_hex_form();
    return 0;
}
