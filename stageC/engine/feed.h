/*
 * feed.h - the "exchange": the whole CSV file held in memory as raw text (Stage C)
 *
 * In a real trading system, trades arrive from the exchange as raw text/bytes
 * and the engine has to parse them. To replay a day of trading we therefore:
 *   1) read the whole file into ONE malloc'd text buffer (fread, no parsing)
 *   2) split it into lines (line[i] points at the start of trade i's text)
 *   3) pull out each trade's timestamp (field 5), which is the replay clock
 * The engine is then handed line[i] at the moment trade i is "released".
 *
 * Memory (stress_full.csv, 8,928,944 trades):
 *   text  ~678 MB   line[] 8 x n = 71 MB   ts[] 8 x n = 71 MB
 * All three are malloc'd to fit the file exactly; feed_free() releases them.
 */
#ifndef FEED_H
#define FEED_H

#include <stddef.h>

typedef struct {
    char       *text;    /* the whole file, '\n' replaced by '\0'     */
    char      **line;    /* line[i] = text of trade i (0-based)       */
    long long  *ts;      /* ts[i]   = timestamp of trade i, in us     */
    long long   n;       /* number of trades                          */
    size_t      bytes;   /* size of the file in bytes                 */
} Feed;

/* Load the file. Returns 1 on success, 0 on error (nothing left allocated). */
int  feed_load(Feed *f, const char *path);

/* Free everything feed_load allocated. Safe to call twice. */
void feed_free(Feed *f);

/* Bytes of heap memory the feed is using (for the memory table). */
size_t feed_memory(const Feed *f);

#endif /* FEED_H */
