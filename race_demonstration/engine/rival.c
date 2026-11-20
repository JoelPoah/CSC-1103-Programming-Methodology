/*
 * rival.c - rival bot P&L simulation (see rival.h)
 */
#include "rival.h"
#include "signal.h"                         /* SIG_BUY, SIG_SELL, SIG_NONE */

/* rival_init
 * What:    no position, no P&L; R converted from us to ns (rounded)
 */
void rival_init(Rival *r, double r_us) {
    r->r_ns = (long long)(r_us * 1000.0 + 0.5);
    r->holding = 0;
    r->pending = SIG_NONE;
    r->buy_price = 0;
    r->pnl = 0;
    r->wins = 0;
    r->losses = 0;
    r->round_trips = 0;
}

/* fill
 * What:    an order is executed at price_cents
 *          BUY  -> remember what we paid
 *          SELL -> add (sell - buy) to the P&L, one round trip done
 */
static void fill(Rival *r, int side, long long price_cents) {
    if (side == SIG_BUY) {
        r->buy_price = price_cents;
    } else {
        r->pnl += price_cents - r->buy_price;
        r->round_trips++;
    }
}

/* rival_on_trade
 * What:    fill an order we lost the race on, at this (the next) trade's price
 */
void rival_on_trade(Rival *r, long long price_cents) {
    if (r->pending != SIG_NONE) {
        fill(r, r->pending, price_cents);
        r->pending = SIG_NONE;
    }
}

/* rival_on_signal
 * What:    decide whether the signal makes us trade, and at which price
 * Steps:   1) BUY only if we hold nothing, SELL only if we hold 1 BTC;
 *             any other signal is ignored (no order)
 *          2) reaction < R  -> we win, fill now at the triggering price
 *             reaction >= R -> rival wins, fill at the next trade's price
 */
void rival_on_signal(Rival *r, int sig, long long reaction_ns, long long price_cents) {
    if (sig == SIG_BUY && !r->holding) {
        r->holding = 1;
    } else if (sig == SIG_SELL && r->holding) {
        r->holding = 0;
    } else {
        return;                       /* nothing to do for this signal */
    }

    if (reaction_ns < r->r_ns) {
        r->wins++;
        fill(r, sig, price_cents);
    } else {
        r->losses++;
        r->pending = sig;             /* filled by the next rival_on_trade */
    }
}
