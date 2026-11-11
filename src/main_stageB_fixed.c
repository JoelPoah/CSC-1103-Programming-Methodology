/*
 * main_stageB_fixed.c - in-memory engine with FIXED-POINT prices
 *
 * Same two phases as main_stageB_inmem.c (load the file into an array,
 * then process the array), but every price and quantity is a long long
 * whole number instead of a double:
 *   price -> cents (x 100), qty -> x 100000   (see engine_fx/fixed.h)
 * Doubles are only used to PRINT results; all the maths in the loop is
 * whole-number maths, so the answers are exact.
 *
 * Built by the Makefile as:
 *   bin/v5_stageB_fixed_point   this file + store/trades_fx_array.c  + engine_fx/ (C)
 *   bin/v6_stageC_malloc        this file + store/trades_fx_malloc.c + engine_fx/ (C)
 *   bin/v7_asm_engine           this file + store/trades_fx_malloc.c + engine_asm/ (assembly)
 * The three programs share every line of this file. Only the files linked
 * with it change, which is what makes the comparisons fair.
 *
 * Run (from the project root):
 *   ./bin/v6_stageC_malloc test20 | dev | stress
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "reader/read_trade_fx.h"
#include "store/trades_fx.h"
#include "engine_fx/fixed.h"
#include "engine_fx/vwap_fx.h"
#include "engine_fx/ohlc_fx.h"
#include "engine_fx/sma_fx.h"
#include "engine_fx/signal_fx.h"
#include "engine/report.h"

/* to_usdt - turn a scaled price back into USDT, only for printing */
static double to_usdt(long long scaled_price) {
    return (double)scaled_price / PRICE_SCALE;
}

/* print_finished_bar - print the bar that ohlc_fx just finished */
static void print_finished_bar(void) {
    print_bar(ohlc_fx_finished_sec(), to_usdt(ohlc_fx_finished_open()),
              to_usdt(ohlc_fx_finished_high()), to_usdt(ohlc_fx_finished_low()),
              to_usdt(ohlc_fx_finished_close()), ohlc_fx_finished_n());
}

/*
 * process - run the engine over trades[0] .. trades[n-1]
 *
 * trades is a pointer to the first trade. trades[i] works on a pointer
 * exactly as it does on an array, so the loop looks the same either way.
 */
static void process(const TradeFx *trades, long long n, int short_win, int long_win,
                    int print_table) {
    int signal_printed = 0;
    long long i;

    /* STEP 1 - reset every calculation */
    vwap_fx_init();
    ohlc_fx_init();
    sma_fx_init(SHORT_SMA_FX, short_win);
    sma_fx_init(LONG_SMA_FX, long_win);
    crossover_fx_init();

    /* STEP 2 - print the table header */
    if (print_table == 1){
        print_table_header();
    }

    /* STEP 3 - the main loop, one array element at a time */
    for (i = 0; i < n; i++){
        long long trade_no = i + 1;          /* trade numbers start at 1 */
        long long price = trades[i].price;
        int signal = SIG_NONE;

        /* 3a) VWAP */
        vwap_fx_update(price, trades[i].qty);

        /* 3b) OHLC: print the bar this trade just finished */
        if (ohlc_fx_update(trades[i].time, price) && print_table == 1){
            print_finished_bar();
        }

        /* 3c) SMA: push the price into both windows */
        sma_fx_push(SHORT_SMA_FX, price);
        sma_fx_push(LONG_SMA_FX, price);

        int have_sma_short = sma_fx_ready(SHORT_SMA_FX);
        int have_sma_long = sma_fx_ready(LONG_SMA_FX);

        /* 3d) crossover signal from the exact sums: no division, no rounding */
        if (have_sma_short && have_sma_long){
            signal = crossover_fx_update(sma_fx_sum(SHORT_SMA_FX), short_win,
                                         sma_fx_sum(LONG_SMA_FX), long_win);
        }

        /* 3e) print the row, or only the first 10 signals in summary mode.
         * Doubles appear only here, when printing. */
        if (print_table == 1){
                print_trade_row(trade_no, to_usdt(price), (double)trades[i].qty / QTY_SCALE,
                trades[i].is_buy, vwap_fx_value(),
                have_sma_short, sma_fx_value(SHORT_SMA_FX),
                have_sma_long, sma_fx_value(LONG_SMA_FX), signal);
        }
        else if (signal != SIG_NONE && signal_printed < 10){
                print_signal(signal, trade_no);
                signal_printed++;
        }
    }
}

/*
 * run - load one file, then process it, timing both phases
 * Returns 0 on success, 1 on error.
 */
static int run(const char *path, int short_win, int long_win, int print_table) {
    uint64_t start_ns, loaded_ns, end_ns;
    long long n;
    TradeFx *trades;

    if (short_win >= long_win || long_win > MAX_WIN_FX){
        printf("bad windows: need short < long <= %d\n", MAX_WIN_FX);
        return 1;
    }

    /* PHASE 1 - load */
    start_ns = now_ns();
    trades = load_trades_fx(path, &n);
    loaded_ns = now_ns();
    if (trades == NULL){
        return 1;
    }
    if (read_trade_fx_digits_lost() > 0){
        printf("WARNING: %lld number(s) had more decimals than fixed.h keeps\n",
               read_trade_fx_digits_lost());
    }

    /* PHASE 2 - process */
    process(trades, n, short_win, long_win, print_table);
    end_ns = now_ns();

    /* summary, last OHLC bar, then the timings and the memory used */
    print_summary(vwap_fx_value(), crossover_fx_buys(), crossover_fx_sells());
    if (ohlc_fx_flush() && print_table == 1) {
        print_bar_header();
        print_finished_bar();
    }
    print_timing(n, (end_ns - start_ns) / 1e6);
    print_load_process((loaded_ns - start_ns) / 1e6, (end_ns - loaded_ns) / 1e6);
    printf("Trade array: %lld bytes\n", trades_fx_bytes(n));

    free_trades_fx(trades);
    return 0;
}

/* print_usage - remind the user which modes exist */
static void print_usage(const char *program) {
    printf("usage: %s test20 | dev | stress\n", program);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "test20") == 0) {
        printf("=== test20.csv (short=%d, long=%d) ===\n", TEST_SHORT, TEST_LONG);
        return run(TEST20_PATH, TEST_SHORT, TEST_LONG, 1);
    }

    if (strcmp(argv[1], "dev") == 0) {
        printf("=== dev_1M.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(DEV_PATH, GROUP_SHORT, GROUP_LONG, 0);
    }

    if (strcmp(argv[1], "stress") == 0) {
        printf("=== stress_full.csv (short=%d, long=%d) ===\n", GROUP_SHORT, GROUP_LONG);
        return run(STRESS_PATH, GROUP_SHORT, GROUP_LONG, 0);
    }

    print_usage(argv[0]);
    return 1;
}
