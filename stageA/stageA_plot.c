/*
 * stageA_plot.c - make signals.dat and signals.png for the Stage A report
 *
 * Kept apart from the timed program (stageA_manual) because the brief says
 * never to write plot files in a version you time.
 * It is never timed, so it may write a file and call gnuplot.
 *
 * What it does:
 *   1) reads dev_1M.csv with the helper library, using the Version 0 SMA
 *      (engine/sma.c, short=35, long=60), so the plot shows Version 0
 *   2) writes trades SLICE_START .. SLICE_END to stageA/signals.dat, one row
 *      per trade once the long SMA exists:
 *        trade  price  sma_short  sma_long  signal (+1 BUY, -1 SELL, 0 none)
 *      and stops reading after SLICE_END (the rest of the file is not needed)
 *   3) checks the slice has at least one BUY and one SELL (the brief asks for it)
 *   4) runs gnuplot on stageA/plot_signals.gnuplot to draw stageA/signals.png
 *
 * Only SMA + signal are needed for the plot, so VWAP and OHLC are skipped.
 *
 * Build (from the project root):
 *   make stageA
 *   or: gcc -O2 -Wall stageA/stageA_plot.c stageA/engine/sma.c stageA/engine/signal.c
 *           stageA/engine/report.c helper_function/trade_io.c -o stageA/stageA_plot
 * Run (from the project root):
 *   ./stageA/stageA_plot
 */
#include <stdio.h>
#include <stdlib.h>                      /* system */
#include "../helper_function/trade_io.h"
#include "engine/sma.h"
#include "engine/signal.h"
#include "engine/report.h"

/* ---- file paths (relative to the project root) ---- */
#define DEV_PATH    "data/group_data/dev_1M.csv"
#define SIGNALS_DAT "stageA/signals.dat"
#define PLOT_SCRIPT "stageA/plot_signals.gnuplot"

/* ---- SMA windows (Group 06) ---- */
#define DEV_SHORT 35
#define DEV_LONG  60

/* ---- slice written to signals.dat (trade numbers, inclusive) ---- */
#define SLICE_START 1
#define SLICE_LEN   2000
#define SLICE_END   (SLICE_START + SLICE_LEN - 1)

int main(void) {
    int buys = 0, sells = 0, rows = 0;

    if (!open_trades(DEV_PATH)) {
        printf("cannot open %s\n", DEV_PATH);
        return 1;
    }
    if (!dat_open(SIGNALS_DAT)) {
        printf("cannot open %s for writing\n", SIGNALS_DAT);
        close_trades();
        return 1;
    }

    sma_init(SHORT_SMA, DEV_SHORT);
    sma_init(LONG_SMA, DEV_LONG);
    crossover_init();

    while (next_trade()) {
        long long t = trade_number();
        double price = trade_price();
        int signal = SIG_NONE;

        if (t > SLICE_END) {
            break;                       /* past the slice: nothing more to write */
        }

        sma_push(SHORT_SMA, price);
        sma_push(LONG_SMA, price);
        if (!sma_ready(SHORT_SMA) || !sma_ready(LONG_SMA)) {
            continue;                    /* no row until both SMAs exist */
        }

        double s = sma_value(SHORT_SMA);
        double l = sma_value(LONG_SMA);
        signal = crossover_update(s, l);

        if (t >= SLICE_START) {
            dat_write_row(t, price, s, l, signal);
            rows++;
            if (signal == SIG_BUY)  buys++;
            if (signal == SIG_SELL) sells++;
        }
    }

    dat_close();
    close_trades();

    printf("Wrote %d rows (trades %d-%d) to %s: %d BUY, %d SELL\n",
           rows, SLICE_START, SLICE_END, SIGNALS_DAT, buys, sells);
    if (buys == 0 || sells == 0) {
        printf("warning: the slice needs at least one BUY and one SELL - move SLICE_START\n");
    }

    printf("Running gnuplot on %s ...\n", PLOT_SCRIPT);
    if (system("gnuplot " PLOT_SCRIPT) != 0) {
        printf("gnuplot failed or is not installed; %s is still written.\n", SIGNALS_DAT);
        return 1;
    }
    printf("Wrote stageA/signals.png\n");
    return 0;
}
