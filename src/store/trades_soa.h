/*
 * trades_soa.h - all trades in memory as a Struct-of-Arrays (Leroi, stretch goal)
 *
 * The same fixed-point data as trades_fx.h, but instead of one array of
 * TradeFx structs, each field has its own array. Trade i is
 * time[i], price[i], qty[i], is_buy[i].
 *
 * Why: the processing loop reads time, price and qty (24 bytes) but never
 * is_buy. In V5's array of 32-byte structs, every cache line it loads also
 * carries is_buy and 4 bytes of padding. With one array per field, the loop
 * only loads the bytes it uses.
 *
 * Compared only with V5 (same reader, same fixed-point modules, same work per
 * trade); it is not a rung of the efficiency ladder.
 * Built by soa_trades_fx.c, used by soa_main_stageB_fixed.c ("make soa").
 */
#ifndef TRADES_SOA_H
#define TRADES_SOA_H

/* One pointer per field, all pointing at arrays of the same length n. */
typedef struct {
    long long *time;     /* timestamps in microseconds */
    long long *price;    /* prices x PRICE_SCALE       */
    long long *qty;      /* quantities x QTY_SCALE     */
    int       *is_buy;   /* 1 = buy, 0 = sell          */
    long long  n;        /* number of trades loaded    */
} TradesSoa;

/* Read every trade in the file into the four arrays and point *out at them.
 * Returns 1 on success, 0 on an error (cannot open, bad row, too many trades).
 * Nothing needs freeing: the arrays are global, as in V5. */
int load_trades_soa(const char *path, TradesSoa *out);

#endif /* TRADES_SOA_H */
