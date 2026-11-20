/*
 * test_sort.c - checks and times the middle-pivot quicksort (stageC/engine/percentile.c)
 *
 * Compares three sorts on the same data:
 *   joel   = Joel's quicksort.c: last element as pivot, Lomuto partition
 *   middle = our quick_sort: middle pivot, Hoare partition
 *   qsort  = the C library's qsort, used ONLY here as the reference answer
 *            and speed to beat (it needs a compare function, see below)
 *
 * Four kinds of data, n values each:
 *   random   values spread from 0 to about 1,000,000,000 (few repeats)
 *   timing   like our processing times: 99% between 40 and 60 ns, 1% up to 100 us
 *   sorted   already in order (0, 1, 2, ...)
 *   equal    every value the same
 *
 * Checks: every sort's result must be in order and equal to qsort's result,
 * and percentile() must give the hand-worked answers in check_percentile().
 *
 * Usage (from the project root):  ./stageC/test_sort [n]      (default n = 1000000)
 * Joel's sort is only run when n <= 20000: on "timing", "sorted" and "equal"
 * it takes about n*n/2 steps and recurses n calls deep.
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* malloc, free, rand, qsort, atoi */
#include "engine/percentile.h"

#define JOEL_MAX 20000

/* ---- Joel's quicksort.c, unchanged except int -> long long ---- */
long long joel_partition(long long arr[], long long low, long long high) {
    long long j = low;
    long long i = low - 1;
    while (j < high) {
        if (arr[j] < arr[high]) {
            i++;
            swap_ll(&arr[i], &arr[j]);
        }
        j++;
    }
    i++;
    swap_ll(&arr[i], &arr[j]);
    return i;
}

void joel_quick_sort(long long arr[], long long low, long long high) {
    long long p;
    if (low < high) {
        p = joel_partition(arr, low, high);
        joel_quick_sort(arr, low, p - 1);
        joel_quick_sort(arr, p + 1, high);
    }
}

/* qsort needs a function that says which of two values comes first */
int compare_ll(const void *a, const void *b) {
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;
    if (x < y) {
        return -1;
    }
    if (x > y) {
        return 1;
    }
    return 0;
}

/* a random number from 0 to about 1,000,000,000 (rand() alone may stop at 32767) */
long long big_rand(void) {
    return ((long long)rand() * 32768 + rand() % 32768) % 1000000000LL;
}

/* fill arr with n values of the given kind (0 random, 1 timing, 2 sorted, 3 equal) */
void make_data(long long arr[], long long n, int kind) {
    long long k;
    for (k = 0; k < n; k++) {
        if (kind == 0) {
            arr[k] = big_rand();
        } else if (kind == 1) {
            if (rand() % 100 == 0) {
                arr[k] = 60 + big_rand() % 100000;
            } else {
                arr[k] = 40 + rand() % 21;
            }
        } else if (kind == 2) {
            arr[k] = k;
        } else {
            arr[k] = 50;
        }
    }
}

/* 1 if a[0..n-1] equals b[0..n-1] and is in order, else 0 */
int same_and_sorted(long long a[], long long b[], long long n) {
    long long k;
    for (k = 0; k < n; k++) {
        if (a[k] != b[k]) {
            return 0;
        }
        if (k > 0 && a[k] < a[k - 1]) {
            return 0;
        }
    }
    return 1;
}

/* hand-worked percentile answers; returns the number of failures */
int check_percentile(void) {
    long long one[1] = {7};
    long long ten[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    long long big[200];
    long long k;
    int fails = 0;

    for (k = 0; k < 200; k++) {
        big[k] = k + 1;               /* 1, 2, ..., 200 */
    }
    /* p50 of 1..10: rank ceil(5) = 5 -> 5;  p99: rank ceil(9.9) = 10 -> 10 */
    if (percentile(ten, 10, 50) != 5)   { printf("FAIL p50 of 1..10\n");  fails++; }
    if (percentile(ten, 10, 99) != 10)  { printf("FAIL p99 of 1..10\n");  fails++; }
    /* p50 of 1..200: rank 100 -> 100;  p99: rank 198 -> 198 */
    if (percentile(big, 200, 50) != 100) { printf("FAIL p50 of 1..200\n"); fails++; }
    if (percentile(big, 200, 99) != 198) { printf("FAIL p99 of 1..200\n"); fails++; }
    /* one value: every percentile is that value */
    if (percentile(one, 1, 50) != 7)    { printf("FAIL p50 of {7}\n");     fails++; }
    if (percentile(one, 1, 99) != 7)    { printf("FAIL p99 of {7}\n");     fails++; }
    if (percentile(one, 0, 50) != 0)    { printf("FAIL empty array\n");    fails++; }
    return fails;
}

int main(int argc, char *argv[]) {
    char *kind_name[4] = {"random", "timing", "sorted", "equal"};
    long long n = 1000000;
    long long *data, *ref, *work;
    long long t0, t_qsort, t_middle, t_joel;
    long long k;
    int kind, fails = 0;

    if (argc > 1) {
        n = atoi(argv[1]);
    }
    if (n < 1) {
        printf("n must be at least 1\n");
        return 1;
    }
    data = malloc(n * sizeof(long long));
    ref = malloc(n * sizeof(long long));
    work = malloc(n * sizeof(long long));
    if (data == NULL || ref == NULL || work == NULL) {
        printf("out of memory for n = %lld\n", n);
        return 1;
    }

    fails = fails + check_percentile();
    if (fails == 0) {
        printf("percentile() hand-worked checks: pass\n");
    } else {
        printf("percentile() hand-worked checks: FAIL\n");
    }
    printf("n = %lld values per test (times in ms)\n\n", n);
    printf("%-8s %10s %10s %10s   %s\n", "data", "qsort", "middle", "joel", "result");

    srand(1);
    for (kind = 0; kind < 4; kind++) {
        make_data(data, n, kind);

        /* reference: qsort */
        for (k = 0; k < n; k++) {
            ref[k] = data[k];
        }
        t0 = now_ns();
        qsort(ref, n, sizeof(long long), compare_ll);
        t_qsort = now_ns() - t0;

        /* ours: middle pivot, Hoare partition */
        for (k = 0; k < n; k++) {
            work[k] = data[k];
        }
        t0 = now_ns();
        quick_sort(work, 0, n - 1);
        t_middle = now_ns() - t0;
        if (!same_and_sorted(work, ref, n)) {
            printf("FAIL: middle-pivot sort is wrong on %s\n", kind_name[kind]);
            fails++;
        }

        /* Joel's: last pivot, Lomuto partition (small n only) */
        t_joel = -1;
        if (n <= JOEL_MAX) {
            for (k = 0; k < n; k++) {
                work[k] = data[k];
            }
            t0 = now_ns();
            joel_quick_sort(work, 0, n - 1);
            t_joel = now_ns() - t0;
            if (!same_and_sorted(work, ref, n)) {
                printf("FAIL: Joel's sort is wrong on %s\n", kind_name[kind]);
                fails++;
            }
        }

        printf("%-8s %10.2f %10.2f ", kind_name[kind], t_qsort / 1e6, t_middle / 1e6);
        if (t_joel >= 0) {
            printf("%10.2f", t_joel / 1e6);
        } else {
            printf("%10s", "skipped");
        }
        printf("   p50 %lld  p99 %lld\n", percentile(ref, n, 50), percentile(ref, n, 99));
    }

    if (fails == 0) {
        printf("\nALL CHECKS PASS\n");
    } else {
        printf("\nSOME CHECKS FAILED\n");
    }
    free(data);
    free(ref);
    free(work);
    return fails != 0;
}
