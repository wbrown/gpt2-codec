//
// Created by Wes Brown on 8/11/21.
//

#ifndef GPT2_CODEC_RDTSC_H
#define GPT2_CODEC_RDTSC_H

#include <stdint.h>

#ifdef __x86_64__
static inline uint64_t RDTSC() {
    unsigned int hi, lo;
    __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}
#elif defined(__arch64__) && !defined(__APPLE__)
static inline uint64_t RDTSC() {
    uint64_t pmccntr;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(pmccntr));
    pmccntr;
}
#else
#include <time.h>
static inline uint64_t RDTSC() {
    return (uint64_t)clock();\
    }
#endif

    void CalibrateRdtscTicks();
    double g_TicksPerNanoSec;

#endif //GPT2_CODEC_RDTSC_H
