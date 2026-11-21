#include "common/hash.h"

#include <stdio.h>

#define FNV1A64_PRIME 1099511628211ULL

uint64_t common_fnv1a64(const void *data, size_t size, uint64_t seed) {
    const unsigned char *bytes = data;
    uint64_t hash = seed;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * FNV1A64_PRIME;
    }
    return hash;
}

uint64_t common_fnv1a64_u64(uint64_t value, uint64_t seed) {
    unsigned char bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (unsigned char)((value >> (8 * i)) & 0xFFu);
    }
    return common_fnv1a64(bytes, sizeof(bytes), seed);
}

void common_fnv1a64_hex(uint64_t hash, char out[17]) {
    snprintf(out, 17, "%016llx", (unsigned long long)hash);
}

/* --- SHA-256 ------------------------------------------------------------------------------------------------------------------------ */

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t rotate_right(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32 - count));
}

static void sha256_block(uint32_t state[8], const unsigned char block[64]) {
    uint32_t w[64];
    for (size_t i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) | ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (size_t i = 16; i < 64; i++) {
        uint32_t s0 = rotate_right(w[i - 15], 7) ^ rotate_right(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotate_right(w[i - 2], 17) ^ rotate_right(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
    for (size_t i = 0; i < 64; i++) {
        uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + choose + SHA256_K[i] + w[i];
        uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void common_sha256_hex(const void *data, size_t size, char out[65]) {
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const unsigned char *bytes = data;
    size_t whole = size / 64;
    for (size_t i = 0; i < whole; i++) {
        sha256_block(state, bytes + i * 64);
    }
    unsigned char tail[128];
    size_t rest = size - whole * 64;
    for (size_t i = 0; i < rest; i++) {
        tail[i] = bytes[whole * 64 + i];
    }
    tail[rest] = 0x80;
    size_t length = rest < 56 ? 64 : 128;
    for (size_t i = rest + 1; i < length - 8; i++) {
        tail[i] = 0;
    }
    uint64_t bits = (uint64_t)size * 8;
    for (size_t i = 0; i < 8; i++) {
        tail[length - 1 - i] = (unsigned char)(bits >> (8 * i));
    }
    sha256_block(state, tail);
    if (length == 128) {
        sha256_block(state, tail + 64);
    }
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < 8; i++) {
        for (size_t k = 0; k < 4; k++) {
            unsigned char byte = (unsigned char)(state[i] >> (24 - 8 * k));
            out[i * 8 + k * 2] = HEX[byte >> 4];
            out[i * 8 + k * 2 + 1] = HEX[byte & 15];
        }
    }
    out[64] = '\0';
}
