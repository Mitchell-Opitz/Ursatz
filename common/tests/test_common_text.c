#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <common/strings.h>

static int g_allocations_left;

static void *limited_realloc(void *pointer, size_t size) {
    if (g_allocations_left-- <= 0) {
        return NULL;
    }
    return realloc(pointer, size);
}

static void test_append(void) {
    CommonText text;
    common_text_init(&text);
    assert(common_text_append(&text, "abc") && common_text_append(&text, ""));
    assert(common_text_appendf(&text, "-%d-%s", 42, "x"));
    assert(text.length == 8 && strcmp(text.data, "abc-42-x") == 0 && !text.failed);
    char big[1000];
    memset(big, 'z', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    assert(common_text_appendf(&text, "%s", big)); /* longer than any small buffer */
    assert(text.length == 8 + 999 && text.data[text.length - 1] == 'z');
    char *taken = common_text_take(&text);
    assert(taken && strlen(taken) == 1007 && text.data == NULL && text.length == 0);
    free(taken);
    /* nothing appended: take gives an empty string */
    taken = common_text_take(&text);
    assert(taken && taken[0] == '\0');
    free(taken);
    common_text_free(&text);
}

static void test_json_string(void) {
    CommonText text;
    common_text_init(&text);
    assert(common_text_append_json_string(&text, "plain"));
    assert(common_text_append(&text, " "));
    assert(common_text_append_json_string(&text, "q\"b\\n\n\t\x01"));
    assert(strcmp(text.data, "\"plain\" \"q\\\"b\\\\n\\n\\t\\u0001\"") == 0);
    common_text_free(&text);
}

static void test_sticky_failure(void) {
    CommonText text;
    common_text_init(&text);
    g_allocations_left = 1;
    common_text_set_realloc(limited_realloc);
    assert(common_text_append(&text, "fits")); /* the first allocation succeeds */
    char big[600];
    memset(big, 'y', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    assert(!common_text_append(&text, big)); /* the second needs more room and fails */
    assert(text.failed);
    common_text_set_realloc(NULL);
    assert(!common_text_append(&text, "more") && !common_text_appendf(&text, "%d", 1) && !common_text_append_json_string(&text, "x"));
    assert(common_text_take(&text) == NULL);
    common_text_free(&text);
    assert(!text.failed && text.data == NULL);
    assert(common_text_append(&text, "again") && strcmp(text.data, "again") == 0);
    common_text_free(&text);
}

static void test_names(void) {
    CommonNames names = {0};
    assert(!common_names_contains(&names, "b"));
    assert(common_names_add(&names, "b") && common_names_add(&names, "a") && common_names_add(&names, "B") &&
           common_names_add(&names, "a"));
    assert(names.count == 4 && common_names_contains(&names, "B") && !common_names_contains(&names, "c"));
    common_names_sort(&names);
    assert(strcmp(names.items[0], "B") == 0 && strcmp(names.items[1], "a") == 0 && strcmp(names.items[2], "a") == 0 &&
           strcmp(names.items[3], "b") == 0);
    common_names_free(&names);
    assert(names.items == NULL && names.count == 0);
    common_names_sort(&names);
}

static void test_strings(void) {
    char *copy = common_strdup("copy");
    assert(copy && strcmp(copy, "copy") == 0);
    free(copy);
    assert(common_strdup(NULL) == NULL);

    char padded[] = "  \t two words \n";
    assert(common_trim(padded) == 9 && strcmp(padded, "two words") == 0);
    char blank[] = " \t ";
    assert(common_trim(blank) == 0 && blank[0] == '\0');
    char tight[] = "x";
    assert(common_trim(tight) == 1 && strcmp(tight, "x") == 0);

    assert(common_compare_ci("Hello", "hello") == 0);
    assert(common_compare_ci("apple", "Banana") < 0 && common_compare_ci("b", "A") > 0);
    assert(common_compare_ci("ab", "abc") < 0 && common_compare_ci("abc", "ab") > 0 && common_compare_ci("", "") == 0);

    char now[32];
    common_utc_now(now);
    assert(strlen(now) == 20 && now[4] == '-' && now[10] == 'T' && now[13] == ':' && now[19] == 'Z');
}

int main(void) {
    test_append();
    test_json_string();
    test_sticky_failure();
    test_names();
    test_strings();
    return 0;
}
