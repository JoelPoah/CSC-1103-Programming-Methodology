/*
 * fixed.h - scale factors for fixed-point numbers (Stage B, V5 onwards)
 *
 * A fixed-point number is a decimal stored as a whole number:
 *     stored value = decimal value x scale
 * and we only divide by the scale again when printing.
 *
 * Why these scales (checked against stress_full.csv):
 *   - prices have at most 2 decimals  -> x 100     stores every price exactly
 *   - quantities have at most 5 decimals -> x 100000 stores every qty exactly
 *
 * Overflow check (long long holds up to about 9.22 x 10^18):
 *   - largest price  79,500.00 -> 7,950,000
 *   - largest qty    18.29992  -> 1,829,992
 *   - one price x qty      <= 7.95e6 x 1.83e6       = 1.5 x 10^13   OK
 *   - VWAP sum of price x qty over the whole day
 *       = total USDT traded (3.40e9) x 100 x 100000  = 3.4 x 10^16   OK (~270x headroom)
 *   - SMA window sum (60 prices) <= 60 x 7.95e6      = 4.8 x 10^8    OK
 *   - crossover test sum_short x 60 <= 4.8e8 x 60    = 2.9 x 10^10   OK
 * Using x 10^8 for qty (all 8 decimals in the file) would make the day's
 * VWAP sum about 3.4 x 10^19, which DOES overflow - so x 100000 is the
 * largest scale that is both exact and safe.
 */
#ifndef FIXED_H
#define FIXED_H

#define PRICE_SCALE 100LL      /* price in cents (2 decimals)       */
#define QTY_SCALE   100000LL   /* qty in units of 0.00001 BTC       */

#endif /* FIXED_H */
