/*
 * soa_main_stageB_fixed.c - V5 with a Struct-of-Arrays layout (Leroi, stretch goal)
 *
 * Copied from v5-v8_main_stageB_fixed.c; only how the loop reads each trade
 * differs. V5 keeps one array of TradeFx structs; this version keeps one
 * array per field (store/trades_soa.h), so the loop loads only the fields it
 * uses (time, price, qty) and never is_buy. Same fixed-point reader, same
 * fixed-point modules, same work per trade, so the comparison with V5 tests
 * the memory layout alone. It is a stretch goal, not a rung of the ladder.
 *
 * It is a copy rather than a change to the V5 main because that file also
 * builds V6, V7 and both V8s, whose official timings must not change.
 *
 * Build (from the project root):
 *   make soa
 *   or: gcc -O2 -Wall src/soa_main_stageB_fixed.c src/store/soa_trades_fx.c
 *       src/reader/v5-v8_read_trade_fx.c src/engine_fx/vwap_fx.c src/engine_fx/v5-v7_ohlc_fx.c
 *       src/engine_fx/sma_fx.c src/engine_fx/signal_fx.c src/engine/report.c
 *       -o bin/soa_stageB_fixed
 * Run (from the project root):
 *   ./bin/soa_stageB_fixed        test20, dev_1M and stress_full
 *   ./bin/soa_stageB_fixed dev    test20 and dev_1M only
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>                      /* strcmp */
#include "config.h"                      /* file paths and SMA windows */
#include "engine_fx/fixed.h"
#include "store/trades_soa.h"
#include "engine_fx/vwap_fx.h"
#include "engine_fx/ohlc_fx.h"
#include "engine_fx/sma_fx.h"
#include "engine_fx/signal_fx.h"
#include "engine/report.h"

/* print_bar_fx - print a fixed-point bar using the shared report function */
static void print_bar_fx(const OhlcBarFx *b) {
    print_bar(b->sec, (double)b->open / PRICE_SCALE, (double)b->high / PRICE_SCALE,
              (double)b->low / PRICE_SCALE, (double)b->close / PRICE_SCALE, b->n);
}

/*
 * process - run the engine over trades 0 .. trades->n - 1
 */
static void process(const TradesSoa *trades, int short_win, int long_win,
                    int print_table) {
    /* Copy the array pointers once. The loop calls functions in other .c
     * files, so the compiler cannot be sure trades->price is unchanged after
     * each call and would load it again on every trade. */
    const long long *time  = trades->time;
    const long long *price = trades->price;
    const long long *qty   = trades->qty;
    long long n = trades->n;

    VwapFx vwap;
    OhlcBarFx bar, finished;
    SmaFx short_sma, long_sma;
    CrossoverFx cross;
    int signals_printed = 0;
    long long i;

    vwap_fx_init(&vwap);
    ohlc_fx_init(&bar);
    ohlc_fx_init(&finished);
    sma_fx_init(&short_sma, short_win);
    sma_fx_init(&long_sma, long_win);
    crossover_fx_init(&cross);

    if (print_table) {
        print_table_header();
    }

    for (i = 0; i < n; i++) {
        long long trade_no = i + 1;
        int signal = SIG_NONE;
        int have_short, have_long;

        /* each field comes from its own array */
        vwap_fx_update(&vwap, price[i], qty[i]);

        if (ohlc_fx_update(&bar, time[i], price[i], &finished) && print_table) {
            print_bar_fx(&finished);
        }

        sma_fx_push(&short_sma, price[i]);
        sma_fx_push(&long_sma, price[i]);
        have_short = sma_fx_ready(&short_sma);
        have_long  = sma_fx_ready(&long_sma);

        /* compare the exact sums - no division, no rounding */
        if (have_short && have_long) {
            signal = crossover_fx_update(&cross, sma_fx_sum(&short_sma), short_win,
                                         sma_fx_sum(&long_sma), long_win);
        }

        /* is_buy is only read here, for the test20 table, so the timed
         * stress_full loop never loads that array */
        if (print_table) {
            print_trade_row(trade_no, (double)price[i] / PRICE_SCALE,
                            (double)qty[i] / QTY_SCALE, trades->is_buy[i],
                            vwap_fx_value(&vwap), have_short, sma_fx_value(&short_sma),
                            have_long, sma_fx_value(&long_sma), signal);
        } else if (signal != SIG_NONE && signals_printed < 10) {
            print_signal(signal, trade_no);
            signals_printed++;
        }
    }

    if (print_table && bar.active) {
        print_bar_header();
        print_bar_fx(&bar);
    }

    print_summary(vwap_fx_value(&vwap), cross.buys, cross.sells);
}

/*
 * run - load one file, then process it, timing both phases
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t t0, t1, t2;
    TradesSoa trades;
    int ok;

    if (short_win >= long_win || long_win > MAX_WIN) {
        printf("bad windows: need short < long <= %d\n", MAX_WIN);
        return 1;
    }

    t0 = now_ns();
    ok = load_trades_soa(path, &trades);
    t1 = now_ns();
    if (!ok) {
        return 1;
    }

    process(&trades, short_win, long_win, print_table);
    t2 = now_ns();
    /* nothing to free: the arrays are global, as in V5 */

    printf("Trades loaded: %lld\n", trades.n);
    printf("Load time:    %.3f ms\n", (t1 - t0) / 1e6);
    printf("Process time: %.3f ms\n", (t2 - t1) / 1e6);
    return 0;
}

int main(int argc, char *argv[]) {
    /* "dev" as the first argument skips stress_full (valgrind runs) */
    int skip_stress = (argc > 1 && strcmp(argv[1], "dev") == 0);

    printf("Prices: fixed-point (price x %lld, qty x %lld)\n\n", PRICE_SCALE, QTY_SCALE);

    printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
    if (run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1) != 0) return 1;

    printf("\n=== dev_1M.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
    if (run(DEV_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;

    if (!skip_stress) {
        printf("\n=== stress_full.csv (short=%d, long=%d) ===\n", DEV_SHORT, DEV_LONG);
        if (run(STRESS_PATH, DEV_SHORT, DEV_LONG, 0) != 0) return 1;
    }

    return 0;
}
