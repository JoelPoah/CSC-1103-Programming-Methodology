/*
 * stats.c - median, mean, mode, min, max and standard deviation of the runs
 */
#include "bench.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int compare_doubles(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* bin = mode resolution in ms: run times never repeat exactly, so they are
 * rounded to the nearest bin first (1 ms for C, 100 ms for Python) */
Stats stats_of(const double *v, int n, double bin) {
    Stats s;
    double sorted[MAX_RUNS], sum = 0, sq = 0;
    int i, j;

    memset(&s, 0, sizeof s);
    s.n = n;
    s.bin = bin;
    if (n <= 0) return s;

    /* STEP 1 - sort a copy: min, max and median come straight from it */
    memcpy(sorted, v, (size_t)n * sizeof(double));
    qsort(sorted, (size_t)n, sizeof(double), compare_doubles);
    s.min = sorted[0];
    s.max = sorted[n - 1];
    s.median = (n % 2) ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2;

    /* STEP 2 - mean and sample standard deviation */
    for (i = 0; i < n; i++) sum += v[i];
    s.mean = sum / n;
    for (i = 0; i < n; i++) sq += (v[i] - s.mean) * (v[i] - s.mean);
    s.sd = n > 1 ? sqrt(sq / (n - 1)) : 0;

    /* STEP 3 - mode: the longest run of equal rounded values in the sorted copy
     * (on a tie the smallest value wins) */
    for (i = 0; i < n; i = j) {
        double key = floor(sorted[i] / bin + 0.5);
        for (j = i; j < n && floor(sorted[j] / bin + 0.5) == key; j++) ;
        if (j - i > s.mode_count) {
            s.mode_count = j - i;
            s.mode = key * bin;
        }
    }
    return s;
}

/* 1234.5 -> "1.23 s", 87.6 -> "87.6 ms", 0 -> "-" */
void fmt_ms(double ms, char *out, size_t size) {
    if (ms <= 0)         snprintf(out, size, "-");
    else if (ms >= 1000) snprintf(out, size, "%.2f s", ms / 1000);
    else                 snprintf(out, size, "%.1f ms", ms);
}

void fmt_mode(const Stats *s, char *out, size_t size) {
    char v[32];
    if (s->mode_count < 2) {
        snprintf(out, size, "none (no two runs equal at %.0f ms resolution)", s->bin);
        return;
    }
    fmt_ms(s->mode, v, sizeof v);
    snprintf(out, size, "%s (%d of %d runs, rounded to %.0f ms)", v, s->mode_count, s->n, s->bin);
}
