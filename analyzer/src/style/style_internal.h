#ifndef URSATZ_ANALYZER_STYLE_INTERNAL_H
#define URSATZ_ANALYZER_STYLE_INTERNAL_H

/* The whole file as a NUL-terminated string the caller frees; NULL when it cannot be read. */
char *style_read_file(const char *path);

#endif
