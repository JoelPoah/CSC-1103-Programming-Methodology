/*
 * sma_naive.h - the SLOW simple moving average, kept only for comparison
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
