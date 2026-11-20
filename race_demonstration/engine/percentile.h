/*
 * percentile.h - quicksort with a middle pivot, and p50 / p99 (Stage C stretch goal, Lucas)
 *
 * Built on Joel's quicksort.c (swap, partition, quickSort), with two changes:
 *   1) the pivot is the MIDDLE element, not the last one
 *   2) the partition is Hoare's (two pointers walking towards each other)
 *      instead of Lomuto's (one pointer walking left to right)
 * Both changes are explained in percentile.c.
 */
#ifndef PERCENTILE_H
#define PERCENTILE_H

/* swap two values (as in Joel's version, but long long for nanosecond times) */
void swap_ll(long long *a, long long *b);

/* sort arr[low..high] from smallest to largest */
void quick_sort(long long arr[], long long low, long long high);

/* the p-th percentile (p = 1..100) of a SORTED array of n values */
long long percentile(long long sorted[], long long n, int p);

#endif /* PERCENTILE_H */
