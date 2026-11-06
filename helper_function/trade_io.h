/*
 * trade_io.h  -  PM Mini Project helper library for Stage A
 * Works on Windows, macOS and Raspberry Pi.
 *
 * Reads the Binance trade file one trade at a time. You do not need pointers
 * or structs to use it: each function returns a plain value.
 *
 *   if (!open_data(TEST20)) return 1;       // or DEV, or STRESS
 *   while (next_trade()) {
 *       double p = trade_price();
 *       ...
 *   }
 *   close_trades();
 *
 * Build:  gcc -O2 your_program.c trade_io.c -o your_program
 */
#ifndef TRADE_IO_H
#define TRADE_IO_H

/* ================= SET THESE TWO LINES ONCE =================
 * DATA_FOLDER : the folder holding stress_full.csv and the Group folders.
 *               "~" means your home folder (e.g. /Users/yourname on a Mac,
 *               C:/Users/yourname on Windows, /home/yourname on the Pi).
 *               Use forward slashes / on every computer.
 * GROUP_FOLDER: your group's folder inside DATA_FOLDER.
 */
#ifndef DATA_FOLDER
#define DATA_FOLDER  "/Users/lawrenceseow/Documents/work/Glasgow/Teaching/JointDegree/2026_2027/Trim_1/CSC1103 /Assignment/pm-data/dataset_BTCUSDT_2026-08-21"
#endif
#ifndef GROUP_FOLDER
#define GROUP_FOLDER "Group01"
#endif

/* ================= The three data files =================
 *   TEST20 : DATA_FOLDER/GROUP_FOLDER/test20.csv    (20 trades, for checking answers)
 *   DEV    : DATA_FOLDER/GROUP_FOLDER/dev_1M.csv    (1 million trades, for development)
 *   STRESS : DATA_FOLDER/stress_full.csv            (full day, for timing)
 */
#define TEST20 1
#define DEV    2
#define STRESS 3

int       open_data(int which);              /* open TEST20, DEV or STRESS; 1 if OK, 0 if not */
int       open_trades(const char *filename); /* open any other file by name or full path      */
int       next_trade(void);                  /* 1 if a trade was read, 0 at end of file       */
void      close_trades(void);                /* call once when finished                       */

long long trade_number(void);                /* 1 for the first trade, 2 for the next...      */
long long trade_id(void);                    /* field 1: exchange trade Id                    */
double    trade_price(void);                 /* field 2: price in USDT                        */
double    trade_qty(void);                   /* field 3: quantity in BTC                      */
double    trade_quote_qty(void);             /* field 4: price x qty, from the file           */
long long trade_time(void);                  /* field 5: timestamp in microseconds            */
int       trade_is_buy(void);                /* 1 = buy, 0 = sell (from field 6)              */

#endif /* TRADE_IO_H */
