/*
 * read_trade_fx.h - fast reader that returns FIXED-POINT numbers (Stage B)
 *
 * Same job as read_trade.h (fast version), but price and qty come back as
 * long long whole numbers, already multiplied by PRICE_SCALE and QTY_SCALE
 * (see engine_fx/fixed.h). The digits go straight from the text into a
 * whole number, so a double is never used and nothing is rounded.
 */
#ifndef READ_TRADE_FX_H
#define READ_TRADE_FX_H

#include <stdio.h>

/* Read the next trade from fp.
 *   *ts     = timestamp in microseconds
 *   *price  = price x PRICE_SCALE   (76589.57   -> 7658957)
 *   *qty    = qty   x QTY_SCALE     (0.00007000 -> 7)
 *   *is_buy = 1 if isBuyerMaker is False, otherwise 0
 * Returns 1 if a trade was read, 0 at the end of the file. */
int read_trade_fx(FILE *fp, long long *ts, long long *price, long long *qty, int *is_buy);

/* How many numbers had a non-zero digit beyond the scale (e.g. a price with
 * a 3rd decimal). 0 means every value in the file was stored exactly. */
long long read_trade_fx_digits_lost(void);

#endif /* READ_TRADE_FX_H */
