//----------------------------------------------------------------------------------------------------------------------
// Random number generator
// Uses the MerseK_RANDOM_TABLE_SIZEe Twister based on code by Takuji Nishimura and Makoto Matsumoto (copyright 2004).
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_platform.h>

#define K_RANDOM_TABLE_SIZE     312

typedef struct  
{
    u64     table[K_RANDOM_TABLE_SIZE];
    u64     index;
}
Random;

// Initialise based on time.
void randomInit(Random* R);

// Initialise with a single seed value.
void randomInitSeed(Random* R, u64 seed);

// Initialise with an array of seeds.
void randomInitArray(Random* R, u64* seeds, i64 len);

// Return a 64-bit pseudo-random number.
u64 random64(Random* R);

// Return a random float inclusive [0, 1].
f64 randomFloat(Random* R);

// Return a random float exclusive [0, 1).
f64 randomFloatNo1(Random* R);

// Return a random float really exclusive (0, 1).
f64 randomFloatNo0Or1(Random* R);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

#include <time.h>

#define MM 156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL    // Most significant 33 bits
#define LM 0x7FFFFFFFULL            // Least significant 31 bits

void randomInit(Random* R)
{
    __time64_t t = _time64(&t);
    randomInitSeed(R, t);
}

void randomInitSeed(Random* R, u64 seed)
{
    R->table[0] = seed;
    for (int i = 1; i < K_RANDOM_TABLE_SIZE; ++i)
        R->table[i] = (6364136223846793005ULL * (R->table[i - 1] ^ (R->table[i - 1] >> 62)) + i);
    R->index = K_RANDOM_TABLE_SIZE;
}

void randomInitArray(Random* R, u64* seeds, i64 len)
{
    u64 i, j, k;
    randomInitSeed(R, 19650218ULL);
    i = 1; j = 0;
    k = (K_RANDOM_TABLE_SIZE > len ? K_RANDOM_TABLE_SIZE : len);
    for (; k; k--) {
        R->table[i] = (R->table[i] ^ ((R->table[i - 1] ^ (R->table[i - 1] >> 62)) * 3935559000370003845ULL))
            + seeds[j] + j;
        ++i; ++j;
        if (i >= K_RANDOM_TABLE_SIZE) { R->table[0] = R->table[K_RANDOM_TABLE_SIZE - 1]; i = 1; }
        if ((i64)j >= len) j = 0;
    }
    for (k = K_RANDOM_TABLE_SIZE - 1; k; k--) {
        R->table[i] = (R->table[i] ^ ((R->table[i - 1] ^ (R->table[i - 1] >> 62)) * 2862933555777941757ULL)) - i;
        i++;
        if (i >= K_RANDOM_TABLE_SIZE) { R->table[0] = R->table[K_RANDOM_TABLE_SIZE - 1]; i = 1; }
    }

    R->table[0] = 1ULL << 63;
}

u64 random64(Random* R)
{
    int i;
    u64 x;
    static u64 mag01[2] = { 0ULL, MATRIX_A };

    if (R->index >= K_RANDOM_TABLE_SIZE)
    {
        // Generate K_RANDOM_TABLE_SIZE words at one time.
        for (i = 0; i < K_RANDOM_TABLE_SIZE - MM; ++i) {
            x = (R->table[i] & UM) | (R->table[i + 1] & LM);
            R->table[i] = R->table[i + MM] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
        }
        for (; i < K_RANDOM_TABLE_SIZE - 1; ++i) {
            x = (R->table[i] & UM) | (R->table[i + 1] & LM);
            R->table[i] = R->table[i + (MM - K_RANDOM_TABLE_SIZE)] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
        }
        x = (R->table[K_RANDOM_TABLE_SIZE - 1] & UM) | (R->table[0] & LM);
        R->table[K_RANDOM_TABLE_SIZE - 1] = R->table[MM - 1] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];

        R->index = 0;
    }

    x = R->table[R->index++];

    x ^= (x >> 29) & 0x5555555555555555ULL;
    x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
    x ^= (x << 37) & 0xFFF7EEE000000000ULL;
    x ^= (x >> 43);

    return x;
}

#undef MM
#undef MATRIX_A
#undef UM
#undef LM


f64 randomFloat(Random* R)
{
    return (random64(R) >> 11) * (1.0 / 9007199254740991.0);
}

f64 randomFloatNo1(Random* R)
{
    return (random64(R) >> 11) * (1.0 / 9007199254740992.0);
}

f64 randomFloatNo0Or1(Random* R)
{
    return ((random64(R) >> 12) + 0.5) * (1.0 / 4503599627370496.0);
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
