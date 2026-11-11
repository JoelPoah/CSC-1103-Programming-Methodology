/*
 * read_trade_sscanf.c - own reader, Week 8 version (see read_trade.h)
 *
 * Same idea as the helper library (fgets + sscanf), but the results come
 * back through pointers instead of through seven separate functions, and
 * the three fields we never use are skipped with "%*".
 */
#define __USE_MINGW_ANSI_STDIO 1     /* Windows: makes %lld work reliably */
#include <stdio.h>
#include "read_trade.h"

/* read_trade
 * What:    read one line and convert it with sscanf
 * Steps:   1) fgets copies the next line into line[]
 *          2) sscanf picks out price, qty, time and the True/False text
 *             ("%*d" and "%*f" read a field and throw it away)
 *          3) if all 4 were found it is a trade; otherwise try the next line
 */
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy) {
    char line[LINE_MAX_LEN];
    char maker[16];

    while (fgets(line, sizeof line, fp) != NULL) {
        /* 7 fields: id,price,qty,quoteQty,time,isBuyerMaker,isBestMatch */
        if (sscanf(line, "%*d,%lf,%lf,%*f,%lld,%15[^,]", price, qty, ts, maker) == 4) {
            /* isBuyerMaker False means a buyer started the trade: a BUY */
            if (maker[0] == 'F' || maker[0] == 'f') {
                *is_buy = 1;
            } else {
                *is_buy = 0;
            }
            return 1;
        }
    }
    return 0;   /* end of file */
}
