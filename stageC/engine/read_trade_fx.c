/*
 * read_trade_fx.c - CSV line -> TradeFx without sscanf (Lucas, see read_trade_fx.h)
 *
 * Stage C: the same parser as the group ladder's src/reader/v5-v8_read_trade_fx.c
 * (V6 - V8), written in the style of the Stage A / B engine files and kept here
 * so every Stage C variant is built from stageC/engine/.
 *
 * Adapted from Jaren's Stage A+ parser (stageA_plus/stageA_plus.c): walk the
 * line one character at a time and build each number from its digits.
 * The difference: Stage A+ produces doubles, this produces fixed-point
 * integers directly, so no double is ever created.
 */
#include <string.h>     /* strchr */
#include "read_trade_fx.h"

/* Decimal places matching PRICE_SCALE (100) and QTY_SCALE (100000) in fixed.h.
 * If fixed.h ever changes, change these two numbers as well. */
#define PRICE_DECIMALS 2
#define QTY_DECIMALS   5

/* A long long holds any number of up to 18 decimal digits */
#define MAX_DIGITS 18

/* is_digit
 * What:    1 if c is one of the characters '0' .. '9'
 */
static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* parse_fixed
 * What:    read "76589.57000000" as a whole number scaled by 10^decimals,
 *          e.g. decimals = 2 gives 7658957. Moves *text past the number.
 * Steps:   1) digits before the '.' go straight into n
 *          2) the first `decimals` digits after the '.' also go into n
 *          3) any further digits must be 0, otherwise the value would not
 *             fit the scale exactly and the line is rejected
 *          4) if the text had fewer decimals, multiply up to the full scale
 * Returns: 1 on success, 0 if the text is not a number that fits
 * Why:     n = n * 10 + digit is one multiply and one add per character,
 *          with none of the format-string work that sscanf does
 * Watch:   the digit count is checked BEFORE each multiply, so n can never
 *          grow past 18 digits and overflow a long long
 */
static int parse_fixed(const char **text, int decimals, long long *value) {
    const char *p = *text;
    long long n = 0;
    int digits = 0;

    if (!is_digit(*p)) {
        return 0;
    }

    /* 1) the whole part */
    while (is_digit(*p)) {
        if (digits == MAX_DIGITS) {
            return 0;
        }
        n = n * 10 + (*p - '0');
        digits++;
        p++;
    }

    /* 2) and 3) the decimal part */
    if (*p == '.') {
        p++;
        while (is_digit(*p)) {
            if (decimals > 0) {
                if (digits == MAX_DIGITS) {
                    return 0;
                }
                n = n * 10 + (*p - '0');
                digits++;
                decimals--;
            } else if (*p != '0') {
                return 0;
            }
            p++;
        }
    }

    /* 4) "76589.5" with 2 decimals -> 7658950 */
    while (decimals > 0) {
        if (digits == MAX_DIGITS) {
            return 0;
        }
        n = n * 10;
        digits++;
        decimals--;
    }

    *text = p;
    *value = n;
    return 1;
}

/* skip_field
 * What:    move *text past the next comma (for fields the engine never uses)
 * Returns: 1 if a comma was found, 0 if the line ended first
 */
static int skip_field(const char **text) {
    const char *p = *text;

    while (*p != ',') {
        if (*p == '\0') {
            return 0;
        }
        p++;
    }
    *text = p + 1;
    return 1;
}

/* skip_comma
 * What:    the next character must be a comma; move *text past it
 * Returns: 1 if it was a comma, 0 if not
 */
static int skip_comma(const char **text) {
    if (**text != ',') {
        return 0;
    }
    *text = *text + 1;
    return 1;
}

/* parse_trade_fx
 * What:    one CSV line -> TradeFx
 * Fields:  id, price, qty, quoteQty, time, isBuyerMaker, isBestMatch
 * Steps:   skip id; read price and qty as fixed-point; skip quoteQty;
 *          read time; the first letter of isBuyerMaker gives the side
 * Watch:   *out is only written once the whole line has been accepted
 */
int parse_trade_fx(const char *line, TradeFx *out) {
    const char *p = line;
    long long price, qty, time_us;
    int is_buy;

    if (!skip_field(&p)) {                                  /* id       */
        return 0;
    }
    if (!parse_fixed(&p, PRICE_DECIMALS, &price) || !skip_comma(&p)) {
        return 0;
    }
    if (!parse_fixed(&p, QTY_DECIMALS, &qty) || !skip_comma(&p)) {
        return 0;
    }
    if (!skip_field(&p)) {                                  /* quoteQty */
        return 0;
    }
    if (!parse_fixed(&p, 0, &time_us) || !skip_comma(&p)) {
        return 0;
    }

    /* isBuyerMaker False means a buyer started the trade: a BUY */
    if (*p == 'F' || *p == 'f') {
        is_buy = 1;
    } else if (*p == 'T' || *p == 't') {
        is_buy = 0;
    } else {
        return 0;
    }

    out->time   = time_us;
    out->price  = price;
    out->qty    = qty;
    out->is_buy = is_buy;
    return 1;
}

/* is_blank_line
 * What:    1 if the line has nothing in it ("", "\n", "\r\n" or "\r")
 */
static int is_blank_line(const char *line) {
    if (line[0] == '\0' || line[0] == '\n') {
        return 1;
    }
    if (line[0] == '\r' && (line[1] == '\n' || line[1] == '\0')) {
        return 1;
    }
    return 0;
}

/* read_trade_fx
 * What:    read the next trade from the file into *out
 * Steps:   same loop as read_trade in v2-v4_read_trade_fast.c (Jaren):
 *          1) fgets one line; reject a line too long for the buffer
 *          2) skip blank lines
 *          3) parse_trade_fx does the digit work (Lucas)
 * Returns: 1 = trade, 0 = EOF, -1 = malformed row / read error
 */
int read_trade_fx(FILE *fp, TradeFx *out) {
    char line[256];

    while (fgets(line, sizeof(line), fp) != NULL) {
        /* a line with no '\n' that is not the last line did not fit */
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            if (fgetc(fp) != EOF || ferror(fp)) {
                return -1;
            }
        }
        if (!is_blank_line(line)) {
            if (!parse_trade_fx(line, out)) {
                return -1;
            }
            return 1;
        }
    }
    if (ferror(fp)) {
        return -1;
    }
    return 0;
}
