/*
 * read_trade_fx.h - read trades straight into fixed-point TradeFx (Lucas, V5+)
 *
 * Stage C: the same code as the group ladder's src/reader/read_trade_fx.h (V7),
 * kept here so Stage C's engine and its assembly live in stageC/engine/.
 *
 * The Stage A+ idea (walk the line as a character array and convert the
 * digits yourself, no sscanf), producing integers instead of doubles.
 */
#ifndef READ_TRADE_FX_H
#define READ_TRADE_FX_H

#include <stdio.h>
#include "trades_fx.h"            /* TradeFx, PRICE_SCALE, QTY_SCALE */

/* Parse "id,price,qty,quoteQty,time,isBuyerMaker,isBestMatch" into *out.
 * price -> cents (x PRICE_SCALE), qty -> x QTY_SCALE, is_buy = 1 when
 * isBuyerMaker is False. Returns 1 on success, 0 if the line is not a trade
 * or has more decimals than the scales can hold exactly. */
int parse_trade_fx(const char *line, TradeFx *out);

/* Read ONE trade from fp into *out (same return values as read_trade).
 * Return 1 = trade, 0 = EOF, -1 = malformed row / read error.
 * Blank lines are ignored. *out changes only when 1 is returned. */
int read_trade_fx(FILE *fp, TradeFx *out);

#endif /* READ_TRADE_FX_H */
