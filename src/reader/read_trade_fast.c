/*
 * read_trade_fast.c - own reader, Week 9 fast parser (see read_trade.h)
 *
 * sscanf is slow because it must read the format string, handle every
 * possible number style and round carefully, on every single line. Our file
 * is always the same shape, so we walk the line as a character array and
 * turn the digit characters into numbers ourselves.
 *
 *   line:  5200000001,76589.57000000,0.00007000,5.36126990,1787296690000123,False,True
 *   pos:   ^ moves from left to right, one character at a time
 */
#include <stdio.h>
#include "read_trade.h"

/* POWER_OF_TEN[d] = 10^d. Each of these is stored exactly in a double. */
static const double POWER_OF_TEN[16] = {
    1e0, 1e1, 1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
    1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15
};

/* is_digit - 1 if c is one of '0' .. '9' */
static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* skip_field
 * What:    move past the current field
 * In/Out:  line = the text, pos = where we are. Returns the position just
 *          after the next comma (or the end of the line).
 */
static int skip_field(const char line[], int pos) {
    while (line[pos] != ',' && line[pos] != '\0') {
        pos++;
    }
    if (line[pos] == ',') {
        pos++;
    }
    return pos;
}

/* parse_whole
 * What:    convert a whole number such as "1787296690000123"
 * In/Out:  line/pos as above, *value = the number. Returns the new position
 *          (on the character after the last digit).
 * Steps:   for each digit: value = value * 10 + digit
 *          ('7' - '0' is 7, because the digit characters are in order)
 */
static int parse_whole(const char line[], int pos, long long *value) {
    long long number = 0;
    while (is_digit(line[pos])) {
        number = number * 10 + (line[pos] - '0');
        pos++;
    }
    *value = number;
    return pos;
}

/* parse_decimal
 * What:    convert a decimal such as "76589.57000000" into a double
 * In/Out:  line/pos as above, *value = the number. Returns the new position.
 * Steps:   1) read ALL the digits as one whole number, ignoring the '.'
 *             76589.57000000 -> 7658957000000, with 8 digits after the '.'
 *          2) divide once by 10^8
 * Why:     the whole number and 10^8 are both exact in a double, so the one
 *          division gives the closest possible double - the same value
 *          sscanf gives, which keeps every result identical to Version 0.
 * Watch:   only exact up to 15 digits in total (prices and quantities in
 *          the file have at most 14), so extra digits are dropped.
 */
static int parse_decimal(const char line[], int pos, double *value) {
    long long digits = 0;      /* all the digits as one whole number */
    int total_digits = 0;      /* how many digits are in it          */
    int after_point = 0;       /* how many of them came after '.'    */
    int seen_point = 0;

    while (is_digit(line[pos]) || (line[pos] == '.' && !seen_point)) {
        if (line[pos] == '.') {
            seen_point = 1;
        } else if (total_digits < 15) {
            digits = digits * 10 + (line[pos] - '0');
            total_digits++;
            if (seen_point) {
                after_point++;
            }
        } else if (!seen_point) {
            /* a 16th digit BEFORE the point cannot be dropped safely.
             * It never happens in our data, so just keep the scale right. */
            after_point--;
        }
        pos++;
    }

    if (after_point >= 0) {
        *value = (double)digits / POWER_OF_TEN[after_point];
    } else {
        *value = (double)digits * POWER_OF_TEN[-after_point];
    }
    return pos;
}

/* read_trade
 * What:    read one line and convert the fields we need by hand
 * Steps:   field 1 id       -> skip
 *          field 2 price    -> parse_decimal
 *          field 3 qty      -> parse_decimal
 *          field 4 quoteQty -> skip
 *          field 5 time     -> parse_whole
 *          field 6 isBuyerMaker -> look at its first letter only
 * Watch:   a line that does not start with a digit is not a trade
 *          (a header or a blank line), so it is skipped.
 */
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy) {
    char line[LINE_MAX_LEN];
    int pos;

    while (fgets(line, sizeof line, fp) != NULL) {
        if (!is_digit(line[0])) {
            continue;
        }
        pos = skip_field(line, 0);                /* 1: trade id      */
        pos = parse_decimal(line, pos, price);    /* 2: price         */
        pos = skip_field(line, pos);
        pos = parse_decimal(line, pos, qty);      /* 3: qty           */
        pos = skip_field(line, pos);
        pos = skip_field(line, pos);              /* 4: quoteQty      */
        pos = parse_whole(line, pos, ts);         /* 5: time          */
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
