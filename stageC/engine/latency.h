/*
 * latency.h - average, worst case and percentiles of per-trade times (Stage C)
 *
 * Every trade adds one sample (ns). The mean and maximum are kept as running
 * values. For p50 / p99 (a stretch goal) every sample is also stored in a
 * malloc'd array, which is sorted once at the end.
 */
#ifndef LATENCY_H
#define LATENCY_H

#include <stdint.h>

typedef struct {
    long long n;          /* samples added                      */
    double    sum;        /* sum of all samples, ns             */
    uint64_t  max;        /* worst case, ns                     */
    uint32_t *samples;    /* every sample (NULL = not kept)     */
    long long cap;        /* size of samples[]                  */
    int       sorted;     /* 1 once samples[] has been sorted   */
} LatStats;

/* cap = how many samples to keep for percentiles (0 = none).
 * Returns 1 on success, 0 if malloc failed. */
int      lat_init(LatStats *s, long long cap);
void     lat_add(LatStats *s, uint64_t ns);
double   lat_mean(const LatStats *s);
/* p in 0..100, e.g. 50 or 99. Sorts the samples the first time. Returns -1
 * if no samples were kept. */
double   lat_percentile(LatStats *s, double p);
void     lat_free(LatStats *s);

#endif /* LATENCY_H */
