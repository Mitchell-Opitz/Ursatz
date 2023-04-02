#include <stdio.h>
#include <string.h>

#include <ursatz_composer/composer.h>

static void print_usage(FILE *out) {
    fprintf(out, "Usage: ursatz-composer [--version] [--help]\n");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("ursatz-composer %s\n", composer_version());
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }
    print_usage(stderr);
    return argc == 1 ? 0 : 1;
}
