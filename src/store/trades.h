/*
 * trades.h - all trades held in memory (Stage B)
 *
 * The whole CSV file is read ONCE into a global array of Trade structs.
 * The engine then loops over the array, so the timed "process" part of
 * the program does no file reading at all.
 */
#ifndef TRADES_H
#define TRADES_H

/* stress_full.csv has 8,928,944 trades, so 10 million leaves some room */
#define MAX_TRADES 10000000

/* One trade. Only the fields the engine actually uses are kept.
 * Fields are ordered biggest first (8-byte ones, then the int) so the
 * compiler adds as little padding as possible: sizeof(Trade) = 32 bytes.
 */
typedef struct {
    long long time;     /* timestamp in microseconds  */
    double    price;    /* price in USDT              */
    double    qty;      /* quantity in BTC            */
    int       is_buy;   /* 1 = buy, 0 = sell          */
} Trade;

/* The global array. It is defined once in trades.c; "extern" lets every
 * file that includes this header use the same array. */
extern Trade trades[MAX_TRADES];

/* Read every trade in the file into trades[].
 * Returns how many trades were loaded, or -1 on error. */
long long load_trades(const char *path);

#endif /* TRADES_H */
