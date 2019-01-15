#include "containers/voice.h"

#include <assert.h>
#include <stddef.h>

static Voice *make_voice(const char *id_value) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    Voice *voice = NULL;
    voice_create(id, &voice);
    return voice;
}

static void test_create_rejects_null_id(void) {
    Voice *voice = NULL;
    VoiceResult r = voice_create(NULL, &voice);
    assert(r == VOICE_ERR_NULL_ID);
    assert(voice == NULL);
}

static void test_create_accepts_valid(void) {
    Voice *voice = make_voice("voice-1");
    assert(voice != NULL);
    voice_destroy(voice);
}

static void test_id_accessor_returns_constructed_id(void) {
    Voice *voice = make_voice("voice-1");
    const EntityID *id = voice_id(voice);
    assert(id != NULL);
    assert(entity_id_value(id) != NULL);
    voice_destroy(voice);
}

static void test_identity_equals_same_id(void) {
    Voice *a = make_voice("voice-1");
    Voice *b = make_voice("voice-1");
    assert(voice_identity_equals(a, b));
    voice_destroy(a);
    voice_destroy(b);
}

static void test_identity_equals_regardless_of_instance(void) {
    Voice *a = make_voice("voice-1");
    Voice *b = make_voice("voice-1");
    assert(a != b);
    assert(voice_identity_equals(a, b));
    voice_destroy(a);
    voice_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Voice *a = make_voice("voice-1");
    Voice *b = make_voice("voice-2");
    assert(!voice_identity_equals(a, b));
    voice_destroy(a);
    voice_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_accepts_valid();
    test_id_accessor_returns_constructed_id();
    test_identity_equals_same_id();
    test_identity_equals_regardless_of_instance();
    test_identity_not_equal_with_different_id();
    return 0;
}