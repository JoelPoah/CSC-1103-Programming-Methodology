/*
 * trades_malloc.h - all trades held in memory, in an array made with malloc (Stage C)
 *
 * Stage B used a fixed global array of 10 million trades (320 MB), whatever
 * the file size. Here the array is allocated with malloc to fit the file:
 *   1) count the lines in the file
 *   2) malloc exactly that many Trade structs
 *   3) read the file again to fill them in
 * The caller must free() the array when it is finished with it.
 */
#ifndef TRADES_MALLOC_H
#define TRADES_MALLOC_H

/* Re-use the same Trade struct as Stage B (32 bytes), so the only difference
 * between the two versions is WHERE the array lives, not what is in it.
 * (That header also declares Stage B's global trades[] array, but this
 * version never uses it and does not link trades.c, so it costs nothing.) */
#include "../../stageB/engine/trades.h"

/* Count the lines in the file. Every trade is one line, so this is how many
 * Trade structs we need. Returns -1 if the file cannot be opened. */
long long count_lines(const char *path);

/* Count the file, malloc an array that fits it, and fill the array.
 * On success: returns the array and sets *n_out to the number of trades.
 * On error:   returns NULL (nothing is left allocated).
 * The caller must call free() on the returned array. */
Trade *load_trades_malloc(const char *path, long long *n_out);

#endif /* TRADES_MALLOC_H */
