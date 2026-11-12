/*
 * feed.c - load the whole CSV into memory as raw text (see feed.h)
 */
#include <stdio.h>
#include <stdlib.h>     /* malloc, free */
#include "feed.h"

/* timestamp_of
 * What:    read field 5 (time, microseconds) from one CSV line
 * Steps:   skip 4 commas, then turn the digits into a number
 * Returns: the timestamp, or -1 if the line does not have 5 fields
 * Note:    this is the replay CLOCK, not part of the engine's timed work
 */
static long long timestamp_of(const char *p) {
    long long value = 0;
    int commas = 0;

    while (*p != '\0' && commas < 4) {
        if (*p == ',') {
            commas++;
        }
        p++;
    }
    if (commas < 4 || *p < '0' || *p > '9') {
        return -1;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }
    return value;
}

/* feed_load
 * What:    read the file into memory and index its lines
 * Steps:   1) find the file size (fseek/ftell) and malloc a buffer of size + 1
 *          2) fread the whole file in one go
 *          3) count '\n' -> upper bound on the number of lines
 *          4) malloc line[] and ts[] of that size
 *          5) walk the buffer: end each line with '\0', keep only lines that
 *             start with a digit (skips headers / blank lines)
 * Watch:   every error path frees what was allocated so far
 */
int feed_load(Feed *f, const char *path) {
    FILE *fp;
    long size;
    size_t i, got;
    long long max_lines = 0, n = 0;
    char *start;

    f->text = NULL;
    f->line = NULL;
    f->ts = NULL;
    f->n = 0;
    f->bytes = 0;

    fp = fopen(path, "rb");        /* "rb": no newline translation on Windows */
    if (fp == NULL) {
        printf("feed: cannot open %s\n", path);
        return 0;
    }

    /* 1) file size. stress_full.csv is ~678 MB, which fits in a long. */
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        printf("feed: %s is empty or too big\n", path);
        fclose(fp);
        return 0;
    }

    f->text = malloc((size_t)size + 1);   /* +1 for a final '\0' */
    if (f->text == NULL) {
        printf("feed: malloc failed for %ld bytes\n", size);
        fclose(fp);
        return 0;
    }

    /* 2) one big read */
    got = fread(f->text, 1, (size_t)size, fp);
    fclose(fp);
    f->text[got] = '\0';
    f->bytes = got;

    /* 3) count lines (+1 in case the last line has no '\n') */
    for (i = 0; i < got; i++) {
        if (f->text[i] == '\n') {
            max_lines++;
        }
    }
    max_lines++;

    /* 4) the index arrays, sized to the file */
    f->line = malloc((size_t)max_lines * sizeof(char *));
    f->ts   = malloc((size_t)max_lines * sizeof(long long));
    if (f->line == NULL || f->ts == NULL) {
        printf("feed: malloc failed for %lld lines\n", max_lines);
        feed_free(f);
        return 0;
    }

    /* 5) split into lines */
    start = f->text;
    for (i = 0; i <= got; i++) {
        char c = f->text[i];
        if (c == '\n' || c == '\r' || c == '\0') {
            f->text[i] = '\0';
            if (*start >= '0' && *start <= '9') {    /* a trade line */
                long long t = timestamp_of(start);
                if (t >= 0) {
                    f->line[n] = start;
                    f->ts[n] = t;
                    n++;
                }
            }
            start = &f->text[i + 1];
        }
    }

    if (n == 0) {
        printf("feed: no trades in %s\n", path);
        feed_free(f);
        return 0;
    }
    f->n = n;
    return 1;
}

/* feed_free
 * What:    release the buffers and reset the struct
 */
void feed_free(Feed *f) {
    free(f->text);
    free(f->line);
    free(f->ts);
    f->text = NULL;
    f->line = NULL;
    f->ts = NULL;
    f->n = 0;
    f->bytes = 0;
}

/* feed_memory
 * What:    how many bytes of heap the feed holds (text + both index arrays)
 * Note:    line[] and ts[] were allocated for max_lines, which is n or n + 1,
 *          so n is used here (the difference is a few bytes).
 */
size_t feed_memory(const Feed *f) {
    return f->bytes + 1 + (size_t)f->n * (sizeof(char *) + sizeof(long long));
}
