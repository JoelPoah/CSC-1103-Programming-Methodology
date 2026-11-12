/*
 * fast_reader.c - Group 06 fast CSV trade reader (see fast_reader.h)
 *
 * Replaces the sscanf-based helper reader with a digit-by-digit parser.
 * The parse_* functions advance a caller-owned cursor (const char **)
 * through the line and report success with 1 / failure with 0.
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "fast_reader.h"

/* Convert digits to an integer, advancing the caller's cursor through *text.
 * Check BEFORE multiplying to avoid signed overflow on malformed input.
 */
static int parse_integer(const char **text, long long *value) {
    const char *p = *text;
    long long n = 0;
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') {
        int digit = *p - '0';
        if (n > (LLONG_MAX - digit) / 10) return 0;
        n = n * 10 + digit;
        p++;
    }
    *text = p;
    *value = n;
    return 1;
}

/* Dataset decimals are nonnegative, have at most eight fractional digits,
 * and use no exponent notation. Accumulate the fractional digits as an
 * integer, then divide once (e.g. 0015 / 10000), rather than calling sscanf.
 */
static int parse_decimal(const char **text, double *value) {
    long long whole;
    unsigned int fraction = 0, scale = 1;
    const char *p = *text;
    if (!parse_integer(&p, &whole)) return 0;
    if (*p == '.') {
        p++;
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') {
            if (scale == 100000000U) return 0;
            fraction = fraction * 10 + (unsigned int)(*p - '0');
            scale *= 10;
            p++;
        }
    }
    *value = (double)whole + (double)fraction / scale;
    *text = p;
    return 1;
}

static int parse_boolean(const char **text, int *value) {
    if (strncmp(*text, "True", 4) == 0) {
        *value = 1;
        *text += 4;
    } else if (strncmp(*text, "False", 5) == 0) {
        *value = 0;
        *text += 5;
    } else {
        return 0;
    }
    return 1;
}

/* Read ONE trade and hand back four values through pointer parameters.
 * Return 1 = trade, 0 = EOF, -1 = malformed row / read error.
 * Blank lines are ignored; the supplied Binance files have no header.
 * All seven fields are validated, even though id/quote/best are unused.
 * Outputs change only after the complete row has passed validation.
 */
int read_trade(FILE *fp, long long *ts, double *price,
               double *qty, int *is_buy) {
    char line[256];
    while (fgets(line, sizeof line, fp) != NULL) {
        const char *p = line;
        long long id, timestamp;
        double parsed_price, parsed_qty, quote;
        int maker, best;

        /* Reject an overlong record instead of parsing its fragments. */
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            int ch = fgetc(fp);
            if (ch != EOF) return -1;
            if (ferror(fp)) return -1;
        }
        if (*p == '\0' || *p == '\n' ||
            (*p == '\r' && (p[1] == '\n' || p[1] == '\0'))) continue;
        if (!parse_integer(&p, &id) || *p++ != ',' ||
            !parse_decimal(&p, &parsed_price) || *p++ != ',' ||
            !parse_decimal(&p, &parsed_qty) || *p++ != ',' ||
            !parse_decimal(&p, &quote) || *p++ != ',' ||
            !parse_integer(&p, &timestamp) || *p++ != ',' ||
            !parse_boolean(&p, &maker) || *p++ != ',' ||
            !parse_boolean(&p, &best)) return -1;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        if (*p != '\0') return -1;

        *ts = timestamp;
        *price = parsed_price;
        *qty = parsed_qty;
        *is_buy = !maker; /* False isBuyerMaker means buyer-initiated trade. */
        return 1;
    }
    if (ferror(fp)) {
        return -1;
    } else {
        return 0;
    }
}
