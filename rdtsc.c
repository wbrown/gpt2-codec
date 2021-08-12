//
// Created by Wes Brown on 8/11/21.
//

// ==========================================================================
// RDTSC timing and calibration stuff
// ==========================================================================

#ifndef UNUSED
#define UNUSED(x)			(void)x
#endif
#include <time.h>
#include <stdint.h>
#include "rdtsc.h"

const int NANO_SECONDS_IN_SEC = 1000000000;
/* returns a static buffer of struct timespec with the time difference of ts1
 * and ts2, ts1 is assumed to be greater than ts2 */
struct timespec *TimeSpecDiff(struct timespec *ts1, struct timespec *ts2)
        {
    static struct timespec ts;
    ts.tv_sec = ts1->tv_sec - ts2->tv_sec;
    ts.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
    if (ts.tv_nsec < 0) {
        ts.tv_sec--;
        ts.tv_nsec += NANO_SECONDS_IN_SEC;
    }
    return &ts;
        }

#pragma GCC push_options
#pragma GCC optimize("O0")
#pragma clang push_options
#pragma clang optimize off
// For CPU load testing.
long __attribute__((noinline)) factorial(int n)
{
    asm ("");
    int c;
    volatile long result = 1;

    for (c = 1; c <= n; c++)
        result = result * c;

    return result;
}

void __attribute__ ((noinline)) CalibrateRdtscTicks()
{
    struct timespec begints, endts;
    uint64_t begin = 0, end = 0;
    clock_gettime(CLOCK_MONOTONIC, &begints);
    begin = RDTSC();
    long i = factorial(100000);
    UNUSED(i);
    end = RDTSC();
    clock_gettime(CLOCK_MONOTONIC, &endts);
    struct timespec *tmpts = TimeSpecDiff(&endts, &begints);
    uint64_t nsecElapsed = (unsigned long)tmpts->tv_sec * \
    1000000000LL + tmpts->tv_nsec;
            g_TicksPerNanoSec = (double)(end - begin)/(double)nsecElapsed;
    UNUSED(i);
}
#pragma GCC pop_options
#pragma clang pop_options