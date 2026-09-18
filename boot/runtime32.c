#include <stddef.h>

typedef unsigned long long u64;

void *memcpy(void *restrict destination, const void *restrict source, size_t length) {
    unsigned char *d = destination;
    const unsigned char *s = source;
    while (length--) *d++ = *s++;
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    unsigned char *d = destination;
    while (length--) *d++ = (unsigned char)value;
    return destination;
}

int memcmp(const void *left, const void *right, size_t length) {
    const unsigned char *a = left;
    const unsigned char *b = right;
    while (length--) {
        if (*a != *b) return (int)*a - (int)*b;
        ++a;
        ++b;
    }
    return 0;
}

static u64 udivmod64(u64 numerator, u64 denominator, u64 *remainder) {
    if (denominator == 0) {
        if (remainder) *remainder = 0;
        return 0;
    }

    u64 quotient = 0;
    u64 rem = 0;
    u64 bit = 1ULL << 63;

    for (unsigned int i = 0; i < 64; ++i) {
        unsigned int carry = (unsigned int)(rem >> 63);
        rem = (rem << 1) | ((numerator & bit) != 0 ? 1ULL : 0ULL);

        if (carry || rem >= denominator) {
            rem -= denominator;
            quotient |= bit;
        }

        bit >>= 1;
    }

    if (remainder) *remainder = rem;
    return quotient;
}

u64 __udivdi3(u64 numerator, u64 denominator) {
    return udivmod64(numerator, denominator, NULL);
}

u64 __umoddi3(u64 numerator, u64 denominator) {
    u64 remainder = 0;
    (void)udivmod64(numerator, denominator, &remainder);
    return remainder;
}
