#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>
#include <common/json.h>

static cJSON *parse(const char *text) {
    cJSON *value = cJSON_Parse(text);
    assert(value);
    return value;
}

static void test_files(void) {
    char folder[64];
    snprintf(folder, sizeof(folder), "common-json-%ld", (long)getpid());
    assert(common_dir_make(folder, NULL) == URSATZ_OK);
    char path[128];
    assert(common_path_join(path, sizeof(path), folder, "value.json"));

    cJSON *value = parse("{\"a\":1,\"b\":[true,\"x\"]}");
    assert(common_json_write_file(path, value, false, NULL) == URSATZ_OK);
    char *text = NULL;
    assert(common_file_read(path, &text, NULL, NULL) == URSATZ_OK && strcmp(text, "{\"a\":1,\"b\":[true,\"x\"]}") == 0);
    free(text);
    assert(common_json_write_file(path, value, true, NULL) == URSATZ_OK);
    assert(common_file_read(path, &text, NULL, NULL) == URSATZ_OK && strchr(text, '\n') != NULL && strchr(text, '\t') != NULL);
    free(text);
    cJSON_Delete(value);

    cJSON *read = NULL;
    assert(common_json_read_file(path, &read, NULL) == URSATZ_OK && cJSON_IsObject(read));
    cJSON_Delete(read);
    cJSON *object = NULL;
    assert(common_json_read_object(path, &object, NULL) == URSATZ_OK && cJSON_IsObject(object));
    cJSON_Delete(object);

    char broken[128];
    UrsatzError error;
    assert(common_path_join(broken, sizeof(broken), folder, "broken.json"));
    assert(common_file_write_atomic(broken, "{\"a\":", 5, NULL) == URSATZ_OK);
    assert(common_json_read_file(broken, &read, &error) == URSATZ_ERR_FORMAT && read == NULL);
    assert(strcmp(error.path, broken) == 0 && strstr(error.message, "broken.json"));

    char array[128];
    assert(common_path_join(array, sizeof(array), folder, "array.json"));
    assert(common_file_write_atomic(array, "[1,2]", 5, NULL) == URSATZ_OK);
    assert(common_json_read_file(array, &read, NULL) == URSATZ_OK && cJSON_IsArray(read));
    cJSON_Delete(read);
    assert(common_json_read_object(array, &object, &error) == URSATZ_ERR_FORMAT && object == NULL);

    char missing[128];
    assert(common_path_join(missing, sizeof(missing), folder, "missing.json"));
    assert(common_json_read_file(missing, &read, &error) == URSATZ_ERR_NOT_FOUND && read == NULL);
    assert(common_json_read_object(missing, &object, NULL) == URSATZ_ERR_NOT_FOUND);
    assert(common_dir_remove(folder, NULL) == URSATZ_OK);
}

static void test_members(void) {
    cJSON *object = parse("{\"s\":\"text\",\"n\":12,\"f\":1.5,\"big\":1e30,\"neg\":-7,\"t\":true,\"z\":null,\"o\":{}}");
    assert(common_json_member(object, "s") && !common_json_member(object, "absent"));
    cJSON *array = parse("[1]");
    assert(common_json_member(array, "s") == NULL && common_json_member(NULL, "s") == NULL);
    cJSON_Delete(array);

    assert(strcmp(common_json_string(object, "s"), "text") == 0);
    assert(strcmp(common_json_string(object, "n"), "") == 0 && strcmp(common_json_string(object, "absent"), "") == 0);
    assert(common_json_string_or_null(object, "s") != NULL && common_json_string_or_null(object, "n") == NULL);
    char *copy = common_json_string_copy(object, "s");
    assert(copy && strcmp(copy, "text") == 0);
    free(copy);
    copy = common_json_string_copy(object, "absent");
    assert(copy && copy[0] == '\0');
    free(copy);

    long long number = 0;
    assert(common_json_long(object, "n", &number) && number == 12);
    assert(common_json_long(object, "neg", &number) && number == -7);
    assert(!common_json_long(object, "f", &number) && !common_json_long(object, "big", &number));
    assert(!common_json_long(object, "s", &number) && !common_json_long(object, "absent", &number));
    assert(!common_json_long(object, "t", &number) && !common_json_long(NULL, "n", &number));

    bool flag = false;
    assert(common_json_bool(object, "t", &flag) && flag);
    assert(!common_json_bool(object, "n", &flag) && !common_json_bool(object, "absent", &flag));
    cJSON_Delete(object);
}

