#ifndef MUSIC_CORE_VALUE_H
#define MUSIC_CORE_VALUE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Value: base type for immutable semantic content. Identity derives from
 * the value's own content, not from any assigned identifier — two Value
 * instances built independently from equal content are equal. Base type
 * for Duration, MusicalTime, PitchClass, Interval, Tempo, etc. (concrete
 * values live at L2+).
 *
 * A Value has no identity field and no mutators: content is copied on
 * creation and there is no API to change it afterward.
 */

typedef struct Value Value;

typedef enum {
    VALUE_OK = 0,
    VALUE_ERR_NULL_TYPE,
    VALUE_ERR_NULL_CONTENT,
    VALUE_ERR_ALLOC
} ValueResult;

/* Copies content_size bytes from content; the Value owns its own copy. */
ValueResult value_create(const char *type, const void *content, size_t content_size, Value **out);
void value_destroy(Value *value);

const char *value_type(const Value *value);
const void *value_content(const Value *value, size_t *out_size);

/* Value equality: same type and same content bytes, regardless of instance. */
bool value_equals(const Value *a, const Value *b);

#endif