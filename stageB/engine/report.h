/*
 * report.h - all printing and file output for Stage A
 *
 * Keeping output here means the calculation files never call printf.
 * Column layout must match data/group_data/test20_expected.txt.
 */
#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>

/* ---- trade table (test20 style) ---- */
void print_table_header(void);
void print_trade_row(long long trade_no, double price, double qty, int is_buy,
                     double vwap, int have_short, double sma_short,
                     int have_long, double sma_long, int sig);

/* ---- one-second OHLC bar ---- */
void print_bar(long long sec, double open, double high, double low,
               double close, int n);
void print_bar_header(void);

/* ---- summary at the end of a file ---- */
void print_summary(double vwap, int buys, int sells);

/* ---- signals.dat slice for gnuplot ---- */
FILE *dat_open(const char *path);   /* returns NULL on failure; caller must check */
void  dat_write_row(FILE *dat, long long trade_no, double price,
                    double sma_short, double sma_long, int sig);
void  dat_close(FILE *dat);

#endif /* REPORT_H */