static void test_building(void) {
    cJSON *object = cJSON_CreateObject();
    assert(common_json_add(object, "a", common_json_number(5)));
    assert(!common_json_add(object, "b", NULL));
    assert(common_json_set_string(object, "name", "first") && common_json_set_string(object, "name", "second"));
    assert(cJSON_GetArraySize(object) == 2 && strcmp(common_json_string(object, "name"), "second") == 0);

    cJSON *array = cJSON_CreateArray();
    assert(common_json_append(array, common_json_number(-3)) && !common_json_append(array, NULL));
    assert(cJSON_GetArraySize(array) == 1 && cJSON_GetArrayItem(array, 0)->valuedouble == -3);
    cJSON_Delete(array);

    cJSON *source = parse("{\"keep\":[1,2],\"nothing\":null}");
    cJSON *copy = cJSON_CreateObject();
    assert(common_json_add_copy_or_null(copy, source, "keep") && common_json_add_copy_or_null(copy, source, "absent"));
    assert(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(copy, "keep")) && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(copy, "absent")));
    cJSON_Delete(copy);
    copy = cJSON_CreateObject();
    assert(common_json_add_copy_if(copy, source, "keep") && common_json_add_copy_if(copy, source, "absent") &&
           common_json_add_copy_if(copy, source, "nothing"));
    assert(cJSON_GetArraySize(copy) == 1 && cJSON_GetObjectItemCaseSensitive(copy, "keep"));
    cJSON_Delete(copy);
    cJSON_Delete(source);
    cJSON_Delete(object);

    char *indented = common_json_indent("{\n\t\"a\": {\n\t\t\"b\": 1\n\t}\n}");
    assert(indented && strcmp(indented, "{\n  \"a\": {\n    \"b\": 1\n  }\n}\n") == 0);
    free(indented);
}

static bool read_forms(const char *json, unsigned forms, long long *num, long long *den) {
    cJSON *value = parse(json);
    bool ok = common_json_rational_read(value, forms, num, den);
    cJSON_Delete(value);
    return ok;
}

static void test_rational_forms(void) {
    long long n = 0, d = 0;
    /* each form is accepted only when its bit is set */
    assert(read_forms("{\"num\":3,\"den\":4}", COMMON_RATIONAL_OBJECT, &n, &d) && n == 3 && d == 4);
    assert(!read_forms("{\"num\":3,\"den\":4}", COMMON_RATIONAL_TEXT | COMMON_RATIONAL_INTEGER, &n, &d));
    assert(read_forms("\"3/4\"", COMMON_RATIONAL_TEXT, &n, &d) && n == 3 && d == 4);
    assert(read_forms("\"-5\"", COMMON_RATIONAL_TEXT, &n, &d) && n == -5 && d == 1);
    assert(!read_forms("\"3/4\"", COMMON_RATIONAL_OBJECT | COMMON_RATIONAL_INTEGER, &n, &d));
    assert(read_forms("7", COMMON_RATIONAL_INTEGER, &n, &d) && n == 7 && d == 1);
    assert(!read_forms("7", COMMON_RATIONAL_OBJECT | COMMON_RATIONAL_TEXT, &n, &d));
    assert(!read_forms("7.5", COMMON_RATIONAL_INTEGER, &n, &d));
    assert(read_forms("7", COMMON_RATIONAL_OBJECT | COMMON_RATIONAL_TEXT | COMMON_RATIONAL_INTEGER, &n, &d) && n == 7);
    assert(!read_forms("true", 15u, &n, &d) && !read_forms("null", 15u, &n, &d) && !read_forms("[1,2]", 15u, &n, &d));
    assert(!read_forms("{}", COMMON_RATIONAL_OBJECT, &n, &d) && !read_forms("{\"num\":1}", COMMON_RATIONAL_OBJECT, &n, &d));
    assert(!read_forms("{\"num\":1.5,\"den\":2}", COMMON_RATIONAL_OBJECT, &n, &d));
    assert(!read_forms("{\"num\":\"1\",\"den\":2}", COMMON_RATIONAL_OBJECT, &n, &d));
    /* the fraction is not reduced */
    assert(read_forms("{\"num\":6,\"den\":8}", COMMON_RATIONAL_OBJECT, &n, &d) && n == 6 && d == 8);
    /* a malformed text */
    const char *bad_texts[] = {"\"\"", "\"a/4\"", "\"3/\"", "\"3/4x\"", "\"3 /4\"", "\"/4\"", "\"3/4/5\""};
    for (size_t i = 0; i < sizeof(bad_texts) / sizeof(bad_texts[0]); i++) {
        assert(!read_forms(bad_texts[i], COMMON_RATIONAL_TEXT, &n, &d));
    }
}

