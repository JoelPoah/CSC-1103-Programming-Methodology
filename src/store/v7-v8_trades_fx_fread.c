/*
 * v7-v8_trades_fx_fread.c - fixed-point trades, the whole file read with ONE fread (Lucas, V7, V8)
 *
 * Lucas's Stage C loader (stageC/engine/feed.c) behind the same
 * load_trades_fx() interface as V5 and V6, so the main program, the parser
 * and the engine do not change. Only HOW the file is read changes:
 *   V6: fgets once per line, and the file is read twice (count, then fill)
 *   V7: one fread copies the whole file into a malloc'd text buffer, then
 *       the lines are found and parsed in memory (parse_trade_fx, Lucas)
 * The text buffer is freed as soon as every line is parsed, so after loading
 * V7 holds the same n x 32-byte array as V6. While loading it also holds the
 * raw text (about 678 MB for stress_full.csv): that is the price of the
 * faster load, and worth showing next to V6 in the memory comparison.
 */
#include <stdio.h>
#include <stdlib.h>     /* malloc, free */
#include "trades_fx.h"
#include "../reader/read_trade_fx.h"

/* read_whole_file
 * What:    copy the whole file into one malloc'd text buffer
 * In/Out:  path = the CSV file, *size_out = number of bytes read.
 *          Returns the buffer (the caller frees it), or NULL on error.
 * Steps:   1) fseek to the end and ftell to get the file size
 *          2) malloc size + 1 bytes (+1 for the final '\0')
 *          3) one fread for the whole file, then end the text with '\0'
 * Watch:   "rb" (binary) so Windows does not turn "\r\n" into "\n" while
 *          reading, and the byte count from ftell matches what fread returns.
 *          ftell returns a long: fine for stress_full.csv (~678 MB < 2 GB).
 */
static char *read_whole_file(const char *path, size_t *size_out) {
    FILE *fp;
    long size;
    size_t got;
    char *text;

    *size_out = 0;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("cannot open %s\n", path);
        return NULL;
    }

    /* 1) the file size: jump to the end, ask where we are, jump back */
    if (fseek(fp, 0, SEEK_END) != 0) {
        printf("cannot find the size of %s\n", path);
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        printf("cannot find the size of %s\n", path);
        fclose(fp);
        return NULL;
    }
    if (size == 0) {
        printf("%s is empty\n", path);
        fclose(fp);
        return NULL;
    }

    /* 2) room for every byte plus the '\0' */
    text = malloc((size_t)size + 1);
    if (text == NULL) {
        printf("malloc failed: not enough memory for %ld bytes\n", size);
        fclose(fp);
        return NULL;
    }

    /* 3) one big read instead of one fgets per line */
    got = fread(text, 1, (size_t)size, fp);
    if (ferror(fp)) {
        printf("read error in %s\n", path);
        fclose(fp);
        free(text);
        return NULL;
    }
    fclose(fp);
    text[got] = '\0';
    *size_out = got;
    return text;
}

/* is_blank
 * What:    1 if the line has nothing in it ("" or just "\r" from a Windows file)
 */
static int is_blank(const char *line) {
    return line[0] == '\0' || (line[0] == '\r' && line[1] == '\0');
}

/* load_trades_fx
 * What:    read the whole file at once and turn every line into a TradeFx
 * In/Out:  path = the CSV file, *n_out = number of trades loaded.
 *          Returns the array (give it back with free_trades_fx), or NULL on error.
 * Steps:   1) read the whole file into memory (read_whole_file)
 *          2) count the '\n' in memory -> capacity (no second file read, unlike V6)
 *          3) malloc capacity TradeFx structs
 *          4) walk the text: end each line with '\0' and parse it
 *          5) free the text, keep only the array
 * Watch:   every error path frees both buffers, so nothing leaks.
 *          Blank lines are skipped; any other line that is not a trade
 *          stops the load, the same as V5 and V6.
 */
TradeFx *load_trades_fx(const char *path, long long *n_out) {
    size_t size, i;
    long long capacity = 1, n = 0;   /* 1 = the last line may have no '\n' */
    char *text, *line;
    TradeFx *trades;

    *n_out = 0;

    /* 1) the whole file, in one go */
    text = read_whole_file(path, &size);
    if (text == NULL) {
        return NULL;
    }

    /* 2) one line per '\n'. This is a loop over memory, not over the disk. */
    for (i = 0; i < size; i++) {
        if (text[i] == '\n') {
            capacity++;
        }
    }

    /* 3) the array, sized to the file. sizeof(TradeFx) is 32 bytes. */
    trades = malloc((size_t)capacity * sizeof(TradeFx));
    if (trades == NULL) {
        printf("malloc failed: not enough memory for %lld trades\n", capacity);
        free(text);
        return NULL;
    }

    /* 4) cut the text into lines. text[size] is the '\0' from step 1,
     *    so the loop also ends the last line even without a '\n'. */
    line = text;
    for (i = 0; i <= size; i++) {
        if (text[i] == '\n' || text[i] == '\0') {
            text[i] = '\0';              /* this line ends here */
            if (!is_blank(line)) {
                if (n >= capacity) {
                    /* stop instead of writing outside the array */
                    printf("more trades than lines in %s (should never happen)\n", path);
                    free(trades);
                    free(text);
                    return NULL;
                }
                if (!parse_trade_fx(line, &trades[n])) {
                    printf("%s: invalid row after trade %lld\n", path, n);
                    free(trades);
                    free(text);
                    return NULL;
                }
                n++;
            }
            line = &text[i + 1];         /* the next line starts after it */
        }
    }

    /* 5) every trade is now in the array, so the raw text is not needed */
    free(text);

    *n_out = n;
    return trades;
}

/* free_trades_fx
 * What:    give the malloc'd array back
 */
void free_trades_fx(TradeFx *trades) {
    free(trades);
}
