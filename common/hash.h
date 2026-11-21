#ifndef URSATZ_COMMON_HASH_H
#define URSATZ_COMMON_HASH_H

#include <stddef.h>
#include <stdint.h>

/*
 * The one FNV-1a 64 of the project (R-622, R-638): the hash of a file's bytes, a snapshot's entries, a form document, a
 * realization. The offset is the published seed; a caller that continues a hash passes the value it has, and one that wants another
 * seed (a hash that must not equal another's) passes its own.
 */
#define COMMON_FNV1A64_OFFSET 14695981039346656037ULL

/* Folds size bytes into seed: for each byte in order, seed xor the byte, times the FNV prime (hash.c). Returns the new hash. */
uint64_t common_fnv1a64(const void *data, size_t size, uint64_t seed);

/* Folds a 64-bit value into seed as 8 bytes, least significant first (the same on every platform): the hash of a number in a larger
   structure. Returns the new hash. */
uint64_t common_fnv1a64_u64(uint64_t value, uint64_t seed);

/* The hash as 16 lowercase hexadecimal digits and a terminating NUL. */
void common_fnv1a64_hex(uint64_t hash, char out[17]);

/* The SHA-256 of size bytes as 64 lowercase hexadecimal digits and a terminating NUL (FIPS 180-4): for a file's identity where FNV-1a is too short. */
void common_sha256_hex(const void *data, size_t size, char out[65]);

#endif
