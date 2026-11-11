/*
 * report.h - all printing and file output, shared by every version
 *
 * Keeping output here means the calculation files never call printf.
 * Column layout must match data/group_data/test20_expected.txt.
 * The signals.dat file is kept open inside report.c, so callers never
 * handle a FILE pointer (same idea as trade_io).
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

/* ---- timing lines (read by run_report.sh, so keep the wording) ---- */
void print_timing(long long trades, double total_ms);
void print_load_process(double load_ms, double process_ms);

/* ---- signals.dat slice for gnuplot ---- */
int   dat_open(const char *path);   /* 1 if OK, 0 if it could not open */
void  dat_write_row(long long trade_no, double price,
                    double sma_short, double sma_long, int sig);
void  dat_close(void);

#endif /* REPORT_H */
