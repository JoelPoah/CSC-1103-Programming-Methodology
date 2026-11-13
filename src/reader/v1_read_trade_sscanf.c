/*
 * v1_read_trade_sscanf.c - Stage A+ first own reader: fgets + sscanf (V1)
 *
 * Same job and same return values as v2-v4_read_trade_fast.c (see read_trade.h),
 * but each line is converted with the sscanf format string from the
 * course helper (helper_function/trade_io.c). The only difference from V0
 * is that the values come back through pointer parameters instead of the
 * helper's global getters; V2 then replaces sscanf with the digit parser.
 */
#include <stdio.h>
#include <string.h>
#include "read_trade.h"

/* Read ONE trade and hand back four values through pointer parameters.
 * Return 1 = trade, 0 = EOF, -1 = malformed row / read error.
 * Blank lines are ignored; the supplied Binance files have no header.
 * Outputs change only after the complete row has been converted.
 */
int read_trade(FILE *fp, long long *ts, double *price,
               double *qty, int *is_buy) {
    char line[256];
    while (fgets(line, sizeof line, fp) != NULL) {
        long long id, timestamp;
        double parsed_price, parsed_qty, quote;
        char maker[16], best[16];

        /* Reject an overlong record instead of parsing its fragments. */
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            int ch = fgetc(fp);
            if (ch != EOF) return -1;
            if (ferror(fp)) return -1;
        }
        if (line[0] == '\0' || line[0] == '\n' ||
            (line[0] == '\r' && (line[1] == '\n' || line[1] == '\0'))) continue;
        /* 7 fields: id,price,qty,quoteQty,time,isBuyerMaker,isBestMatch */
        if (sscanf(line, "%lld,%lf,%lf,%lf,%lld,%15[^,],%15s",
                   &id, &parsed_price, &parsed_qty, &quote, &timestamp,
                   maker, best) != 7) return -1;

        *ts = timestamp;
        *price = parsed_price;
        *qty = parsed_qty;
        /* False isBuyerMaker means buyer-initiated trade. */
        *is_buy = (maker[0] == 'F' || maker[0] == 'f') ? 1 : 0;
        return 1;
    }
    if (ferror(fp)) {
        return -1;
    } else {
        return 0;
    }
}
