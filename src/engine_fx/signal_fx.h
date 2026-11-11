/*
 * signal_fx.h - BUY / SELL crossover signals from the exact window SUMS
 *
 * The signal only needs to know which average is bigger:
 *
 *     sum_short / short_win   compared with   sum_long / long_win
 *
 * Multiplying both sides by short_win x long_win (both positive, so the
 * comparison does not flip) gives a test with no division at all:
 *
 *     diff = sum_short x long_win - sum_long x short_win
 *
 * diff > 0 means the short SMA is above the long SMA, diff < 0 means below,
 * and diff == 0 means they are EXACTLY equal - something a double cannot
 * promise. BUY and SELL follow the same rule as engine/signal.h.
 *
 * This header has two versions of the code. The Makefile links exactly one:
 *   engine_fx/signal_fx.c   - C
 *   engine_asm/signal_fx.S  - hand-written AArch64 assembly (Raspberry Pi)
 */
#ifndef SIGNAL_FX_H
#define SIGNAL_FX_H

#include "../engine/signal.h"   /* SIG_BUY, SIG_SELL, SIG_NONE */

void crossover_fx_init(void);

/* Returns SIG_BUY, SIG_SELL or SIG_NONE for this trade. */
int  crossover_fx_update(long long sum_short, int short_win,
                         long long sum_long, int long_win);

int  crossover_fx_buys(void);    /* total BUY signals so far  */
int  crossover_fx_sells(void);   /* total SELL signals so far */

#endif /* SIGNAL_FX_H */
