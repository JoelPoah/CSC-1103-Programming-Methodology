/*
 * fast_reader.h - Group 06 fast CSV trade reader
 *
 * Reads one Binance trade row at a time with a hand-written digit parser
 * (no sscanf / strtod), and returns the four fields the engine needs
 * through pointer parameters.
 *
 * Row format (no header):
 *   id,price,qty,quote_qty,time,isBuyerMaker,isBestMatch
 */
#ifndef FAST_READER_H
#define FAST_READER_H

#include <stdio.h>

/* Read ONE trade from fp.
 * Return 1 = trade, 0 = EOF, -1 = malformed row / read error.
 * Outputs change only after the complete row has passed validation.
 */
int read_trade(FILE *fp, long long *ts, double *price,
               double *qty, int *is_buy);

#endif /* FAST_READER_H */
