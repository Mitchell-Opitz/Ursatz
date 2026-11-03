#ifndef URSATZ_COMPOSER_UTIL_H
#define URSATZ_COMPOSER_UTIL_H

#include <stdbool.h>
#include <stddef.h>

/* Small helpers the sources share. Private: not installed, not part of the API. */

/* The absolute value of an int. */
int util_absolute(int value);

/* How many bits are set. */
unsigned util_bits_set(unsigned bits);

/* -1, 0 or 1 by the sign of the value. */
int util_sign(int value);

/* The pitch class, 0 to 11, of a MIDI pitch or any integer (PITCH_SEMITONES_PER_OCTAVE). */
int util_pitch_class(int pitch);

/* A MIDI pitch as "C#4" ("Db4" when prefer_flats): the library's pitch class name and the octave of middle C = 4. */
void util_pitch_name(int midi, bool prefer_flats, char *out, size_t size);

/*
 * A key as "Db major" or "C# minor": the tonic spelled with the fewest accidentals in the key signature, sharps on a
 * tie, through the standard-tonality framework. The pitch class is taken mod 12.
 */
void util_key_name(int tonic_pitch_class, bool minor, char *out, size_t size);

/*
 * The major scale (or the natural minor) on a tonic, read by standard-tonality: a 12-bit mask over pitch classes, and
 * the degree (1 to 7) a pitch is read as with its alteration from that degree (0 for a note of the scale). The pitch
 * and the tonic are taken mod 12.
 */
unsigned util_scale_mask(int tonic_pitch_class, bool minor);
unsigned util_scale_mask_melodic(int tonic_pitch_class, bool minor);
bool util_scale_degree(int tonic_pitch_class, bool minor, int pitch, int *degree, int *alteration);

/* The semitones above the tonic of a degree (1 to 7) in the major scale or the natural minor. */
int util_scale_semitone(bool minor, int degree);

/* A malloc'd copy of the text ("" for NULL), or NULL when out of memory. The caller frees it. */
char *util_copy_string(const char *text);

/*
 * Reads the whole file at path into a NUL-terminated string in *text (the caller frees it); *size, when not NULL, is
 * its length without the terminator. False, with *text NULL, when the file cannot be read.
 */
bool util_read_file(const char *path, char **text, size_t *size);

#endif
