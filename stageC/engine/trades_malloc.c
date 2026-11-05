/*
 * trades_malloc.c - load all trades into a malloc'd array (see trades_malloc.h)
 */
#include <stdio.h>
#include <stdlib.h>     /* malloc, free */
#include "trades_malloc.h"
#include "../../helper_function/trade_io.h"

/* count_lines
 * What:    count how many lines the file has
 * In/Out:  path = the CSV file. Returns the line count, or -1 on error.
 * Steps:   read the file one line at a time with fgets and add 1 per line.
 * Why:     this pass does NOT convert any numbers (no sscanf), so it is much
 *          cheaper than the real read in load_trades_malloc.
 * Watch:   a CSV row is about 80 characters, far less than the 256-char
 *          buffer, so one fgets call reads exactly one line. Even if that
 *          were ever wrong, load_trades_malloc still never writes past the
 *          end of the array (it checks n < capacity).
 */
long long count_lines(const char *path) {
    char line[256];
    long long lines = 0;
    FILE *f = fopen(path, "r");

    if (f == NULL) {
        printf("cannot open %s\n", path);
        return -1;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        lines++;
    }
    fclose(f);
    return lines;
}

/* load_trades_malloc
 * What:    make an array that fits the file exactly and copy every trade into it
 * In/Out:  path = the CSV file, *n_out = number of trades loaded.
 *          Returns the array (caller must free it), or NULL on error.
 * Steps:   1) count the lines -> capacity
 *          2) malloc capacity Trade structs, and check malloc did not fail
 *          3) open the file with the helper library and fill trades[n]
 *          4) close the file and return the array
 * Watch:   every error path frees the array first, so nothing leaks.
 */
Trade *load_trades_malloc(const char *path, long long *n_out) {
    long long capacity, n = 0;
    Trade *trades;

    *n_out = 0;

    /* 1) how many trades can the file hold? */
    capacity = count_lines(path);
    if (capacity <= 0) {
        /* -1 = could not open; 0 = empty file (malloc(0) is not useful) */
        if (capacity == 0) {
            printf("%s is empty\n", path);
        }
        return NULL;
    }

    /* 2) ask for exactly enough memory. sizeof(Trade) is 32 bytes.
     *    (size_t) makes the multiplication use the type malloc expects. */
    trades = malloc((size_t)capacity * sizeof(Trade));
    if (trades == NULL) {
        printf("malloc failed: not enough memory for %lld trades\n", capacity);
        return NULL;
    }

    /* 3) same loop as Stage B's load_trades, but into the malloc'd array */
    if (!open_trades(path)) {
        free(trades);
        return NULL;
    }
    while (next_trade()) {
        if (n >= capacity) {
            /* stop instead of writing outside the array */
            printf("more trades than lines in %s (should never happen)\n", path);
            close_trades();
            free(trades);
            return NULL;
        }
        trades[n].time   = trade_time();
        trades[n].price  = trade_price();
        trades[n].qty    = trade_qty();
        trades[n].is_buy = trade_is_buy();
        n++;
    }
    close_trades();

    /* 4) n can be a little smaller than capacity if some lines were not
     *    trades (e.g. a header or blank line); those few slots stay unused. */
    *n_out = n;
    return trades;
}
