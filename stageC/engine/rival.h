/*
 * rival.h - the rival bot: who gets the price when a signal appears (Stage C)
 *
 * Rules from the brief:
 *   - the rival reacts to every signal in R microseconds (standard R = 1 us)
 *   - our reaction time = processing time of the trade that raised the signal
 *   - reaction < R  -> we win and trade at the triggering trade's price
 *     otherwise     -> the rival wins and we trade at the NEXT trade's price
 *   - start with no BTC; BUY 1 BTC if holding none, SELL it if holding one
 *   - P&L = sum of selling prices - sum of buying prices
 *
 * All prices are in cents (long long), so the P&L is exact.
 *
 * Open position at the end: if the last order was a BUY that was never sold,
 * its price is NOT counted, so P&L covers closed round trips only.
 * Otherwise one unmatched buy would subtract ~76,000 USDT and hide the
 * difference we want to measure.
 */
#ifndef RIVAL_H
#define RIVAL_H

typedef struct {
    long long r_ns;         /* rival reaction time, ns                       */
    int       holding;      /* 1 = we hold (or have ordered) 1 BTC           */
    int       pending;      /* SIG_BUY / SIG_SELL waiting for the next trade  */
    long long buy_price;    /* cents, price paid for the BTC we hold          */
    long long pnl;          /* cents, closed round trips only                 */
    int       wins;         /* signals where we beat the rival                */
    int       losses;       /* signals where the rival beat us                */
    int       round_trips;  /* completed buy -> sell pairs                    */
} Rival;

void rival_init(Rival *r, double r_us);

/* Call for EVERY trade, BEFORE rival_on_signal for that trade: if an order is
 * waiting (we lost the race on the previous signal), it is filled at this
 * trade's price. */
void rival_on_trade(Rival *r, long long price_cents);

/* Call when the engine raises a signal on a trade.
 * reaction_ns = our reaction time for that trade, price_cents = its price. */
void rival_on_signal(Rival *r, int sig, long long reaction_ns, long long price_cents);

#endif /* RIVAL_H */
