#ifndef URSATZ_WORKSHOP_ANALYSIS_STYLE_MODEL_IO_H
#define URSATZ_WORKSHOP_ANALYSIS_STYLE_MODEL_IO_H

#include <stddef.h>

#include <theory/style/style_model_file.h>

/*
 * The style model file on disk (R-691): the library parses and prints the text of the file (style_model_file.h) and holds no file IO;
 * these read and write it with the common file helpers.
 */

/* Reads the file and parses it (style_model_file_parse); no categories dropped with count 0. IO when the file cannot be read, ARGUMENT
   for a NULL path. */
StyleModelFileResult workshop_style_model_read(const char *path, StyleModel **out, size_t *pieces);

/* Prints the profile (style_model_file_text) and writes the text to path atomically. IO when the file cannot be written. */
StyleModelFileResult workshop_style_model_write(const StyleProfile *profile, const char *name, const char *model_version, const char *path);

#endif
