/*
 * read_trade_fx.c - fast reader for fixed-point numbers (see read_trade_fx.h)
 */
#include <stdio.h>
#include "read_trade.h"        /* LINE_MAX_LEN */
#include "read_trade_fx.h"
#include "../engine_fx/fixed.h"

static long long digits_lost = 0;   /* numbers that did not fit the scale exactly */

/* is_digit - 1 if c is one of '0' .. '9' */
static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* skip_field - return the position just after the next comma */
static int skip_field(const char line[], int pos) {
    while (line[pos] != ',' && line[pos] != '\0') {
        pos++;
    }
    if (line[pos] == ',') {
        pos++;
    }
    return pos;
}

/* parse_whole - convert a whole number: value = value * 10 + digit */
static int parse_whole(const char line[], int pos, long long *value) {
    long long number = 0;
    while (is_digit(line[pos])) {
        number = number * 10 + (line[pos] - '0');
        pos++;
    }
    *value = number;
    return pos;
}

/* parse_fixed
 * What:    convert a decimal into a whole number that keeps `decimals`
 *          digits after the point
 *          "76589.57000000" with decimals = 2 -> 7658957
 *          "0.00007000"     with decimals = 5 -> 7
 * In/Out:  line/pos = the text and where we are, *value = the result.
 *          Returns the new position (after the last digit).
 * Steps:   1) the digits before the '.' build the number as usual
 *          2) the first `decimals` digits after the '.' carry on building it
 *          3) if the text had fewer decimals, multiply by 10 for each one
 *             missing ("76589.5" -> 7658950)
 *          4) any digits after that are beyond our scale. They are all '0'
 *             in our data; a non-zero one is counted in digits_lost.
 */
static int parse_fixed(const char line[], int pos, int decimals, long long *value) {
    long long number = 0;
    int kept = 0;          /* decimals used so far */
    int lost = 0;          /* 1 if a non-zero digit had to be dropped */

    while (is_digit(line[pos])) {
        number = number * 10 + (line[pos] - '0');
        pos++;
    }
    if (line[pos] == '.') {
        pos++;
        while (is_digit(line[pos])) {
            if (kept < decimals) {
                number = number * 10 + (line[pos] - '0');
                kept++;
            } else if (line[pos] != '0') {
                lost = 1;
            }
            pos++;
        }
    }
    while (kept < decimals) {
        number = number * 10;
        kept++;
    }

    if (lost) {
        digits_lost++;
    }
    *value = number;
    return pos;
}

/* read_trade_fx
 * What:    read one line and convert the fields we need by hand
 * Steps:   same walk along the line as read_trade_fast.c; only the two
 *          decimal fields use parse_fixed instead of parse_decimal.
 */
int read_trade_fx(FILE *fp, long long *ts, long long *price, long long *qty, int *is_buy) {
    char line[LINE_MAX_LEN];
    int pos;

    while (fgets(line, sizeof line, fp) != NULL) {
        if (!is_digit(line[0])) {
            continue;                                             /* header / blank line */
        }
        pos = skip_field(line, 0);                                /* 1: trade id */
        pos = parse_fixed(line, pos, PRICE_DECIMALS, price);      /* 2: price    */
        pos = skip_field(line, pos);
        pos = parse_fixed(line, pos, QTY_DECIMALS, qty);          /* 3: qty      */
        pos = skip_field(line, pos);
        pos = skip_field(line, pos);                              /* 4: quoteQty */
        pos = parse_whole(line, pos, ts);                         /* 5: time     */
        pos = skip_field(line, pos);

        /* 6: "False" means a buyer started the trade: a BUY */
        if (line[pos] == 'F' || line[pos] == 'f') {
            *is_buy = 1;
        } else {
            *is_buy = 0;
        }
        return 1;
    }
    return 0;   /* end of file */
}

long long read_trade_fx_digits_lost(void) {
    return digits_lost;
}
