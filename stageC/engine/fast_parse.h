/*
 * fast_parse.h - turn one CSV line into a fixed-point TradeFx (Stage C)
 *
 * The Stage A+ idea (walk the line as a character array and convert the
 * digits yourself, no sscanf), producing integers instead of doubles.
 */
#ifndef FAST_PARSE_H
#define FAST_PARSE_H

#include "../../stageB/engine/trades_fx.h"   /* TradeFx, PRICE_SCALE, QTY_SCALE */

/* Parse "id,price,qty,quoteQty,time,isBuyerMaker,isBestMatch" into *out.
 * price -> cents (x PRICE_SCALE), qty -> x QTY_SCALE, is_buy = 1 when
 * isBuyerMaker is False. Returns 1 on success, 0 if the line is not a trade
 * or has more decimals than the scales can hold exactly. */
int parse_trade_fx(const char *line, TradeFx *out);

#endif /* FAST_PARSE_H */
