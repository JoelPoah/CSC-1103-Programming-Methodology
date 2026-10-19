/*
 * report.c - printing and file output (see report.h)
 *
 * Column widths come from the original stageA_stream.c so the output
 * stays identical. Keep them unless you check test20_expected.txt again.
 */
#include <stdio.h>
#include "report.h"
#include "signal.h"

/* the open signals.dat file; only this file touches it */
static FILE *dat = NULL;

/* print_table_header
 * What:    the column titles for the per-trade table
 * Columns: trade, price, qty, is_buy, VWAP, SMA_short, SMA_long, signal
 * Widths:  -6, -16, -12, -6, -16, -16, -16, then the signal text
 */
void print_table_header(void) {
    printf("%-6s %-16s %-12s %-6s %-16s %-16s %-16s %s\n" , "trade", "price", "qty", "is_buy", "VWAP", "SMA_short", "SMA_long", "signal");
}

/* print_trade_row
 * What:    one line of the table for one trade
 * In/Out:  all values come from the caller. Returns nothing.
 * Watch:   when have_short / have_long is 0, print "-" in that column
 *          instead of a number. Last column: "BUY", "SELL" or nothing.
 */
void print_trade_row(long long trade_no, double price, double qty, int is_buy,
                     double vwap, int have_short, double sma_short,
                     int have_long, double sma_long, int sig) {
    printf("%-6lld %-16.8f %-12.8f %-6d %-16.8f ", trade_no, price, qty, is_buy, vwap);
    if (have_short) {
        printf("%-16.8f ", sma_short);
    }
    else{
        printf("%-16s ", "-");
    }
    if (have_long) {
        printf("%-16.8f ", sma_long);
    }else{
        printf("%-16s ", "-");
    }
    if (sig == SIG_BUY) {
        printf("BUY\n");
    } else if (sig == SIG_SELL) {
        printf("SELL\n");
    } else {
        printf("\n");
    }
}

/* print_signal
 * What:    one signal line for summary mode (the first few signals of a big file)
 * Format:  "  signal BUY  at trade N" / "  signal SELL at trade N"
 */
void print_signal(int sig, long long trade_no) {
    if (sig == SIG_BUY) {
        printf("  signal BUY  at trade %lld\n", trade_no);
    } else if (sig == SIG_SELL) {
        printf("  signal SELL at trade %lld\n", trade_no);
    }
}

/* print_bar_header
 * What:    the title and column names printed before the OHLC bars
 */
void print_bar_header(void) {
    printf("\nOne-second OHLC bars (second = time / 1,000,000)\n");
    printf("%-12s %-16s %-16s %-16s %-16s %s\n",
           "second", "open", "high", "low", "close", "trades");
}

/* print_bar
 * What:    one OHLC bar line: second, open, high, low, close, trade count
 * Widths:  -12 for the second, -16 for each price, then the count
 */
void print_bar(long long sec, double open, double high, double low,
               double close, int n) {
    printf("%-12lld %-16.8f %-16.8f %-16.8f %-16.8f %d\n",
           sec, open, high, low, close, n);
}

/* print_summary
 * What:    the final VWAP and the BUY / SELL counts
 * Format:  "\nFinal VWAP: %.8f\n" then "Signals: %d BUY, %d SELL\n"
 */
void print_summary(double vwap, int buys, int sells) {
    printf("\nFinal VWAP: %.8f\n", vwap);
    printf("Signals: %d BUY, %d SELL\n", buys, sells);
}

/* dat_open
 * What:    open the .dat file for writing and write its header line
 * In/Out:  path = file name. Returns 1 if OK, 0 if it could not open.
 * Header:  "# trade price sma_short sma_long signal(+1 BUY, -1 SELL, 0 none)"
 */
int dat_open(const char *path) {
    dat = fopen(path, "w");
    if (dat == NULL) {
        return 0;
    }
    fprintf(dat, "# trade price sma_short sma_long signal(+1 BUY, -1 SELL, 0 none)\n");
    return 1;
}

/* dat_write_row
 * What:    one data row: "trade price sma_short sma_long signal"
 * Format:  %lld %.8f %.8f %.8f %d
 * Watch:   does nothing if no file is open
 */
void dat_write_row(long long trade_no, double price,
                   double sma_short, double sma_long, int sig) {
    if (dat != NULL) {
        fprintf(dat, "%lld %.8f %.8f %.8f %d\n",
                trade_no, price, sma_short, sma_long, sig);
    }
}

/* dat_close
 * What:    close the file if it is open (safe to call when none is open)
 */
void dat_close(void) {
    if (dat != NULL) {
        fclose(dat);
        dat = NULL;
    }
}
