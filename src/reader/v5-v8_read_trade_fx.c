/*
 * v5-v8_read_trade_fx.c - CSV line -> TradeFx without sscanf (Lucas, see read_trade_fx.h)
 *
 * Adapted from Jaren's Stage A+ parser (stageA_plus/stageA_plus.c): walk the
 * line one character at a time and build each number from its digits.
 * The difference: Stage A+ produces doubles, this produces fixed-point
 * integers directly, so no double is ever created.
 */
#include <string.h>
#include "read_trade_fx.h"

/* Decimal places matching PRICE_SCALE and QTY_SCALE in fixed.h */
#define PRICE_DECIMALS 2
#define QTY_DECIMALS   5
#if PRICE_SCALE != 100LL || QTY_SCALE != 100000LL
#error "fixed.h scales changed: update PRICE_DECIMALS and QTY_DECIMALS"
#endif

/* A long long holds any number of up to 18 decimal digits */
#define MAX_DIGITS 18

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
 */
static int parse_fixed(const char **text, int decimals, long long *value) {
    const char *p = *text;
    unsigned long long n = 0;      /* unsigned: an overflow wraps safely */
    int digits = 0;

    if (*p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        n = n * 10 + (*p - '0');
        digits++;
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            if (decimals > 0) {
                n = n * 10 + (*p - '0');
                digits++;
                decimals--;
            } else if (*p != '0') {
                return 0;
            }
            p++;
        }
    }
    while (decimals > 0) {
        n *= 10;
        digits++;
        decimals--;
    }
    /* Checked last to keep the loops simple. A longer number could have
     * wrapped n, but it is rejected here before n is ever used. */
    if (digits > MAX_DIGITS) {
        return 0;
    }
    *text = p;
    *value = (long long)n;
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

    if (!skip_field(&p) ||
        !parse_fixed(&p, PRICE_DECIMALS, &price) || *p++ != ',' ||
        !parse_fixed(&p, QTY_DECIMALS, &qty) || *p++ != ',' ||
        !skip_field(&p) ||
        !parse_fixed(&p, 0, &time_us) || *p++ != ',') {
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
    while (fgets(line, sizeof line, fp) != NULL) {
        /* Reject an overlong record instead of parsing its fragments. */
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            int ch = fgetc(fp);
            if (ch != EOF) return -1;
            if (ferror(fp)) return -1;
        }
        if (line[0] == '\0' || line[0] == '\n' ||
            (line[0] == '\r' && (line[1] == '\n' || line[1] == '\0'))) continue;
        if (!parse_trade_fx(line, out)) return -1;
        return 1;
    }
    if (ferror(fp)) {
        return -1;
    } else {
        return 0;
    }
}
