#include "imath.h"

#include "err.h"

uint64_t gcd_chunk(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

uint64_t isqrt_chunk(uint64_t x) {
    /* Integer square root (floor) without libm. */
    uint64_t r = 0;
    uint64_t bit = 1ULL << 62; /* Highest power-of-four <= 2^64 */
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

uint64_t mod_inverse(uint64_t a, uint64_t mod, int *err_out) {
    if (mod == 0) {
        if (err_out) *err_out = HC_ERR_INVALID;
        return 0;
    }

    /* Extended Euclid in signed 128-bit space. */
    __int128 t = 0;
    __int128 newt = 1;
    __int128 r = (__int128)mod;
    __int128 newr = (__int128)(a % mod);

    while (newr != 0) {
        __int128 q = r / newr;

        __int128 tmp_t = newt;
        newt = t - q * newt;
        t = tmp_t;

        __int128 tmp_r = newr;
        newr = r - q * newr;
        r = tmp_r;
    }

    if (r != 1) {
        if (err_out) *err_out = HC_ERR_MATH;
        return 0;
    }

    if (t < 0) t += mod;
    if (t >= mod) t %= mod;
    return (uint64_t)t;
}

uint64_t fnv1a_hash(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

size_t next_pow2(size_t x) {
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}
