/*
 * trade_io.c  -  PM Mini Project helper library for Stage A (see trade_io.h)
 *
 * Study this file in Week 8: it shows how fopen, fgets and sscanf read a file.
 * It uses sscanf, which is easy but slow. In Week 9 you will write a faster parser.
 */
#define __USE_MINGW_ANSI_STDIO 1     /* Windows: makes %lld work reliably */
#include <stdio.h>
#include <stdlib.h>                  /* getenv: finds your home folder    */
#include "trade_io.h"

static FILE     *trades_file = NULL; /* the open file                      */
static long long count = 0;          /* trades read so far                 */
static long long skipped = 0;        /* lines that could not be read       */

/* values of the current trade */
static long long cur_id, cur_time;
static double    cur_price, cur_qty, cur_quote;
static int       cur_is_buy;

/* Copy a folder name into out, replacing a leading "~" with your home folder */
static void expand_home(const char *folder, char *out, int size) {
    const char *home;
    if (folder[0] == '~') {
        home = getenv("HOME");                   /* Mac, Linux, Pi */
        if (home == NULL) home = getenv("USERPROFILE");   /* Windows */
        if (home == NULL) home = "";
        snprintf(out, size, "%s%s", home, folder + 1);
    } else {
        snprintf(out, size, "%s", folder);
    }
}

/* Open one exact path */
static int open_path(const char *path) {
    trades_file = fopen(path, "r");
    count = 0;
    skipped = 0;
    if (trades_file == NULL) {
        printf("trade_io: cannot open %s\n", path);
        printf("trade_io: check DATA_FOLDER and GROUP_FOLDER in trade_io.h\n");
        return 0;
    }
    printf("trade_io: reading %s\n", path);
    return 1;
}

int open_data(int which) {
    char folder[400], path[512];
    expand_home(DATA_FOLDER, folder, sizeof folder);

    if (which == TEST20)      snprintf(path, sizeof path, "%s/%s/test20.csv", folder, GROUP_FOLDER);
    else if (which == DEV)    snprintf(path, sizeof path, "%s/%s/dev_1M.csv", folder, GROUP_FOLDER);
    else if (which == STRESS) snprintf(path, sizeof path, "%s/stress_full.csv", folder);
    else {
        printf("trade_io: open_data() needs TEST20, DEV or STRESS\n");
        return 0;
    }
    return open_path(path);
}

int open_trades(const char *filename) {
    return open_path(filename);
}

int next_trade(void) {
    char line[256];
    char maker[16], best[16];

    if (trades_file == NULL) return 0;

    while (fgets(line, sizeof line, trades_file) != NULL) {
        /* 7 fields: id,price,qty,quoteQty,time,isBuyerMaker,isBestMatch */
        if (sscanf(line, "%lld,%lf,%lf,%lf,%lld,%15[^,],%15s",
                   &cur_id, &cur_price, &cur_qty, &cur_quote, &cur_time, maker, best) == 7) {
            /* isBuyerMaker False means a buyer started the trade: a BUY */
            cur_is_buy = (maker[0] == 'F' || maker[0] == 'f') ? 1 : 0;
            count++;
            return 1;
        }
        skipped++;                   /* e.g. a header row or a blank line */
    }
    return 0;                        /* end of file */
}

void close_trades(void) {
    if (trades_file != NULL) {
        fclose(trades_file);
        trades_file = NULL;
    }
    if (skipped > 0) {
        printf("trade_io: %lld line(s) skipped because they were not trades.\n", skipped);
    }
}

long long trade_number(void)    { return count; }
long long trade_id(void)        { return cur_id; }
double    trade_price(void)     { return cur_price; }
double    trade_qty(void)       { return cur_qty; }
double    trade_quote_qty(void) { return cur_quote; }
long long trade_time(void)      { return cur_time; }
int       trade_is_buy(void)    { return cur_is_buy; }
