/*
 * sma_naive.h - the SLOW simple moving average, kept only for comparison
 *
 * Stage C: copied from stageB/engine/sma_naive.h (unchanged), the same way Stage B
 * copied Stage A's modules, so Stage C's engine and its assembly live here.
 *
 * Uses the same Sma struct and circular buffer as sma.h, but ignores the
 * running sum: every time we ask for the average it re-adds all N prices
 * in the window. That costs N additions per trade instead of one add and
 * one subtract, so it shows how much the running sum saves.
 *
 * sma_init() and sma_ready() from sma.h work unchanged with these.
 */
#ifndef SMA_NAIVE_H
#define SMA_NAIVE_H

#include "sma.h"

void   sma_naive_push(Sma *s, double price);   /* store the price only        */
double sma_naive_value(const Sma *s);          /* re-add the window, divide N */

#endif /* SMA_NAIVE_H */
