#ifndef IMATH_H
#define IMATH_H

#include <stddef.h>
#include <stdint.h>

uint64_t gcd_chunk(uint64_t a, uint64_t b);
uint64_t isqrt_chunk(uint64_t x);
uint64_t mod_inverse(uint64_t a, uint64_t mod, int *err_out);
uint64_t fnv1a_hash(const char *s);
size_t next_pow2(size_t x);

#endif /* IMATH_H */