/*
 * v6_trades_fx_malloc.c - fixed-point trades in a malloc'd array (Leroi, V6)
 *
 * Leroi's Stage C loader (stageC/engine/trades_malloc.c) with the TradeFx
 * struct from V5, so the only difference between V5 and V6 is WHERE the
 * array lives, not what is in it:
 *   1) count the lines in the file
 *   2) malloc exactly that many TradeFx structs
 *   3) read the file again to fill them in (read_trade_fx, Lucas)
 */
#include <stdio.h>
#include <stdlib.h>     /* malloc, free */
#include "trades_fx.h"
#include "../reader/read_trade_fx.h"

/* count_lines
 * What:    count how many lines the file has
 * In/Out:  path = the CSV file. Returns the line count, or -1 on error.
 * Steps:   read the file one line at a time with fgets and add 1 per line.
 * Why:     this pass does NOT convert any numbers, so it is much cheaper
 *          than the real read in load_trades_fx.
 * Watch:   a CSV row is about 80 characters, far less than the 256-char
 *          buffer, so one fgets call reads exactly one line. Even if that
 *          were ever wrong, load_trades_fx still never writes past the
 *          end of the array (it checks n < capacity).
 */
static long long count_lines(const char *path) {
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

/* load_trades_fx
 * What:    make an array that fits the file exactly and copy every trade into it
 * In/Out:  path = the CSV file, *n_out = number of trades loaded.
 *          Returns the array (give it back with free_trades_fx), or NULL on error.
 * Steps:   1) count the lines -> capacity
 *          2) malloc capacity TradeFx structs, and check malloc did not fail
 *          3) open the file and fill trades[n]
 *          4) close the file and return the array
 * Watch:   every error path frees the array first, so nothing leaks.
 */
TradeFx *load_trades_fx(const char *path, long long *n_out) {
    long long capacity, n = 0;
    int status;
    TradeFx t;
    TradeFx *trades;
    FILE *fp;

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

    /* 2) ask for exactly enough memory. sizeof(TradeFx) is 32 bytes.
     *    (size_t) makes the multiplication use the type malloc expects. */
    trades = malloc((size_t)capacity * sizeof(TradeFx));
    if (trades == NULL) {
        printf("malloc failed: not enough memory for %lld trades\n", capacity);
        return NULL;
    }

    /* 3) same loop as the V5 loader, but into the malloc'd array */
    fp = fopen(path, "r");
    if (fp == NULL) {
        printf("cannot open %s\n", path);
        free(trades);
        return NULL;
    }
    while ((status = read_trade_fx(fp, &t)) == 1) {
        if (n >= capacity) {
            /* stop instead of writing outside the array */
            printf("more trades than lines in %s (should never happen)\n", path);
            fclose(fp);
            free(trades);
            return NULL;
        }
        trades[n] = t;
        n++;
    }
    fclose(fp);
    if (status < 0) {
        printf("%s: invalid row or read error after trade %lld\n", path, n);
        free(trades);
        return NULL;
    }

    /* 4) n can be a little smaller than capacity if some lines were blank;
     *    those few slots stay unused. */
    *n_out = n;
    return trades;
}

/* free_trades_fx
 * What:    give the malloc'd array back
 */
void free_trades_fx(TradeFx *trades) {
    free(trades);
}
