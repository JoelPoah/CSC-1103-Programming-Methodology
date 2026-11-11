/*
 * read_trade.h - our own trade reader (Stage A+), replaces the helper library
 *
 * One call reads one line of the CSV file and hands back several results
 * through pointer parameters:
 *
 *   FILE *fp = fopen("data/group_data/dev_1M.csv", "r");
 *   long long ts;  double price, qty;  int is_buy;
 *   while (read_trade(fp, &ts, &price, &qty, &is_buy)) {
 *       ...
 *   }
 *   fclose(fp);
 *
 * One header, two versions of the code. The Makefile links exactly one:
 *   read_trade_sscanf.c - Week 8: fgets + sscanf (easy, slow)
 *   read_trade_fast.c   - Week 9: walks the line and converts the digits itself
 */
#ifndef READ_TRADE_H
#define READ_TRADE_H

#include <stdio.h>

#define LINE_MAX_LEN 256   /* a CSV row is about 80 characters */

/* Read the next trade from fp.
 *   *ts     = field 5, timestamp in microseconds
 *   *price  = field 2, price in USDT
 *   *qty    = field 3, quantity in BTC
 *   *is_buy = 1 if isBuyerMaker (field 6) is False, otherwise 0
 * Returns 1 if a trade was read, 0 at the end of the file.
 * Lines that are not trades (a header, a blank line) are skipped. */
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy);

#endif /* READ_TRADE_H */
