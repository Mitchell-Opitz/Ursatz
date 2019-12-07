#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <midi-file-path>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return 1;
    }

    fclose(file);
    printf("Received file: %s\n", path);
    return 0;
}
