/*
 * trades_fx_malloc.c - trades in a malloc'd array that fits the file exactly
 *                      (Stage C, see trades_fx.h)
 */
#include <stdio.h>
#include <stdlib.h>     /* malloc, free */
#include "trades_fx.h"
#include "../reader/read_trade_fx.h"

#define COUNT_BLOCK 65536   /* read the file 64 KB at a time when counting */

/* count_lines
 * What:    count how many lines the file has
 * In/Out:  path = the CSV file. Returns the line count, or -1 on error.
 * Steps:   read the file in big blocks and count the '\n' characters.
 * Why:     this pass does NOT convert any numbers, so it is much cheaper
 *          than the real read in load_trades_fx.
 * Watch:   the last line may not end with '\n', so it is counted separately.
 */
static long long count_lines(const char *path) {
    static char block[COUNT_BLOCK];
    long long lines = 0;
    size_t got, i;
    char last = '\n';
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        printf("cannot open %s\n", path);
        return -1;
    }
    while ((got = fread(block, 1, sizeof block, f)) > 0) {
        for (i = 0; i < got; i++) {
            if (block[i] == '\n') {
                lines++;
            }
        }
        last = block[got - 1];
    }
    if (last != '\n') {
        lines++;            /* final line without a newline */
    }
    fclose(f);
    return lines;
}

/* load_trades_fx
 * What:    make an array that fits the file exactly and copy every trade into it
 * In/Out:  path = the CSV file, *count = number of trades loaded.
 *          Returns the array (give it back with free_trades_fx), or NULL on error.
 * Steps:   1) count the lines -> capacity
 *          2) malloc capacity TradeFx structs, and check malloc did not fail
 *          3) open the file and fill trades[n]
 *          4) close the file and return the array
 * Watch:   every error path frees the array first, so nothing leaks.
 */
TradeFx *load_trades_fx(const char *path, long long *count) {
    long long capacity, n = 0;
    TradeFx *trades;
    FILE *trades_file;

    *count = 0;

    /* 1) how many trades can the file hold? */
    capacity = count_lines(path);
    if (capacity <= 0) {
        /* -1 = could not open; 0 = empty file (malloc(0) is not useful) */
        if (capacity == 0) {
            printf("%s is empty\n", path);
        }
        return NULL;
    }

    /* 2) ask for exactly enough memory. sizeof(TradeFx) is 32 bytes.
     *    (size_t) makes the multiplication use the type malloc expects. */
    trades = malloc((size_t)capacity * sizeof(TradeFx));
    if (trades == NULL) {
        printf("malloc failed: not enough memory for %lld trades\n", capacity);
        return NULL;
    }

    /* 3) same loop as the global-array version, but into the malloc'd array */
    trades_file = fopen(path, "r");
    if (trades_file == NULL) {
        printf("cannot open %s\n", path);
        free(trades);
        return NULL;
    }
    while (n < capacity &&
           read_trade_fx(trades_file, &trades[n].time, &trades[n].price,
                         &trades[n].qty, &trades[n].is_buy)) {
        n++;
    }
    fclose(trades_file);

    /* 4) n can be a little smaller than capacity if some lines were not
     *    trades (e.g. a header or blank line); those few slots stay unused. */
    *count = n;
    return trades;
}

/* free_trades_fx
 * What:    give the memory back to the system
 */
void free_trades_fx(TradeFx *trades) {
    free(trades);
}

/* trades_fx_bytes
 * What:    only as many bytes as there are trades
 */
long long trades_fx_bytes(long long count) {
    return count * (long long)sizeof(TradeFx);
}