static void test_rational_denominators(void) {
    long long n = 0, d = 0;
    /* without ZERO_DEN the denominator must be positive */
    assert(!read_forms("{\"num\":1,\"den\":0}", COMMON_RATIONAL_OBJECT, &n, &d));
    assert(!read_forms("{\"num\":1,\"den\":-2}", COMMON_RATIONAL_OBJECT, &n, &d));
    assert(!read_forms("\"1/0\"", COMMON_RATIONAL_TEXT, &n, &d) && !read_forms("\"1/-2\"", COMMON_RATIONAL_TEXT, &n, &d));
    /* with it a zero denominator is read (the Composer's "none"), a negative one still is not */
    assert(read_forms("{\"num\":0,\"den\":0}", COMMON_RATIONAL_OBJECT | COMMON_RATIONAL_ZERO_DEN, &n, &d) && n == 0 && d == 0);
    assert(read_forms("\"5/0\"", COMMON_RATIONAL_TEXT | COMMON_RATIONAL_ZERO_DEN, &n, &d) && n == 5 && d == 0);
    assert(!read_forms("{\"num\":1,\"den\":-1}", COMMON_RATIONAL_OBJECT | COMMON_RATIONAL_ZERO_DEN, &n, &d));
    assert(read_forms("{\"num\":-1,\"den\":3}", COMMON_RATIONAL_OBJECT, &n, &d) && n == -1 && d == 3);
}

static void test_rational_writers(void) {
    cJSON *object = common_json_rational_object(6, 8);
    assert(object && cJSON_GetObjectItemCaseSensitive(object, "num")->valuedouble == 6 &&
           cJSON_GetObjectItemCaseSensitive(object, "den")->valuedouble == 8);
    long long n = 0, d = 0;
    assert(common_json_rational_read(object, COMMON_RATIONAL_OBJECT, &n, &d) && n == 6 && d == 8);
    char *printed = cJSON_PrintUnformatted(object);
    assert(strcmp(printed, "{\"num\":6,\"den\":8}") == 0);
    free(printed);
    cJSON_Delete(object);

    char text[16];
    assert(common_json_rational_text(-3, 4, text, sizeof(text)) && strcmp(text, "-3/4") == 0);
    char tight[8];
    assert(!common_json_rational_text(123456789, 987654321, tight, sizeof(tight)));
    cJSON *string = cJSON_CreateString(text);
    assert(common_json_rational_read(string, COMMON_RATIONAL_TEXT, &n, &d) && n == -3 && d == 4);
    cJSON_Delete(string);
}

int main(void) {
    test_files();
    test_members();
    test_building();
    test_rational_forms();
    test_rational_denominators();
    test_rational_writers();
    return 0;
}
