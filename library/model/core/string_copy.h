#ifndef URSATZ_CORE_STRING_COPY_H
#define URSATZ_CORE_STRING_COPY_H

/*
 * A heap copy of `text` that the caller frees with free(). NULL in, NULL out; NULL on allocation failure. The one string copy
 * every module uses, so no module carries its own.
 */
char *ursatz_string_copy(const char *text);

#endif
