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

/* The vectors of FIPS 180-4: the empty text, "abc" and the 56-byte text that needs a second padding block. */
static void test_sha256(void) {
    char hex[65];
    common_sha256_hex("", 0, hex);
    assert(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
    common_sha256_hex("abc", 3, hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    const char *long_text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    common_sha256_hex(long_text, strlen(long_text), hex);
    assert(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
}

int main(void) {
    test_sha256();
    test_published_values();
    test_continuing_and_seeds();
    test_hex_form();
    return 0;
}
