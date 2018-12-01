#include "core/value.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void test_create_rejects_null_type(void) {
    int content = 4;
    Value *value = NULL;
    ValueResult r = value_create(NULL, &content, sizeof(content), &value);
    assert(r == VALUE_ERR_NULL_TYPE);
    assert(value == NULL);
}

static void test_create_rejects_null_content(void) {
    Value *value = NULL;
    ValueResult r = value_create("Duration", NULL, 0, &value);
    assert(r == VALUE_ERR_NULL_CONTENT);
    assert(value == NULL);
}

static void test_create_accepts_valid(void) {
    int content = 4;
    Value *value = NULL;
    ValueResult r = value_create("Duration", &content, sizeof(content), &value);
    assert(r == VALUE_OK);
    assert(value != NULL);
    value_destroy(value);
}

/* Immutability: mutating the caller's buffer after creation must not
   change the Value's own content, since it holds a defensive copy. */
static void test_immutable_to_caller_buffer_mutation(void) {
    int content = 4;
    Value *value = NULL;
    value_create("Duration", &content, sizeof(content), &value);

    content = 99;

    size_t stored_size = 0;
    const void *stored = value_content(value, &stored_size);
    assert(stored_size == sizeof(int));
    assert(*(const int *)stored == 4);

    value_destroy(value);
}

/* Property: value equality holds for equal-content instances constructed
   independently, regardless of instance or backing memory. */
static void test_value_equality_independent_instances(void) {
    int a_content = 4;
    int b_content = 4;

    Value *a = NULL;
    Value *b = NULL;
    value_create("Duration", &a_content, sizeof(a_content), &a);
    value_create("Duration", &b_content, sizeof(b_content), &b);

    assert(a != b);
    assert(value_equals(a, b));

    value_destroy(a);
    value_destroy(b);
}

static void test_value_inequality_different_content(void) {
    int a_content = 4;
    int b_content = 5;

    Value *a = NULL;
    Value *b = NULL;
    value_create("Duration", &a_content, sizeof(a_content), &a);
    value_create("Duration", &b_content, sizeof(b_content), &b);

    assert(!value_equals(a, b));

    value_destroy(a);
    value_destroy(b);
}

static void test_value_inequality_different_type_same_content(void) {
    int content = 4;

    Value *a = NULL;
    Value *b = NULL;
    value_create("Duration", &content, sizeof(content), &a);
    value_create("Tempo", &content, sizeof(content), &b);

    assert(!value_equals(a, b));

    value_destroy(a);
    value_destroy(b);
}

int main(void) {
    test_create_rejects_null_type();
    test_create_rejects_null_content();
    test_create_accepts_valid();
    test_immutable_to_caller_buffer_mutation();
    test_value_equality_independent_instances();
    test_value_inequality_different_content();
    test_value_inequality_different_type_same_content();
    return 0;
}