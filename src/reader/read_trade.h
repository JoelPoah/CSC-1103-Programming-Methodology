/*
 * read_trade.h - Stage A+ own trade reader (Jaren)
 *
 * One header, two versions. Link exactly ONE of them:
 *   v1_read_trade_sscanf.c  - fgets + sscanf                      (V1)
 *   v2-v4_read_trade_fast.c - hand-written digit-by-digit parser  (V2 - V4)
 * Both read one Binance trade row at a time and hand back the four fields
 * the engine needs through pointer parameters.
 *
 * Row format (no header):
 *   id,price,qty,quote_qty,time,isBuyerMaker,isBestMatch
 */
#ifndef READ_TRADE_H
#define READ_TRADE_H

#include <stdio.h>

/* Read ONE trade from fp.
 * Return 1 = trade, 0 = EOF, -1 = malformed row / read error.
 * Outputs change only after the complete row has passed validation.
 */
int read_trade(FILE *fp, long long *ts, double *price,
               double *qty, int *is_buy);

#endif /* READ_TRADE_H */
