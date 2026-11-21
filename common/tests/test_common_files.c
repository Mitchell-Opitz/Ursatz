#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>

static char g_root[64];

static void put(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static void test_path_helpers(void) {
    char out[32];
    assert(common_path_join(out, sizeof(out), "a", "b") && strcmp(out, "a/b") == 0);
    assert(common_path_join(out, sizeof(out), "a/", "b") && strcmp(out, "a/b") == 0);
    assert(!common_path_join(out, 4, "abc", "d"));
    assert(common_path_is_dir(g_root) && common_path_exists(g_root));
    char file[96];
    assert(common_path_join(file, sizeof(file), g_root, "plain.txt"));
    assert(!common_path_exists(file));
    put(file, "x");
    assert(common_path_exists(file) && !common_path_is_dir(file));
}

static void test_read(void) {
    char path[96];
    assert(common_path_join(path, sizeof(path), g_root, "read.txt"));
    put(path, "hello\nworld");
    char *data = NULL;
    size_t size = 0;
    UrsatzError error;
    assert(common_file_read(path, &data, &size, &error) == URSATZ_OK);
    assert(size == 11 && strcmp(data, "hello\nworld") == 0);
    free(data);
    assert(common_file_read(path, &data, NULL, NULL) == URSATZ_OK);
    free(data);
    put(path, "");
    assert(common_file_read(path, &data, &size, NULL) == URSATZ_OK && size == 0 && data[0] == '\0');
    free(data);
    char large[96];
    assert(common_path_join(large, sizeof(large), g_root, "large.bin"));
    FILE *file = fopen(large, "wb");
    for (int i = 0; i < 20000; i++) {
        fputs("0123456789", file);
    }
    fclose(file);
    assert(common_file_read(large, &data, &size, NULL) == URSATZ_OK && size == 200000 && strlen(data) == 200000);
    free(data);
    char missing[96];
    assert(common_path_join(missing, sizeof(missing), g_root, "missing.txt"));
    assert(common_file_read(missing, &data, &size, &error) == URSATZ_ERR_NOT_FOUND);
    assert(data == NULL && size == 0 && strcmp(error.path, missing) == 0 && strstr(error.message, "missing.txt"));
}

static void test_write_atomic(void) {
    char path[96];
    assert(common_path_join(path, sizeof(path), g_root, "atomic.txt"));
    assert(common_file_write_atomic(path, "first", 5, NULL) == URSATZ_OK);
    assert(common_file_write_atomic(path, "second version", 14, NULL) == URSATZ_OK);
    char *data = NULL;
    assert(common_file_read(path, &data, NULL, NULL) == URSATZ_OK && strcmp(data, "second version") == 0);
    free(data);
    assert(common_file_write_atomic(path, "", 0, NULL) == URSATZ_OK);
    assert(common_file_read(path, &data, NULL, NULL) == URSATZ_OK && data[0] == '\0');
    free(data);
    /* no temporary is left beside the file */
    CommonNames names = {0};
    assert(common_dir_list(g_root, &names, NULL) == URSATZ_OK);
    for (size_t i = 0; i < names.count; i++) {
        assert(strstr(names.items[i], ".tmp") == NULL);
    }
    common_names_free(&names);
    /* a folder that is not there is a failure, and nothing is created */
    char bad[128];
    UrsatzError error;
    assert(common_path_join(bad, sizeof(bad), g_root, "no-folder/file.txt"));
    assert(common_file_write_atomic(bad, "x", 1, &error) == URSATZ_ERR_NOT_FOUND);
    assert(strcmp(error.path, bad) == 0);
}

static void test_append(void) {
    char path[96];
    assert(common_path_join(path, sizeof(path), g_root, "append.log"));
    assert(common_file_append_sync(path, "one\n", 4, NULL) == URSATZ_OK);
    assert(common_file_append_sync(path, "two\n", 4, NULL) == URSATZ_OK);
    char *data = NULL;
    assert(common_file_read(path, &data, NULL, NULL) == URSATZ_OK && strcmp(data, "one\ntwo\n") == 0);
    free(data);
}

static void test_folders(void) {
    char nested[128];
    char leaf[160];
    assert(common_path_join(nested, sizeof(nested), g_root, "a/b/c"));
    assert(common_dir_make(nested, NULL) == URSATZ_OK && common_path_is_dir(nested));
    assert(common_dir_make(nested, NULL) == URSATZ_OK); /* an existing folder is OK */
    assert(common_path_join(leaf, sizeof(leaf), nested, "leaf.txt"));
    put(leaf, "x");
    UrsatzError error;
    assert(common_dir_make(leaf, &error) == URSATZ_ERR_CONFLICT); /* a file where a folder is wanted */
    char fresh[128];
    assert(common_path_join(fresh, sizeof(fresh), g_root, "fresh"));
    assert(common_dir_create(fresh, NULL) == URSATZ_OK);
    assert(common_dir_create(fresh, &error) == URSATZ_ERR_CONFLICT && strcmp(error.path, fresh) == 0);
    assert(common_dir_sync(fresh, NULL) == URSATZ_OK);
    char top[128];
    assert(common_path_join(top, sizeof(top), g_root, "a"));
    assert(common_dir_remove(top, NULL) == URSATZ_OK && !common_path_exists(top));
    assert(common_dir_remove(top, NULL) == URSATZ_OK); /* absent is OK */
    assert(common_dir_remove(fresh, NULL) == URSATZ_OK && !common_path_exists(fresh));
}

static void test_list(void) {
    char folder[96];
    assert(common_path_join(folder, sizeof(folder), g_root, "listed"));
    assert(common_dir_make(folder, NULL) == URSATZ_OK);
    const char *names[] = {"zeta", "alpha", "Mid", "beta.json", ".hidden", "a b"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[160];
        assert(common_path_join(path, sizeof(path), folder, names[i]));
        put(path, "x");
    }
    CommonNames listed = {0};
    assert(common_dir_list(folder, &listed, NULL) == URSATZ_OK);
    const char *want[] = {".hidden", "Mid", "a b", "alpha", "beta.json", "zeta"}; /* bytewise, no "." or ".." */
    assert(listed.count == 6);
    for (size_t i = 0; i < 6; i++) {
        assert(strcmp(listed.items[i], want[i]) == 0);
    }
    common_names_free(&listed);
    char missing[96];
    UrsatzError error;
    assert(common_path_join(missing, sizeof(missing), g_root, "absent"));
    assert(common_dir_list(missing, &listed, &error) == URSATZ_ERR_NOT_FOUND && listed.count == 0);
}

static void test_join_alloc_copy_parent(void) {
    char *joined = common_path_join_alloc("dir", "file.txt");
    assert(joined && strcmp(joined, "dir/file.txt") == 0);
    free(joined);
    joined = common_path_join_alloc("dir/", "file.txt");
    assert(joined && strcmp(joined, "dir/file.txt") == 0);
    free(joined);
    /* an empty folder part leaves the stored relative path as it is */
    joined = common_path_join_alloc("", "rel/file.txt");
    assert(joined && strcmp(joined, "rel/file.txt") == 0);
    free(joined);
    assert(common_path_join_alloc(NULL, "x") == NULL && common_path_join_alloc("x", NULL) == NULL);

    char from[160], to[160], deep[200];
    assert(common_path_join(from, sizeof(from), g_root, "from.txt"));
    assert(common_path_join(to, sizeof(to), g_root, "to.txt"));
    put(from, "copy me");
    assert(common_file_copy(from, to, NULL) == URSATZ_OK);
    char *data = NULL;
    size_t size = 0;
    assert(common_file_read(to, &data, &size, NULL) == URSATZ_OK && size == 7 && strcmp(data, "copy me") == 0);
    free(data);
    assert(common_file_copy(from, from, NULL) == URSATZ_OK);
    UrsatzError error = {0};
    char missing[160];
    assert(common_path_join(missing, sizeof(missing), g_root, "absent.txt"));
    assert(common_file_copy(missing, to, &error) == URSATZ_ERR_NOT_FOUND);

    snprintf(deep, sizeof(deep), "%s/a/b/c.txt", g_root);
    assert(common_dir_make_parent(deep, NULL) == URSATZ_OK);
    char folder[200];
    snprintf(folder, sizeof(folder), "%s/a/b", g_root);
    assert(common_path_is_dir(folder));
    assert(common_dir_make_parent(deep, NULL) == URSATZ_OK);
    assert(common_dir_make_parent("bare.txt", NULL) == URSATZ_OK);
    assert(common_dir_make_parent("/top.txt", NULL) == URSATZ_OK);
}

int main(void) {
    snprintf(g_root, sizeof(g_root), "common-files-%ld", (long)getpid());
    assert(common_dir_make(g_root, NULL) == URSATZ_OK);
    test_path_helpers();
    test_read();
    test_write_atomic();
    test_append();
    test_folders();
    test_list();
    test_join_alloc_copy_parent();
    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    return 0;
}
