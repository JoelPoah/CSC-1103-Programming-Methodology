/*
 * ohlc_fx.h - one-second OHLC bars with fixed-point prices
 *
 * Same job as engine/ohlc.h, but every price is a long long
 * (price x PRICE_SCALE, see fixed.h). Divide by PRICE_SCALE when printing.
 *
 * This header has two versions of the code. The Makefile links exactly one:
 *   engine_fx/ohlc_fx.c   - C
 *   engine_asm/ohlc_fx.S  - hand-written AArch64 assembly (Raspberry Pi)
 */
#ifndef OHLC_FX_H
#define OHLC_FX_H

#define MICROS_PER_SEC 1000000LL   /* trade time is in microseconds */

void ohlc_fx_init(void);

/* Add one trade. Returns 1 if the previous bar was finished by this trade
 * (read it with the ohlc_fx_finished_* functions), otherwise 0. */
int  ohlc_fx_update(long long time_us, long long price);

/* After the last trade: finish the current bar. Returns 1 if there was one. */
int  ohlc_fx_flush(void);

/* the most recently finished bar */
long long ohlc_fx_finished_sec(void);
long long ohlc_fx_finished_open(void);
long long ohlc_fx_finished_high(void);
long long ohlc_fx_finished_low(void);
long long ohlc_fx_finished_close(void);
int       ohlc_fx_finished_n(void);

#endif /* OHLC_FX_H */
