#ifndef URSATZ_MODEL_CORE_CONTENT_HASH_H
#define URSATZ_MODEL_CORE_CONTENT_HASH_H

#include <stddef.h>
#include <stdint.h>

/*
 * The one FNV-1a 64 of the project (R-622): the hash of a file's bytes, a snapshot's entries, a form document, a realization.
 * The offset is the published seed; a caller that continues a hash passes the value it has, and one that wants another seed (a
 * hash that must not equal another's) passes its own.
 */
#define CONTENT_HASH_FNV1A64_OFFSET 14695981039346656037ULL

/* Folds size bytes into hash: for each byte in order, hash xor the byte, times the FNV prime (content_hash.c). */
uint64_t content_hash_fnv1a64(uint64_t hash, const void *data, size_t size);

/* The hash as 16 lowercase hexadecimal digits and a terminating NUL. */
void content_hash_hex16(uint64_t hash, char out[17]);

#endif
