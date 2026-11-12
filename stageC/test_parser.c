/*
 * test_parser.c - check the fast parser against the sscanf method it replaced
 *
 *   1) hand-written lines, good and malformed, with known answers
 *   2) if a CSV file is given: every line of it is parsed both ways and
 *      the two results must be identical
 *
 * Build:  make test_parser
 * Run:    ./stageC/test_parser data/stress_full.csv
 * Prints PASS and returns 0 if everything matched, otherwise prints FAIL.
 */
#include <stdio.h>
#include "engine/feed.h"
#include "engine/fast_parse.h"

/* one hand-written test: the input and the expected result */
typedef struct {
    const char *line;
    int         accepted;      /* 1 = must parse, 0 = must be rejected */
    long long   price, qty, time;
    int         is_buy;
} Case;

static const Case CASES[] = {
    { "1,76589.57000000,0.00007000,5.36,1787296690782142,False,True",
      1, 7658957, 7, 1787296690782142LL, 1 },
    { "1,76589.57,0.00007,5.36,1787296690782142,True,True",
      1, 7658957, 7, 1787296690782142LL, 0 },
    { "1,76589.5,1,5.36,10,False,True", 1, 7658950, 100000, 10, 1 },
    { "",                                0, 0, 0, 0, 0 },   /* empty line            */
    { "abc",                             0, 0, 0, 0, 0 },   /* no fields             */
    { "1,76589.571,0.1,5,10,False,True", 0, 0, 0, 0, 0 },   /* 3 decimals in a price */
    { "1,99999999999999999999,0.1,5,10,False,True",
                                         0, 0, 0, 0, 0 },   /* too many digits       */
    { "1,1.00,0.1",                      0, 0, 0, 0, 0 },   /* line stops early      */
    { "1,1.00,0.1,5,10",                 0, 0, 0, 0, 0 },   /* no isBuyerMaker       */
    { "1,1.00,0.1,5,10,Maybe,True",      0, 0, 0, 0, 0 },   /* bad isBuyerMaker      */
};

/* run_cases - returns how many hand-written tests failed */
static int run_cases(void) {
    int count = (int)(sizeof CASES / sizeof CASES[0]);
    int failures = 0;
    int i;

    for (i = 0; i < count; i++) {
        const Case *c = &CASES[i];
        TradeFx t = { 0, 0, 0, 0 };
        int accepted = parse_trade_fx(c->line, &t);
        int ok = (accepted == c->accepted);

        if (ok && accepted) {
            ok = t.price == c->price && t.qty == c->qty &&
                 t.time == c->time && t.is_buy == c->is_buy;
        }
        if (!ok) {
            printf("FAIL case %d: <%s>\n", i + 1, c->line);
            failures++;
        }
    }
    printf("hand-written cases: %d run, %d failed\n", count, failures);
    return failures;
}

/* to_fixed - the conversion Stage B's loader uses */
static long long to_fixed(double value, long long scale) {
    return (long long)(value * scale + 0.5);
}

/* parse_with_sscanf - the slow method, used here as the reference answer */
static int parse_with_sscanf(const char *line, TradeFx *out) {
    long long id;
    double price, qty, quote;
    char maker[16], best[16];

    if (sscanf(line, "%lld,%lf,%lf,%lf,%lld,%15[^,],%15s",
               &id, &price, &qty, &quote, &out->time, maker, best) != 7) {
        return 0;
    }
    out->price  = to_fixed(price, PRICE_SCALE);
    out->qty    = to_fixed(qty, QTY_SCALE);
    out->is_buy = (maker[0] == 'F' || maker[0] == 'f') ? 1 : 0;
    return 1;
}

/* compare_file - parse every line both ways; returns the number of mismatches,
 * or -1 if the file cannot be loaded */
static long long compare_file(const char *path) {
    Feed feed;
    long long i, mismatches = 0;

    if (!feed_load(&feed, path)) {
        return -1;
    }
    for (i = 0; i < feed.n; i++) {
        TradeFx fast, slow;
        int fast_ok = parse_trade_fx(feed.line[i], &fast);
        int slow_ok = parse_with_sscanf(feed.line[i], &slow);
        int same = fast_ok && slow_ok && fast.time == slow.time &&
                   fast.price == slow.price && fast.qty == slow.qty &&
                   fast.is_buy == slow.is_buy;

        if (!same) {
            if (mismatches < 5) {
                printf("MISMATCH line %lld: %s\n", i + 1, feed.line[i]);
            }
            mismatches++;
        }
    }
    printf("%s: %lld lines compared, %lld mismatches\n", path, feed.n, mismatches);
    feed_free(&feed);
    return mismatches;
}

int main(int argc, char *argv[]) {
    int failures = run_cases();
    int i;

    for (i = 1; i < argc; i++) {
        if (compare_file(argv[i]) != 0) {
            failures++;
        }
    }
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
