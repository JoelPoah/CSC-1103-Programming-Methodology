/*
 * report.h - all printing, shared by every version of the ladder
 *
 * Keeping output here means the calculation files never call printf.
 * Column layout must match data/group_data/test20_expected.txt.
 * No plot files: the versions in src/ are timed, and the brief says a timed
 * version must never write signals.dat. The plot is made by stageA/stageA_plot.c.
 */
#ifndef REPORT_H
#define REPORT_H

/* ---- trade table (test20 style) ---- */
void print_table_header(void);
void print_trade_row(long long trade_no, double price, double qty, int is_buy,
                     double vwap, int have_short, double sma_short,
                     int have_long, double sma_long, int sig);

/* ---- one signal line in summary mode: "  signal BUY  at trade N" ---- */
void print_signal(int sig, long long trade_no);

/* ---- one-second OHLC bar ---- */
void print_bar(long long sec, double open, double high, double low,
               double close, int n);
void print_bar_header(void);

/* ---- summary at the end of a file ---- */
void print_summary(double vwap, int buys, int sells);

#endif /* REPORT_H */
