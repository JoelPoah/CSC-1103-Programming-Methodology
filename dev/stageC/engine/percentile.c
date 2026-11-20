/*
 * percentile.c - quicksort with a middle pivot, and p50 / p99 (Stage C stretch goal, Lucas)
 *
 * Starts from Joel's quicksort.c. Same three parts: swap, partition, quick_sort.
 *
 * Change 1: middle pivot.
 *   Joel's version uses the LAST element as the pivot. If the array is already
 *   sorted (or nearly sorted), the last element is the biggest, so every
 *   partition splits n values into n-1 and 0. That takes n*n/2 steps instead
 *   of about n*log2(n), and the recursion goes n calls deep.
 *   The MIDDLE element of a sorted section is its median, so the split is even.
 *
 * Change 2: Hoare partition instead of Lomuto.
 *   Our timings repeat a lot: on the Pi most mem trades take 50 ns or 51 ns.
 *   Lomuto only moves values that are "< pivot" to the left, so when every value
 *   equals the pivot, everything stays on one side. That is n*n/2 steps again,
 *   whichever pivot is picked. Hoare's version walks in from BOTH ends and also
 *   stops on values equal to the pivot, so equal values are shared out evenly.
 *
 * Example with arr = [7, 2, 9, 4, 5] (low = 0, high = 4), pivot = arr[2] = 9:
 *   i walks right until arr[i] >= 9:  i = 2 (9)
 *   j walks left  until arr[j] <= 9:  j = 4 (5)
 *   i < j, so swap:                   [7, 2, 5, 4, 9]
 *   i walks right: i = 4 (9);  j walks left: j = 3 (4)
 *   i >= j, so stop and return j = 3: left part is arr[0..3], right part arr[4..4]
 *   Every value in the left part is <= 9, every value in the right part is >= 9.
 *
 * Only lecture C: functions, arrays, pointers, while loops, if / else, recursion.
 */
#include "percentile.h"

/* swap_ll
 * What:   swap the two values that a and b point to (same as Joel's swap)
 */
void swap_ll(long long *a, long long *b) {
    long long temp = *a;
    *a = *b;
    *b = temp;
}

/* partition_middle (Hoare partition, middle pivot)
 * What:    split arr[low..high] into a left part and a right part, so that
 *          every left value <= pivot <= every right value
 * Steps:   1) pivot = the value in the middle of the section
 *          2) i starts just before low, j just after high
 *          3) move i right until arr[i] >= pivot
 *             move j left  until arr[j] <= pivot
 *          4) if i and j have not crossed, swap arr[i] and arr[j] and repeat 3)
 *             otherwise stop: j is the last index of the left part
 * Returns: j. The left part is arr[low..j], the right part is arr[j+1..high].
 * Unlike Lomuto, the pivot does not end up at its final place, so quick_sort
 * sorts both parts including j (not "p - 1" and "p + 1").
 */
long long partition_middle(long long arr[], long long low, long long high) {
    long long pivot = arr[low + (high - low) / 2];
    long long i = low - 1;
    long long j = high + 1;

    while (i < j) {
        i++;
        while (arr[i] < pivot) {
            i++;
        }
        j--;
        while (arr[j] > pivot) {
            j--;
        }
        if (i < j) {
            swap_ll(&arr[i], &arr[j]);
        }
    }
    return j;
}

/* quick_sort
 * What:   sort arr[low..high] from smallest to largest
 * Steps:  base case: 0 or 1 value (low >= high) is already sorted
 *         otherwise split with partition_middle, then sort each part.
 *         The smaller part is sorted by a recursive call and the bigger part by
 *         going round the while loop again, so the recursion is never more than
 *         about log2(n) calls deep (about 23 for 8.9 million values).
 */
void quick_sort(long long arr[], long long low, long long high) {
    long long p;

    while (low < high) {
        p = partition_middle(arr, low, high);
        if (p - low < high - p) {
            quick_sort(arr, low, p);       /* left part is smaller */
            low = p + 1;
        } else {
            quick_sort(arr, p + 1, high);  /* right part is smaller */
            high = p;
        }
    }
}

/* percentile
 * What:    the p-th percentile of a sorted array (nearest-rank method)
 * Steps:   the p-th percentile is the value at rank ceil(p * n / 100), counting
 *          ranks from 1. In whole numbers, ceil(a / 100) = (a + 99) / 100.
 *          The array index is rank - 1.
 * Example: n = 200, p = 99 -> rank = (19800 + 99) / 100 = 198 -> sorted[197]
 * Returns: the value, or 0 if the array is empty
 */
long long percentile(long long sorted[], long long n, int p) {
    long long rank;

    if (n <= 0) {
        return 0;
    }
    rank = (p * n + 99) / 100;
    if (rank < 1) {
        rank = 1;
    }
    if (rank > n) {
        rank = n;
    }
    return sorted[rank - 1];
}
