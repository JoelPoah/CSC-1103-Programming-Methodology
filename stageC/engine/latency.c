/*
 * latency.c - latency statistics (see latency.h)
 */
#include <stdlib.h>     /* malloc, free, qsort */
#include "latency.h"

int lat_init(LatStats *s, long long cap) {
    s->n = 0;
    s->sum = 0.0;
    s->max = 0;
    s->samples = NULL;
    s->cap = 0;
    s->sorted = 0;
    if (cap > 0) {
        s->samples = malloc((size_t)cap * sizeof(uint32_t));
        if (s->samples == NULL) {
            return 0;
        }
        s->cap = cap;
    }
    return 1;
}

/* lat_add
 * What:    record one time. Samples above ~4.29 s are clipped to fit in
 *          32 bits (4 bytes per trade instead of 8 halves the memory).
 */
void lat_add(LatStats *s, uint64_t ns) {
    if (s->samples != NULL && s->n < s->cap) {
        s->samples[s->n] = ns > UINT32_MAX ? UINT32_MAX : (uint32_t)ns;
    }
    s->n++;
    s->sum += (double)ns;
    if (ns > s->max) {
        s->max = ns;
    }
    s->sorted = 0;
}

double lat_mean(const LatStats *s) {
    return s->n > 0 ? s->sum / (double)s->n : 0.0;
}

/* compare two uint32 for qsort (smallest first) */
static int cmp_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

/* lat_percentile
 * What:    the value below which p% of the samples fall
 * Steps:   sort once (O(n log n)), then pick index p/100 x (count - 1)
 */
double lat_percentile(LatStats *s, double p) {
    long long count = s->n < s->cap ? s->n : s->cap;
    long long idx;

    if (s->samples == NULL || count == 0) {
        return -1.0;
    }
    if (!s->sorted) {
        qsort(s->samples, (size_t)count, sizeof(uint32_t), cmp_u32);
        s->sorted = 1;
    }
    idx = (long long)(p / 100.0 * (double)(count - 1) + 0.5);
    return (double)s->samples[idx];
}

void lat_free(LatStats *s) {
    free(s->samples);
    s->samples = NULL;
    s->cap = 0;
}
